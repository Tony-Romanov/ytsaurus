#include "schemeshard__operation_part.h"
#include "schemeshard_impl.h"

#include <contrib/ydb/core/base/auth.h>

namespace {

using namespace NKikimr;
using namespace NSchemeShard;

bool CheckSidExistsOrIsNonYdb(const std::unordered_map<TString, NLogin::TLoginProvider::TSidRecord>& sids, const TString& sid) {
    // non-YDB user's sid format is <login>@<subsystem>
    return sid.Contains('@') || sids.contains(sid);
}

class TModifyACL: public TSubOperationBase {
public:
    using TSubOperationBase::TSubOperationBase;

    THolder<TProposeResponse> Propose(const TString&, TOperationContext& context) override {
        const TTabletId ssId = context.SS->SelfTabletId();
        const TString databaseName = CanonizePath(context.SS->RootPathElements);

        const TString& parentPathStr = Transaction.GetWorkingDir();
        const auto& op = Transaction.GetModifyACL();
        const auto& name = op.GetName();
        const auto& acl = op.GetDiffACL();
        const auto& owner = op.GetNewOwner();

        LOG_NOTICE_S(context.Ctx, NKikimrServices::FLAT_TX_SCHEMESHARD, "TModifyACL Propose"
            << ", path: " << parentPathStr << "/" << name
            << ", operationId: " << OperationId
            << ", at schemeshard: " << ssId);

        auto result = MakeHolder<TProposeResponse>(NKikimrScheme::StatusSuccess, ui64(OperationId.GetTxId()), ui64(ssId));

        const auto path = TPath::Resolve(parentPathStr, context.SS).Dive(name);
        {
            const auto checks = path.Check();
            checks
                .NotEmpty()
                .NotUnderDomainUpgrade()
                .IsAtLocalSchemeShard()
                .IsResolved()
                .NotDeleted()
                .NotUnderDeleting()
                .IsCommonSensePath()
                .IsValidACL(acl);

            if (!checks) {
                result->SetError(checks.GetStatus(), checks.GetError());
                return result;
            }
        }

        TString errStr;
        if (!context.SS->CheckApplyIf(Transaction, errStr, path->PathType)) {
            result->SetError(NKikimrScheme::StatusPreconditionFailed, errStr);
            return result;
        }

        bool isAdmin = (context.UserToken && IsAdministrator(AppData(), context.UserToken.Get()));

        if (acl && AppData()->FeatureFlags.GetEnableStrictAclCheck()) {
            NACLib::TDiffACL diffACL(acl);
            for (const NACLibProto::TDiffACE& diffACE : diffACL.GetDiffACE()) {
                if (static_cast<NACLib::EDiffType>(diffACE.GetDiffType()) == NACLib::EDiffType::Add) {
                    // add diff type is allowed if:
                    // - subject is a cluster administrator
                    // - or target sid is an external one (not a ydb-local)
                    // - or target sid is a local one and exist in this database
                    const auto& targetSid = diffACE.GetACE().GetSID();
                    bool allowed = (isAdmin || CheckSidExistsOrIsNonYdb(context.SS->LoginProvider.Sids, targetSid));
                    if (!allowed) {
                        result->SetError(NKikimrScheme::StatusPreconditionFailed,
                            TStringBuilder() << "SID " << targetSid << " not found in database `" << databaseName << "`");
                        return result;
                    }
                } // remove diff type is allowed in any case
            }
        }
        if (owner && AppData()->FeatureFlags.GetEnableStrictAclCheck()) {
            // ownership transfer is allowed if:
            // - subject is a cluster administrator
            // - or target sid is an external one (not a ydb-local)
            // - or target sid is a local one and exist in this database
            bool allowed = (isAdmin || CheckSidExistsOrIsNonYdb(context.SS->LoginProvider.Sids, owner));
            if (!allowed) {
                result->SetError(NKikimrScheme::StatusPreconditionFailed,
                    TStringBuilder() << "Owner SID " << owner << " not found in database `" << databaseName << "`");
                return result;
            }
        }

        THashSet<TPathId> subTree;
        if (acl || (owner && path.Base()->IsTable())) {
            subTree = context.SS->ListSubTree(path.Base()->PathId, context.Ctx);
        }

        THashSet<TPathId> affectedPaths;
        NIceDb::TNiceDb db(context.GetDB());

        if (acl) {
            ++path.Base()->ACLVersion;
            path.Base()->ApplyACL(acl);
            context.SS->PersistACL(db, path.Base());

            for (const auto& pathId : subTree) {
                context.OnComplete.PublishToSchemeBoard(OperationId, pathId);
            }

            affectedPaths.insert(subTree.begin(), subTree.end());
        }

        if (owner) {
            THashSet<TPathId> pathIds = {path.Base()->PathId};
            if (path.Base()->IsTable()) {
                pathIds = subTree;
            }

            for (const auto& pathId : pathIds) {
                if (!context.SS->PathsById.contains(pathId)) {
                    Y_VERIFY_DEBUG_S(false, "unreachable");
                    continue;
                }

                auto pathEl = context.SS->PathsById.at(pathId);

                pathEl->Owner = owner;
                context.SS->PersistOwner(db, pathEl);

                ++pathEl->DirAlterVersion;
                context.SS->PersistPathDirAlterVersion(db, pathEl);

                context.SS->ClearDescribePathCaches(pathEl);
                context.OnComplete.PublishToSchemeBoard(OperationId, pathId);
            }

            affectedPaths.insert(pathIds.begin(), pathIds.end());
        }

        if ((acl && !path.Base()->IsPQGroup()) || owner) {
            const auto parent = path.Parent();
            ++parent.Base()->DirAlterVersion;
            context.SS->PersistPathDirAlterVersion(db, parent.Base());
            context.SS->ClearDescribePathCaches(parent.Base());
            context.OnComplete.PublishToSchemeBoard(OperationId, parent.Base()->PathId);
        }

        context.OnComplete.UpdateTenants(std::move(affectedPaths));
        context.OnComplete.DoneOperation(OperationId);

        return result;
    }

    void AbortPropose(TOperationContext&) override {
        Y_ABORT("no AbortPropose for TModifyACL");
    }

    bool ProgressState(TOperationContext&) override {
        Y_ABORT("no ProgressState for TModifyACL");
    }

    void AbortUnsafe(TTxId, TOperationContext&) override {
        Y_ABORT("no AbortUnsafe for TModifyACL");
    }
};

}

namespace NKikimr::NSchemeShard {

ISubOperation::TPtr CreateModifyACL(TOperationId id, const TTxTransaction& tx) {
    return MakeSubOperation<TModifyACL>(id, tx);
}

ISubOperation::TPtr CreateModifyACL(TOperationId id, TTxState::ETxState state) {
    Y_ABORT_UNLESS(state == TTxState::Invalid);
    return MakeSubOperation<TModifyACL>(id);
}

}
