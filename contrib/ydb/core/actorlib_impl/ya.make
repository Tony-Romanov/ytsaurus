LIBRARY()

SRCS(
    actor_tracker.cpp
    actor_tracker.h
    async_destroyer.h
    connect_socket_protocol.cpp
    connect_socket_protocol.h
    defs.h
    destruct_actor.h
    http_request_protocol.h
    long_timer.cpp
    long_timer.h
    name_service_client_protocol.cpp
    name_service_client_protocol.h
    proto_ready_actor.h
    read_data_protocol.cpp
    read_data_protocol.h
    read_http_reply_protocol.cpp
    read_http_reply_protocol.h
    router_rr.h
    send_data_protocol.cpp
    send_data_protocol.h
)

PEERDIR(
    contrib/ydb/library/actors/core
    contrib/ydb/library/actors/dnscachelib
    contrib/ydb/library/actors/protos
    library/cpp/containers/stack_vector
    library/cpp/digest/crc32c
    library/cpp/html/pcdata
    library/cpp/lwtrace
    library/cpp/lwtrace/mon
    library/cpp/messagebus/monitoring
    library/cpp/monlib/dynamic_counters
    library/cpp/monlib/service/pages/resources
    library/cpp/monlib/service/pages/tablesorter
    library/cpp/packedtypes
    library/cpp/sliding_window
    contrib/ydb/core/base
    contrib/ydb/core/driver_lib/version
    contrib/ydb/core/mon
    contrib/ydb/core/node_whiteboard
    contrib/ydb/core/protos
    contrib/ydb/core/util
)

YQL_LAST_ABI_VERSION()

END()

RECURSE_FOR_TESTS(
    ut
)
