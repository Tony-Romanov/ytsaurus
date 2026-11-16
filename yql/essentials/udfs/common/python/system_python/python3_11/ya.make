YQL_PYTHON3_UDF(systempython3_11_udf)

REGISTER_YQL_PYTHON_UDF(
    NAME SystemPython3_11
    RESOURCE_NAME SystemPython3_11
)

IF (USE_LOCAL_PYTHON)
    LDFLAGS("-lpython3.11")
ENDIF()

SET(EXPORTS_FILE ${ARCADIA_ROOT}/yql/essentials/udfs/common/python/system_python/backend.exports)

SRCS(
    ../backend.cpp
)

PEERDIR(
    yql/essentials/public/udf
)

END()
