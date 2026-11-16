YQL_PYTHON3_UDF(systempython3_12_udf)

REGISTER_YQL_PYTHON_UDF(
    NAME SystemPython3_12
    RESOURCE_NAME SystemPython3_12
)

IF (USE_LOCAL_PYTHON)
    LDFLAGS("-lpython3.12")
ENDIF()

SET(EXPORTS_FILE ${ARCADIA_ROOT}/yql/essentials/udfs/common/python/system_python/backend.exports)

SRCS(
    ../backend.cpp
)

PEERDIR(
    yql/essentials/public/udf
)

END()
