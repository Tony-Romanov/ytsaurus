#include "type_info.h"

#include <Python.h>

#include <library/cpp/resource/resource.h>

#include <yql/essentials/public/udf/udf_terminator.h>
#include <yql/essentials/public/udf/udf_type_builder.h>
#include <yql/essentials/public/udf/udf_value_builder.h>

#include <util/generic/scope.h>
#include <util/generic/singleton.h>
#include <util/generic/yexception.h>
#include <util/string/builder.h>

using namespace NYql::NUdf;

namespace NPython::NStableAbi {
namespace {

// PyObject is opaque under Py_LIMITED_API. Use ABI functions even for refcounts;
// never access object layouts, code objects, PyTypeObject, or private symbols.
class TObject {
public:
    explicit TObject(PyObject* object = nullptr)
        : Object_(object)
    {
    }
    TObject(const TObject&) = delete;
    TObject& operator=(const TObject&) = delete;
    ~TObject() {
        if (Object_) {
            Py_DecRef(Object_);
        }
    }
    PyObject* Get() const {
        return Object_;
    }
    explicit operator bool() const {
        return Object_ != nullptr;
    }

private:
    PyObject* Object_;
};

TString ToString(PyObject* object) {
    TObject bytes(PyUnicode_AsUTF8String(object));
    if (!bytes) {
        return {};
    }
    char* data = nullptr;
    Py_ssize_t size = 0;
    if (PyBytes_AsStringAndSize(bytes.Get(), &data, &size) < 0) {
        return {};
    }
    return TString(data, size);
}

void Check(PyObject* result, TStringBuf operation) {
    if (result) {
        return;
    }
    PyObject* type = nullptr;
    PyObject* value = nullptr;
    PyObject* traceback = nullptr;
    PyErr_Fetch(&type, &value, &traceback);
    TObject ownedType(type), ownedValue(value), ownedTraceback(traceback);
    TString message;
    if (value) {
        TObject text(PyObject_Str(value));
        if (text) {
            message = ToString(text.Get());
        }
    }
    PyErr_Clear();
    ythrow yexception() << "Python type inference: " << operation << ": " << message;
}

void Compile(TStringBuf source, const char* filename, PyObject* globals) {
    const TString terminated(source);
    TObject code(Py_CompileString(terminated.c_str(), filename, Py_file_input));
    Check(code.Get(), "compile");
    TObject result(PyEval_EvalCode(code.Get(), globals, globals));
    Check(result.Get(), "execute module");
}

void InstallTypes() {
    TObject globals(PyDict_New());
    Check(globals.Get(), "create globals");
    const auto bootstrap = NResource::Find("/python/stable_abi/bootstrap.py");
    Compile(bootstrap, "yql-signature-bootstrap", globals.Get());
    PyObject* install = PyDict_GetItemString(globals.Get(), "install_types");
    Check(install, "find type bootstrap");
    const auto base = NResource::Find("/python/stable_abi/type_base.py");
    const auto typing = NResource::Find("/python/stable_abi/typing.py");
    const auto annotations = NResource::Find("/python/stable_abi/yql_typing.py");
    TObject baseArg(PyUnicode_FromStringAndSize(base.data(), base.size()));
    TObject typingArg(PyUnicode_FromStringAndSize(typing.data(), typing.size()));
    TObject annotationsArg(PyUnicode_FromStringAndSize(annotations.data(), annotations.size()));
    Check(baseArg.Get(), "type_base source");
    Check(typingArg.Get(), "typing source");
    Check(annotationsArg.Get(), "annotation source");
    TObject result(PyObject_CallFunctionObjArgs(install, baseArg.Get(), typingArg.Get(), annotationsArg.Get(), nullptr));
    Check(result.Get(), "install annotation types");
}

class TInterpreter {
public:
    TInterpreter() {
        const char* reportedVersion = Py_GetVersion();
        Y_ENSURE(reportedVersion, "libpython3.so returned no Python version");
        const TStringBuf version(reportedVersion);
        Y_ENSURE(version.StartsWith("3.") && !version.Contains("free-threading"),
                 "Python type inference requires GIL-enabled CPython 3: " << version);
        Y_ENSURE(!Py_IsInitialized(), "another Python runtime is already initialized in this process");
        // Avoid choosing Python's prefix via a python3 executable in PATH.
        // PyConfig is not in the 3.8 Limited API. This deprecated setter remains
        // part of the Stable ABI and is needed by the supported older runtimes.
        Y_PRAGMA_DIAGNOSTIC_PUSH
        Y_PRAGMA_NO_DEPRECATED
        Py_SetProgramName(L"YQL::SystemPython3");
        Y_PRAGMA_DIAGNOSTIC_POP
        Py_InitializeEx(0);
        try {
#ifdef SYSTEM_PYTHON3_EMBEDDED
            // Enable the bundled standard library before executing Python code.
            TObject runtime(PyImport_ImportModule("__res"));
            Check(runtime.Get(), "initialize bundled Python runtime");
#endif
            InstallTypes();
        } catch (...) {
            Py_Finalize();
            throw;
        }
        State_ = PyEval_SaveThread();
    }
    ~TInterpreter() {
        PyEval_RestoreThread(State_);
        Py_Finalize();
    }

private:
    PyThreadState* State_;
};

TString Signature(TStringBuf helper, TStringBuf script, TStringBuf name) {
    TString signature;
    WithPython([&] {
        TObject globals(PyDict_New());
        Check(globals.Get(), "create signature globals");
        Compile(helper, "yql-signature-helper", globals.Get());
        PyObject* function = PyDict_GetItemString(globals.Get(), "signature");
        Check(function, "find signature helper");
        TObject scriptArg(PyBytes_FromStringAndSize(script.data(), script.size()));
        TObject nameArg(PyBytes_FromStringAndSize(name.data(), name.size()));
        Check(scriptArg.Get(), "script source");
        Check(nameArg.Get(), "function name");
        TObject isolatedGlobals(PyBool_FromLong(1));
        Check(isolatedGlobals.Get(), "signature execution mode");
        TObject result(PyObject_CallFunctionObjArgs(function, scriptArg.Get(), nameArg.Get(), isolatedGlobals.Get(), nullptr));
        Check(result.Get(), "extract signature");
        signature = ToString(result.Get());
        if (PyErr_Occurred()) {
            Check(nullptr, "signature must be a string");
        }
    });
    return signature;
}

class TSignature: public TBoxedValue {
public:
    explicit TSignature(TSourcePosition position)
        : Position_(position)
    {
    }

private:
    TUnboxedValue Run(const IValueBuilder* builder, const TUnboxedValuePod* args) const override {
        try {
            return builder->NewString(Signature(args[0].AsStringRef(), args[1].AsStringRef(), args[2].AsStringRef()));
        } catch (const std::exception& error) {
            UdfTerminate((TStringBuilder() << Position_ << error.what()).c_str());
        }
    }
    TSourcePosition Position_;
};

class TTypeInfoModule: public IUdfModule {
public:
    void GetAllFunctions(IFunctionsSink& sink) const override {
        sink.Add(TStringRef::Of("Signature"));
    }
    void BuildFunctionTypeInfo(const TStringRef& name, TType*, const TStringRef&,
                               ui32 flags, IFunctionTypeInfoBuilder& builder) const override {
        if (name != TStringRef::Of("Signature")) {
            return;
        }
        builder.SimpleSignature<char*(char*, char*, char*)>();
        if (!(flags & TFlags::TypesOnly)) {
            builder.Implementation(new TSignature(builder.GetSourcePosition()));
        }
    }
    void CleanupOnTerminate() const override {
    }
};

} // namespace

void WithPython(const std::function<void()>& callback) {
    Singleton<TInterpreter>();
    const auto state = PyGILState_Ensure();
    Y_DEFER {
        PyGILState_Release(state);
    };
    callback();
}

TUniquePtr<IUdfModule> CreateTypeInfoModule() {
    return new TTypeInfoModule;
}

} // namespace NPython::NStableAbi
