#pragma once

#include <yql/essentials/public/udf/udf_registrator.h>

#include <functional>

namespace NPython::NStableAbi {

// Owns one GIL-enabled CPython interpreter. Calls are serialized by its
// GIL. Backends attaching to it must use standalone=false and must not finalize it.
void WithPython(const std::function<void()>& callback);

// Signature(helper_script, user_script, function_name) -> YQL type string.
// Creating the module or resolving its types does not initialize Python.
NYql::NUdf::TUniquePtr<NYql::NUdf::IUdfModule> CreateTypeInfoModule();

} // namespace NPython::NStableAbi
