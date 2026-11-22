#include "completion_data.h"

#include "../resources/resource.h"

namespace {

// Canonical resource section keys. Must match the [section] names in
// resources/completions.dat and map 1:1 to the Language enum (except Plain).
const wchar_t* SectionKey(Language language) {
    switch (language) {
        case Language::Cpp: return L"cpp";
        case Language::C: return L"c";
        case Language::CMake: return L"cmake";
        case Language::Python: return L"python";
        case Language::JavaScript: return L"javascript";
        case Language::TypeScript: return L"typescript";
        case Language::Java: return L"java";
        case Language::CSharp: return L"csharp";
        case Language::Go: return L"go";
        case Language::Rust: return L"rust";
        case Language::Html: return L"html";
        case Language::Css: return L"css";
        case Language::Sql: return L"sql";
        case Language::Yaml: return L"yaml";
        case Language::Xml: return L"xml";
        case Language::Json: return L"json";
        case Language::Markdown: return L"markdown";
        case Language::Batch: return L"batch";
        case Language::PowerShell: return L"powershell";
        case Language::Php: return L"php";
        case Language::Ruby: return L"ruby";
        case Language::Swift: return L"swift";
        case Language::Kotlin: return L"kotlin";
        default: return nullptr;
    }
}

std::unordered_map<std::wstring, std::vector<std::wstring>>& Table() {
    static std::unordered_map<std::wstring, std::vector<std::wstring>> table;
    return table;
}

bool& Loaded() {
    static bool loaded = false;
    return loaded;
}

bool LoadResourceText(std::wstring& out) {
    HMODULE module = GetModuleHandleW(nullptr);
    if (!module) return false;
    HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(NEO_RCDATA_COMPLETIONS), RT_RCDATA);
    if (!resource) return false;
    HGLOBAL memory = LoadResource(module, resource);
    if (!memory) return false;
    DWORD size = SizeofResource(module, resource);
    const char* bytes = (const char*)LockResource(memory);
    if (!bytes || size == 0) return false;
    out = Utf8ToWide(std::string_view(bytes, size));
    return true;
}

void Parse(const std::wstring& text) {
    Table().clear();
    std::wstring current;
    std::wistringstream stream(text);
    std::wstring line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        std::wstring trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == L'#') continue;
        if (trimmed.size() >= 2 && trimmed.front() == L'[' && trimmed.back() == L']') {
            current = ToLower(trimmed.substr(1, trimmed.size() - 2));
            Table()[current]; // ensure the section exists even when empty
            continue;
        }
        if (current.empty()) continue;
        // Keep the literal entry (may contain trailing spaces inside code tokens).
        std::wstring entry = Trim(line);
        if (!entry.empty()) Table()[current].push_back(entry);
    }
    Loaded() = true;
}

void EnsureLoaded() {
    if (Loaded()) return;
    std::wstring text;
    if (LoadResourceText(text)) {
        Parse(text);
    } else {
        Loaded() = true; // resource missing: return empty lists without crashing
    }
}

} // namespace

const std::vector<std::wstring>& CompletionData::ForLanguage(Language language) {
    EnsureLoaded();
    static const std::vector<std::wstring> empty;
    const wchar_t* key = SectionKey(language);
    if (!key) return empty;
    auto it = Table().find(key);
    return it == Table().end() ? empty : it->second;
}

void CompletionData::Reload() {
    Loaded() = false;
    std::wstring text;
    if (LoadResourceText(text)) Parse(text);
    else Loaded() = true;
}

size_t CompletionData::LanguageCount() {
    EnsureLoaded();
    return Table().size();
}
