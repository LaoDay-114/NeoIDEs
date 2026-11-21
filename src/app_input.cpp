#include "app.h"

#include <imm.h>

namespace {

int MenuCountFor(const std::vector<MenuEntryDef>& entries) {
    int n = 0;
    for (const auto& e : entries) n += e.separator ? 1 : 1;
    return n;
}

bool IsWordBoundaryKey(WPARAM key) {
    return key == VK_LEFT || key == VK_RIGHT || key == VK_HOME || key == VK_END ||
           key == VK_UP || key == VK_DOWN || key == VK_PRIOR || key == VK_NEXT;
}

} // namespace

LRESULT App::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    hwnd_ = hwnd;
    switch (msg) {
        case WM_NCCREATE:
            return TRUE;

        case WM_NCCALCSIZE:
            if (wParam) {
                auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
                if (IsZoomed(hwnd_)) {
                    MONITORINFO mi{};
                    mi.cbSize = sizeof(mi);
                    if (GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &mi)) {
                        params->rgrc[0] = mi.rcWork;
                    }
                }
                return 0;
            }
            break;

        case WM_CREATE:
            OnCreate();
            return 0;

        case WM_SIZE:
            width_ = LOWORD(lParam);
            height_ = HIWORD(lParam);
            if (width_ > 0 && height_ > 0) {
                renderer_.Resize((UINT)width_, (UINT)height_);
                UpdateLayout();
                UpdateEditorMetrics();
                Invalidate();
            }
            return 0;

        case WM_DPICHANGED: {
            UINT newDpi = HIWORD(wParam);
            dpi_ = newDpi > 0 ? newDpi / 96.0f : 1.0f;
            RECT* suggested = reinterpret_cast<RECT*>(lParam);
            if (suggested) {
                SetWindowPos(hwnd_, nullptr, suggested->left, suggested->top,
                             suggested->right - suggested->left, suggested->bottom - suggested->top,
                             SWP_NOZORDER | SWP_NOACTIVATE);
            }
            for (auto& doc : docs_) doc->InvalidateLayouts();
            UpdateLayout();
            UpdateEditorMetrics();
            Invalidate();
            return 0;
        }

        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
            info->ptMinTrackSize.x = (LONG)S(900.0f);
            info->ptMinTrackSize.y = (LONG)S(560.0f);
            return 0;
        }

        case WM_NCHITTEST: {
            LRESULT hit = DefWindowProcW(hwnd_, msg, wParam, lParam);
            if (hit != HTCLIENT) return hit;
            POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ScreenToClient(hwnd_, &pt);
            float x = (float)pt.x;
            float y = (float)pt.y;
            bool maximized = IsZoomed(hwnd_) != FALSE;
            if (!maximized) {
                float border = S(6.0f);
                bool left = x < border;
                bool right = x >= (float)width_ - border;
                bool top = y < border;
                bool bottom = y >= (float)height_ - border;
                if (top && left) return HTTOPLEFT;
                if (top && right) return HTTOPRIGHT;
                if (bottom && left) return HTBOTTOMLEFT;
                if (bottom && right) return HTBOTTOMRIGHT;
                if (left) return HTLEFT;
                if (right) return HTRIGHT;
                if (top) return HTTOP;
                if (bottom) return HTBOTTOM;
            }
            if (y >= 0 && y < RectHeight(L_.titleBar)) {
                if (Contains(L_.maxButton, x, y)) return HTMAXBUTTON;
                if (Contains(L_.closeButton, x, y) || Contains(L_.minButton, x, y) ||
                    Contains(L_.terminalButton, x, y) || MenuLabelAt(x, y) >= 0) {
                    return HTCLIENT;
                }
                return HTCAPTION;
            }
            return HTCLIENT;
        }

        case WM_NCLBUTTONDOWN:
            if (wParam == HTMAXBUTTON) {
                ShowWindow(hwnd_, IsZoomed(hwnd_) ? SW_RESTORE : SW_MAXIMIZE);
                Invalidate();
                return 0;
            }
            break;

        case WM_NCLBUTTONDBLCLK:
            if (wParam == HTCAPTION || wParam == HTMAXBUTTON) {
                ShowWindow(hwnd_, IsZoomed(hwnd_) ? SW_RESTORE : SW_MAXIMIZE);
                Invalidate();
                return 0;
            }
            break;

        case WM_ACTIVATE:
            windowActive_ = LOWORD(wParam) != WA_INACTIVE;
            if (!windowActive_) {
                if (auto* doc = ActiveDoc()) doc->EndEditGroup();
            }
            ResetBlink();
            Invalidate();
            return 0;

        case WM_SETFOCUS:
        case WM_KILLFOCUS:
            ResetBlink();
            if (msg == WM_SETFOCUS) SetFocus(hwnd_);
            if (msg == WM_KILLFOCUS) {
                if (auto* doc = ActiveDoc()) doc->EndEditGroup();
            }
            Invalidate();
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC dc = BeginPaint(hwnd_, &ps);
            Paint(dc);
            EndPaint(hwnd_, &ps);
            return 0;
        }

        case WM_TIMER:
            if (wParam == 1) OnTimer();
            return 0;

        case WM_MOUSEMOVE:
            OnMouseMove((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam),
                        (wParam & MK_LBUTTON) != 0);
            return 0;

        case WM_LBUTTONDOWN:
            SetFocus(hwnd_);
            SetCapture(hwnd_);
            OnMouseDown((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), 1);
            return 0;

        case WM_LBUTTONDBLCLK:
            OnMouseDown((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), 2);
            return 0;

        case WM_LBUTTONUP:
            OnMouseUp();
            return 0;

        case WM_MBUTTONDOWN: {
            float x = (float)GET_X_LPARAM(lParam);
            float y = (float)GET_Y_LPARAM(lParam);
            if (!docs_.empty() && Contains(L_.tabBar, x, y)) {
                ComputeTabs();
                for (const auto& tab : tabViews_) {
                    if (Contains(tab.rect, x, y)) {
                        CloseDocument(docs_[(size_t)tab.doc].get());
                        return 0;
                    }
                }
            }
            return 0;
        }

        case WM_MOUSEWHEEL: {
            POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ScreenToClient(hwnd_, &pt);
            bool ctrl = (LOWORD(wParam) & MK_CONTROL) != 0;
            bool shift = (LOWORD(wParam) & MK_SHIFT) != 0;
            OnMouseWheel((float)pt.x, (float)pt.y, (float)GET_WHEEL_DELTA_WPARAM(wParam), ctrl, shift);
            return 0;
        }

        case WM_KEYDOWN: {
            bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
            OnKeyDown(wParam, ctrl, shift, alt);
            return 0;
        }

        case WM_CHAR:
            OnChar((wchar_t)wParam);
            return 0;

        case WM_CAPTURECHANGED:
            if (drag_ != DragMode::None) {
                drag_ = DragMode::None;
                Invalidate();
            }
            break;

        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT) {
                POINT pt{};
                GetCursorPos(&pt);
                ScreenToClient(hwnd_, &pt);
                float x = (float)pt.x;
                float y = (float)pt.y;
                LPWSTR cursor = IDC_ARROW;
                if (drag_ == DragMode::Sidebar) cursor = IDC_SIZEWE;
                else if (drag_ == DragMode::Panel) cursor = IDC_SIZENS;
                else if (!modalOpen_ && overlay_ == Overlay::None) {
                    if (Contains(L_.sidebarSplitter, x, y)) cursor = IDC_SIZEWE;
                    else if (Contains(L_.panelSplitter, x, y)) cursor = IDC_SIZENS;
                    else if (!docs_.empty() && Contains(L_.editor, x, y) &&
                             x >= L_.editor.left + gutterWidth_) {
                        cursor = IDC_IBEAM;
                    }
                } else if (overlay_ == Overlay::Find) {
                    FindRects fr = GetFindRects();
                    if (Contains(fr.input, x, y)) cursor = IDC_IBEAM;
                }
                SetCursor(LoadCursorW(nullptr, cursor));
                return TRUE;
            }
            break;

        case WM_DROPFILES:
            HandleDropFiles((HDROP)wParam);
            return 0;

        case WM_IME_STARTCOMPOSITION: {
            HIMC context = ImmGetContext(hwnd_);
            if (context) {
                if (auto* doc = ActiveDoc()) {
                    float lineY = LineTop(*doc, doc->caret.line);
                    float x = L_.editor.left + gutterWidth_ + CaretXFor(*doc, doc->caret.line, doc->caret.col) - doc->scrollX;
                    COMPOSITIONFORM form{};
                    form.dwStyle = CFS_POINT;
                    form.ptCurrentPos.x = (LONG)x;
                    form.ptCurrentPos.y = (LONG)(lineY + lineHeight_);
                    ImmSetCompositionWindow(context, &form);
                }
                ImmReleaseContext(hwnd_, context);
            }
            break;
        }

        case WM_CLOSE:
            RequestExit();
            return 0;

        case WM_DESTROY:
            OnDestroy();
            return 0;

        case WM_NCDESTROY:
            SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);
            break;

        default:
            break;
    }
    return DefWindowProcW(hwnd_, msg, wParam, lParam);
}

