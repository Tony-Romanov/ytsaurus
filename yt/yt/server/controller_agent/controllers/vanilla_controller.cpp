#include "vanilla_controller.h"

#include "job_info.h"
#include "operation_controller_detail.h"
#include "table.h"
#include "task.h"

#include <yt/yt/server/controller_agent/config.h>
#include <yt/yt/server/controller_agent/operation.h>

#include <yt/yt/server/lib/chunk_pools/vanilla_chunk_pool.h>

#include <yt/yt/server/lib/scheduler/helpers.h>

#include <yt/yt/ytlib/controller_agent/proto/job.pb.h>

#include <yt/yt/ytlib/scheduler/proto/resources.pb.h>

#include <yt/yt/ytlib/table_client/public.h>

#include <yt/yt/client/ypath/rich.h>

#include <library/cpp/yt/memory/non_null_ptr.h>

namespace NYT::NControllerAgent::NControllers {

using namespace NChunkPools;
using namespace NConcurrency;
using namespace NControllerAgent::NProto;
using namespace NScheduler::NProto;
using namespace NScheduler;
using namespace NTableClient;
using namespace NYPath;
using namespace NYTree;
using namespace NYson;

using NYT::ToProto;

////////////////////////////////////////////////////////////////////////////////

namespace {

DEFINE_ENUM(EOperationIncarnationSwitchReason,
    (JobAborted)
    (JobFailed)
    (JobInterrupted)
    (JobIncarnationsDifferAfterRevival)
    (JobLackAfterRevival)
);

TEnumIndexedArray<EOperationIncarnationSwitchReason, NProfiling::TCounter> OperationIncarnationSwitchCounters;

NProfiling::TCounter GangOperationStartedCounter;

} // namespace

class TVanillaController;

class TGangManager
{
public:
    //! Used only for persistence.
    TGangManager() = default;
    TGangManager(TGangManager&& other) = default;
    TGangManager& operator=(TGangManager&& other) = default;

    TGangManager(
        TVanillaController* controller,
        const TVanillaOperationOptionsPtr& config);

    const TOperationIncarnation& GetCurrentIncarnation() const noexcept;

    bool IsEnabled() const noexcept;

    void TrySwitchToNewIncarnation(bool operationIsReviving, EOperationIncarnationSwitchReason reason);

    void TrySwitchToNewIncarnation(
        const std::optional<TOperationIncarnation>& consideredIncarnation,
        bool operationIsReviving,
        EOperationIncarnationSwitchReason reason);

    void UpdateConfig(const TVanillaOperationOptionsPtr& config) noexcept;

    // COMPAT(pogorelov)
    void SetVanillaController(TVanillaController* controller) noexcept;

private:
    bool Enabled_ = false;
    TOperationIncarnation Incarnation_;

    TVanillaController* VanillaOperationController_ = nullptr;

    TOperationIncarnation GenerateNewIncarnation();

    PHOENIX_DECLARE_TYPE(TGangManager, 0xa01a5a9b);

