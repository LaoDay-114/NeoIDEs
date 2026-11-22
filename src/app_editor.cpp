#include "app.h"

namespace {

std::wstring Spaces(int count) {
    return count > 0 ? std::wstring((size_t)count, L' ') : std::wstring();
}

int LeadingVisualSpaces(const std::wstring& text) {
    int col = 0;
    for (wchar_t c : text) {
        if (c == L' ') ++col;
        else if (c == L'\t') col += 4 - (col % 4);
        else break;
    }
    return col;
}

int LeadingDocChars(const std::wstring& text) {
    size_t i = 0;
    while (i < text.size() && (text[i] == L' ' || text[i] == L'\t')) ++i;
    return (int)i;
}

Color TokenColorFor(TokenKind kind, const ThemeColors& t) {
    switch (kind) {
        case TokenKind::Keyword: return t.synKeyword;
        case TokenKind::Type: return t.synType;
        case TokenKind::String: return t.synString;
        case TokenKind::Number: return t.synNumber;
        case TokenKind::Comment: return t.synComment;
        case TokenKind::Preproc: return t.synPreproc;
        case TokenKind::Function: return t.synFunction;
        case TokenKind::Macro: return t.synMacro;
        case TokenKind::Builtin: return t.synBuiltin;
        case TokenKind::Property: return t.synProperty;
        default: return t.text;
    }
}

bool BlinkOn(ULONGLONG resetTick, bool active) {
    if (!active) return true;
    return ((GetTickCount64() - resetTick) / 530) % 2 == 0;
}

std::vector<std::pair<int, int>> SelectedLines(const Document& doc) {
    std::vector<std::pair<int, int>> ranges;
    if (!doc.HasSelection()) return ranges;
    auto [a, b] = doc.SelectionRange();
    ranges.push_back({a.line, b.line});
    return ranges;
}

} // namespace

// ------------------------------------------------------------------ tabs

void App::DrawTabs() {
    if (docs_.empty()) return;
    UpdateLayout();
    ComputeTabs();
    renderer_.FillRect(L_.tabBar, theme_.tabBarBg);
    renderer_.FillRect(MakeRect(L_.tabBar.left, L_.tabBar.bottom - S(1.0f),
                                L_.tabBar.right, L_.tabBar.bottom), theme_.border);

    renderer_.PushClip(MakeRect(L_.tabBar.left, L_.tabBar.top, L_.tabBar.right, L_.tabBar.bottom));
    TextStyle titleStyle;
    titleStyle.size = S(12.0f);
    titleStyle.weight = 500;
    titleStyle.vAlign = 1;
    TextStyle iconStyle = titleStyle;
    iconStyle.family = renderer_.IconFont();
    iconStyle.size = S(13.0f);
    iconStyle.hAlign = 1;
    for (const auto& tab : tabViews_) {
        if (tab.doc < 0 || tab.doc >= (int)docs_.size()) continue;
        Document& doc = *docs_[(size_t)tab.doc];
        bool isActive = tab.doc == active_;
        bool hover = Contains(tab.rect, mouseX_, mouseY_);
        Rect body = MakeRect(tab.rect.left, tab.rect.top, tab.rect.right, tab.rect.bottom);
        if (isActive) {
            renderer_.FillRoundedRect(body, S(7.0f), theme_.tabActiveBg);
            renderer_.FillRect(MakeRect(body.left + S(10.0f), body.top,
                                        body.right - S(10.0f), body.top + S(2.0f)),
                               theme_.accent);
        } else if (hover) {
            renderer_.FillRoundedRect(body, S(7.0f), theme_.hover);
        }

        Rect iconRect = MakeRect(body.left + S(10.0f), body.top + S(8.0f),
                                 body.left + S(28.0f), body.bottom - S(8.0f));
        renderer_.DrawLanguageIcon(doc.language, iconRect, theme_.textFaint);

        Rect titleRect = MakeRect(body.left + S(30.0f), body.top,
                                  body.right - S(30.0f), body.bottom);
        TextStyle documentTitleStyle = titleStyle;
        documentTitleStyle.uiText = doc.name.empty();
        renderer_.Text(doc.name.empty() ? L"Untitled" : doc.name, titleRect,
                       isActive ? theme_.text : theme_.textDim, documentTitleStyle);

        bool closeHover = Contains(tab.close, mouseX_, mouseY_);
        if (closeHover) {
            renderer_.FillRoundedRect(Inset(tab.close, -S(2.0f)), S(5.0f), theme_.pressed);
            renderer_.Text(L"\uE711", tab.close, theme_.text, iconStyle);
        } else if (doc.dirty) {
            float cx = (tab.close.left + tab.close.right) * 0.5f;
            float cy = (tab.close.top + tab.close.bottom) * 0.5f;
            renderer_.DrawEllipse(cx, cy, S(3.0f), S(3.0f), theme_.accent, true);
        } else {
            renderer_.Text(L"\uE711", tab.close, theme_.textFaint, iconStyle);
        }
    }
    bool newHover = Contains(tabNewButton_, mouseX_, mouseY_);
    if (newHover) renderer_.FillRoundedRect(tabNewButton_, S(6.0f), theme_.hover);
    renderer_.Text(L"\uE710", tabNewButton_, newHover ? theme_.text : theme_.textDim, iconStyle);
    renderer_.PopClip();
}

// ------------------------------------------------------------------ editor metrics / paint

void App::UpdateVisibleScrollbars() {
    Document* doc = ActiveDoc();
    if (!doc) {
        showVScroll_ = false;
        showHScroll_ = false;
        return;
    }
    float scrollbar = S(12.0f);
    float contentH = doc->LineCount() * lineHeight_;
    float contentW = doc->maxColumns * monoCharWidth_ + S(36.0f);
    float baseW = std::max(0.0f, RectWidth(L_.editor) - gutterWidth_);
    float baseH = RectHeight(L_.editor);

    showVScroll_ = contentH > baseH + 1.0f;
    showHScroll_ = !settings_.wordWrap && contentW > baseW + 1.0f;
    float viewW = std::max(0.0f, baseW - (showVScroll_ ? scrollbar : 0.0f));
    float viewH = std::max(0.0f, baseH - (showHScroll_ ? scrollbar : 0.0f));
    showVScroll_ = contentH > viewH + 1.0f;
    showHScroll_ = !settings_.wordWrap && contentW > viewW + 1.0f;
    viewW = std::max(0.0f, baseW - (showVScroll_ ? scrollbar : 0.0f));
    viewH = std::max(0.0f, baseH - (showHScroll_ ? scrollbar : 0.0f));

    editorTextViewport_ = MakeRect(L_.editor.left + gutterWidth_, L_.editor.top,
                                   L_.editor.left + gutterWidth_ + viewW,
                                   L_.editor.top + viewH);

    if (showVScroll_) {
        vScrollTrack_ = MakeRect(L_.editor.right - scrollbar, L_.editor.top,
                                 L_.editor.right, L_.editor.top + viewH);
        float thumbH = std::max(S(32.0f), viewH * std::min(1.0f, viewH / std::max(1.0f, contentH)));
        float maxScroll = std::max(1.0f, contentH - viewH);
        float t = std::clamp(doc->scrollY / maxScroll, 0.0f, 1.0f);
        float thumbTop = vScrollTrack_.top + (viewH - thumbH) * t;
        vScrollThumb_ = MakeRect(vScrollTrack_.left + S(2.0f), thumbTop,
                                 vScrollTrack_.right - S(2.0f), thumbTop + thumbH);
    } else {
        vScrollTrack_ = Rect{};
        vScrollThumb_ = Rect{};
    }
    if (showHScroll_) {
        hScrollTrack_ = MakeRect(L_.editor.left + gutterWidth_, L_.editor.bottom - scrollbar,
                                 L_.editor.left + gutterWidth_ + viewW, L_.editor.bottom);
        float thumbW = std::max(S(32.0f), viewW * std::min(1.0f, viewW / std::max(1.0f, contentW)));
        float maxScroll = std::max(1.0f, contentW - viewW);
        float t = std::clamp(doc->scrollX / maxScroll, 0.0f, 1.0f);
        float thumbLeft = hScrollTrack_.left + (viewW - thumbW) * t;
        hScrollThumb_ = MakeRect(thumbLeft, hScrollTrack_.top + S(2.0f),
                                 thumbLeft + thumbW, hScrollTrack_.bottom - S(2.0f));
    } else {
        hScrollTrack_ = Rect{};
        hScrollThumb_ = Rect{};
    }
}

float App::LineTop(const Document& doc, int line) const {
    return L_.editor.top + line * lineHeight_ - doc.scrollY;
}

int App::DocColumnToDisplay(const Line& line, int col) {
    col = std::clamp(col, 0, (int)line.text.size());
    int display = 0;
    for (int i = 0; i < col; ++i) {
        if (line.text[(size_t)i] == L'\t') display += 4 - (display % 4);
        else display += TextCellWidth(line.text[(size_t)i]);
    }
    return display;
}

