#include "syntax.h"
#include "completion_data.h"

#include <cctype>
#include <cwchar>

namespace {

bool IsIdentStart(wchar_t c) {
    return iswalpha(c) || c == L'_' || c >= 0x80;
}

bool IsIdentChar(wchar_t c) {
    return iswalnum(c) || c == L'_' || c >= 0x80;
}

bool IsDigit(wchar_t c) {
    return c >= L'0' && c <= L'9';
}

bool IsAllUpperIdentifier(const std::wstring& s) {
    bool hasAlpha = false;
    for (wchar_t c : s) {
        if (iswalpha(c)) {
            hasAlpha = true;
            if (iswlower(c)) return false;
        } else if (!(IsDigit(c) || c == L'_')) {
            return false;
        }
    }
    return hasAlpha;
}

const std::unordered_set<std::wstring>& CppKeywords() {
    static const std::unordered_set<std::wstring> set = {
        L"alignas", L"alignof", L"and", L"and_eq", L"asm", L"bitand", L"bitor",
        L"break", L"case", L"catch", L"class", L"compl", L"concept", L"const",
        L"consteval", L"constexpr", L"constinit", L"const_cast", L"continue",
        L"co_await", L"co_return", L"co_yield", L"decltype", L"default", L"delete",
        L"do", L"dynamic_cast", L"else", L"enum", L"explicit", L"export", L"extern",
        L"false", L"for", L"friend", L"goto", L"if", L"inline", L"mutable",
        L"namespace", L"new", L"noexcept", L"not", L"not_eq", L"nullptr", L"operator",
        L"or", L"or_eq", L"private", L"protected", L"public", L"register",
        L"reinterpret_cast", L"requires", L"return", L"sizeof", L"static",
        L"static_assert", L"static_cast", L"struct", L"switch", L"template", L"this",
        L"thread_local", L"throw", L"true", L"try", L"typedef", L"typeid",
        L"typename", L"union", L"using", L"virtual", L"volatile", L"while",
        L"xor", L"xor_eq", L"override", L"final", L"import", L"module", L"nullptr_t"
    };
    return set;
}

const std::unordered_set<std::wstring>& CppTypes() {
    static const std::unordered_set<std::wstring> set = {
        L"auto", L"bool", L"char", L"char8_t", L"char16_t", L"char32_t", L"double",
        L"float", L"int", L"long", L"short", L"signed", L"unsigned", L"void",
        L"wchar_t", L"size_t", L"ssize_t", L"ptrdiff_t", L"int8_t", L"int16_t",
        L"int32_t", L"int64_t", L"uint8_t", L"uint16_t", L"uint32_t", L"uint64_t",
        L"intptr_t", L"uintptr_t", L"std", L"string", L"wstring", L"string_view",
        L"vector", L"array", L"map", L"unordered_map", L"set", L"unordered_set",
        L"pair", L"tuple", L"optional", L"variant", L"unique_ptr", L"shared_ptr",
        L"weak_ptr", L"function", L"thread", L"mutex", L"atomic", L"span"
    };
    return set;
}

const std::unordered_set<std::wstring>& CBuiltins() {
    static const std::unordered_set<std::wstring> set = {
        L"printf", L"fprintf", L"sprintf", L"snprintf", L"scanf", L"fscanf",
        L"malloc", L"calloc", L"realloc", L"free", L"memcpy", L"memset", L"memmove",
        L"strlen", L"strcmp", L"strcpy", L"strncpy", L"fopen", L"fclose", L"fread",
        L"fwrite", L"fgets", L"fputs", L"puts", L"getchar", L"putchar", L"exit",
        L"assert", L"stdin", L"stdout", L"stderr", L"NULL"
    };
    return set;
}

const std::unordered_set<std::wstring>& PythonKeywords() {
    static const std::unordered_set<std::wstring> set = {
        L"and", L"as", L"assert", L"async", L"await", L"break", L"case", L"class",
        L"continue", L"def", L"del", L"elif", L"else", L"except", L"finally", L"for",
        L"from", L"global", L"if", L"import", L"in", L"is", L"lambda", L"match",
        L"nonlocal", L"not", L"or", L"pass", L"raise", L"return", L"try", L"while",
        L"with", L"yield", L"True", L"False", L"None", L"self", L"cls"
    };
    return set;
}

const std::unordered_set<std::wstring>& JsKeywords() {
    static const std::unordered_set<std::wstring> set = {
        L"break", L"case", L"catch", L"class", L"const", L"continue", L"debugger",
        L"default", L"delete", L"do", L"else", L"export", L"extends", L"finally",
        L"for", L"function", L"if", L"import", L"in", L"instanceof", L"let", L"new",
        L"return", L"static", L"super", L"switch", L"this", L"throw", L"try",
        L"typeof", L"var", L"void", L"while", L"with", L"yield", L"async", L"await",
        L"of", L"true", L"false", L"null", L"undefined", L"interface", L"type",
        L"enum", L"implements", L"readonly", L"namespace", L"declare", L"abstract",
        L"public", L"private", L"protected", L"as", L"from", L"satisfies"
    };
    return set;
}

const std::unordered_set<std::wstring>& CmakeKeywords();

const std::unordered_set<std::wstring>& KeywordsFor(Language language) {
    static const std::unordered_set<std::wstring> java = {
        L"abstract", L"assert", L"boolean", L"break", L"byte", L"case", L"catch",
        L"char", L"class", L"const", L"continue", L"default", L"do", L"double",
        L"else", L"enum", L"extends", L"final", L"finally", L"float", L"for",
        L"if", L"implements", L"import", L"instanceof", L"int", L"interface",
        L"long", L"native", L"new", L"package", L"private", L"protected", L"public",
        L"return", L"short", L"static", L"strictfp", L"super", L"switch",
        L"synchronized", L"this", L"throw", L"throws", L"transient", L"try",
        L"void", L"volatile", L"while", L"true", L"false", L"null", L"var",
        L"record", L"sealed", L"permits", L"yield"
    };
    static const std::unordered_set<std::wstring> csharp = {
        L"abstract", L"as", L"async", L"await", L"base", L"bool", L"break", L"byte",
        L"case", L"catch", L"char", L"checked", L"class", L"const", L"continue",
        L"decimal", L"default", L"delegate", L"do", L"double", L"else", L"enum",
        L"event", L"explicit", L"extern", L"false", L"finally", L"fixed", L"float",
        L"for", L"foreach", L"if", L"implicit", L"in", L"int", L"interface",
        L"internal", L"is", L"lock", L"long", L"namespace", L"new", L"null",
        L"object", L"operator", L"out", L"override", L"params", L"private",
        L"protected", L"public", L"readonly", L"ref", L"return", L"sbyte", L"sealed",
        L"short", L"sizeof", L"stackalloc", L"static", L"string", L"struct",
        L"switch", L"this", L"throw", L"true", L"try", L"typeof", L"uint", L"ulong",
        L"unchecked", L"unsafe", L"ushort", L"using", L"var", L"virtual", L"void",
        L"volatile", L"while", L"record", L"required", L"init", L"global"
    };
    static const std::unordered_set<std::wstring> go = {
        L"break", L"case", L"chan", L"const", L"continue", L"default", L"defer",
        L"else", L"fallthrough", L"for", L"func", L"go", L"goto", L"if", L"import",
        L"interface", L"map", L"package", L"range", L"return", L"select", L"struct",
        L"switch", L"type", L"var", L"bool", L"byte", L"complex64", L"complex128",
        L"error", L"float32", L"float64", L"int", L"int8", L"int16", L"int32",
        L"int64", L"rune", L"string", L"uint", L"uint8", L"uint16", L"uint32",
        L"uint64", L"uintptr", L"true", L"false", L"iota", L"nil", L"any"
    };
    static const std::unordered_set<std::wstring> rust = {
        L"as", L"async", L"await", L"break", L"const", L"continue", L"crate", L"dyn",
        L"else", L"enum", L"extern", L"false", L"fn", L"for", L"if", L"impl",
        L"in", L"let", L"loop", L"match", L"mod", L"move", L"mut", L"pub", L"ref",
        L"return", L"Self", L"self", L"static", L"struct", L"super", L"trait", L"true",
        L"type", L"unsafe", L"use", L"where", L"while", L"bool", L"char", L"f32",
        L"f64", L"i8", L"i16", L"i32", L"i64", L"i128", L"isize", L"str", L"String",
        L"u8", L"u16", L"u32", L"u64", L"u128", L"usize", L"as", L"Some", L"None"
    };
    static const std::unordered_set<std::wstring> sql = {
        L"add", L"all", L"alter", L"and", L"as", L"asc", L"begin", L"between",
        L"by", L"case", L"check", L"column", L"commit", L"constraint", L"create",
        L"database", L"default", L"delete", L"desc", L"distinct", L"drop", L"else",
        L"end",         L"exists", L"foreign", L"from", L"group", L"having", L"in", L"index",
        L"inner", L"insert", L"into", L"is", L"join", L"key", L"left", L"like",
        L"limit", L"not", L"null", L"offset", L"on", L"or", L"order", L"outer",
        L"primary", L"references", L"right", L"rollback", L"select", L"set",
        L"table", L"then", L"to", L"union", L"unique", L"update", L"values",
        L"when", L"where", L"with", L"view"
    };
    static const std::unordered_set<std::wstring> php = {
        L"abstract", L"and", L"array", L"as", L"break", L"callable", L"case", L"catch",
        L"class", L"clone", L"const", L"continue", L"declare", L"default", L"do", L"echo",
        L"else", L"elseif", L"empty", L"extends", L"final", L"finally", L"fn", L"for",
        L"foreach", L"function", L"global", L"goto", L"if", L"implements", L"include",
        L"include_once", L"instanceof", L"interface", L"isset", L"list", L"match",
        L"namespace", L"new", L"or", L"print", L"private", L"protected", L"public",
        L"readonly", L"require", L"require_once", L"return", L"static", L"switch",
        L"throw", L"trait", L"try", L"unset", L"use", L"var", L"while", L"xor", L"yield",
        L"true", L"false", L"null"
    };
    static const std::unordered_set<std::wstring> ruby = {
        L"alias", L"and", L"begin", L"break", L"case", L"class", L"def", L"defined?",
        L"do", L"else", L"elsif", L"end", L"ensure", L"false", L"for", L"if", L"in",
        L"module", L"next", L"nil", L"not", L"or", L"redo", L"rescue", L"retry", L"return",
        L"self", L"super", L"then", L"true", L"undef", L"unless", L"until", L"when",
        L"while", L"yield"
    };
    static const std::unordered_set<std::wstring> swift = {
        L"actor", L"as", L"Any", L"associatedtype", L"async", L"await", L"break", L"case",
        L"catch", L"class", L"continue", L"default", L"defer", L"deinit", L"enum",
        L"extension", L"fallthrough", L"false", L"final", L"for", L"func", L"guard", L"if",
        L"import", L"in", L"init", L"inout", L"internal", L"let", L"mutating", L"nil",
        L"open", L"private", L"protocol", L"public", L"repeat", L"return", L"self", L"Self",
        L"static", L"struct", L"subscript", L"super", L"switch", L"throw", L"throws",
        L"true", L"try", L"typealias", L"var", L"where", L"while"
    };
    static const std::unordered_set<std::wstring> kotlin = {
        L"abstract", L"actual", L"as", L"break", L"by", L"catch", L"class", L"companion",
        L"const", L"continue", L"data", L"do", L"else", L"enum", L"expect", L"external",
        L"false", L"final", L"finally", L"for", L"fun", L"if", L"import", L"in", L"inline",
        L"inner", L"interface", L"internal", L"is", L"lateinit", L"null", L"object", L"open",
        L"operator", L"out", L"override", L"package", L"private", L"protected", L"public",
        L"reified", L"return", L"sealed", L"suspend", L"tailrec", L"this", L"throw", L"true",
        L"try", L"typealias", L"val", L"var", L"when", L"where", L"while"
    };
    switch (language) {
        case Language::Java: return java;
        case Language::CSharp: return csharp;
        case Language::Go: return go;
        case Language::Rust: return rust;
        case Language::Sql: return sql;
        case Language::Php: return php;
        case Language::Ruby: return ruby;
        case Language::Swift: return swift;
        case Language::Kotlin: return kotlin;
        case Language::JavaScript:
        case Language::TypeScript: return JsKeywords();
        case Language::Cpp:
        case Language::C: return CppKeywords();
        case Language::Python: return PythonKeywords();
        case Language::CMake: return CmakeKeywords();
        default: {
            static const std::unordered_set<std::wstring> empty;
            return empty;
        }
    }
}

const std::unordered_set<std::wstring>& CmakeKeywords() {
    static const std::unordered_set<std::wstring> set = {
        L"if", L"elseif", L"else", L"endif", L"foreach", L"endforeach", L"while",
        L"endwhile", L"function", L"endfunction", L"macro", L"endmacro", L"return",
        L"break", L"continue", L"set", L"unset", L"list", L"string", L"file",
        L"message", L"option", L"add_executable", L"add_library", L"add_subdirectory",
        L"target_link_libraries", L"target_include_directories",
        L"target_compile_options", L"target_compile_features", L"find_package",
        L"include", L"project", L"cmake_minimum_required", L"install"
    };
    return set;
}

void AddToken(std::vector<Token>& tokens, int start, int len, TokenKind kind) {
    if (len <= 0 || kind == TokenKind::Normal) return;
    if (!tokens.empty()) {
        Token& back = tokens.back();
        if (back.kind == kind && back.start + back.len == start) {
            back.len += len;
            return;
        }
    }
    tokens.push_back(Token{start, len, kind});
}

bool IsQuote(wchar_t c) {
    return c == L'"' || c == L'\'' || c == L'`';
}

int ScanString(const std::wstring& text, int start, wchar_t quote, bool keepBackslash = true) {
    int i = start + 1;
    int n = (int)text.size();
    while (i < n) {
        if (text[i] == L'\\' && keepBackslash) {
            i += 2;
            continue;
        }
        if (text[i] == quote) return i + 1;
        if (text[i] == L'\n') return i;
        ++i;
    }
    return n;
}

int ScanNumber(const std::wstring& text, int start) {
    int n = (int)text.size();
    int i = start;
    if (text[i] == L'0' && i + 1 < n && (text[i + 1] == L'x' || text[i + 1] == L'X' ||
                                          text[i + 1] == L'b' || text[i + 1] == L'B')) {
        i += 2;
        while (i < n && (iswalnum(text[i]) || text[i] == L'\'')) ++i;
        return i;
    }
    bool seenDot = false;
    while (i < n) {
        wchar_t c = text[i];
        if (IsDigit(c) || c == L'\'') {
            ++i;
        } else if (c == L'.' && !seenDot) {
            seenDot = true;
            ++i;
        } else if ((c == L'+' || c == L'-') && i > start &&
                   (text[i - 1] == L'e' || text[i - 1] == L'E' ||
                    text[i - 1] == L'p' || text[i - 1] == L'P')) {
            ++i;
        } else if (iswalpha(c)) {
            ++i;
        } else {
            break;
        }
    }
    return i;
}

void HighlightCLike(const std::wstring& text, Language lang, LexState& state,
                    std::vector<Token>& tokens, bool hashComments) {
    const int n = (int)text.size();
    int i = 0;
    if (state == LexState::BlockComment) {
        int close = (int)text.find(L"*/");
        if (close == (int)std::wstring::npos) {
            AddToken(tokens, 0, n, TokenKind::Comment);
            return;
        }
        AddToken(tokens, 0, close + 2, TokenKind::Comment);
        i = close + 2;
        state = LexState::Normal;
    }

    bool preprocessorLanguage = (lang == Language::Cpp || lang == Language::C);
    if (preprocessorLanguage) {
        int first = 0;
        while (first < n && iswspace(text[first])) ++first;
        if (first < n && text[first] == L'#') {
            AddToken(tokens, first, n - first, TokenKind::Preproc);
            return;
        }
    }

    while (i < n) {
        wchar_t c = text[i];
        if (iswspace(c)) {
            ++i;
            continue;
        }
        if (hashComments && c == L'#') {
            AddToken(tokens, i, n - i, TokenKind::Comment);
            return;
        }
        if (c == L'/' && i + 1 < n && text[i + 1] == L'/') {
            AddToken(tokens, i, n - i, TokenKind::Comment);
            return;
        }
        if (c == L'/' && i + 1 < n && text[i + 1] == L'*') {
            int close = (int)text.find(L"*/", i + 2);
            if (close == (int)std::wstring::npos) {
                AddToken(tokens, i, n - i, TokenKind::Comment);
                state = LexState::BlockComment;
                return;
            }
            AddToken(tokens, i, close + 2 - i, TokenKind::Comment);
            i = close + 2;
            continue;
        }
        if (IsQuote(c)) {
            int end = ScanString(text, i, c);
            AddToken(tokens, i, end - i, TokenKind::String);
            i = end;
            continue;
        }
        if (IsDigit(c) || (c == L'.' && i + 1 < n && IsDigit(text[i + 1]))) {
            int end = ScanNumber(text, i);
            AddToken(tokens, i, end - i, TokenKind::Number);
            i = end;
            continue;
        }
        if (IsIdentStart(c)) {
            int start = i;
            while (i < n && IsIdentChar(text[i])) ++i;
            std::wstring ident = text.substr(start, i - start);
            int next = i;
            while (next < n && (text[next] == L' ' || text[next] == L'\t')) ++next;
            bool call = next < n && text[next] == L'(';
            TokenKind kind = TokenKind::Normal;

            if (lang == Language::C || lang == Language::Cpp) {
                if (CppTypes().count(ident)) kind = TokenKind::Type;
                else if (CppKeywords().count(ident)) kind = TokenKind::Keyword;
                else if (CBuiltins().count(ident)) kind = TokenKind::Builtin;
                else if (IsAllUpperIdentifier(ident)) kind = TokenKind::Macro;
                else if (!ident.empty() && iswupper(ident[0])) kind = TokenKind::Type;
                else if (call) kind = TokenKind::Function;
            } else if (lang == Language::Java || lang == Language::CSharp ||
                       lang == Language::Go || lang == Language::Rust ||
                       lang == Language::Swift || lang == Language::Kotlin ||
                       lang == Language::Php || lang == Language::Ruby) {
                if (KeywordsFor(lang).count(ident)) kind = TokenKind::Keyword;
                else if (!ident.empty() && iswupper(ident[0])) kind = TokenKind::Type;
                else if (call) kind = TokenKind::Function;
            } else if (lang == Language::JavaScript || lang == Language::TypeScript) {
                if (JsKeywords().count(ident)) kind = TokenKind::Keyword;
                else if (!ident.empty() && iswupper(ident[0])) kind = TokenKind::Type;
                else if (call) kind = TokenKind::Function;
            } else if (lang == Language::CMake) {
                if (CmakeKeywords().count(ident)) kind = TokenKind::Keyword;
                else if (call) kind = TokenKind::Function;
            } else if (lang == Language::Batch) {
                if (IEquals(ident, L"rem")) {
                    AddToken(tokens, start, n - start, TokenKind::Comment);
                    return;
                }
                if (IEquals(ident, L"if") || IEquals(ident, L"else") ||
                    IEquals(ident, L"for") || IEquals(ident, L"goto") ||
                    IEquals(ident, L"call") || IEquals(ident, L"set") ||
                    IEquals(ident, L"echo")) {
                    kind = TokenKind::Keyword;
                }
            } else if (lang == Language::PowerShell) {
                if (IEquals(ident, L"if") || IEquals(ident, L"else") ||
                    IEquals(ident, L"elseif") || IEquals(ident, L"foreach") ||
                    IEquals(ident, L"function") || IEquals(ident, L"param") ||
                    IEquals(ident, L"return") || IEquals(ident, L"while") ||
                    IEquals(ident, L"switch") || IEquals(ident, L"try") ||
                    IEquals(ident, L"catch") || IEquals(ident, L"finally") ||
                    IEquals(ident, L"class") || IEquals(ident, L"using") ||
                    IEquals(ident, L"in")) {
                    kind = TokenKind::Keyword;
                } else if (call) {
                    kind = TokenKind::Function;
                }
            }
            AddToken(tokens, start, i - start, kind);
            continue;
        }
        if ((lang == Language::PowerShell) && c == L'$') {
            int start = i++;
            while (i < n && (IsIdentChar(text[i]) || text[i] == L':' || text[i] == L'.')) ++i;
            AddToken(tokens, start, i - start, TokenKind::Builtin);
            continue;
        }
        if (c == L'@' && lang == Language::Python) {
            int start = i++;
            while (i < n && IsIdentChar(text[i])) ++i;
            AddToken(tokens, start, i - start, TokenKind::Function);
            continue;
        }
        ++i;
    }
}

void HighlightSql(const std::wstring& text, LexState& state,
                  std::vector<Token>& tokens) {
    const int n = (int)text.size();
    int i = 0;
    if (state == LexState::BlockComment) {
        int close = (int)text.find(L"*/");
        if (close == (int)std::wstring::npos) {
            AddToken(tokens, 0, n, TokenKind::Comment);
            return;
        }
        AddToken(tokens, 0, close + 2, TokenKind::Comment);
        i = close + 2;
        state = LexState::Normal;
    }
    while (i < n) {
        wchar_t c = text[(size_t)i];
        if (iswspace(c)) { ++i; continue; }
        if (c == L'-' && i + 1 < n && text[(size_t)i + 1] == L'-') {
            AddToken(tokens, i, n - i, TokenKind::Comment);
            return;
        }
        if (c == L'/' && i + 1 < n && text[(size_t)i + 1] == L'*') {
            int close = (int)text.find(L"*/", (size_t)i + 2);
            if (close == (int)std::wstring::npos) {
                AddToken(tokens, i, n - i, TokenKind::Comment);
                state = LexState::BlockComment;
                return;
            }
            AddToken(tokens, i, close + 2 - i, TokenKind::Comment);
            i = close + 2;
            continue;
        }
        if (c == L'\'' || c == L'"') {
            int end = i + 1;
            while (end < n) {
                if (text[(size_t)end] == c) {
                    if (end + 1 < n && text[(size_t)end + 1] == c) { end += 2; continue; }
                    ++end;
                    break;
                }
                if (text[(size_t)end] == L'\\' && end + 1 < n) end += 2;
                else ++end;
            }
            AddToken(tokens, i, end - i, TokenKind::String);
            i = end;
            continue;
        }
        if (IsDigit(c)) {
            int end = ScanNumber(text, i);
            AddToken(tokens, i, end - i, TokenKind::Number);
            i = end;
            continue;
        }
        if (IsIdentStart(c)) {
            int start = i++;
            while (i < n && IsIdentChar(text[(size_t)i])) ++i;
            std::wstring ident = ToLower(text.substr((size_t)start, (size_t)(i - start)));
            TokenKind kind = KeywordsFor(Language::Sql).count(ident)
                                 ? TokenKind::Keyword : TokenKind::Normal;
            if (kind == TokenKind::Normal) {
                int next = i;
                while (next < n && iswspace(text[(size_t)next])) ++next;
                if (next < n && text[(size_t)next] == L'(') kind = TokenKind::Function;
            }
            AddToken(tokens, start, i - start, kind);
            continue;
        }
        ++i;
    }
}

void HighlightMarkup(const std::wstring& text, LexState& state,
                     std::vector<Token>& tokens) {
    const int n = (int)text.size();
    int i = 0;
    if (state == LexState::BlockComment) {
        int close = (int)text.find(L"-->");
        if (close == (int)std::wstring::npos) {
            AddToken(tokens, 0, n, TokenKind::Comment);
            return;
        }
        AddToken(tokens, 0, close + 3, TokenKind::Comment);
        i = close + 3;
        state = LexState::Normal;
    }
    while (i < n) {
        if (i + 3 < n && text.compare((size_t)i, 4, L"<!--") == 0) {
            int close = (int)text.find(L"-->", (size_t)i + 4);
            if (close == (int)std::wstring::npos) {
                AddToken(tokens, i, n - i, TokenKind::Comment);
                state = LexState::BlockComment;
                return;
            }
            AddToken(tokens, i, close + 3 - i, TokenKind::Comment);
            i = close + 3;
            continue;
        }
        if (text[(size_t)i] != L'<') { ++i; continue; }
        int tagStart = i++;
        if (i < n && text[(size_t)i] == L'/') ++i;
        while (i < n && iswspace(text[(size_t)i])) ++i;
        int nameStart = i;
        while (i < n && (IsIdentChar(text[(size_t)i]) || text[(size_t)i] == L'-' ||
                         text[(size_t)i] == L':')) ++i;
        if (i == nameStart) { i = tagStart + 1; continue; }
        AddToken(tokens, tagStart, 1, TokenKind::Preproc);
        AddToken(tokens, nameStart, i - nameStart, TokenKind::Type);
        while (i < n && text[(size_t)i] != L'>') {
            if (iswspace(text[(size_t)i]) || text[(size_t)i] == L'/') { ++i; continue; }
            if (IsQuote(text[(size_t)i])) {
                int end = ScanString(text, i, text[(size_t)i]);
                AddToken(tokens, i, end - i, TokenKind::String);
                i = end;
                continue;
            }
            if (IsIdentStart(text[(size_t)i])) {
                int start = i++;
                while (i < n && (IsIdentChar(text[(size_t)i]) || text[(size_t)i] == L'-' ||
                                 text[(size_t)i] == L':')) ++i;
                AddToken(tokens, start, i - start, TokenKind::Property);
                continue;
            }
            ++i;
        }
        if (i < n) ++i;
        if (i > tagStart + 1 && text[(size_t)(i - 1)] == L'>') {
            AddToken(tokens, i - 1, 1, TokenKind::Preproc);
        }
    }
}

void HighlightCss(const std::wstring& text, LexState& state,
                  std::vector<Token>& tokens) {
    const int n = (int)text.size();
    int i = 0;
    if (state == LexState::BlockComment) {
        int close = (int)text.find(L"*/");
        if (close == (int)std::wstring::npos) {
            AddToken(tokens, 0, n, TokenKind::Comment);
            return;
        }
        AddToken(tokens, 0, close + 2, TokenKind::Comment);
        i = close + 2;
        state = LexState::Normal;
    }
    while (i < n) {
        wchar_t c = text[(size_t)i];
        if (iswspace(c)) { ++i; continue; }
        if (c == L'/' && i + 1 < n && text[(size_t)i + 1] == L'*') {
            int close = (int)text.find(L"*/", (size_t)i + 2);
            if (close == (int)std::wstring::npos) {
                AddToken(tokens, i, n - i, TokenKind::Comment);
                state = LexState::BlockComment;
                return;
            }
            AddToken(tokens, i, close + 2 - i, TokenKind::Comment);
            i = close + 2;
            continue;
        }
        if (IsQuote(c)) {
            int end = ScanString(text, i, c);
            AddToken(tokens, i, end - i, TokenKind::String);
            i = end;
            continue;
        }
        if (IsDigit(c)) {
            int end = ScanNumber(text, i);
            AddToken(tokens, i, end - i, TokenKind::Number);
            i = end;
            continue;
        }
        if (IsIdentStart(c)) {
            int start = i++;
            while (i < n && (IsIdentChar(text[(size_t)i]) || text[(size_t)i] == L'-')) ++i;
            int next = i;
            while (next < n && iswspace(text[(size_t)next])) ++next;
            TokenKind kind = next < n && text[(size_t)next] == L':'
                                 ? TokenKind::Property
                                 : (next < n && text[(size_t)next] == L'('
                                        ? TokenKind::Function : TokenKind::Normal);
            AddToken(tokens, start, i - start, kind);
            continue;
        }
        ++i;
    }
}

void HighlightYaml(const std::wstring& text, std::vector<Token>& tokens) {
    const int n = (int)text.size();
    int i = 0;
    while (i < n) {
        wchar_t c = text[(size_t)i];
        if (iswspace(c)) { ++i; continue; }
        if (c == L'#') { AddToken(tokens, i, n - i, TokenKind::Comment); return; }
        if (IsQuote(c)) {
            int end = ScanString(text, i, c);
            AddToken(tokens, i, end - i, TokenKind::String);
            i = end;
            continue;
        }
        if (IsDigit(c) || (c == L'-' && i + 1 < n && IsDigit(text[(size_t)i + 1]))) {
            int end = ScanNumber(text, i);
            AddToken(tokens, i, end - i, TokenKind::Number);
            i = end;
            continue;
        }
        if (IsIdentStart(c) || c == L'-') {
            int start = i++;
            while (i < n && (IsIdentChar(text[(size_t)i]) || text[(size_t)i] == L'-' ||
                             text[(size_t)i] == L'.')) ++i;
            int next = i;
            while (next < n && iswspace(text[(size_t)next])) ++next;
            std::wstring ident = ToLower(text.substr((size_t)start, (size_t)(i - start)));
            TokenKind kind = next < n && text[(size_t)next] == L':'
                                 ? TokenKind::Property
                                 : ((ident == L"true" || ident == L"false" ||
                                     ident == L"null" || ident == L"yes" || ident == L"no")
                                        ? TokenKind::Keyword : TokenKind::Normal);
            AddToken(tokens, start, i - start, kind);
            continue;
        }
        ++i;
    }
}

void HighlightPython(const std::wstring& text, std::vector<Token>& tokens) {
    const int n = (int)text.size();
    int i = 0;
    while (i < n) {
        wchar_t c = text[i];
        if (iswspace(c)) {
            ++i;
            continue;
        }
        if (c == L'#') {
            AddToken(tokens, i, n - i, TokenKind::Comment);
            return;
        }
        if (IsQuote(c)) {
            if (i + 2 < n && text[i + 1] == c && text[i + 2] == c) {
                int close = (int)text.find(std::wstring(3, c), i + 3);
                int end = close == (int)std::wstring::npos ? n : close + 3;
                AddToken(tokens, i, end - i, TokenKind::String);
                i = end;
                continue;
            }
            int end = ScanString(text, i, c);
            AddToken(tokens, i, end - i, TokenKind::String);
            i = end;
            continue;
        }
        if (IsDigit(c) || (c == L'.' && i + 1 < n && IsDigit(text[i + 1]))) {
            int end = ScanNumber(text, i);
            AddToken(tokens, i, end - i, TokenKind::Number);
            i = end;
            continue;
        }
        if (IsIdentStart(c)) {
            int start = i;
            while (i < n && IsIdentChar(text[i])) ++i;
            std::wstring ident = text.substr(start, i - start);
            TokenKind kind = TokenKind::Normal;
            if (PythonKeywords().count(ident)) kind = TokenKind::Keyword;
            else if (!ident.empty() && iswupper(ident[0])) kind = TokenKind::Type;
            int next = i;
            while (next < n && (text[next] == L' ' || text[next] == L'\t')) ++next;
            if (kind == TokenKind::Normal && next < n && text[next] == L'(') kind = TokenKind::Function;
            AddToken(tokens, start, i - start, kind);
            continue;
        }
        ++i;
    }
}

void HighlightJson(const std::wstring& text, std::vector<Token>& tokens) {
    const int n = (int)text.size();
    int i = 0;
    while (i < n) {
        wchar_t c = text[i];
        if (iswspace(c)) {
            ++i;
            continue;
        }
        if (IsQuote(c)) {
            int end = ScanString(text, i, c);
            int next = end;
            while (next < n && iswspace(text[next])) ++next;
            TokenKind kind = (next < n && text[next] == L':') ? TokenKind::Property : TokenKind::String;
            AddToken(tokens, i, end - i, kind);
            i = end;
            continue;
        }
        if (IsDigit(c) || c == L'-') {
            int end = ScanNumber(text, i);
            AddToken(tokens, i, end - i, TokenKind::Number);
            i = end;
            continue;
        }
        if (IsIdentStart(c)) {
            int start = i;
            while (i < n && IsIdentChar(text[i])) ++i;
            std::wstring ident = text.substr(start, i - start);
            if (ident == L"true" || ident == L"false" || ident == L"null") {
                AddToken(tokens, start, i - start, TokenKind::Keyword);
            }
            continue;
        }
        ++i;
    }
}

void HighlightMarkdown(const std::wstring& text, std::vector<Token>& tokens) {
    int first = 0;
    int n = (int)text.size();
    while (first < n && iswspace(text[first])) ++first;
    if (first >= n) return;
    if (IStartsWith(text.substr(first), L"```") || IStartsWith(text.substr(first), L"~~~")) {
        AddToken(tokens, first, n - first, TokenKind::Comment);
        return;
    }
    if (text[first] == L'#') {
        int p = first;
        while (p < n && text[p] == L'#') ++p;
        if (p < n && text[p] == L' ') {
            AddToken(tokens, first, p - first, TokenKind::Keyword);
            AddToken(tokens, p + 1, n - p - 1, TokenKind::Type);
            return;
        }
    }
    if (text[first] == L'>') {
        AddToken(tokens, first, n - first, TokenKind::Comment);
        return;
    }
    int i = 0;
    while (i < n) {
        if (text[i] == L'`') {
            int close = (int)text.find(L'`', i + 1);
            int end = close == (int)std::wstring::npos ? n : close + 1;
            AddToken(tokens, i, end - i, TokenKind::String);
            i = end;
        } else if (i + 1 < n && text[i] == L'*' && text[i + 1] == L'*') {
            int close = (int)text.find(L"**", i + 2);
            int end = close == (int)std::wstring::npos ? n : close + 2;
            AddToken(tokens, i, end - i, TokenKind::Keyword);
            i = end;
        } else {
            ++i;
        }
    }
}

} // namespace

