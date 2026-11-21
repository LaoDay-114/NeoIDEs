#include "app.h"

#include <algorithm>
#include <cmath>

namespace {

Color LogColor(LogKind kind, const ThemeColors& t) {
    switch (kind) {
        case LogKind::Error: return t.error;
        case LogKind::Warning: return t.warning;
        case LogKind::Success: return t.success;
        case LogKind::Command: return t.accent;
        case LogKind::Muted: return t.textFaint;
        case LogKind::Info: return t.info;
        default: return t.textDim;
    }
}

std::wstring StripLineBreaks(std::wstring text) {
    while (!text.empty() && (text.back() == L'\n' || text.back() == L'\r')) text.pop_back();
    return text;
}

} // namespace

void App::AppendOutput(const std::wstring& text, LogKind kind,
                       const std::wstring& navPath, int navLine, int navCol) {
    std::wstring content = text;
    if (content.size() > 24000) content = content.substr(0, 24000) + L"…";
    size_t start = 0;
    while (true) {
        size_t newline = content.find(L'\n', start);
        std::wstring piece = newline == std::wstring::npos
                                 ? content.substr(start)
                                 : content.substr(start, newline - start);
        piece = StripLineBreaks(piece);
        auto entry = std::make_unique<OutputEntry>();
        entry->text = piece;
        entry->kind = kind;
        entry->navPath = navPath;
        entry->navLine = navLine;
        entry->navCol = navCol;
        output_.push_back(std::move(entry));
        if (newline == std::wstring::npos) break;
        start = newline + 1;
    }
    while (output_.size() > 2200) {
        output_.erase(output_.begin());
    }
    outputAutoScroll_ = true;
    if (settings_.panelVisible) {
        UpdateLayout();
        float bodyWidth = std::max(80.0f, RectWidth(L_.panel) - S(28.0f));
        RebuildOutputLayouts(bodyWidth);
        OutputScrollToBottom();
    }
}

void App::ClearOutput() {
    output_.clear();
    outputContentHeight_ = 0;
    outputScroll_ = 0;
    outputLayoutWidth_ = -1;
    outputAutoScroll_ = true;
    outputHitRects_.clear();
    Invalidate();
}

void App::OutputScrollToBottom() {
    if (outputLayoutWidth_ < 0 && settings_.panelVisible) {
        float bodyWidth = std::max(80.0f, RectWidth(L_.panel) - S(28.0f));
        RebuildOutputLayouts(bodyWidth);
    }
    float viewH = RectHeight(outputBodyRect_);
    outputScroll_ = std::max(0.0f, outputContentHeight_ - viewH);
    outputAutoScroll_ = true;
}

void App::RebuildOutputLayouts(float width) {
    if (!renderer_.DWrite()) return;
    float usableWidth = std::max(120.0f, width - S(20.0f));
    bool widthChanged = std::fabs(usableWidth - outputLayoutWidth_) >= 1.0f;
    if (!widthChanged) {
        bool missing = false;
        for (const auto& entry : output_) {
            if (!entry->layout) {
                missing = true;
                break;
            }
        }
        if (!missing) return;
    }
    outputLayoutWidth_ = usableWidth;
    TextStyle style;
    style.size = S(11.5f);
    style.family = renderer_.MonoFont();
    style.weight = 400;
    style.wrap = true;
    style.ellipsis = false;
    style.vAlign = 0;
    float total = 0;
    for (auto& entry : output_) {
        if (entry->layout && widthChanged) {
            entry->layout->Release();
            entry->layout = nullptr;
        }
        if (!entry->layout) {
            entry->layout = renderer_.CreateLayout(entry->text.empty() ? L" " : entry->text,
                                                   usableWidth, 100000.0f, style);
        }
        float height = S(18.0f);
        float textWidth = 0;
        if (entry->layout) {
            DWRITE_TEXT_METRICS metrics{};
            entry->layout->GetMetrics(&metrics);
            height = std::max(S(18.0f), metrics.height + S(4.0f));
            textWidth = metrics.width;
        }
        entry->height = height;
        entry->width = textWidth;
        total += height;
    }
    outputContentHeight_ = total + S(12.0f);
}

