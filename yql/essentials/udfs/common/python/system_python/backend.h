#pragma once

#include <yql/essentials/public/udf/udf_registrator.h>

namespace NYql::NSystemPython {

using TCreateModule = NUdf::IUdfModule* (*)();

} // namespace NYql::NSystemPython
