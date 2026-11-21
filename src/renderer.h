#pragma once

#include "common.h"

class Renderer {
public:
    Renderer() = default;
    ~Renderer() { Shutdown(); }

    bool Init(HWND hwnd);
    void Shutdown();

    void Resize(UINT width, UINT height);
    void BeginDraw();
    void EndDraw();
    bool IsReady() const { return target_ != nullptr; }

    void Clear(Color color);
    void FillRect(const Rect& rect, Color color);
    void FillRoundedRect(const Rect& rect, float radius, Color color);
    void StrokeRect(const Rect& rect, Color color, float width = 1.0f);
    void StrokeRoundedRect(const Rect& rect, float radius, Color color, float width = 1.0f);
    void DrawLine(float x1, float y1, float x2, float y2, Color color, float width = 1.0f);
    void DrawEllipse(float cx, float cy, float rx, float ry, Color color, bool filled);
    void FillTriangle(float x1, float y1, float x2, float y2, float x3, float y3, Color color);

    void PushClip(const Rect& rect);
    void PopClip();

    void Text(const std::wstring& text, const Rect& rect, Color color, const TextStyle& style);
    D2D1_SIZE_F Measure(const std::wstring& text, const TextStyle& style);
    IDWriteTextLayout* CreateLayout(const std::wstring& text, float maxWidth, float maxHeight,
                                    const TextStyle& style);
    void DrawLayout(IDWriteTextLayout* layout, float x, float y, Color color);
    IDWriteTextFormat* FormatFor(const TextStyle& style);

    ID2D1SolidColorBrush* Brush(Color color);
    IDWriteFactory* DWrite() const { return dwrite_; }
    ID2D1Factory* D2DFactory() const { return d2d_; }

    bool HasFontFamily(const wchar_t* name) const;
    void SetChinese(bool chinese) { chinese_ = chinese; }
    const std::wstring& UiFont() const { return uiFont_; }
    const std::wstring& MonoFont() const { return monoFont_; }
    const std::wstring& IconFont() const { return iconFont_; }

private:
    struct FormatKey {
        std::wstring family;
        float size = 0;
        int weight = 400;
        bool italic = false;
        int align = 0;
        bool wrap = false;

        bool operator<(const FormatKey& o) const;
    };

    bool CreateTarget();
    void DiscardTarget();
    void PickFonts();

    HWND hwnd_ = nullptr;
    ID2D1Factory* d2d_ = nullptr;
    IDWriteFactory* dwrite_ = nullptr;
    ID2D1HwndRenderTarget* target_ = nullptr;
    UINT width_ = 0;
    UINT height_ = 0;
    std::wstring uiFont_ = L"Segoe UI";
    std::wstring monoFont_ = L"Consolas";
    std::wstring iconFont_ = L"Segoe MDL2 Assets";
    bool chinese_ = false;
    std::unordered_map<uint32_t, ID2D1SolidColorBrush*> brushes_;
    std::map<FormatKey, IDWriteTextFormat*> formats_;
};