std::wstring App::TerminalWorkingDirectory() const {
    std::wstring dir = projectRoot_;
    if (dir.empty()) {
        dir = AppDataDirectory() + L"\\scratch";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
    }
    return dir;
}

bool App::StartTerminal() {
    if (process_ && !process_->IsDone()) {
        terminalRunning_ = true;
        return true;
    }
    if (process_) process_.reset();

    std::wstring shell = SearchExecutable(L"cmd.exe");
    if (shell.empty()) shell = L"cmd.exe";

    bool utf8 = settings_.terminalUtf8;
    std::wstring arguments = L"/D /Q";
    if (utf8) arguments += L" /K chcp 65001>nul";

    auto process = std::make_unique<Process>();
    process->SetCodePage(utf8 ? CP_UTF8 : CP_OEMCP);
    std::wstring error;
    std::wstring cwd = TerminalWorkingDirectory();
    if (!process->Start(shell, arguments, cwd, error)) {
        AppendOutput(error, LogKind::Error);
        SetStatus(L"Could not start the terminal", 3);
        return false;
    }
    process_ = std::move(process);
    terminalRunning_ = true;
    terminalInputFocused_ = true;
    terminalInput_.SetText(L"");
    terminalHistoryIndex_ = -1;
    AppendOutput(L"cmd · " + cwd, LogKind::Muted);
    SetStatus(L"Terminal ready", 0);
    Invalidate();
    return true;
}

void App::ToggleTerminal() {
    if (settings_.panelVisible) {
        settings_.panelVisible = false;
        terminalInputFocused_ = false;
        UpdateLayout();
        Invalidate();
        return;
    }
    settings_.panelVisible = true;
    UpdateLayout();
    StartTerminal();
    terminalInputFocused_ = true;
    ResetBlink();
    Invalidate();
}

void App::FocusTerminal() {
    if (!settings_.panelVisible) {
        settings_.panelVisible = true;
        UpdateLayout();
    }
    if (!process_ || process_->IsDone()) StartTerminal();
    terminalInputFocused_ = true;
    ResetBlink();
    Invalidate();
}

void App::SubmitTerminalLine() {
    std::wstring command = terminalInput_.text;
    if (!process_ || process_->IsDone()) {
        if (!StartTerminal()) return;
    }
    AppendOutput(L"> " + command, LogKind::Command);
    if (!command.empty() &&
        (terminalHistory_.empty() || terminalHistory_.back() != command)) {
        terminalHistory_.push_back(command);
        if (terminalHistory_.size() > 200) terminalHistory_.erase(terminalHistory_.begin());
    }
    terminalHistoryIndex_ = -1;
    terminalInput_.SetText(L"");
    if (process_) process_->WriteLine(command);
    ResetBlink();
    Invalidate();
}

bool App::HandleTerminalKey(WPARAM key, bool ctrl, bool shift) {
    if (!terminalInputFocused_ || !settings_.panelVisible ||
        overlay_ != Overlay::None || modalOpen_) {
        return false;
    }
    switch (key) {
        case VK_RETURN:
            SubmitTerminalLine();
            return true;
        case VK_ESCAPE:
            terminalInputFocused_ = false;
            Invalidate();
            return true;
        case VK_BACK:
            terminalInput_.Backspace(ctrl);
            ResetBlink();
            Invalidate();
            return true;
        case VK_DELETE:
            terminalInput_.DeleteForward(ctrl);
            ResetBlink();
            Invalidate();
            return true;
        case VK_LEFT:
            terminalInput_.Move(-1, shift, ctrl);
            ResetBlink();
            Invalidate();
            return true;
        case VK_RIGHT:
            terminalInput_.Move(1, shift, ctrl);
            ResetBlink();
            Invalidate();
            return true;
        case VK_HOME:
            terminalInput_.Home(shift, ctrl);
            ResetBlink();
            Invalidate();
            return true;
        case VK_END:
            terminalInput_.End(shift);
            ResetBlink();
            Invalidate();
            return true;
        case VK_UP:
        case VK_DOWN: {
            if (terminalHistory_.empty()) return true;
            if (key == VK_UP) {
                if (terminalHistoryIndex_ < 0) {
                    terminalHistoryIndex_ = (int)terminalHistory_.size() - 1;
                } else {
                    terminalHistoryIndex_ = std::max(0, terminalHistoryIndex_ - 1);
                }
                terminalInput_.SetText(terminalHistory_[(size_t)terminalHistoryIndex_]);
            } else {
                if (terminalHistoryIndex_ < 0) return true;
                ++terminalHistoryIndex_;
                if (terminalHistoryIndex_ >= (int)terminalHistory_.size()) {
                    terminalHistoryIndex_ = -1;
                    terminalInput_.SetText(L"");
                } else {
                    terminalInput_.SetText(terminalHistory_[(size_t)terminalHistoryIndex_]);
                }
            }
            ResetBlink();
            Invalidate();
            return true;
        }
        default:
            break;
    }
    return false;
}

