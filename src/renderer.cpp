#include "renderer.h"
#include "localization.h"
#include "../resources/resource.h"

#include <cwchar>
#include <tuple>
#include <wincodec.h>

namespace {

struct LanguageVisual {
    const wchar_t* monogram;
    uint32_t color;
};

const LanguageVisual& VisualForLanguage(Language language) {
    static const LanguageVisual generic{L"\uE8A5", 0};
    switch (language) {
        case Language::Cpp: { static const LanguageVisual v{L"C++", 0x1F5C99}; return v; }
        case Language::C: { static const LanguageVisual v{L"C", 0x6C7A89}; return v; }
        case Language::CMake: { static const LanguageVisual v{L"CM", 0xC4453B}; return v; }
        case Language::Python: { static const LanguageVisual v{L"Py", 0x2E7D9E}; return v; }
        case Language::JavaScript: { static const LanguageVisual v{L"JS", 0xC8A417}; return v; }
        case Language::TypeScript: { static const LanguageVisual v{L"TS", 0x3178C6}; return v; }
        case Language::Java: { static const LanguageVisual v{L"J", 0xD96F00}; return v; }
        case Language::CSharp: { static const LanguageVisual v{L"C#", 0x8A4FA3}; return v; }
        case Language::Go: { static const LanguageVisual v{L"Go", 0x0099B0}; return v; }
        case Language::Rust: { static const LanguageVisual v{L"Rs", 0xB05A2A}; return v; }
        case Language::Html: { static const LanguageVisual v{L"<>", 0xD9542A}; return v; }
        case Language::Css: { static const LanguageVisual v{L"#", 0x2A6BC9}; return v; }
        case Language::Sql: { static const LanguageVisual v{L"DB", 0x3F7D4E}; return v; }
        case Language::Yaml: { static const LanguageVisual v{L"Y", 0x8B6FA8}; return v; }
        case Language::Xml: { static const LanguageVisual v{L"X", 0x9B59B6}; return v; }
        case Language::Json: { static const LanguageVisual v{L"{}", 0x6B7280}; return v; }
        case Language::Markdown: { static const LanguageVisual v{L"Md", 0x4C6EF5}; return v; }
        case Language::Batch: { static const LanguageVisual v{L"BAT", 0x5D6D7E}; return v; }
        case Language::PowerShell: { static const LanguageVisual v{L"PS", 0x2F7AD1}; return v; }
        case Language::Php: { static const LanguageVisual v{L"Ph", 0x777BB4}; return v; }
        case Language::Ruby: { static const LanguageVisual v{L"Rb", 0xCC342D}; return v; }
        case Language::Swift: { static const LanguageVisual v{L"Sw", 0xF05138}; return v; }
        case Language::Kotlin: { static const LanguageVisual v{L"Kt", 0xA97BFF}; return v; }
        default: return generic;
    }
}

} // namespace

bool Renderer::FormatKey::operator<(const FormatKey& o) const {
    return std::tie(family, size, weight, italic, align, wrap) <
           std::tie(o.family, o.size, o.weight, o.italic, o.align, o.wrap);
}

bool Renderer::Init(HWND hwnd) {
    hwnd_ = hwnd;
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2d_);
    if (FAILED(hr)) return false;
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                             reinterpret_cast<IUnknown**>(&dwrite_));
    if (FAILED(hr)) return false;
    PickFonts();
    return true;
}

void Renderer::Shutdown() {
    DiscardTarget();
    for (auto& [_, format] : formats_) {
        if (format) format->Release();
    }
    formats_.clear();
    if (dwrite_) {
        dwrite_->Release();
        dwrite_ = nullptr;
    }
    if (d2d_) {
        d2d_->Release();
        d2d_ = nullptr;
    }
    hwnd_ = nullptr;
}

