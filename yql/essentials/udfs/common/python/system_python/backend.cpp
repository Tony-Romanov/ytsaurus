#include "backend.h"

#include <yql/essentials/udfs/common/python/python_udf/python_udf.h>

extern "C" YQL_UDF_API NYql::NUdf::IUdfModule* CreateSystemPython3Module() {
    return NYql::NUdf::GetYqlPythonUdfModule(
               "SystemPython3", NYql::NUdf::EPythonFlavor::System, false)
        .Release();
}
