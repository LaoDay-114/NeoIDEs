#pragma once

#include "common.h"
#include "syntax.h"

// Tree-sitter's opaque language type. Declared globally so generated binding
// tables can declare the language accessors with the same signature.
struct TSLanguage;

namespace LanguagePack {

// Generated binding between a language id and its compiled tree-sitter parser.
// `parser` must be the TSLanguage accessor (tree_sitter_<lang>), never an
// external scanner factory.
struct GrammarBinding {
    const wchar_t* id;
    const wchar_t* name;
    const TSLanguage* (*parser)();
};

extern const GrammarBinding GrammarBindings[];
extern const size_t GrammarBindingCount;

struct Entry {
    std::wstring id;
    std::wstring name;
    std::wstring monogram;
    std::wstring symbol;
    uint32_t color = 0;
    std::vector<std::wstring> extensions;
};

// Loads language-pack metadata from the embedded RCDATA resource. Safe to call
// repeatedly; initialization happens once.
void Initialize();

size_t Count();
const Entry* At(size_t index);
const Entry* FindById(const std::wstring& id);
const Entry* FindByPath(const std::wstring& path);

// Maps a language-pack id to the legacy Language enum. Returns Language::Plain
// when the id is not one of the built-in 23 languages.
Language LegacyLanguageFor(const std::wstring& id);

// Detects the most likely language using tree-sitter parse quality plus the
// language-pack extension metadata. When detectedId is provided it receives
// the language-pack id, including for languages beyond the legacy enum.
Language Detect(const std::wstring& utf16Text, std::wstring* detectedId);

// Visual metadata used by the generic badge-based file icon renderer.
bool VisualFor(const std::wstring& id, std::wstring& monogram, uint32_t& color);

// Display name for status bars and lists.
std::wstring DisplayName(const std::wstring& id, Language legacy);

} // namespace LanguagePack
