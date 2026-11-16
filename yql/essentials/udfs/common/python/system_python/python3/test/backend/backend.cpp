#include "../../../backend.h"

#include <yql/essentials/public/udf/udf_type_builder.h>

using namespace NYql::NUdf;

namespace {
class TTestModule: public IUdfModule {
public:
    static TStringRef Name() {
        return TStringRef::Of("Unused");
    }
    void GetAllFunctions(IFunctionsSink&) const override {
    }
    void CleanupOnTerminate() const override {
    }
    void BuildFunctionTypeInfo(const TStringRef&, TType*, const TStringRef&,
                               ui32, IFunctionTypeInfoBuilder& builder) const override {
        builder.SetError(TStringRef::Of("dispatcher test backend selected"));
    }
};
} // namespace

extern "C" YQL_UDF_API IUdfModule* CreateSystemPython3Module() {
    return new TTestModule();
}

REGISTER_MODULES(TTestModule)
