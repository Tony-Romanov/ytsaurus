#include "rewrite_io_utils.h"

#include <contrib/ydb/core/kqp/provider/yql_kikimr_expr_nodes.h>
#include <contrib/ydb/core/kqp/provider/yql_kikimr_provider.h>
#include <yql/essentials/core/expr_nodes/yql_expr_nodes.h>
#include <yql/essentials/core/yql_expr_optimize.h>
#include <yql/essentials/providers/common/provider/yql_provider.h>
#include <yql/essentials/providers/common/provider/yql_provider_names.h>
#include <yql/essentials/sql/sql.h>
#include <yql/essentials/sql/v1/sql.h>
#include <yql/essentials/sql/v1/lexer/antlr3/lexer.h>
#include <yql/essentials/sql/v1/lexer/antlr3_ansi/lexer.h>
#include <yql/essentials/sql/v1/proto_parser/antlr3/proto_parser.h>
#include <yql/essentials/sql/v1/proto_parser/antlr3_ansi/proto_parser.h>
#include <yql/essentials/sql/v1/lexer/antlr4/lexer.h>
#include <yql/essentials/sql/v1/lexer/antlr4_ansi/lexer.h>
#include <yql/essentials/sql/v1/proto_parser/antlr4/proto_parser.h>
#include <yql/essentials/sql/v1/proto_parser/antlr4_ansi/proto_parser.h>
#include <yql/essentials/utils/log/log.h>

