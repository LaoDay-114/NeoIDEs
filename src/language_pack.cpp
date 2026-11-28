#include "language_pack.h"

#include "../resources/resource.h"
#include <tree_sitter/api.h>

namespace LanguagePack {
namespace {

std::vector<Entry>& Entries() {
    static std::vector<Entry> entries;
    return entries;
}

bool& Initialized() {
    static bool initialized = false;
    return initialized;
}

std::vector<std::wstring> SplitExt(const std::wstring& value) {
    std::vector<std::wstring> out;
    size_t start = 0;
    while (start <= value.size()) {
        size_t comma = value.find(L',', start);
        std::wstring part = Trim(value.substr(start, comma == std::wstring::npos
                                                   ? std::wstring::npos : comma - start));
        if (!part.empty()) out.push_back(part);
        if (comma == std::wstring::npos) break;
        start = comma + 1;
    }
    return out;
}

bool LoadText(std::wstring& out) {
    HMODULE module = GetModuleHandleW(nullptr);
    if (!module) return false;
    HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(NEO_RCDATA_LANGPACK), RT_RCDATA);
    if (!resource) return false;
    HGLOBAL memory = LoadResource(module, resource);
    if (!memory) return false;
    DWORD size = SizeofResource(module, resource);
    const char* bytes = static_cast<const char*>(LockResource(memory));
    if (!bytes || size == 0) return false;
    out = Utf8ToWide(std::string_view(bytes, size));
    return true;
}

void Parse(const std::wstring& text) {
    Entries().clear();
    Entry current;
    bool haveCurrent = false;
    std::wistringstream stream(text);
    std::wstring line;
    auto commit = [&]() {
        if (haveCurrent && !current.id.empty()) Entries().push_back(current);
        current = Entry{};
        haveCurrent = false;
    };
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        std::wstring trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == L'#') continue;
        if (trimmed.front() == L'[' && trimmed.back() == L']') {
            commit();
            current.id = ToLower(trimmed.substr(1, trimmed.size() - 2));
            haveCurrent = true;
            continue;
        }
        if (!haveCurrent) continue;
        size_t eq = trimmed.find(L'=');
        if (eq == std::wstring::npos) continue;
        std::wstring key = ToLower(Trim(trimmed.substr(0, eq)));
        std::wstring value = Trim(trimmed.substr(eq + 1));
        if (key == L"name") current.name = value;
        else if (key == L"ext") current.extensions = SplitExt(value);
        else if (key == L"symbol") current.symbol = value;
        else if (key == L"mono") current.monogram = value;
        else if (key == L"color") current.color = (uint32_t)wcstoull(value.c_str(), nullptr, 0);
    }
    commit();
}

void EnsureInitialized() {
    if (Initialized()) return;
    Initialized() = true;
    std::wstring text;
    if (LoadText(text)) Parse(text);
}

struct ParserDeleter {
    void operator()(TSParser* p) const { if (p) ts_parser_delete(p); }
};
struct TreeDeleter {
    void operator()(TSTree* t) const { if (t) ts_tree_delete(t); }
};

int ParseScore(TSParser* parser, const GrammarBinding& binding, const std::string& source) {
    const TSLanguage* language = binding.parser();
    if (!language || !ts_parser_set_language(parser, language)) return -1000000;
    std::unique_ptr<TSTree, TreeDeleter> tree(
        ts_parser_parse_string(parser, nullptr, source.data(), (uint32_t)source.size()));
    if (!tree) return -1000000;
    TSNode root = ts_tree_root_node(tree.get());
    if (ts_node_is_null(root)) return -1000000;
    if (ts_node_has_error(root)) return -1000000;
    uint32_t named = ts_node_named_child_count(root);
    if (named == 0) return -1000000;
    int score = 8 + (int)std::min<uint32_t>(named, 24) * 2;
    // A few generic tokens improve discrimination between markup and code.
    if (source.find("def ") != std::string::npos ||
        source.find("import ") != std::string::npos) {
        if (wcscmp(binding.id, L"python") == 0) score += 6;
    }
    if (source.find("#include") != std::string::npos) {
        if (wcscmp(binding.id, L"cpp") == 0 || wcscmp(binding.id, L"c") == 0) score += 6;
    }
    return score;
}

} // namespace

void Initialize() {
    EnsureInitialized();
}

size_t Count() {
    EnsureInitialized();
    return Entries().size();
}

const Entry* At(size_t index) {
    EnsureInitialized();
    return index < Entries().size() ? &Entries()[index] : nullptr;
}

