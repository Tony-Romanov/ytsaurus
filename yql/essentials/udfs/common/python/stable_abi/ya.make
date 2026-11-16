PY23_NATIVE_LIBRARY()

YQL_ABI_VERSION(2 45 0)

SRCS(type_info.cpp)

# Select the same Python headers as the calling UDF, but use only the Limited API.
CFLAGS(-DPy_LIMITED_API=0x03080000)

IF (USE_ARCADIA_PYTHON)
    CFLAGS(-DSYSTEM_PYTHON3_EMBEDDED)
ENDIF()

PEERDIR(
    library/cpp/resource
    yql/essentials/public/udf
)

RESOURCE(
    bootstrap.py /python/stable_abi/bootstrap.py
    yt/python/yt/type_info/type_base.py /python/stable_abi/type_base.py
    yt/python/yt/type_info/typing.py /python/stable_abi/typing.py
    yql/essentials/udfs/common/python/bindings/typing.py /python/stable_abi/yql_typing.py
)

END()
