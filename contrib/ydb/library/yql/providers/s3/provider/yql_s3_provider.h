#pragma once

#include <yql/essentials/core/yql_data_provider.h>
#include <contrib/ydb/library/yql/providers/common/token_accessor/client/factory.h>
#include <contrib/ydb/library/yql/providers/common/http_gateway/yql_http_default_retry_policy.h>
#include <contrib/ydb/library/yql/providers/common/http_gateway/yql_http_gateway.h>

#include "yql_s3_settings.h"

namespace NKikimr::NMiniKQL {
   class IFunctionRegistry;
}

namespace NYql {

struct TS3State : public TThrRefBase
{
    using TPtr = TIntrusivePtr<TS3State>;

    struct TTableMeta {
        const TStructExprType* ItemType = nullptr;
        TVector<TString> ColumnOrder;
    };

    std::unordered_map<std::pair<TString, TString>, TTableMeta, THash<std::pair<TString, TString>>> Tables;

    TTypeAnnotationContext* Types = nullptr;
    TS3Configuration::TPtr Configuration = MakeIntrusive<TS3Configuration>();
    const NKikimr::NMiniKQL::IFunctionRegistry* FunctionRegistry = nullptr;
    ISecuredServiceAccountCredentialsFactory::TPtr CredentialsFactory;
    IHTTPGateway::TPtr Gateway;
    IHTTPGateway::TRetryPolicy::TPtr GatewayRetryPolicy = GetHTTPDefaultRetryPolicy();
    ui32 ExecutorPoolId = 0;
    std::list<TVector<TString>> PrimaryKeys;
    NActors::TActorSystem* ActorSystem = nullptr;
};

TDataProviderInitializer GetS3DataProviderInitializer(IHTTPGateway::TPtr gateway, ISecuredServiceAccountCredentialsFactory::TPtr credentialsFactory = nullptr, NActors::TActorSystem* actorSystem = nullptr);

TIntrusivePtr<IDataProvider> CreateS3DataSource(TS3State::TPtr state);
TIntrusivePtr<IDataProvider> CreateS3DataSink(TS3State::TPtr state);

} // namespace NYql
