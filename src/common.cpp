#include "common.h"

#include <cctype>
#include <cerrno>

std::wstring Utf8ToWide(std::string_view s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), (int)s.size(), nullptr, 0);
    UINT cp = CP_UTF8;
    DWORD flags = MB_ERR_INVALID_CHARS;
    if (len <= 0) {
        cp = CP_ACP;
        flags = 0;
        len = MultiByteToWideChar(cp, flags, s.data(), (int)s.size(), nullptr, 0);
    }
    if (len <= 0) return {};
    std::wstring out((size_t)len, L'\0');
    MultiByteToWideChar(cp, flags, s.data(), (int)s.size(), out.data(), len);
    return out;
}

std::string WideToUtf8(std::wstring_view s) {
    if (s.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string out((size_t)len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), len, nullptr, nullptr);
    return out;
}

std::wstring Trim(const std::wstring& s) {
    size_t a = 0;
    size_t b = s.size();
    while (a < b && iswspace(s[a])) ++a;
    while (b > a && iswspace(s[b - 1])) --b;
    return s.substr(a, b - a);
}

std::wstring ToLower(std::wstring s) {
    for (auto& c : s) c = (wchar_t)towlower(c);
    return s;
}

std::wstring ToUpper(std::wstring s) {
    for (auto& c : s) c = (wchar_t)towupper(c);
    return s;
}

bool IEquals(std::wstring_view a, std::wstring_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (towlower(a[i]) != towlower(b[i])) return false;
    }
    return true;
}

bool IStartsWith(std::wstring_view s, std::wstring_view prefix) {
    if (prefix.size() > s.size()) return false;
    return IEquals(s.substr(0, prefix.size()), prefix);
}

bool IEndsWith(std::wstring_view s, std::wstring_view suffix) {
    if (suffix.size() > s.size()) return false;
    return IEquals(s.substr(s.size() - suffix.size()), suffix);
}

bool IContains(std::wstring_view s, std::wstring_view needle) {
    if (needle.empty()) return true;
    if (needle.size() > s.size()) return false;
    const size_t n = s.size() - needle.size();
    for (size_t i = 0; i <= n; ++i) {
        if (IEquals(s.substr(i, needle.size()), needle)) return true;
    }
    return false;
}

bool IsWordChar(wchar_t c) {
    return iswalnum(c) || c == L'_';
}

int WordStart(const std::wstring& s, int pos) {
    pos = std::clamp(pos, 0, (int)s.size());
    while (pos > 0 && IsWordChar(s[pos - 1])) --pos;
    return pos;
}

int WordEnd(const std::wstring& s, int pos) {
    pos = std::clamp(pos, 0, (int)s.size());
    while (pos < (int)s.size() && IsWordChar(s[pos])) ++pos;
    return pos;
}

std::wstring QuoteArg(const std::wstring& arg) {
    if (!arg.empty() && arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
        return arg;
    }
    std::wstring out;
    out.push_back(L'"');
    int backslashes = 0;
    for (wchar_t c : arg) {
        if (c == L'\\') {
            ++backslashes;
        } else if (c == L'"') {
            out.append((size_t)backslashes * 2 + 1, L'\\');
            out.push_back(L'"');
            backslashes = 0;
        } else {
            out.append((size_t)backslashes, L'\\');
            backslashes = 0;
            out.push_back(c);
        }
    }
    out.append((size_t)backslashes * 2, L'\\');
    out.push_back(L'"');
    return out;
}

std::wstring BaseName(const std::wstring& path) {
    try {
        auto p = std::filesystem::path(path);
        return p.filename().wstring();
    } catch (...) {
        return path;
    }
}

std::wstring DirName(const std::wstring& path) {
    try {
        auto p = std::filesystem::path(path);
        return p.parent_path().wstring();
    } catch (...) {
        return {};
    }
}

std::wstring NormalizePath(const std::wstring& path) {
    if (path.empty()) return {};
    std::error_code ec;
    auto p = std::filesystem::weakly_canonical(std::filesystem::path(path), ec);
    if (ec) {
        p = std::filesystem::absolute(std::filesystem::path(path), ec);
        if (ec) return path;
    }
    return p.wstring();
}

