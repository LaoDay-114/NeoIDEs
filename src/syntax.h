#pragma once

#include "common.h"

enum class Language {
    Cpp,
    C,
    CMake,
    Python,
    JavaScript,
    TypeScript,
    Java,
    CSharp,
    Go,
    Rust,
    Html,
    Css,
    Sql,
    Yaml,
    Xml,
    Json,
    Markdown,
    Batch,
    PowerShell,
    Plain
};

Language LanguageFromPath(const std::wstring& path);
const wchar_t* LanguageName(Language language);

enum class TokenKind : uint8_t {
    Normal,
    Keyword,
    Type,
    String,
    Number,
    Comment,
    Preproc,
    Function,
    Macro,
    Builtin,
    Property
};

struct Token {
    int start = 0;
    int len = 0;
    TokenKind kind = TokenKind::Normal;
};

enum class LexState : uint8_t {
    Normal,
    BlockComment
};

void HighlightLine(const std::wstring& text, Language language, LexState stateIn,
                   std::vector<Token>& tokens, LexState& stateOut);

struct SyntaxSymbol {
    std::wstring name;
    std::wstring kind;
    int line = 0;
};

std::vector<SyntaxSymbol> ParseDocumentSymbols(const std::wstring& text, Language language);
std::vector<std::wstring> BuildCompletions(const std::wstring& text, Language language,
                                           const std::wstring& prefix);