Language LanguageFromPath(const std::wstring& path) {
    std::wstring name = ToLower(BaseName(path));
    if (name == L"cmakelists.txt") return Language::CMake;
    size_t dot = name.find_last_of(L'.');
    std::wstring ext = dot == std::wstring::npos ? L"" : name.substr(dot);
    if (ext == L".cpp" || ext == L".cc" || ext == L".cxx" || ext == L".c++" ||
        ext == L".hpp" || ext == L".hh" || ext == L".hxx" || ext == L".h" ||
        ext == L".h++" || ext == L".inl" || ext == L".ipp" || ext == L".tpp") {
        return Language::Cpp;
    }
    if (ext == L".c") return Language::C;
    if (ext == L".cmake") return Language::CMake;
    if (ext == L".py" || ext == L".pyw") return Language::Python;
    if (ext == L".js" || ext == L".mjs" || ext == L".cjs") return Language::JavaScript;
    if (ext == L".ts" || ext == L".tsx" || ext == L".jsx" || ext == L".mts" || ext == L".cts") {
        return Language::TypeScript;
    }
    if (ext == L".java") return Language::Java;
    if (ext == L".cs") return Language::CSharp;
    if (ext == L".go") return Language::Go;
    if (ext == L".rs") return Language::Rust;
    if (ext == L".html" || ext == L".htm" || ext == L".xhtml") return Language::Html;
    if (ext == L".css" || ext == L".scss" || ext == L".less") return Language::Css;
    if (ext == L".sql") return Language::Sql;
    if (ext == L".yaml" || ext == L".yml") return Language::Yaml;
    if (ext == L".xml" || ext == L".svg") return Language::Xml;
    if (ext == L".json" || ext == L".jsonc" || ext == L".json5") return Language::Json;
    if (ext == L".md" || ext == L".markdown" || ext == L".mdx") return Language::Markdown;
    if (ext == L".bat" || ext == L".cmd") return Language::Batch;
    if (ext == L".ps1" || ext == L".psm1") return Language::PowerShell;
    if (ext == L".php" || ext == L".phtml" || ext == L".php3") return Language::Php;
    if (ext == L".rb") return Language::Ruby;
    if (ext == L".swift") return Language::Swift;
    if (ext == L".kt" || ext == L".kts") return Language::Kotlin;
    return Language::Plain;
}