void App::OnMouseDown(float x, float y, int clicks) {
    mouseX_ = x;
    mouseY_ = y;
    ResetBlink();

    if (modalOpen_) {
        for (size_t i = 0; i < modalButtonRects_.size() && i < modalButtons_.size(); ++i) {
            if (Contains(modalButtonRects_[i], x, y)) {
                FinishModal(modalButtons_[i].result);
                return;
            }
        }
        return;
    }

    if (openMenu_ >= 0) {
        if (HandleMenuClick(x, y)) return;
    }

    if (overlay_ != Overlay::None) {
        if (overlay_ == Overlay::Find) {
            FindRects fr = GetFindRects();
            if (Contains(fr.bar, x, y)) {
                if (Contains(fr.close, x, y)) CloseFind();
                else if (Contains(fr.prev, x, y)) FindNext(-1);
                else if (Contains(fr.next, x, y)) FindNext(1);
                else if (Contains(fr.caseToggle, x, y)) {
                    findCaseSensitive_ = !findCaseSensitive_;
                    if (!findField_.text.empty()) {
                        findOrigin_ = ActiveDoc() ? ActiveDoc()->caret : Cursor{};
                        UpdateFindMatches(true);
                    }
                }
                Invalidate();
                return;
            }
            CloseFind();
            // fall through: the click also positions the editor caret
        } else {
            OnOverlayMouseDown(x, y);
            return;
        }
    }

    if (Contains(L_.titleBar, x, y)) {
        int menu = MenuLabelAt(x, y);
        if (menu >= 0) {
            openMenu_ = (openMenu_ == menu) ? -1 : menu;
            Invalidate();
            return;
        }
        switch (TitleControlAt(x, y)) {
            case 1: ShowWindow(hwnd_, SW_MINIMIZE); return;
            case 2: ShowWindow(hwnd_, IsZoomed(hwnd_) ? SW_RESTORE : SW_MAXIMIZE); Invalidate(); return;
            case 3: RequestExit(); return;
            case 4: ToggleTerminal(); return;
            default: return;
        }
    }

    if (Contains(L_.rail, x, y)) {
        float bx = (L_.rail.left + L_.rail.right) * 0.5f;
        float btn = S(40.0f);
        float gap = S(46.0f);
        float top = L_.rail.top + S(9.0f);
        for (int i = 0; i < 3; ++i) {
            Rect r = MakeRect(bx - btn * 0.5f, top + i * gap, bx + btn * 0.5f, top + i * gap + btn);
            if (Contains(r, x, y)) {
                if (i == 0) {
                    settings_.sidebarVisible = !settings_.sidebarVisible;
                    UpdateLayout();
                    SaveSettings();
                    Invalidate();
                } else if (i == 1) {
                    OpenFind();
                } else {
                    ToggleTerminal();
                }
                return;
            }
        }
        Rect settingsRect = MakeRect(bx - btn * 0.5f, L_.rail.bottom - S(58.0f),
                                     bx + btn * 0.5f, L_.rail.bottom - S(18.0f));
        if (Contains(settingsRect, x, y)) {
            OpenSettings();
            return;
        }
        return;
    }

    if (Contains(L_.sidebarSplitter, x, y)) {
        drag_ = DragMode::Sidebar;
        dragStart_ = x;
        dragInitial_ = S(settings_.sidebarWidth);
        SetCapture(hwnd_);
        return;
    }
    if (Contains(L_.panelSplitter, x, y)) {
        drag_ = DragMode::Panel;
        dragStart_ = y;
        dragInitial_ = S(settings_.panelHeight);
        SetCapture(hwnd_);
        return;
    }

    if (settings_.sidebarVisible && Contains(L_.sidebar, x, y)) {
        if (Contains(explorerNewButton_, x, y)) {
            NewFileInProject();
            return;
        }
        if (Contains(explorerRefreshButton_, x, y)) {
            tree_.Refresh();
            SetStatus(L"Explorer refreshed", 0);
            Invalidate();
            return;
        }
        HandleTreeClick(x, y);
        return;
    }

    if (!docs_.empty() && Contains(L_.tabBar, x, y)) {
        ComputeTabs();
        for (const auto& tab : tabViews_) {
            if (Contains(tab.close, x, y)) {
                CloseDocument(docs_[(size_t)tab.doc].get());
                return;
            }
            if (Contains(tab.rect, x, y)) {
                ActivateDoc(tab.doc);
                return;
            }
        }
        if (Contains(tabNewButton_, x, y)) {
            NewUntitled();
            return;
        }
        return;
    }

    if (settings_.panelVisible && Contains(L_.panel, x, y)) {
        if (Contains(outputClearButton_, x, y)) {
            ClearOutput();
            return;
        }
        if (Contains(outputCloseButton_, x, y)) {
            settings_.panelVisible = false;
            terminalInputFocused_ = false;
            UpdateLayout();
            SaveSettings();
            Invalidate();
            return;
        }
        HandleOutputClick(x, y);
        return;
    }

    if (!docs_.empty() && Contains(L_.editor, x, y)) {
        terminalInputFocused_ = false;
        UpdateVisibleScrollbars();
        Document* doc = ActiveDoc();
        if (!doc) return;
        if (showVScroll_ && Contains(vScrollTrack_, x, y)) {
            if (Contains(vScrollThumb_, x, y)) {
                drag_ = DragMode::VScroll;
                dragStart_ = y;
                SetCapture(hwnd_);
            } else {
                Rect view = editorTextViewport_;
                doc->scrollY += (y < vScrollThumb_.top ? -1.0f : 1.0f) * RectHeight(view) * 0.9f;
                ClampScroll(*doc);
                Invalidate();
            }
            return;
        }
        if (showHScroll_ && Contains(hScrollTrack_, x, y)) {
            if (Contains(hScrollThumb_, x, y)) {
                drag_ = DragMode::HScroll;
                dragStart_ = x;
                SetCapture(hwnd_);
            } else {
                doc->scrollX += (x < hScrollThumb_.left ? -1.0f : 1.0f) * RectWidth(editorTextViewport_) * 0.8f;
                ClampScroll(*doc);
                Invalidate();
            }
            return;
        }
        if (x < L_.editor.left + gutterWidth_) {
            int line = (int)std::floor((y - L_.editor.top + doc->scrollY) / std::max(1.0f, lineHeight_));
            line = std::clamp(line, 0, doc->LineCount() - 1);
            doc->EndEditGroup();
            Cursor a{line, 0};
            Cursor b = line + 1 < doc->LineCount() ? Cursor{line + 1, 0}
                                                    : Cursor{line, (int)doc->LineText(line).size()};
            doc->SetSelection(a, b);
            ResetBlink();
            Invalidate();
            return;
        }
        EditorMouseDown(x, y, clicks);
        return;
    }

    if (docs_.empty() && Contains(L_.main, x, y)) {
        for (const auto& [rect, path] : welcomeRecentRects_) {
            if (Contains(rect, x, y)) {
                SetProject(path);
                return;
            }
        }
        if (Contains(welcomeOpenFolder_, x, y)) PromptOpenFolder();
        else if (Contains(welcomeOpenFile_, x, y)) PromptOpenFiles();
        else if (Contains(welcomeNewFile_, x, y)) NewUntitled();
        else if (Contains(welcomePalette_, x, y)) OpenPalette();
        return;
    }
}

