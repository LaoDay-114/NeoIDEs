#include "app.h"

namespace {

constexpr float kPaletteRow = 34.0f;
constexpr float kQuickRow = 34.0f;

std::wstring TrimTo(const std::wstring& value, size_t maxLen) {
    if (value.size() <= maxLen) return value;
    return value.substr(0, maxLen) + L"…";
}

Color ToggleTrack(bool on, const ThemeColors& t) {
    return on ? t.accent : WithAlpha(t.textFaint, 0.28f);
}

bool BlinkOn(ULONGLONG resetTick, bool active) {
    if (!active) return true;
    return ((GetTickCount64() - resetTick) / 530) % 2 == 0;
}

} // namespace

// ------------------------------------------------------------------ text field primitives

void TextField::Insert(const std::wstring& value) {
    if (value.empty()) return;
    DeleteSelection();
    caret = std::clamp(caret, 0, (int)text.size());
    text.insert((size_t)caret, value);
    caret += (int)value.size();
    anchor = caret;
}

void TextField::DeleteSelection() {
    if (!HasSelection()) return;
    auto [a, b] = Selection();
    text.erase((size_t)a, (size_t)(b - a));
    caret = a;
    anchor = a;
}

void TextField::Backspace(bool word) {
    if (HasSelection()) {
        DeleteSelection();
        return;
    }
    if (caret <= 0) return;
    int pos = caret;
    if (word) {
        pos = caret - 1;
        while (pos > 0 && !IsWordChar(text[(size_t)pos])) --pos;
        while (pos > 0 && IsWordChar(text[(size_t)pos - 1])) --pos;
        if (pos == caret - 1) pos = std::max(0, caret - 1);
    } else {
        --pos;
        if (pos > 0 && IsLowSurrogate(text[(size_t)pos]) &&
            IsHighSurrogate(text[(size_t)pos - 1])) --pos;
    }
    text.erase((size_t)pos, (size_t)(caret - pos));
    caret = pos;
    anchor = pos;
}

void TextField::DeleteForward(bool word) {
    if (HasSelection()) {
        DeleteSelection();
        return;
    }
    if (caret >= (int)text.size()) return;
    int pos = caret;
    if (word) {
        while (pos < (int)text.size() && !IsWordChar(text[(size_t)pos])) ++pos;
        while (pos < (int)text.size() && IsWordChar(text[(size_t)pos])) ++pos;
    } else {
        ++pos;
        if (IsHighSurrogate(text[(size_t)caret]) && pos < (int)text.size() &&
            IsLowSurrogate(text[(size_t)pos])) ++pos;
    }
    text.erase((size_t)caret, (size_t)(pos - caret));
    anchor = caret;
}

void TextField::Move(int delta, bool extend, bool word) {
    int pos = caret;
    if (word) {
        if (delta < 0) {
            while (pos > 0 && !IsWordChar(text[(size_t)pos - 1])) --pos;
            while (pos > 0 && IsWordChar(text[(size_t)pos - 1])) --pos;
        } else {
            while (pos < (int)text.size() && !IsWordChar(text[(size_t)pos])) ++pos;
            while (pos < (int)text.size() && IsWordChar(text[(size_t)pos])) ++pos;
        }
    } else {
        int next = std::clamp(pos + delta, 0, (int)text.size());
        if (delta < 0 && next > 0 && next < (int)text.size() &&
            IsHighSurrogate(text[(size_t)next - 1]) && IsLowSurrogate(text[(size_t)next])) {
            --next;
        } else if (delta > 0 && next > 0 && next < (int)text.size() &&
                   IsHighSurrogate(text[(size_t)next - 1]) && IsLowSurrogate(text[(size_t)next])) {
            ++next;
        }
        pos = next;
    }
    caret = pos;
    if (!extend) anchor = caret;
}

std::wstring TextField::Selected() const {
    if (!HasSelection()) return {};
    auto [a, b] = Selection();
    return text.substr((size_t)a, (size_t)(b - a));
}

bool App::HandleTextFieldKey(TextField& field, WPARAM key, bool ctrl, bool shift) {
    field.Clamp();
    switch (key) {
        case VK_LEFT:
            field.Move(-1, shift, ctrl);
            return true;
        case VK_RIGHT:
            field.Move(1, shift, ctrl);
            return true;
        case VK_HOME:
            field.Home(shift, false);
            return true;
        case VK_END:
            field.End(shift);
            return true;
        case VK_BACK:
            field.Backspace(ctrl);
            return true;
        case VK_DELETE:
            field.DeleteForward(ctrl);
            return true;
        default:
            break;
    }
    if (ctrl) {
        if (key == 'A') {
            field.SelectAll();
            return true;
        }
        if (key == 'C') {
            std::wstring selected = field.Selected();
            if (!selected.empty()) SetClipboardText(hwnd_, selected);
            return true;
        }
        if (key == 'X') {
            std::wstring selected = field.Selected();
            if (!selected.empty()) {
                SetClipboardText(hwnd_, selected);
                field.DeleteSelection();
            }
            return true;
        }
        if (key == 'V') {
            field.Insert(ClipboardText(hwnd_));
            return true;
        }
    }
    return false;
}

void App::DrawTextField(TextField& field, const Rect& rect, float fontSize, Color color,
                        const std::wstring& placeholder, bool focused) {
    field.Clamp();
    renderer_.FillRoundedRect(rect, S(6.0f), theme_.inputBg);
    renderer_.StrokeRoundedRect(rect, S(6.0f),
                                focused ? WithAlpha(theme_.accent, 0.65f) : theme_.border, 1.0f);

    Rect inner = MakeRect(rect.left + S(9.0f), rect.top, rect.right - S(9.0f), rect.bottom);
    TextStyle style;
    style.size = fontSize;
    style.weight = 400;
    style.vAlign = 1;
    style.ellipsis = false;
    style.wrap = false;
    style.uiText = false;

    std::wstring prefix = field.text.substr(0, (size_t)std::clamp(field.caret, 0, (int)field.text.size()));
    float caretOffset = renderer_.Measure(prefix, style).width;
    float visibleWidth = RectWidth(inner);
    if (caretOffset - field.scroll > visibleWidth - S(7.0f)) {
        field.scroll = caretOffset - visibleWidth + S(7.0f);
    }
    if (caretOffset - field.scroll < S(2.0f)) {
        field.scroll = std::max(0.0f, caretOffset - S(2.0f));
    }
    if (field.text.empty()) field.scroll = 0;

    renderer_.PushClip(inner);
    if (field.HasSelection()) {
        auto [a, b] = field.Selection();
        float x0 = inner.left - field.scroll + renderer_.Measure(field.text.substr(0, (size_t)a), style).width;
        float x1 = inner.left - field.scroll + renderer_.Measure(field.text.substr(0, (size_t)b), style).width;
        renderer_.FillRect(MakeRect(x0, rect.top + S(4.0f), x1, rect.bottom - S(4.0f)), theme_.selection);
    }
    float textWidth = renderer_.Measure(field.text, style).width;
    if (!field.text.empty()) {
        renderer_.Text(field.text,
                       MakeRect(inner.left - field.scroll, rect.top,
                                inner.left - field.scroll + textWidth + S(20.0f), rect.bottom),
                       color, style);
    }
    if (focused && BlinkOn(blinkResetTick_, windowActive_)) {
        float cx = inner.left - field.scroll + caretOffset;
        renderer_.FillRect(MakeRect(cx, rect.top + S(6.0f), cx + std::max(1.0f, S(1.4f)),
                                    rect.bottom - S(7.0f)), theme_.accent);
    }
    renderer_.PopClip();

    if (field.text.empty() && !placeholder.empty()) {
        TextStyle placeholderStyle = style;
        placeholderStyle.uiText = true;
        renderer_.Text(placeholder, inner, theme_.textFaint, placeholderStyle);
    }
}

TextField* App::ActiveTextField() {
    switch (overlay_) {
        case Overlay::Find: return &findField_;
        case Overlay::Palette: return &paletteField_;
        case Overlay::QuickOpen: return &quickField_;
        case Overlay::Goto: return &gotoField_;
        case Overlay::Prompt: return &prompt_.field;
        default: return nullptr;
    }
}

bool App::IsOverlayTextFieldActive() const {
    return overlay_ == Overlay::Find || overlay_ == Overlay::Palette ||
           overlay_ == Overlay::QuickOpen || overlay_ == Overlay::Goto ||
           overlay_ == Overlay::Prompt;
}

// ------------------------------------------------------------------ sidebar / welcome