const wchar_t* LanguageName(Language language) {
    switch (language) {
        case Language::Cpp: return L"C++";
        case Language::C: return L"C";
        case Language::CMake: return L"CMake";
        case Language::Python: return L"Python";
        case Language::JavaScript: return L"JavaScript";
        case Language::TypeScript: return L"TypeScript";
        case Language::Java: return L"Java";
        case Language::CSharp: return L"C#";
        case Language::Go: return L"Go";
        case Language::Rust: return L"Rust";
        case Language::Html: return L"HTML";
        case Language::Css: return L"CSS";
        case Language::Sql: return L"SQL";
        case Language::Yaml: return L"YAML";
        case Language::Xml: return L"XML";
        case Language::Json: return L"JSON";
        case Language::Markdown: return L"Markdown";
        case Language::Batch: return L"Batch";
        case Language::PowerShell: return L"PowerShell";
        case Language::Php: return L"PHP";
        case Language::Ruby: return L"Ruby";
        case Language::Swift: return L"Swift";
        case Language::Kotlin: return L"Kotlin";
        default: return L"Plain Text";
    }
}

namespace {

constexpr size_t kDetectionBudget = 6000;
constexpr int kMinimumDetectionScore = 5;

bool TextContains(const std::wstring& s, const wchar_t* needle) {
    return s.find(needle) != std::wstring::npos;
}

bool TextStarts(const std::wstring& s, const wchar_t* needle) {
    size_t n = wcslen(needle);
    return s.size() >= n && s.compare(0, n, needle) == 0;
}

bool ContainsWord(const std::wstring& s, const wchar_t* word) {
    size_t length = wcslen(word);
    if (length == 0) return false;
    size_t pos = 0;
    while ((pos = s.find(word, pos)) != std::wstring::npos) {
        bool left = pos == 0 || !IsWordChar(s[pos - 1]);
        size_t after = pos + length;
        bool right = after >= s.size() || !IsWordChar(s[after]);
        if (left && right) return true;
        pos = after;
    }
    return false;
}

bool ContainsDollarAssignment(const std::wstring& s) {
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] != L'$') continue;
        size_t j = i + 1;
        if (j >= s.size() || !IsIdentStart(s[j])) continue;
        while (j < s.size() && IsIdentChar(s[j])) ++j;
        while (j < s.size() && (s[j] == L' ' || s[j] == L'\t')) ++j;
        if (j < s.size() && s[j] == L'=') return true;
    }
    return false;
}