void App::StopProcess() {
    if (!process_) {
        terminalRunning_ = false;
        return;
    }
    process_->Stop();
    process_.reset();
    terminalRunning_ = false;
    AppendOutput(L"Terminal stopped.", LogKind::Muted);
    SetStatus(L"Terminal stopped", 2);
    Invalidate();
}

void App::PollProcess() {
    if (!process_) return;
    std::vector<std::wstring> lines;
    bool gotLines = process_->PollLines(lines);
    for (const auto& line : lines) AppendOutput(StripLineBreaks(line), LogKind::Normal);

    bool done = false;
    int exitCode = 0;
    if (!process_->PollDone(done, exitCode) || !done) {
        if (gotLines && outputAutoScroll_) Invalidate();
        return;
    }

    std::vector<std::wstring> rest;
    if (process_->PollLines(rest)) {
        for (const auto& line : rest) AppendOutput(StripLineBreaks(line), LogKind::Normal);
    }
    process_.reset();
    terminalRunning_ = false;
    AppendOutput(L"Terminal exited with code " + std::to_wstring(exitCode) + L".",
                 exitCode == 0 ? LogKind::Muted : LogKind::Error);
    SetStatus(L"Terminal exited", exitCode == 0 ? 1 : 2);
    Invalidate();
}

