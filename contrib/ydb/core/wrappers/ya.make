LIBRARY()

IF (OS_WINDOWS)
    CFLAGS(
        -DKIKIMR_DISABLE_S3_WRAPPER
    )
ELSE()
    SRCS(
        s3_wrapper.cpp
        s3_storage.cpp
        s3_storage_config.cpp
        abstract.cpp
        fake_storage.cpp
        fake_storage_config.cpp
        unavailable_storage.cpp
    )
    PEERDIR(
        contrib/libs/aws-sdk-cpp/aws-cpp-sdk-s3
        contrib/libs/curl
        contrib/ydb/library/actors/core
        contrib/ydb/core/base
        contrib/ydb/core/protos
        contrib/ydb/core/wrappers/events
    )
ENDIF()

END()

RECURSE_FOR_TESTS(
    ut
    ut_helpers
)
