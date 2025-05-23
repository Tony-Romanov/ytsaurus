LIBRARY()

SRCS(
    name_set_json.cpp
    name_set.cpp
    name_index.cpp
    name_service.cpp
)

PEERDIR(
    yql/essentials/core/sql_types
    yql/essentials/sql/v1/complete/name/service
    yql/essentials/sql/v1/complete/name/service/ranking
    yql/essentials/sql/v1/complete/text
)

RESOURCE(
    yql/essentials/data/language/pragmas_opensource.json pragmas_opensource.json
    yql/essentials/data/language/types.json types.json
    yql/essentials/data/language/sql_functions.json sql_functions.json
    yql/essentials/data/language/udfs_basic.json udfs_basic.json
    yql/essentials/data/language/statements_opensource.json statements_opensource.json
    yql/essentials/data/language/rules_corr_basic.json rules_corr_basic.json
)

END()