    friend class TVanillaController;
};

////////////////////////////////////////////////////////////////////////////////

class TVanillaTask
    : public TTask
{
public:
    TVanillaTask(
        ITaskHostPtr taskHost,
        TVanillaTaskSpecPtr spec,
        TString name,
        std::vector<TOutputStreamDescriptorPtr> outputStreamDescriptors,
        std::vector<TInputStreamDescriptorPtr> inputStreamDescriptors);

    //! Used only for persistence.
    TVanillaTask() = default;

    TString GetTitle() const override;
    TString GetName() const;

    TDataFlowGraph::TVertexDescriptor GetVertexDescriptor() const override;

    IPersistentChunkPoolInputPtr GetChunkPoolInput() const override;

    IPersistentChunkPoolOutputPtr GetChunkPoolOutput() const override;

    TUserJobSpecPtr GetUserJobSpec() const override;

    TExtendedJobResources GetNeededResources(const TJobletPtr& /*joblet*/) const override;

    TExtendedJobResources GetMinNeededResourcesHeavy() const override;

    void BuildJobSpec(TJobletPtr joblet, TJobSpec* jobSpec) override;

    EJobType GetJobType() const override;

    void FinishInput() override;

    TJobFinishedResult OnJobCompleted(TJobletPtr joblet, TCompletedJobSummary& jobSummary) override;

    bool IsJobInterruptible() const override;

    void TrySwitchToNewOperationIncarnation(
        const TJobletPtr& joblet,
        bool operationIsReviving,
        EOperationIncarnationSwitchReason reason);

    bool IsJobRestartingEnabled() const noexcept;

    int GetTargetJobCount() const noexcept;

    IVanillaChunkPoolOutputPtr GetVanillaChunkPool() const noexcept;

    TConfigurator<TVanillaTaskSpec> ConfigureUpdate();

private:
    TVanillaController* VanillaController_ = nullptr;

    TVanillaTaskSpecPtr Spec_;
    TString Name_;

    TJobSpec JobSpecTemplate_;

    //! This chunk pool does not really operate with chunks, it is used as an interface for a job counter in it.
    IVanillaChunkPoolOutputPtr VanillaChunkPool_;

    bool IsInputDataWeightHistogramSupported() const override;

    TJobSplitterConfigPtr GetJobSplitterConfig() const override;

    void InitJobSpecTemplate();

    IChunkPoolOutput::TCookie ExtractCookieForAllocation(const TAllocation& allocation, const TNewJobConstraints& newJobConstraints) final;
    TNewJobConstraints GetNewJobConstraints(const TAllocation& allocation) const final;

    PHOENIX_DECLARE_POLYMORPHIC_TYPE(TVanillaTask, 0x55e9aacd);
};

PHOENIX_DEFINE_TYPE(TVanillaTask);
DEFINE_REFCOUNTED_TYPE(TVanillaTask)
DECLARE_REFCOUNTED_CLASS(TVanillaTask)

////////////////////////////////////////////////////////////////////////////////

class TVanillaController
    : public TOperationControllerBase
{
public:
    TVanillaController(
        TVanillaOperationSpecPtr spec,
        TControllerAgentConfigPtr config,
        TVanillaOperationOptionsPtr options,
        IOperationControllerHostPtr host,
        TOperation* operation);

    //! Used only for persistence.
    TVanillaController() = default;

    void CustomMaterialize() override;

    TString GetLoggingProgress() const override;

    std::vector<TRichYPath> GetInputTablePaths() const override;

    void InitOutputTables() override;

    std::vector<TRichYPath> GetOutputTablePaths() const override;

    std::optional<TRichYPath> GetStderrTablePath() const override;

    TBlobTableWriterConfigPtr GetStderrTableWriterConfig() const override;

    std::optional<TRichYPath> GetCoreTablePath() const override;

    TBlobTableWriterConfigPtr GetCoreTableWriterConfig() const override;

    bool GetEnableCudaGpuCoreDump() const override;

    TStringBuf GetDataWeightParameterNameForJob(EJobType /*jobType*/) const override;

    TYsonStructPtr GetTypedSpec() const override;

    std::vector<EJobType> GetSupportedJobTypesForJobsDurationAnalyzer() const override;

    bool IsCompleted() const override;

    std::vector<TUserJobSpecPtr> GetUserJobSpecs() const override;

    void ValidateRevivalAllowed() const override;

    void ValidateSnapshot() const override;

    void InitUserJobSpec(
        NControllerAgent::NProto::TUserJobSpec* proto,
        const TJobletPtr& joblet) const override;

    bool OnJobCompleted(
        TJobletPtr joblet,
        std::unique_ptr<TCompletedJobSummary> jobSummary) final;

    bool OnJobFailed(
        TJobletPtr joblet,
        std::unique_ptr<TFailedJobSummary> jobSummary) final;

    bool OnJobAborted(
        TJobletPtr joblet,
        std::unique_ptr<TAbortedJobSummary> jobSummary) final;

    TJobletPtr CreateJoblet(
        TTask* task,
        TJobId jobId,
        TString treeId,
        int taskJobIndex,
        std::optional<TString> poolPath,
        bool treeIsTentative) final;

    void UpdateConfig(const TControllerAgentConfigPtr& config) final;

    void TrySwitchToNewOperationIncarnation(
        const TJobletPtr& joblet,
        bool operationIsReviving,
        EOperationIncarnationSwitchReason reason);

    void OnOperationIncarnationChanged(bool operationIsReviving, EOperationIncarnationSwitchReason reason);

    TOperationSpecBasePtr ParseTypedSpec(const INodePtr& spec) const override;
    TOperationSpecBaseConfigurator GetOperationSpecBaseConfigurator() const override;

    using TOperationControllerBase::HasJobUniquenessRequirements;

    bool IsOperationGang() const;

    void AbortJobsByCookies(
        TTask* task,
        const std::vector<NChunkPools::TOutputCookie>& cookies,
        EAbortReason abortReason);

private:
    TVanillaOperationSpecPtr Spec_;
    TVanillaOperationOptionsPtr Options_;

    std::vector<TVanillaTaskPtr> Tasks_;
    std::vector<std::vector<TOutputTablePtr>> TaskOutputTables_;

    std::optional<TGangManager> GangManager_;

    void ValidateOperationLimits();

    TError CheckJobsIncarnationsEqual() const;

    std::optional<EOperationIncarnationSwitchReason> ShouldRestartJobsOnRevival() const;

    void OnOperationRevived() final;
    void BuildControllerInfoYson(NYTree::TFluentMap fluent) const final;

    void TrySwitchToNewOperationIncarnation(bool operationIsReviving, EOperationIncarnationSwitchReason reason);

    void RestartAllRunningJobsPreservingAllocations(bool operationIsReviving);

    PHOENIX_DECLARE_POLYMORPHIC_TYPE(TVanillaController, 0x99fa99ae);
};

PHOENIX_DEFINE_TYPE(TVanillaController);

////////////////////////////////////////////////////////////////////////////////

TGangManager::TGangManager(
    TVanillaController* controller,
    const TVanillaOperationOptionsPtr& options)
    : Enabled_(options->GangManager->Enabled)
    , Incarnation_(GenerateNewIncarnation())
    , VanillaOperationController_(controller)
{
    const auto& Logger = VanillaOperationController_->GetLogger();
    YT_LOG_INFO(
        "Gang manager created (OperationIncarnation: %v, Enabled: %v)",
        Incarnation_,
        Enabled_);
}

void TGangManager::RegisterMetadata(auto&& registrar)
{
    PHOENIX_REGISTER_FIELD(1, Enabled_);

    // COMPAT(pogorelov): Remove after all CAs are 25.1.
    PHOENIX_REGISTER_FIELD(2, Incarnation_,
        .SinceVersion(ESnapshotVersion::OperationIncarnationIsStrongTypedef)
        .WhenMissing([] (TThis* this_, auto& context) {
            auto incarnationStr = Load<TString>(context);
            this_->Incarnation_ = TOperationIncarnation(std::move(incarnationStr));
        }));

    PHOENIX_REGISTER_FIELD(3, VanillaOperationController_,
        .SinceVersion(ESnapshotVersion::MonitoringDescriptorsPreserving));
}

const TOperationIncarnation& TGangManager::GetCurrentIncarnation() const noexcept
{
    return Incarnation_;
}

void TGangManager::TrySwitchToNewIncarnation(bool operationIsReviving, EOperationIncarnationSwitchReason reason)
{
    const auto& Logger = VanillaOperationController_->GetLogger();

    if (!IsEnabled()) {
        YT_LOG_INFO("Switching operation to new incarnation is disabled by config; fail operation");
        VanillaOperationController_->OnOperationFailed(
            TError("Switching gang operation to new incarnation is disabled by config")
            << TErrorAttribute("incarnation_switch_reason", reason));
        return;
    }

    auto oldIncarnation = std::exchange(Incarnation_, GenerateNewIncarnation());

    YT_LOG_INFO(
        "Switching operation to new incarnation (From: %v, To: %v, Reason: %v)",
        oldIncarnation,
        Incarnation_,
        reason);

    VanillaOperationController_->OnOperationIncarnationChanged(operationIsReviving, reason);
}

void TGangManager::TrySwitchToNewIncarnation(
    const std::optional<TOperationIncarnation>& consideredIncarnation,
    bool operationIsReviving,
    EOperationIncarnationSwitchReason reason)
{
    if (consideredIncarnation == Incarnation_) {
        TrySwitchToNewIncarnation(operationIsReviving, reason);
    }
}

void TGangManager::UpdateConfig(const TVanillaOperationOptionsPtr& config) noexcept
{
    if (config->GangManager->Enabled != Enabled_) {
        const auto& Logger = VanillaOperationController_->GetLogger();

        YT_LOG_DEBUG(
            "Reconfiguring gang manager (OldEnabled: %v, NewEnabled: %v)",
            Enabled_,
            config->GangManager->Enabled);

        Enabled_ = config->GangManager->Enabled;
    }
}

void TGangManager::SetVanillaController(TVanillaController* controller) noexcept
{
    VanillaOperationController_ = controller;
}

bool TGangManager::IsEnabled() const noexcept
{
    return Enabled_;
}

TOperationIncarnation TGangManager::GenerateNewIncarnation()
{
    return TOperationIncarnation(ToString(TGuid::Create()));
}

PHOENIX_DEFINE_TYPE(TGangManager);

////////////////////////////////////////////////////////////////////////////////

TVanillaTask::TVanillaTask(
    ITaskHostPtr taskHost,
    TVanillaTaskSpecPtr spec,
    TString name,
    std::vector<TOutputStreamDescriptorPtr> outputStreamDescriptors,
    std::vector<TInputStreamDescriptorPtr> inputStreamDescriptors)
    : TTask(std::move(taskHost), std::move(outputStreamDescriptors), std::move(inputStreamDescriptors))
    , VanillaController_(dynamic_cast<TVanillaController*>(TaskHost_))
    , Spec_(std::move(spec))
    , Name_(std::move(name))
    , VanillaChunkPool_(CreateVanillaChunkPool(TVanillaChunkPoolOptions{
        .JobCount = Spec_->JobCount,
        .RestartCompletedJobs = Spec_->RestartCompletedJobs,
        .Logger = Logger.WithTag("Name: %v", Name_),
    }))
{ }

void TVanillaTask::RegisterMetadata(auto&& registrar)
{
    registrar.template BaseType<TTask>();

    PHOENIX_REGISTER_FIELD(1, Spec_);
    PHOENIX_REGISTER_FIELD(2, Name_);
    PHOENIX_REGISTER_FIELD(3, VanillaChunkPool_);
    PHOENIX_REGISTER_FIELD(4, JobSpecTemplate_);

    registrar.AfterLoad([] (TThis* this_, auto& /*context*/) {
        // COMPAT(pogorelov)
        this_->VanillaChunkPool_->SetJobCount(this_->Spec_->JobCount);

        this_->VanillaController_ = dynamic_cast<TVanillaController*>(this_->TaskHost_);
        YT_VERIFY(this_->VanillaController_);
    });
}

TString TVanillaTask::GetTitle() const
{
    return Format("Vanilla(%v)", Name_);
}

TString TVanillaTask::GetName() const
{
    return Name_;
}

TDataFlowGraph::TVertexDescriptor TVanillaTask::GetVertexDescriptor() const
{
    return GetName();
}

IPersistentChunkPoolInputPtr TVanillaTask::GetChunkPoolInput() const
{
    static IPersistentChunkPoolInputPtr NullPool = nullptr;
    return NullPool;
}

IPersistentChunkPoolOutputPtr TVanillaTask::GetChunkPoolOutput() const
{
    return GetVanillaChunkPool();
}

TUserJobSpecPtr TVanillaTask::GetUserJobSpec() const
{
    return Spec_;
}

TExtendedJobResources TVanillaTask::GetNeededResources(const TJobletPtr& /*joblet*/) const
{
    return GetMinNeededResourcesHeavy();
}

TExtendedJobResources TVanillaTask::GetMinNeededResourcesHeavy() const
{
    TExtendedJobResources result;
    result.SetUserSlots(1);
    result.SetCpu(Spec_->CpuLimit);
    // NB: JobProxyMemory is the only memory that is related to IO. Footprint is accounted below.
    result.SetJobProxyMemory(0);
    result.SetJobProxyMemoryWithFixedWriteBufferSize(0);
    AddFootprintAndUserJobResources(result);
    return result;
}

void TVanillaTask::BuildJobSpec(TJobletPtr joblet, TJobSpec* jobSpec)
{
    YT_ASSERT_INVOKER_AFFINITY(TaskHost_->GetJobSpecBuildInvoker());

    jobSpec->CopyFrom(JobSpecTemplate_);
    AddOutputTableSpecs(jobSpec, joblet);
}

EJobType TVanillaTask::GetJobType() const
{
    return EJobType::Vanilla;
}

void TVanillaTask::FinishInput()
{
    TTask::FinishInput();

    InitJobSpecTemplate();
}

TJobFinishedResult TVanillaTask::OnJobCompleted(TJobletPtr joblet, TCompletedJobSummary& jobSummary)
{
    auto result = TTask::OnJobCompleted(joblet, jobSummary);

    RegisterOutput(jobSummary, joblet->ChunkListIds, joblet);

    // When restart_completed_jobs = %true, job completion may create new pending jobs in same task.
    UpdateTask();

    return result;
}

bool TVanillaTask::IsJobInterruptible() const
{
    if (!TTask::IsJobInterruptible()) {
        return false;
    }

    // We do not allow to interrupt job without interruption_signal
    // because there are no more ways to notify vanilla job about it.
    return Spec_->InterruptionSignal.has_value();
}

void TVanillaTask::TrySwitchToNewOperationIncarnation(
    const TJobletPtr& joblet,
    bool operationIsReviving,
    EOperationIncarnationSwitchReason reason)
{
    if (IsJobRestartingEnabled()) {
        YT_LOG_DEBUG("Trying to switch operation to new incarnation");

        auto* vanillaController = dynamic_cast<TVanillaController*>(TaskHost_);
        YT_VERIFY(vanillaController);
        vanillaController->TrySwitchToNewOperationIncarnation(joblet, operationIsReviving, reason);
    } else {
        YT_LOG_DEBUG("Job restarting is disabled, skip new incarnation operation switch");
    }
}

bool TVanillaTask::IsJobRestartingEnabled() const noexcept
{
    return static_cast<bool>(Spec_->GangManager);
}

int TVanillaTask::GetTargetJobCount() const noexcept
{
    return Spec_->JobCount;
}

IVanillaChunkPoolOutputPtr TVanillaTask::GetVanillaChunkPool() const noexcept
{
    return VanillaChunkPool_;
}

bool TVanillaTask::IsInputDataWeightHistogramSupported() const
{
    return false;
}

TJobSplitterConfigPtr TVanillaTask::GetJobSplitterConfig() const
{
    // In vanilla operations we don't want neither job splitting nor job speculation.
    auto config = TaskHost_->GetJobSplitterConfigTemplate();

    config->EnableJobSplitting = false;
    config->EnableJobSpeculation = false;

    return config;
}

void TVanillaTask::InitJobSpecTemplate()
{
    JobSpecTemplate_.set_type(ToProto(EJobType::Vanilla));
    auto* jobSpecExt = JobSpecTemplate_.MutableExtension(TJobSpecExt::job_spec_ext);

    jobSpecExt->set_io_config(ConvertToYsonString(Spec_->JobIO).ToString());

    TaskHost_->InitUserJobSpecTemplate(
        jobSpecExt->mutable_user_job_spec(),
        Spec_,
        TaskHost_->GetUserFiles(Spec_),
        TaskHost_->GetSpec()->DebugArtifactsAccount);
}

TVanillaTask::TNewJobConstraints TVanillaTask::GetNewJobConstraints(const TAllocation& allocation) const
{
    auto result = TTask::GetNewJobConstraints(allocation);
    if (allocation.LastJobInfo && VanillaController_->IsOperationGang()) {
        result.OutputCookie = allocation.LastJobInfo.OutputCookie;
        result.MonitoringDescriptor = allocation.LastJobInfo.MonitoringDescriptor;
        if (!result.MonitoringDescriptor) {
            // NB(pogorelov): If it would be just unset optional,
            // we could get descriptor intended for another allocation.
            result.MonitoringDescriptor = NullMonitoringDescriptor;
        }
    }

    return result;
}

IChunkPoolOutput::TCookie TVanillaTask::ExtractCookieForAllocation(
    const TAllocation& allocation,
    const TNewJobConstraints& newJobConstraints)
{
    if (newJobConstraints.OutputCookie) {
        VanillaChunkPool_->Extract(*newJobConstraints.OutputCookie);

        return *newJobConstraints.OutputCookie;
    }

    return VanillaChunkPool_->Extract(NodeIdFromAllocationId(allocation.Id));
}

TConfigurator<TVanillaTaskSpec> TVanillaTask::ConfigureUpdate()
{
    TConfigurator<TVanillaTaskSpec> configurator;
    configurator.Field("job_count", &TVanillaTaskSpec::JobCount)
        .Validator(BIND_NO_PROPAGATE([&] (const int& oldJobCount, const int& /*newJobCount*/) {
            YT_VERIFY(oldJobCount == VanillaChunkPool_->GetJobCount());
            THROW_ERROR_EXCEPTION_IF(
                VanillaController_->IsOperationGang(),
                "Cannot update \"job_count\" for gang operations");

            // TODO(coteeq): It is actually saner to check for a single task's
            // FailOnJobRestart but it is probably not very useful anyway.
            THROW_ERROR_EXCEPTION_IF(
                VanillaController_->HasJobUniquenessRequirements(),
                "Cannot update \"job_count\" when \"fail_on_job_restart\" is set");
        }))
        .Updater(BIND_NO_PROPAGATE([&] (const int& oldJobCount, const int& newJobCount) {
            YT_LOG_DEBUG_IF(
                oldJobCount != VanillaChunkPool_->GetJobCount(),
                "Chunk pool's job count is inconsistent with the spec; "
                "assuming revival (SpecJobCount: %v, ChunkPoolJobCount: %v)",
                oldJobCount,
                VanillaChunkPool_->GetJobCount());

            auto cookiesToAbort = VanillaChunkPool_->UpdateJobCount(newJobCount);
            if (!cookiesToAbort.empty()) {
                constexpr int MaxCookiesInLog = 5;
                YT_LOG_DEBUG(
                    "Try to abort jobs by cookies (Count: %v, Cookies: %v)",
                    cookiesToAbort.size(),
                    MakeShrunkFormattableView(cookiesToAbort, TDefaultFormatter(), MaxCookiesInLog));

                VanillaController_->AbortJobsByCookies(
                    this,
                    cookiesToAbort,
                    EAbortReason::JobCountChangedByUserRequest);
            }
            UpdateTask();
        }));

    return configurator;
}

////////////////////////////////////////////////////////////////////////////////

TVanillaController::TVanillaController(
    TVanillaOperationSpecPtr spec,
    TControllerAgentConfigPtr config,
    TVanillaOperationOptionsPtr options,
    IOperationControllerHostPtr host,
    TOperation* operation)
    : TOperationControllerBase(
        spec,
        config,
        options,
        host,
        operation)
    , Spec_(std::move(spec))
    , Options_(options)
{
    for (const auto& [taskName, taskSpec] : Spec_->Tasks) {
        if (taskSpec->GangManager) {
            GangManager_.emplace(this, GetConfig()->VanillaOperationOptions);
            break;
        }
    }

    GangOperationStartedCounter.Increment();
}

void TVanillaController::RegisterMetadata(auto&& registrar)
{
    registrar.template BaseType<TOperationControllerBase>();

    PHOENIX_REGISTER_FIELD(1, Spec_);
    PHOENIX_REGISTER_FIELD(2, Options_);
    PHOENIX_REGISTER_FIELD(3, Tasks_);
    PHOENIX_REGISTER_FIELD(4, TaskOutputTables_);

    PHOENIX_REGISTER_FIELD(5, GangManager_,
        .SinceVersion(ESnapshotVersion::MonitoringDescriptorsPreserving)
        .WhenMissing([] (TThis* this_, auto& context) {
            auto gangManager = Load<TGangManager>(context);

            for (const auto& [taskName, taskSpec] : this_->Spec_->Tasks) {
                if (taskSpec->GangManager) {
                    this_->GangManager_.emplace(std::move(gangManager));
                    break;
                }
            }
        }));

    registrar.AfterLoad([] (TThis* this_, auto& /*context*/) {
        if (this_->GangManager_) {
            // COMPAT(pogorelov)
            this_->GangManager_->SetVanillaController(this_);

            const auto& Logger = this_->GetLogger();

            YT_LOG_INFO(
                "Gang manager loaded (Incarnation: %v, Enabled: %v)",
                this_->GangManager_->GetCurrentIncarnation(),
                this_->GangManager_->IsEnabled());
        }
    });
}

void TVanillaController::CustomMaterialize()
{
    ValidateOperationLimits();

    for (const auto& [taskName, taskSpec] : Spec_->Tasks) {
        std::vector<TOutputStreamDescriptorPtr> streamDescriptors;
        int taskIndex = Tasks.size();
        for (int index = 0; index < std::ssize(TaskOutputTables_[taskIndex]); ++index) {
            auto streamDescriptor = TaskOutputTables_[taskIndex][index]->GetStreamDescriptorTemplate(index)->Clone();
            streamDescriptor->DestinationPool = GetSink();
            streamDescriptor->TargetDescriptor = TDataFlowGraph::SinkDescriptor;
            streamDescriptors.push_back(std::move(streamDescriptor));
        }

        auto task = New<TVanillaTask>(
            this,
            taskSpec,
            taskName,
            std::move(streamDescriptors),
            std::vector<TInputStreamDescriptorPtr>{});
        RegisterTask(task);
        FinishTaskInput(task);

        GetDataFlowGraph()->RegisterEdge(
            TDataFlowGraph::SourceDescriptor,
            task->GetVertexDescriptor());

        Tasks_.emplace_back(std::move(task));
        ValidateUserFileCount(taskSpec, taskName);
    }
}

TString TVanillaController::GetLoggingProgress() const
{
    const auto& jobCounter = GetTotalJobCounter();
    return Format(
        "Jobs = {T: %v, R: %v, C: %v, P: %v, F: %v, A: %v}, ",
        jobCounter->GetTotal(),
        jobCounter->GetRunning(),
        jobCounter->GetCompletedTotal(),
        GetPendingJobCount(),
        jobCounter->GetFailed(),
        jobCounter->GetAbortedTotal());
}

std::vector<TRichYPath> TVanillaController::GetInputTablePaths() const
{
    return {};
}

void TVanillaController::InitOutputTables()
{
    TOperationControllerBase::InitOutputTables();

    TaskOutputTables_.reserve(Spec_->Tasks.size());
    for (const auto& [taskName, taskSpec] : Spec_->Tasks) {
        auto& taskOutputTables = TaskOutputTables_.emplace_back();
        taskOutputTables.reserve(taskSpec->OutputTablePaths.size());
        for (const auto& outputTablePath : taskSpec->OutputTablePaths) {
            taskOutputTables.push_back(GetOrCrash(PathToOutputTable_, outputTablePath.GetPath()));
        }
    }
}

std::vector<TRichYPath> TVanillaController::GetOutputTablePaths() const
{
    std::vector<TRichYPath> outputTablePaths;
    for (const auto& [taskName, taskSpec] : Spec_->Tasks) {
        outputTablePaths.insert(outputTablePaths.end(), taskSpec->OutputTablePaths.begin(), taskSpec->OutputTablePaths.end());
    }
    return outputTablePaths;
}

std::optional<TRichYPath> TVanillaController::GetStderrTablePath() const
{
    return Spec_->StderrTablePath;
}

TBlobTableWriterConfigPtr TVanillaController::GetStderrTableWriterConfig() const
{
    return Spec_->StderrTableWriter;
}

std::optional<TRichYPath> TVanillaController::GetCoreTablePath() const
{
    return Spec_->CoreTablePath;
}

TBlobTableWriterConfigPtr TVanillaController::GetCoreTableWriterConfig() const
{
    return Spec_->CoreTableWriter;
}

bool TVanillaController::GetEnableCudaGpuCoreDump() const
{
    return Spec_->EnableCudaGpuCoreDump;
}

TStringBuf TVanillaController::GetDataWeightParameterNameForJob(EJobType /*jobType*/) const
{
    return TStringBuf();
}

TYsonStructPtr TVanillaController::GetTypedSpec() const
{
    return Spec_;
}

std::vector<EJobType> TVanillaController::GetSupportedJobTypesForJobsDurationAnalyzer() const
{
    return {};
}

bool TVanillaController::IsCompleted() const
{
    for (const auto& task : Tasks_) {
        if (!task->IsCompleted()) {
            return false;
        }
    }

    return true;
}

std::vector<TUserJobSpecPtr> TVanillaController::GetUserJobSpecs() const
{
    std::vector<TUserJobSpecPtr> specs;
    specs.reserve(Spec_->Tasks.size());
    for (const auto& [taskName, taskSpec] : Spec_->Tasks) {
        specs.emplace_back(taskSpec);
    }
    return specs;
}

void TVanillaController::ValidateRevivalAllowed() const
{
    // Even if fail_on_job_restart is set, we can not decline revival at this point
    // as it is still possible that all jobs are running or completed, thus the revival is permitted.
}

void TVanillaController::ValidateSnapshot() const
{
    if (!HasJobUniquenessRequirements()) {
        return;
    }

    int expectedJobCount = 0;
    for (const auto& [taskName, taskSpec] : Spec_->Tasks) {
        expectedJobCount += taskSpec->JobCount;
    }
    const auto& jobCounter = GetTotalJobCounter();
    int startedJobCount = jobCounter->GetRunning() + jobCounter->GetCompletedTotal();

    if (expectedJobCount != jobCounter->GetRunning()) {
        THROW_ERROR_EXCEPTION(
            NScheduler::EErrorCode::OperationFailedOnJobRestart,
            "Cannot revive operation when \"fail_on_job_restart\" option is set in operation spec or user job spec "
            "and not all jobs have already been started according to the operation snapshot "
            "(i.e. not all jobs are running or completed)")
            << TErrorAttribute("reason", EFailOnJobRestartReason::JobCountMismatchAfterRevival)
            << TErrorAttribute("operation_type", OperationType)
            << TErrorAttribute("expected_job_count", expectedJobCount)
            << TErrorAttribute("started_job_count", startedJobCount);
    }
}

void TVanillaController::InitUserJobSpec(
    NControllerAgent::NProto::TUserJobSpec* proto,
    const TJobletPtr& joblet) const
{
    YT_ASSERT_INVOKER_AFFINITY(GetJobSpecBuildInvoker());

    TOperationControllerBase::InitUserJobSpec(proto, joblet);
}

bool TVanillaController::OnJobCompleted(
    TJobletPtr joblet,
    std::unique_ptr<TCompletedJobSummary> jobSummary)
{
    YT_ASSERT_INVOKER_AFFINITY(GetCancelableInvoker(Config->JobEventsControllerQueue));

    auto interruptionReason = jobSummary->InterruptionReason;

    if (!TOperationControllerBase::OnJobCompleted(joblet, std::move(jobSummary))) {
        return false;
    }

    if (joblet->JobType == EJobType::Vanilla && interruptionReason != EInterruptionReason::None) {
        static_cast<TVanillaTask*>(joblet->Task)->TrySwitchToNewOperationIncarnation(
            joblet,
            /*operationIsReviving*/ false,
            EOperationIncarnationSwitchReason::JobInterrupted);
    }

    return true;
}

bool TVanillaController::OnJobFailed(
    TJobletPtr joblet,
    std::unique_ptr<TFailedJobSummary> jobSummary)
{
    YT_ASSERT_INVOKER_AFFINITY(GetCancelableInvoker(Config->JobEventsControllerQueue));

    if (!TOperationControllerBase::OnJobFailed(joblet, std::move(jobSummary))) {
        return false;
    }

    if (joblet->JobType == EJobType::Vanilla) {
        static_cast<TVanillaTask*>(joblet->Task)->TrySwitchToNewOperationIncarnation(
            joblet,
            /*operationIsReviving*/ false,
            EOperationIncarnationSwitchReason::JobFailed);
    }

    return true;
}

bool TVanillaController::OnJobAborted(
    TJobletPtr joblet,
    std::unique_ptr<TAbortedJobSummary> jobSummary)
{
    YT_ASSERT_INVOKER_AFFINITY(GetCancelableInvoker(Config->JobEventsControllerQueue));

    if (!TOperationControllerBase::OnJobAborted(joblet, std::move(jobSummary))) {
        return false;
    }

    if (joblet->JobType == EJobType::Vanilla) {
        static_cast<TVanillaTask*>(joblet->Task)->TrySwitchToNewOperationIncarnation(
            joblet,
            /*operationIsReviving*/ false,
            EOperationIncarnationSwitchReason::JobAborted);
    }

    return true;
}

TJobletPtr TVanillaController::CreateJoblet(
    TTask* task,
    TJobId jobId,
    TString treeId,
    int taskJobIndex,
    std::optional<TString> poolPath,
    bool treeIsTentative)
{
    YT_ASSERT_INVOKER_POOL_AFFINITY(CancelableInvokerPool);

    auto joblet = TOperationControllerBase::CreateJoblet(
        task,
        jobId,
        std::move(treeId),
        taskJobIndex,
        std::move(poolPath),
        treeIsTentative);

    if (GangManager_) {
        joblet->OperationIncarnation = GangManager_->GetCurrentIncarnation();
    }

    return joblet;
}

void TVanillaController::UpdateConfig(const TControllerAgentConfigPtr& config)
{
    TOperationControllerBase::UpdateConfig(config);

    if (GangManager_) {
        GangManager_->UpdateConfig(config->VanillaOperationOptions);
    }
}

void TVanillaController::TrySwitchToNewOperationIncarnation(
    const TJobletPtr& joblet,
    bool operationIsReviving,
    EOperationIncarnationSwitchReason reason)
{
    YT_ASSERT_INVOKER_AFFINITY(GetCancelableInvoker(Config->JobEventsControllerQueue));

    if (auto state = State.load(); state != EControllerState::Running) {
        YT_LOG_INFO("Operation is not running, skip switching to new incarnation (State: %v)", state);
        return;
    }

    YT_VERIFY(GangManager_);

    GangManager_->TrySwitchToNewIncarnation(joblet->OperationIncarnation, operationIsReviving, reason);
}

// NB(pogorelov): In case of restarting job during operation revival, we do not know the latest job id, started on node in the allocation,
// so we can not create new job immediately, and we do not force new job started in the same allocation.
// We could avoid this problem if node will know operation incarnation of jobs, but I'm not sure that this case is important.
void TVanillaController::RestartAllRunningJobsPreservingAllocations(bool operationIsReviving)
{
    YT_ASSERT_INVOKER_AFFINITY(GetCancelableInvoker(Config->JobEventsControllerQueue));

    auto abortReason = EAbortReason::OperationIncarnationChanged;

    std::vector<TNonNullPtr<TAllocation>> allocationsToRestartJobs;
    allocationsToRestartJobs.reserve(size(AllocationMap_));

    // NB(pogorelov): OnJobAborted may produce context switches only if operation has been finished, in such cases we break the loop.
    for (auto& [allocationId, allocation] : AllocationMap_) {
        if (const auto& joblet = allocation.Joblet) {
            auto jobId = joblet->JobId;
            bool contextSwitchesDetected = false;
            TOneShotContextSwitchGuard guard([&] {
                contextSwitchesDetected = true;
            });

            allocationsToRestartJobs.push_back(GetPtr(allocation));

            // NB(pogorelov): We abort job with requestNewJob even if we aren't able to schedule new job since:
            // 1) Job aborting causes immediate job releasing.
            // 2) Job tracker doesn't expect running job releasing (intentionally).
            // 3) We can't schedule new job before aborting old one.
            // TODO(pogorelov): It could be fixed by defering job releasing untill the end of method, think about it.
            Host->AbortJob(jobId, abortReason, /*requestNewJob*/ true);

            if (auto operationFinished = !OnJobAborted(joblet, std::make_unique<TAbortedJobSummary>(jobId, abortReason))) {
                YT_LOG_DEBUG("Operation finished during restarting jobs (JobId: %v)", jobId);
                return;
            }

            YT_LOG_FATAL_IF(
                contextSwitchesDetected,
                "Unexpected context switches detected during restarting job (JobId: %v)",
                jobId);
        }
    }

    // Currently gang operation may not contain not vanilla tasks at all.
    for (const auto& task : Tasks_) {
        YT_VERIFY(task->GetJobType() == EJobType::Vanilla);

        auto chunkPool = task->GetVanillaChunkPool();
        chunkPool->LostAll();
    }

    if (operationIsReviving) {
        YT_LOG_DEBUG("No later job absence guaranteed, do not create new jobs immediately");

        UpdateAllTasks();

        // NB(pogorelov): We can not just do nothing here with job constraints
        // because current and new allocation can conflict over job cookie, for example.
        // We could preserve allocation alive resetting job constraints.
        // But the occurrence is not so frequent.
        // And to not violate some convenient invariants (like preserving job cookie for allocation),
        // we just finish current allocations.
        for (auto& [allocationId, allocation] : AllocationMap_) {
            allocation.NewJobsForbiddenReason = EScheduleFailReason::AllocationFinishRequested;
        }

        return;
    }

    for (auto allocation : allocationsToRestartJobs) {
        auto failReason = TryScheduleNextJob(*allocation, allocation->LastJobInfo.JobId);
        if (failReason) {
            YT_LOG_DEBUG(
                "Failed to restart job, just aborting it (AllocationId: %v, CurrentJobId: %v, ScheduleNewJobFailReason: %v)",
                allocation->Id,
                allocation->LastJobInfo.JobId,
                failReason);

            allocation->NewJobsForbiddenReason = failReason;
        }
    }

    UpdateAllTasks();
}

void TVanillaController::OnOperationIncarnationChanged(bool operationIsReviving, EOperationIncarnationSwitchReason reason)
{
    YT_ASSERT_INVOKER_POOL_AFFINITY(InvokerPool);

    TForbidContextSwitchGuard guard;

    OperationIncarnationSwitchCounters[reason].Increment();

    RestartAllRunningJobsPreservingAllocations(operationIsReviving);
}

bool TVanillaController::IsOperationGang() const
{
    return GangManager_.has_value();
}

void TVanillaController::ValidateOperationLimits()
{
    if (std::ssize(Spec_->Tasks) > Options_->MaxTaskCount) {
        THROW_ERROR_EXCEPTION(
            "Maximum number of tasks exceeded: %v > %v",
            Spec_->Tasks.size(),
            Options_->MaxTaskCount);
    }

    i64 totalJobCount = 0;
    for (const auto& [taskName, taskSpec] : Spec_->Tasks) {
        totalJobCount += taskSpec->JobCount;
    }
    if (totalJobCount > Options_->MaxTotalJobCount) {
        THROW_ERROR_EXCEPTION(
            "Maximum total job count exceeded: %v > %v",
            totalJobCount,
            Options_->MaxTotalJobCount);
    }
}

TError TVanillaController::CheckJobsIncarnationsEqual() const
{
    YT_ASSERT_INVOKER_POOL_AFFINITY(InvokerPool);

    if (empty(AllocationMap_)) {
        return TError();
    }

    const TJoblet* joblet = nullptr;
    for (const auto& [id, allocation] : AllocationMap_) {
        if (allocation.Joblet) {
            if (!joblet) {
                joblet = allocation.Joblet.Get();
                continue;
            }

            if (allocation.Joblet->OperationIncarnation != joblet->OperationIncarnation) {
                return TError("Some jobs were settled in different operation incarnations")
                    << TErrorAttribute("first_job_id", joblet->JobId)
                    << TErrorAttribute("first_job_operation_incarnation", joblet->OperationIncarnation)
                    << TErrorAttribute("second_job_id", allocation.Joblet->JobId)
                    << TErrorAttribute("second_job_operation_incarnation", allocation.Joblet->OperationIncarnation);
            }
        }
    }

    return TError();
}

std::optional<EOperationIncarnationSwitchReason> TVanillaController::ShouldRestartJobsOnRevival() const
{
    YT_ASSERT_INVOKER_POOL_AFFINITY(InvokerPool);

    if (auto error = CheckJobsIncarnationsEqual(); !error.IsOK()) {
        // NB(pogorelov): If some jobs are in different operation incarnations then switching to new incarnation was enabled before revival, so we do not check spec.
        YT_LOG_DEBUG(
            error,
            "Some of revived jobs are in different operation incarnations, switching to new incarnation");

        return EOperationIncarnationSwitchReason::JobIncarnationsDifferAfterRevival;
    }

    THashMap<TTask*, int> jobCountByTasks;
    for (const auto& [id, allocation] : AllocationMap_) {
        if (allocation.Joblet && allocation.Joblet->JobType == EJobType::Vanilla) {
            ++jobCountByTasks[allocation.Joblet->Task];
        }
    }

    for (const auto& task : Tasks_) {
        if (!task->IsJobRestartingEnabled()) {
            continue;
        }

        auto jobCountIt = jobCountByTasks.find(static_cast<TTask*>(task.Get()));
        if (jobCountIt == end(jobCountByTasks)) {
            YT_LOG_DEBUG(
                "No jobs started in task, switching to new incarnation (TaskName: %v)",
                task->GetTitle());

            return EOperationIncarnationSwitchReason::JobLackAfterRevival;
        }

        if (auto jobCount = jobCountIt->second;
            jobCount != task->GetTargetJobCount())
        {
            YT_LOG_DEBUG(
                "Not all jobs started in task, switching to new incarnation (TaskName: %v, RevivedJobCount: %v, TargetJobCount: %v)",
                task->GetTitle(),
                jobCount,
                task->GetTargetJobCount());

            return EOperationIncarnationSwitchReason::JobLackAfterRevival;
        }
    }

    return std::nullopt;
}

void TVanillaController::OnOperationRevived()
{
    YT_ASSERT_INVOKER_POOL_AFFINITY(InvokerPool);

    TOperationControllerBase::OnOperationRevived();

    if (IsOperationGang()) {
        if (auto maybeIncarnationSwitchReason = ShouldRestartJobsOnRevival()) {
            YT_LOG_DEBUG(
                "Switching to new operation incarnation during revival (Reason: %v)",
                *maybeIncarnationSwitchReason);

            TrySwitchToNewOperationIncarnation(/*operationIsReviving*/ true, *maybeIncarnationSwitchReason);
        }
    }
}

void TVanillaController::BuildControllerInfoYson(TFluentMap fluent) const
{
    YT_ASSERT_INVOKER_POOL_AFFINITY(InvokerPool);

    TOperationControllerBase::BuildControllerInfoYson(fluent);

    if (GangManager_) {
        fluent.Item("operation_incarnation").Value(GangManager_->GetCurrentIncarnation());
    }
}

void TVanillaController::TrySwitchToNewOperationIncarnation(bool operationIsReviving, EOperationIncarnationSwitchReason reason)
{
    YT_ASSERT_INVOKER_AFFINITY(GetCancelableInvoker(Config->JobEventsControllerQueue));

    YT_VERIFY(GangManager_);

    GangManager_->TrySwitchToNewIncarnation(operationIsReviving, reason);
}

TOperationSpecBasePtr TVanillaController::ParseTypedSpec(const INodePtr& spec) const
{
    return ParseOperationSpec<TVanillaOperationSpec>(ConvertToNode(spec));
}

TOperationSpecBaseConfigurator TVanillaController::GetOperationSpecBaseConfigurator() const
{
    auto configurator = TConfigurator<TVanillaOperationSpec>();

    auto& tasksRegistrar = configurator.MapField("tasks", &TVanillaOperationSpec::Tasks)
        .ValidateOnAdded(BIND_NO_PROPAGATE([] (const TString& key, const TVanillaTaskSpecPtr& /*newSpec*/) {
            THROW_ERROR_EXCEPTION("Cannot create a new task")
                << TErrorAttribute("new_task_name", key);
        }))
        .ValidateOnRemoved(BIND_NO_PROPAGATE([] (const TString& key, const TVanillaTaskSpecPtr& /*oldSpec*/) {
            THROW_ERROR_EXCEPTION("Cannot remove a task")
                << TErrorAttribute("old_task_name", key);
        }))
        .OnAdded(BIND_NO_PROPAGATE([] (const TString& /*key*/, const TVanillaTaskSpecPtr& /*newSpec*/) -> TConfigurator<TVanillaTaskSpec> {
            YT_ABORT();
        }))
        .OnRemoved(BIND_NO_PROPAGATE([] (const TString& /*key*/, const TVanillaTaskSpecPtr& /*oldSpec*/) {
            YT_ABORT();
        }));

    for (const auto& task : Tasks_) {
        tasksRegistrar.ConfigureChild(task->GetName(), task->ConfigureUpdate());
    }

    return configurator;
}

void TVanillaController::AbortJobsByCookies(
    TTask* task,
    const std::vector<TOutputCookie>& cookiesVector,
    EAbortReason abortReason)
{
    THashSet<TOutputCookie> cookies(cookiesVector.begin(), cookiesVector.end());

    for (const auto& [_, allocation] : AllocationMap_) {
        if (cookies.empty()) {
            break;
        }

        if (!allocation.Joblet || allocation.Joblet->Task != task) {
            continue;
        }

        auto cookieIt = cookies.find(allocation.Joblet->OutputCookie);
        if (cookieIt != cookies.end()) {
            DoAbortJob(
                allocation.Joblet,
                abortReason,
                /*requestJobTrackerJobAbortion*/ true,
                /*force*/ true);
            cookies.erase(cookieIt);
        }
    }

    YT_VERIFY(cookies.empty());
}

////////////////////////////////////////////////////////////////////////////////

IOperationControllerPtr CreateVanillaController(
    TControllerAgentConfigPtr config,
    IOperationControllerHostPtr host,
    TOperation* operation)
{
    auto options = CreateOperationOptions(config->VanillaOperationOptions, operation->GetOptionsPatch());
    auto spec = ParseOperationSpec<TVanillaOperationSpec>(UpdateSpec(options->SpecTemplate, operation->GetSpec()));
    return New<TVanillaController>(std::move(spec), std::move(config), std::move(options), std::move(host), operation);
}

////////////////////////////////////////////////////////////////////////////////

void InitVanillaProfilers(const NProfiling::TProfiler& profiler)
{
    for (auto reason : TEnumTraits<EOperationIncarnationSwitchReason>::GetDomainValues()) {
        OperationIncarnationSwitchCounters[reason] = profiler
            .WithTag("reason", FormatEnum(reason))
            .Counter("/gang_operations/incarnation_switch_count");
    }

    GangOperationStartedCounter = profiler.Counter("/gang_operations/started_count");
}

////////////////////////////////////////////////////////////////////////////////

} // namespace NYT::NControllerAgent::NControllers
