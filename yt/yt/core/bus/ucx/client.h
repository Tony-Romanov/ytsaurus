#pragma once

#include "config.h"

#include <yt/yt/core/bus/client.h>

namespace NYT::NBus::NUcx {

IBusClientPtr CreateBusClient(TBusClientConfigPtr config);

} // namespace NYT::NBus::NUcx
