#include "client.h"
#include "server.h"

#include <yt/yt/core/bus/bus.h>
#include <yt/yt/core/bus/message_handler.h>
#include <yt/yt/core/logging/log.h>
#include <yt/yt/core/misc/error.h>
#include <yt/yt/core/net/address.h>
#include <yt/yt/core/profiling/timing.h>
#include <yt/yt/core/ytree/convert.h>
#include <yt/yt/core/ytree/fluent.h>
#include <yt/yt/core/ytree/ypath_service.h>

#include <contrib/libs/ucx/src/ucp/api/ucp.h>

#include <util/system/thread.h>

#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>

#include <netdb.h>
#include <netinet/in.h>

namespace NYT::NBus::NUcx {
namespace {

using namespace NYTree;

constexpr ui32 WireMagic = 0x59545543; // "YTUC"
constexpr unsigned ActiveMessageId = 17;

const NProfiling::TProfiler UcxProfiler("/ucx");
const auto UcxSentBytes = UcxProfiler.Counter("/sent_bytes");
const auto UcxReceivedBytes = UcxProfiler.Counter("/received_bytes");
const auto UcxSentMessages = UcxProfiler.Counter("/sent_messages");
const auto UcxReceivedMessages = UcxProfiler.Counter("/received_messages");
const NLogging::TLogger Logger("UcxBus");

TError MakeUcxError(TStringBuf message, ucs_status_t status)
{
    return TError(NBus::EErrorCode::TransportError, "%v: %v", message, ucs_status_string(status));
}

void CheckUcx(ucs_status_t status, TStringBuf message)
{
    if (status != UCS_OK) {
        THROW_ERROR MakeUcxError(message, status);
    }
}

template <class T>
void AppendPod(std::vector<char>* output, const T& value)
{
    auto offset = output->size();
    output->resize(offset + sizeof(T));
    std::memcpy(output->data() + offset, &value, sizeof(T));
}

TSharedMutableRef SerializeMessage(const TSharedRefArray& message)
{
    std::vector<char> data;
    AppendPod(&data, WireMagic);
    auto partCount = static_cast<ui32>(message.Size());
    AppendPod(&data, partCount);
    for (const auto& part : message) {
        i64 size = part ? static_cast<i64>(part.Size()) : -1;
        AppendPod(&data, size);
    }
    for (const auto& part : message) {
        if (part && part.Size() > 0) {
            auto offset = data.size();
            data.resize(offset + part.Size());
            std::memcpy(data.data() + offset, part.Begin(), part.Size());
        }
    }

    auto result = TSharedMutableRef::Allocate(data.size(), {.InitializeStorage = false});
    std::memcpy(result.Begin(), data.data(), data.size());
    return result;
}

TErrorOr<TSharedRefArray> DeserializeMessage(TSharedRef data)
{
    const char* current = data.Begin();
    const char* end = data.End();
    auto read = [&] <class T> (T* value) {
        if (end - current < static_cast<ssize_t>(sizeof(T))) {
            return false;
        }
        std::memcpy(value, current, sizeof(T));
        current += sizeof(T);
        return true;
    };

    ui32 magic;
    ui32 partCount;
    if (!read(&magic) || !read(&partCount) || magic != WireMagic || partCount > MaxMessagePartCount) {
        return TError(NBus::EErrorCode::TransportError, "Malformed UCX bus message header");
    }
    std::vector<i64> sizes(partCount);
    i64 payloadSize = 0;
    for (auto& size : sizes) {
        if (!read(&size) || size < -1 || size > static_cast<i64>(MaxMessagePartSize)) {
            return TError(NBus::EErrorCode::TransportError, "Malformed UCX bus message part size");
        }
        if (size > 0 && payloadSize > std::numeric_limits<i64>::max() - size) {
            return TError(NBus::EErrorCode::TransportError, "UCX bus message size overflow");
        }
        payloadSize += std::max<i64>(size, 0);
    }
    if (end - current != payloadSize) {
        return TError(NBus::EErrorCode::TransportError, "UCX bus message payload size mismatch");
    }

    std::vector<TSharedRef> parts;
    parts.reserve(partCount);
    for (auto size : sizes) {
        if (size < 0) {
            parts.push_back({});
        } else {
            parts.push_back(data.Slice(current, current + size));
            current += size;
        }
    }
    return TSharedRefArray(std::move(parts), TSharedRefArray::TMoveParts{});
}

class TEngine;
DECLARE_REFCOUNTED_CLASS(TUcxBus)

class TUcxBus
    : public IBus
{
public:
    TUcxBus(TEngine* engine, ucp_ep_h endpoint, std::string endpointDescription, IMessageHandlerPtr handler);
    ~TUcxBus();

    const std::string& GetEndpointDescription() const override;
    const IAttributeDictionary& GetEndpointAttributes() const override;
    const std::string& GetEndpointAddress() const override;
    const NNet::TNetworkAddress& GetEndpointNetworkAddress() const override;
    TBusNetworkStatistics GetNetworkStatistics() const override;
    bool IsEndpointLocal() const override;
    bool IsEncrypted() const override;
    TFuture<void> GetReadyFuture() const override;
    TFuture<void> Send(TSharedRefArray message, const TSendOptions& options) override;
    void SetTosLevel(TTosLevel) override;
    void Terminate(const TError& error) override;
    DEFINE_SIGNAL_OVERRIDE(void(const TError&), Terminated);

    void HandlePayload(TSharedRef payload);
    ucp_ep_h GetEndpoint() const;

private:
    TEngine* const Engine_;
    const ucp_ep_h Endpoint_;
    const std::string EndpointDescription_;
    const std::string EndpointAddress_;
    const IAttributeDictionaryPtr EndpointAttributes_;
    const IMessageHandlerPtr Handler_;
    std::atomic<bool> IsTerminated_ = false;
    std::mutex StateLock_;
    TError TerminationError_;
};

DEFINE_REFCOUNTED_TYPE(TUcxBus)

class TEngine
{
public:
    explicit TEngine(const std::string& transports)
    {
        ucp_config_t* config = nullptr;
        CheckUcx(ucp_config_read(nullptr, nullptr, &config), "Cannot read UCX configuration");
        try {
            CheckUcx(ucp_config_modify(config, "TLS", transports.c_str()), "Cannot configure UCX transports");
            ucp_params_t params{};
            params.field_mask = UCP_PARAM_FIELD_FEATURES;
            params.features = UCP_FEATURE_AM;
            CheckUcx(ucp_init(&params, config, &Context_), "Cannot initialize UCX");
        } catch (...) {
            ucp_config_release(config);
            throw;
        }
        ucp_config_release(config);

        try {
            ucp_worker_params_t workerParams{};
            workerParams.field_mask = UCP_WORKER_PARAM_FIELD_THREAD_MODE;
            workerParams.thread_mode = UCS_THREAD_MODE_MULTI;
            CheckUcx(ucp_worker_create(Context_, &workerParams, &Worker_), "Cannot create UCX worker");
            CheckUcx(ucp_worker_create(Context_, &workerParams, &ListenerWorker_), "Cannot create UCX listener worker");

            ucp_am_handler_param_t handler{};
            handler.field_mask = UCP_AM_HANDLER_PARAM_FIELD_ID |
                UCP_AM_HANDLER_PARAM_FIELD_CB |
                UCP_AM_HANDLER_PARAM_FIELD_ARG;
            handler.id = ActiveMessageId;
            handler.cb = &TEngine::OnActiveMessage;
            handler.arg = this;
            CheckUcx(ucp_worker_set_am_recv_handler(Worker_, &handler), "Cannot install UCX AM handler");

            ProgressThread_ = std::thread([this] {
                while (!Stopping_.load()) {
                    auto progress = ucp_worker_progress(Worker_) + ucp_worker_progress(ListenerWorker_);
                    if (progress == 0) {
                        std::this_thread::yield();
                    }
                }
            });
        } catch (...) {
            if (ListenerWorker_) {
                ucp_worker_destroy(ListenerWorker_);
            }
            if (Worker_) {
                ucp_worker_destroy(Worker_);
            }
            ucp_cleanup(Context_);
            throw;
        }
    }

