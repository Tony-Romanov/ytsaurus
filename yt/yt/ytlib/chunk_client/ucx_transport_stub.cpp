#include "ucx_transport.h"

#include <yt/yt/core/misc/error.h>

namespace NYT::NChunkClient {

void ValidateUcxHardware()
{
    THROW_ERROR_EXCEPTION("UCX transport is supported on Linux only");
}

void ConfigureUcxTransport(bool, std::string)
{ }

NRpc::IChannelPtr FindUcxChannel(
    const NNodeTrackerClient::TNodeDescriptor&,
    const NNodeTrackerClient::TNetworkPreferenceList&)
{
    return nullptr;
}

} // namespace NYT::NChunkClient
