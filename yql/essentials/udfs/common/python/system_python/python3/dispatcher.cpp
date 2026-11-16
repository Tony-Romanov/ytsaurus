#include "../backend.h"
#include <yql/essentials/udfs/common/python/stable_abi/type_info.h>

#ifdef SYSTEM_PYTHON3_EMBEDDED
    #include <yql/essentials/udfs/common/python/python_udf/python_udf.h>
#endif

#include <yql/essentials/public/udf/udf_type_builder.h>

#include <util/generic/singleton.h>
#include <util/generic/yexception.h>
#include <util/string/cast.h>
#include <util/string/split.h>
#include <util/system/dynlib.h>
#include <util/system/file.h>
#include <util/system/tempfile.h>

#include <dlfcn.h>

#include <array>
#include <atomic>
#include <cstring>
#include <exception>

// Py_GetVersion is part of CPython's Stable ABI and needs no interpreter
// initialization. No version-specific Python headers or structures are used.
extern "C" const char* Py_GetVersion();

using namespace NYql::NUdf;

namespace {

TBackTraceCallback BackTraceCallback = nullptr;

#ifndef SYSTEM_PYTHON3_EMBEDDED
// pack.py appends backend ELF files, a text index, then a little-endian index
// length and this magic. This mode links only to the system libpython3.so.
constexpr TStringBuf Magic("YQL_SYSTEM_PY3\0\0", 16);
constexpr size_t FooterSize = 8 + Magic.size();

TString DetectVersion() {
    const char* version = Py_GetVersion();
    Y_ENSURE(version, "SystemPython3: libpython3.so returned no Python version");
    const TStringBuf fullVersion(version);
    ui32 minor = 0;
    Y_ENSURE(fullVersion.StartsWith("3.") &&
                 TryFromString(fullVersion.After('.').Before('.'), minor),
             "SystemPython3: expected CPython 3, got " << fullVersion);
    Y_ENSURE(!fullVersion.Contains("free-threading"),
             "SystemPython3: free-threaded CPython is not supported: " << fullVersion);
    return "3." + ToString(minor);
}

TString DispatcherPath() {
    Dl_info info{};
    Y_ENSURE(dladdr(reinterpret_cast<void*>(&DispatcherPath), &info) && info.dli_fname,
             "SystemPython3: cannot locate dispatcher library");
    return info.dli_fname;
}

void ExtractBackend(const TString& version, TFile& output) {
    TFile input(DispatcherPath(), OpenExisting | RdOnly);
    const ui64 length = input.GetLength();
    std::array<char, FooterSize> footer;
    Y_ENSURE(length >= FooterSize &&
                 input.Pread(footer.data(), footer.size(), length - FooterSize) == footer.size() &&
                 std::memcmp(footer.data() + 8, Magic.data(), Magic.size()) == 0,
             "SystemPython3: no embedded backends; package the dispatcher using pack.py");
    ui64 indexSize = 0;
    for (size_t i = 0; i < 8; ++i) {
        indexSize |= ui64(static_cast<unsigned char>(footer[i])) << (i * 8);
    }
    Y_ENSURE(indexSize <= 65536 && indexSize <= length - FooterSize,
             "SystemPython3: corrupt backend index");
    const ui64 indexOffset = length - FooterSize - indexSize;
    TString index = TString::Uninitialized(indexSize);
    Y_ENSURE(input.Pread(index.Detach(), indexSize, indexOffset) == indexSize,
             "SystemPython3: cannot read backend index");
    TString supported;
    for (const auto& line : StringSplitter(index).Split('\n').SkipEmpty()) {
        TVector<TString> fields = StringSplitter(line.Token()).Split('\t');
        Y_ENSURE(fields.size() == 3, "SystemPython3: corrupt backend index entry");
        if (supported) {
            supported += ", ";
        }
        supported += fields[0];
        if (fields[0] != version) {
            continue;
        }
        const ui64 offset = FromString<ui64>(fields[1]);
        const ui64 size = FromString<ui64>(fields[2]);
        Y_ENSURE(offset <= indexOffset && size <= indexOffset - offset,
                 "SystemPython3: backend is outside its package");
        std::array<char, 65536> buffer;
        for (ui64 copied = 0; copied < size;) {
            const size_t count = Min<ui64>(buffer.size(), size - copied);
            Y_ENSURE(input.Pread(buffer.data(), count, offset + copied) == count,
                     "SystemPython3: cannot read embedded backend");
            output.Write(buffer.data(), count);
            copied += count;
        }
        return;
    }
    ythrow yexception() << "SystemPython3: libpython3.so selects " << version
                        << ", but this UDF contains backends for: " << supported;
}
#endif

class TBackend {
public:
    TBackend() {
#ifdef SYSTEM_PYTHON3_EMBEDDED
        NPython::NStableAbi::WithPython([&] {
            Module_ = GetYqlPythonUdfModule("SystemPython3", EPythonFlavor::Arcadia, false);
        });
#else
        const auto version = DetectVersion();
        auto file = TTempFileHandle::InCurrentDir("systempython3", "so");
        ExtractBackend(version, file);
        file.Flush();

        Library_.Open(file.Name().c_str(), RTLD_NOW | RTLD_LOCAL);
        Library_.SetUnloadable(false);
        const auto abi = reinterpret_cast<TAbiVersionFunctionPtr>(Library_.Sym("AbiVersion"));
        Y_ENSURE(IsAbiCompatible(abi()), "SystemPython3: incompatible backend UDF ABI");
        if (auto setter = reinterpret_cast<TSetBackTraceCallbackPtr>(Library_.SymOptional("SetBackTraceCallback"))) {
            setter(BackTraceCallback);
        }
        const auto create = reinterpret_cast<NYql::NSystemPython::TCreateModule>(
            Library_.Sym("CreateSystemPython3Module"));
        NPython::NStableAbi::WithPython([&] { Module_.Reset(create()); });
#endif
    }

