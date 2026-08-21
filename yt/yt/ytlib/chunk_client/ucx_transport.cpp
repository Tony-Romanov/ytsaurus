#include "ucx_transport.h"

#include <yt/yt/core/bus/ucx/client.h>
#include <yt/yt/core/rpc/bus/channel.h>
#include <yt/yt/core/logging/log.h>
#include <yt/yt/core/misc/error.h>
#include <yt/yt/core/profiling/timing.h>

#include <contrib/libs/ibdrv/symbols.h>

#include <util/generic/singleton.h>

#include <mutex>

namespace NYT::NChunkClient {
namespace {

const NLogging::TLogger Logger("UcxTransport");
const NProfiling::TProfiler Profiler("/ucx");
const auto Fallbacks = Profiler.Counter("/fallbacks");
const auto ConnectionErrors = Profiler.Counter("/connection_errors");
const auto RetryDelay = TDuration::Seconds(30);

struct TState
{
    std::mutex Lock;
    std::mutex CreateLock;
    bool Enabled = false;
    std::string Transports;
    THashMap<std::string, NRpc::IChannelPtr> Channels;
    THashMap<std::string, TInstant> RetryAt;
};

TState* GetState()
{
    return Singleton<TState>();
}

std::optional<std::string> FindEndpoint(
    const NNodeTrackerClient::TNodeDescriptor& descriptor,
    const NNodeTrackerClient::TNetworkPreferenceList& networks)
{
    for (const auto& network : networks) {
        auto it = descriptor.Addresses().find(Format("ucx/%v", network));
        if (it != descriptor.Addresses().end()) {
            return it->second;
        }
    }
    return std::nullopt;
}

} // namespace

void ValidateUcxHardware()
{
    const auto* symbols = ::IBSym();
    if (!symbols ||
        !symbols->ibv_get_device_list ||
        !symbols->ibv_free_device_list ||
        !symbols->ibv_open_device ||
        !symbols->ibv_close_device ||
        !symbols->ibv_query_device ||
        !symbols->ibv_query_port)
    {
        THROW_ERROR_EXCEPTION("Required ibverbs symbols are missing");
    }

    int deviceCount = 0;
    auto** devices = symbols->ibv_get_device_list(&deviceCount);
    if (!devices || deviceCount <= 0) {
        if (devices) {
            symbols->ibv_free_device_list(devices);
        }
        THROW_ERROR_EXCEPTION("No RDMA devices found");
    }

    bool hasActivePort = false;
    for (int deviceIndex = 0; deviceIndex < deviceCount && !hasActivePort; ++deviceIndex) {
        auto* context = symbols->ibv_open_device(devices[deviceIndex]);
        if (!context) {
            continue;
        }
        ibv_device_attr deviceAttributes{};
        if (symbols->ibv_query_device(context, &deviceAttributes) == 0) {
            for (ui8 port = 1; port <= deviceAttributes.phys_port_cnt; ++port) {
                ibv_port_attr portAttributes{};
                if (symbols->ibv_query_port(
                        context,
                        port,
                        reinterpret_cast<_compat_ibv_port_attr*>(&portAttributes)) == 0 &&
                    portAttributes.state == IBV_PORT_ACTIVE)
                {
                    hasActivePort = true;
                    break;
                }
            }
        }
        symbols->ibv_close_device(context);
    }
    symbols->ibv_free_device_list(devices);

    if (!hasActivePort) {
        THROW_ERROR_EXCEPTION("No active RDMA ports found");
    }
}

void ConfigureUcxTransport(bool enabled, std::string transports)
{
    auto* state = GetState();
    THashMap<std::string, NRpc::IChannelPtr> channelsToClose;
    {
        std::lock_guard guard(state->Lock);
        state->Enabled = enabled;
        state->Transports = std::move(transports);
        if (!enabled) {
            channelsToClose.swap(state->Channels);
            state->RetryAt.clear();
        }
    }
}

NRpc::IChannelPtr FindUcxChannel(
    const NNodeTrackerClient::TNodeDescriptor& descriptor,
    const NNodeTrackerClient::TNetworkPreferenceList& networks)
{
    auto endpoint = FindEndpoint(descriptor, networks);
    if (!endpoint) {
        return nullptr;
    }

    auto* state = GetState();
    {
        std::lock_guard guard(state->Lock);
        if (!state->Enabled) {
            return nullptr;
        }
        if (auto it = state->RetryAt.find(*endpoint); it != state->RetryAt.end() && TInstant::Now() < it->second) {
            return nullptr;
        }
        if (auto it = state->Channels.find(*endpoint); it != state->Channels.end()) {
            return it->second;
        }
    }

    std::lock_guard createGuard(state->CreateLock);
    std::string transports;
    bool retrying = false;
    {
        std::lock_guard guard(state->Lock);
        if (!state->Enabled) {
            return nullptr;
        }
        if (auto it = state->Channels.find(*endpoint); it != state->Channels.end()) {
            return it->second;
        }
        if (auto it = state->RetryAt.find(*endpoint); it != state->RetryAt.end()) {
            if (TInstant::Now() < it->second) {
                return nullptr;
            }
            state->RetryAt.erase(it);
            retrying = true;
        }
        transports = state->Transports;
    }
    if (retrying) {
        YT_LOG_INFO("Retrying UCX transport for peer after TCP fallback (RemoteAddress: %v)", *endpoint);
    }

    try {
        auto config = New<NBus::NUcx::TBusClientConfig>();
        config->Address = *endpoint;
        config->Transports = transports;
        auto channel = NRpc::NBus::CreateBusChannel(NBus::NUcx::CreateBusClient(config));
        channel->SubscribeTerminated(BIND([endpoint = *endpoint] (const TError& error) {
            auto* state = GetState();
            bool fallback;
            {
                std::lock_guard guard(state->Lock);
                state->Channels.erase(endpoint);
                fallback = state->Enabled;
                if (fallback) {
                    state->RetryAt[endpoint] = TInstant::Now() + RetryDelay;
                }
            }
            if (!fallback) {
                return;
            }
            ConnectionErrors.Increment();
            Fallbacks.Increment();
            YT_LOG_WARNING(error,
                "UCX transport disabled for peer; falling back to TCP (RemoteAddress: %v)",
                endpoint);
        }));
        {
            std::lock_guard guard(state->Lock);
            if (!state->Enabled || state->RetryAt.contains(*endpoint)) {
                return nullptr;
            }
            state->Channels.emplace(*endpoint, channel);
        }
        YT_LOG_INFO("UCX transport enabled for peer (RemoteAddress: %v)", *endpoint);
        return channel;
    } catch (const std::exception& ex) {
        {
            std::lock_guard guard(state->Lock);
            if (state->Enabled) {
                state->RetryAt[*endpoint] = TInstant::Now() + RetryDelay;
            }
        }
        ConnectionErrors.Increment();
        Fallbacks.Increment();
        YT_LOG_WARNING(ex,
            "Failed to enable UCX transport for peer; falling back to TCP (RemoteAddress: %v)",
            *endpoint);
        return nullptr;
    }
}

} // namespace NYT::NChunkClient