bool SamePath(const std::wstring& a, const std::wstring& b) {
    return IEquals(NormalizePath(a), NormalizePath(b));
}

std::wstring FormatDuration(double seconds) {
    wchar_t buf[64];
    if (seconds < 1.0) {
        swprintf_s(buf, L"%d ms", (int)std::lround(seconds * 1000.0));
    } else if (seconds < 60.0) {
        swprintf_s(buf, L"%.2f s", seconds);
    } else {
        swprintf_s(buf, L"%.0f m %.0f s", std::floor(seconds / 60.0), std::fmod(seconds, 60.0));
    }
    return buf;
}

std::wstring FormatCount(size_t n) {
    std::wstring s = std::to_wstring(n);
    int insert = (int)s.size() - 3;
    while (insert > 0) {
        s.insert((size_t)insert, 1, L',');
        insert -= 3;
    }
    return s;
}

std::wstring ClipboardText(HWND hwnd) {
    if (!OpenClipboard(hwnd)) return {};
    std::wstring result;
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (h) {
        auto* p = static_cast<const wchar_t*>(GlobalLock(h));
        if (p) {
            result = p;
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
    std::wstring normalized;
    normalized.reserve(result.size());
    for (size_t i = 0; i < result.size(); ++i) {
        wchar_t c = result[i];
        if (c == L'\r') {
            if (i + 1 < result.size() && result[i + 1] == L'\n') continue;
            normalized.push_back(L'\n');
        } else {
            normalized.push_back(c);
        }
    }
    return normalized;
}

void SetClipboardText(HWND hwnd, const std::wstring& text) {
    if (!OpenClipboard(hwnd)) return;
    EmptyClipboard();
    size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (mem) {
        void* p = GlobalLock(mem);
        if (p) {
            memcpy(p, text.c_str(), bytes);
            GlobalUnlock(mem);
            if (!SetClipboardData(CF_UNICODETEXT, mem)) {
                GlobalFree(mem);
            }
        } else {
            GlobalFree(mem);
        }
    }
    CloseClipboard();
}

bool SaveBinaryFile(const std::wstring& path, const std::string& bytes) {
    std::ofstream f(std::filesystem::path(path), std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(bytes.data(), (std::streamsize)bytes.size());
    return f.good();
}

bool ReadBinaryFile(const std::wstring& path, std::string& bytes) {
    std::ifstream f(std::filesystem::path(path), std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end);
    auto size = f.tellg();
    if (size < 0) return false;
    f.seekg(0, std::ios::beg);
    bytes.resize((size_t)size);
    if (size > 0) f.read(bytes.data(), size);
    return f.good() || f.eof();
}

void RevealInExplorer(const std::wstring& path) {
    std::wstring args = L"/select," + QuoteArg(path);
    ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
}

std::wstring AppDataDirectory() {
    PWSTR raw = nullptr;
    std::wstring result;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_CREATE, nullptr, &raw))) {
        result = raw;
        CoTaskMemFree(raw);
    } else {
        wchar_t buf[MAX_PATH]{};
        DWORD n = GetEnvironmentVariableW(L"APPDATA", buf, MAX_PATH);
        if (n > 0 && n < MAX_PATH) result = buf;
    }
    if (result.empty()) return L".";
    result += L"\\NeoIDEs";
    std::error_code ec;
    std::filesystem::create_directories(result, ec);
    return result;
}