bool Renderer::HasFontFamily(const wchar_t* name) const {
    if (!dwrite_ || !name) return false;
    IDWriteFontCollection* collection = nullptr;
    if (FAILED(dwrite_->GetSystemFontCollection(&collection, FALSE)) || !collection) return false;
    UINT32 index = 0;
    BOOL exists = FALSE;
    HRESULT hr = collection->FindFamilyName(name, &index, &exists);
    collection->Release();
    return SUCCEEDED(hr) && exists;
}

void Renderer::PickFonts() {
    if (HasFontFamily(L"Segoe UI Variable Text")) uiFont_ = L"Segoe UI Variable Text";
    else if (HasFontFamily(L"Segoe UI")) uiFont_ = L"Segoe UI";

    if (HasFontFamily(L"Cascadia Mono")) monoFont_ = L"Cascadia Mono";
    else if (HasFontFamily(L"Cascadia Code")) monoFont_ = L"Cascadia Code";
    else if (HasFontFamily(L"JetBrains Mono")) monoFont_ = L"JetBrains Mono";
    else monoFont_ = L"Consolas";

    if (HasFontFamily(L"Segoe Fluent Icons")) iconFont_ = L"Segoe Fluent Icons";
    else if (HasFontFamily(L"Segoe MDL2 Assets")) iconFont_ = L"Segoe MDL2 Assets";
}

bool Renderer::CreateTarget() {
    if (!d2d_ || !hwnd_ || width_ == 0 || height_ == 0) return false;
    DiscardTarget();
    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps = D2D1::HwndRenderTargetProperties(
        hwnd_, D2D1::SizeU(width_, height_), D2D1_PRESENT_OPTIONS_NONE);
    HRESULT hr = d2d_->CreateHwndRenderTarget(props, hwndProps, &target_);
    if (FAILED(hr)) {
        target_ = nullptr;
        return false;
    }
    target_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    target_->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    target_->SetDpi(96.0f, 96.0f);
    return true;
}

void Renderer::DiscardTarget() {
    for (auto& [_, brush] : brushes_) {
        if (brush) brush->Release();
    }
    brushes_.clear();
    // D2D bitmaps belong to the render target and must be rebuilt with it.
    if (brandImage_) {
        brandImage_->Release();
        brandImage_ = nullptr;
    }
    if (target_) {
        target_->Release();
        target_ = nullptr;
    }
}

void Renderer::Resize(UINT width, UINT height) {
    width_ = width;
    height_ = height;
    if (!target_) {
        CreateTarget();
        return;
    }
    if (width == 0 || height == 0) {
        DiscardTarget();
        return;
    }
    target_->Resize(D2D1::SizeU(width, height));
}

void Renderer::BeginDraw() {
    if (!target_ && !CreateTarget()) return;
    target_->BeginDraw();
}

void Renderer::EndDraw() {
    if (!target_) return;
    HRESULT hr = target_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET || FAILED(hr)) {
        DiscardTarget();
    }
}

void Renderer::Clear(Color color) {
    if (target_) target_->Clear(color);
}

void Renderer::FillRect(const Rect& rect, Color color) {
    if (!target_ || rect.right <= rect.left || rect.bottom <= rect.top) return;
    target_->FillRectangle(rect, Brush(color));
}

void Renderer::FillRoundedRect(const Rect& rect, float radius, Color color) {
    if (!target_ || rect.right <= rect.left || rect.bottom <= rect.top) return;
    target_->FillRoundedRectangle(D2D1::RoundedRect(rect, radius, radius), Brush(color));
}

void Renderer::StrokeRect(const Rect& rect, Color color, float width) {
    if (!target_) return;
    target_->DrawRectangle(rect, Brush(color), width);
}

void Renderer::StrokeRoundedRect(const Rect& rect, float radius, Color color, float width) {
    if (!target_) return;
    target_->DrawRoundedRectangle(D2D1::RoundedRect(rect, radius, radius), Brush(color), width);
}

void Renderer::DrawLine(float x1, float y1, float x2, float y2, Color color, float width) {
    if (!target_) return;
    target_->DrawLine(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), Brush(color), width);
}

