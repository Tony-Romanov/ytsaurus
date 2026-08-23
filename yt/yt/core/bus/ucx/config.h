#pragma once

#include "public.h"

#include <yt/yt/core/ytree/yson_struct.h>

namespace NYT::NBus::NUcx {

struct TBusConfig
    : public NYTree::TYsonStruct
{
    //! Comma-separated UCX_TLS allow list. The transports built into YTsaurus are:
    //! - rc (rc_mlx5 and rc_verbs): reliable RDMA connection;
    //! - dc (dc_mlx5): dynamically connected Mellanox RDMA transport;
    //! - ud (ud_mlx5 and ud_verbs): unreliable-datagram RDMA transport;
    //! - tcp: TCP sockets managed by UCX;
    //! - sm/shm/mm (posix and sysv): shared memory between local processes;
    //! - self: loopback within one UCP worker; it is not usable for the current
    //!   client/server node-to-node integration.
    //! The ib alias enables all built RDMA transports, while all enables every
    //! built transport. Exact transport names listed in parentheses are accepted too.
    std::string Transports;

    REGISTER_YSON_STRUCT(TBusConfig);
    static void Register(TRegistrar registrar);
};

DEFINE_REFCOUNTED_TYPE(TBusConfig)

struct TBusClientConfig
    : public TBusConfig
{
    std::optional<std::string> Address;

    REGISTER_YSON_STRUCT(TBusClientConfig);
    static void Register(TRegistrar registrar);
};

DEFINE_REFCOUNTED_TYPE(TBusClientConfig)

struct TBusServerConfig
    : public TBusConfig
{
    std::optional<int> Port;

    REGISTER_YSON_STRUCT(TBusServerConfig);
    static void Register(TRegistrar registrar);
};

DEFINE_REFCOUNTED_TYPE(TBusServerConfig)

} // namespace NYT::NBus::NUcx
