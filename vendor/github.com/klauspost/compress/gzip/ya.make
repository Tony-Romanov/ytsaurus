GO_LIBRARY()

LICENSE(
    Apache-2.0 AND
    BSD-3-Clause AND
    MIT
)

VERSION(v1.17.11)

SRCS(
    gunzip.go
    gzip.go
)

GO_TEST_SRCS(
    gunzip_test.go
    gzip_test.go
)

GO_XTEST_SRCS(example_test.go)

END()

RECURSE(
    gotest
)