    IUdfModule& Module() const {
        return *Module_;
    }

private:
    TDynamicLibrary Library_;
    TUniquePtr<IUdfModule> Module_;
};

struct TBackendState {
    TBackendState() {
        try {
            Value = MakeHolder<TBackend>();
        } catch (...) {
            Error = std::current_exception();
        }
    }

    IUdfModule& Module() const {
        if (Error) {
            std::rethrow_exception(Error);
        }
        return Value->Module();
    }

    THolder<TBackend> Value;
    std::exception_ptr Error;
};

class TDispatcherModule: public IUdfModule {
public:
    static TStringRef Name() {
        return TStringRef::Of("SystemPython3");
    }
    void GetAllFunctions(IFunctionsSink&) const override {
    }

    void BuildFunctionTypeInfo(const TStringRef& name, TType* userType,
                               const TStringRef& typeConfig, ui32 flags, IFunctionTypeInfoBuilder& builder) const override {
        if (flags & TFlags::TypesOnly) {
            return;
        }
        try {
            Backend().BuildFunctionTypeInfo(name, userType, typeConfig, flags, builder);
        } catch (const std::exception& error) {
            builder.SetError(TStringBuf(error.what()));
        }
    }

    void CleanupOnTerminate() const override {
        if (auto* backend = Backend_.load()) {
            backend->CleanupOnTerminate();
        }
    }

private:
    IUdfModule& Backend() const {
        // Initialization is serialized across registries and chooses one Python
        // for the process. Types-only registration never initializes Python.
        auto& module = Singleton<TBackendState>()->Module();
        Backend_ = &module;
        return module;
    }

    mutable std::atomic<IUdfModule*> Backend_ = nullptr;
};

} // namespace

extern "C" YQL_UDF_API void Register(IRegistrator& registrator, ui32 flags) {
    Y_UNUSED(flags);
    RegisterHelper<TDispatcherModule>(registrator);
    registrator.AddModule(TStringRef::Of("SystemPython3TypeInfo"), NPython::NStableAbi::CreateTypeInfoModule());
}

extern "C" YQL_UDF_API ui32 AbiVersion() {
    return CurrentAbiVersion();
}

extern "C" YQL_UDF_API void SetBackTraceCallback(TBackTraceCallback callback) {
    BackTraceCallback = callback;
    SetBackTraceCallbackImpl(callback);
}
