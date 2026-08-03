#include "completion_factory.h"

#include <yql/essentials/sql/v1/ide/completion/name/service/ranking/frequency.h>

#include <yql/essentials/utils/utf8.h>

#include <library/cpp/json/json_reader.h>
#include <library/cpp/json/json_writer.h>

#include <util/charset/utf8.h>
#include <util/generic/vector.h>

#include <cctype>

namespace {

size_t UTF8PositionToBytes(TStringBuf text, size_t position) {
    const TStringBuf substr = SubstrUTF8(text, position, text.length());
    return substr.begin() - text.begin();
}

NSQLComplete::TCompletionInput MakeCompletionInput(TString& text, ui64 position) {
    const size_t lengthUtf8 = GetNumberOfUTF8Chars(text);
    if (lengthUtf8 < position) {
        ythrow yexception() << "provided position " << position << " is out of range " << lengthUtf8;
    }

    return {
        .Text = text,
        .CursorPosition = UTF8PositionToBytes(text, position),
    };
}

enum class EReadDocumentResult {
    Complete,
    EndOfStream,
    Incomplete,
};

EReadDocumentResult ReadJsonDocument(IInputStream& input, TString& document) {
    document.clear();

    for (char current; input.ReadChar(current);) {
        if (!std::isspace(static_cast<unsigned char>(current))) {
            document.push_back(current);
            break;
        }
    }

    if (document.empty()) {
        return EReadDocumentResult::EndOfStream;
    }

    const char first = document.front();
    if (first != '{' && first != '[' && first != '"') {
        for (char current; input.ReadChar(current);) {
            if (std::isspace(static_cast<unsigned char>(current))) {
                break;
            }
            document.push_back(current);
        }
        return EReadDocumentResult::Complete;
    }

    bool isInString = (first == '"');
    bool isEscaped = false;
    TVector<char> closingCharacters;
    if (first == '{') {
        closingCharacters.push_back('}');
    } else if (first == '[') {
        closingCharacters.push_back(']');
    }

    for (char current; input.ReadChar(current);) {
        document.push_back(current);

        if (isInString) {
            if (isEscaped) {
                isEscaped = false;
            } else if (current == '\\') {
                isEscaped = true;
            } else if (current == '"') {
                isInString = false;
                if (closingCharacters.empty()) {
                    return EReadDocumentResult::Complete;
                }
            }
            continue;
        }

        if (current == '"') {
            isInString = true;
        } else if (current == '{') {
            closingCharacters.push_back('}');
        } else if (current == '[') {
            closingCharacters.push_back(']');
        } else if (current == '}' || current == ']') {
            if (!closingCharacters.empty() && closingCharacters.back() == current) {
                closingCharacters.pop_back();
                if (closingCharacters.empty()) {
                    return EReadDocumentResult::Complete;
                }
            } else if (!closingCharacters.empty() && closingCharacters.front() == current) {
                // Let the JSON parser report mismatched nested brackets, but
                // terminate the malformed top-level document for recovery.
                return EReadDocumentResult::Complete;
            }
        }
    }

    return EReadDocumentResult::Incomplete;
}

void WriteResponse(const TVector<NSQLComplete::TCandidate>& candidates) {
    NJson::TJsonArray response;
    for (const auto& candidate : candidates) {
        response.AppendValue(NJson::TJsonMap{
            {"suggestion", candidate.Content},
            {"type", ToString(candidate.Kind)},
        });
    }

    NJson::WriteJson(&Cout, &response, false);
    Cout << Endl;
}

void WriteEmptyResponse() {
    WriteResponse({});
}

void LogRequestError(TStringBuf message) {
    Cerr << "Failed to process request: " << message << Endl;
}

void ReadRequest(TStringBuf document, const TCompletionFactory& completionFactory) {
    try {
        NJson::TJsonValue request;
        NJson::ReadJsonTree(document, &request, true);
        if (!request.IsMap()) {
            ythrow yexception() << "request must be a map";
        }

        TString query;
        if (request.Has("query")) {
            if (!request["query"].IsString()) {
                ythrow yexception() << "query must be a string";
            }
            query = request["query"].GetStringSafe();
        }

        const ui64 queryLength = GetNumberOfUTF8Chars(query);
        ui64 position = queryLength;
        if (request.Has("position")) {
            if (!request["position"].IsUInteger()) {
                ythrow yexception() << "position must be a non-negative integer";
            }
            position = Min<ui64>(request["position"].GetUIntegerSafe(), queryLength);
        }

        const NJson::TJsonValue* schema = nullptr;
        if (request.Has("schema")) {
            schema = &request["schema"];
        }

        auto engine = completionFactory.MakeEngine(schema);
        auto input = MakeCompletionInput(query, position);
        auto output = engine->CompleteAsync(input).ExtractValueSync();
        WriteResponse(output.Candidates);
    } catch (const std::exception& error) {
        LogRequestError(error.what());
        WriteEmptyResponse();
    } catch (...) {
        LogRequestError(CurrentExceptionMessage());
        WriteEmptyResponse();
    }
}

void Run(const TCompletionFactory& completionFactory) {
    for (;;) {
        TString document;
        const auto readResult = ReadJsonDocument(Cin, document);
        if (readResult == EReadDocumentResult::EndOfStream) {
            return;
        }
        if (readResult == EReadDocumentResult::Incomplete) {
            LogRequestError("unexpected end of input inside JSON");
            WriteEmptyResponse();
            return;
        }

        ReadRequest(document, completionFactory);
    }
}

} // namespace

int main(int argc, char**) {
    if (argc != 1) {
        Cerr << "complete does not accept command-line arguments" << Endl;
        return 1;
    }

    try {
        TCompletionFactory completionFactory(NSQLComplete::LoadFrequencyData());
        Run(completionFactory);
        return 0;
    } catch (...) {
        Cerr << CurrentExceptionMessage() << Endl;
        return 1;
    }
}