const Entry* FindById(const std::wstring& id) {
    EnsureInitialized();
    std::wstring key = ToLower(id);
    for (const Entry& entry : Entries()) {
        if (entry.id == key) return &entry;
    }
    return nullptr;
}

const Entry* FindByPath(const std::wstring& path) {
    EnsureInitialized();
    std::wstring name = ToLower(BaseName(path));
    if (name == L"cmakelists.txt") return FindById(L"cmake");
    if (name == L"dockerfile") return FindById(L"dockerfile");
    if (name == L"makefile") return FindById(L"make");
    size_t dot = name.find_last_of(L'.');
    if (dot == std::wstring::npos) return nullptr;
    std::wstring ext = name.substr(dot + 1);
    for (const Entry& entry : Entries()) {
        for (const std::wstring& candidate : entry.extensions) {
            if (ToLower(candidate) == ext) return &entry;
        }
    }
    return nullptr;
}

Language LegacyLanguageFor(const std::wstring& id) {
    static const std::unordered_map<std::wstring, Language> map = {
        {L"cpp", Language::Cpp}, {L"c", Language::C}, {L"cmake", Language::CMake},
        {L"python", Language::Python}, {L"javascript", Language::JavaScript},
        {L"typescript", Language::TypeScript}, {L"java", Language::Java},
        {L"csharp", Language::CSharp}, {L"go", Language::Go}, {L"rust", Language::Rust},
        {L"html", Language::Html}, {L"css", Language::Css}, {L"sql", Language::Sql},
        {L"yaml", Language::Yaml}, {L"xml", Language::Xml}, {L"json", Language::Json},
        {L"markdown", Language::Markdown}, {L"batch", Language::Batch},
        {L"powershell", Language::PowerShell}, {L"php", Language::Php},
        {L"ruby", Language::Ruby}, {L"swift", Language::Swift}, {L"kotlin", Language::Kotlin},
    };
    auto it = map.find(ToLower(id));
    return it == map.end() ? Language::Plain : it->second;
}

Language Detect(const std::wstring& utf16Text, std::wstring* detectedId) {
    EnsureInitialized();
    if (detectedId) detectedId->clear();

    // All 23 languages are parsed by the bundled tree-sitter language pack.
    // The legacy scorer is retained only as a deterministic tie-breaker for
    // very short snippets where several grammars parse without errors.
    Language legacy = LegacyDetectLanguageFromContent(utf16Text);
    std::string source = WideToUtf8(utf16Text);
    if (source.size() > 16384) source.resize(16384);
    std::unique_ptr<TSParser, ParserDeleter> parser(ts_parser_new());
    if (!parser) return legacy;

    const GrammarBinding* best = nullptr;
    int bestScore = 10;
    for (size_t i = 0; i < GrammarBindingCount; ++i) {
        const GrammarBinding& binding = GrammarBindings[i];
        int score = ParseScore(parser.get(), binding, source);
        if (score > bestScore) {
            bestScore = score;
            best = &binding;
        }
    }
    if (!best) return legacy;
    Language mapped = LegacyLanguageFor(best->id);

    // A strong signal from the legacy detector is authoritative. Tree-sitter
    // may parse an incomplete one-line fragment without errors and score it
    // highly under an unrelated grammar, which made language detection flip
    // intermittently while typing. Only use the tree-sitter result when the
    // legacy detector has no answer, or to pick the concrete language id.
    if (legacy != Language::Plain) {
        if (detectedId) {
            bool bestMatches = mapped == legacy;
            for (const Entry& entry : Entries()) {
                if (LegacyLanguageFor(entry.id) != legacy) continue;
                if (!bestMatches || entry.id == best->id) {
                    *detectedId = entry.id;
                    break;
                }
            }
            if (detectedId->empty()) *detectedId = best->id;
        }
        return legacy;
    }

    if (detectedId) *detectedId = best->id;
    return mapped == Language::Plain ? Language::Extended : mapped;
}

bool VisualFor(const std::wstring& id, std::wstring& monogram, uint32_t& color) {
    const Entry* entry = FindById(id);
    if (!entry) return false;
    monogram = entry->monogram;
    color = entry->color;
    return true;
}

std::wstring DisplayName(const std::wstring& id, Language legacy) {
    const Entry* entry = FindById(id);
    if (entry && !entry->name.empty()) return entry->name;
    return LanguageName(legacy);
}

} // namespace LanguagePack