int App::DisplayColumnToDoc(const Line& line, int displayColumn) {
    displayColumn = std::max(0, displayColumn);
    int display = 0;
    for (int i = 0; i < (int)line.text.size(); ++i) {
        if (display >= displayColumn) return i;
        int width = 0;
        if (line.text[(size_t)i] == L'\t') {
            width = 4 - (display % 4);
        } else {
            width = TextCellWidth(line.text[(size_t)i]);
        }
        int next = display + width;
        if (displayColumn < next) return i;
        display = next;
    }
    return (int)line.text.size();
}

int App::DocColumnToTextIndex(const Line& line, int col) {
    col = std::clamp(col, 0, (int)line.text.size());
    int textIndex = 0;
    int displayColumn = 0;
    for (int i = 0; i < col; ++i) {
        wchar_t c = line.text[(size_t)i];
        if (c == L'\t') {
            int spaces = 4 - (displayColumn % 4);
            textIndex += spaces;
            displayColumn += spaces;
        } else {
            ++textIndex;
            displayColumn += TextCellWidth(c);
        }
    }
    return textIndex;
}

IDWriteTextLayout* App::GetLineLayout(Document& doc, int line) {
    if (line < 0 || line >= doc.LineCount()) return nullptr;
    Line& l = doc.lines[(size_t)line];
    if (l.layout && l.layoutFontSize == editorFontPx_) return l.layout;
    if (l.layout) l.ReleaseLayout();
    std::wstring display;
    display.reserve(l.text.size());
    for (wchar_t c : l.text) {
        if (c == L'\t') {
            int spaces = 4 - (int)(display.size() % 4);
            display.append((size_t)spaces, L' ');
        } else {
            display.push_back(c);
        }
    }
    TextStyle style;
    style.size = editorFontPx_;
    style.family = renderer_.MonoFont();
    style.weight = 400;
    style.ellipsis = false;
    style.vAlign = 1;
    style.uiText = false;
    l.layout = renderer_.CreateLayout(display, std::max(1.0f, (float)width_), lineHeight_, style);
    l.display = std::move(display);
    l.layoutFontSize = editorFontPx_;
    return l.layout;
}

float App::CaretXFor(Document& doc, int line, int col) {
    if (line < 0 || line >= doc.LineCount()) return editorTextViewport_.left;
    return editorTextViewport_.left - doc.scrollX +
           DocColumnToDisplay(doc.lines[(size_t)line], col) * monoCharWidth_;
}

void App::DrawEditor() {
    Document* doc = ActiveDoc();
    if (!doc) return;
    Rect editor = L_.editor;
    renderer_.FillRect(editor, theme_.editorBg);
    UpdateLayout();
    UpdateVisibleScrollbars();
    ClampScroll(*doc);

    float top = editor.top;
    float bottom = editor.bottom;
    float left = editor.left;
    float gutterRight = left + gutterWidth_;
    Rect gutter = MakeRect(left, top, gutterRight, bottom);
    renderer_.FillRect(gutter, theme_.gutterBg);
    renderer_.FillRect(MakeRect(gutterRight - S(1.0f), top, gutterRight, bottom), theme_.borderSoft);

    int firstLine = (int)std::floor(doc->scrollY / std::max(1.0f, lineHeight_));
    int visible = (int)std::ceil(RectHeight(editorTextViewport_) / std::max(1.0f, lineHeight_)) + 2;
    firstLine = std::max(0, firstLine);
    int lastLine = std::min(doc->LineCount() - 1, firstLine + visible);
    doc->EnsureTokenization(std::min(doc->LineCount() - 1, lastLine + 1));

    auto [selStart, selEnd] = doc->SelectionRange();
    bool hasSelection = doc->HasSelection();

    // Current-line highlight and gutter accent.
    {
        float y = LineTop(*doc, doc->caret.line);
        if (y + lineHeight_ > top && y < bottom) {
            renderer_.FillRect(MakeRect(gutterRight, y, editorTextViewport_.right, y + lineHeight_),
                               theme_.lineHighlight);
            renderer_.FillRect(MakeRect(left, y, left + S(3.0f), y + lineHeight_),
                               WithAlpha(theme_.accent, 0.65f));
        }
    }

    TextStyle mono;
    mono.size = editorFontPx_;
    mono.family = renderer_.MonoFont();
    mono.weight = 400;
    mono.ellipsis = false;
    mono.vAlign = 1;
    mono.uiText = false;

    TextStyle lineNumberStyle;
    lineNumberStyle.size = S(11.5f);
    lineNumberStyle.family = renderer_.MonoFont();
    lineNumberStyle.weight = 400;
    lineNumberStyle.hAlign = 2;
    lineNumberStyle.vAlign = 1;

    // Line numbers live in the gutter, outside the text viewport clip.
    for (int line = firstLine; line <= lastLine; ++line) {
        float y = LineTop(*doc, line);
        if (y + lineHeight_ < top || y > bottom) continue;
        std::wstring number = std::to_wstring(line + 1);
        renderer_.Text(number, MakeRect(left, y, gutterRight - S(12.0f), y + lineHeight_),
                       line == doc->caret.line ? theme_.lineNumberActive : theme_.lineNumber,
                       lineNumberStyle);
    }

    // Selection and find highlights are painted behind the glyphs.
    renderer_.PushClip(editorTextViewport_);
    if (hasSelection) {
        for (int line = std::max(firstLine, selStart.line);
             line <= std::min(lastLine, selEnd.line); ++line) {
            const Line& l = doc->lines[(size_t)line];
            int startCol = line == selStart.line ? selStart.col : 0;
            int endCol = line == selEnd.line ? selEnd.col : (int)l.text.size();
            if (line != selStart.line && startCol == 0 && endCol == 0 && line == selEnd.line) continue;
            int d0 = DocColumnToDisplay(l, startCol);
            int d1 = DocColumnToDisplay(l, endCol);
            if (line > selStart.line && endCol == (int)l.text.size()) d1 = std::max(d1, d0 + 1);
            float x0 = editorTextViewport_.left - doc->scrollX + d0 * monoCharWidth_;
            float x1 = editorTextViewport_.left - doc->scrollX + d1 * monoCharWidth_;
            float y = LineTop(*doc, line);
            if (x1 <= editorTextViewport_.left || x0 >= editorTextViewport_.right) continue;
            if (x1 - x0 < 2.0f) x1 = x0 + S(2.0f);
            renderer_.FillRect(MakeRect(std::max(x0, editorTextViewport_.left), y,
                                        std::min(x1, editorTextViewport_.right), y + lineHeight_),
                               theme_.selection);
        }
    }
    if (overlay_ == Overlay::Find && !findField_.text.empty()) {
        for (size_t i = 0; i < findMatches_.size(); ++i) {
            const FindMatch& match = findMatches_[i];
            if (match.line < firstLine || match.line > lastLine) continue;
            const Line& l = doc->lines[(size_t)match.line];
            int d0 = DocColumnToDisplay(l, match.start);
            int d1 = DocColumnToDisplay(l, match.end);
            float x0 = editorTextViewport_.left - doc->scrollX + d0 * monoCharWidth_;
            float x1 = editorTextViewport_.left - doc->scrollX + d1 * monoCharWidth_;
            float y = LineTop(*doc, match.line);
            Color color = (int)i == findCurrent_ ? WithAlpha(theme_.accent, 0.38f) : theme_.findMatch;
            renderer_.FillRoundedRect(MakeRect(x0, y + S(2.0f), x1, y + lineHeight_ - S(2.0f)),
                                      S(2.0f), color);
        }
    }

    for (int line = firstLine; line <= lastLine; ++line) {
        Line& l = doc->lines[(size_t)line];
        float y = LineTop(*doc, line);
        if (y + lineHeight_ < top || y > bottom) continue;

        // Indent guides, limited to the leading whitespace of the line.
        int leading = LeadingVisualSpaces(l.text);
        for (int guide = 4; guide < leading; guide += 4) {
            float gx = editorTextViewport_.left - doc->scrollX + guide * monoCharWidth_;
            if (gx < editorTextViewport_.left || gx > editorTextViewport_.right) continue;
            renderer_.FillRect(MakeRect(gx, y + S(2.0f), gx + 1.0f, y + lineHeight_ - S(2.0f)),
                               theme_.indentGuide);
        }

        // Split the expanded tab text into colored token segments.
        std::wstring display;
        display.reserve(l.text.size());
        for (wchar_t c : l.text) {
            if (c == L'\t') {
                int spaces = 4 - (int)(display.size() % 4);
                display.append((size_t)spaces, L' ');
            } else {
                display.push_back(c);
            }
        }
        float textLeft = editorTextViewport_.left - doc->scrollX;
        auto drawRange = [&](int index0, int index1, int cell0, int cell1, Color color) {
            if (index1 <= index0 || cell1 <= cell0) return;
            float x0 = textLeft + cell0 * monoCharWidth_;
            float x1 = textLeft + cell1 * monoCharWidth_;
            if (x1 < editorTextViewport_.left || x0 > editorTextViewport_.right) return;
            std::wstring piece = display.substr((size_t)index0, (size_t)(index1 - index0));
            renderer_.Text(piece, MakeRect(x0, y, x1 + S(4.0f), y + lineHeight_),
                           color, mono);
        };
        int cursorIndex = 0;
        int cursorCell = 0;
        for (const Token& token : l.tokens) {
            int tokenStart = std::clamp(token.start, 0, (int)l.text.size());
            int tokenEnd = std::clamp(token.start + token.len, 0, (int)l.text.size());
            int index0 = DocColumnToTextIndex(l, tokenStart);
            int index1 = DocColumnToTextIndex(l, tokenEnd);
            int cell0 = DocColumnToDisplay(l, tokenStart);
            int cell1 = DocColumnToDisplay(l, tokenEnd);
            if (index0 > cursorIndex) {
                drawRange(cursorIndex, index0, cursorCell, cell0, theme_.text);
            }
            drawRange(index0, index1, cell0, cell1, TokenColorFor(token.kind, theme_));
            cursorIndex = std::max(cursorIndex, index1);
            cursorCell = std::max(cursorCell, cell1);
        }
        if (cursorIndex < (int)display.size()) {
            drawRange(cursorIndex, (int)display.size(), cursorCell,
                      DocColumnToDisplay(l, (int)l.text.size()), theme_.text);
        }
    }

    // Caret.
    if (BlinkOn(blinkResetTick_, windowActive_)) {
        float cx = CaretXFor(*doc, doc->caret.line, doc->caret.col);
        float cy = LineTop(*doc, doc->caret.line);
        if (cx >= editorTextViewport_.left - S(1.0f) && cx <= editorTextViewport_.right &&
            cy + lineHeight_ >= top && cy <= bottom) {
            renderer_.FillRect(MakeRect(cx, cy + S(2.0f), cx + std::max(1.0f, S(1.6f)),
                                        cy + lineHeight_ - S(2.0f)), theme_.accent);
        }
    }
    renderer_.PopClip();

    // Scrollbars.
    if (showVScroll_) {
        bool hover = Contains(vScrollTrack_, mouseX_, mouseY_) || drag_ == DragMode::VScroll;
        if (hover) renderer_.FillRoundedRect(vScrollTrack_, S(6.0f), WithAlpha(theme_.hover, 0.7f));
        renderer_.FillRoundedRect(vScrollThumb_, S(6.0f),
                                  hover ? theme_.scrollbarHover : theme_.scrollbar);
    }
    if (showHScroll_) {
        bool hover = Contains(hScrollTrack_, mouseX_, mouseY_) || drag_ == DragMode::HScroll;
        if (hover) renderer_.FillRoundedRect(hScrollTrack_, S(6.0f), WithAlpha(theme_.hover, 0.7f));
        renderer_.FillRoundedRect(hScrollThumb_, S(6.0f),
                                  hover ? theme_.scrollbarHover : theme_.scrollbar);
    }

    if (overlay_ == Overlay::Find) DrawFindBar();
    if (completionVisible_) DrawCompletionPopup();
}

