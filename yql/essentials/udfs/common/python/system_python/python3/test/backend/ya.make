YQL_UDF(systempython_dispatcher_test_udf)

SET(EXPORTS_FILE ${ARCADIA_ROOT}/yql/essentials/udfs/common/python/system_python/backend.exports)

YQL_ABI_VERSION(2 45 0)

SRCS(backend.cpp)

END()
