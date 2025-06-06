#include "clickhouse_config.h"

#if USE_NLP

#include <Columns/ColumnString.h>
#include <Core/Settings.h>
#include <DataTypes/DataTypeString.h>
#include <Functions/FunctionFactory.h>
#include <Functions/FunctionHelpers.h>
#include <Functions/IFunction.h>
#include <Interpreters/Context.h>

#error #include <libstemmer.h>


namespace DB
{
namespace ErrorCodes
{
    extern const int ILLEGAL_COLUMN;
    extern const int ILLEGAL_TYPE_OF_ARGUMENT;
    extern const int SUPPORT_IS_DISABLED;
}

namespace
{

struct StemImpl
{
    static void vector(
        const ColumnString::Chars & data,
        const ColumnString::Offsets & offsets,
        ColumnString::Chars & res_data,
        ColumnString::Offsets & res_offsets,
        const String & language,
        size_t input_rows_count)
    {
        sb_stemmer * stemmer = sb_stemmer_new(language.data(), "UTF_8");

        if (stemmer == nullptr)
        {
            throw Exception(ErrorCodes::ILLEGAL_TYPE_OF_ARGUMENT, "Language {} is not supported for function stem", language);
        }

        res_data.resize(data.size());
        res_offsets.assign(offsets);

        UInt64 data_size = 0;
        for (UInt64 i = 0; i < input_rows_count; ++i)
        {
            /// Note that accessing -1th element is valid for PaddedPODArray.
            size_t original_size = offsets[i] - offsets[i - 1];
            const sb_symbol * result = sb_stemmer_stem(stemmer,
                reinterpret_cast<const uint8_t *>(data.data() + offsets[i - 1]),
                static_cast<int>(original_size - 1));
            size_t new_size = sb_stemmer_length(stemmer) + 1;

            memcpy(res_data.data() + data_size, result, new_size);

            data_size += new_size;
            res_offsets[i] = data_size;
        }
        res_data.resize(data_size);
        sb_stemmer_delete(stemmer);
    }
};


class FunctionStem : public IFunction
{
public:
    static constexpr auto name = "stem";

    static FunctionPtr create(ContextPtr context)
    {
        if (!context->getSettingsRef().allow_experimental_nlp_functions)
            throw Exception(ErrorCodes::SUPPORT_IS_DISABLED,
                            "Natural language processing function '{}' is experimental. "
                            "Set `allow_experimental_nlp_functions` setting to enable it", name);

        return std::make_shared<FunctionStem>();
    }

    String getName() const override { return name; }

    size_t getNumberOfArguments() const override { return 2; }

    bool isSuitableForShortCircuitArgumentsExecution(const DataTypesWithConstInfo & /*arguments*/) const override { return true; }

    DataTypePtr getReturnTypeImpl(const DataTypes & arguments) const override
    {
        if (!isString(arguments[0]))
            throw Exception(ErrorCodes::ILLEGAL_TYPE_OF_ARGUMENT, "Illegal type {} of argument of function {}",
                arguments[0]->getName(), getName());
        if (!isString(arguments[1]))
            throw Exception(ErrorCodes::ILLEGAL_TYPE_OF_ARGUMENT, "Illegal type {} of argument of function {}",
                arguments[1]->getName(), getName());
        return arguments[1];
    }

    bool useDefaultImplementationForConstants() const override { return true; }

    ColumnNumbers getArgumentsThatAreAlwaysConstant() const override { return {0}; }

    ColumnPtr executeImpl(const ColumnsWithTypeAndName & arguments, const DataTypePtr &, size_t input_rows_count) const override
    {
        const auto & langcolumn = arguments[0].column;
        const auto & strcolumn = arguments[1].column;

        const ColumnConst * lang_col = checkAndGetColumn<ColumnConst>(langcolumn.get());
        const ColumnString * words_col = checkAndGetColumn<ColumnString>(strcolumn.get());

        if (!lang_col)
            throw Exception(ErrorCodes::ILLEGAL_COLUMN, "Illegal column {} of argument of function {}",
                arguments[0].column->getName(), getName());
        if (!words_col)
            throw Exception(ErrorCodes::ILLEGAL_COLUMN, "Illegal column {} of argument of function {}",
                arguments[1].column->getName(), getName());

        String language = lang_col->getValue<String>();

        auto col_res = ColumnString::create();
        StemImpl::vector(words_col->getChars(), words_col->getOffsets(), col_res->getChars(), col_res->getOffsets(), language, input_rows_count);
        return col_res;
    }
};

}

REGISTER_FUNCTION(Stem)
{
    factory.registerFunction<FunctionStem>();
}

}

#endif