void App::DrawCompletionPopup() {
    Document* doc = ActiveDoc();
    if (!doc || completionItems_.empty()) return;

    const int total = (int)completionItems_.size();
    const int pageCount = std::max(1, (total + kCompletionPageSize - 1) / kCompletionPageSize);
    completionPage_ = std::clamp(completionPage_, 0, pageCount - 1);
    const int start = completionPage_ * kCompletionPageSize;
    const int visibleRows = std::min(kCompletionPageSize, total - start);

    const float rowHeight = S(23.0f);
    const float popupWidth = S(240.0f);
    const bool showRemaining = !completionPending_ && !completionRemaining_.empty();
    const float infoHeight = showRemaining ? S(18.0f) : 0.0f;
    const bool showPager = pageCount > 1;
    const float pagerHeight = showPager ? S(22.0f) : 0.0f;
    const float popupHeight = rowHeight * visibleRows + infoHeight + pagerHeight;

    float left = CaretXFor(*doc, doc->caret.line, doc->caret.col);
    float top = LineTop(*doc, doc->caret.line) + lineHeight_;
    if (left + popupWidth > editorTextViewport_.right) left = editorTextViewport_.right - popupWidth;
    if (top + popupHeight > L_.editor.bottom) {
        top = LineTop(*doc, doc->caret.line) - popupHeight;
    }
    left = std::clamp(left, L_.editor.left, std::max(L_.editor.left, L_.editor.right - popupWidth));
    top = std::clamp(top, L_.editor.top, std::max(L_.editor.top, L_.editor.bottom - popupHeight));
    completionRect_ = MakeRect(left, top, left + popupWidth, top + popupHeight);
    renderer_.FillRoundedRect(OffsetRect(completionRect_, 0, S(3.0f)), S(6.0f), theme_.shadow);
    renderer_.FillRoundedRect(completionRect_, S(6.0f), theme_.panelBg);
    renderer_.StrokeRoundedRect(completionRect_, S(6.0f), theme_.border, 1.0f);

    TextStyle item;
    item.size = S(11.5f);
    item.family = renderer_.MonoFont();
    item.vAlign = 1;
    item.uiText = false;
    completionRowRects_.clear();
    for (int i = 0; i < visibleRows; ++i) {
        Rect row = MakeRect(completionRect_.left + S(4.0f),
                            completionRect_.top + rowHeight * i,
                            completionRect_.right - S(4.0f),
                            completionRect_.top + rowHeight * (i + 1));
        completionRowRects_.push_back(row);
        bool selected = start + i == completionSelected_ && !completionPending_;
        if (selected) renderer_.FillRoundedRect(Inset(row, S(1.0f)), S(4.0f), theme_.accentSoft);
        renderer_.Text(completionItems_[(size_t)(start + i)],
                       MakeRect(row.left + S(8.0f), row.top, row.right - S(6.0f), row.bottom),
                       selected ? theme_.accent : theme_.text, item);
    }

    float footerTop = completionRect_.top + rowHeight * visibleRows;
    if (showRemaining) {
        renderer_.FillRect(MakeRect(completionRect_.left + S(6.0f), footerTop,
                                    completionRect_.right - S(6.0f), footerTop + S(1.0f)),
                           theme_.borderSoft);
        TextStyle info;
        info.size = S(9.5f);
        info.vAlign = 1;
        info.hAlign = 2;
        renderer_.Text(L"Remaining tokens: " + completionRemaining_,
                       MakeRect(completionRect_.left + S(6.0f), footerTop,
                                completionRect_.right - S(10.0f), footerTop + infoHeight),
                       theme_.textFaint, info);
        footerTop += infoHeight;
    }

    if (showPager) {
        renderer_.FillRect(MakeRect(completionRect_.left + S(6.0f), footerTop,
                                    completionRect_.right - S(6.0f), footerTop + S(1.0f)),
                           theme_.borderSoft);
        TextStyle nav;
        nav.size = S(13.0f);
        nav.hAlign = 1;
        nav.vAlign = 1;
        bool canPrev = completionPage_ > 0;
        bool canNext = completionPage_ < pageCount - 1;
        completionPrevRect_ = MakeRect(completionRect_.left + S(4.0f), footerTop + S(2.0f),
                                       completionRect_.left + S(26.0f), completionRect_.bottom - S(2.0f));
        completionNextRect_ = MakeRect(completionRect_.right - S(26.0f), footerTop + S(2.0f),
                                       completionRect_.right - S(4.0f), completionRect_.bottom - S(2.0f));
        renderer_.Text(L"\uE76B", completionPrevRect_,
                       canPrev ? theme_.text : theme_.textFaint, nav);
        renderer_.Text(L"\uE76C", completionNextRect_,
                       canNext ? theme_.text : theme_.textFaint, nav);
        TextStyle pageLabel;
        pageLabel.size = S(10.0f);
        pageLabel.hAlign = 1;
        pageLabel.vAlign = 1;
        renderer_.Text(std::to_wstring(completionPage_ + 1) + L" / " + std::to_wstring(pageCount),
                       MakeRect(completionRect_.left + S(30.0f), footerTop,
                                completionRect_.right - S(30.0f), completionRect_.bottom),
                       theme_.textDim, pageLabel);
    } else {
        completionPrevRect_ = Rect{};
        completionNextRect_ = Rect{};
    }
}

// ------------------------------------------------------------------ editing helpers

