#pragma once

#include "config.h"

#include <yt/yt/core/bus/server.h>

namespace NYT::NBus::NUcx {

IBusServerPtr CreateBusServer(TBusServerConfigPtr config);

} // namespace NYT::NBus::NUcx
