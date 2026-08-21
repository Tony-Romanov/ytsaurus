#pragma once

#include <yt/yt/client/node_tracker_client/node_directory.h>

#include <yt/yt/core/rpc/public.h>

namespace NYT::NChunkClient {

//! Throws if libibverbs is unavailable or no active IB/RoCE port is visible.
void ValidateUcxHardware();

void ConfigureUcxTransport(bool enabled, std::string transports);

NRpc::IChannelPtr FindUcxChannel(
    const NNodeTrackerClient::TNodeDescriptor& descriptor,
    const NNodeTrackerClient::TNetworkPreferenceList& networks);

} // namespace NYT::NChunkClient
