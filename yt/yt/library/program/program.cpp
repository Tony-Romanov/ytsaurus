#include "program.h"

#include "build_attributes.h"

#include <yt/yt/build/build.h>

#include <yt/yt/core/misc/crash_handler.h>
#include <yt/yt/core/misc/fs.h>
#include <yt/yt/core/misc/shutdown.h>

#include <yt/yt/core/yson/writer.h>
#include <yt/yt/core/yson/null_consumer.h>

#include <yt/yt/core/logging/log_manager.h>

#include <yt/yt/library/profiling/tcmalloc/profiler.h>

#include <yt/yt/library/signals/signal_registry.h>

#include <yt/yt/library/ytprof/heap_profiler.h>

#include <library/cpp/yt/system/exit.h>

#include <library/cpp/yt/backtrace/absl_unwinder/absl_unwinder.h>

#include <tcmalloc/malloc_extension.h>

#include <absl/debugging/stacktrace.h>

#include <util/system/thread.h>
#include <util/system/sigset.h>

#include <stdlib.h>

#ifdef _unix_
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

#ifdef _linux_
#include <grp.h>
#include <sys/prctl.h>
#endif

namespace NYT {

using namespace NYson;
using namespace NSignals;

////////////////////////////////////////////////////////////////////////////////

class TProgram::TOptsParseResult
    : public NLastGetopt::TOptsParseResult
{
public:
    TOptsParseResult(TProgram* owner, int argc, const char** argv)
        : Owner_(owner)
    {
        Init(&Owner_->Opts_, argc, argv);
    }

    void HandleError() const override
    {
        Owner_->OnError(CurrentExceptionMessage());
        Cerr << Endl << "Try running '" << Owner_->Argv0_ << " --help' for more information." << Endl;
        Owner_->Exit(ToUnderlying(EProcessExitCode::ArgumentsError));
    }

private:
    TProgram* const Owner_;
};

////////////////////////////////////////////////////////////////////////////////

TProgram::TProgram()
{
    Opts_.AddHelpOption();
    Opts_.AddLongOption("yt-version", "Prints YT version")
        .NoArgument()
        .StoreValue(&PrintYTVersion_, true);
    Opts_.AddLongOption("version", "Print version")
        .NoArgument()
        .StoreValue(&PrintVersion_, true);
    Opts_.AddLongOption("yson", "Prints build information in YSON")
        .NoArgument()
        .StoreValue(&UseYson_, true);
    Opts_.AddLongOption("build", "Prints build information")
        .NoArgument()
        .StoreValue(&PrintBuild_, true);
    Opts_.SetFreeArgsNum(0);
}

TProgram::~TProgram() = default;

void TProgram::SetCrashOnError()
{
    CrashOnError_ = true;
}

void TProgram::HandleVersionAndBuild()
{
    if (PrintVersion_) {
        PrintVersionAndExit();
    }
    if (PrintYTVersion_) {
        PrintYTVersionAndExit();
    }
    if (PrintBuild_) {
        PrintBuildAndExit();
    }
}

int TProgram::Run(int argc, const char** argv)
{
    ::srand(time(nullptr));

    Argv0_ = TString(argv[0]);
    OptsParseResult_ = std::make_unique<TOptsParseResult>(this, argc, argv);

    auto run = [&] {
        HandleVersionAndBuild();
        DoRun();
    };

    if (!CrashOnError_) {
        try {
            run();
            Exit(ToUnderlying(EProcessExitCode::OK));
        } catch (...) {
            OnError(CurrentExceptionMessage());
            Exit(ToUnderlying(EProcessExitCode::GenericError));
        }
    } else {
        run();
        Exit(ToUnderlying(EProcessExitCode::OK));
    }

    // Cannot reach this due to #Exit calls above.
    YT_ABORT();
}

void TProgram::Abort(int code) noexcept
{
    NLogging::TLogManager::Get()->Shutdown();
    AbortProcessSilently(code);
}

void TProgram::Exit(int code) noexcept
{
#if defined(_linux_) && defined(CLANG_COVERAGE)
    __llvm_profile_write_file();
#endif

    // This explicit call may become obsolete some day;
    // cf. the comment section for NYT::Shutdown.
    Shutdown({
        .AbortOnHang = ShouldAbortOnHungShutdown(),
        .HungExitCode = code,
    });

    ::exit(code);
}

bool TProgram::ShouldAbortOnHungShutdown() noexcept
{
    return true;
}

void TProgram::OnError(const TString& message) noexcept
{
    try {
        Cerr << message << Endl;
    } catch (...) {
        // Just ignore it; STDERR might be closed already,
        // and write() would result in EPIPE.
    }
}

void TProgram::PrintYTVersionAndExit()
{
    if (UseYson_) {
        THROW_ERROR_EXCEPTION("--yson is not supported when printing version");
    }
    Cout << GetVersion() << Endl;
    Exit(0);
}

void TProgram::PrintBuildAndExit()
{
    if (UseYson_) {
        TYsonWriter writer(&Cout, EYsonFormat::Pretty);
        Serialize(BuildBuildAttributes(), &writer);
        Cout << Endl;
    } else {
        Cout << "Build Time: " << GetBuildTime() << Endl;
        Cout << "Build Host: " << GetBuildHost() << Endl;
    }
    Exit(0);
}

void TProgram::PrintVersionAndExit()
{
    PrintYTVersionAndExit();
}

const NLastGetopt::TOptsParseResult& TProgram::GetOptsParseResult() const
{
    return *OptsParseResult_;
}

////////////////////////////////////////////////////////////////////////////////

TProgramException::TProgramException(TString what)
    : What_(std::move(what))
{ }

const char* TProgramException::what() const noexcept
{
    return What_.c_str();
}

////////////////////////////////////////////////////////////////////////////////

TString CheckPathExistsArgMapper(const TString& arg)
{
    if (!NFS::Exists(arg)) {
        throw TProgramException(Format("File %v does not exist", arg));
    }
    return arg;
}

NYson::TYsonString CheckYsonArgMapper(const TString& arg)
{
    ParseYsonStringBuffer(arg, EYsonType::Node, GetNullYsonConsumer());
    return NYson::TYsonString(arg);
}

void ConfigureUids()
{
#ifdef _unix_
    uid_t ruid, euid;
#ifdef _linux_
    uid_t suid;
    YT_VERIFY(getresuid(&ruid, &euid, &suid) == 0);
#else
    ruid = getuid();
    euid = geteuid();
#endif
    if (euid == 0) {
        // if real uid is already root do not set root as supplementary ids.
        if (ruid != 0) {
            YT_VERIFY(setgroups(0, nullptr) == 0);
        }
        // if effective uid == 0 (e. g. set-uid-root), alter saved = effective, effective = real.
#ifdef _linux_
        YT_VERIFY(setresuid(ruid, ruid, euid) == 0);
        // Make server suid_dumpable = 1.
        YT_VERIFY(prctl(PR_SET_DUMPABLE, 1)  == 0);
#else
        YT_VERIFY(setuid(euid) == 0);
        YT_VERIFY(seteuid(ruid) == 0);
        YT_VERIFY(setruid(ruid) == 0);
#endif
    }
    umask(0000);
#endif
}

void ConfigureIgnoreSigpipe()
{
#ifdef _unix_
    signal(SIGPIPE, SIG_IGN);
#endif
}

void ConfigureCrashHandler()
{
    TSignalRegistry::Get()->PushCallback(AllCrashSignals, CrashSignalHandler);
    TSignalRegistry::Get()->PushDefaultSignalHandler(AllCrashSignals);
}

namespace {

void ExitZero(int /*unused*/)
{
#if defined(_linux_) && defined(CLANG_COVERAGE)
    __llvm_profile_write_file();
#endif
    // TODO(babenko): replace with pure "exit" some day.
    // Currently this causes some RPC requests to master to be replied with "Promise abandoned" error,
    // which is not retriable.
    AbortProcessSilently(EProcessExitCode::OK);
}

} // namespace

void ConfigureExitZeroOnSigterm()
{
#ifdef _unix_
    signal(SIGTERM, ExitZero);
#endif
}

void ConfigureAllocator(const TAllocatorOptions& options)
{
#ifdef _linux_
    NProfiling::EnableTCMallocProfiler();

    NYTProf::EnableMemoryProfilingTags(options.SnapshotUpdatePeriod);

    NBacktrace::SetAbslStackUnwinder();
#else
    Y_UNUSED(options);
#endif
}

////////////////////////////////////////////////////////////////////////////////

} // namespace NYT