void App::DrawSidebar() {
    if (!settings_.sidebarVisible) return;
    renderer_.FillRect(L_.sidebar, theme_.sidebarBg);
    renderer_.FillRect(MakeRect(L_.sidebar.right - S(1.0f), L_.sidebar.top,
                                L_.sidebar.right, L_.sidebar.bottom), theme_.borderSoft);

    float headerTop = L_.sidebar.top;
    TextStyle labelStyle;
    labelStyle.size = S(10.5f);
    labelStyle.weight = 700;
    labelStyle.vAlign = 1;
    renderer_.Text(L"EXPLORER",
                   MakeRect(L_.sidebar.left + S(14.0f), headerTop + S(7.0f),
                            L_.sidebar.right - S(60.0f), headerTop + S(28.0f)),
                   theme_.textFaint, labelStyle);

    TextStyle projectStyle;
    projectStyle.size = S(12.5f);
    projectStyle.weight = 650;
    projectStyle.vAlign = 1;
    renderer_.Text(projectRoot_.empty() ? L"No Folder" : BaseName(projectRoot_),
                   MakeRect(L_.sidebar.left + S(14.0f), headerTop + S(27.0f),
                            L_.sidebar.right - S(60.0f), headerTop + S(50.0f)),
                   projectRoot_.empty() ? theme_.textFaint : theme_.text, projectStyle);

    TextStyle icon;
    icon.family = renderer_.IconFont();
    icon.size = S(13.0f);
    icon.hAlign = 1;
    icon.vAlign = 1;
    bool newHover = Contains(explorerNewButton_, mouseX_, mouseY_);
    bool refreshHover = Contains(explorerRefreshButton_, mouseX_, mouseY_);
    if (!projectRoot_.empty()) {
        if (newHover) renderer_.FillRoundedRect(explorerNewButton_, S(5.0f), theme_.hover);
        if (refreshHover) renderer_.FillRoundedRect(explorerRefreshButton_, S(5.0f), theme_.hover);
        renderer_.Text(L"\uE710", explorerNewButton_, newHover ? theme_.text : theme_.textFaint, icon);
        renderer_.Text(L"\uE72C", explorerRefreshButton_, refreshHover ? theme_.text : theme_.textFaint, icon);
    }

    renderer_.FillRect(MakeRect(L_.sidebar.left + S(10.0f), L_.sidebar.top + S(58.0f),
                                L_.sidebar.right - S(10.0f), L_.sidebar.top + S(59.0f)),
                       theme_.borderSoft);

    BuildTreeRows();
    renderer_.PushClip(treeViewRect_);
    if (projectRoot_.empty()) {
        float cy = treeViewRect_.top + S(70.0f);
        TextStyle emptyStyle;
        emptyStyle.size = S(12.0f);
        emptyStyle.hAlign = 1;
        emptyStyle.vAlign = 1;
        renderer_.Text(L"No folder is open",
                       MakeRect(treeViewRect_.left, cy - S(26.0f), treeViewRect_.right, cy),
                       theme_.textDim, emptyStyle);
        TextStyle actionStyle = emptyStyle;
        actionStyle.weight = 600;
        renderer_.Text(L"Open Folder",
                       MakeRect(treeViewRect_.left, cy + S(4.0f), treeViewRect_.right, cy + S(32.0f)),
                       theme_.accent, actionStyle);
    } else if (treeRows_.empty()) {
        TextStyle emptyStyle;
        emptyStyle.size = S(12.0f);
        emptyStyle.hAlign = 1;
        emptyStyle.vAlign = 1;
        renderer_.Text(L"Empty folder",
                       MakeRect(treeViewRect_.left, treeViewRect_.top + S(40.0f),
                                treeViewRect_.right, treeViewRect_.top + S(72.0f)),
                       theme_.textFaint, emptyStyle);
    } else {
        TextStyle treeStyle;
        treeStyle.size = S(12.0f);
        treeStyle.vAlign = 1;
        treeStyle.uiText = false;
        for (const auto& row : treeRows_) {
            if (!row.node) continue;
            if (row.rect.bottom < treeViewRect_.top || row.rect.top > treeViewRect_.bottom) continue;
            bool selected = !treeSelectedPath_.empty() && IEquals(row.node->path, treeSelectedPath_);
            bool hover = Contains(row.rect, mouseX_, mouseY_);
            if (selected) {
                renderer_.FillRoundedRect(Inset(row.rect, S(2.0f)), S(5.0f), theme_.accentSoft);
                renderer_.FillRoundedRect(MakeRect(row.rect.left + S(2.0f), row.rect.top + S(5.0f),
                                                    row.rect.left + S(4.0f), row.rect.bottom - S(5.0f)),
                                          S(1.0f), theme_.accent);
            } else if (hover) {
                renderer_.FillRoundedRect(Inset(row.rect, S(2.0f)), S(5.0f), theme_.hover);
            }
            float x = row.rect.left + S(9.0f) + row.depth * S(15.0f);
            float centerY = (row.rect.top + row.rect.bottom) * 0.5f;
            if (row.node->isDir) {
                renderer_.Text(row.node->expanded ? L"\uE70D" : L"\uE76C",
                               MakeRect(x - S(2.0f), row.rect.top, x + S(12.0f), row.rect.bottom),
                               theme_.textFaint, icon);
                x += S(14.0f);
                renderer_.Text(L"\uE8B7",
                               MakeRect(x, row.rect.top, x + S(18.0f), row.rect.bottom),
                               selected ? theme_.accent : theme_.textDim, icon);
                x += S(20.0f);
            } else {
                Color fileColor = theme_.textFaint;
                Language lang = LanguageFromPath(row.node->name);
                if (lang == Language::Cpp) fileColor = theme_.synType;
                else if (lang == Language::C) fileColor = theme_.synFunction;
                else if (lang == Language::CMake) fileColor = theme_.synPreproc;
                else if (lang == Language::Python) fileColor = theme_.warning;
                else if (lang == Language::Json) fileColor = theme_.synNumber;
                else if (lang == Language::Markdown) fileColor = theme_.info;
                renderer_.Text(L"\uE8A5",
                               MakeRect(x + S(2.0f), row.rect.top, x + S(18.0f), row.rect.bottom),
                               fileColor, icon);
                x += S(22.0f);
            }
            renderer_.Text(row.node->name,
                           MakeRect(x, row.rect.top, row.rect.right - S(8.0f), row.rect.bottom),
                           selected ? theme_.text : theme_.textDim, treeStyle);
        }
    }
    renderer_.PopClip();

    if (treeContentHeight_ > RectHeight(treeViewRect_) && RectHeight(treeViewRect_) > 0) {
        float trackTop = treeViewRect_.top;
        float trackH = RectHeight(treeViewRect_);
        float thumbH = std::max(S(28.0f), trackH * (trackH / std::max(1.0f, treeContentHeight_)));
        float maxScroll = std::max(1.0f, treeContentHeight_ - trackH);
        float top = trackTop + (trackH - thumbH) * std::clamp(treeScroll_ / maxScroll, 0.0f, 1.0f);
        renderer_.FillRoundedRect(MakeRect(L_.sidebar.right - S(7.0f), top,
                                            L_.sidebar.right - S(3.0f), top + thumbH),
                                  S(2.0f), theme_.scrollbar);
    }
}

