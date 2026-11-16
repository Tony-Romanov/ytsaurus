YQL_PYTHON3_UDF(systempython3_8_udf)

REGISTER_YQL_PYTHON_UDF(
    NAME SystemPython3_8
    RESOURCE_NAME SystemPython3_8
)

IF (USE_LOCAL_PYTHON)
    LDFLAGS("-lpython3.8")
ENDIF()

SET(EXPORTS_FILE ${ARCADIA_ROOT}/yql/essentials/udfs/common/python/system_python/backend.exports)

SRCS(
    ../backend.cpp
)

PEERDIR(
    yql/essentials/public/udf
)

END()
