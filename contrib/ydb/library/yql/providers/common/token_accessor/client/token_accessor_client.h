#pragma once

#include <contrib/ydb/library/yql/providers/common/token_accessor/grpc/token_accessor_pb.grpc.pb.h>
#include <contrib/ydb/public/sdk/cpp/src/library/grpc/client/grpc_client_low.h>

#include <contrib/ydb/public/sdk/cpp/include/ydb-cpp-sdk/client/types/credentials/credentials.h>
#include <util/datetime/base.h>

namespace NYql {

std::shared_ptr<NYdb::ICredentialsProvider> CreateTokenAccessorCredentialsProvider(
    const TString& tokenAccessorEndpoint,
    bool useSsl,
    const TString& sslCaCert,
    const TString& serviceAccountId,
    const TString& serviceAccountIdSignature,
    const TDuration& refreshPeriod = TDuration::Hours(1),
    const TDuration& requestTimeout = TDuration::Seconds(10)
);

std::shared_ptr<NYdb::ICredentialsProvider> CreateTokenAccessorCredentialsProvider(
    std::shared_ptr<NYdbGrpc::TGRpcClientLow> client,
    std::shared_ptr<NYdbGrpc::TServiceConnection<TokenAccessorService>> connection,
    const TString& serviceAccountId,
    const TString& serviceAccountIdSignature,
    const TDuration& refreshPeriod = TDuration::Hours(1),
    const TDuration& requestTimeout = TDuration::Seconds(10)
);

}
