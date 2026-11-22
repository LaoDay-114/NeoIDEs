#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

// Keep these as the single source of truth: build scripts may also pass them
// with -D, so guard against redefinition. WIN32_LEAN_AND_MEAN trims rarely used
// Win32 APIs from windows.h; NOMINMAX keeps its min/max macros from clashing
// with std::min/std::max used throughout the codebase.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#ifndef NOMINMAX
#define NOMINMAX 1
#endif

#include <windows.h>
#include <windowsx.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <knownfolders.h>
#include <commdlg.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cwctype>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
#endif
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif

using Rect = D2D1_RECT_F;
using Color = D2D1_COLOR_F;

inline Color Col(uint32_t rgb, float alpha = 1.0f) {
    return Color{
        static_cast<float>((rgb >> 16) & 0xFF) / 255.0f,
        static_cast<float>((rgb >> 8) & 0xFF) / 255.0f,
        static_cast<float>(rgb & 0xFF) / 255.0f,
        alpha
    };
}

inline Color WithAlpha(Color c, float alpha) {
    c.a = alpha;
    return c;
}

inline Rect MakeRect(float l, float t, float r, float b) {
    return Rect{l, t, r, b};
}

inline float RectWidth(const Rect& r) {
    return std::max(0.0f, r.right - r.left);
}

inline float RectHeight(const Rect& r) {
    return std::max(0.0f, r.bottom - r.top);
}

inline bool Contains(const Rect& r, float x, float y) {
    return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
}

inline Rect Inset(const Rect& r, float d) {
    return Rect{r.left + d, r.top + d, r.right - d, r.bottom - d};
}

inline Rect OffsetRect(const Rect& r, float dx, float dy) {
    return Rect{r.left + dx, r.top + dy, r.right + dx, r.bottom + dy};
}

inline Rect FromWinRect(const RECT& r) {
    return Rect{(float)r.left, (float)r.top, (float)r.right, (float)r.bottom};
}

struct ThemeColors {
    Color windowBg;
    Color titleBar;
    Color railBg;
    Color sidebarBg;
    Color editorBg;
    Color tabBarBg;
    Color tabActiveBg;
    Color panelBg;
    Color statusBg;
    Color gutterBg;

    Color border;
    Color borderSoft;
    Color text;
    Color textDim;
    Color textFaint;
    Color accent;
    Color accentHover;
    Color accentSoft;
    Color selection;
    Color lineHighlight;
    Color lineNumber;
    Color lineNumberActive;
    Color indentGuide;
    Color scrollbar;
    Color scrollbarHover;
    Color hover;
    Color pressed;
    Color inputBg;
    Color overlay;
    Color shadow;

    Color error;
    Color warning;
    Color success;
    Color info;
    Color findMatch;

    Color synKeyword;
    Color synType;
    Color synString;
    Color synNumber;
    Color synComment;
    Color synPreproc;
    Color synFunction;
    Color synMacro;
    Color synBuiltin;
    Color synProperty;
};

ThemeColors MakeDarkTheme();
ThemeColors MakeLightTheme();

struct TextStyle {
    float size = 13.0f;
    int weight = 400;
    bool italic = false;
    int hAlign = 0;   // 0 = leading, 1 = center, 2 = trailing
    int vAlign = 0;   // 0 = top, 1 = center, 2 = bottom
    bool wrap = false;
    bool ellipsis = true;
    bool uiText = true;
    std::wstring family;  // empty = renderer's UI font
};

// ---------------------------------------------------------------- text utils

inline bool IsHighSurrogate(wchar_t c) { return c >= 0xD800 && c <= 0xDBFF; }
inline bool IsLowSurrogate(wchar_t c) { return c >= 0xDC00 && c <= 0xDFFF; }

inline int TextCellWidth(wchar_t c) {
    if (IsLowSurrogate(c)) return 0;
    if (IsHighSurrogate(c)) return 2;
    if ((c >= 0x0300 && c <= 0x036F) || (c >= 0x1AB0 && c <= 0x1AFF) ||
        (c >= 0x1DC0 && c <= 0x1DFF) || (c >= 0x20D0 && c <= 0x20FF) ||
        (c >= 0xFE20 && c <= 0xFE2F)) return 0;
    if (c >= 0x1100 && c <= 0x115F) return 2;
    if (c == 0x2329 || c == 0x232A || (c >= 0x2E80 && c <= 0xA4CF && c != 0x303F) ||
        (c >= 0xAC00 && c <= 0xD7A3) || (c >= 0xF900 && c <= 0xFAFF) ||
        (c >= 0xFE10 && c <= 0xFE19) || (c >= 0xFE30 && c <= 0xFE6F) ||
        (c >= 0xFF00 && c <= 0xFF60) || (c >= 0xFFE0 && c <= 0xFFE6)) return 2;
    return 1;
}

std::wstring Utf8ToWide(std::string_view s);
std::string WideToUtf8(std::wstring_view s);
std::wstring Trim(const std::wstring& s);
std::wstring ToLower(std::wstring s);
std::wstring ToUpper(std::wstring s);
bool IEquals(std::wstring_view a, std::wstring_view b);
bool IStartsWith(std::wstring_view s, std::wstring_view prefix);
bool IEndsWith(std::wstring_view s, std::wstring_view suffix);
bool IContains(std::wstring_view s, std::wstring_view needle);
bool IsWordChar(wchar_t c);
int WordStart(const std::wstring& s, int pos);
int WordEnd(const std::wstring& s, int pos);
std::wstring QuoteArg(const std::wstring& arg);
std::wstring BaseName(const std::wstring& path);
std::wstring DirName(const std::wstring& path);
std::wstring NormalizePath(const std::wstring& path);
bool SamePath(const std::wstring& a, const std::wstring& b);
std::wstring FormatDuration(double seconds);
std::wstring FormatCount(size_t n);

// ------------------------------------------------------------- clipboard etc.

std::wstring ClipboardText(HWND hwnd);
void SetClipboardText(HWND hwnd, const std::wstring& text);
bool SaveBinaryFile(const std::wstring& path, const std::string& bytes);
bool ReadBinaryFile(const std::wstring& path, std::string& bytes);
void RevealInExplorer(const std::wstring& path);
std::wstring AppDataDirectory();
