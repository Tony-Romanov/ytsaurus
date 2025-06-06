LIBRARY()

SRCS(
    compile_context.cpp
    compile_context.h
    compile_result.cpp
    compile_result.h
    db_key_resolver.cpp
    db_key_resolver.h
    mkql_compile_service.cpp
    yql_expr_minikql.cpp
    yql_expr_minikql.h
)

PEERDIR(
    contrib/ydb/library/actors/core
    library/cpp/threading/future
    contrib/ydb/core/base
    contrib/ydb/core/engine
    contrib/ydb/core/kqp/provider
    contrib/ydb/core/scheme
    yql/essentials/ast
    yql/essentials/core
    yql/essentials/minikql
    yql/essentials/providers/common/mkql
)

YQL_LAST_ABI_VERSION()

END()

RECURSE_FOR_TESTS(
    ut
)