namespace NYql {
namespace {

using namespace NNodes;

constexpr const char* QueryGraphNodeSignature = "SavedQueryGraph";

TExprNode::TPtr CompileViewQuery(
    TExprContext& ctx,
    NKikimr::NKqp::TKqpTranslationSettingsBuilder& settingsBuilder,
    IModuleResolver::TPtr moduleResolver,
    const TViewPersistedData& viewData
) {
    auto translationSettings = settingsBuilder.Build(ctx);
    translationSettings.Mode = NSQLTranslation::ESqlMode::LIMITED_VIEW;
    NSQLTranslation::Deserialize(viewData.CapturedContext, translationSettings);

    NSQLTranslationV1::TLexers lexers;
    lexers.Antlr3 = NSQLTranslationV1::MakeAntlr3LexerFactory();
    lexers.Antlr3Ansi = NSQLTranslationV1::MakeAntlr3AnsiLexerFactory();
    lexers.Antlr4 = NSQLTranslationV1::MakeAntlr4LexerFactory();
    lexers.Antlr4Ansi = NSQLTranslationV1::MakeAntlr4AnsiLexerFactory();
    NSQLTranslationV1::TParsers parsers;
    parsers.Antlr3 = NSQLTranslationV1::MakeAntlr3ParserFactory();
    parsers.Antlr3Ansi = NSQLTranslationV1::MakeAntlr3AnsiParserFactory();
    parsers.Antlr4 = NSQLTranslationV1::MakeAntlr4ParserFactory();
    parsers.Antlr4Ansi = NSQLTranslationV1::MakeAntlr4AnsiParserFactory();

    NSQLTranslation::TTranslators translators(
        nullptr,
        NSQLTranslationV1::MakeTranslator(lexers, parsers),
        nullptr
    );

    TAstParseResult queryAst;
    queryAst = NSQLTranslation::SqlToYql(translators, viewData.QueryText, translationSettings);

    ctx.IssueManager.AddIssues(queryAst.Issues);
    if (!queryAst.IsOk()) {
        return nullptr;
    }

    TExprNode::TPtr queryGraph;
    if (!CompileExpr(*queryAst.Root, queryGraph, ctx, moduleResolver.get(), nullptr)) {
        return nullptr;
    }

    return queryGraph;
}

void AddChild(const TExprNode::TPtr& parent, const TExprNode::TPtr& newChild) {
    auto childrenToChange = parent->ChildrenList();
    childrenToChange.emplace_back(newChild);
    parent->ChangeChildrenInplace(std::move(childrenToChange));
}

TExprNode::TPtr FindSavedQueryGraph(const TExprNode::TPtr& carrier) {
    if (carrier->ChildrenSize() == 0) {
        return nullptr;
    }
    auto lastChild = carrier->Children().back();
    return lastChild->IsCallable(QueryGraphNodeSignature) ? lastChild->ChildPtr(0) : TExprNode::TPtr();
}

void SaveQueryGraph(const TExprNode::TPtr& carrier, TExprContext& ctx, const TExprNode::TPtr& payload) {
    AddChild(carrier, ctx.NewCallable(payload->Pos(), QueryGraphNodeSignature, {payload}));
}

void InsertExecutionOrderDependencies(
    TExprNode::TPtr& queryGraph,
    const TExprNode::TPtr& worldBefore,
    TExprContext& ctx
) {
    const auto initialWorldOfTheQuery = FindNode(queryGraph, [](const TExprNode::TPtr& node) {
        return node->IsWorld();
    });
    if (!initialWorldOfTheQuery) {
        return;
    }
    queryGraph = ctx.ReplaceNode(std::move(queryGraph), *initialWorldOfTheQuery, worldBefore);
}

bool CheckTopLevelness(const TExprNode::TPtr& candidateRead, const TExprNode::TPtr& queryGraph) {
    THashSet<TExprNode::TPtr> readsInCandidateSubgraph;
    VisitExpr(candidateRead, [&readsInCandidateSubgraph](const TExprNode::TPtr& node) {
        if (node->IsCallable(ReadName)) {
            readsInCandidateSubgraph.emplace(node);
        }
        return true;
    });

    return !FindNode(queryGraph, [&readsInCandidateSubgraph](const TExprNode::TPtr& node) {
        return node->IsCallable(ReadName) && !readsInCandidateSubgraph.contains(node);
    });
}

}

TExprNode::TPtr FindTopLevelRead(const TExprNode::TPtr& queryGraph) {
    const TExprNode::TPtr* lastReadInTopologicalOrder = nullptr;
    VisitExpr(
        queryGraph,
        nullptr,
        [&lastReadInTopologicalOrder](const TExprNode::TPtr& node) {
            if (node->IsCallable(ReadName)) {
                lastReadInTopologicalOrder = &node;
            }
            return true;
        }
    );

    if (!lastReadInTopologicalOrder) {
        return nullptr;
    }

    YQL_ENSURE(CheckTopLevelness(*lastReadInTopologicalOrder, queryGraph),
               "Info for developers: assumption that there is only one top level Read! is wrong "
               "for the expression graph of the query stored in the view:\n"
                   << queryGraph->Dump());

    return *lastReadInTopologicalOrder;
}

TExprNode::TPtr RewriteReadFromView(
    const TExprNode::TPtr& node,
    TExprContext& ctx,
    NKikimr::NKqp::TKqpTranslationSettingsBuilder& settingsBuilder,
    IModuleResolver::TPtr moduleResolver,
    const TViewPersistedData& viewData
) {
    YQL_PROFILE_FUNC(DEBUG);

    const TCoRead readNode(node->ChildPtr(0));
    const auto worldBeforeThisRead = readNode.World().Ptr();

    TExprNode::TPtr queryGraph = FindSavedQueryGraph(readNode.Ptr());
    if (!queryGraph) {
        queryGraph = CompileViewQuery(ctx, settingsBuilder, moduleResolver, viewData);
        if (!queryGraph) {
            ctx.AddError(TIssue(ctx.GetPosition(readNode.Pos()),
                         "The query stored in the view cannot be compiled."));
            return nullptr;
        }
        YQL_CLOG(TRACE, ProviderKqp) << "Expression graph of the query stored in the view:\n"
                                     << NCommon::ExprToPrettyString(ctx, *queryGraph);

        InsertExecutionOrderDependencies(queryGraph, worldBeforeThisRead, ctx);
        SaveQueryGraph(readNode.Ptr(), ctx, queryGraph);
    }

    if (node->IsCallable(RightName)) {
        return queryGraph;
    }

    const auto topLevelRead = FindTopLevelRead(queryGraph);
    if (!topLevelRead) {
        return worldBeforeThisRead;
    }
    return Build<TCoLeft>(ctx, node->Pos()).Input(topLevelRead).Done().Ptr();
}

}
