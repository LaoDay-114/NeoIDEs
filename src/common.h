#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

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
    std::wstring family;  // empty = renderer's UI font
};

// ---------------------------------------------------------------- text utils

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
