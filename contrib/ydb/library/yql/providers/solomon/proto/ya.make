PROTO_LIBRARY()

SRCS(
    dq_solomon_shard.proto
    metrics_queue.proto
)

PEERDIR(
    contrib/ydb/library/yql/dq/actors/protos
)

EXCLUDE_TAGS(GO_PROTO)

END()
