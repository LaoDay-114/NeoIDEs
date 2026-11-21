#include "syntax.h"

#include <cctype>

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
    switch (language) {
        case Language::Java: return java;
        case Language::CSharp: return csharp;
        case Language::Go: return go;
        case Language::Rust: return rust;
        case Language::Sql: return sql;
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
                       lang == Language::Go || lang == Language::Rust) {
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
        default: return L"Plain Text";
    }
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

    for (const std::wstring& keyword : KeywordsFor(language)) add(keyword);
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
