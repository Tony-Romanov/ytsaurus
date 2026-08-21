#pragma once

#include "public.h"

#include <yt/yt/core/ytree/yson_struct.h>

namespace NYT::NBus::NUcx {

struct TBusConfig
    : public NYTree::TYsonStruct
{
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