ThemeColors MakeDarkTheme() {
    ThemeColors t{};
    t.windowBg = Col(0x101116);
    t.titleBar = Col(0x14151B);
    t.railBg = Col(0x14151B);
    t.sidebarBg = Col(0x17181F);
    t.editorBg = Col(0x1B1C24);
    t.tabBarBg = Col(0x17181F);
    t.tabActiveBg = Col(0x1B1C24);
    t.panelBg = Col(0x16171D);
    t.statusBg = Col(0x121318);
    t.gutterBg = Col(0x191A21);

    t.border = Col(0x2A2C36);
    t.borderSoft = Col(0x23252E);
    t.text = Col(0xD7DAE2);
    t.textDim = Col(0x9499A8);
    t.textFaint = Col(0x666C7C);
    t.accent = Col(0x8B7CFF);
    t.accentHover = Col(0xA79BFF);
    t.accentSoft = Col(0x8B7CFF, 0.14f);
    t.selection = Col(0x8B7CFF, 0.28f);
    t.lineHighlight = Col(0xFFFFFF, 0.018f);
    t.lineNumber = Col(0x5C6272);
    t.lineNumberActive = Col(0xC8CCD8);
    t.indentGuide = Col(0x2C2E3A);
    t.scrollbar = Col(0x4B4F60, 0.70f);
    t.scrollbarHover = Col(0x666B80, 0.90f);
    t.hover = Col(0x22242D);
    t.pressed = Col(0x2B2E3A);
    t.inputBg = Col(0x111218);
    t.overlay = Col(0x05060A, 0.55f);
    t.shadow = Col(0x000000, 0.45f);

    t.error = Col(0xFF6B7A);
    t.warning = Col(0xF2C46D);
    t.success = Col(0x63D8A3);
    t.info = Col(0x76B7FF);
    t.findMatch = Col(0xF2C46D, 0.22f);

    t.synKeyword = Col(0xC792EA);
    t.synType = Col(0x82AAFF);
    t.synString = Col(0xC3E88D);
    t.synNumber = Col(0xF78C6C);
    t.synComment = Col(0x6B7280);
    t.synPreproc = Col(0xFFCB6B);
    t.synFunction = Col(0x8BD5FF);
    t.synMacro = Col(0xF07178);
    t.synBuiltin = Col(0x89DDFF);
    t.synProperty = Col(0x89DDFF);
    return t;
}

ThemeColors MakeLightTheme() {
    ThemeColors t{};
    t.windowBg = Col(0xEEF0F5);
    t.titleBar = Col(0xEDEFF4);
    t.railBg = Col(0xEDEFF4);
    t.sidebarBg = Col(0xF4F5F9);
    t.editorBg = Col(0xFCFCFE);
    t.tabBarBg = Col(0xF1F2F7);
    t.tabActiveBg = Col(0xFCFCFE);
    t.panelBg = Col(0xF7F8FB);
    t.statusBg = Col(0xEDEFF4);
    t.gutterBg = Col(0xF8F9FC);

    t.border = Col(0xD8DBE4);
    t.borderSoft = Col(0xE4E6ED);
    t.text = Col(0x262932);
    t.textDim = Col(0x626978);
    t.textFaint = Col(0x969CAA);
    t.accent = Col(0x6B5BEA);
    t.accentHover = Col(0x5747D6);
    t.accentSoft = Col(0x6B5BEA, 0.12f);
    t.selection = Col(0x6B5BEA, 0.20f);
    t.lineHighlight = Col(0x000000, 0.02f);
    t.lineNumber = Col(0x9AA0AE);
    t.lineNumberActive = Col(0x3A3F4C);
    t.indentGuide = Col(0xE1E3EA);
    t.scrollbar = Col(0xB9BECB, 0.85f);
    t.scrollbarHover = Col(0x999FB0, 0.95f);
    t.hover = Col(0xE7E9F0);
    t.pressed = Col(0xDDDFE9);
    t.inputBg = Col(0xFFFFFF);
    t.overlay = Col(0x1B1D26, 0.28f);
    t.shadow = Col(0x1B1D26, 0.22f);

    t.error = Col(0xD93A4E);
    t.warning = Col(0xA96B00);
    t.success = Col(0x12805C);
    t.info = Col(0x1F6FD1);
    t.findMatch = Col(0xF2B33D, 0.32f);

    t.synKeyword = Col(0x8E44C9);
    t.synType = Col(0x2464C4);
    t.synString = Col(0x257A3E);
    t.synNumber = Col(0xB75A16);
    t.synComment = Col(0x8A909E);
    t.synPreproc = Col(0x9C6500);
    t.synFunction = Col(0x0B6E99);
    t.synMacro = Col(0xB03A48);
    t.synBuiltin = Col(0x006D8F);
    t.synProperty = Col(0x0B6E99);
    return t;
}