void App::OnMouseMove(float x, float y, bool) {
    mouseX_ = x;
    mouseY_ = y;

    if (drag_ != DragMode::None) {
        switch (drag_) {
            case DragMode::Sidebar:
                settings_.sidebarWidth = std::clamp((dragInitial_ + (x - dragStart_)) / dpi_, 200.0f, 520.0f);
                UpdateLayout();
                break;
            case DragMode::Panel:
                settings_.panelHeight = std::clamp((dragInitial_ - (y - dragStart_)) / dpi_, 100.0f, 760.0f);
                UpdateLayout();
                break;
            case DragMode::EditorSelect:
                EditorMouseMove(x, y);
                break;
            case DragMode::VScroll: {
                Document* doc = ActiveDoc();
                if (doc) {
                    UpdateVisibleScrollbars();
                    float track = RectHeight(vScrollTrack_);
                    float thumb = RectHeight(vScrollThumb_);
                    float travel = std::max(1.0f, track - thumb);
                    float viewH = RectHeight(editorTextViewport_);
                    float contentH = doc->LineCount() * lineHeight_;
                    float maxScroll = std::max(0.0f, contentH - viewH + S(12.0f));
                    doc->scrollY += (y - dragStart_) / travel * maxScroll;
                    dragStart_ = y;
                    ClampScroll(*doc);
                }
                break;
            }
            case DragMode::HScroll: {
                Document* doc = ActiveDoc();
                if (doc) {
                    UpdateVisibleScrollbars();
                    float track = RectWidth(hScrollTrack_);
                    float thumb = RectWidth(hScrollThumb_);
                    float travel = std::max(1.0f, track - thumb);
                    float viewW = RectWidth(editorTextViewport_);
                    float contentW = doc->maxColumns * monoCharWidth_ + S(40.0f);
                    float maxScroll = std::max(0.0f, contentW - viewW);
                    doc->scrollX += (x - dragStart_) / travel * maxScroll;
                    dragStart_ = x;
                    ClampScroll(*doc);
                }
                break;
            }
            case DragMode::OutputScroll: {
                float track = RectHeight(outputBodyRect_);
                float content = outputContentHeight_;
                float thumbH = track > 0 ? std::max(S(24.0f), track * std::min(1.0f, track / std::max(1.0f, content))) : 0;
                float travel = std::max(1.0f, track - thumbH);
                float maxScroll = std::max(0.0f, content - track);
                outputScroll_ += (y - dragStart_) / travel * maxScroll;
                dragStart_ = y;
                outputScroll_ = std::clamp(outputScroll_, 0.0f, maxScroll);
                outputAutoScroll_ = outputScroll_ >= maxScroll - 2.0f;
                break;
            }
            default:
                break;
        }
        Invalidate();
        return;
    }

    if (openMenu_ >= 0) HandleMenuMouseMove(x, y);
    Invalidate();
}