// An unquoted "key: value" mapping line. Statement terminators and braces
// exclude CSS declarations and C-style code from being mistaken for YAML.
bool IsMappingLine(const std::wstring& line) {
    if (TextContains(line, L";") || TextContains(line, L"{") || TextContains(line, L"}")) return false;
    size_t i = 0;
    while (i < line.size() && iswspace(line[i])) ++i;
    if (i >= line.size() || line[i] == L'"' || line[i] == L'\'') return false;
    size_t start = i;
    while (i < line.size() && (IsIdentChar(line[i]) || line[i] == L'-' || line[i] == L'.')) ++i;
    if (i == start) return false;
    while (i < line.size() && iswspace(line[i])) ++i;
    if (i >= line.size() || line[i] != L':') return false;
    ++i;
    while (i < line.size() && iswspace(line[i])) ++i;
    return i < line.size();
}

// Line-level signals shared by the CSS, YAML and Markdown heuristics.
struct DetectionSignals {
    int mappingLines = 0;
    int cssSelectors = 0;
    int cssDeclarations = 0;
    int markdownHeadings = 0;
    int markdownBullets = 0;

    explicit DetectionSignals(const std::vector<std::wstring>& lines) {
        for (const std::wstring& sourceLine : lines) {
            std::wstring trimmed = Trim(sourceLine);
            if (IsMappingLine(sourceLine)) ++mappingLines;
            if (!trimmed.empty()) {
                size_t brace = trimmed.find(L'{');
                if (brace != std::wstring::npos &&
                    (brace == 0 || (trimmed[brace - 1] != L':' && trimmed[brace - 1] != L'='))) {
                    ++cssSelectors;
                }
                if (TextContains(trimmed, L":") && TextContains(trimmed, L";")) ++cssDeclarations;
            }
            if (TextStarts(trimmed, L"# ")) ++markdownHeadings;
            if (TextStarts(trimmed, L"- ") || TextStarts(trimmed, L"* ") ||
                TextStarts(trimmed, L"> ")) ++markdownBullets;
        }
    }
};