void App::DrawWelcome() {
    Rect main = L_.main;
    renderer_.FillRect(main, theme_.editorBg);
    float contentW = std::min(S(760.0f), std::max(S(420.0f), RectWidth(main) - S(80.0f)));
    float left = main.left + (RectWidth(main) - contentW) * 0.5f;
    float top = main.top + std::max(S(36.0f), RectHeight(main) * 0.09f);

    DrawLogo(left, top, S(54.0f));

    TextStyle title;
    title.size = S(34.0f);
    title.weight = 680;
    title.vAlign = 1;
    renderer_.Text(L"NeoIDEs", MakeRect(left + S(70.0f), top - S(2.0f),
                                        left + contentW, top + S(40.0f)),
                   theme_.text, title);

    TextStyle tagline;
    tagline.size = S(13.0f);
    tagline.vAlign = 1;
    renderer_.Text(L"A fast, focused editor for C and C++.",
                   MakeRect(left + S(70.0f), top + S(38.0f), left + contentW, top + S(64.0f)),
                   theme_.textDim, tagline);

    float actionsTop = top + S(106.0f);
    float gap = S(12.0f);
    float actionW = (contentW - gap) * 0.5f;
    float actionH = S(62.0f);
    welcomeNewFile_ = MakeRect(left, actionsTop, left + actionW, actionsTop + actionH);
    welcomeOpenFile_ = MakeRect(left + actionW + gap, actionsTop,
                                left + contentW, actionsTop + actionH);
    welcomeOpenFolder_ = MakeRect(left, actionsTop + actionH + gap,
                                  left + actionW, actionsTop + actionH * 2 + gap);
    welcomePalette_ = MakeRect(left + actionW + gap, actionsTop + actionH + gap,
                               left + contentW, actionsTop + actionH * 2 + gap);

    struct WelcomeAction {
        Rect rect;
        const wchar_t* glyph;
        const wchar_t* title;
        const wchar_t* shortcut;
    } actions[4] = {
        {welcomeNewFile_, L"\uE710", L"New File", L"Ctrl+N"},
        {welcomeOpenFile_, L"\uE8E5", L"Open File", L"Ctrl+O"},
        {welcomeOpenFolder_, L"\uE8B7", L"Open Folder", L"Ctrl+Shift+O"},
        {welcomePalette_, L"\uE945", L"Command Palette", L"Ctrl+Shift+P"},
    };

    TextStyle icon;
    icon.family = renderer_.IconFont();
    icon.size = S(20.0f);
    icon.vAlign = 1;
    icon.hAlign = 1;
    TextStyle actionTitle;
    actionTitle.size = S(13.0f);
    actionTitle.weight = 600;
    actionTitle.vAlign = 1;
    TextStyle actionKey;
    actionKey.size = S(10.5f);
    actionKey.vAlign = 1;
    for (const auto& action : actions) {
        bool hover = Contains(action.rect, mouseX_, mouseY_);
        renderer_.FillRoundedRect(action.rect, S(9.0f), hover ? theme_.hover : theme_.sidebarBg);
        renderer_.StrokeRoundedRect(action.rect, S(9.0f), theme_.borderSoft, 1.0f);
        Rect iconRect = MakeRect(action.rect.left + S(10.0f), action.rect.top,
                                 action.rect.left + S(48.0f), action.rect.bottom);
        renderer_.Text(action.glyph, iconRect, hover ? theme_.accent : theme_.textDim, icon);
        renderer_.Text(action.title,
                       MakeRect(action.rect.left + S(54.0f), action.rect.top,
                                action.rect.right - S(10.0f), action.rect.top + actionH * 0.62f),
                       hover ? theme_.text : theme_.textDim, actionTitle);
        renderer_.Text(action.shortcut,
                       MakeRect(action.rect.left + S(54.0f), action.rect.top + actionH * 0.40f,
                                action.rect.right - S(10.0f), action.rect.bottom - S(4.0f)),
                       theme_.textFaint, actionKey);
    }

    float recentTop = actionsTop + actionH * 2 + gap + S(34.0f);
    TextStyle section;
    section.size = S(10.5f);
    section.weight = 700;
    renderer_.Text(L"RECENT PROJECTS", MakeRect(left, recentTop - S(18.0f),
                                                left + contentW, recentTop),
                   theme_.textFaint, section);

    welcomeRecentRects_.clear();
    if (settings_.recentProjects.empty()) {
        TextStyle hint;
        hint.size = S(12.0f);
        renderer_.Text(L"Open a folder to start exploring your project. Drag a file or folder here anytime.",
                       MakeRect(left, recentTop + S(8.0f), left + contentW, recentTop + S(34.0f)),
                       theme_.textFaint, hint);
    } else {
        float y = recentTop + S(2.0f);
        int shown = 0;
        for (const auto& path : settings_.recentProjects) {
            if (shown >= 5) break;
            Rect row = MakeRect(left, y, left + contentW, y + S(38.0f));
            bool hover = Contains(row, mouseX_, mouseY_);
            if (hover) renderer_.FillRoundedRect(row, S(7.0f), theme_.hover);
            renderer_.Text(L"\uE8B7", MakeRect(row.left + S(8.0f), row.top,
                                               row.left + S(30.0f), row.bottom),
                           hover ? theme_.accent : theme_.textFaint, icon);
            TextStyle recentName;
            recentName.size = S(12.0f);
            recentName.weight = 600;
            recentName.vAlign = 1;
            recentName.uiText = false;
            renderer_.Text(BaseName(path),
                           MakeRect(row.left + S(36.0f), row.top, row.left + contentW * 0.45f, row.bottom),
                           hover ? theme_.text : theme_.textDim, recentName);
            TextStyle recentPath;
            recentPath.size = S(10.5f);
            recentPath.vAlign = 1;
            recentPath.hAlign = 2;
            recentPath.uiText = false;
            renderer_.Text(path,
                           MakeRect(row.left + contentW * 0.45f, row.top, row.right - S(10.0f), row.bottom),
                           theme_.textFaint, recentPath);
            welcomeRecentRects_.push_back({row, path});
            y += S(40.0f);
            ++shown;
        }
    }

    TextStyle footer;
    footer.size = S(10.5f);
    footer.hAlign = 1;
    renderer_.Text(L"Tip: press Ctrl+P to jump to a file, or Ctrl+Shift+P for every command.",
                   MakeRect(main.left, main.bottom - S(34.0f), main.right, main.bottom - S(10.0f)),
                   theme_.textFaint, footer);
}

// ------------------------------------------------------------------ modal

Rect App::GetModalCardRect() {
    float w = std::min(S(480.0f), (float)width_ - S(80.0f));
    float h = S(224.0f);
    float left = ((float)width_ - w) * 0.5f;
    float top = ((float)height_ - h) * 0.5f;
    return MakeRect(left, top, left + w, top + h);
}

void App::ShowModal(const std::wstring& title, const std::wstring& message,
                    std::vector<ModalButtonInfo> buttons, int enterResult, int escapeResult,
                    std::function<void(int)> callback) {
    if (buttons.empty()) buttons.push_back(ModalButtonInfo{L"OK", 0, true});
    modalOpen_ = true;
    modalTitle_ = title;
    modalMessage_ = message;
    modalButtons_ = std::move(buttons);
    modalEnterResult_ = enterResult;
    modalEscResult_ = escapeResult;
    modalCallback_ = std::move(callback);
    modalCard_ = GetModalCardRect();
    modalButtonRects_.assign(modalButtons_.size(), Rect{});
    float y = modalCard_.bottom - S(53.0f);
    float right = modalCard_.right - S(22.0f);
    for (int i = (int)modalButtons_.size() - 1; i >= 0; --i) {
        TextStyle style;
        style.size = S(12.0f);
        style.weight = 550;
        float textW = renderer_.Measure(modalButtons_[(size_t)i].label, style).width;
        float w = std::max(S(84.0f), textW + S(34.0f));
        modalButtonRects_[(size_t)i] = MakeRect(right - w, y, right, y + S(32.0f));
        right -= w + S(8.0f);
    }
    ResetBlink();
    Invalidate();
}

void App::FinishModal(int result) {
    if (!modalOpen_) return;
    modalOpen_ = false;
    auto callback = modalCallback_;
    modalCallback_ = nullptr;
    modalButtons_.clear();
    modalButtonRects_.clear();
    Invalidate();
    if (callback) callback(result);
    Invalidate();
}

void App::DrawModal() {
    if (!modalOpen_) return;
    renderer_.FillRect(MakeRect(0, 0, (float)width_, (float)height_), theme_.overlay);
    Rect card = modalCard_;
    renderer_.FillRoundedRect(OffsetRect(card, 0, S(8.0f)), S(12.0f), theme_.shadow);
    renderer_.FillRoundedRect(card, S(12.0f), theme_.panelBg);
    renderer_.StrokeRoundedRect(card, S(12.0f), theme_.border, 1.0f);

    renderer_.DrawEllipse(card.left + S(36.0f), card.top + S(38.0f), S(14.0f), S(14.0f),
                          theme_.accentSoft, true);
    renderer_.DrawEllipse(card.left + S(36.0f), card.top + S(38.0f), S(6.0f), S(6.0f),
                          theme_.accent, true);

    TextStyle title;
    title.size = S(16.0f);
    title.weight = 650;
    title.vAlign = 1;
    renderer_.Text(modalTitle_,
                   MakeRect(card.left + S(60.0f), card.top + S(20.0f),
                            card.right - S(22.0f), card.top + S(56.0f)),
                   theme_.text, title);

    TextStyle body;
    body.size = S(12.5f);
    body.wrap = true;
    body.ellipsis = false;
    renderer_.Text(modalMessage_,
                   MakeRect(card.left + S(24.0f), card.top + S(70.0f),
                            card.right - S(24.0f), card.bottom - S(64.0f)),
                   theme_.textDim, body);

    for (size_t i = 0; i < modalButtons_.size() && i < modalButtonRects_.size(); ++i) {
        const ModalButtonInfo& button = modalButtons_[i];
        const Rect& r = modalButtonRects_[i];
        bool hover = Contains(r, mouseX_, mouseY_);
        TextStyle style;
        style.size = S(12.0f);
        style.weight = 600;
        style.hAlign = 1;
        style.vAlign = 1;
        if (button.primary) {
            renderer_.FillRoundedRect(r, S(7.0f), hover ? theme_.accentHover : theme_.accent);
            renderer_.Text(button.label, r, Col(0x111218), style);
        } else {
            if (hover) renderer_.FillRoundedRect(r, S(7.0f), theme_.hover);
            renderer_.StrokeRoundedRect(r, S(7.0f), theme_.border, 1.0f);
            renderer_.Text(button.label, r, hover ? theme_.text : theme_.textDim, style);
        }
    }
}

// ------------------------------------------------------------------ overlays

Rect App::PaletteCardRect() const {
    float w = std::min(S(650.0f), (float)width_ - S(80.0f));
    int rows = std::clamp((int)paletteFiltered_.size(), 1, 10);
    float h = S(54.0f + rows * kPaletteRow + 34.0f);
    float left = ((float)width_ - w) * 0.5f;
    float top = std::max(S(64.0f), (float)height_ * 0.13f);
    return MakeRect(left, top, left + w, top + h);
}