void App::OnMouseUp() {
    if (drag_ == DragMode::Sidebar || drag_ == DragMode::Panel) SaveSettings();
    drag_ = DragMode::None;
    if (GetCapture() == hwnd_) ReleaseCapture();
    Invalidate();
}

void App::OnMouseWheel(float x, float y, float delta, bool ctrl, bool shift) {
    mouseX_ = x;
    mouseY_ = y;
    float steps = delta / (float)WHEEL_DELTA;
    if (ctrl) {
        int old = settings_.editorFontSize;
        settings_.editorFontSize = std::clamp(settings_.editorFontSize + (steps > 0 ? 1 : -1), 9, 40);
        if (settings_.editorFontSize != old) {
            for (auto& doc : docs_) doc->InvalidateLayouts();
            UpdateEditorMetrics();
            SaveSettings();
            Invalidate();
        }
        return;
    }

    if (modalOpen_ || openMenu_ >= 0) return;

    if (overlay_ == Overlay::Palette) {
        if (!paletteFiltered_.empty()) {
            int rows = (int)std::min<size_t>((size_t)10, paletteFiltered_.size());
            paletteSel_ = std::clamp(paletteSel_ - (int)std::lround(steps), 0, (int)paletteFiltered_.size() - 1);
            if (paletteSel_ < paletteOffset_) paletteOffset_ = paletteSel_;
            if (paletteSel_ >= paletteOffset_ + rows) paletteOffset_ = paletteSel_ - rows + 1;
        }
        Invalidate();
        return;
    }
    if (overlay_ == Overlay::QuickOpen) {
        if (!quickFiltered_.empty()) {
            int rows = (int)std::min<size_t>((size_t)8, quickFiltered_.size());
            quickSel_ = std::clamp(quickSel_ - (int)std::lround(steps), 0, (int)quickFiltered_.size() - 1);
            if (quickSel_ < quickOffset_) quickOffset_ = quickSel_;
            if (quickSel_ >= quickOffset_ + rows) quickOffset_ = quickSel_ - rows + 1;
        }
        Invalidate();
        return;
    }
    if (overlay_ != Overlay::None) return;

    if (settings_.panelVisible && Contains(L_.panel, x, y)) {
        float viewH = RectHeight(outputBodyRect_);
        float maxScroll = std::max(0.0f, outputContentHeight_ - viewH);
        outputScroll_ = std::clamp(outputScroll_ - steps * S(48.0f), 0.0f, maxScroll);
        outputAutoScroll_ = outputScroll_ >= maxScroll - 2.0f;
        Invalidate();
        return;
    }
    if (settings_.sidebarVisible && Contains(L_.sidebar, x, y)) {
        float viewH = RectHeight(treeViewRect_);
        float maxScroll = std::max(0.0f, treeContentHeight_ - viewH);
        treeScroll_ = std::clamp(treeScroll_ - steps * S(72.0f), 0.0f, maxScroll);
        Invalidate();
        return;
    }
    if (!docs_.empty() && Contains(L_.tabBar, x, y)) {
        ComputeTabs();
        float maxScroll = std::max(0.0f, tabContentWidth_ - RectWidth(L_.main));
        tabScroll_ = std::clamp(tabScroll_ - steps * S(80.0f), 0.0f, maxScroll);
        Invalidate();
        return;
    }
    if (!docs_.empty() && Contains(L_.editor, x, y)) {
        EditorWheel(x, y, delta, ctrl, shift);
    }
}

