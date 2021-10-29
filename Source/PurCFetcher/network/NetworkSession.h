/*
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
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

#include "PrefetchCache.h"
#include "SandboxExtension.h"
#include "AdClickAttribution.h"
#include "NetworkStorageSession.h"
#include "RegistrableDomain.h"
#include "SessionID.h"
#include <wtf/HashSet.h>
#include <wtf/Ref.h>
#include <wtf/Seconds.h>
#include <wtf/UniqueRef.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace PurcFetcher {
class NetworkStorageSession;
class ResourceRequest;
enum class IncludeHttpOnlyCookies : bool;
enum class ShouldSample : bool;
struct SecurityOriginData;
}

namespace WebKit {

class NetworkDataTask;
class NetworkProcess;
class NetworkResourceLoader;
class NetworkSocketChannel;
class WebSocketTask;
struct NetworkSessionCreationParameters;

enum class WebsiteDataType : uint32_t;

namespace NetworkCache {
class Cache;
}

class NetworkSession : public CanMakeWeakPtr<NetworkSession> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<NetworkSession> create(NetworkProcess&, NetworkSessionCreationParameters&&);
    virtual ~NetworkSession();

    virtual void invalidateAndCancel();
    virtual void clearCredentials() { };
    virtual bool shouldLogCookieInformation() const { return false; }
    virtual Seconds loadThrottleLatency() const { return { }; }
    virtual Vector<PurcFetcher::SecurityOriginData> hostNamesWithAlternativeServices() const { return { }; }
    virtual void deleteAlternativeServicesForHostNames(const Vector<String>&) { }
    virtual void clearAlternativeServices(WallTime) { }

    PAL::SessionID sessionID() const { return m_sessionID; }
    NetworkProcess& networkProcess() { return m_networkProcess; }
    PurcFetcher::NetworkStorageSession* networkStorageSession() const;

    void registerNetworkDataTask(NetworkDataTask& task) { m_dataTaskSet.add(&task); }
    void unregisterNetworkDataTask(NetworkDataTask& task) { m_dataTaskSet.remove(&task); }

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WebResourceLoadStatisticsStore* resourceLoadStatistics() const { return m_resourceLoadStatistics.get(); }
    void setResourceLoadStatisticsEnabled(bool);
    void recreateResourceLoadStatisticStore(CompletionHandler<void()>&&);
    bool isResourceLoadStatisticsEnabled() const;
    void notifyResourceLoadStatisticsProcessed();
    void deleteAndRestrictWebsiteDataForRegistrableDomains(OptionSet<WebsiteDataType>, RegistrableDomainsToDeleteOrRestrictWebsiteDataFor&&, bool shouldNotifyPage, CompletionHandler<void(const HashSet<PurcFetcher::RegistrableDomain>&)>&&);
    void registrableDomainsWithWebsiteData(OptionSet<WebsiteDataType>, bool shouldNotifyPage, CompletionHandler<void(HashSet<PurcFetcher::RegistrableDomain>&&)>&&);
    void logDiagnosticMessageWithValue(const String& message, const String& description, unsigned value, unsigned significantFigures, PurcFetcher::ShouldSample);
    void notifyPageStatisticsTelemetryFinished(unsigned numberOfPrevalentResources, unsigned numberOfPrevalentResourcesWithUserInteraction, unsigned numberOfPrevalentResourcesWithoutUserInteraction, unsigned topPrevalentResourceWithUserInteractionDaysSinceUserInteraction, unsigned medianDaysSinceUserInteractionPrevalentResourceWithUserInteraction, unsigned top3NumberOfPrevalentResourcesWithUI, unsigned top3MedianSubFrameWithoutUI, unsigned top3MedianSubResourceWithoutUI, unsigned top3MedianUniqueRedirectsWithoutUI, unsigned top3MedianDataRecordsRemovedWithoutUI);
    bool enableResourceLoadStatisticsLogTestingEvent() const { return m_enableResourceLoadStatisticsLogTestingEvent; }
    void setResourceLoadStatisticsLogTestingEvent(bool log) { m_enableResourceLoadStatisticsLogTestingEvent = log; }
    virtual bool hasIsolatedSession(const PurcFetcher::RegistrableDomain) const { return false; }
    virtual void clearIsolatedSessions() { }
    void setShouldDowngradeReferrerForTesting(bool);
    bool shouldDowngradeReferrer() const;
    void setThirdPartyCookieBlockingMode(PurcFetcher::ThirdPartyCookieBlockingMode);
    void setShouldEnbleSameSiteStrictEnforcement(PurcFetcher::SameSiteStrictEnforcementEnabled);
    void destroyResourceLoadStatistics(CompletionHandler<void()>&&);
    void flushAndDestroyPersistentStore(CompletionHandler<void()>&&);
#endif
    
    virtual bool hasAppBoundSession() const { return false; }
    virtual void clearAppBoundSession() { }
    void storeAdClickAttribution(PurcFetcher::AdClickAttribution&&);
    void handleAdClickAttributionConversion(PurcFetcher::AdClickAttribution::Conversion&&, const URL& requestURL, const PurcFetcher::ResourceRequest& redirectRequest);
    void dumpAdClickAttribution(CompletionHandler<void(String)>&&);
    void clearAdClickAttribution();
    void clearAdClickAttributionForRegistrableDomain(PurcFetcher::RegistrableDomain&&);
    void setAdClickAttributionOverrideTimerForTesting(bool value);
    void setAdClickAttributionConversionURLForTesting(URL&&);
    void markAdClickAttributionsAsExpiredForTesting();

    void addKeptAliveLoad(Ref<NetworkResourceLoader>&&);
    void removeKeptAliveLoad(NetworkResourceLoader&);

    NetworkCache::Cache* cache() { return m_cache.get(); }

    PrefetchCache& prefetchCache() { return m_prefetchCache; }
    void clearPrefetchCache() { m_prefetchCache.clear(); }

    virtual std::unique_ptr<WebSocketTask> createWebSocketTask(NetworkSocketChannel&, const PurcFetcher::ResourceRequest&, const String& protocol);
    virtual void removeWebSocketTask(WebSocketTask&) { }
    virtual void addWebSocketTask(WebSocketTask&) { }


    unsigned testSpeedMultiplier() const { return m_testSpeedMultiplier; }
    bool allowsServerPreconnect() const { return m_allowsServerPreconnect; }

    bool isStaleWhileRevalidateEnabled() const { return m_isStaleWhileRevalidateEnabled; }

#if ENABLE(SERVICE_WORKER)
    void addSoftUpdateLoader(std::unique_ptr<ServiceWorkerSoftUpdateLoader>&& loader) { m_softUpdateLoaders.add(WTFMove(loader)); }
    void removeSoftUpdateLoader(ServiceWorkerSoftUpdateLoader* loader) { m_softUpdateLoaders.remove(loader); }
#endif

protected:
    NetworkSession(NetworkProcess&, const NetworkSessionCreationParameters&);

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    void forwardResourceLoadStatisticsSettings();
#endif

    PAL::SessionID m_sessionID;
    Ref<NetworkProcess> m_networkProcess;
    HashSet<NetworkDataTask*> m_dataTaskSet;
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    String m_resourceLoadStatisticsDirectory;
    RefPtr<WebResourceLoadStatisticsStore> m_resourceLoadStatistics;
    ShouldIncludeLocalhost m_shouldIncludeLocalhostInResourceLoadStatistics { ShouldIncludeLocalhost::Yes };
    EnableResourceLoadStatisticsDebugMode m_enableResourceLoadStatisticsDebugMode { EnableResourceLoadStatisticsDebugMode::No };
    PurcFetcher::RegistrableDomain m_resourceLoadStatisticsManualPrevalentResource;
    bool m_enableResourceLoadStatisticsLogTestingEvent;
    bool m_downgradeReferrer { true };
    PurcFetcher::ThirdPartyCookieBlockingMode m_thirdPartyCookieBlockingMode { PurcFetcher::ThirdPartyCookieBlockingMode::All };
    PurcFetcher::SameSiteStrictEnforcementEnabled m_sameSiteStrictEnforcementEnabled { PurcFetcher::SameSiteStrictEnforcementEnabled::No };
    PurcFetcher::FirstPartyWebsiteDataRemovalMode m_firstPartyWebsiteDataRemovalMode { PurcFetcher::FirstPartyWebsiteDataRemovalMode::AllButCookies };
    PurcFetcher::RegistrableDomain m_standaloneApplicationDomain;
#endif
    bool m_isStaleWhileRevalidateEnabled { false };

    HashSet<Ref<NetworkResourceLoader>> m_keptAliveLoads;

    PrefetchCache m_prefetchCache;

#if ENABLE_ASSERTS
    bool m_isInvalidated { false };
#endif
    RefPtr<NetworkCache::Cache> m_cache;
    unsigned m_testSpeedMultiplier { 1 };
    bool m_allowsServerPreconnect { true };

#if ENABLE(SERVICE_WORKER)
    HashSet<std::unique_ptr<ServiceWorkerSoftUpdateLoader>> m_softUpdateLoaders;
#endif
};

} // namespace WebKit