Rect App::QuickCardRect() const {
    float w = std::min(S(720.0f), (float)width_ - S(80.0f));
    int rows = std::clamp((int)quickFiltered_.size(), 1, 9);
    float h = S(54.0f + rows * kQuickRow + 34.0f);
    float left = ((float)width_ - w) * 0.5f;
    float top = std::max(S(64.0f), (float)height_ * 0.12f);
    return MakeRect(left, top, left + w, top + h);
}

Rect App::GetGotoCardRect() const {
    float w = std::min(S(430.0f), (float)width_ - S(80.0f));
    float h = S(218.0f);
    float left = ((float)width_ - w) * 0.5f;
    float top = ((float)height_ - h) * 0.5f - S(40.0f);
    return MakeRect(left, top, left + w, top + h);
}

Rect App::GetPromptCardRect() const {
    float w = std::min(S(500.0f), (float)width_ - S(80.0f));
    float h = S(248.0f);
    float left = ((float)width_ - w) * 0.5f;
    float top = ((float)height_ - h) * 0.5f - S(30.0f);
    return MakeRect(left, top, left + w, top + h);
}

Rect App::GetAboutCardRect() const {
    float w = std::min(S(540.0f), (float)width_ - S(80.0f));
    float h = S(390.0f);
    float left = ((float)width_ - w) * 0.5f;
    float top = ((float)height_ - h) * 0.5f;
    return MakeRect(left, top, left + w, top + h);
}

SettingsRects App::GetSettingsRects() const {
    float w = std::min(S(650.0f), (float)width_ - S(70.0f));
    float h = std::min(S(600.0f), (float)height_ - S(70.0f));
    float left = ((float)width_ - w) * 0.5f;
    float top = ((float)height_ - h) * 0.5f;
    SettingsRects r;
    r.card = MakeRect(left, top, left + w, top + h);
    r.close = MakeRect(left + w - S(42.0f), top + S(14.0f),
                       left + w - S(14.0f), top + S(42.0f));
    float rowRight = left + w - S(30.0f);
    r.darkButton = MakeRect(rowRight - S(154.0f), top + S(94.0f),
                            rowRight - S(82.0f), top + S(124.0f));
    r.lightButton = MakeRect(rowRight - S(76.0f), top + S(94.0f),
                             rowRight, top + S(124.0f));
    r.languageEnglish = MakeRect(rowRight - S(154.0f), top + S(136.0f),
                                 rowRight - S(80.0f), top + S(164.0f));
    r.languageChinese = MakeRect(rowRight - S(74.0f), top + S(136.0f),
                                 rowRight, top + S(164.0f));
    r.fontMinus = MakeRect(rowRight - S(124.0f), top + S(192.0f),
                           rowRight - S(96.0f), top + S(220.0f));
    r.fontPlus = MakeRect(rowRight - S(42.0f), top + S(192.0f),
                          rowRight - S(14.0f), top + S(220.0f));
    r.wordWrapToggle = MakeRect(rowRight - S(48.0f), top + S(232.0f),
                                rowRight - S(8.0f), top + S(256.0f));
    r.hiddenToggle = MakeRect(rowRight - S(48.0f), top + S(304.0f),
                              rowRight - S(8.0f), top + S(328.0f));
    r.sessionToggle = MakeRect(rowRight - S(48.0f), top + S(340.0f),
                               rowRight - S(8.0f), top + S(364.0f));
    r.terminalUtf8Toggle = MakeRect(rowRight - S(48.0f), top + S(410.0f),
                                    rowRight - S(8.0f), top + S(434.0f));
    return r;
}

void App::DrawOverlay() {
    if (overlay_ == Overlay::None || overlay_ == Overlay::Find) return;
    renderer_.FillRect(MakeRect(0, 0, (float)width_, (float)height_), theme_.overlay);
    switch (overlay_) {
        case Overlay::Palette: DrawPalette(); break;
        case Overlay::QuickOpen: DrawQuickOpen(); break;
        case Overlay::Goto: DrawGotoLine(); break;
        case Overlay::Prompt: DrawPrompt(); break;
        case Overlay::Settings: DrawSettings(); break;
        case Overlay::About: DrawAbout(); break;
        default: break;
    }
}

void App::DrawPalette() {
    paletteCard_ = PaletteCardRect();
    renderer_.FillRoundedRect(OffsetRect(paletteCard_, 0, S(10.0f)), S(12.0f), theme_.shadow);
    renderer_.FillRoundedRect(paletteCard_, S(12.0f), theme_.panelBg);
    renderer_.StrokeRoundedRect(paletteCard_, S(12.0f), theme_.border, 1.0f);

    float left = paletteCard_.left;
    float top = paletteCard_.top;
    Rect inputRect = MakeRect(left + S(14.0f), top + S(10.0f),
                              paletteCard_.right - S(14.0f), top + S(44.0f));
    DrawTextField(paletteField_, inputRect, S(13.0f), theme_.text,
                  L"Type a command…", true);

    paletteRowRects_.clear();
    int visibleRows = std::clamp((int)paletteFiltered_.size(), 1, 10);
    float rowTop = top + S(54.0f);
    TextStyle label;
    label.size = S(12.0f);
    label.vAlign = 1;
    TextStyle category = label;
    category.size = S(10.0f);
    category.hAlign = 2;
    category.weight = 550;
    TextStyle shortcut = label;
    shortcut.size = S(10.0f);
    shortcut.hAlign = 2;

    for (int i = 0; i < visibleRows; ++i) {
        Rect row = MakeRect(left + S(8.0f), rowTop + i * S(kPaletteRow),
                            paletteCard_.right - S(8.0f), rowTop + (i + 1) * S(kPaletteRow));
        paletteRowRects_.push_back(row);
        int index = paletteOffset_ + i;
        bool hasItem = index >= 0 && index < (int)paletteFiltered_.size();
        if (!hasItem) {
            if (paletteFiltered_.empty() && i == 0) {
                renderer_.Text(L"No matching commands", row, theme_.textFaint, label);
            }
            continue;
        }
        bool selected = index == paletteSel_;
        bool hover = Contains(row, mouseX_, mouseY_);
        if (selected || hover) {
            renderer_.FillRoundedRect(Inset(row, S(2.0f)), S(6.0f),
                                      selected ? theme_.accentSoft : theme_.hover);
            if (selected) {
                renderer_.FillRoundedRect(MakeRect(row.left + S(2.0f), row.top + S(6.0f),
                                                    row.left + S(4.0f), row.bottom - S(6.0f)),
                                          S(1.0f), theme_.accent);
            }
        }
        const CommandInfo& info = commands_[(size_t)paletteFiltered_[(size_t)index]];
        renderer_.Text(info.label,
                       MakeRect(row.left + S(14.0f), row.top,
                                row.right - S(250.0f), row.bottom),
                       selected ? theme_.text : theme_.textDim, label);
        renderer_.Text(info.category,
                       MakeRect(row.right - S(244.0f), row.top,
                                row.right - S(116.0f), row.bottom),
                       theme_.textFaint, category);
        renderer_.Text(info.shortcut,
                       MakeRect(row.right - S(110.0f), row.top, row.right - S(12.0f), row.bottom),
                       theme_.textFaint, shortcut);
    }

    TextStyle footer;
    footer.size = S(10.0f);
    footer.vAlign = 1;
    renderer_.Text(L"↑↓ Navigate     Enter Run     Esc Close",
                   MakeRect(left + S(16.0f), paletteCard_.bottom - S(30.0f),
                            paletteCard_.right - S(16.0f), paletteCard_.bottom - S(8.0f)),
                   theme_.textFaint, footer);
}

