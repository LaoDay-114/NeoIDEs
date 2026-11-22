#include "syntax.h"
#include "completion_data.h"
#include "ai_completion.h"

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

    // -------------------------------------------------- content detection
    assert(DetectLanguageFromContent(L"") == Language::Plain);
    assert(DetectLanguageFromContent(L"just some plain text without code") == Language::Plain);
    assert(DetectLanguageFromContent(L"#include <iostream>\nint main() { return 0; }") ==
           Language::Cpp);
    assert(DetectLanguageFromContent(L"#include <stdio.h>\nint main(void) { printf(\"hi\"); }") ==
           Language::C);
    assert(DetectLanguageFromContent(L"cmake_minimum_required(VERSION 3.25)\nproject(Demo)") ==
           Language::CMake);
    assert(DetectLanguageFromContent(L"#!/usr/bin/env python3\ndef main():\n    pass") ==
           Language::Python);
    assert(DetectLanguageFromContent(L"def greet(name):\n    return name") == Language::Python);
    assert(DetectLanguageFromContent(L"const x = () => 1;\nconsole.log(x);") ==
           Language::JavaScript);
    assert(DetectLanguageFromContent(L"export interface User { name: string }") ==
           Language::TypeScript);
    assert(DetectLanguageFromContent(L"package main\n\nimport \"fmt\"\nfunc main() { fmt.Println(\"hi\") }") ==
           Language::Go);
    assert(DetectLanguageFromContent(L"fn main() {\n    let mut x = 1;\n}") == Language::Rust);
    assert(DetectLanguageFromContent(L"public class Main {\n public static void main(String[] args)"
                                     L" { System.out.println(\"hi\"); }\n}") == Language::Java);
    assert(DetectLanguageFromContent(L"using System;\nclass P { static void Main()"
                                     L" { Console.WriteLine(\"hi\"); } }") == Language::CSharp);
    assert(DetectLanguageFromContent(L"<!DOCTYPE html>\n<html><body><div>hi</div></body></html>") ==
           Language::Html);
    assert(DetectLanguageFromContent(L"<?xml version=\"1.0\"?>\n<root><item/></root>") ==
           Language::Xml);
    assert(DetectLanguageFromContent(L"body { color: red; margin: 0; }") == Language::Css);
    assert(DetectLanguageFromContent(L"SELECT id FROM users WHERE active = 1;") == Language::Sql);
    assert(DetectLanguageFromContent(L"server:\n  host: localhost\n  port: 8080") ==
           Language::Yaml);
    assert(DetectLanguageFromContent(L"{\"name\": \"neo\", \"ok\": true}") == Language::Json);
    assert(DetectLanguageFromContent(L"# Title\n\nSome **bold** text") == Language::Markdown);
    assert(DetectLanguageFromContent(L"@echo off\necho hello") == Language::Batch);
    assert(DetectLanguageFromContent(L"$name = \"neo\"\nWrite-Host $name") ==
           Language::PowerShell);
    assert(DetectLanguageFromContent(L"<?php\necho \"hi\";\nfunction f($x) { return $x; }") ==
           Language::Php);
    assert(DetectLanguageFromContent(L"require 'set'\nclass Foo\n  def bar\n    puts 'hi'\n  end\nend") ==
           Language::Ruby);
    assert(DetectLanguageFromContent(L"import Foundation\nstruct Point {\n  let x: Int\n}\nfunc f() -> Int { return 1 }") ==
           Language::Swift);
    assert(DetectLanguageFromContent(L"package demo\ndata class User(val name: String)\nfun main() { println(\"hi\") }") ==
           Language::Kotlin);

    // Only the opening of a document participates in detection.
    std::wstring longPlain(20000, L'x');
    longPlain += L"\ndef f():";
    assert(DetectLanguageFromContent(longPlain) == Language::Plain);

    // -------------------------------------------------- resource completions
    assert(CompletionData::LanguageCount() == 23); // 23 languages, Plain has no section
    const auto& cpp = CompletionData::ForLanguage(Language::Cpp);
    assert(!cpp.empty());
    assert(std::find(cpp.begin(), cpp.end(), L"constexpr") != cpp.end());
    const auto& py = CompletionData::ForLanguage(Language::Python);
    assert(std::find(py.begin(), py.end(), L"elif") != py.end());
    assert(CompletionData::ForLanguage(Language::Plain).empty());
    assert(!BuildCompletions(L"int x;", Language::Cpp, L"con").empty());

    // -------------------------------------------------- AI endpoint building
    assert(BuildAiEndpoint(L"https://api.openai.com", AiProtocol::Completion) ==
           L"https://api.openai.com/v1/completions");
    assert(BuildAiEndpoint(L"https://api.openai.com/", AiProtocol::ChatCompletions) ==
           L"https://api.openai.com/v1/chat/completions");
    assert(BuildAiEndpoint(L"https://api.openai.com/v1", AiProtocol::Responses) ==
           L"https://api.openai.com/v1/responses");
    assert(BuildAiEndpoint(L"https://proxy.local/v1/chat/completions", AiProtocol::ChatCompletions) ==
           L"https://proxy.local/v1/chat/completions");
    assert(AiPathProtocol(L"https://x.com/v1/completions") == (int)AiProtocol::Completion);
    assert(AiPathProtocol(L"https://x.com/v1/chat/completions") == (int)AiProtocol::ChatCompletions);
    assert(AiPathProtocol(L"https://x.com") == -1);
    assert(AiUrlRoot(L"https://x.com/v1/completions") == L"https://x.com");
    assert(BuildAiEndpoint(AiUrlRoot(L"https://x.com/v1/completions"), AiProtocol::Responses) ==
           L"https://x.com/v1/responses");

    std::wcout << L"Syntax tests passed.\n";
    return 0;
}