// Accumulates weighted evidence. The winner must clear kMinimumDetectionScore
// so short, inconclusive snippets keep the default/plain icon.
class LanguageScores {
public:
    void Add(Language language, int weight) { scores_[(int)language] += weight; }

    Language Best(int minimum) const {
        static const Language order[] = {
            Language::Cpp, Language::C, Language::CMake, Language::Python,
            Language::JavaScript, Language::TypeScript, Language::Java,
            Language::CSharp, Language::Go, Language::Rust, Language::Html,
            Language::Css, Language::Sql, Language::Yaml, Language::Xml,
            Language::Json, Language::Markdown, Language::Batch, Language::PowerShell,
            Language::Php, Language::Ruby, Language::Swift, Language::Kotlin
        };
        Language best = Language::Plain;
        int bestScore = minimum - 1;
        for (Language language : order) {
            auto it = scores_.find((int)language);
            if (it != scores_.end() && it->second > bestScore) {
                bestScore = it->second;
                best = language;
            }
        }
        return best;
    }

private:
    std::unordered_map<int, int> scores_;
};

// Each Score* function owns the heuristics for one language family. Adding a
// language means writing one more function and calling it from
// DetectLanguageFromContent below.
void ScoreShebang(const std::wstring& first, LanguageScores& scores) {
    if (!TextStarts(first, L"#!")) return;
    if (TextContains(first, L"python")) scores.Add(Language::Python, 14);
    else if (TextContains(first, L"node")) scores.Add(Language::JavaScript, 14);
    else if (TextContains(first, L"pwsh") || TextContains(first, L"powershell"))
        scores.Add(Language::PowerShell, 14);
    else if (TextContains(first, L"bash") || TextContains(first, L"sh"))
        scores.Add(Language::Batch, 10);
}

void ScoreCMake(const std::wstring& lower, const std::wstring& first, LanguageScores& scores) {
    if (TextContains(lower, L"cmake_minimum_required(")) scores.Add(Language::CMake, 8);
    if (TextContains(lower, L"add_executable(") || TextContains(lower, L"add_library("))
        scores.Add(Language::CMake, 6);
    if (TextContains(lower, L"target_link_libraries(") ||
        TextContains(lower, L"target_include_directories(") ||
        TextContains(lower, L"target_compile_options("))
        scores.Add(Language::CMake, 5);
    if (TextStarts(first, L"project(")) scores.Add(Language::CMake, 4);
}

void ScoreCAndCpp(const std::wstring& lower, LanguageScores& scores) {
    if (TextContains(lower, L"#include")) {
        scores.Add(Language::Cpp, 3);
        scores.Add(Language::C, 2);
        if (TextContains(lower, L"<iostream") || TextContains(lower, L"<vector") ||
            TextContains(lower, L"<string") || TextContains(lower, L"<memory") ||
            TextContains(lower, L"<algorithm") || TextContains(lower, L"<unordered_"))
            scores.Add(Language::Cpp, 6);
        if (TextContains(lower, L"<stdio.h") || TextContains(lower, L"<stdlib.h") ||
            TextContains(lower, L"<string.h"))
            scores.Add(Language::C, 6);
    }
    if (ContainsWord(lower, L"using") && ContainsWord(lower, L"namespace")) scores.Add(Language::Cpp, 7);
    if (TextContains(lower, L"std::")) scores.Add(Language::Cpp, 5);
    if (TextContains(lower, L"template<") || TextContains(lower, L"template <")) scores.Add(Language::Cpp, 4);
    if (ContainsWord(lower, L"class") && TextContains(lower, L"public:")) scores.Add(Language::Cpp, 4);
    if (ContainsWord(lower, L"namespace")) scores.Add(Language::Cpp, 3);
    if (TextContains(lower, L"printf(") || TextContains(lower, L"malloc(") ||
        TextContains(lower, L"struct ")) scores.Add(Language::C, 3);
    if (TextContains(lower, L"int main(")) {
        scores.Add(Language::Cpp, 2);
        scores.Add(Language::C, 2);
    }
}

