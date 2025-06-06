#pragma once
#include "selector/abstract/selector.h"
#include "storage/abstract/storage.h"

#include <contrib/ydb/core/tx/columnshard/bg_tasks/abstract/control.h>
#include <contrib/ydb/core/tx/columnshard/bg_tasks/abstract/task.h>
#include <contrib/ydb/core/tx/columnshard/export/common/identifier.h>
#include <contrib/ydb/core/tx/columnshard/common/path_id.h>
#include <contrib/ydb/core/formats/arrow/serializer/abstract.h>
#include <contrib/ydb/core/tx/columnshard/export/protos/task.pb.h>

#include <contrib/ydb/library/conclusion/status.h>
#include <contrib/ydb/library/conclusion/result.h>

namespace NKikimr::NOlap::NExport {

class TExportTask: public NBackgroundTasks::TInterfaceProtoAdapter<NKikimrColumnShardExportProto::TExportTask, NBackground::ITaskDescription> {
public:
    static TString GetClassNameStatic() {
        return "CS::EXPORT";
    }
private:
    TIdentifier Identifier = TIdentifier(TInternalPathId{});
    YDB_READONLY_DEF(TSelectorContainer, Selector);
    YDB_READONLY_DEF(TStorageInitializerContainer, StorageInitializer);
    YDB_READONLY_DEF(NArrow::NSerialization::TSerializerContainer, Serializer);
    YDB_READONLY_DEF(std::optional<ui64>, TxId);

    virtual TConclusionStatus DoDeserializeFromProto(const NKikimrColumnShardExportProto::TExportTask& proto) override;
    virtual NKikimrColumnShardExportProto::TExportTask DoSerializeToProto() const override;
    static const inline TFactory::TRegistrator<TExportTask> Registrator = TFactory::TRegistrator<TExportTask>(GetClassNameStatic());
    virtual std::shared_ptr<NBackground::ISessionLogic> DoBuildSession() const override;

public:



    virtual TString GetClassName() const override {
        return GetClassNameStatic();
    }

    NBackground::TSessionControlContainer BuildConfirmControl() const;
    NBackground::TSessionControlContainer BuildAbortControl() const;

    const TIdentifier& GetIdentifier() const {
        return Identifier;
    }

    TExportTask() = default;

    TExportTask(const TIdentifier& id, const TSelectorContainer& selector, const TStorageInitializerContainer& storageInitializer, 
        const NArrow::NSerialization::TSerializerContainer& serializer, const std::optional<ui64> txId = {})
        : Identifier(id)
        , Selector(selector)
        , StorageInitializer(storageInitializer)
        , Serializer(serializer)
        , TxId(txId)
    {
    }

    TString DebugString() const {
        return TStringBuilder() << "{task_id=" << Identifier.DebugString() << ";selector=" << Selector.DebugString() << ";}";
    }
};
}
