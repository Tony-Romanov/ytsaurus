#pragma once

#include <contrib/ydb/core/scheme/scheme_tabledefs.h>
#include <contrib/ydb/core/kqp/common/kqp_tx_manager.h>

namespace NKikimr {
namespace NKqp {

struct TKqpBufferWriterSettings {
    TActorId SessionActorId;
    IKqpTransactionManagerPtr TxManager;
    NWilson::TTraceId TraceId;
    TIntrusivePtr<TKqpCounters> Counters;
    TIntrusivePtr<NTxProxy::TTxProxyMon> TxProxyMon;
    std::shared_ptr<NKikimr::NMiniKQL::TScopedAlloc> Alloc;
};

NActors::IActor* CreateKqpBufferWriterActor(TKqpBufferWriterSettings&& settings);

}
}
