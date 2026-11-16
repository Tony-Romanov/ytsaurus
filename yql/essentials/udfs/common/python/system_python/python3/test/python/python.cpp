#include <cstdlib>
#include <dlfcn.h>

// Test-only interposition of the version reported by the real system library.
// All other APIs, including interpreter initialization, retain their real ABI.
extern "C" __attribute__((visibility("default"))) const char* Py_GetVersion() {
    return std::getenv("SYSTEM_PYTHON_DISPATCHER_TEST_VERSION");
}

extern "C" __attribute__((visibility("default"))) int Py_IsInitialized() {
    if (std::getenv("SYSTEM_PYTHON_DISPATCHER_TEST_INITIALIZED")) {
        return 1;
    }
    const auto initialized = reinterpret_cast<int (*)()>(dlsym(RTLD_NEXT, "Py_IsInitialized"));
    return initialized ? initialized() : 0;
}
