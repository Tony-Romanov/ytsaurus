#include "optimizer.h"
#include <contrib/ydb/core/tx/columnshard/engines/storage/optimizer/sbuckets/logic/one_head/logic.h>

namespace NKikimr::NOlap::NStorageOptimizer::NSBuckets {

TOptimizerPlanner::TOptimizerPlanner(const TInternalPathId pathId, const std::shared_ptr<IStoragesManager>& storagesManager, const std::shared_ptr<arrow::Schema>& primaryKeysSchema, const std::shared_ptr<IOptimizationLogic>& logic)
    : TBase(pathId)
    , Counters(std::make_shared<TCounters>())
    , PrimaryKeysSchema(primaryKeysSchema)
    , Buckets(primaryKeysSchema, storagesManager, Counters, logic)
    , StoragesManager(storagesManager)
{

}

}