    ~TEngine()
    {
        Stop();
        if (Worker_) {
            ucp_worker_destroy(Worker_);
        }
        if (ListenerWorker_) {
            ucp_worker_destroy(ListenerWorker_);
        }
        if (Context_) {
            ucp_cleanup(Context_);
        }
    }

    void Stop()
    {
        if (Stopping_.exchange(true)) {
            return;
        }
        if (ProgressThread_.joinable()) {
            ProgressThread_.join();
        }
        if (Listener_) {
            ucp_listener_destroy(Listener_);
            Listener_ = nullptr;
        }
        std::vector<TUcxBusPtr> buses;
        {
            std::lock_guard guard(Lock_);
            for (const auto& [_, bus] : Buses_) {
                if (auto strongBus = bus.Lock()) {
                    buses.push_back(std::move(strongBus));
                }
            }
            Buses_.clear();
            OwnedBuses_.clear();
        }
        for (const auto& bus : buses) {
            bus->Terminate(TError(NBus::EErrorCode::TransportError, "UCX engine stopped"));
        }
    }

    TUcxBusPtr Connect(const std::string& address, IMessageHandlerPtr handler)
    {
        auto networkAddress = Resolve(address);
        ucp_ep_params_t params{};
        params.field_mask = UCP_EP_PARAM_FIELD_SOCK_ADDR |
            UCP_EP_PARAM_FIELD_FLAGS |
            UCP_EP_PARAM_FIELD_ERR_HANDLER |
            UCP_EP_PARAM_FIELD_ERR_HANDLING_MODE;
        params.flags = UCP_EP_PARAMS_FLAGS_CLIENT_SERVER;
        params.sockaddr.addr = networkAddress.GetSockAddr();
        params.sockaddr.addrlen = networkAddress.GetLength();
        params.err_handler.cb = &TEngine::OnEndpointError;
        params.err_handler.arg = this;
        params.err_mode = UCP_ERR_HANDLING_MODE_PEER;
        ucp_ep_h endpoint = nullptr;
        CheckUcx(ucp_ep_create(Worker_, &params, &endpoint), "Cannot create UCX endpoint");
        auto bus = New<TUcxBus>(this, endpoint, address, std::move(handler));
        AddBus(endpoint, bus);
        return bus;
    }

