LIBRARY()

SRCS(
    ext_tx_base.cpp
    write_queue.cpp
)

PEERDIR(
    contrib/ydb/core/tx/columnshard/hooks/abstract
)

END()
