#pragma once
#include <contrib/ydb/core/tx/columnshard/engines/storage/optimizer/sbuckets/logic/abstract/logic.h>

namespace NKikimr::NOlap::NStorageOptimizer::NSBuckets {

class TTimeSliceLogic: public IOptimizationLogic {
private:
    TDuration FreshnessCheckDuration = TDuration::Seconds(300);

    std::vector<TPortionInfo::TConstPtr> GetPortionsForMerge(const TInstant now, const ui64 memLimit, const TBucketInfo& bucket) const;

    virtual TCalcWeightResult DoCalcWeight(const TInstant now, const TBucketInfo& bucket) const override;

    virtual TCompactionTaskResult DoBuildTask(const TInstant now, const ui64 memLimit, const TBucketInfo& bucket) const override;
public:
    TTimeSliceLogic(const TDuration freshnessCheckDuration)
        : FreshnessCheckDuration(freshnessCheckDuration)
    {

    }
};

}   // namespace NKikimr::NOlap::NStorageOptimizer::NSBuckets
