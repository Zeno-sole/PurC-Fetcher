/*
 * @file fetcher-process.cpp
 * @author XueShuming
 * @date 2021/11/17
 * @brief The impl for fetcher process.
 *
 * Copyright (C) 2021 FMSoft <https://www.fmsoft.cn>
 *
 * This file is a part of PurC (short for Purring Cat), an HVML interpreter.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "fetcher-process.h"

#include "NetworkProcessCreationParameters.h"
#include "NetworkProcessMessages.h"

#include <wtf/RunLoop.h>

using namespace PurCFetcher;

PcFetcherProcess::PcFetcherProcess(struct pcfetcher* fetcher,
        bool alwaysRunsAtBackgroundPriority)
 : m_fetcher(fetcher)
 , m_alwaysRunsAtBackgroundPriority(alwaysRunsAtBackgroundPriority)
{
}

PcFetcherProcess::~PcFetcherProcess()
{
    if (m_connection)
        m_connection->invalidate();

    if (m_processLauncher) {
        m_processLauncher->invalidate();
        m_processLauncher = nullptr;
    }
    auto pendingMessages = WTFMove(m_pendingMessages);
    for (auto& pendingMessage : pendingMessages) {
        if (pendingMessage.asyncReplyInfo)
            pendingMessage.asyncReplyInfo->first(nullptr);
    }
}

void PcFetcherProcess::getLaunchOptions(ProcessLauncher::LaunchOptions& launchOptions)
{
    launchOptions.processIdentifier = m_processIdentifier;

    if (const char* userDirectorySuffix = getenv("DIRHELPER_USER_DIR_SUFFIX"))
        launchOptions.extraInitializationData.add("user-directory-suffix"_s, userDirectorySuffix);

    if (m_alwaysRunsAtBackgroundPriority)
        launchOptions.extraInitializationData.add("always-runs-at-background-priority"_s, "true");

    launchOptions.processType = ProcessLauncher::ProcessType::Network;

}

void PcFetcherProcess::connect()
{
    ASSERT(!m_processLauncher);
    ProcessLauncher::LaunchOptions launchOptions;
    getLaunchOptions(launchOptions);
    m_processLauncher = ProcessLauncher::create(this, WTFMove(launchOptions));
}

void PcFetcherProcess::terminate()
{
    if (m_processLauncher)
        m_processLauncher->terminateProcess();
}

void PcFetcherProcess::initFetcherProcess()
{
     NetworkProcessCreationParameters parameters;
     send(Messages::NetworkProcess::InitializeNetworkProcess(parameters), 0);
}

PcFetcherProcess::State PcFetcherProcess::state() const
{
    if (m_processLauncher && m_processLauncher->isLaunching())
        return PcFetcherProcess::State::Launching;

    if (!m_connection)
        return PcFetcherProcess::State::Terminated;

    return PcFetcherProcess::State::Running;
}

bool PcFetcherProcess::wasTerminated() const
{
    switch (state()) {
    case PcFetcherProcess::State::Launching:
        return false;
    case PcFetcherProcess::State::Terminated:
        return true;
    case PcFetcherProcess::State::Running:
        break;
    }

    auto pid = processIdentifier();
    if (!pid)
        return true;

    return false;
}

bool PcFetcherProcess::sendMessage(std::unique_ptr<IPC::Encoder> encoder, OptionSet<IPC::SendOption> sendOptions, Optional<std::pair<CompletionHandler<void(IPC::Decoder*)>, uint64_t>>&& asyncReplyInfo, ShouldStartProcessThrottlerActivity shouldStartProcessThrottlerActivity)
{
    if (asyncReplyInfo && canSendMessage() && shouldStartProcessThrottlerActivity == ShouldStartProcessThrottlerActivity::Yes) {
        auto completionHandler = std::exchange(asyncReplyInfo->first, nullptr);
        asyncReplyInfo->first = [completionHandler = WTFMove(completionHandler)](IPC::Decoder* decoder) mutable {
            completionHandler(decoder);
        };
    }

    switch (state()) {
    case State::Launching:
        // If we're waiting for the child process to launch, we need to stash away the messages so we can send them once we have a connection.
        m_pendingMessages.append({ WTFMove(encoder), sendOptions, WTFMove(asyncReplyInfo) });
        return true;

    case State::Running:
        if (asyncReplyInfo)
            IPC::addAsyncReplyHandler(*connection(), asyncReplyInfo->second, std::exchange(asyncReplyInfo->first, nullptr));
        if (connection()->sendMessage(WTFMove(encoder), sendOptions))
            return true;
        break;

    case State::Terminated:
        break;
    }

    if (asyncReplyInfo && asyncReplyInfo->first) {
        RunLoop::current().dispatch([completionHandler = WTFMove(asyncReplyInfo->first)]() mutable {
            completionHandler(nullptr);
        });
    }

    return false;
}

void PcFetcherProcess::addMessageReceiver(IPC::ReceiverName messageReceiverName, IPC::MessageReceiver& messageReceiver)
{
    m_messageReceiverMap.addMessageReceiver(messageReceiverName, messageReceiver);
}

void PcFetcherProcess::addMessageReceiver(IPC::ReceiverName messageReceiverName, uint64_t destinationID, IPC::MessageReceiver& messageReceiver)
{
    m_messageReceiverMap.addMessageReceiver(messageReceiverName, destinationID, messageReceiver);
}

void PcFetcherProcess::removeMessageReceiver(IPC::ReceiverName messageReceiverName, uint64_t destinationID)
{
    m_messageReceiverMap.removeMessageReceiver(messageReceiverName, destinationID);
}

void PcFetcherProcess::removeMessageReceiver(IPC::ReceiverName messageReceiverName)
{
    m_messageReceiverMap.removeMessageReceiver(messageReceiverName);
}

bool PcFetcherProcess::dispatchMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    return m_messageReceiverMap.dispatchMessage(connection, decoder);
}

bool PcFetcherProcess::dispatchSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, std::unique_ptr<IPC::Encoder>& replyEncoder)
{
    return m_messageReceiverMap.dispatchSyncMessage(connection, decoder, replyEncoder);
}

void PcFetcherProcess::didFinishLaunching(ProcessLauncher*, IPC::Connection::Identifier connectionIdentifier)
{
    ASSERT(!m_connection);

    if (!IPC::Connection::identifierIsValid(connectionIdentifier))
        return;

    m_connection = IPC::Connection::createServerConnection(connectionIdentifier, *this);

    connectionWillOpen(*m_connection);
    m_connection->open();

    for (auto&& pendingMessage : std::exchange(m_pendingMessages, { })) {
        if (!shouldSendPendingMessage(pendingMessage))
            continue;
        auto encoder = WTFMove(pendingMessage.encoder);
        auto sendOptions = pendingMessage.sendOptions;
        if (pendingMessage.asyncReplyInfo)
            IPC::addAsyncReplyHandler(*connection(), pendingMessage.asyncReplyInfo->second, WTFMove(pendingMessage.asyncReplyInfo->first));
        m_connection->sendMessage(WTFMove(encoder), sendOptions);
    }
}

void PcFetcherProcess::shutDownProcess()
{
    switch (state()) {
    case State::Launching:
        m_processLauncher->invalidate();
        m_processLauncher = nullptr;
        break;
    case State::Running:
        break;
    case State::Terminated:
        return;
    }

    if (!m_connection)
        return;

#if 0
    if (canSendMessage())
        send(Messages::AuxiliaryProcess::ShutDown(), 0);
#endif

    m_connection->invalidate();
    m_connection = nullptr;
}

void PcFetcherProcess::setProcessSuppressionEnabled(bool processSuppressionEnabled)
{
    UNUSED_PARAM(processSuppressionEnabled);
}

void PcFetcherProcess::didClose(IPC::Connection&)
{
}

void PcFetcherProcess::didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName)
{
}

void PcFetcherProcess::connectionWillOpen(IPC::Connection&)
{
}

void PcFetcherProcess::logInvalidMessage(IPC::Connection&, IPC::MessageName)
{
}
