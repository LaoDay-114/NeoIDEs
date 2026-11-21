#include "syntax.h"

#include <cassert>
#include <iostream>

namespace {

bool HasToken(const std::vector<Token>& tokens, TokenKind kind) {
    for (const Token& token : tokens) {
        if (token.kind == kind) return true;
    }
    return false;
}

std::vector<Token> Highlight(const std::wstring& text, Language language,
                             LexState stateIn = LexState::Normal,
                             LexState* stateOut = nullptr) {
    std::vector<Token> tokens;
    LexState next = stateIn;
    HighlightLine(text, language, stateIn, tokens, next);
    if (stateOut) *stateOut = next;
    return tokens;
}

} // namespace

int main() {
    assert(TextCellWidth(L'中') == 2);
    assert(TextCellWidth(L'\u0301') == 0);
    assert(IsWordChar(L'中'));

    assert(LanguageFromPath(L"App.java") == Language::Java);
    assert(LanguageFromPath(L"App.cs") == Language::CSharp);
    assert(LanguageFromPath(L"main.go") == Language::Go);
    assert(LanguageFromPath(L"lib.rs") == Language::Rust);
    assert(LanguageFromPath(L"index.html") == Language::Html);
    assert(LanguageFromPath(L"style.css") == Language::Css);
    assert(LanguageFromPath(L"query.sql") == Language::Sql);
    assert(LanguageFromPath(L"config.yaml") == Language::Yaml);
    assert(LanguageFromPath(L"image.svg") == Language::Xml);

    assert(HasToken(Highlight(L"public class Widget {}", Language::Java), TokenKind::Keyword));
    const auto csharpTokens = Highlight(L"public class Widget { string name = \"NeoIDEs\"; }",
                                        Language::CSharp);
    assert(HasToken(csharpTokens, TokenKind::Keyword));
    assert(HasToken(csharpTokens, TokenKind::Type));
    assert(HasToken(csharpTokens, TokenKind::String));
    assert(HasToken(Highlight(L"func main() {}", Language::Go), TokenKind::Keyword));
    assert(HasToken(Highlight(L"fn main() {}", Language::Rust), TokenKind::Keyword));
    assert(HasToken(Highlight(L"select name from users", Language::Sql), TokenKind::Keyword));
    assert(HasToken(Highlight(L"server:", Language::Yaml), TokenKind::Property));
    assert(HasToken(Highlight(L"<button title=\"ok\">", Language::Html), TokenKind::String));
    assert(HasToken(Highlight(L"color: red;", Language::Css), TokenKind::Property));

    LexState htmlState = LexState::Normal;
    assert(HasToken(Highlight(L"<!-- hidden", Language::Html, LexState::Normal, &htmlState),
                    TokenKind::Comment));
    assert(htmlState == LexState::BlockComment);
    assert(HasToken(Highlight(L"still hidden --> <button>", Language::Html, htmlState, &htmlState),
                    TokenKind::Comment));
    assert(htmlState == LexState::Normal);

    const std::wstring source = L"struct Customer {\n  fn Connect() {}\n}";
    std::vector<SyntaxSymbol> symbols = ParseDocumentSymbols(source, Language::Rust);
    bool foundCustomer = false;
    bool foundConnect = false;
    for (const SyntaxSymbol& symbol : symbols) {
        foundCustomer = foundCustomer || symbol.name == L"Customer";
        foundConnect = foundConnect || symbol.name == L"Connect";
    }
    assert(foundCustomer && foundConnect);

    std::vector<std::wstring> completions =
        BuildCompletions(source, Language::Rust, L"Cust");
    assert(std::find(completions.begin(), completions.end(), L"Customer") != completions.end());

    std::wcout << L"Syntax tests passed.\n";
    return 0;
}
