BUILD_ONLY_IF(OS_LINUX)

YQL_PYTHON3_UDF(systempython3_udf)

YQL_ABI_VERSION(2 45 0)

SRCS(
    dispatcher.cpp
)

IF (USE_ARCADIA_PYTHON)
    CFLAGS(-DSYSTEM_PYTHON3_EMBEDDED)
    PEERDIR(
        library/python/runtime_py3
        yql/essentials/udfs/common/python/main_py3
        yql/essentials/udfs/common/python/python_udf
    )
ELSEIF (USE_LOCAL_PYTHON)
    # As in python3_X, python-config supplies the library search directories.
    # The dispatcher uses the version-independent library and Stable ABI.
    LDFLAGS(-Wl,--push-state,--no-as-needed -lpython3 -Wl,--pop-state)
ENDIF()

PEERDIR(
    yql/essentials/udfs/common/python/stable_abi
)

END()

RECURSE_FOR_TESTS(
    test
)