void App::EditorEnsureCaretVisible() {
    Document* doc = ActiveDoc();
    if (!doc) return;
    UpdateVisibleScrollbars();
    float viewH = std::max(1.0f, RectHeight(editorTextViewport_));
    float viewW = std::max(1.0f, RectWidth(editorTextViewport_));
    float caretTop = doc->caret.line * lineHeight_;
    float caretBottom = caretTop + lineHeight_;
    if (caretTop < doc->scrollY) {
        doc->scrollY = caretTop;
    } else if (caretBottom > doc->scrollY + viewH) {
        doc->scrollY = caretBottom - viewH;
    }
    float caretX = DocColumnToDisplay(doc->lines[(size_t)doc->caret.line], doc->caret.col) * monoCharWidth_;
    float margin = S(24.0f);
    if (caretX < doc->scrollX + margin) {
        doc->scrollX = std::max(0.0f, caretX - margin);
    } else if (caretX > doc->scrollX + viewW - margin) {
        doc->scrollX = caretX - viewW + margin;
    }
    ClampScroll(*doc);
}

void App::EditorMouseDown(float x, float y, int clicks) {
    Document* doc = ActiveDoc();
    if (!doc) return;
    doc->EndEditGroup();
    EditorHitTest(x, y, doc->caret, true);
    bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    if (clicks >= 2) {
        const std::wstring& text = doc->LineText(doc->caret.line);
        if (clicks >= 3) {
            doc->anchor = Cursor{doc->caret.line, 0};
            doc->caret.col = (int)text.size();
        } else if (IsWordChar(text.empty() ? L' ' : text[(size_t)std::min(doc->caret.col, (int)text.size() - 1)])) {
            int start = doc->caret.col;
            if (start > 0 && (start >= (int)text.size() || !IsWordChar(text[(size_t)start]))) --start;
            int wordStart = WordStart(text, start);
            int wordEnd = WordEnd(text, start);
            doc->anchor = Cursor{doc->caret.line, wordStart};
            doc->caret.col = wordEnd;
        } else {
            doc->anchor = doc->caret;
        }
    } else if (shift) {
        // Keep the anchor, move the caret.
    } else {
        doc->anchor = doc->caret;
    }
    doc->desiredCol = -1;
    drag_ = DragMode::EditorSelect;
    ResetBlink();
    Invalidate();
}

void App::EditorMouseMove(float x, float y) {
    Document* doc = ActiveDoc();
    if (!doc) return;
    Cursor target{};
    if (EditorHitTest(x, y, target, true)) {
        doc->caret = doc->ClampCursor(target);
        doc->desiredCol = -1;
        EditorEnsureCaretVisible();
        ResetBlink();
        Invalidate();
    }
}

void App::EditorMouseUp() {
    if (drag_ == DragMode::EditorSelect) {
        drag_ = DragMode::None;
        Invalidate();
    }
}

bool App::EditorHitTest(float x, float y, Cursor& out, bool clampOutside) {
    Document* doc = ActiveDoc();
    if (!doc) return false;
    float viewTop = L_.editor.top;
    float viewBottom = L_.editor.bottom;
    if (!clampOutside && (y < viewTop || y >= viewBottom)) return false;
    float localY = std::clamp(y - viewTop + doc->scrollY, 0.0f, std::max(0.0f, (doc->LineCount() - 1) * lineHeight_));
    int line = (int)std::floor(localY / std::max(1.0f, lineHeight_));
    line = std::clamp(line, 0, doc->LineCount() - 1);
    float localX = x - (L_.editor.left + gutterWidth_) + doc->scrollX;
    int displayCol = (int)std::floor(localX / std::max(1.0f, monoCharWidth_));
    if (localX < 0) displayCol = 0;
    out.line = line;
    out.col = DisplayColumnToDoc(doc->lines[(size_t)line], displayCol);
    if (!clampOutside) {
        if (x < L_.editor.left) return false;
        if (x > L_.editor.right) return false;
    }
    return true;
}

void App::EditorWheel(float, float, float delta, bool, bool shift) {
    Document* doc = ActiveDoc();
    if (!doc) return;
    float steps = delta / (float)WHEEL_DELTA;
    if (shift || settings_.wordWrap) {
        doc->scrollX = std::max(0.0f, doc->scrollX - steps * monoCharWidth_ * 6.0f);
    } else {
        doc->scrollY -= steps * lineHeight_ * 3.0f;
    }
    ClampScroll(*doc);
    Invalidate();
}

void App::EditorTypeCharacter(wchar_t ch) {
    Document* doc = ActiveDoc();
    if (!doc || ch < 32) return;
    bool wordChar = IsWordChar(ch) || ch == L'_';
    if (wordChar) completionClickDismissed_ = false; // a new word re-enables completions
    completionVisible_ = false;
    completionPending_ = false;
    // Reopen completions after the character is inserted on any success path.
    struct CompletionGuard {
        App* app;
        bool wordChar;
        ~CompletionGuard() { if (wordChar) app->AutoOpenCompletion(); }
    } guard{this, wordChar};
    doc->BeginEdit(true);

    auto wrapSelection = [&](wchar_t open, wchar_t close) -> bool {
        if (!doc->HasSelection()) return false;
        std::wstring selected = doc->SelectedText();
        auto [a, b] = doc->SelectionRange();
        doc->DeleteRaw(a, b);
        doc->InsertRaw(a, std::wstring(1, open) + selected + std::wstring(1, close));
        doc->SetSelection(Cursor{a.line, a.col + 1}, Cursor{a.line, a.col + 1 + (int)selected.size()});
        doc->MarkDirty();
        return true;
    };

    if (doc->HasSelection()) {
        if (ch == L'(') { if (wrapSelection(L'(', L')')) { EditorEnsureCaretVisible(); Invalidate(); return; } }
        if (ch == L'[') { if (wrapSelection(L'[', L']')) { EditorEnsureCaretVisible(); Invalidate(); return; } }
        if (ch == L'{') { if (wrapSelection(L'{', L'}')) { EditorEnsureCaretVisible(); Invalidate(); return; } }
        if (ch == L'"') { if (wrapSelection(L'"', L'"')) { EditorEnsureCaretVisible(); Invalidate(); return; } }
        if (ch == L'\'') { if (wrapSelection(L'\'', L'\'')) { EditorEnsureCaretVisible(); Invalidate(); return; } }
        doc->DeleteSelectionRaw();
        doc->MarkDirty();
    }

    const std::wstring& lineText = doc->LineText(doc->caret.line);
    wchar_t nextChar = doc->caret.col < (int)lineText.size() ? lineText[(size_t)doc->caret.col] : 0;
    if ((ch == L')' || ch == L']' || ch == L'}' || ch == L'"' || ch == L'\'') && nextChar == ch) {
        doc->SetCaret(Cursor{doc->caret.line, doc->caret.col + 1}, false);
        ResetBlink();
        Invalidate();
        return;
    }
    if (ch == L'}' && nextChar == L'}') {
        // Closing brace typed over an auto-inserted one: dedent the current line.
        int leading = LeadingDocChars(lineText);
        if (leading >= 4 && lineText.find_first_not_of(L" \t") == std::wstring::npos) {
            doc->BeginEdit(false);
            doc->DeleteRaw(Cursor{doc->caret.line, 0}, Cursor{doc->caret.line, leading});
            doc->InsertRaw(Cursor{doc->caret.line, 0}, Spaces(leading - 4));
            doc->SetCaret(Cursor{doc->caret.line, leading - 4}, false);
            doc->MarkDirty();
            doc->SetCaret(Cursor{doc->caret.line, doc->caret.col + 1}, false);
            Invalidate();
            return;
        }
    }

    std::wstring insert(1, ch);
    if (ch == L'(' || ch == L'[' || ch == L'{' || ch == L'"' || ch == L'\'') {
        wchar_t close = ch == L'(' ? L')' : ch == L'[' ? L']' : ch == L'{' ? L'}' : ch;
        insert.push_back(close);
        doc->InsertRaw(doc->caret, insert);
        doc->SetCaret(Cursor{doc->caret.line, doc->caret.col - 1}, false);
    } else {
        doc->InsertRaw(doc->caret, insert);
    }
    EditorEnsureCaretVisible();
    ResetBlink();
    Invalidate();
}

void App::OpenCompletion() {
    Document* doc = ActiveDoc();
    completionVisible_ = false;
    completionPending_ = false;
    completionItems_.clear();
    if (!doc) return;
    const std::wstring& line = doc->LineText(doc->caret.line);
    int start = std::clamp(doc->caret.col, 0, (int)line.size());
    while (start > 0 && (IsWordChar(line[(size_t)start - 1]) || line[(size_t)start - 1] == L'_')) {
        --start;
    }
    if (start == doc->caret.col) return;
    completionStartCol_ = start;
    std::wstring prefix = line.substr((size_t)start, (size_t)(doc->caret.col - start));
    completionRemaining_.clear();
    completionSelected_ = 0;
    completionPage_ = 0;
    completionClickDismissed_ = false;

    if (settings_.aiCompletionEnabled) {
        // AI mode: kick off an async request; results are applied in OnTimer.
        StartAiCompletion();
        return;
    }

    // Built-in mode: completions come from the embedded resource database.
    completionItems_ = BuildCompletions(doc->Content(), doc->language, prefix);
    if (completionItems_.empty()) return;
    completionPending_ = false;
    completionVisible_ = true;
    Invalidate();
}

