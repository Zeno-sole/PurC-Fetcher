/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "NetworkSessionCreationParameters.h"
#include "SandboxExtension.h"
#include "Cookie.h"
#include "StorageQuotaManager.h"
#include "SessionID.h"
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace IPC {
class Decoder;
class Encoder;
}

namespace PurCFetcher {

struct WebsiteDataStoreParameters {
    WebsiteDataStoreParameters() = default;
    WebsiteDataStoreParameters(WebsiteDataStoreParameters&&) = default;
    WebsiteDataStoreParameters& operator=(WebsiteDataStoreParameters&&) = default;
    ~WebsiteDataStoreParameters();

    void encode(IPC::Encoder&) const;
    static Optional<WebsiteDataStoreParameters> decode(IPC::Decoder&);

    Vector<uint8_t> uiProcessCookieStorageIdentifier;
    SandboxExtension::Handle cookieStoragePathExtensionHandle;
    Vector<PurCFetcher::Cookie> pendingCookies;
    NetworkSessionCreationParameters networkSessionParameters;

    String localStorageDirectory;
    SandboxExtension::Handle localStorageDirectoryExtensionHandle;

    String cacheStorageDirectory;
    SandboxExtension::Handle cacheStorageDirectoryExtensionHandle;

    uint64_t perOriginStorageQuota { PurCFetcher::StorageQuotaManager::defaultQuota() };
    uint64_t perThirdPartyOriginStorageQuota { PurCFetcher::StorageQuotaManager::defaultThirdPartyQuota() };
};

} // namespace PurCFetcher
