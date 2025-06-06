#include "name_service.h"

#include "name_index.h"

#include <yql/essentials/sql/v1/complete/name/service/ranking/ranking.h>
#include <yql/essentials/sql/v1/complete/text/case.h>

namespace NSQLComplete {

    const TVector<TStringBuf> FilteredByPrefix(const TString& prefix, const TNameIndex& index Y_LIFETIME_BOUND) {
        TNameIndexEntry normalized = {
            .Normalized = NormalizeName(prefix),
            .Original = "",
        };

        auto range = std::ranges::equal_range(
            std::begin(index), std::end(index),
            normalized, NameIndexCompareLimit(normalized.Normalized.size()));

        TVector<TStringBuf> filtered;
        for (const TNameIndexEntry& entry : range) {
            filtered.emplace_back(TStringBuf(entry.Original));
        }
        return filtered;
    }

    const TVector<TStringBuf> FilteredByPrefix(
        const TString& prefix,
        const TVector<TString>& sorted Y_LIFETIME_BOUND) {
        auto [first, last] = EqualRange(
            std::begin(sorted), std::end(sorted),
            prefix, NoCaseCompareLimit(prefix.size()));
        return TVector<TStringBuf>(first, last);
    }

    template <class T, class S = TStringBuf>
    void AppendAs(TVector<TGenericName>& target, const TVector<S>& source) {
        for (const auto& element : source) {
            target.emplace_back(T{TString(element)});
        }
    }

    TString Prefixed(const TStringBuf requestPrefix, const TStringBuf delimeter, const TNamespaced& namespaced) {
        TString prefix;
        if (!namespaced.Namespace.empty()) {
            prefix += namespaced.Namespace;
            prefix += delimeter;
        }
        prefix += requestPrefix;
        return prefix;
    }

    void FixPrefix(TString& name, const TStringBuf delimeter, const TNamespaced& namespaced) {
        if (namespaced.Namespace.empty()) {
            return;
        }
        name.remove(0, namespaced.Namespace.size() + delimeter.size());
    }

    void FixPrefix(TGenericName& name, const TNameRequest& request) {
        std::visit([&](auto& name) -> size_t {
            using T = std::decay_t<decltype(name)>;
            if constexpr (std::is_same_v<T, TPragmaName>) {
                FixPrefix(name.Indentifier, ".", *request.Constraints.Pragma);
            }
            if constexpr (std::is_same_v<T, TFunctionName>) {
                FixPrefix(name.Indentifier, "::", *request.Constraints.Function);
            }
            return 0;
        }, name);
    }

    class TStaticNameService: public INameService {
    public:
        explicit TStaticNameService(TNameSet names, IRanking::TPtr ranking)
            : Pragmas_(BuildNameIndex(std::move(names.Pragmas), NormalizeName))
            , Types_(BuildNameIndex(std::move(names.Types), NormalizeName))
            , Functions_(BuildNameIndex(std::move(names.Functions), NormalizeName))
            , Hints_([hints = std::move(names.Hints)] {
                THashMap<EStatementKind, TNameIndex> index;
                for (auto& [k, hints] : hints) {
                    index.emplace(k, BuildNameIndex(std::move(hints), NormalizeName));
                }
                return index;
            }())
            , Ranking_(std::move(ranking))
        {
        }

        TFuture<TNameResponse> Lookup(TNameRequest request) const override {
            TNameResponse response;

            Sort(request.Keywords, NoCaseCompare);
            AppendAs<TKeyword>(
                response.RankedNames,
                FilteredByPrefix(request.Prefix, request.Keywords));

            if (request.Constraints.Pragma) {
                auto prefix = Prefixed(request.Prefix, ".", *request.Constraints.Pragma);
                auto names = FilteredByPrefix(prefix, Pragmas_);
                AppendAs<TPragmaName>(response.RankedNames, names);
            }

            if (request.Constraints.Type) {
                AppendAs<TTypeName>(
                    response.RankedNames,
                    FilteredByPrefix(request.Prefix, Types_));
            }

            if (request.Constraints.Function) {
                auto prefix = Prefixed(request.Prefix, "::", *request.Constraints.Function);
                auto names = FilteredByPrefix(prefix, Functions_);
                AppendAs<TFunctionName>(response.RankedNames, names);
            }

            if (request.Constraints.Hint) {
                const auto stmt = request.Constraints.Hint->Statement;
                if (const auto* hints = Hints_.FindPtr(stmt)) {
                    AppendAs<THintName>(
                        response.RankedNames,
                        FilteredByPrefix(request.Prefix, *hints));
                }
            }

            Ranking_->CropToSortedPrefix(response.RankedNames, request.Limit);

            for (auto& name : response.RankedNames) {
                FixPrefix(name, request);
            }

            return NThreading::MakeFuture(std::move(response));
        }

    private:
        TNameIndex Pragmas_;
        TNameIndex Types_;
        TNameIndex Functions_;
        THashMap<EStatementKind, TNameIndex> Hints_;
        IRanking::TPtr Ranking_;
    };

    INameService::TPtr MakeStaticNameService(TNameSet names, TFrequencyData frequency) {
        return INameService::TPtr(new TStaticNameService(
            Pruned(std::move(names), frequency),
            MakeDefaultRanking(std::move(frequency))));
    }

    INameService::TPtr MakeStaticNameService(TNameSet names, IRanking::TPtr ranking) {
        return MakeIntrusive<TStaticNameService>(std::move(names), std::move(ranking));
    }

} // namespace NSQLComplete