    void Listen(int port, IMessageHandlerPtr handler)
    {
        ServerHandler_ = std::move(handler);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        address.sin_port = htons(port);
        ucp_listener_params_t params{};
        params.field_mask = UCP_LISTENER_PARAM_FIELD_SOCK_ADDR |
            UCP_LISTENER_PARAM_FIELD_CONN_HANDLER;
        params.sockaddr.addr = reinterpret_cast<const sockaddr*>(&address);
        params.sockaddr.addrlen = sizeof(address);
        params.conn_handler.cb = &TEngine::OnConnectionRequest;
        params.conn_handler.arg = this;
        CheckUcx(ucp_listener_create(ListenerWorker_, &params, &Listener_), "Cannot create UCX listener");
    }

    TFuture<void> Send(ucp_ep_h endpoint, TSharedRefArray message)
    {
        struct TSendContext
        {
            TSharedMutableRef Payload;
            i64 MessageSize = 0;
            TPromise<void> Promise = NewPromise<void>();
        };
        auto context = std::make_unique<TSendContext>();
        context->MessageSize = GetByteSize(message);
        context->Payload = SerializeMessage(message);
        auto future = context->Promise.ToFuture();
        ucp_request_param_t params{};
        params.op_attr_mask = UCP_OP_ATTR_FIELD_CALLBACK |
            UCP_OP_ATTR_FIELD_USER_DATA |
            UCP_OP_ATTR_FIELD_FLAGS;
        params.cb.send = [] (void* request, ucs_status_t status, void* userData) {
            std::unique_ptr<TSendContext> context(static_cast<TSendContext*>(userData));
            if (status == UCS_OK) {
                UcxSentBytes.Increment(context->MessageSize);
                UcxSentMessages.Increment();
                context->Promise.Set();
            } else {
                context->Promise.Set(MakeUcxError("UCX send failed", status));
            }
            ucp_request_free(request);
        };
        params.user_data = context.get();
        params.flags = UCP_AM_SEND_FLAG_REPLY;
        auto* request = ucp_am_send_nbx(
            endpoint,
            ActiveMessageId,
            nullptr,
            0,
            context->Payload.Begin(),
            context->Payload.Size(),
            &params);
        if (UCS_PTR_IS_ERR(request)) {
            context->Promise.Set(MakeUcxError("Cannot start UCX send", UCS_PTR_STATUS(request)));
        } else if (request == nullptr) {
            UcxSentBytes.Increment(context->MessageSize);
            UcxSentMessages.Increment();
            context->Promise.Set();
        } else {
            context.release();
        }
        return future;
    }

