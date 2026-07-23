#include "parser.h"

#include <yql/essentials/sql/v1/ide/completion/text/word.h>
#include <yql/essentials/sql/v1/ide/pure_ast/parser.h>

#include <util/charset/utf8.h>

namespace NSQLComplete {

namespace {

class TParser: public IParser {
public:
    explicit TParser(NSQLPureAST::IParser::TPtr parser)
        : Parser_(std::move(parser))
    {
    }

    TParsedInput Parse(TCompletionInput input) override {
        Recovered_.clear();
        if (IsRecoverable(input)) {
            Recovered_ = TString(input.Text);
            // "_" is to parse `SELECT x._ FROM table`
            //        instead of `SELECT x.FROM table`
            Recovered_.insert(input.CursorPosition, "_");
            input.Text = Recovered_;
        }

        TStringBuf prefix = TStringBuf(input.Text).Head(input.CursorPosition);
        input.CursorPosition = GetNumberOfUTF8Chars(prefix);

        NSQLPureAST::TParseTree tree = Parser_->Parse(input.Text);
        if (!Recovered_.empty() && tree.Parser->getNumberOfSyntaxErrors() != 0 && HasFollowingAliasedExpression(tree, prefix.size())) {
            const auto syntaxErrors = tree.Parser->getNumberOfSyntaxErrors();

            // `_ column AS alias` is parsed as a column with two aliases. Try
            // treating the cursor as a result column missing its right comma.
            const auto separatorPosition = prefix.size() + 1;
            Recovered_.insert(separatorPosition, ",");
            const auto separatedTree = Parser_->Parse(Recovered_);
            if (separatedTree.Parser->getNumberOfSyntaxErrors() < syntaxErrors) {
                tree = separatedTree;
            } else {
                Recovered_.erase(separatorPosition, 1);
                tree = Parser_->Parse(Recovered_);
            }
        }

        return {
            .Original = {
                .Text = tree.Text,
                .CursorPosition = input.CursorPosition,
            },
            .Tokens = tree.Tokens,
            .Parser = tree.Parser,
            .SqlQuery = tree.SqlQuery,
        };
    }

private:
    bool HasFollowingAliasedExpression(const NSQLPureAST::TParseTree& tree, size_t cursorPosition) const {
        tree.Tokens->fill();
        i64 parenthesisDepth = 0;
        for (size_t index = 0; index < tree.Tokens->size(); ++index) {
            const auto token = tree.Tokens->get(index);
            if (token->getStartIndex() <= cursorPosition) {
                continue;
            }

            if (const auto type = token->getType(); type == SQLv1::TOKEN_LPAREN) {
                ++parenthesisDepth;
            } else if (type == SQLv1::TOKEN_RPAREN) {
                if (parenthesisDepth == 0) {
                    return false;
                }
                --parenthesisDepth;
            } else if (parenthesisDepth == 0) {
                if (type == SQLv1::TOKEN_AS) {
                    return true;
                }
                if (type == SQLv1::TOKEN_COMMA ||
                    type == SQLv1::TOKEN_FROM ||
                    type == SQLv1::TOKEN_WITHOUT ||
                    type == SQLv1::TOKEN_SEMICOLON) {
                    return false;
                }
            }
        }
        return false;
    }

    bool IsRecoverable(TCompletionInput input) const {
        TStringBuf s = input.Text;
        size_t i = input.CursorPosition;
        return (i < s.size() && IsWordBoundary(s[i]) || i == s.size());
    }

    TString Recovered_;
    NSQLPureAST::IParser::TPtr Parser_;
};

} // namespace

IParser::TPtr MakeParser(bool isAnsiLexer) {
    return MakeHolder<TParser>(NSQLPureAST::MakeParser(isAnsiLexer));
}

} // namespace NSQLComplete