void App::DrawQuickOpen() {
    quickCard_ = QuickCardRect();
    renderer_.FillRoundedRect(OffsetRect(quickCard_, 0, S(10.0f)), S(12.0f), theme_.shadow);
    renderer_.FillRoundedRect(quickCard_, S(12.0f), theme_.panelBg);
    renderer_.StrokeRoundedRect(quickCard_, S(12.0f), theme_.border, 1.0f);

    float left = quickCard_.left;
    float top = quickCard_.top;
    Rect inputRect = MakeRect(left + S(14.0f), top + S(10.0f),
                              quickCard_.right - S(14.0f), top + S(44.0f));
    DrawTextField(quickField_, inputRect, S(13.0f), theme_.text,
                  projectRoot_.empty() ? L"Type a file name…" : L"Search files by name…", true);

    quickRowRects_.clear();
    int visibleRows = std::clamp((int)quickFiltered_.size(), 1, 9);
    float rowTop = top + S(54.0f);
    TextStyle name;
    name.size = S(12.0f);
    name.weight = 550;
    name.vAlign = 1;
    name.uiText = false;
    TextStyle pathStyle;
    pathStyle.size = S(10.0f);
    pathStyle.vAlign = 1;
    pathStyle.hAlign = 2;
    pathStyle.uiText = false;

    for (int i = 0; i < visibleRows; ++i) {
        Rect row = MakeRect(left + S(8.0f), rowTop + i * S(kQuickRow),
                            quickCard_.right - S(8.0f), rowTop + (i + 1) * S(kQuickRow));
        quickRowRects_.push_back(row);
        int index = quickOffset_ + i;
        if (index < 0 || index >= (int)quickFiltered_.size()) {
            if (quickFiltered_.empty() && i == 0) {
                renderer_.Text(projectRoot_.empty() ? L"Open a folder to search its files"
                                                    : L"No matching files",
                               row, theme_.textFaint, name);
            }
            continue;
        }
        bool selected = index == quickSel_;
        bool hover = Contains(row, mouseX_, mouseY_);
        if (selected || hover) {
            renderer_.FillRoundedRect(Inset(row, S(2.0f)), S(6.0f),
                                      selected ? theme_.accentSoft : theme_.hover);
            if (selected) {
                renderer_.FillRoundedRect(MakeRect(row.left + S(2.0f), row.top + S(6.0f),
                                                    row.left + S(4.0f), row.bottom - S(6.0f)),
                                          S(1.0f), theme_.accent);
            }
        }
        const QuickFileInfo& file = quickFiles_[(size_t)quickFiltered_[(size_t)index]];
        TextStyle fileIcon;
        fileIcon.family = renderer_.IconFont();
        fileIcon.size = S(12.0f);
        fileIcon.hAlign = 1;
        fileIcon.vAlign = 1;
        renderer_.Text(L"\uE8A5",
                       MakeRect(row.left + S(10.0f), row.top, row.left + S(28.0f), row.bottom),
                       theme_.synType, fileIcon);
        renderer_.Text(file.name,
                       MakeRect(row.left + S(32.0f), row.top,
                                row.left + RectWidth(row) * 0.45f, row.bottom),
                       selected ? theme_.text : theme_.textDim, name);
        renderer_.Text(file.relative,
                       MakeRect(row.left + RectWidth(row) * 0.45f, row.top,
                                row.right - S(12.0f), row.bottom),
                       theme_.textFaint, pathStyle);
    }

    TextStyle footer;
    footer.size = S(10.0f);
    footer.vAlign = 1;
    renderer_.Text(L"↑↓ Navigate     Enter Open     Esc Close",
                   MakeRect(left + S(16.0f), quickCard_.bottom - S(30.0f),
                            quickCard_.right - S(16.0f), quickCard_.bottom - S(8.0f)),
                   theme_.textFaint, footer);
}

void App::DrawGotoLine() {
    gotoCard_ = GetGotoCardRect();
    renderer_.FillRoundedRect(OffsetRect(gotoCard_, 0, S(9.0f)), S(12.0f), theme_.shadow);
    renderer_.FillRoundedRect(gotoCard_, S(12.0f), theme_.panelBg);
    renderer_.StrokeRoundedRect(gotoCard_, S(12.0f), theme_.border, 1.0f);

    TextStyle title;
    title.size = S(15.0f);
    title.weight = 650;
    renderer_.Text(L"Go to Line",
                   MakeRect(gotoCard_.left + S(22.0f), gotoCard_.top + S(18.0f),
                            gotoCard_.right - S(22.0f), gotoCard_.top + S(46.0f)),
                   theme_.text, title);

    const Document* doc = ActiveDoc();
    TextStyle hint;
    hint.size = S(11.0f);
    int lineCount = doc ? doc->LineCount() : 1;
    renderer_.Text(L"Enter a line number between 1 and " + std::to_wstring(lineCount) + L".",
                   MakeRect(gotoCard_.left + S(22.0f), gotoCard_.top + S(46.0f),
                            gotoCard_.right - S(22.0f), gotoCard_.top + S(70.0f)),
                   theme_.textDim, hint);

    gotoInput_ = MakeRect(gotoCard_.left + S(22.0f), gotoCard_.top + S(80.0f),
                          gotoCard_.right - S(22.0f), gotoCard_.top + S(116.0f));
    DrawTextField(gotoField_, gotoInput_, S(13.0f), theme_.text, L"Line number", true);

    float btnY = gotoCard_.bottom - S(48.0f);
    gotoCancel_ = MakeRect(gotoCard_.right - S(176.0f), btnY,
                           gotoCard_.right - S(98.0f), btnY + S(30.0f));
    gotoAccept_ = MakeRect(gotoCard_.right - S(90.0f), btnY,
                           gotoCard_.right - S(22.0f), btnY + S(30.0f));
    TextStyle btn;
    btn.size = S(11.5f);
    btn.weight = 600;
    btn.hAlign = 1;
    btn.vAlign = 1;
    bool cancelHover = Contains(gotoCancel_, mouseX_, mouseY_);
    bool acceptHover = Contains(gotoAccept_, mouseX_, mouseY_);
    if (cancelHover) renderer_.FillRoundedRect(gotoCancel_, S(6.0f), theme_.hover);
    renderer_.StrokeRoundedRect(gotoCancel_, S(6.0f), theme_.border, 1.0f);
    renderer_.Text(L"Cancel", gotoCancel_, cancelHover ? theme_.text : theme_.textDim, btn);
    renderer_.FillRoundedRect(gotoAccept_, S(6.0f),
                              acceptHover ? theme_.accentHover : theme_.accent);
    renderer_.Text(L"Go", gotoAccept_, Col(0x111218), btn);
}

void App::DrawPrompt() {
    promptCard_ = GetPromptCardRect();
    renderer_.FillRoundedRect(OffsetRect(promptCard_, 0, S(9.0f)), S(12.0f), theme_.shadow);
    renderer_.FillRoundedRect(promptCard_, S(12.0f), theme_.panelBg);
    renderer_.StrokeRoundedRect(promptCard_, S(12.0f), theme_.border, 1.0f);

    TextStyle title;
    title.size = S(16.0f);
    title.weight = 650;
    renderer_.Text(prompt_.title,
                   MakeRect(promptCard_.left + S(22.0f), promptCard_.top + S(18.0f),
                            promptCard_.right - S(22.0f), promptCard_.top + S(48.0f)),
                   theme_.text, title);

    TextStyle label;
    label.size = S(11.5f);
    renderer_.Text(prompt_.label,
                   MakeRect(promptCard_.left + S(22.0f), promptCard_.top + S(56.0f),
                            promptCard_.right - S(22.0f), promptCard_.top + S(78.0f)),
                   theme_.textDim, label);

    promptInput_ = MakeRect(promptCard_.left + S(22.0f), promptCard_.top + S(84.0f),
                            promptCard_.right - S(22.0f), promptCard_.top + S(120.0f));
    DrawTextField(prompt_.field, promptInput_, S(13.0f), theme_.text, L"File name", true);

    if (!prompt_.error.empty()) {
        TextStyle error;
        error.size = S(11.0f);
        renderer_.Text(prompt_.error,
                       MakeRect(promptCard_.left + S(22.0f), promptCard_.top + S(126.0f),
                                promptCard_.right - S(22.0f), promptCard_.top + S(148.0f)),
                       theme_.error, error);
    }

    float btnY = promptCard_.bottom - S(50.0f);
    promptCancel_ = MakeRect(promptCard_.right - S(190.0f), btnY,
                             promptCard_.right - S(106.0f), btnY + S(31.0f));
    promptAccept_ = MakeRect(promptCard_.right - S(98.0f), btnY,
                             promptCard_.right - S(22.0f), btnY + S(31.0f));
    TextStyle btn;
    btn.size = S(11.5f);
    btn.weight = 600;
    btn.hAlign = 1;
    btn.vAlign = 1;
    bool cancelHover = Contains(promptCancel_, mouseX_, mouseY_);
    bool acceptHover = Contains(promptAccept_, mouseX_, mouseY_);
    if (cancelHover) renderer_.FillRoundedRect(promptCancel_, S(6.0f), theme_.hover);
    renderer_.StrokeRoundedRect(promptCancel_, S(6.0f), theme_.border, 1.0f);
    renderer_.Text(L"Cancel", promptCancel_, cancelHover ? theme_.text : theme_.textDim, btn);
    renderer_.FillRoundedRect(promptAccept_, S(6.0f),
                              acceptHover ? theme_.accentHover : theme_.accent);
    renderer_.Text(prompt_.confirmLabel, promptAccept_, Col(0x111218), btn);
}