void App::AutoOpenCompletion() {
    if (completionClickDismissed_) return; // stay hidden until the next typed word
    if (modalOpen_ || overlay_ != Overlay::None) return;
    OpenCompletion();
}

bool App::HandleCompletionMouseDown(float x, float y) {
    if (!completionVisible_ || completionPending_) return false;
    if (!Contains(completionRect_, x, y)) {
        // A click elsewhere in the editor dismisses the popup until the next word.
        if (Contains(L_.editor, x, y)) {
            completionVisible_ = false;
            completionClickDismissed_ = true;
            Invalidate();
        }
        return false;
    }
    if (completionPrevRect_.right > completionPrevRect_.left &&
        Contains(completionPrevRect_, x, y)) {
        NavigateCompletionPage(-1);
        return true;
    }
    if (completionNextRect_.right > completionNextRect_.left &&
        Contains(completionNextRect_, x, y)) {
        NavigateCompletionPage(1);
        return true;
    }
    for (size_t i = 0; i < completionRowRects_.size(); ++i) {
        if (!Contains(completionRowRects_[i], x, y)) continue;
        completionSelected_ = completionPage_ * kCompletionPageSize + (int)i;
        AcceptCompletion();
        return true;
    }
    return true; // click inside popup but not on an item: swallow it
}

bool App::NavigateCompletionPage(int delta) {
    if (!completionVisible_ || completionPending_ || completionItems_.empty()) return false;
    int pageCount = std::max(1, ((int)completionItems_.size() + kCompletionPageSize - 1) /
                                    kCompletionPageSize);
    int next = std::clamp(completionPage_ + delta, 0, pageCount - 1);
    if (next == completionPage_) return true;
    completionPage_ = next;
    completionSelected_ = completionPage_ * kCompletionPageSize;
    Invalidate();
    return true;
}

bool App::WarnAiUrlMismatch(std::function<void(int)> onResolved) {
    int embedded = AiPathProtocol(settings_.aiBaseUrl);
    if (embedded < 0 || embedded == settings_.aiProtocol) return false;
    if (modalOpen_) return false;

    std::wstring fixed = BuildAiEndpoint(AiUrlRoot(settings_.aiBaseUrl),
                                         (AiProtocol)settings_.aiProtocol);
    std::wstring message =
        L"The Base URL already contains the path of another protocol.\n\n" +
        std::wstring(L"Base URL: ") + settings_.aiBaseUrl + L"\n" +
        L"Selected protocol: " + AiProtocolName((AiProtocol)settings_.aiProtocol) + L"\n\n" +
        L"Choose \"Fix URL\" to use:\n" + fixed;

    ShowModal(L"Base URL / Protocol Mismatch", message,
              {{L"Cancel", 0, false}, {L"Continue Anyway", 1, false}, {L"Fix URL", 2, true}},
              2, 0,
              [this, onResolved](int result) {
                  if (result == 2) ApplyAiUrlFix();
                  if (onResolved) onResolved(result);
              });
    return true;
}

void App::ApplyAiUrlFix() {
    settings_.aiBaseUrl = BuildAiEndpoint(AiUrlRoot(settings_.aiBaseUrl),
                                          (AiProtocol)settings_.aiProtocol);
    aiBaseUrlField_.SetText(settings_.aiBaseUrl);
    SyncAiSettingsFields();
    SaveSettings();
    if (settingsTab_ == 1) Invalidate();
}

void App::TestAiModel() {
    AiSettings settings;
    settings.enabled = settings_.aiCompletionEnabled;
    settings.baseUrl = settings_.aiBaseUrl;
    settings.apiKey = settings_.aiApiKey;
    settings.modelId = settings_.aiModelId;
    settings.protocol = (AiProtocol)settings_.aiProtocol;

    if (settings.apiKey.empty() || settings.modelId.empty() || settings.baseUrl.empty()) {
        SetStatus(L"AI model test failed: Base URL, API Key and Model ID are required.", 2);
        return;
    }
    SetStatus(L"Testing AI model…", 0);

    std::thread([this, settings]() {
        AiCompletionResult result = RequestAiCompletion(settings, L"int ", L"", Language::Cpp);
        // Status updates are marshalled to the UI thread through a timer flag.
        std::lock_guard<std::mutex> lock(aiMutex_);
        aiTestDone_ = true;
        aiTestOk_ = result.ok;
        aiTestMessage_ = result.ok
            ? L"AI model OK: " + settings.modelId
            : L"AI model test failed: " + result.error;
    }).detach();
}

void App::StartAiCompletion(bool skipMismatchCheck) {
    Document* doc = ActiveDoc();
    if (!doc) return;
    if (aiBusy_.load()) return; // a request is already in flight

    const std::wstring& line = doc->LineText(doc->caret.line);
    std::wstring prefix = line.substr((size_t)completionStartCol_,
                                      (size_t)std::max(0, doc->caret.col - completionStartCol_));
    std::wstring suffix = doc->caret.col < (int)line.size()
                              ? line.substr((size_t)doc->caret.col) : L"";

    AiSettings settings;
    settings.enabled = settings_.aiCompletionEnabled;
    settings.baseUrl = settings_.aiBaseUrl;
    settings.apiKey = settings_.aiApiKey;
    settings.modelId = settings_.aiModelId;
    settings.protocol = (AiProtocol)settings_.aiProtocol;

    if (settings.apiKey.empty() || settings.modelId.empty() || settings.baseUrl.empty()) {
        SetStatus(L"AI completion is not configured. Check Settings > AI.", 2);
        completionItems_ = BuildCompletions(doc->Content(), doc->language, prefix);
        completionVisible_ = !completionItems_.empty();
        completionPending_ = false;
        Invalidate();
        return;
    }

    if (!skipMismatchCheck && WarnAiUrlMismatch([this](int result) {
            if (result == 0) { // cancelled: drop the pending completion
                completionVisible_ = false;
                completionPending_ = false;
                Invalidate();
                return;
            }
            StartAiCompletion(true); // continue with original or corrected URL
        })) {
        // A modal is open; the callback resumes this request.
        completionItems_ = {L"AI completing…"};
        completionVisible_ = true;
        completionPending_ = true;
        Invalidate();
        return;
    }

    if (aiWorker_.joinable()) aiWorker_.join();
    ++aiRequestId_;
    {
        std::lock_guard<std::mutex> lock(aiMutex_);
        aiPendingSettings_ = settings;
        aiPendingPrefix_ = prefix;
        aiPendingSuffix_ = suffix;
        aiPendingLanguage_ = doc->language;
        aiResultReady_ = false;
    }
    aiBusy_.store(true);
    completionPending_ = true;
    completionRemaining_.clear();
    completionItems_ = {L"AI completing…"};
    completionVisible_ = true;
    Invalidate();

    unsigned long long requestId = aiRequestId_;
    aiWorker_ = std::thread([this, requestId]() {
        AiSettings workerSettings;
        std::wstring workerPrefix, workerSuffix;
        Language workerLanguage = Language::Plain;
        {
            std::lock_guard<std::mutex> lock(aiMutex_);
            workerSettings = aiPendingSettings_;
            workerPrefix = aiPendingPrefix_;
            workerSuffix = aiPendingSuffix_;
            workerLanguage = aiPendingLanguage_;
        }
        AiCompletionResult result = RequestAiCompletion(workerSettings, workerPrefix,
                                                        workerSuffix, workerLanguage);
        {
            std::lock_guard<std::mutex> lock(aiMutex_);
            if (requestId == aiRequestId_) {
                aiPendingResult_ = std::move(result);
                aiResultReady_ = true;
            }
        }
        aiBusy_.store(false);
    });
}

void App::PollAiCompletion() {
    if (!completionPending_) return;
    {
        std::lock_guard<std::mutex> lock(aiMutex_);
        if (!aiResultReady_) return;
        aiResultReady_ = false;
        if (aiCompletedId_ == aiRequestId_) return;
        aiCompletedId_ = aiRequestId_;

        completionPending_ = false;
        if (aiPendingResult_.ok) {
            completionItems_ = aiPendingResult_.suggestions;
            completionRemaining_ = aiPendingResult_.remainingTokens;
            completionSelected_ = 0;
            completionVisible_ = !completionItems_.empty();
            SetStatus(L"AI completion ready", 1);
        } else {
            // Fall back to built-in completions when the API fails.
            Document* doc = ActiveDoc();
            if (doc) {
                completionItems_ = BuildCompletions(doc->Content(), doc->language, aiPendingPrefix_);
                completionVisible_ = !completionItems_.empty();
            }
            completionRemaining_.clear();
            SetStatus(L"AI completion failed: " + aiPendingResult_.error, 2);
        }
    }
    Invalidate();
}