void ScorePython(const std::wstring& lower, const std::wstring& first,
                 const std::vector<std::wstring>& lines, LanguageScores& scores) {
    if (ContainsWord(lower, L"def")) scores.Add(Language::Python, 5);
    if (ContainsWord(lower, L"import")) scores.Add(Language::Python, 3);
    if (TextStarts(first, L"import ") && !TextContains(first, L" from ")) scores.Add(Language::Python, 3);
    if (TextContains(lower, L"if __name__")) scores.Add(Language::Python, 8);
    if (ContainsWord(lower, L"elif") || ContainsWord(lower, L"lambda") ||
        ContainsWord(lower, L"yield")) scores.Add(Language::Python, 4);
    if (ContainsWord(lower, L"self") || TextContains(lower, L"self.")) scores.Add(Language::Python, 3);
    if (TextContains(lower, L"print(")) scores.Add(Language::Python, 3);
    if (ContainsWord(lower, L"class") && TextContains(lower, L":")) scores.Add(Language::Python, 2);
    for (const std::wstring& sourceLine : lines) {
        std::wstring trimmed = ToLower(Trim(sourceLine));
        if (TextStarts(trimmed, L"from ") && TextContains(trimmed, L" import ")) {
            scores.Add(Language::Python, 4);
            break;
        }
    }
}

void ScoreJavaScriptAndTypeScript(const std::wstring& lower, const std::wstring& first,
                                  LanguageScores& scores) {
    if (ContainsWord(lower, L"function")) {
        scores.Add(Language::JavaScript, 5);
        scores.Add(Language::TypeScript, 4);
    }
    if (TextContains(lower, L"=>")) {
        scores.Add(Language::JavaScript, 4);
        scores.Add(Language::TypeScript, 3);
    }
    if (TextContains(lower, L"console.log")) {
        scores.Add(Language::JavaScript, 7);
        scores.Add(Language::TypeScript, 3);
    }
    if (TextContains(lower, L"document.") || TextContains(lower, L"window.")) scores.Add(Language::JavaScript, 5);
    if (TextContains(lower, L"require(") || TextContains(lower, L"module.exports"))
        scores.Add(Language::JavaScript, 5);
    if (TextContains(lower, L"export default")) scores.Add(Language::JavaScript, 5);
    if (ContainsWord(lower, L"import") && TextContains(lower, L" from ")) {
        scores.Add(Language::JavaScript, 4);
        scores.Add(Language::TypeScript, 3);
    }
    if (ContainsWord(lower, L"const") || ContainsWord(lower, L"let")) {
        scores.Add(Language::JavaScript, 2);
        scores.Add(Language::TypeScript, 2);
    }
    if (TextStarts(first, L"const ") || TextStarts(first, L"let ")) scores.Add(Language::JavaScript, 3);
    if (TextContains(lower, L"===")) scores.Add(Language::JavaScript, 2);
    if (ContainsWord(lower, L"interface")) scores.Add(Language::TypeScript, 6);
    if (TextContains(lower, L"export interface")) scores.Add(Language::TypeScript, 8);
    if (TextContains(lower, L": string") || TextContains(lower, L": number") ||
        TextContains(lower, L": boolean") || TextContains(lower, L": any"))
        scores.Add(Language::TypeScript, 4);
    if (TextContains(lower, L"as const") || ContainsWord(lower, L"declare") ||
        TextContains(lower, L"readonly ")) scores.Add(Language::TypeScript, 4);
    if (TextContains(lower, L"type ") && TextContains(lower, L" = ")) scores.Add(Language::TypeScript, 3);
}

void ScoreJava(const std::wstring& lower, const std::wstring& first, LanguageScores& scores) {
    if (TextContains(lower, L"public class") || TextContains(lower, L"public interface"))
        scores.Add(Language::Java, 6);
    if (TextStarts(first, L"package ")) scores.Add(Language::Java, 5);
    if (TextContains(lower, L"import java.")) scores.Add(Language::Java, 7);
    if (TextContains(lower, L"static void main") || TextContains(lower, L"string[] args"))
        scores.Add(Language::Java, 7);
    if (TextContains(lower, L"system.out.println")) scores.Add(Language::Java, 6);
    if (TextContains(lower, L"@override")) scores.Add(Language::Java, 3);
}

void ScoreCSharp(const std::wstring& lower, LanguageScores& scores) {
    if (TextContains(lower, L"using system")) scores.Add(Language::CSharp, 7);
    if (TextContains(lower, L"console.writeline") || TextContains(lower, L"console.readline"))
        scores.Add(Language::CSharp, 7);
    if (ContainsWord(lower, L"namespace")) scores.Add(Language::CSharp, 3);
    if (TextContains(lower, L"[serializable]") || TextContains(lower, L"[flags]")) scores.Add(Language::CSharp, 3);
    if (TextContains(lower, L"#region") || TextContains(lower, L"#endregion")) scores.Add(Language::CSharp, 4);
    if (TextContains(lower, L"string[] args") || TextContains(lower, L"static void main"))
        scores.Add(Language::CSharp, 3);
}

void ScoreGo(const std::wstring& lower, const std::wstring& first, LanguageScores& scores) {
    if (TextStarts(first, L"package ")) scores.Add(Language::Go, 5);
    if (TextStarts(first, L"package main")) scores.Add(Language::Go, 4);
    if (ContainsWord(lower, L"func")) scores.Add(Language::Go, 4);
    if (TextContains(lower, L"func main(")) scores.Add(Language::Go, 7);
    if (TextContains(lower, L":=")) scores.Add(Language::Go, 5);
    if (TextContains(lower, L"fmt.")) scores.Add(Language::Go, 6);
    if (TextContains(lower, L"import (") || TextContains(lower, L"go func") ||
        ContainsWord(lower, L"defer") || ContainsWord(lower, L"chan")) scores.Add(Language::Go, 4);
}

void ScoreRust(const std::wstring& lower, LanguageScores& scores) {
    if (TextContains(lower, L"fn main(")) scores.Add(Language::Rust, 8);
    if (ContainsWord(lower, L"fn")) scores.Add(Language::Rust, 2);
    if (TextContains(lower, L"let mut ")) scores.Add(Language::Rust, 6);
    if (TextContains(lower, L"use std::")) scores.Add(Language::Rust, 6);
    if (TextContains(lower, L"#[derive") || TextContains(lower, L"#![")) scores.Add(Language::Rust, 5);
    if (TextContains(lower, L".unwrap()") || TextContains(lower, L"impl ")) scores.Add(Language::Rust, 4);
    if (ContainsWord(lower, L"pub") && ContainsWord(lower, L"fn")) scores.Add(Language::Rust, 4);
    if (TextContains(lower, L"->")) scores.Add(Language::Rust, 2);
}

void ScoreMarkup(const std::wstring& lower, LanguageScores& scores) {
    if (TextContains(lower, L"<!doctype html") || TextContains(lower, L"<html")) scores.Add(Language::Html, 9);
    if (TextContains(lower, L"<head") || TextContains(lower, L"<body") ||
        TextContains(lower, L"<div") || TextContains(lower, L"<script") ||
        TextContains(lower, L"<meta")) scores.Add(Language::Html, 4);
    if (TextContains(lower, L"<?xml")) scores.Add(Language::Xml, 10);
    if (TextContains(lower, L"<!doctype") && !TextContains(lower, L"html")) scores.Add(Language::Xml, 4);
    if (TextContains(lower, L"<svg") || TextContains(lower, L"xmlns=")) scores.Add(Language::Xml, 5);
    if (TextContains(lower, L"<!--")) scores.Add(Language::Html, 1);
}

void ScoreCss(const std::wstring& lower, const DetectionSignals& signals, LanguageScores& scores) {
    if (TextContains(lower, L"@media") || TextContains(lower, L"@import url")) scores.Add(Language::Css, 5);
    if (TextContains(lower, L"color:") || TextContains(lower, L"background:") ||
        TextContains(lower, L"margin:") || TextContains(lower, L"padding:") ||
        TextContains(lower, L"font-size:")) scores.Add(Language::Css, 4);
    if (signals.cssSelectors > 0 && signals.cssDeclarations > 0) scores.Add(Language::Css, 5);
}