void App::DrawSettings() {
    SettingsRects r = GetSettingsRects();
    const Rect& card = r.card;
    renderer_.FillRoundedRect(OffsetRect(card, 0, S(10.0f)), S(13.0f), theme_.shadow);
    renderer_.FillRoundedRect(card, S(13.0f), theme_.panelBg);
    renderer_.StrokeRoundedRect(card, S(13.0f), theme_.border, 1.0f);

    TextStyle title;
    title.size = S(19.0f);
    title.weight = 680;
    renderer_.Text(L"Settings",
                   MakeRect(card.left + S(28.0f), card.top + S(18.0f),
                            card.right - S(60.0f), card.top + S(52.0f)),
                   theme_.text, title);
    TextStyle subtitle;
    subtitle.size = S(11.0f);
    renderer_.Text(L"Appearance, editor behavior and terminal",
                   MakeRect(card.left + S(28.0f), card.top + S(48.0f),
                            card.right - S(60.0f), card.top + S(70.0f)),
                   theme_.textFaint, subtitle);

    TextStyle icon;
    icon.family = renderer_.IconFont();
    icon.size = S(16.0f);
    icon.hAlign = 1;
    icon.vAlign = 1;
    bool closeHover = Contains(r.close, mouseX_, mouseY_);
    if (closeHover) renderer_.FillRoundedRect(r.close, S(6.0f), theme_.hover);
    renderer_.Text(L"\uE711", r.close, closeHover ? theme_.text : theme_.textDim, icon);

    auto sectionTitle = [&](const wchar_t* text, float y) {
        TextStyle section;
        section.size = S(10.0f);
        section.weight = 700;
        section.vAlign = 1;
        renderer_.Text(text, MakeRect(card.left + S(28.0f), card.top + S(y),
                                      card.right - S(28.0f), card.top + y + S(20.0f)),
                       theme_.textFaint, section);
    };
    auto rowLabel = [&](const wchar_t* text, const wchar_t* hint, float y) {
        TextStyle label;
        label.size = S(12.0f);
        label.weight = 550;
        label.vAlign = 1;
        renderer_.Text(text, MakeRect(card.left + S(28.0f), card.top + y,
                                      card.right - S(80.0f), card.top + y + S(28.0f)),
                       theme_.text, label);
        if (hint) {
            TextStyle sub;
            sub.size = S(10.0f);
            sub.vAlign = 1;
            renderer_.Text(hint, MakeRect(card.left + S(28.0f), card.top + y + S(16.0f),
                                          card.right - S(80.0f), card.top + y + S(38.0f)),
                           theme_.textFaint, sub);
        }
    };
    auto toggle = [&](const Rect& rect, bool on) {
        Rect pill = MakeRect(rect.right - S(38.0f), (rect.top + rect.bottom) * 0.5f - S(10.0f),
                             rect.right, (rect.top + rect.bottom) * 0.5f + S(10.0f));
        renderer_.FillRoundedRect(pill, S(10.0f), ToggleTrack(on, theme_));
        float knobX = on ? pill.right - S(9.0f) : pill.left + S(9.0f);
        renderer_.DrawEllipse(knobX, (pill.top + pill.bottom) * 0.5f, S(7.0f), S(7.0f),
                              Col(0xFFFFFF), true);
    };
    auto divider = [&](float y) {
        renderer_.FillRect(MakeRect(card.left + S(28.0f), card.top + y,
                                    card.right - S(28.0f), card.top + y + 1.0f),
                           theme_.borderSoft);
    };

    sectionTitle(L"APPEARANCE", 82.0f);
    rowLabel(L"Theme", nullptr, 104.0f);
    auto themeButton = [&](const Rect& rect, const wchar_t* label, bool active, bool glyphDark) {
        bool hover = Contains(rect, mouseX_, mouseY_);
        renderer_.FillRoundedRect(rect, S(6.0f), active ? theme_.accentSoft : (hover ? theme_.hover : theme_.inputBg));
        renderer_.StrokeRoundedRect(rect, S(6.0f), active ? WithAlpha(theme_.accent, 0.55f) : theme_.border, 1.0f);
        TextStyle style;
        style.size = S(10.5f);
        style.weight = 550;
        style.hAlign = 1;
        style.vAlign = 1;
        renderer_.Text(label, rect, active ? theme_.accent : (hover ? theme_.text : theme_.textDim), style);
        (void)glyphDark;
    };
    themeButton(r.darkButton, L"Dark", darkTheme_, true);
    themeButton(r.lightButton, L"Light", !darkTheme_, false);
    TextStyle languageLabel;
    languageLabel.size = S(12.0f);
    languageLabel.weight = 550;
    languageLabel.vAlign = 1;
    renderer_.Text(L"Language",
                   MakeRect(card.left + S(28.0f), card.top + S(136.0f),
                            r.languageEnglish.left - S(10.0f), card.top + S(164.0f)),
                   theme_.text, languageLabel);
    themeButton(r.languageEnglish, L"English", !settings_.chineseUi, false);
    themeButton(r.languageChinese, L"中文", settings_.chineseUi, false);
    divider(174.0f);

    sectionTitle(L"EDITOR", 182.0f);
    rowLabel(L"Font size", L"Ctrl + mouse wheel also zooms", 206.0f);
    renderer_.FillRoundedRect(r.fontMinus, S(6.0f), theme_.inputBg);
    renderer_.StrokeRoundedRect(r.fontMinus, S(6.0f), theme_.border, 1.0f);
    renderer_.FillRoundedRect(r.fontPlus, S(6.0f), theme_.inputBg);
    renderer_.StrokeRoundedRect(r.fontPlus, S(6.0f), theme_.border, 1.0f);
    TextStyle smallBtn;
    smallBtn.size = S(15.0f);
    smallBtn.hAlign = 1;
    smallBtn.vAlign = 1;
    renderer_.Text(L"−", r.fontMinus, theme_.textDim, smallBtn);
    renderer_.Text(L"+", r.fontPlus, theme_.textDim, smallBtn);
    Rect fontValue = MakeRect(r.fontMinus.right + S(6.0f), r.fontMinus.top,
                              r.fontPlus.left - S(6.0f), r.fontMinus.bottom);
    TextStyle valueStyle;
    valueStyle.size = S(12.5f);
    valueStyle.weight = 600;
    valueStyle.hAlign = 1;
    valueStyle.vAlign = 1;
    renderer_.Text(std::to_wstring(settings_.editorFontSize), fontValue, theme_.text, valueStyle);
    rowLabel(L"Word wrap", nullptr, 232.0f);
    toggle(r.wordWrapToggle, settings_.wordWrap);
    divider(272.0f);

    sectionTitle(L"WORKSPACE", 284.0f);
    rowLabel(L"Show hidden files", nullptr, 306.0f);
    toggle(r.hiddenToggle, settings_.showHidden);
    rowLabel(L"Restore last session", nullptr, 342.0f);
    toggle(r.sessionToggle, settings_.restoreSession);
    divider(382.0f);

    sectionTitle(L"TERMINAL", 394.0f);
    rowLabel(L"UTF-8 output", L"Runs cmd with chcp 65001 so Chinese text displays correctly.",
             416.0f);
    toggle(r.terminalUtf8Toggle, settings_.terminalUtf8);

    TextStyle footer;
    footer.size = S(10.0f);
    footer.vAlign = 1;
    renderer_.Text(L"Changes are saved automatically.    Esc to close.",
                   MakeRect(card.left + S(28.0f), card.bottom - S(38.0f),
                            card.right - S(28.0f), card.bottom - S(16.0f)),
                   theme_.textFaint, footer);
}

void App::DrawAbout() {
    Rect card = GetAboutCardRect();
    renderer_.FillRoundedRect(OffsetRect(card, 0, S(10.0f)), S(13.0f), theme_.shadow);
    renderer_.FillRoundedRect(card, S(13.0f), theme_.panelBg);
    renderer_.StrokeRoundedRect(card, S(13.0f), theme_.border, 1.0f);

    DrawLogo(card.left + S(28.0f), card.top + S(28.0f), S(52.0f));
    TextStyle title;
    title.size = S(24.0f);
    title.weight = 700;
    title.vAlign = 1;
    renderer_.Text(L"NeoIDEs", MakeRect(card.left + S(94.0f), card.top + S(28.0f),
                                        card.right - S(28.0f), card.top + S(62.0f)),
                   theme_.text, title);
    TextStyle version;
    version.size = S(11.0f);
    version.vAlign = 1;
    renderer_.Text(L"Version 1.0.0", MakeRect(card.left + S(95.0f), card.top + S(60.0f),
                                             card.right - S(28.0f), card.top + S(82.0f)),
                   theme_.textFaint, version);

    renderer_.FillRect(MakeRect(card.left + S(28.0f), card.top + S(100.0f),
                                card.right - S(28.0f), card.top + S(101.0f)), theme_.borderSoft);

    TextStyle body;
    body.size = S(12.0f);
    body.wrap = true;
    body.ellipsis = false;
    renderer_.Text(L"A modern, lightweight IDE for C and C++, built with the native Windows API, Direct2D and DirectWrite. No Electron. No web runtime. Just a fast native editor.",
                   MakeRect(card.left + S(28.0f), card.top + S(116.0f),
                            card.right - S(28.0f), card.top + S(182.0f)),
                   theme_.textDim, body);
    TextStyle tech;
    tech.size = S(10.5f);
    tech.vAlign = 1;
    renderer_.Text(L"Win32  •  Direct2D  •  DirectWrite  •  C++20",
                   MakeRect(card.left + S(28.0f), card.top + S(190.0f),
                            card.right - S(28.0f), card.top + S(214.0f)),
                   theme_.accent, tech);

    TextStyle shortcutsTitle;
    shortcutsTitle.size = S(10.0f);
    shortcutsTitle.weight = 700;
    renderer_.Text(L"ESSENTIAL SHORTCUTS", MakeRect(card.left + S(28.0f), card.top + S(234.0f),
                                                   card.right - S(28.0f), card.top + S(252.0f)),
                   theme_.textFaint, shortcutsTitle);
    TextStyle shortcuts;
    shortcuts.size = S(10.5f);
    renderer_.Text(L"Ctrl+P  Quick open        Ctrl+Shift+P  Command palette\n"
                   L"Ctrl+F  Find in file      F5  Terminal         Ctrl+J  Toggle panel",
                   MakeRect(card.left + S(28.0f), card.top + S(256.0f),
                            card.right - S(28.0f), card.top + S(310.0f)),
                   theme_.textDim, shortcuts);

    Rect ok = MakeRect(card.right - S(108.0f), card.bottom - S(50.0f),
                       card.right - S(24.0f), card.bottom - S(18.0f));
    bool hover = Contains(ok, mouseX_, mouseY_);
    renderer_.FillRoundedRect(ok, S(6.0f), hover ? theme_.accentHover : theme_.accent);
    TextStyle okStyle;
    okStyle.size = S(11.5f);
    okStyle.weight = 650;
    okStyle.hAlign = 1;
    okStyle.vAlign = 1;
    renderer_.Text(L"Close", ok, Col(0x111218), okStyle);
}