void App::AcceptCompletion() {
    Document* doc = ActiveDoc();
    if (!completionVisible_ || completionPending_ || !doc || completionSelected_ < 0 ||
        completionSelected_ >= (int)completionItems_.size()) {
        completionVisible_ = false;
        completionPending_ = false;
        return;
    }
    const std::wstring replacement = completionItems_[(size_t)completionSelected_];
    Cursor start{doc->caret.line, completionStartCol_};
    doc->BeginEdit(false);
    doc->DeleteRaw(start, doc->caret);
    doc->InsertRaw(start, replacement);
    doc->SetCaret(Cursor{start.line, start.col + (int)replacement.size()}, false);
    doc->MarkDirty();
    completionVisible_ = false;
    completionPending_ = false;
    EditorEnsureCaretVisible();
    ResetBlink();
    Invalidate();
}

bool App::HandleCompletionKey(WPARAM key) {
    if (!completionVisible_) return false;
    if (key == VK_ESCAPE) {
        completionVisible_ = false;
        completionPending_ = false;
        Invalidate();
        return true;
    }
    if (completionPending_) return true; // ignore navigation while waiting for AI

    if (key == VK_UP || key == VK_DOWN) {
        int delta = key == VK_UP ? -1 : 1;
        completionSelected_ = (completionSelected_ + delta + (int)completionItems_.size()) %
                              (int)completionItems_.size();
        completionPage_ = completionSelected_ / kCompletionPageSize;
        Invalidate();
        return true;
    }
    if (key == VK_LEFT || key == VK_RIGHT) {
        return NavigateCompletionPage(key == VK_LEFT ? -1 : 1);
    }
    if (key == VK_OEM_3) {
        suppressNextBacktickChar_ = true; // its WM_CHAR must not insert a backtick
        AcceptCompletion();
        return true;
    }
    if (key == VK_RETURN || key == VK_TAB) {
        AcceptCompletion();
        return true;
    }
    return false;
}

bool App::EditorKeyDown(WPARAM key, bool ctrl, bool shift, bool) {
    Document* doc = ActiveDoc();
    if (!doc) return false;
    switch (key) {
        case VK_LEFT:
            EditorMoveHorizontal(-1, shift, ctrl);
            return true;
        case VK_RIGHT:
            EditorMoveHorizontal(1, shift, ctrl);
            return true;
        case VK_UP:
            EditorMoveVertical(-1, shift);
            return true;
        case VK_DOWN:
            EditorMoveVertical(1, shift);
            return true;
        case VK_HOME:
            EditorHome(shift, ctrl);
            return true;
        case VK_END:
            EditorEnd(shift, ctrl);
            return true;
        case VK_PRIOR:
            doc->EndEditGroup();
            doc->SetCaret(Cursor{std::max(0, doc->caret.line - VisibleLineCount()), doc->caret.col}, shift);
            doc->desiredCol = -1;
            EditorEnsureCaretVisible();
            Invalidate();
            return true;
        case VK_NEXT:
            doc->EndEditGroup();
            doc->SetCaret(Cursor{std::min(doc->LineCount() - 1,
                                          doc->caret.line + VisibleLineCount()), doc->caret.col}, shift);
            doc->desiredCol = -1;
            EditorEnsureCaretVisible();
            Invalidate();
            return true;
        case VK_BACK:
            EditorBackspace();
            return true;
        case VK_DELETE:
            EditorDelete();
            return true;
        case VK_RETURN:
            EditorEnter();
            return true;
        case VK_TAB:
            EditorTab(shift);
            return true;
        default:
            break;
    }
    return false;
}

void App::EditorBackspace() {
    Document* doc = ActiveDoc();
    if (!doc) return;
    doc->EndEditGroup();
    if (doc->HasSelection()) {
        auto [a, b] = doc->SelectionRange();
        doc->BeginEdit(false);
        doc->DeleteRaw(a, b);
        doc->MarkDirty();
        EditorEnsureCaretVisible();
        Invalidate();
        return;
    }
    Cursor caret = doc->caret;
    bool word = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    if (caret.col == 0) {
        if (caret.line == 0) return;
        doc->BeginEdit(false);
        int previousLen = (int)doc->LineText(caret.line - 1).size();
        doc->DeleteRaw(Cursor{caret.line - 1, previousLen}, Cursor{caret.line, 0});
        doc->SetCaret(Cursor{caret.line - 1, previousLen}, false);
        doc->MarkDirty();
    } else {
        int target = caret.col - 1;
        const std::wstring& line = doc->LineText(caret.line);
        if (target > 0 && IsLowSurrogate(line[(size_t)target]) &&
            IsHighSurrogate(line[(size_t)target - 1])) --target;
        if (word) target = WordStart(doc->LineText(caret.line), caret.col - 1);
        doc->BeginEdit(false);
        doc->DeleteRaw(Cursor{caret.line, target}, Cursor{caret.line, caret.col});
        doc->MarkDirty();
    }
    EditorEnsureCaretVisible();
    Invalidate();
}

void App::EditorDelete() {
    Document* doc = ActiveDoc();
    if (!doc) return;
    doc->EndEditGroup();
    if (doc->HasSelection()) {
        auto [a, b] = doc->SelectionRange();
        doc->BeginEdit(false);
        doc->DeleteRaw(a, b);
        doc->MarkDirty();
        EditorEnsureCaretVisible();
        Invalidate();
        return;
    }
    Cursor caret = doc->caret;
    if (caret.line >= doc->LineCount() - 1 && caret.col >= (int)doc->LineText(caret.line).size()) return;
    doc->BeginEdit(false);
    if (caret.col >= (int)doc->LineText(caret.line).size()) {
        doc->DeleteRaw(Cursor{caret.line, caret.col}, Cursor{caret.line + 1, 0});
        doc->SetCaret(caret, false);
    } else {
        bool word = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        int target = caret.col + 1;
        const std::wstring& line = doc->LineText(caret.line);
        if (caret.col + 1 < (int)line.size() &&
            IsHighSurrogate(line[(size_t)caret.col]) &&
            IsLowSurrogate(line[(size_t)caret.col + 1])) ++target;
        if (word) target = WordEnd(doc->LineText(caret.line), caret.col + 1);
        doc->DeleteRaw(Cursor{caret.line, caret.col}, Cursor{caret.line, target});
        doc->SetCaret(caret, false);
    }
    doc->MarkDirty();
    EditorEnsureCaretVisible();
    Invalidate();
}

void App::EditorMoveHorizontal(int direction, bool extend, bool word) {
    Document* doc = ActiveDoc();
    if (!doc) return;
    doc->EndEditGroup();
    Cursor caret = doc->caret;
    if (!extend && doc->HasSelection()) {
        auto [a, b] = doc->SelectionRange();
        Cursor target = direction < 0 ? a : b;
        doc->SetCaret(target, false);
        EditorEnsureCaretVisible();
        Invalidate();
        return;
    }
    if (word) {
        if (direction < 0) {
            if (caret.col == 0) {
                if (caret.line > 0) {
                    int previousLen = (int)doc->LineText(caret.line - 1).size();
                    caret = Cursor{caret.line - 1, previousLen};
                }
            } else {
                caret.col = WordStart(doc->LineText(caret.line), caret.col - 1);
            }
        } else {
            if (caret.col >= (int)doc->LineText(caret.line).size()) {
                if (caret.line < doc->LineCount() - 1) caret = Cursor{caret.line + 1, 0};
            } else {
                caret.col = WordEnd(doc->LineText(caret.line), caret.col);
            }
        }
    } else {
        if (direction < 0) {
            if (caret.col > 0) {
                --caret.col;
                const std::wstring& line = doc->LineText(caret.line);
                if (caret.col > 0 && IsLowSurrogate(line[(size_t)caret.col]) &&
                    IsHighSurrogate(line[(size_t)caret.col - 1])) --caret.col;
            }
            else if (caret.line > 0) caret = Cursor{caret.line - 1, (int)doc->LineText(caret.line - 1).size()};
        } else {
            const std::wstring& line = doc->LineText(caret.line);
            if (caret.col < (int)line.size()) {
                ++caret.col;
                if (caret.col < (int)line.size() &&
                    IsHighSurrogate(line[(size_t)caret.col - 1]) &&
                    IsLowSurrogate(line[(size_t)caret.col])) ++caret.col;
            }
            else if (caret.line < doc->LineCount() - 1) caret = Cursor{caret.line + 1, 0};
        }
    }
    doc->SetCaret(caret, extend);
    doc->desiredCol = -1;
    EditorEnsureCaretVisible();
    ResetBlink();
    Invalidate();
}

void App::EditorMoveVertical(int direction, bool extend) {
    Document* doc = ActiveDoc();
    if (!doc) return;
    doc->EndEditGroup();
    int desired = doc->desiredCol;
    const Line& current = doc->lines[(size_t)doc->caret.line];
    if (desired < 0) desired = DocColumnToDisplay(current, doc->caret.col);
    int targetLine = std::clamp(doc->caret.line + direction, 0, doc->LineCount() - 1);
    const Line& target = doc->lines[(size_t)targetLine];
    Cursor caret{targetLine, DisplayColumnToDoc(target, desired)};
    doc->caret = caret;
    if (!extend) doc->anchor = caret;
    doc->desiredCol = desired;
    EditorEnsureCaretVisible();
    ResetBlink();
    Invalidate();
}