void ScoreSql(const std::wstring& lower, LanguageScores& scores) {
    if (ContainsWord(lower, L"select") && ContainsWord(lower, L"from")) scores.Add(Language::Sql, 7);
    if (TextContains(lower, L"insert into") || TextContains(lower, L"create table") ||
        TextContains(lower, L"drop table") || TextContains(lower, L"alter table"))
        scores.Add(Language::Sql, 7);
    if (ContainsWord(lower, L"where") || ContainsWord(lower, L"join") ||
        (ContainsWord(lower, L"group") && ContainsWord(lower, L"by"))) scores.Add(Language::Sql, 2);
    if (TextContains(lower, L";") && ContainsWord(lower, L"select")) scores.Add(Language::Sql, 2);
}

void ScoreYaml(const std::wstring& lower, const std::wstring& first,
               const std::vector<std::wstring>& lines, const DetectionSignals& signals,
               LanguageScores& scores) {
    if (signals.mappingLines >= 2) scores.Add(Language::Yaml, 6);
    else if (signals.mappingLines == 1) scores.Add(Language::Yaml, 4);
    if (TextStarts(first, L"---") || TextStarts(first, L"%yaml")) scores.Add(Language::Yaml, 6);
    if (!lines.empty()) {
        std::wstring trimmed = ToLower(Trim(lines.front()));
        if (IsMappingLine(trimmed)) scores.Add(Language::Yaml, 2);
    }
    if (TextContains(lower, L"\n  - ") || TextStarts(first, L"- ")) scores.Add(Language::Yaml, 1);
}

void ScoreJson(const std::wstring& documentStart, LanguageScores& scores) {
    if ((TextStarts(documentStart, L"{") || TextStarts(documentStart, L"[")) &&
        TextContains(documentStart, L"\"") && TextContains(documentStart, L":")) {
        scores.Add(Language::Json, 8);
    }
    if (TextStarts(documentStart, L"{") && TextContains(documentStart, L"\":")) scores.Add(Language::Json, 4);
}

void ScoreMarkdown(const std::wstring& lower, const DetectionSignals& signals, LanguageScores& scores) {
    if (signals.markdownHeadings > 0)
        scores.Add(Language::Markdown, 3 + 2 * std::min(signals.markdownHeadings, 4));
    if (signals.markdownBullets > 0) scores.Add(Language::Markdown, 2);
    if (TextContains(lower, L"**") && TextContains(lower, L"](")) scores.Add(Language::Markdown, 3);
    if (TextContains(lower, L"```")) scores.Add(Language::Markdown, 4);
}

void ScoreBatch(const std::wstring& lower, const std::wstring& first, LanguageScores& scores) {
    if (TextContains(lower, L"@echo off")) scores.Add(Language::Batch, 9);
    if (TextStarts(first, L"echo ") || TextContains(lower, L"\necho ")) scores.Add(Language::Batch, 3);
    if (TextContains(lower, L"set /p") || TextContains(lower, L"%~dp0") ||
        TextContains(lower, L"if exist") || TextContains(lower, L"goto :")) scores.Add(Language::Batch, 5);
    if (ContainsWord(lower, L"rem") && TextContains(lower, L"@")) scores.Add(Language::Batch, 2);
}

void ScoreShell(const std::wstring& lower, const std::wstring& first, LanguageScores& scores) {
    if (TextContains(lower, L"write-host") || TextContains(lower, L"get-childitem") ||
        TextContains(lower, L"set-executionpolicy") || TextContains(lower, L"import-module") ||
        TextContains(lower, L"new-item")) scores.Add(Language::PowerShell, 7);
    if (TextStarts(first, L"param(") || TextContains(lower, L"\nparam(")) scores.Add(Language::PowerShell, 5);
    if (TextStarts(first, L"#requires") || TextContains(lower, L"\n#requires"))
        scores.Add(Language::PowerShell, 6);
    if (ContainsDollarAssignment(lower)) scores.Add(Language::PowerShell, 4);
    if (TextStarts(first, L"$")) scores.Add(Language::PowerShell, 2);
    if (TextContains(lower, L"-eq") || TextContains(lower, L"| where-object") ||
        TextContains(lower, L"foreach (")) scores.Add(Language::PowerShell, 3);
}

void ScorePhp(const std::wstring& lower, const std::wstring& first, LanguageScores& scores) {
    if (TextContains(lower, L"<?php") || TextStarts(first, L"<?php")) scores.Add(Language::Php, 12);
    if (TextContains(lower, L"<?=")) scores.Add(Language::Php, 6);
    if (ContainsWord(lower, L"namespace") && TextContains(lower, L";")) scores.Add(Language::Php, 2);
    if (TextContains(lower, L"$this->") || TextContains(lower, L"->")) scores.Add(Language::Php, 5);
    if (ContainsWord(lower, L"function") && TextContains(lower, L"$")) scores.Add(Language::Php, 6);
    if (TextContains(lower, L"echo ") || TextContains(lower, L"var_dump(")) scores.Add(Language::Php, 3);
    if (TextContains(lower, L"include_once") || TextContains(lower, L"require_once")) scores.Add(Language::Php, 4);
}

void ScoreRuby(const std::wstring& lower, const std::wstring& first, LanguageScores& scores) {
    if (TextStarts(first, L"#!/usr/bin/env ruby") || TextStarts(first, L"#!/bin/ruby")) scores.Add(Language::Ruby, 14);
    if (ContainsWord(lower, L"require") && TextContains(lower, L"'")) scores.Add(Language::Ruby, 4);
    if (TextContains(lower, L"puts ") || ContainsWord(lower, L"do")) scores.Add(Language::Ruby, 4);
    if (ContainsWord(lower, L"def") && TextContains(lower, L"end")) scores.Add(Language::Ruby, 6);
    if (ContainsWord(lower, L"class") && ContainsWord(lower, L"end")) scores.Add(Language::Ruby, 3);
    if (TextContains(lower, L"attr_accessor") || TextContains(lower, L"attr_reader")) scores.Add(Language::Ruby, 5);
    if (TextContains(lower, L"@") && TextContains(lower, L"def ")) scores.Add(Language::Ruby, 2);
}

void ScoreSwift(const std::wstring& lower, const std::wstring& first, LanguageScores& scores) {
    if (ContainsWord(lower, L"import") && TextContains(lower, L"Foundation")) scores.Add(Language::Swift, 8);
    if (ContainsWord(lower, L"func") && TextContains(lower, L"->")) scores.Add(Language::Swift, 5);
    if (ContainsWord(lower, L"let") || ContainsWord(lower, L"var")) scores.Add(Language::Swift, 2);
    if (ContainsWord(lower, L"struct") || ContainsWord(lower, L"class")) scores.Add(Language::Swift, 2);
    if (TextContains(lower, L"guard let") || TextContains(lower, L"if let")) scores.Add(Language::Swift, 6);
    if (ContainsWord(lower, L"protocol") || ContainsWord(lower, L"extension")) scores.Add(Language::Swift, 4);
    if (TextContains(lower, L"print(") && ContainsWord(lower, L"import")) scores.Add(Language::Swift, 3);
}

void ScoreKotlin(const std::wstring& lower, const std::wstring& first, LanguageScores& scores) {
    if (ContainsWord(lower, L"package") && TextContains(lower, L"import ")) scores.Add(Language::Kotlin, 4);
    if (ContainsWord(lower, L"fun") && TextContains(lower, L":")) scores.Add(Language::Kotlin, 6);
    if (TextContains(lower, L"val ") || TextContains(lower, L"var ")) scores.Add(Language::Kotlin, 3);
    if (ContainsWord(lower, L"data class") || TextContains(lower, L"data class")) scores.Add(Language::Kotlin, 8);
    if (ContainsWord(lower, L"when") && ContainsWord(lower, L"->")) scores.Add(Language::Kotlin, 4);
    if (TextContains(lower, L"println(") || TextContains(lower, L"null")) scores.Add(Language::Kotlin, 2);
    if (ContainsWord(lower, L"companion object") || TextContains(lower, L"companion object")) scores.Add(Language::Kotlin, 6);
}

} // namespace

