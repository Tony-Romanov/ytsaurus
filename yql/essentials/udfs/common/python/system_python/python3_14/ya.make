YQL_PYTHON3_UDF(systempython3_14_udf)

REGISTER_YQL_PYTHON_UDF(
    NAME SystemPython3_14
    RESOURCE_NAME SystemPython3_14
)

IF (USE_LOCAL_PYTHON)
    LDFLAGS("-lpython3.14")
ENDIF()

SET(EXPORTS_FILE ${ARCADIA_ROOT}/yql/essentials/udfs/common/python/system_python/backend.exports)

SRCS(
    ../backend.cpp
)

PEERDIR(
    yql/essentials/public/udf
)

END()
