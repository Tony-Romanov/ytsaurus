GTEST(unittester-core-bus-ucx)

INCLUDE(${ARCADIA_ROOT}/yt/ya_cpp.make.inc)

SRCS(
    ucx_bus_ut.cpp
)

PEERDIR(
    yt/yt/core
    yt/yt/core/bus/ucx
    yt/yt/core/bus/unittests/lib
    yt/yt/core/test_framework
    library/cpp/testing/common
)

SIZE(MEDIUM)

ENV(UCX_NET_DEVICES=lo)

END()