    void RemoveBus(ucp_ep_h endpoint)
    {
        std::lock_guard guard(Lock_);
        Buses_.erase(endpoint);
        OwnedBuses_.erase(endpoint);
    }

private:
    ucp_context_h Context_ = nullptr;
    ucp_worker_h Worker_ = nullptr;
    ucp_worker_h ListenerWorker_ = nullptr;
    ucp_listener_h Listener_ = nullptr;
    IMessageHandlerPtr ServerHandler_;
    std::atomic<bool> Stopping_ = false;
    std::thread ProgressThread_;
    std::mutex Lock_;
    THashMap<ucp_ep_h, TWeakPtr<TUcxBus>> Buses_;
    THashMap<ucp_ep_h, TUcxBusPtr> OwnedBuses_;

    static NNet::TNetworkAddress Resolve(const std::string& address)
    {
        TStringBuf host;
        int port;
        NNet::ParseServiceAddress(address, &host, &port);
        addrinfo hints{};
        // The server listener is deliberately IPv4-only. Asking the resolver for
        // the same family avoids IPv4-mapped IPv6 addresses, which UCX sockcm
        // cannot reliably map back to the selected data device.
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        addrinfo* result = nullptr;
        auto service = ToString(port);
        int error = getaddrinfo(std::string(host).c_str(), service.c_str(), &hints, &result);
        if (error != 0) {
            THROW_ERROR_EXCEPTION("Cannot resolve UCX endpoint %Qv: %v", address, gai_strerror(error));
        }
        std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> holder(result, &freeaddrinfo);
        return NNet::TNetworkAddress(*result->ai_addr, result->ai_addrlen);
    }

    void AddBus(ucp_ep_h endpoint, const TUcxBusPtr& bus, bool own = false)
    {
        std::lock_guard guard(Lock_);
        Buses_.emplace(endpoint, MakeWeak(bus));
        if (own) {
            OwnedBuses_.emplace(endpoint, bus);
        }
    }

    TUcxBusPtr FindBus(ucp_ep_h endpoint)
    {
        std::lock_guard guard(Lock_);
        auto it = Buses_.find(endpoint);
        return it == Buses_.end() ? nullptr : it->second.Lock();
    }

    static void OnConnectionRequest(ucp_conn_request_h request, void* arg)
    {
        auto* self = static_cast<TEngine*>(arg);
        ucp_ep_params_t params{};
        params.field_mask = UCP_EP_PARAM_FIELD_CONN_REQUEST |
            UCP_EP_PARAM_FIELD_ERR_HANDLER |
            UCP_EP_PARAM_FIELD_ERR_HANDLING_MODE;
        params.conn_request = request;
        params.err_handler.cb = &TEngine::OnEndpointError;
        params.err_handler.arg = self;
        params.err_mode = UCP_ERR_HANDLING_MODE_PEER;
        ucp_ep_h endpoint = nullptr;
        auto status = ucp_ep_create(self->Worker_, &params, &endpoint);
        if (status != UCS_OK) {
            YT_LOG_ERROR(MakeUcxError("Cannot accept UCX endpoint", status));
            return;
        }
        auto bus = New<TUcxBus>(self, endpoint, "ucx-peer", self->ServerHandler_);
        self->AddBus(endpoint, bus, /*own*/ true);
    }

    static void OnEndpointError(void* arg, ucp_ep_h endpoint, ucs_status_t status)
    {
        auto* self = static_cast<TEngine*>(arg);
        if (auto bus = self->FindBus(endpoint)) {
            bus->Terminate(MakeUcxError("UCX endpoint failed", status));
        }
    }

