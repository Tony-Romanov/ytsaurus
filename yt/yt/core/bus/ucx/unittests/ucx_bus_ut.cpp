#include <yt/yt/core/bus/ucx/client.h>
#include <yt/yt/core/bus/ucx/server.h>

#include <yt/yt/core/bus/bus.h>
#include <yt/yt/core/bus/client.h>
#include <yt/yt/core/bus/server.h>
#include <yt/yt/core/bus/unittests/lib/handlers.h>
#include <yt/yt/core/bus/unittests/lib/helpers.h>

#include <yt/yt/core/concurrency/scheduler_api.h>
#include <yt/yt/core/net/address.h>
#include <yt/yt/core/test_framework/framework.h>

#include <library/cpp/testing/common/network.h>

namespace NYT::NBus::NUcx::NTests {
namespace {

using namespace NConcurrency;

struct TUcxTestTransport
{
    NTesting::TPortHolder Port;
    IBusServerPtr Server;
    IBusClientPtr Client;

    explicit TUcxTestTransport(
        IMessageHandlerPtr serverHandler,
        std::string host = "127.0.0.1")
        : Port(NTesting::GetFreePort())
    {
        auto serverConfig = New<TBusServerConfig>();
        serverConfig->Address = host;
        serverConfig->Port = Port;
        serverConfig->Transports = "tcp";
        Server = CreateBusServer(serverConfig);
        Server->Start(std::move(serverHandler));

        auto clientConfig = New<TBusClientConfig>();
        clientConfig->Address = NNet::BuildServiceAddress(host, Port);
        clientConfig->Transports = "tcp";
        Client = CreateBusClient(clientConfig);
    }

    ~TUcxTestTransport()
    {
        WaitFor(Server->Stop()).ThrowOnError();
    }
};

TEST(TUcxBusTest, RequestReplyPreservesMessageParts)
{
    TUcxTestTransport transport(New<NBus::NTests::TReplying42BusHandler>(4));
    auto replyHandler = New<NBus::NTests::TChecking42BusHandler>(1);
    auto bus = transport.Client->CreateBus(replyHandler);

    WaitFor(bus->Send(
        NBus::NTests::CreateMessage(4, 64_KB),
        {.TrackingLevel = EDeliveryTrackingLevel::Full}))
        .ThrowOnError();
    replyHandler->WaitUntilDone();
}

TEST(TUcxBusTest, Ipv6ListenerStartsAndStops)
{
    auto port = NTesting::GetFreePort();
    auto serverConfig = New<TBusServerConfig>();
    serverConfig->Address = "::1";
    serverConfig->Port = port;
    serverConfig->Transports = "tcp";
    auto server = CreateBusServer(serverConfig);
    server->Start(New<NBus::NTests::TEmptyBusHandler>());
    WaitFor(server->Stop()).ThrowOnError();
}

TEST(TUcxBusTest, LargeMessage)
{
    auto handler = New<NBus::NTests::TDirectPlacementBusHandler>();
    TUcxTestTransport transport(handler);
    auto bus = transport.Client->CreateBus(New<NBus::NTests::TEmptyBusHandler>());

    WaitFor(bus->Send(
        NBus::NTests::CreateMessage(3, 8_MB),
        {.TrackingLevel = EDeliveryTrackingLevel::Full}))
        .ThrowOnError();

    EXPECT_EQ(handler->WaitForReceivedPartSizes(), std::vector<i64>({8_MB, 8_MB, 8_MB}));
    EXPECT_FALSE(handler->SawDirectPlacementTransfer());
}

TEST(TUcxBusTest, SendAfterTerminationFails)
{
    TUcxTestTransport transport(New<NBus::NTests::TEmptyBusHandler>());
    auto bus = transport.Client->CreateBus(New<NBus::NTests::TEmptyBusHandler>());
    WaitFor(bus->Send(
        NBus::NTests::CreateMessage(1),
        {.TrackingLevel = EDeliveryTrackingLevel::Full}))
        .ThrowOnError();
    bus->Terminate(TError("test termination"));

    auto result = WaitFor(bus->Send(
        NBus::NTests::CreateMessage(1),
        {.TrackingLevel = EDeliveryTrackingLevel::Full}));
    EXPECT_FALSE(result.IsOK());
}

} // namespace
} // namespace NYT::NBus::NUcx::NTests