void App::EditorHome(bool extend, bool document) {
    Document* doc = ActiveDoc();
    if (!doc) return;
    doc->EndEditGroup();
    Cursor caret = doc->caret;
    if (document) {
        caret = Cursor{0, 0};
    } else {
        int leading = LeadingDocChars(doc->LineText(caret.line));
        caret.col = (caret.col == leading) ? 0 : leading;
    }
    doc->SetCaret(caret, extend);
    doc->desiredCol = -1;
    EditorEnsureCaretVisible();
    ResetBlink();
    Invalidate();
}

void App::EditorEnd(bool extend, bool document) {
    Document* doc = ActiveDoc();
    if (!doc) return;
    doc->EndEditGroup();
    Cursor caret = doc->caret;
    if (document) {
        caret = Cursor{doc->LineCount() - 1, (int)doc->LineText(doc->LineCount() - 1).size()};
    } else {
        caret.col = (int)doc->LineText(caret.line).size();
    }
    doc->SetCaret(caret, extend);
    doc->desiredCol = -1;
    EditorEnsureCaretVisible();
    ResetBlink();
    Invalidate();
}

void App::EditorEnter() {
    Document* doc = ActiveDoc();
    if (!doc) return;
    doc->BeginEdit(false);
    if (doc->HasSelection()) {
        auto [a, b] = doc->SelectionRange();
        doc->DeleteRaw(a, b);
    }
    Cursor caret = doc->caret;
    const std::wstring& text = doc->LineText(caret.line);
    std::wstring before = text.substr(0, (size_t)caret.col);
    std::wstring after = text.substr((size_t)caret.col);
    std::wstring indent;
    size_t lastNonSpace = before.find_last_not_of(L" \t");
    if (lastNonSpace != std::wstring::npos) {
        wchar_t last = before[lastNonSpace];
        if (last == L'{' || last == L'[' || last == L'(' || last == L':') {
            indent += L"    ";
        }
    }
    std::wstring baseIndent;
    size_t firstNonSpace = before.find_first_not_of(L" \t");
    if (firstNonSpace != std::wstring::npos) baseIndent = before.substr(0, firstNonSpace);
    indent = baseIndent + indent;
    std::wstring trimmedAfter = Trim(after);
    if (!trimmedAfter.empty() && (trimmedAfter[0] == L'}' || trimmedAfter[0] == L']' || trimmedAfter[0] == L')')) {
        // Keep the closing brace aligned with the current block.
        if (indent.size() >= 4) indent.resize(indent.size() - 4);
    }
    std::wstring insertion = L"\n" + indent;
    doc->InsertRaw(caret, insertion);
    doc->MarkDirty();
    EditorEnsureCaretVisible();
    ResetBlink();
    Invalidate();
}

void App::EditorTab(bool outdent) {
    Document* doc = ActiveDoc();
    if (!doc) return;
    doc->EndEditGroup();
    if (doc->HasSelection()) {
        EditorIndent(outdent);
        return;
    }
    if (outdent) {
        EditorIndent(true);
        return;
    }
    doc->BeginEdit(false);
    int display = DocColumnToDisplay(doc->lines[(size_t)doc->caret.line], doc->caret.col);
    int spaces = 4 - (display % 4);
    doc->InsertRaw(doc->caret, Spaces(spaces));
    doc->MarkDirty();
    EditorEnsureCaretVisible();
    Invalidate();
}

void App::EditorIndent(bool outdent) {
    Document* doc = ActiveDoc();
    if (!doc) return;
    auto [a, b] = doc->SelectionRange();
    int startLine = a.line;
    int endLine = b.line;
    if (doc->HasSelection() && b.col == 0 && b.line > a.line) --endLine;
    doc->BeginEdit(false);
    for (int line = startLine; line <= endLine; ++line) {
        if (line < 0 || line >= doc->LineCount()) continue;
        if (outdent) {
            int remove = 0;
            const std::wstring& text = doc->LineText(line);
            while (remove < 4 && remove < (int)text.size() &&
                   (text[(size_t)remove] == L' ' || text[(size_t)remove] == L'\t')) {
                ++remove;
            }
            if (remove > 0) doc->DeleteRaw(Cursor{line, 0}, Cursor{line, remove});
        } else {
            doc->InsertRaw(Cursor{line, 0}, L"    ");
        }
    }
    doc->MarkDirty();
    if (doc->HasSelection()) {
        int shift = outdent ? -1 : 1;
        auto fix = [&](Cursor c) {
            c.col = std::max(0, c.col + shift * 4);
            return doc->ClampCursor(c);
        };
        // Approximate the caret movement; the selection remains a useful block selection.
        doc->caret = fix(doc->caret);
        doc->anchor = fix(doc->anchor);
    }
    EditorEnsureCaretVisible();
    Invalidate();
}

void App::EditorCopy(bool cut) {
    Document* doc = ActiveDoc();
    if (!doc) return;
    if (doc->HasSelection()) {
        SetClipboardText(hwnd_, doc->SelectedText());
        if (cut) {
            auto [a, b] = doc->SelectionRange();
            doc->BeginEdit(false);
            doc->DeleteRaw(a, b);
            doc->MarkDirty();
            EditorEnsureCaretVisible();
            Invalidate();
        }
    } else {
        std::wstring line = doc->LineText(doc->caret.line);
        line.push_back(L'\n');
        SetClipboardText(hwnd_, line);
        if (cut) {
            doc->BeginEdit(false);
            int lineIndex = doc->caret.line;
            if (lineIndex + 1 < doc->LineCount()) {
                doc->DeleteRaw(Cursor{lineIndex, 0}, Cursor{lineIndex + 1, 0});
                doc->SetCaret(Cursor{lineIndex, 0}, false);
            } else {
                int length = (int)doc->LineText(lineIndex).size();
                doc->DeleteRaw(Cursor{lineIndex, 0}, Cursor{lineIndex, length});
                doc->SetCaret(Cursor{lineIndex, 0}, false);
            }
            doc->MarkDirty();
            EditorEnsureCaretVisible();
            Invalidate();
        }
    }
    SetStatus(cut ? L"Cut" : L"Copied", 0);
}

void App::EditorPaste() {
    Document* doc = ActiveDoc();
    if (!doc) return;
    std::wstring text = ClipboardText(hwnd_);
    if (text.empty()) return;
    doc->BeginEdit(false);
    if (doc->HasSelection()) doc->DeleteRaw(doc->SelectionRange().first, doc->SelectionRange().second);
    doc->InsertRaw(doc->caret, text);
    doc->MarkDirty();
    EditorEnsureCaretVisible();
    ResetBlink();
    Invalidate();
}

void App::EditorToggleComment() {
    Document* doc = ActiveDoc();
    if (!doc) return;
    if (doc->language == Language::Markdown || doc->language == Language::Plain ||
        doc->language == Language::Json) return;
    auto [a, b] = doc->SelectionRange();
    int startLine = a.line;
    int endLine = b.line;
    if (doc->HasSelection() && b.col == 0 && b.line > a.line) --endLine;
    std::wstring prefix = L"// ";
    std::wstring suffix;
    switch (doc->language) {
        case Language::Python:
        case Language::CMake:
        case Language::Batch:
        case Language::PowerShell:
        case Language::Yaml:
            prefix = L"# ";
            break;
        case Language::Sql:
            prefix = L"-- ";
            break;
        case Language::Html:
        case Language::Xml:
            prefix = L"<!-- ";
            suffix = L" -->";
            break;
        case Language::Css:
            prefix = L"/* ";
            suffix = L" */";
            break;
        default:
            break;
    }
    bool allCommented = true;
    for (int line = startLine; line <= endLine; ++line) {
        std::wstring trimmed = doc->LineText(line);
        size_t first = trimmed.find_first_not_of(L" \t");
        if (first == std::wstring::npos) continue;
        if (!IStartsWith(trimmed.substr(first), prefix) ||
            (!suffix.empty() && !IEndsWith(trimmed, suffix))) {
            allCommented = false;
            break;
        }
    }
    doc->BeginEdit(false);
    for (int line = startLine; line <= endLine; ++line) {
        std::wstring text = doc->LineText(line);
        size_t first = text.find_first_not_of(L" \t");
        if (first == std::wstring::npos) continue;
        if (allCommented) {
            if (IStartsWith(text.substr(first), prefix)) {
                if (!suffix.empty() && IEndsWith(text, suffix)) {
                    int end = (int)text.size();
                    doc->DeleteRaw(Cursor{line, end - (int)suffix.size()},
                                   Cursor{line, end});
                }
                doc->DeleteRaw(Cursor{line, (int)first},
                               Cursor{line, (int)(first + prefix.size())});
                doc->MarkDirty();
            }
        } else {
            doc->InsertRaw(Cursor{line, (int)first}, prefix);
            if (!suffix.empty()) {
                doc->InsertRaw(Cursor{line, (int)(text.size() + prefix.size())}, suffix);
            }
            doc->MarkDirty();
        }
    }
    EditorEnsureCaretVisible();
    Invalidate();
}

