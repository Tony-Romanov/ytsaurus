PY3TEST()

PEERDIR(
    contrib/python/pytest-asyncio
    contrib/python/pytest-mock
)

TEST_SRCS(
    test_pytest_mock.py
)

NO_LINT()

END()