    struct TReceiveContext
    {
        TEngine* Engine;
        ucp_ep_h Endpoint;
        TSharedMutableRef Payload;
    };

    static ucs_status_t OnActiveMessage(
        void* arg,
        const void*,
        size_t,
        void* data,
        size_t length,
        const ucp_am_recv_param_t* params)
    {
        auto* self = static_cast<TEngine*>(arg);
        auto endpoint = params->recv_attr & UCP_AM_RECV_ATTR_FIELD_REPLY_EP
            ? params->reply_ep
            : nullptr;
        if (!endpoint || !self->FindBus(endpoint)) {
            return UCS_OK;
        }
        auto payload = TSharedMutableRef::Allocate(length, {.InitializeStorage = false});
        if (params->recv_attr & UCP_AM_RECV_ATTR_FLAG_RNDV) {
            auto context = std::make_unique<TReceiveContext>(TReceiveContext{self, endpoint, payload});
            ucp_request_param_t receiveParams{};
            receiveParams.op_attr_mask = UCP_OP_ATTR_FIELD_CALLBACK | UCP_OP_ATTR_FIELD_USER_DATA;
            receiveParams.cb.recv_am = [] (void* request, ucs_status_t status, size_t, void* userData) {
                std::unique_ptr<TReceiveContext> context(static_cast<TReceiveContext*>(userData));
                if (status == UCS_OK) {
                    context->Engine->HandlePayload(context->Endpoint, context->Payload);
                }
                ucp_request_free(request);
            };
            receiveParams.user_data = context.get();
            auto* request = ucp_am_recv_data_nbx(self->Worker_, data, payload.Begin(), length, &receiveParams);
            if (UCS_PTR_IS_ERR(request)) {
                return UCS_PTR_STATUS(request);
            }
            if (request == nullptr) {
                self->HandlePayload(endpoint, payload);
            } else {
                context.release();
            }
            return UCS_INPROGRESS;
        }
        std::memcpy(payload.Begin(), data, length);
        self->HandlePayload(endpoint, payload);
        return UCS_OK;
    }