void App::OnKeyDown(WPARAM key, bool ctrl, bool shift, bool alt) {
    if (modalOpen_) {
        if (HandleModalKey(key)) {
            ResetBlink();
            Invalidate();
        }
        return;
    }

    if (overlay_ != Overlay::None) {
        OnOverlayKey(key, ctrl, shift);
        ResetBlink();
        Invalidate();
        return;
    }

    if (openMenu_ >= 0) {
        if (key == VK_ESCAPE) {
            openMenu_ = -1;
            Invalidate();
            return;
        }
        openMenu_ = -1;
    }

    bool handled = false;
    Document* doc = ActiveDoc();
    if (ctrl) {
        switch (key) {
            case 'N': NewUntitled(); handled = true; break;
            case 'O':
                if (shift) PromptOpenFolder();
                else PromptOpenFiles();
                handled = true;
                break;
            case 'S':
                if (shift) {
                    if (doc) SaveDocument(*doc, true);
                } else if (doc) {
                    SaveDocument(*doc, false);
                }
                handled = true;
                break;
            case 'W':
                if (shift) RequestCloseAll();
                else if (doc) CloseDocument(doc);
                handled = true;
                break;
            case 'Z':
                if (doc) {
                    doc->EndEditGroup();
                    if (shift) doc->Redo();
                    else doc->Undo();
                    EditorEnsureCaretVisible();
                    Invalidate();
                }
                handled = true;
                break;
            case 'Y':
                if (doc) {
                    doc->EndEditGroup();
                    doc->Redo();
                    EditorEnsureCaretVisible();
                    Invalidate();
                }
                handled = true;
                break;
            case 'A':
                if (doc) {
                    doc->EndEditGroup();
                    doc->SelectAll();
                    ResetBlink();
                    Invalidate();
                }
                handled = true;
                break;
            case 'C': EditorCopy(false); handled = true; break;
            case 'X': EditorCopy(true); handled = true; break;
            case 'V': EditorPaste(); handled = true; break;
            case 'F':
                if (shift) {
                    PromptOpenFolder();
                } else {
                    OpenFind();
                }
                handled = true;
                break;
            case 'G': OpenGotoLine(); handled = true; break;
            case 'P':
                if (shift) OpenPalette();
                else OpenQuickOpen();
                handled = true;
                break;
            case 'B':
                if (!shift) ToggleTerminal();
                handled = true;
                break;
            case 'J': RunCommand(Cmd::TogglePanel); handled = true; break;
            case 'E':
                if (shift) RunCommand(Cmd::ToggleSidebar);
                handled = true;
                break;
            case '=':
            case VK_OEM_PLUS:
            case VK_ADD:
                RunCommand(Cmd::ZoomIn);
                handled = true;
                break;
            case VK_OEM_MINUS:
            case VK_SUBTRACT:
                RunCommand(Cmd::ZoomOut);
                handled = true;
                break;
            case '0':
                RunCommand(Cmd::ZoomReset);
                handled = true;
                break;
            case VK_OEM_COMMA:
                OpenSettings();
                handled = true;
                break;
            case VK_OEM_2:
                EditorToggleComment();
                handled = true;
                break;
            case VK_TAB:
                SwitchTab(shift ? -1 : 1);
                handled = true;
                break;
            default:
                break;
        }
    }
    if (handled) {
        ResetBlink();
        if (IsWordBoundaryKey(key)) EditorEnsureCaretVisible();
        Invalidate();
        return;
    }

    if (HandleTerminalKey(key, ctrl, shift)) {
        if (IsWordBoundaryKey(key)) EditorEnsureCaretVisible();
        return;
    }

    if (key == VK_F5) {
        FocusTerminal();
        ResetBlink();
        return;
    }
    if (key == VK_F3) {
        OpenFind();
        FindNext(shift ? -1 : 1);
        return;
    }
    if (key == VK_ESCAPE) {
        if (doc && doc->HasSelection()) {
            doc->anchor = doc->caret;
            ResetBlink();
            Invalidate();
        }
        return;
    }
    if (key == VK_F1) {
        OpenPalette();
        return;
    }
    if (key == VK_ESCAPE) return;

    if (doc) {
        if (EditorKeyDown(key, ctrl, shift, alt)) {
            ResetBlink();
            Invalidate();
        }
    }
}

