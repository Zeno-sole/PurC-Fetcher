/*
 * Copyright (C) 2010-2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "DownloadID.h"
#include "DownloadMap.h"
#include "NetworkDataTask.h"
#include "PendingDownload.h"
#include "PolicyDecision.h"
#include "SandboxExtension.h"
#include "NotImplemented.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>

namespace PAL {
class SessionID;
}

namespace PurCFetcher {
class BlobDataFileReference;
class ResourceHandle;
class ResourceRequest;
class ResourceResponse;
}

namespace IPC {
class Connection;
class DataReference;
}

namespace PurCFetcher {

class AuthenticationManager;
class Download;
class NetworkConnectionToWebProcess;
class NetworkLoad;
class PendingDownload;

class DownloadManager {
    WTF_MAKE_NONCOPYABLE(DownloadManager);

public:
    class Client {
    public:
        virtual ~Client() { }

        virtual void didCreateDownload() = 0;
        virtual void didDestroyDownload() = 0;
        virtual IPC::Connection* downloadProxyConnection() = 0;
        virtual IPC::Connection* parentProcessConnectionForDownloads() = 0;
        virtual AuthenticationManager& downloadsAuthenticationManager() = 0;
        virtual void pendingDownloadCanceled(DownloadID) = 0;
        virtual NetworkSession* networkSession(PAL::SessionID) const = 0;
        virtual void ref() const = 0;
        virtual void deref() const = 0;
    };

    explicit DownloadManager(Client&);

    void startDownload(PAL::SessionID, DownloadID, const PurCFetcher::ResourceRequest&, Optional<NavigatingToAppBoundDomain>, const String& suggestedName = { });
    void dataTaskBecameDownloadTask(DownloadID, std::unique_ptr<Download>&&);
    void continueWillSendRequest(DownloadID, PurCFetcher::ResourceRequest&&);
    void willDecidePendingDownloadDestination(NetworkDataTask&, ResponseCompletionHandler&&);
    void convertNetworkLoadToDownload(DownloadID, std::unique_ptr<NetworkLoad>&&, ResponseCompletionHandler&&, const PurCFetcher::ResourceRequest&, const PurCFetcher::ResourceResponse&);
    void continueDecidePendingDownloadDestination(DownloadID, String destination, SandboxExtension::Handle&&, bool allowOverwrite);

    void resumeDownload(PAL::SessionID, DownloadID, const IPC::DataReference& resumeData, const String& path, SandboxExtension::Handle&&);

    void cancelDownload(DownloadID);
    Download* download(DownloadID downloadID) { return m_downloads.get(downloadID); }

    void downloadFinished(Download&);
    bool isDownloading() const { return !m_downloads.isEmpty(); }

    void applicationDidEnterBackground();
    void applicationWillEnterForeground();

    void didCreateDownload();
    void didDestroyDownload();

    IPC::Connection* downloadProxyConnection();
    AuthenticationManager& downloadsAuthenticationManager();
    
    Client& client() { return m_client; }

private:
    Client& m_client;
    HashMap<DownloadID, std::unique_ptr<PendingDownload>> m_pendingDownloads;
    HashMap<DownloadID, std::pair<RefPtr<NetworkDataTask>, ResponseCompletionHandler>> m_downloadsWaitingForDestination;
    HashMap<DownloadID, RefPtr<NetworkDataTask>> m_downloadsAfterDestinationDecided;
    DownloadMap m_downloads;
};

} // namespace PurCFetcher
