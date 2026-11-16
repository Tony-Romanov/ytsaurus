BUILD_ONLY_IF(OS_LINUX)

PY3TEST()

TEST_SRCS(test_dispatcher.py)

IF (USE_ARCADIA_PYTHON)
    ENV(SYSTEM_PYTHON3_TEST_EMBEDDED=1)
ENDIF()

DEPENDS(
    yql/essentials/tools/minirun
    yql/essentials/udfs/common/python/system_python/python3
    yql/essentials/udfs/common/python/system_python/python3/test/backend
    yql/essentials/udfs/common/python/system_python/python3/test/python
)

DATA(arcadia/yql/essentials/udfs/common/python/system_python/python3/pack.py)

END()