void App::OnChar(wchar_t ch) {
    if (ch < 32) return;
    bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
    if (ctrl && !alt) return;
    if (modalOpen_ || openMenu_ >= 0) return;
    if (overlay_ != Overlay::None) {
        OnOverlayChar(ch);
        ResetBlink();
        Invalidate();
        return;
    }
    if (terminalInputFocused_ && settings_.panelVisible) {
        terminalInput_.Insert(std::wstring(1, ch));
        ResetBlink();
        Invalidate();
        return;
    }
    if (ActiveDoc()) {
        EditorTypeCharacter(ch);
        ResetBlink();
        Invalidate();
    }
}

void App::OnTimer() {
    PollProcess();
    ULONGLONG phase = ((GetTickCount64() - blinkResetTick_) / 530) % 2;
    if ((int)phase != lastBlinkPhase_) {
        lastBlinkPhase_ = (int)phase;
        Invalidate();
    }
}

void App::ResetBlink() {
    blinkResetTick_ = GetTickCount64();
    lastBlinkPhase_ = -1;
}

void App::HandleDropFiles(HDROP drop) {
    UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
    std::vector<std::wstring> files;
    for (UINT i = 0; i < count; ++i) {
        UINT len = DragQueryFileW(drop, i, nullptr, 0);
        std::wstring path(len, L'\0');
        DragQueryFileW(drop, i, path.data(), len + 1);
        files.push_back(path);
    }
    DragFinish(drop);
    for (const auto& path : files) {
        std::error_code ec;
        if (std::filesystem::is_directory(path, ec)) {
            SetProject(path);
        } else {
            if (projectRoot_.empty()) SetProject(DirName(path), false);
            OpenFile(path);
        }
    }
}

void App::HandleMenuMouseMove(float x, float y) {
    if (openMenu_ < 0) return;
    if (y >= L_.titleBar.top && y < L_.titleBar.bottom) {
        int label = MenuLabelAt(x, y);
        if (label >= 0 && label != openMenu_) {
            openMenu_ = label;
            Invalidate();
        }
    }
}

bool App::HandleMenuClick(float x, float y) {
    int label = MenuLabelAt(x, y);
    if (label >= 0) {
        openMenu_ = (openMenu_ == label) ? -1 : label;
        Invalidate();
        return true;
    }
    if (openMenu_ >= 0) {
        Rect drop{};
        std::vector<Rect> rects = MenuEntryRects(openMenu_, &drop);
        const auto& entries = menus_[(size_t)openMenu_].entries;
        for (size_t i = 0; i < rects.size() && i < entries.size(); ++i) {
            if (entries[i].separator) continue;
            if (Contains(rects[i], x, y)) {
                Cmd cmd = entries[i].cmd;
                bool enabled = CanRunCommand(cmd);
                openMenu_ = -1;
                if (enabled) RunCommand(cmd);
                Invalidate();
                return true;
            }
        }
        if (!Contains(drop, x, y)) {
            openMenu_ = -1;
            Invalidate();
        }
        return true;
    }
    return false;
}

int App::TitleControlAt(float x, float y) const {
    if (Contains(L_.closeButton, x, y)) return 3;
    if (Contains(L_.maxButton, x, y)) return 2;
    if (Contains(L_.minButton, x, y)) return 1;
    if (Contains(L_.terminalButton, x, y)) return 4;
    return 0;
}

int App::MenuLabelAt(float x, float y) const {
    for (int i = 0; i < 5 && i < (int)menus_.size(); ++i) {
        if (Contains(L_.menuLabels[i], x, y)) return i;
    }
    return -1;
}

bool App::FindBarHit(float x, float y, FindRects& rects) const {
    rects = GetFindRects();
    return Contains(rects.bar, x, y);
}

Color App::StatusColor() const {
    switch (statusKind_) {
        case 1: return theme_.success;
        case 2: return theme_.warning;
        case 3: return theme_.error;
        default: return theme_.info;
    }
}

void App::SetStatus(const std::wstring& text, int kind) {
    statusText_ = text;
    statusKind_ = kind;
    if (hwnd_) Invalidate();
}

int App::VisibleLineCount() const {
    const Document* doc = ActiveDoc();
    if (!doc) return 1;
    int count = (int)std::floor(RectHeight(L_.editor) / std::max(1.0f, lineHeight_));
    return std::max(1, count);
}

// -------------------------------------------------------------- commands/menus