void Renderer::DrawEllipse(float cx, float cy, float rx, float ry, Color color, bool filled) {
    if (!target_) return;
    if (filled) target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), rx, ry), Brush(color));
    else target_->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), rx, ry), Brush(color), 1.0f);
}

void Renderer::FillTriangle(float x1, float y1, float x2, float y2, float x3, float y3, Color color) {
    if (!target_ || !d2d_) return;
    ID2D1PathGeometry* geometry = nullptr;
    if (FAILED(d2d_->CreatePathGeometry(&geometry)) || !geometry) return;
    ID2D1GeometrySink* sink = nullptr;
    if (FAILED(geometry->Open(&sink)) || !sink) {
        geometry->Release();
        return;
    }
    sink->BeginFigure(D2D1::Point2F(x1, y1), D2D1_FIGURE_BEGIN_FILLED);
    sink->AddLine(D2D1::Point2F(x2, y2));
    sink->AddLine(D2D1::Point2F(x3, y3));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();
    target_->FillGeometry(geometry, Brush(color));
    sink->Release();
    geometry->Release();
}

void Renderer::DrawLanguageIcon(Language language, const Rect& rect, Color mutedColor, float alpha) {
    const LanguageVisual& visual = VisualForLanguage(language);
    if (!target_) return;

    // Only unrecognized/plain content uses the generic document glyph.
    if (language == Language::Plain) {
        TextStyle glyph;
        glyph.family = iconFont_;
        glyph.size = std::max(8.0f, std::min(RectWidth(rect), RectHeight(rect)) * 0.85f);
        glyph.hAlign = 1;
        glyph.vAlign = 1;
        Text(visual.monogram, rect, WithAlpha(mutedColor, alpha), glyph);
        return;
    }

    float side = std::min(RectWidth(rect), RectHeight(rect));
    if (side <= 0.0f) return;
    float cx = (rect.left + rect.right) * 0.5f;
    float cy = (rect.top + rect.bottom) * 0.5f;
    float badgeSize = side * 0.92f;
    Rect badge{cx - badgeSize * 0.5f, cy - badgeSize * 0.5f,
               cx + badgeSize * 0.5f, cy + badgeSize * 0.5f};

    Color brand = Col(visual.color);
    FillRoundedRect(badge, badgeSize * 0.22f, WithAlpha(brand, 0.16f * alpha));
    StrokeRoundedRect(badge, badgeSize * 0.22f, WithAlpha(brand, 0.9f * alpha),
                      std::max(0.8f, badgeSize * 0.055f));

    size_t length = wcslen(visual.monogram);
    TextStyle label;
    label.size = badgeSize * (length >= 3 ? 0.34f : (length == 2 ? 0.44f : 0.54f));
    label.weight = 700;
    label.hAlign = 1;
    label.vAlign = 1;
    label.ellipsis = false;
    Text(visual.monogram, badge, WithAlpha(brand, alpha), label);
}

bool Renderer::EnsureBrandImage() {
    if (brandImage_) return true;
    if (!target_) return false;

    HMODULE module = GetModuleHandleW(nullptr);
    HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(NEO_RCDATA_LOGO), RT_RCDATA);
    if (!resource) return false;
    HGLOBAL memory = LoadResource(module, resource);
    if (!memory) return false;
    void* data = LockResource(memory);
    DWORD size = SizeofResource(module, resource);
    if (!data || size == 0) return false;

    IWICImagingFactory* factory = nullptr;
    IWICStream* stream = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;
    bool ok = false;

    // WIC decodes the PNG embedded as RCDATA. Direct2D cannot consume the
    // decoder's native pixel format directly (CreateBitmapFromWicBitmap fails
    // with WINCODEC_ERR_UNSUPPORTEDPIXELFORMAT), so convert to premultiplied
    // BGRA first.
    if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_IWICImagingFactory,
                                   reinterpret_cast<void**>(&factory))) && factory) {
        if (SUCCEEDED(factory->CreateStream(&stream)) && stream &&
            SUCCEEDED(stream->InitializeFromMemory(static_cast<BYTE*>(data), size)) &&
            SUCCEEDED(factory->CreateDecoderFromStream(stream, nullptr,
                                                       WICDecodeMetadataCacheOnLoad,
                                                       &decoder)) && decoder &&
            SUCCEEDED(decoder->GetFrame(0, &frame)) && frame &&
            SUCCEEDED(factory->CreateFormatConverter(&converter)) && converter &&
            SUCCEEDED(converter->Initialize(frame, GUID_WICPixelFormat32bppPBGRA,
                                            WICBitmapDitherTypeNone, nullptr, 0.0,
                                            WICBitmapPaletteTypeCustom))) {
            ok = SUCCEEDED(target_->CreateBitmapFromWicBitmap(converter, nullptr, &brandImage_));
        }
    }

    if (converter) converter->Release();
    if (frame) frame->Release();
    if (decoder) decoder->Release();
    if (stream) stream->Release();
    if (factory) factory->Release();
    return ok;
}