void App::DrawOutputPanel() {
    if (!settings_.panelVisible) return;
    UpdateLayout();
    Rect panel = L_.panel;
    if (panel.right <= panel.left || panel.bottom <= panel.top) return;
    renderer_.FillRect(panel, theme_.panelBg);
    renderer_.FillRect(MakeRect(panel.left, panel.top, panel.right, panel.top + S(1.0f)),
                       theme_.border);
    renderer_.FillRect(MakeRect(panel.left, panel.top, panel.left + S(1.0f), panel.bottom),
                       theme_.borderSoft);

    float headerH = S(35.0f);
    Rect header = MakeRect(panel.left, panel.top, panel.right, panel.top + headerH);
    renderer_.FillRect(header, theme_.tabBarBg);
    renderer_.FillRect(MakeRect(header.left, header.bottom - S(1.0f), header.right, header.bottom),
                       theme_.borderSoft);

    TextStyle icon;
    icon.family = renderer_.IconFont();
    icon.size = S(13.0f);
    icon.hAlign = 1;
    icon.vAlign = 1;
    renderer_.Text(L"\uE756", MakeRect(header.left + S(10.0f), header.top,
                                        header.left + S(34.0f), header.bottom),
                   theme_.textDim, icon);

    TextStyle title;
    title.size = S(10.5f);
    title.weight = 700;
    title.vAlign = 1;
    renderer_.Text(L"TERMINAL", MakeRect(header.left + S(38.0f), header.top,
                                         header.left + S(116.0f), header.bottom),
                   theme_.textDim, title);

    if (terminalRunning_) {
        renderer_.DrawEllipse(header.left + S(126.0f), (header.top + header.bottom) * 0.5f,
                              S(3.0f), S(3.0f), theme_.success, true);
        TextStyle activityStyle;
        activityStyle.size = S(10.5f);
        activityStyle.vAlign = 1;
        renderer_.Text(L"cmd", MakeRect(header.left + S(133.0f), header.top,
                                        header.left + S(190.0f), header.bottom),
                       theme_.success, activityStyle);
    }

    outputCloseButton_ = MakeRect(header.right - S(34.0f), header.top + S(7.0f),
                                  header.right - S(8.0f), header.bottom - S(7.0f));
    outputClearButton_ = MakeRect(header.right - S(66.0f), header.top + S(7.0f),
                                  header.right - S(40.0f), header.bottom - S(7.0f));
    bool clearHover = Contains(outputClearButton_, mouseX_, mouseY_);
    bool closeHover = Contains(outputCloseButton_, mouseX_, mouseY_);
    if (clearHover) renderer_.FillRoundedRect(outputClearButton_, S(5.0f), theme_.hover);
    if (closeHover) renderer_.FillRoundedRect(outputCloseButton_, S(5.0f), theme_.hover);
    renderer_.Text(L"\uE894", outputClearButton_, clearHover ? theme_.text : theme_.textFaint, icon);
    renderer_.Text(L"\uE711", outputCloseButton_, closeHover ? theme_.text : theme_.textFaint, icon);

    float inputH = S(30.0f);
    terminalInputRect_ = MakeRect(panel.left + S(8.0f), panel.bottom - inputH - S(5.0f),
                                  panel.right - S(12.0f), panel.bottom - S(5.0f));
    outputBodyRect_ = MakeRect(panel.left, panel.top + headerH, panel.right,
                               terminalInputRect_.top - S(3.0f));
    renderer_.FillRect(outputBodyRect_, theme_.panelBg);
    float bodyW = RectWidth(outputBodyRect_) - S(18.0f);
    if (bodyW > 0) RebuildOutputLayouts(bodyW);
    float viewH = RectHeight(outputBodyRect_);
    float maxScroll = std::max(0.0f, outputContentHeight_ - viewH);
    if (outputAutoScroll_) outputScroll_ = maxScroll;
    outputScroll_ = std::clamp(outputScroll_, 0.0f, maxScroll);
    if (outputScroll_ >= maxScroll - 1.0f) outputAutoScroll_ = true;

    renderer_.PushClip(outputBodyRect_);
    if (output_.empty()) {
        TextStyle hint;
        hint.size = S(11.5f);
        hint.wrap = true;
        hint.ellipsis = false;
        renderer_.Text(terminalRunning_
                           ? L"Type a command below and press Enter."
                           : L"Type a command below and press Enter to start cmd.exe.\n"
                             L"中文输出可在 Settings → Terminal 中调整编码。",
                       MakeRect(outputBodyRect_.left + S(14.0f), outputBodyRect_.top + S(16.0f),
                                outputBodyRect_.right - S(24.0f), outputBodyRect_.top + S(70.0f)),
                       theme_.textFaint, hint);
    }
    outputHitRects_.clear();
    float y = outputBodyRect_.top + S(5.0f) - outputScroll_;
    for (size_t i = 0; i < output_.size(); ++i) {
        OutputEntry& entry = *output_[i];
        float h = std::max(S(16.0f), entry.height);
        Rect lineRect = MakeRect(outputBodyRect_.left + S(7.0f), y,
                                 outputBodyRect_.right - S(18.0f), y + h);
        if (y + h >= outputBodyRect_.top && y <= outputBodyRect_.bottom) {
            bool clickable = !entry.navPath.empty() && entry.navLine > 0;
            bool hover = clickable && Contains(lineRect, mouseX_, mouseY_);
            if (hover) {
                renderer_.FillRoundedRect(Inset(lineRect, S(2.0f)), S(4.0f), theme_.hover);
            }
            if (entry.layout) {
                renderer_.DrawLayout(entry.layout, outputBodyRect_.left + S(10.0f), y,
                                     LogColor(entry.kind, theme_));
            }
            if (clickable) {
                outputHitRects_.push_back({lineRect, (int)i});
            }
        }
        y += h;
    }
    renderer_.PopClip();

    if (outputContentHeight_ > viewH && viewH > 0) {
        float thumbH = std::max(S(26.0f), viewH * (viewH / std::max(1.0f, outputContentHeight_)));
        float top = outputBodyRect_.top + (viewH - thumbH) *
                    std::clamp(outputScroll_ / std::max(1.0f, maxScroll), 0.0f, 1.0f);
        renderer_.FillRoundedRect(MakeRect(outputBodyRect_.right - S(9.0f), top,
                                            outputBodyRect_.right - S(4.0f), top + thumbH),
                                  S(2.5f), theme_.scrollbar);
    }

    // Command line row
    renderer_.FillRoundedRect(terminalInputRect_, S(6.0f),
                              terminalInputFocused_ ? WithAlpha(theme_.accent, 0.10f) : theme_.hover);
    if (terminalInputFocused_) {
        renderer_.StrokeRoundedRect(terminalInputRect_, S(6.0f),
                                    WithAlpha(theme_.accent, 0.55f), 1.0f);
    }
    TextStyle promptStyle;
    promptStyle.size = S(11.5f);
    promptStyle.family = renderer_.MonoFont();
    promptStyle.vAlign = 1;
    renderer_.Text(L">", MakeRect(terminalInputRect_.left + S(8.0f), terminalInputRect_.top,
                                   terminalInputRect_.left + S(20.0f), terminalInputRect_.bottom),
                   theme_.accent, promptStyle);

    Rect textRect = MakeRect(terminalInputRect_.left + S(21.0f), terminalInputRect_.top,
                             terminalInputRect_.right - S(8.0f), terminalInputRect_.bottom);
    renderer_.PushClip(textRect);
    TextStyle inputStyle;
    inputStyle.size = S(11.5f);
    inputStyle.family = renderer_.MonoFont();
    inputStyle.vAlign = 1;
    std::wstring shown = terminalInput_.text.empty() ? L"输入命令…" : terminalInput_.text;
    renderer_.Text(shown, textRect,
                   terminalInput_.text.empty() ? theme_.textFaint : theme_.text, inputStyle);
    if (terminalInputFocused_ && ((GetTickCount64() - blinkResetTick_) / 530) % 2 == 0) {
        std::wstring prefix = terminalInput_.text.substr(0, (size_t)terminalInput_.caret);
        float caretX = textRect.left + renderer_.Measure(prefix, inputStyle).width;
        float cy = (terminalInputRect_.top + terminalInputRect_.bottom) * 0.5f;
        renderer_.FillRect(MakeRect(caretX, cy - S(8.0f), caretX + S(1.5f), cy + S(8.0f)),
                           theme_.accent);
    }
    renderer_.PopClip();
}

void App::HandleOutputClick(float x, float y) {
    if (Contains(terminalInputRect_, x, y)) {
        FocusTerminal();
        return;
    }
    for (const auto& [rect, index] : outputHitRects_) {
        if (!Contains(rect, x, y)) continue;
        if (index < 0 || index >= (int)output_.size()) return;
        const OutputEntry& entry = *output_[(size_t)index];
        if (!entry.navPath.empty() && entry.navLine > 0) {
            if (!std::filesystem::exists(entry.navPath)) {
                SetStatus(L"Source file not found: " + BaseName(entry.navPath), 3);
                return;
            }
            if (!OpenFile(entry.navPath)) return;
            Document* doc = ActiveDoc();
            if (!doc) return;
            doc->EndEditGroup();
            Cursor cursor;
            cursor.line = std::clamp(entry.navLine - 1, 0, doc->LineCount() - 1);
            cursor.col = std::clamp(entry.navCol > 0 ? entry.navCol - 1 : 0, 0,
                                    (int)doc->LineText(cursor.line).size());
            doc->SetCaret(cursor, false);
            EditorEnsureCaretVisible();
            ResetBlink();
            Invalidate();
        }
        return;
    }
    if (Contains(L_.panel, x, y)) FocusTerminal();
}
