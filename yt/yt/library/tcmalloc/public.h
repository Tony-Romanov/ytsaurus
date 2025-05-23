#pragma once

#include <yt/yt/core/misc/configurable_singleton_decl.h>

#include <library/cpp/yt/memory/ref_counted.h>

namespace NYT::NTCMalloc {

////////////////////////////////////////////////////////////////////////////////

DECLARE_REFCOUNTED_STRUCT(TTCMallocConfig)
DECLARE_REFCOUNTED_STRUCT(THeapSizeLimitConfig)
DECLARE_REFCOUNTED_STRUCT(TDynamicTCMallocConfig)
DECLARE_REFCOUNTED_STRUCT(TDynamicHeapSizeLimitConfig)

YT_DECLARE_RECONFIGURABLE_SINGLETON(TTCMallocConfig, TDynamicTCMallocConfig);

////////////////////////////////////////////////////////////////////////////////

} // namespace NYT::NTCMalloc