// ------------------------------------------------------------------ overlay actions

void App::OpenPalette() {
    overlay_ = Overlay::Palette;
    paletteField_.SetText(L"");
    RefreshPaletteFilter();
    ResetBlink();
    Invalidate();
}

void App::OpenQuickOpen() {
    RefreshQuickIndex();
    overlay_ = Overlay::QuickOpen;
    quickField_.SetText(L"");
    RefreshQuickFilter();
    ResetBlink();
    Invalidate();
}

void App::OpenGotoLine() {
    if (!ActiveDoc()) return;
    overlay_ = Overlay::Goto;
    gotoField_.SetText(L"");
    GetGotoCardRect();
    gotoCard_ = GetGotoCardRect();
    gotoInput_ = MakeRect(gotoCard_.left + S(22.0f), gotoCard_.top + S(80.0f),
                          gotoCard_.right - S(22.0f), gotoCard_.top + S(116.0f));
    gotoCancel_ = MakeRect(gotoCard_.right - S(176.0f), gotoCard_.bottom - S(48.0f),
                           gotoCard_.right - S(98.0f), gotoCard_.bottom - S(18.0f));
    gotoAccept_ = MakeRect(gotoCard_.right - S(90.0f), gotoCard_.bottom - S(48.0f),
                           gotoCard_.right - S(22.0f), gotoCard_.bottom - S(18.0f));
    ResetBlink();
    Invalidate();
}

void App::OpenPrompt(const std::wstring& title, const std::wstring& label,
                     const std::wstring& initial, const std::wstring& confirmLabel,
                     std::function<bool(const std::wstring&)> onAccept) {
    overlay_ = Overlay::Prompt;
    prompt_.title = title;
    prompt_.label = label;
    prompt_.confirmLabel = confirmLabel;
    prompt_.error.clear();
    prompt_.field.SetText(initial);
    prompt_.field.SelectAll();
    prompt_.onAccept = std::move(onAccept);
    ResetBlink();
    Invalidate();
}

void App::OpenSettings() {
    overlay_ = Overlay::Settings;
    ResetBlink();
    Invalidate();
}

void App::CloseSettings() {
    if (overlay_ == Overlay::Settings) overlay_ = Overlay::None;
    Invalidate();
}

void App::CloseOverlay() {
    if (overlay_ == Overlay::Find) {
        CloseFind();
        return;
    }
    overlay_ = Overlay::None;
    ResetBlink();
    Invalidate();
}

int App::FuzzyScore(const std::wstring& text, const std::wstring& query, int* positions) const {
    if (query.empty()) return 0;
    std::wstring hay = ToLower(text);
    std::wstring needle = ToLower(query);
    size_t h = 0;
    int score = 0;
    int streak = 0;
    size_t matched = 0;
    for (size_t i = 0; i < needle.size(); ++i) {
        bool found = false;
        while (h < hay.size()) {
            bool boundary = h == 0 || hay[h - 1] == L' ' || hay[h - 1] == L'_' ||
                            hay[h - 1] == L'-' || hay[h - 1] == L'\\' || hay[h - 1] == L'/';
            if (hay[h] == needle[i]) {
                if (positions) positions[matched] = (int)h;
                ++matched;
                score += 10;
                if (h == 0 || boundary) score += 18;
                score += streak * 6;
                ++streak;
                ++h;
                found = true;
                break;
            }
            --score;
            streak = 0;
            ++h;
        }
        if (!found) return -1000000;
    }
    if (hay.find(needle) != std::wstring::npos) score += 40;
    score -= (int)hay.size() / 4;
    return score;
}

void App::RefreshPaletteFilter() {
    paletteFiltered_.clear();
    std::wstring query = paletteField_.text;
    std::vector<std::pair<int, int>> scored;
    scored.reserve(commands_.size());
    for (int i = 0; i < (int)commands_.size(); ++i) {
        const CommandInfo& info = commands_[(size_t)i];
        int scoreLabel = FuzzyScore(info.label, query);
        int scoreCategory = FuzzyScore(info.category, query);
        int score = std::max(scoreLabel, scoreCategory - 8);
        if (query.empty()) score = 1000 - i;
        if (score > -1000000) scored.push_back({score, i});
    }
    std::stable_sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
        if (a.first != b.first) return a.first > b.first;
        return a.second < b.second;
    });
    for (const auto& item : scored) paletteFiltered_.push_back(item.second);
    paletteSel_ = paletteFiltered_.empty() ? 0 : std::clamp(paletteSel_, 0, (int)paletteFiltered_.size() - 1);
    paletteOffset_ = 0;
    Invalidate();
}

void App::RefreshQuickFilter() {
    if (quickFilesDirty_) BuildQuickFiles();
    quickFiltered_.clear();
    std::wstring query = quickField_.text;
    std::vector<std::pair<int, int>> scored;
    scored.reserve(quickFiles_.size());
    for (int i = 0; i < (int)quickFiles_.size(); ++i) {
        const QuickFileInfo& file = quickFiles_[(size_t)i];
        int score = FuzzyScore(file.relative, query);
        int nameScore = FuzzyScore(file.name, query);
        if (nameScore > -1000000) score = std::max(score, nameScore + 30);
        if (query.empty()) score = -i;
        if (score > -1000000) scored.push_back({score, i});
    }
    std::stable_sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
        if (a.first != b.first) return a.first > b.first;
        return a.second < b.second;
    });
    for (const auto& item : scored) {
        quickFiltered_.push_back(item.second);
        if (quickFiltered_.size() >= 4000) break;
    }
    quickSel_ = quickFiltered_.empty() ? 0 : std::clamp(quickSel_, 0, (int)quickFiltered_.size() - 1);
    quickOffset_ = 0;
    Invalidate();
}

void App::ExecuteSelectedPaletteItem() {
    if (paletteSel_ < 0 || paletteSel_ >= (int)paletteFiltered_.size()) {
        CloseOverlay();
        return;
    }
    Cmd cmd = commands_[(size_t)paletteFiltered_[(size_t)paletteSel_]].cmd;
    CloseOverlay();
    RunCommand(cmd);
}

void App::ExecuteSelectedQuickFile() {
    if (quickSel_ < 0 || quickSel_ >= (int)quickFiltered_.size()) {
        CloseOverlay();
        return;
    }
    std::wstring path = quickFiles_[(size_t)quickFiltered_[(size_t)quickSel_]].path;
    CloseOverlay();
    OpenFile(path);
}

void App::ExecuteGotoLine() {
    Document* doc = ActiveDoc();
    if (!doc) {
        CloseOverlay();
        return;
    }
    int line = 1;
    try {
        size_t used = 0;
        line = std::stoi(gotoField_.text, &used);
        if (used == 0) line = 1;
    } catch (...) {
        line = 1;
    }
    line = std::clamp(line, 1, doc->LineCount());
    doc->EndEditGroup();
    doc->SetCaret(Cursor{line - 1, 0}, false);
    CloseOverlay();
    EditorEnsureCaretVisible();
    Invalidate();
}

void App::ExecutePrompt() {
    if (!prompt_.onAccept) {
        CloseOverlay();
        return;
    }
    if (prompt_.onAccept(prompt_.field.text)) {
        CloseOverlay();
    } else {
        Invalidate();
    }
}

// ------------------------------------------------------------------ overlay input

