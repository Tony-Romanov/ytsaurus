LIBRARY()

ADDINCL(
    contrib/ydb/library/yql/udfs/common/clickhouse/client/base
    contrib/ydb/library/yql/udfs/common/clickhouse/client/base/pcg-random
    contrib/ydb/library/yql/udfs/common/clickhouse/client/src
)

# Added because of library header contrib/libs/apache/arrow/cpp/src/arrow/util/value_parsing.h
CFLAGS(
    -Wno-unused-parameter
)

SRCS(
    arrow_fetcher.cpp
    arrow_inferencinator.cpp
    infer_config.cpp
)

PEERDIR(
    contrib/libs/apache/arrow

    contrib/ydb/core/external_sources/object_storage

    contrib/ydb/library/yql/providers/s3/compressors
)

END()

RECURSE_FOR_TESTS(
    ut
)
