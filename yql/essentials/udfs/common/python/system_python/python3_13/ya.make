YQL_PYTHON3_UDF(systempython3_13_udf)

REGISTER_YQL_PYTHON_UDF(
    NAME SystemPython3_13
    RESOURCE_NAME SystemPython3_13
)

IF (USE_LOCAL_PYTHON)
    LDFLAGS("-lpython3.13")
ENDIF()

SET(EXPORTS_FILE ${ARCADIA_ROOT}/yql/essentials/udfs/common/python/system_python/backend.exports)

SRCS(
    ../backend.cpp
)

PEERDIR(
    yql/essentials/public/udf
)

END()