void App::OnOverlayKey(WPARAM key, bool ctrl, bool shift) {
    if (overlay_ == Overlay::Find) {
        HandleFindKey(key, ctrl, shift);
        return;
    }
    if (overlay_ == Overlay::Settings) {
        if (key == VK_ESCAPE) CloseSettings();
        return;
    }
    if (overlay_ == Overlay::About) {
        if (key == VK_ESCAPE || key == VK_RETURN || key == VK_SPACE) CloseOverlay();
        return;
    }
    if (key == VK_ESCAPE) {
        CloseOverlay();
        return;
    }
    if (overlay_ == Overlay::Palette) {
        if (key == VK_UP) {
            if (!paletteFiltered_.empty()) {
                paletteSel_ = std::max(0, paletteSel_ - 1);
                if (paletteSel_ < paletteOffset_) paletteOffset_ = paletteSel_;
            }
            return;
        }
        if (key == VK_DOWN) {
            if (!paletteFiltered_.empty()) {
                paletteSel_ = std::min((int)paletteFiltered_.size() - 1, paletteSel_ + 1);
                if (paletteSel_ >= paletteOffset_ + 10) paletteOffset_ = paletteSel_ - 9;
            }
            return;
        }
        if (key == VK_PRIOR || key == VK_NEXT) {
            int delta = key == VK_PRIOR ? -10 : 10;
            if (!paletteFiltered_.empty()) {
                paletteSel_ = std::clamp(paletteSel_ + delta, 0, (int)paletteFiltered_.size() - 1);
                paletteOffset_ = std::clamp(paletteSel_ - 4, 0,
                                            std::max(0, (int)paletteFiltered_.size() - 10));
            }
            return;
        }
        if (key == VK_RETURN) {
            ExecuteSelectedPaletteItem();
            return;
        }
        if (HandleTextFieldKey(paletteField_, key, ctrl, shift)) {
            RefreshPaletteFilter();
        }
        return;
    }
    if (overlay_ == Overlay::QuickOpen) {
        if (key == VK_UP) {
            if (!quickFiltered_.empty()) {
                quickSel_ = std::max(0, quickSel_ - 1);
                if (quickSel_ < quickOffset_) quickOffset_ = quickSel_;
            }
            return;
        }
        if (key == VK_DOWN) {
            if (!quickFiltered_.empty()) {
                quickSel_ = std::min((int)quickFiltered_.size() - 1, quickSel_ + 1);
                if (quickSel_ >= quickOffset_ + 9) quickOffset_ = quickSel_ - 8;
            }
            return;
        }
        if (key == VK_PRIOR || key == VK_NEXT) {
            int delta = key == VK_PRIOR ? -9 : 9;
            if (!quickFiltered_.empty()) {
                quickSel_ = std::clamp(quickSel_ + delta, 0, (int)quickFiltered_.size() - 1);
                quickOffset_ = std::clamp(quickSel_ - 4, 0,
                                          std::max(0, (int)quickFiltered_.size() - 9));
            }
            return;
        }
        if (key == VK_RETURN) {
            ExecuteSelectedQuickFile();
            return;
        }
        if (HandleTextFieldKey(quickField_, key, ctrl, shift)) {
            RefreshQuickFilter();
        }
        return;
    }
    if (overlay_ == Overlay::Goto) {
        if (key == VK_RETURN) {
            ExecuteGotoLine();
            return;
        }
        if (HandleTextFieldKey(gotoField_, key, ctrl, shift)) {
            gotoField_.text.erase(std::remove_if(gotoField_.text.begin(), gotoField_.text.end(),
                [](wchar_t c) { return c < L'0' || c > L'9'; }), gotoField_.text.end());
            gotoField_.Clamp();
            return;
        }
        return;
    }
    if (overlay_ == Overlay::Prompt) {
        if (key == VK_RETURN) {
            ExecutePrompt();
            return;
        }
        HandleTextFieldKey(prompt_.field, key, ctrl, shift);
        return;
    }
}

void App::OnOverlayChar(wchar_t ch) {
    if (ch < 32) return;
    OnOverlayText(std::wstring(1, ch));
}

void App::OnOverlayText(const std::wstring& text) {
    if (text.empty()) return;
    switch (overlay_) {
        case Overlay::Find:
            findField_.Insert(text);
            FindQueryChanged();
            break;
        case Overlay::Palette:
            paletteField_.Insert(text);
            RefreshPaletteFilter();
            break;
        case Overlay::QuickOpen:
            quickField_.Insert(text);
            RefreshQuickFilter();
            break;
        case Overlay::Goto:
            if (std::all_of(text.begin(), text.end(),
                            [](wchar_t ch) { return ch >= L'0' && ch <= L'9'; })) {
                gotoField_.Insert(text);
            }
            break;
        case Overlay::Prompt:
            prompt_.field.Insert(text);
            prompt_.error.clear();
            break;
        default:
            break;
    }
}

void App::OnOverlayMouseDown(float x, float y) {
    switch (overlay_) {
        case Overlay::Palette: {
            Rect card = paletteCard_.right > paletteCard_.left ? paletteCard_ : PaletteCardRect();
            if (!Contains(card, x, y)) {
                CloseOverlay();
                return;
            }
            for (size_t i = 0; i < paletteRowRects_.size(); ++i) {
                if (!Contains(paletteRowRects_[i], x, y)) continue;
                int index = paletteOffset_ + (int)i;
                if (index >= 0 && index < (int)paletteFiltered_.size()) {
                    paletteSel_ = index;
                    ExecuteSelectedPaletteItem();
                }
                return;
            }
            return;
        }
        case Overlay::QuickOpen: {
            Rect card = quickCard_.right > quickCard_.left ? quickCard_ : QuickCardRect();
            if (!Contains(card, x, y)) {
                CloseOverlay();
                return;
            }
            for (size_t i = 0; i < quickRowRects_.size(); ++i) {
                if (!Contains(quickRowRects_[i], x, y)) continue;
                int index = quickOffset_ + (int)i;
                if (index >= 0 && index < (int)quickFiltered_.size()) {
                    quickSel_ = index;
                    ExecuteSelectedQuickFile();
                }
                return;
            }
            return;
        }
        case Overlay::Goto: {
            Rect card = gotoCard_.right > gotoCard_.left ? gotoCard_ : GetGotoCardRect();
            if (!Contains(card, x, y)) {
                CloseOverlay();
                return;
            }
            if (Contains(gotoAccept_, x, y)) {
                ExecuteGotoLine();
            } else if (Contains(gotoCancel_, x, y)) {
                CloseOverlay();
            }
            return;
        }
        case Overlay::Prompt: {
            Rect card = promptCard_.right > promptCard_.left ? promptCard_ : GetPromptCardRect();
            if (!Contains(card, x, y)) {
                CloseOverlay();
                return;
            }
            if (Contains(promptAccept_, x, y)) {
                ExecutePrompt();
            } else if (Contains(promptCancel_, x, y)) {
                CloseOverlay();
            }
            return;
        }
        case Overlay::About: {
            Rect card = GetAboutCardRect();
            if (!Contains(card, x, y)) {
                CloseOverlay();
                return;
            }
            Rect ok = MakeRect(card.right - S(108.0f), card.bottom - S(50.0f),
                               card.right - S(24.0f), card.bottom - S(18.0f));
            if (Contains(ok, x, y)) CloseOverlay();
            return;
        }
        case Overlay::Settings: {
            SettingsRects r = GetSettingsRects();
            if (!Contains(r.card, x, y)) {
                CloseSettings();
                return;
            }
            if (Contains(r.close, x, y)) {
                CloseSettings();
                return;
            }
            if (Contains(r.darkButton, x, y)) {
                if (!darkTheme_) ApplyTheme(true);
                SaveSettings();
                return;
            }
            if (Contains(r.lightButton, x, y)) {
                if (darkTheme_) ApplyTheme(false);
                SaveSettings();
                return;
            }
            if (Contains(r.languageEnglish, x, y) || Contains(r.languageChinese, x, y)) {
                settings_.chineseUi = Contains(r.languageChinese, x, y);
                renderer_.SetChinese(settings_.chineseUi);
                SaveSettings();
                UpdateLayout();
                Invalidate();
                return;
            }
            if (Contains(r.fontMinus, x, y) || Contains(r.fontPlus, x, y)) {
                int delta = Contains(r.fontPlus, x, y) ? 1 : -1;
                settings_.editorFontSize = std::clamp(settings_.editorFontSize + delta, 9, 40);
                for (auto& doc : docs_) doc->InvalidateLayouts();
                UpdateEditorMetrics();
                SaveSettings();
                Invalidate();
                return;
            }
            if (Contains(r.wordWrapToggle, x, y)) {
                settings_.wordWrap = !settings_.wordWrap;
                for (auto& doc : docs_) doc->InvalidateLayouts();
                UpdateEditorMetrics();
                SaveSettings();
                Invalidate();
                return;
            }
            if (Contains(r.hiddenToggle, x, y)) {
                settings_.showHidden = !settings_.showHidden;
                if (!projectRoot_.empty()) {
                    tree_.SetRoot(projectRoot_, settings_.showHidden);
                    quickFilesDirty_ = true;
                }
                SaveSettings();
                Invalidate();
                return;
            }
            if (Contains(r.sessionToggle, x, y)) {
                settings_.restoreSession = !settings_.restoreSession;
                SaveSettings();
                Invalidate();
                return;
            }
            if (Contains(r.terminalUtf8Toggle, x, y)) {
                settings_.terminalUtf8 = !settings_.terminalUtf8;
                SaveSettings();
                if (terminalRunning_) {
                    StopProcess();
                    StartTerminal();
                }
                UpdateLayout();
                Invalidate();
                return;
            }
            return;
        }
        default:
            CloseOverlay();
            return;
    }
}

void App::UpdateHoverForMouse(float, float) {
    // Hover state is derived directly from mouseX_/mouseY_ while painting.
}