    void HandlePayload(ucp_ep_h endpoint, TSharedRef payload)
    {
        if (auto bus = FindBus(endpoint)) {
            bus->HandlePayload(std::move(payload));
        }
    }
};

TUcxBus::TUcxBus(TEngine* engine, ucp_ep_h endpoint, std::string endpointDescription, IMessageHandlerPtr handler)
    : Engine_(engine)
    , Endpoint_(endpoint)
    , EndpointDescription_(std::move(endpointDescription))
    , EndpointAddress_(EndpointDescription_)
    , EndpointAttributes_(ConvertToAttributes(BuildYsonStringFluently()
        .BeginMap()
            .Item("address").Value(EndpointDescription_)
            .Item("transport").Value("ucx")
        .EndMap()))
    , Handler_(std::move(handler))
{ }

TUcxBus::~TUcxBus()
{
    Terminate(TError(NBus::EErrorCode::TransportError, "UCX bus destroyed"));
}

const std::string& TUcxBus::GetEndpointDescription() const { return EndpointDescription_; }
const IAttributeDictionary& TUcxBus::GetEndpointAttributes() const { return *EndpointAttributes_; }
const std::string& TUcxBus::GetEndpointAddress() const { return EndpointAddress_; }
const NNet::TNetworkAddress& TUcxBus::GetEndpointNetworkAddress() const { return NNet::NullNetworkAddress; }
TBusNetworkStatistics TUcxBus::GetNetworkStatistics() const { return {}; }
bool TUcxBus::IsEndpointLocal() const { return false; }
bool TUcxBus::IsEncrypted() const { return false; }
TFuture<void> TUcxBus::GetReadyFuture() const { return MakeFuture<void>(TError{}); }
TFuture<void> TUcxBus::Send(TSharedRefArray message, const TSendOptions&)
{
    if (IsTerminated_.load()) {
        std::lock_guard guard(StateLock_);
        return MakeFuture<void>(TerminationError_);
    }
    return Engine_->Send(Endpoint_, std::move(message));
}
void TUcxBus::SetTosLevel(TTosLevel) { }
ucp_ep_h TUcxBus::GetEndpoint() const { return Endpoint_; }

void TUcxBus::Terminate(const TError& error)
{
    {
        std::lock_guard guard(StateLock_);
        if (IsTerminated_.exchange(true)) {
            return;
        }
        TerminationError_ = error;
    }
    Engine_->RemoveBus(Endpoint_);
    ucp_request_param_t params{};
    params.op_attr_mask = UCP_OP_ATTR_FIELD_FLAGS;
    params.flags = UCP_EP_CLOSE_FLAG_FORCE;
    auto* request = ucp_ep_close_nbx(Endpoint_, &params);
    if (request && !UCS_PTR_IS_ERR(request)) {
        ucp_request_free(request);
    }
    Terminated_.Fire(error);
}

void TUcxBus::HandlePayload(TSharedRef payload)
{
    auto messageOrError = DeserializeMessage(std::move(payload));
    if (!messageOrError.IsOK()) {
        Terminate(messageOrError);
        return;
    }
    UcxReceivedBytes.Increment(GetByteSize(messageOrError.Value()));
    UcxReceivedMessages.Increment();
    Handler_->HandleMessage(std::move(messageOrError.Value()), MakeStrong(this), nullptr, TPacketId::Create());
}

class TBusClient
    : public IBusClient
{
public:
    explicit TBusClient(TBusClientConfigPtr config)
        : Config_(std::move(config))
        , EndpointDescription_(Config_->Address.value_or("<unconfigured>"))
        , EndpointAttributes_(ConvertToAttributes(BuildYsonStringFluently()
            .BeginMap().Item("address").Value(EndpointDescription_).Item("transport").Value("ucx").EndMap()))
        , Engine_(GetSharedEngine(Config_->Transports))
    { }

    const std::string& GetEndpointDescription() const override { return EndpointDescription_; }
    const IAttributeDictionary& GetEndpointAttributes() const override { return *EndpointAttributes_; }
    void Reconfigure(const NBus::NTcp::TBusClientDynamicConfigPtr&) override { }
    IBusPtr CreateBus(IMessageHandlerPtr handler, const TCreateBusOptions&) override
    {
        if (!Config_->Address) {
            THROW_ERROR_EXCEPTION("UCX client address is not configured");
        }
        return Engine_->Connect(*Config_->Address, std::move(handler));
    }

private:
    const TBusClientConfigPtr Config_;
    const std::string EndpointDescription_;
    const IAttributeDictionaryPtr EndpointAttributes_;
    std::shared_ptr<TEngine> Engine_;

    static std::shared_ptr<TEngine> GetSharedEngine(const std::string& transports)
    {
        static std::mutex lock;
        static THashMap<std::string, std::weak_ptr<TEngine>> engines;

        std::lock_guard guard(lock);
        if (auto engine = engines[transports].lock()) {
            return engine;
        }
        auto engine = std::make_shared<TEngine>(transports);
        engines[transports] = engine;
        return engine;
    }
};

class TBusServer
    : public IBusServer
{
public:
    explicit TBusServer(TBusServerConfigPtr config)
        : Config_(std::move(config))
        , Engine_(std::make_unique<TEngine>(Config_->Transports))
    { }

    void Start(IMessageHandlerPtr handler) override
    {
        if (!Config_->Port) {
            THROW_ERROR_EXCEPTION("UCX server port is not configured");
        }
        Engine_->Listen(*Config_->Port, std::move(handler));
    }

    TFuture<void> Stop() override
    {
        Engine_.reset();
        return MakeFuture<void>(TError{});
    }

    IYPathServicePtr GetOrchidService() const override
    {
        return IYPathService::FromProducer(BIND([port = Config_->Port] (NYson::IYsonConsumer* consumer) {
            BuildYsonFluently(consumer).BeginMap().Item("port").Value(port).EndMap();
        }));
    }

private:
    const TBusServerConfigPtr Config_;
    std::unique_ptr<TEngine> Engine_;
};

} // namespace

IBusClientPtr CreateBusClient(TBusClientConfigPtr config)
{
    return New<TBusClient>(std::move(config));
}

IBusServerPtr CreateBusServer(TBusServerConfigPtr config)
{
    return New<TBusServer>(std::move(config));
}

} // namespace NYT::NBus::NUcx