bool Renderer::DrawBrandImage(const Rect& rect, float opacity) {
    if (!EnsureBrandImage() || !target_) return false;
    D2D1_SIZE_U pixels = brandImage_->GetPixelSize();
    D2D1_RECT_F source = D2D1::RectF(0.0f, 0.0f, (float)pixels.width, (float)pixels.height);
    D2D1_RECT_F destination = D2D1::RectF(rect.left, rect.top, rect.right, rect.bottom);
    target_->DrawBitmap(brandImage_, destination, std::clamp(opacity, 0.0f, 1.0f),
                        D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, source);
    return true;
}

void Renderer::PushClip(const Rect& rect) {
    if (!target_) return;
    target_->PushAxisAlignedClip(rect, D2D1_ANTIALIAS_MODE_ALIASED);
}

void Renderer::PopClip() {
    if (!target_) return;
    target_->PopAxisAlignedClip();
}

ID2D1SolidColorBrush* Renderer::Brush(Color color) {
    if (!target_) return nullptr;
    auto channel = [](float value) {
        return (uint32_t)std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f);
    };
    // Fold all four channels into the key. Keeping only one channel here made
    // unrelated colors share a brush, which washed out switches and syntax
    // colors that happened to have the same red value.
    uint32_t key = (channel(color.r) << 24) | (channel(color.g) << 16) |
                   (channel(color.b) << 8) | channel(color.a);
    auto it = brushes_.find(key);
    if (it != brushes_.end()) return it->second;
    ID2D1SolidColorBrush* brush = nullptr;
    D2D1_COLOR_F c = D2D1::ColorF(color.r, color.g, color.b, color.a);
    if (FAILED(target_->CreateSolidColorBrush(c, &brush))) return nullptr;
    brushes_[key] = brush;
    return brush;
}

IDWriteTextFormat* Renderer::FormatFor(const TextStyle& style) {
    if (!dwrite_) return nullptr;
    FormatKey key;
    key.family = style.family.empty() ? uiFont_ : style.family;
    key.size = std::max(1.0f, style.size);
    key.weight = style.weight;
    key.italic = style.italic;
    key.align = style.hAlign;
    key.wrap = style.wrap;
    auto it = formats_.find(key);
    if (it != formats_.end()) return it->second;

    IDWriteTextFormat* format = nullptr;
    HRESULT hr = dwrite_->CreateTextFormat(
        key.family.c_str(), nullptr,
        (DWRITE_FONT_WEIGHT)key.weight,
        style.italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        key.size, L"", &format);
    if (FAILED(hr) || !format) return nullptr;
    DWRITE_TEXT_ALIGNMENT align = DWRITE_TEXT_ALIGNMENT_LEADING;
    if (style.hAlign == 1) align = DWRITE_TEXT_ALIGNMENT_CENTER;
    else if (style.hAlign == 2) align = DWRITE_TEXT_ALIGNMENT_TRAILING;
    format->SetTextAlignment(align);
    format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    format->SetWordWrapping(style.wrap ? DWRITE_WORD_WRAPPING_WRAP : DWRITE_WORD_WRAPPING_NO_WRAP);
    formats_[key] = format;
    return format;
}