Language DetectLanguageFromContent(const std::wstring& text) {
    std::wstring prefix = text;
    if (prefix.size() > kDetectionBudget) prefix.resize(kDetectionBudget);

    // Normalize line endings and BOM before any heuristic runs.
    std::wstring cleaned;
    cleaned.reserve(prefix.size());
    for (wchar_t c : prefix) {
        if (c != L'\r' && c != 0xFEFF) cleaned.push_back(c);
    }
    const std::wstring lower = ToLower(cleaned);
    const std::wstring documentStart = Trim(cleaned);

    std::vector<std::wstring> lines;
    std::wistringstream stream(cleaned);
    std::wstring line;
    while (std::getline(stream, line)) lines.push_back(line);

    std::wstring first;
    for (const std::wstring& candidate : lines) {
        std::wstring trimmed = Trim(candidate);
        if (!trimmed.empty()) {
            first = ToLower(trimmed);
            break;
        }
    }

    const DetectionSignals signals(lines);
    LanguageScores scores;

    ScoreShebang(first, scores);
    ScoreCMake(lower, first, scores);
    ScoreCAndCpp(lower, scores);
    ScorePython(lower, first, lines, scores);
    ScoreJavaScriptAndTypeScript(lower, first, scores);
    ScoreJava(lower, first, scores);
    ScoreCSharp(lower, scores);
    ScoreGo(lower, first, scores);
    ScoreRust(lower, scores);
    ScoreMarkup(lower, scores);
    ScoreCss(lower, signals, scores);
    ScoreSql(lower, scores);
    ScoreYaml(lower, first, lines, signals, scores);
    ScoreJson(documentStart, scores);
    ScoreMarkdown(lower, signals, scores);
    ScoreBatch(lower, first, scores);
    ScoreShell(lower, first, scores);
    ScorePhp(lower, first, scores);
    ScoreRuby(lower, first, scores);
    ScoreSwift(lower, first, scores);
    ScoreKotlin(lower, first, scores);

    return scores.Best(kMinimumDetectionScore);
}

void HighlightLine(const std::wstring& text, Language language, LexState stateIn,
                   std::vector<Token>& tokens, LexState& stateOut) {
    tokens.clear();
    stateOut = stateIn;
    if (text.empty()) return;
    switch (language) {
        case Language::Cpp:
        case Language::C:
        case Language::JavaScript:
        case Language::TypeScript:
        case Language::Java:
        case Language::CSharp:
        case Language::Go:
        case Language::Rust:
        case Language::Swift:
        case Language::Kotlin:
            HighlightCLike(text, language, stateOut, tokens, false);
            break;
        case Language::CMake:
            HighlightCLike(text, language, stateOut, tokens, true);
            break;
        case Language::Python:
            HighlightPython(text, tokens);
            break;
        case Language::Json:
            HighlightJson(text, tokens);
            break;
        case Language::Markdown:
            HighlightMarkdown(text, tokens);
            break;
        case Language::Batch:
            HighlightCLike(text, language, stateOut, tokens, false);
            break;
        case Language::PowerShell:
        case Language::Ruby:
        case Language::Php:
            HighlightCLike(text, language, stateOut, tokens, true);
            break;
        case Language::Html:
        case Language::Xml:
            HighlightMarkup(text, stateOut, tokens);
            break;
        case Language::Css:
            HighlightCss(text, stateOut, tokens);
            break;
        case Language::Sql:
            HighlightSql(text, stateOut, tokens);
            break;
        case Language::Yaml:
            HighlightYaml(text, tokens);
            break;
        default:
            break;
    }
}

namespace {

bool IsTokenizedAs(const std::vector<Token>& tokens, int position, TokenKind kind) {
    for (const Token& token : tokens) {
        if (position >= token.start && position < token.start + token.len) {
            return token.kind == kind;
        }
    }
    return false;
}

void AddUniqueSymbol(std::vector<SyntaxSymbol>& symbols, const std::wstring& name,
                     const std::wstring& kind, int line) {
    if (name.empty()) return;
    for (const SyntaxSymbol& symbol : symbols) {
        if (IEquals(symbol.name, name)) return;
    }
    symbols.push_back(SyntaxSymbol{name, kind, line});
}

std::wstring WordAt(const std::wstring& text, int& position) {
    while (position < (int)text.size() && !IsIdentStart(text[(size_t)position])) ++position;
    int start = position;
    while (position < (int)text.size() && IsIdentChar(text[(size_t)position])) ++position;
    return text.substr((size_t)start, (size_t)(position - start));
}

} // namespace

std::vector<SyntaxSymbol> ParseDocumentSymbols(const std::wstring& text, Language language) {
    std::vector<SyntaxSymbol> symbols;
    std::vector<std::wstring> declarationWords = {
        L"class", L"struct", L"interface", L"enum", L"trait", L"namespace",
        L"def", L"function", L"func", L"fn", L"type"
    };
    std::wistringstream stream(text);
    std::wstring line;
    int lineNumber = 0;
    LexState state = LexState::Normal;
    while (std::getline(stream, line)) {
        if (language == Language::Html || language == Language::Xml) {
            std::vector<Token> tokens;
            LexState stateOut = state;
            HighlightLine(line, language, state, tokens, stateOut);
            state = stateOut;
            size_t pos = 0;
            while ((pos = line.find(L'<', pos)) != std::wstring::npos) {
                if (IsTokenizedAs(tokens, (int)pos, TokenKind::Comment)) {
                    ++pos;
                    continue;
                }
                size_t name = pos + 1;
                if (name < line.size() && line[name] == L'/') { pos = name + 1; continue; }
                while (name < line.size() && iswspace(line[name])) ++name;
                size_t end = name;
                while (end < line.size() &&
                       (IsIdentChar(line[end]) || line[end] == L'-' || line[end] == L':')) ++end;
                if (end > name) AddUniqueSymbol(symbols, line.substr(name, end - name), L"tag", lineNumber);
                pos = std::max(end, pos + 1);
            }
            ++lineNumber;
            continue;
        }

        std::vector<Token> tokens;
        LexState stateOut = state;
        HighlightLine(line, language, state, tokens, stateOut);
        state = stateOut;
        for (size_t i = 0; i < declarationWords.size(); ++i) {
            const std::wstring& keyword = declarationWords[i];
            size_t pos = 0;
            while ((pos = line.find(keyword, pos)) != std::wstring::npos) {
                bool leftBoundary = pos == 0 || !IsIdentChar(line[pos - 1]);
                size_t after = pos + keyword.size();
                bool rightBoundary = after >= line.size() || !IsIdentChar(line[after]);
                if (!leftBoundary || !rightBoundary ||
                    IsTokenizedAs(tokens, (int)pos, TokenKind::Comment) ||
                    IsTokenizedAs(tokens, (int)pos, TokenKind::String)) {
                    pos = after;
                    continue;
                }
                int namePos = (int)after;
                while (namePos < (int)line.size() &&
                       (iswspace(line[(size_t)namePos]) || line[(size_t)namePos] == L'*' ||
                        line[(size_t)namePos] == L'&')) ++namePos;
                if (keyword == L"func" && namePos < (int)line.size() &&
                    line[(size_t)namePos] == L'(') {
                    size_t close = line.find(L')', (size_t)namePos);
                    if (close != std::wstring::npos) namePos = (int)close + 1;
                    while (namePos < (int)line.size() && iswspace(line[(size_t)namePos])) ++namePos;
                }
                std::wstring name = WordAt(line, namePos);
                if (!name.empty()) AddUniqueSymbol(symbols, name, keyword, lineNumber);
                pos = after;
            }
        }
        ++lineNumber;
    }
    return symbols;
}

std::vector<std::wstring> BuildCompletions(const std::wstring& text, Language language,
                                           const std::wstring& prefix) {
    std::vector<std::wstring> result;
    if (prefix.empty()) return result;
    auto add = [&](const std::wstring& value) {
        if (value.size() < prefix.size() || !IStartsWith(value, prefix) ||
            IEquals(value, prefix)) return;
        for (const std::wstring& existing : result) {
            if (IEquals(existing, value)) return;
        }
        if (result.size() < 256) result.push_back(value);
    };

    // Built-in language data comes from the embedded RCDATA resource, never
    // from hard-coded tables in this translation unit.
    for (const std::wstring& keyword : CompletionData::ForLanguage(language)) add(keyword);
    // Fall back to the legacy keyword table only when the resource has no
    // section for this language (e.g. Plain) so completions still work.
    if (CompletionData::ForLanguage(language).empty()) {
        for (const std::wstring& keyword : KeywordsFor(language)) add(keyword);
    }
    for (const SyntaxSymbol& symbol : ParseDocumentSymbols(text, language)) add(symbol.name);

    std::wistringstream stream(text);
    std::wstring line;
    LexState state = LexState::Normal;
    while (std::getline(stream, line) && result.size() < 256) {
        std::vector<Token> tokens;
        LexState nextState = state;
        HighlightLine(line, language, state, tokens, nextState);
        state = nextState;
        int i = 0;
        while (i < (int)line.size() && result.size() < 256) {
            if (!IsIdentStart(line[(size_t)i])) { ++i; continue; }
            int start = i;
            while (i < (int)line.size() && IsIdentChar(line[(size_t)i])) ++i;
            if (IsTokenizedAs(tokens, start, TokenKind::Comment) ||
                IsTokenizedAs(tokens, start, TokenKind::String)) continue;
            add(line.substr((size_t)start, (size_t)(i - start)));
        }
    }
    std::stable_sort(result.begin(), result.end(), [](const std::wstring& a, const std::wstring& b) {
        return ToLower(a) < ToLower(b);
    });
    if (result.size() > 12) result.resize(12);
    return result;
}