void App::InitCommands() {
    auto add = [&](Cmd id, const wchar_t* label, const wchar_t* shortcut, const wchar_t* category) {
        commands_.push_back(CommandInfo{id, label, shortcut ? shortcut : L"", category});
    };
    add(Cmd::NewFile, L"New File", L"Ctrl+N", L"File");
    add(Cmd::NewFileInFolder, L"New File in Folder…", L"", L"File");
    add(Cmd::OpenFile, L"Open File…", L"Ctrl+O", L"File");
    add(Cmd::OpenFolder, L"Open Folder…", L"Ctrl+Shift+O", L"File");
    add(Cmd::Save, L"Save", L"Ctrl+S", L"File");
    add(Cmd::SaveAs, L"Save As…", L"Ctrl+Shift+S", L"File");
    add(Cmd::SaveAll, L"Save All", L"", L"File");
    add(Cmd::CloseTab, L"Close Editor", L"Ctrl+W", L"File");
    add(Cmd::CloseAll, L"Close All Editors", L"Ctrl+Shift+W", L"File");
    add(Cmd::Exit, L"Exit", L"Alt+F4", L"File");
    add(Cmd::Undo, L"Undo", L"Ctrl+Z", L"Edit");
    add(Cmd::Redo, L"Redo", L"Ctrl+Y", L"Edit");
    add(Cmd::Cut, L"Cut", L"Ctrl+X", L"Edit");
    add(Cmd::Copy, L"Copy", L"Ctrl+C", L"Edit");
    add(Cmd::Paste, L"Paste", L"Ctrl+V", L"Edit");
    add(Cmd::Find, L"Find in File…", L"Ctrl+F", L"Edit");
    add(Cmd::FindNext, L"Find Next", L"F3", L"Edit");
    add(Cmd::FindPrev, L"Find Previous", L"Shift+F3", L"Edit");
    add(Cmd::GoToLine, L"Go to Line…", L"Ctrl+G", L"Edit");
    add(Cmd::CommandPalette, L"Command Palette…", L"Ctrl+Shift+P", L"View");
    add(Cmd::QuickOpen, L"Quick Open File…", L"Ctrl+P", L"View");
    add(Cmd::ToggleSidebar, L"Toggle Sidebar", L"Ctrl+Shift+E", L"View");
    add(Cmd::TogglePanel, L"Toggle Terminal", L"Ctrl+J", L"View");
    add(Cmd::Terminal, L"Focus Terminal", L"F5", L"View");
    add(Cmd::ClearOutput, L"Clear Terminal", L"", L"View");
    add(Cmd::ZoomIn, L"Zoom In", L"Ctrl+=", L"View");
    add(Cmd::ZoomOut, L"Zoom Out", L"Ctrl+-", L"View");
    add(Cmd::ZoomReset, L"Reset Zoom", L"Ctrl+0", L"View");
    add(Cmd::ToggleTheme, L"Toggle Light / Dark Theme", L"", L"View");
    add(Cmd::Settings, L"Settings…", L"Ctrl+,", L"View");
    add(Cmd::Stop, L"Stop Terminal", L"Ctrl+.", L"Terminal");
    add(Cmd::RefreshExplorer, L"Refresh Explorer", L"", L"View");
    add(Cmd::About, L"About NeoIDEs", L"", L"Help");
}

void App::InitMenus() {
    menus_.clear();
    menus_.push_back(MenuDef{L"File", {
        {Cmd::NewFile, false}, {Cmd::NewFileInFolder, false}, {Cmd::OpenFile, false},
        {Cmd::OpenFolder, false}, {Cmd::None, true}, {Cmd::Save, false},
        {Cmd::SaveAs, false}, {Cmd::SaveAll, false}, {Cmd::None, true},
        {Cmd::CloseTab, false}, {Cmd::CloseAll, false}, {Cmd::None, true}, {Cmd::Exit, false}
    }});
    menus_.push_back(MenuDef{L"Edit", {
        {Cmd::Undo, false}, {Cmd::Redo, false}, {Cmd::None, true},
        {Cmd::Cut, false}, {Cmd::Copy, false}, {Cmd::Paste, false}, {Cmd::None, true},
        {Cmd::Find, false}, {Cmd::FindNext, false}, {Cmd::FindPrev, false}, {Cmd::None, true},
        {Cmd::GoToLine, false}
    }});
    menus_.push_back(MenuDef{L"View", {
        {Cmd::CommandPalette, false}, {Cmd::QuickOpen, false}, {Cmd::None, true},
        {Cmd::ToggleSidebar, false}, {Cmd::TogglePanel, false}, {Cmd::ClearOutput, false},
        {Cmd::None, true}, {Cmd::ZoomIn, false}, {Cmd::ZoomOut, false}, {Cmd::ZoomReset, false},
        {Cmd::None, true}, {Cmd::ToggleTheme, false}, {Cmd::Settings, false}
    }});
    menus_.push_back(MenuDef{L"Terminal", {
        {Cmd::Terminal, false}, {Cmd::TogglePanel, false}, {Cmd::None, true},
        {Cmd::ClearOutput, false}, {Cmd::Stop, false}
    }});
    menus_.push_back(MenuDef{L"Help", {
        {Cmd::About, false}
    }});
}

const CommandInfo* App::CommandFor(Cmd cmd) const {
    for (const auto& info : commands_) {
        if (info.cmd == cmd) return &info;
    }
    return nullptr;
}

bool App::CanRunCommand(Cmd cmd) const {
    const Document* doc = ActiveDoc();
    bool running = terminalRunning_;
    switch (cmd) {
        case Cmd::Save:
        case Cmd::SaveAs:
        case Cmd::CloseTab:
        case Cmd::Find:
        case Cmd::FindNext:
        case Cmd::FindPrev:
        case Cmd::GoToLine:
        case Cmd::Paste:
            return doc != nullptr;
        case Cmd::Undo: return doc && doc->CanUndo();
        case Cmd::Redo: return doc && doc->CanRedo();
        case Cmd::Cut:
        case Cmd::Copy:
            return doc && doc->HasSelection();
        case Cmd::SaveAll:
        case Cmd::CloseAll:
            return !docs_.empty();
        case Cmd::Terminal:
            return true;
        case Cmd::Stop:
            return running;
        case Cmd::RefreshExplorer:
            return !projectRoot_.empty();
        case Cmd::NewFileInFolder:
            return !projectRoot_.empty();
        case Cmd::ToggleSidebar:
        case Cmd::TogglePanel:
            return true;
        default:
            return true;
    }
}

