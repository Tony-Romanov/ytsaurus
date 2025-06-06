#pragma once
#include <contrib/ydb/core/tx/columnshard/data_accessor/abstract/collector.h>
#include <contrib/ydb/core/tx/columnshard/common/path_id.h>

namespace NKikimr::NOlap::NDataAccessorControl::NInMem {
class TCollector: public IGranuleDataAccessor {
private:
    using TBase = IGranuleDataAccessor;
    THashMap<ui64, TPortionDataAccessor> Accessors;
    virtual void DoAskData(const std::vector<TPortionInfo::TConstPtr>& portions, const std::shared_ptr<IAccessorCallback>& callback,
        const TString& consumer) override;
    virtual TDataCategorized DoAnalyzeData(const std::vector<TPortionInfo::TConstPtr>& portions, const TString& consumer) override;
    virtual void DoModifyPortions(const std::vector<TPortionDataAccessor>& add,
        const std::vector<ui64>& remove) override;

public:
    TCollector(const TInternalPathId pathId)
        : TBase(pathId) {
    }
};

}   // namespace NKikimr::NOlap::NDataAccessorControl::NInMem
