#pragma once
#include <contrib/ydb/library/accessor/accessor.h>
#include <contrib/ydb/library/conclusion/status.h>
#include <contrib/ydb/library/conclusion/result.h>
#include <contrib/ydb/core/protos/tx_columnshard.pb.h>
#include <contrib/ydb/core/tx/columnshard/common/path_id.h>

namespace NKikimrColumnShardExportProto {
class TIdentifier;
}

namespace NKikimrTxColumnShard {
class TBackupTxBody;
}

namespace NKikimr::NOlap::NExport {

class TIdentifier {
private:
    YDB_READONLY_DEF(TInternalPathId, PathId);

    TIdentifier() = default;
    TConclusionStatus DeserializeFromProto(const NKikimrColumnShardExportProto::TIdentifier& proto);
public:
    TIdentifier(const TInternalPathId pathId)
        : PathId(pathId)
    {

    }

    static TConclusion<TIdentifier> BuildFromProto(const NKikimrTxColumnShard::TBackupTxBody& proto);
    static TConclusion<TIdentifier> BuildFromProto(const NKikimrColumnShardExportProto::TIdentifier& proto);

    NKikimrColumnShardExportProto::TIdentifier SerializeToProto() const;

    TString ToString() const;

    bool operator==(const TIdentifier& id) const {
        return PathId == id.PathId;
    }

    TString DebugString() const;
};

}