Rect App::MenuDropRect(int menuIndex) const {
    float rowH = S(30.0f);
    float gap = S(9.0f);
    float pad = S(6.0f);
    float height = pad * 2;
    int separators = 0;
    for (const auto& e : menus_[(size_t)menuIndex].entries) {
        height += e.separator ? gap : rowH;
        if (e.separator) ++separators;
    }
    float width = S(266.0f);
    float left = L_.menuLabels[menuIndex].left;
    left = std::clamp(left, S(8.0f), std::max(S(8.0f), (float)width_ - width - S(8.0f)));
    Rect r = MakeRect(left, L_.titleBar.bottom + S(4.0f), left + width,
                      L_.titleBar.bottom + S(4.0f) + height);
    return r;
}

std::vector<Rect> App::MenuEntryRects(int menuIndex, Rect* dropRect) const {
    std::vector<Rect> rects;
    if (menuIndex < 0 || menuIndex >= (int)menus_.size()) return rects;
    float rowH = S(30.0f);
    float gap = S(9.0f);
    float pad = S(6.0f);
    Rect drop = MenuDropRect(menuIndex);
    if (dropRect) *dropRect = drop;
    float y = drop.top + pad;
    const auto& entries = menus_[(size_t)menuIndex].entries;
    rects.reserve(entries.size());
    for (const auto& entry : entries) {
        float h = entry.separator ? gap : rowH;
        rects.push_back(MakeRect(drop.left, y, drop.right, y + h));
        y += h;
    }
    return rects;
}

void App::RunCommand(Cmd cmd) {
    Document* doc = ActiveDoc();
    switch (cmd) {
        case Cmd::NewFile:
            NewUntitled();
            break;
        case Cmd::NewFileInFolder:
            NewFileInProject();
            break;
        case Cmd::OpenFile:
            PromptOpenFiles();
            break;
        case Cmd::OpenFolder:
            PromptOpenFolder();
            break;
        case Cmd::Save:
            if (doc) SaveDocument(*doc, false);
            break;
        case Cmd::SaveAs:
            if (doc) SaveDocument(*doc, true);
            break;
        case Cmd::SaveAll:
            SaveAllDocuments();
            break;
        case Cmd::CloseTab:
            if (doc) CloseDocument(doc);
            break;
        case Cmd::CloseAll:
            RequestCloseAll();
            break;
        case Cmd::Exit:
            RequestExit();
            break;
        case Cmd::Undo:
            if (doc) {
                doc->EndEditGroup();
                doc->Undo();
                EditorEnsureCaretVisible();
                Invalidate();
            }
            break;
        case Cmd::Redo:
            if (doc) {
                doc->EndEditGroup();
                doc->Redo();
                EditorEnsureCaretVisible();
                Invalidate();
            }
            break;
        case Cmd::Cut: EditorCopy(true); break;
        case Cmd::Copy: EditorCopy(false); break;
        case Cmd::Paste: EditorPaste(); break;
        case Cmd::Find: OpenFind(); break;
        case Cmd::FindNext: FindNext(1); break;
        case Cmd::FindPrev: FindNext(-1); break;
        case Cmd::GoToLine: OpenGotoLine(); break;
        case Cmd::CommandPalette: OpenPalette(); break;
        case Cmd::QuickOpen: OpenQuickOpen(); break;
        case Cmd::ToggleSidebar:
            settings_.sidebarVisible = !settings_.sidebarVisible;
            UpdateLayout();
            SaveSettings();
            Invalidate();
            break;
        case Cmd::TogglePanel:
            ToggleTerminal();
            SaveSettings();
            break;
        case Cmd::ClearOutput:
            ClearOutput();
            break;
        case Cmd::ZoomIn:
        case Cmd::ZoomOut:
        case Cmd::ZoomReset: {
            int old = settings_.editorFontSize;
            if (cmd == Cmd::ZoomIn) settings_.editorFontSize = std::min(40, settings_.editorFontSize + 1);
            else if (cmd == Cmd::ZoomOut) settings_.editorFontSize = std::max(9, settings_.editorFontSize - 1);
            else settings_.editorFontSize = 15;
            if (settings_.editorFontSize != old) {
                for (auto& d : docs_) d->InvalidateLayouts();
                UpdateEditorMetrics();
                SaveSettings();
                Invalidate();
            }
            break;
        }
        case Cmd::ToggleTheme:
            ApplyTheme(!darkTheme_);
            SaveSettings();
            break;
        case Cmd::Settings:
            OpenSettings();
            break;
        case Cmd::Terminal:
            FocusTerminal();
            break;
        case Cmd::Stop:
            StopProcess();
            break;
        case Cmd::RefreshExplorer:
            if (!projectRoot_.empty()) {
                tree_.Refresh();
                SetStatus(L"Explorer refreshed", 0);
                Invalidate();
            }
            break;
        case Cmd::About:
            overlay_ = Overlay::About;
            Invalidate();
            break;
        default:
            break;
    }
}

bool App::HandleModalKey(WPARAM key) {
    if (key == VK_ESCAPE) {
        FinishModal(modalEscResult_);
        return true;
    }
    if (key == VK_RETURN) {
        FinishModal(modalEnterResult_);
        return true;
    }
    return true;
}