// ------------------------------------------------------------------ find

FindRects App::GetFindRects() const {
    float width = S(456.0f);
    float height = S(38.0f);
    float right = L_.editor.right - S(20.0f);
    float top = L_.editor.top + S(10.0f);
    FindRects r;
    r.bar = MakeRect(right - width, top, right, top + height);
    r.input = MakeRect(r.bar.left + S(10.0f), r.bar.top + S(5.0f),
                       r.bar.left + width * 0.46f, r.bar.bottom - S(5.0f));
    r.prev = MakeRect(r.input.right + S(4.0f), r.bar.top + S(5.0f),
                      r.input.right + S(32.0f), r.bar.bottom - S(5.0f));
    r.next = MakeRect(r.prev.right + S(2.0f), r.bar.top + S(5.0f),
                      r.prev.right + S(30.0f), r.bar.bottom - S(5.0f));
    r.caseToggle = MakeRect(r.next.right + S(6.0f), r.bar.top + S(6.0f),
                            r.next.right + S(42.0f), r.bar.bottom - S(6.0f));
    r.count = MakeRect(r.caseToggle.right + S(4.0f), r.bar.top,
                       r.bar.right - S(36.0f), r.bar.bottom);
    r.close = MakeRect(r.bar.right - S(34.0f), r.bar.top + S(5.0f),
                       r.bar.right - S(6.0f), r.bar.bottom - S(5.0f));
    return r;
}

void App::DrawFindBar() {
    FindRects r = GetFindRects();
    renderer_.FillRoundedRect(OffsetRect(r.bar, 0, S(4.0f)), S(9.0f), theme_.shadow);
    renderer_.FillRoundedRect(r.bar, S(9.0f), theme_.panelBg);
    renderer_.StrokeRoundedRect(r.bar, S(9.0f), theme_.border, 1.0f);
    DrawTextField(findField_, r.input, S(12.0f), theme_.text, L"Find", true);
    TextStyle icon;
    icon.family = renderer_.IconFont();
    icon.size = S(13.0f);
    icon.hAlign = 1;
    icon.vAlign = 1;
    bool prevHover = Contains(r.prev, mouseX_, mouseY_);
    bool nextHover = Contains(r.next, mouseX_, mouseY_);
    if (prevHover) renderer_.FillRoundedRect(r.prev, S(5.0f), theme_.hover);
    if (nextHover) renderer_.FillRoundedRect(r.next, S(5.0f), theme_.hover);
    renderer_.Text(L"\uE70E", r.prev, prevHover ? theme_.text : theme_.textDim, icon);
    renderer_.Text(L"\uE70D", r.next, nextHover ? theme_.text : theme_.textDim, icon);
    bool caseHover = Contains(r.caseToggle, mouseX_, mouseY_);
    if (findCaseSensitive_) {
        renderer_.FillRoundedRect(r.caseToggle, S(5.0f), theme_.accentSoft);
        renderer_.StrokeRoundedRect(r.caseToggle, S(5.0f), WithAlpha(theme_.accent, 0.5f), 1.0f);
    } else if (caseHover) {
        renderer_.FillRoundedRect(r.caseToggle, S(5.0f), theme_.hover);
    }
    TextStyle caseStyle;
    caseStyle.size = S(11.0f);
    caseStyle.weight = 600;
    caseStyle.hAlign = 1;
    caseStyle.vAlign = 1;
    renderer_.Text(L"Aa", r.caseToggle,
                   findCaseSensitive_ ? theme_.accent : (caseHover ? theme_.text : theme_.textDim),
                   caseStyle);
    std::wstring count;
    Color countColor = theme_.textFaint;
    if (!findField_.text.empty()) {
        if (findMatches_.empty()) {
            count = L"No results";
            countColor = theme_.error;
        } else {
            int current = std::clamp(findCurrent_, 0, (int)findMatches_.size() - 1);
            count = std::to_wstring(current + 1) + L" of " + std::to_wstring(findMatches_.size());
            countColor = theme_.textDim;
        }
    }
    TextStyle countStyle;
    countStyle.size = S(10.5f);
    countStyle.hAlign = 2;
    countStyle.vAlign = 1;
    renderer_.Text(count, r.count, countColor, countStyle);
    bool closeHover = Contains(r.close, mouseX_, mouseY_);
    if (closeHover) renderer_.FillRoundedRect(r.close, S(5.0f), theme_.hover);
    renderer_.Text(L"\uE711", r.close, closeHover ? theme_.text : theme_.textDim, icon);
}

void App::OpenFind() {
    Document* doc = ActiveDoc();
    if (!doc) return;
    if (overlay_ != Overlay::Find) {
        overlay_ = Overlay::Find;
        if (doc->HasSelection()) {
            auto [a, b] = doc->SelectionRange();
            if (a.line == b.line) {
                std::wstring selected = doc->SelectedText();
                if (selected.find(L'\n') == std::wstring::npos && selected.size() < 200) {
                    findField_.SetText(selected);
                }
            }
        }
        findField_.SelectAll();
        findOrigin_ = doc->caret;
        findCurrent_ = -1;
    }
    UpdateFindMatches(true);
    Invalidate();
}

void App::CloseFind() {
    if (overlay_ == Overlay::Find) overlay_ = Overlay::None;
    findMatches_.clear();
    findCurrent_ = -1;
    Invalidate();
}

void App::UpdateFindMatches(bool selectCurrent) {
    findMatches_.clear();
    findCurrent_ = -1;
    Document* doc = ActiveDoc();
    if (!doc || findField_.text.empty()) return;
    std::wstring query = findField_.text;
    std::wstring loweredQuery = findCaseSensitive_ ? query : ToLower(query);
    size_t queryLen = query.size();
    int total = 0;
    for (int line = 0; line < doc->LineCount() && total < 5000; ++line) {
        const std::wstring& text = doc->LineText(line);
        std::wstring haystack = findCaseSensitive_ ? text : ToLower(text);
        size_t offset = 0;
        while (offset + queryLen <= haystack.size() && total < 5000) {
            size_t found = haystack.find(loweredQuery, offset);
            if (found == std::wstring::npos) break;
            findMatches_.push_back(FindMatch{line, (int)found, (int)(found + queryLen)});
            ++total;
            offset = found + std::max<size_t>(1, queryLen);
        }
    }
    if (findMatches_.empty()) return;
    int best = -1;
    for (size_t i = 0; i < findMatches_.size(); ++i) {
        const FindMatch& m = findMatches_[i];
        if (m.line > findOrigin_.line || (m.line == findOrigin_.line && m.start >= findOrigin_.col)) {
            best = (int)i;
            break;
        }
    }
    if (best < 0) best = 0;
    findCurrent_ = best;
    if (selectCurrent) GotoFindMatch(best);
}

void App::GotoFindMatch(int index) {
    Document* doc = ActiveDoc();
    if (!doc || findMatches_.empty()) return;
    index = ((index % (int)findMatches_.size()) + (int)findMatches_.size()) % (int)findMatches_.size();
    const FindMatch& match = findMatches_[(size_t)index];
    if (match.line < 0 || match.line >= doc->LineCount()) return;
    findCurrent_ = index;
    doc->EndEditGroup();
    doc->SetSelection(Cursor{match.line, match.start}, Cursor{match.line, match.end});
    EditorEnsureCaretVisible();
    ResetBlink();
    Invalidate();
}

void App::FindNext(int direction) {
    Document* doc = ActiveDoc();
    if (!doc) return;
    if (overlay_ != Overlay::Find) {
        OpenFind();
        return;
    }
    if (findField_.text.empty()) return;
    if (findMatches_.empty()) {
        UpdateFindMatches(true);
        if (findMatches_.empty()) return;
    }
    if (findCurrent_ < 0) findCurrent_ = 0;
    else findCurrent_ += direction > 0 ? 1 : -1;
    if (findCurrent_ < 0) findCurrent_ = (int)findMatches_.size() - 1;
    if (findCurrent_ >= (int)findMatches_.size()) findCurrent_ = 0;
    GotoFindMatch(findCurrent_);
}

void App::FindQueryChanged() {
    findCurrent_ = -1;
    UpdateFindMatches(true);
    Invalidate();
}

bool App::HandleFindKey(WPARAM key, bool ctrl, bool shift) {
    if (key == VK_ESCAPE) {
        CloseFind();
        return true;
    }
    if (key == VK_RETURN) {
        FindNext(shift ? -1 : 1);
        return true;
    }
    if (HandleTextFieldKey(findField_, key, ctrl, shift)) {
        FindQueryChanged();
        return true;
    }
    return false;
}
