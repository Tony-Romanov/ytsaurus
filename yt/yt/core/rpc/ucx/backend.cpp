#include <yt/yt/core/rpc/backend_detail.h>
#include <yt/yt/core/rpc/endpoint_address.h>

#include <yt/yt/core/rpc/bus/channel.h>
#include <yt/yt/core/rpc/bus/server.h>

#include <yt/yt/core/bus/ucx/client.h>
#include <yt/yt/core/bus/ucx/server.h>

#include <yt/yt/core/net/local_address.h>
#include <yt/yt/core/net/address.h>

namespace NYT::NRpc::NUcx {
namespace {

using TClientConfig = NYT::NBus::NUcx::TBusClientConfig;
using TClientConfigPtr = NYT::NBus::NUcx::TBusClientConfigPtr;
using TServerConfig = NYT::NBus::NUcx::TBusServerConfig;
using TServerConfigPtr = NYT::NBus::NUcx::TBusServerConfigPtr;

class TUcxChannelFactory
    : public IChannelFactory
{
public:
    explicit TUcxChannelFactory(TClientConfigPtr config)
        : Config_(std::move(config))
    { }

    IChannelPtr CreateChannel(const std::string& address) override
    {
        auto config = New<TClientConfig>();
        config->Transports = Config_->Transports;
        config->Address = address;
        return NRpc::NBus::CreateBusChannel(NYT::NBus::NUcx::CreateBusClient(std::move(config)));
    }

private:
    const TClientConfigPtr Config_;
};

class TUcxBackend
    : public TBackendBase<TClientConfig, TServerConfig>
{
public:
    TStringBuf GetProtocol() final
    {
        return "yt-ucx"_sb;
    }

protected:
    std::string DoBuildLocalEndpointAddress(const TServerConfigPtr& config) final
    {
        if (!config->Port) {
            THROW_ERROR_EXCEPTION("UCX RPC backend is not bound to a port");
        }
        return FormatEndpointAddress({
            .Protocol = GetProtocol(),
            .Address = NNet::BuildServiceAddress(NNet::GetLocalHostName(), *config->Port),
        });
    }

    IChannelFactoryPtr DoCreateChannelFactory(const TClientConfigPtr& config) final
    {
        return New<TUcxChannelFactory>(config);
    }

    IServerPtr DoCreateServer(const TServerConfigPtr& config) final
    {
        return NRpc::NBus::CreateBusServer(NYT::NBus::NUcx::CreateBusServer(config));
    }
};

YT_DEFINE_RPC_BACKEND(TUcxBackend);

} // namespace
} // namespace NYT::NRpc::NUcx