IDWriteTextLayout* Renderer::CreateLayout(const std::wstring& text, float maxWidth, float maxHeight,
                                          const TextStyle& style) {
    if (!dwrite_) return nullptr;
    std::wstring displayText = style.uiText ? TranslateUiText(text, chinese_) : text;
    TextStyle fmtStyle = style;
    fmtStyle.hAlign = 0;
    fmtStyle.wrap = style.wrap;
    IDWriteTextFormat* format = FormatFor(fmtStyle);
    if (!format) return nullptr;
    IDWriteTextLayout* layout = nullptr;
    if (FAILED(dwrite_->CreateTextLayout(displayText.c_str(), (UINT32)displayText.size(), format,
                                         std::max(1.0f, maxWidth), std::max(1.0f, maxHeight), &layout))) {
        return nullptr;
    }
    layout->SetTextAlignment(
        style.hAlign == 1 ? DWRITE_TEXT_ALIGNMENT_CENTER :
        style.hAlign == 2 ? DWRITE_TEXT_ALIGNMENT_TRAILING : DWRITE_TEXT_ALIGNMENT_LEADING);
    layout->SetParagraphAlignment(
        style.vAlign == 1 ? DWRITE_PARAGRAPH_ALIGNMENT_CENTER :
        style.vAlign == 2 ? DWRITE_PARAGRAPH_ALIGNMENT_FAR : DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    if (style.ellipsis) {
        IDWriteInlineObject* sign = nullptr;
        if (SUCCEEDED(dwrite_->CreateEllipsisTrimmingSign(format, &sign)) && sign) {
            DWRITE_TRIMMING trimming{};
            trimming.granularity = DWRITE_TRIMMING_GRANULARITY_CHARACTER;
            layout->SetTrimming(&trimming, sign);
            sign->Release();
        }
    }
    return layout;
}

void Renderer::DrawLayout(IDWriteTextLayout* layout, float x, float y, Color color) {
    if (!target_ || !layout) return;
    target_->DrawTextLayout(D2D1::Point2F(x, y), layout, Brush(color),
                            D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

void Renderer::Text(const std::wstring& text, const Rect& rect, Color color, const TextStyle& style) {
    if (!target_ || rect.right <= rect.left || rect.bottom <= rect.top) return;
    TextStyle layoutStyle = style;
    layoutStyle.vAlign = 0;
    // Give the layout essentially unlimited height and center the resulting
    // block ourselves. Using the rect height as the layout height makes
    // DirectWrite clip glyphs whenever the font's natural line height is a
    // fraction larger than the row, which is common for 15-18px UI text.
    IDWriteTextLayout* layout = CreateLayout(text, RectWidth(rect), 100000.0f, layoutStyle);
    if (!layout) return;
    DWRITE_TEXT_METRICS metrics{};
    layout->GetMetrics(&metrics);
    float y = rect.top;
    if (style.vAlign == 1) {
        y += std::max(0.0f, (RectHeight(rect) - metrics.height) * 0.5f);
    } else if (style.vAlign == 2) {
        y += std::max(0.0f, RectHeight(rect) - metrics.height);
    }
    DrawLayout(layout, rect.left, y, color);
    layout->Release();
}

D2D1_SIZE_F Renderer::Measure(const std::wstring& text, const TextStyle& style) {
    if (!dwrite_) return D2D1::SizeF(0, 0);
    TextStyle fmtStyle = style;
    fmtStyle.hAlign = 0;
    fmtStyle.ellipsis = false;
    IDWriteTextLayout* layout = CreateLayout(text, 100000.0f, 100000.0f, fmtStyle);
    if (!layout) return D2D1::SizeF(0, 0);
    DWRITE_TEXT_METRICS metrics{};
    layout->GetMetrics(&metrics);
    layout->Release();
    return D2D1::SizeF(metrics.widthIncludingTrailingWhitespace, metrics.height);
}
