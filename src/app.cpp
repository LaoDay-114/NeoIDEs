#include "app.h"

#include <dwmapi.h>
#include <imm.h>

namespace {

constexpr wchar_t kWindowClass[] = L"NeoIDEs.MainWindow";
constexpr UINT_PTR kTimerId = 1;
constexpr int kIconResource = 101;

UINT DpiForWindowSafe(HWND hwnd) {
    using Fn = UINT(WINAPI*)(HWND);
    static Fn fn = reinterpret_cast<Fn>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
    if (fn) return fn(hwnd);
    HDC dc = GetDC(hwnd);
    UINT dpi = 96;
    if (dc) {
        dpi = (UINT)GetDeviceCaps(dc, LOGPIXELSX);
        ReleaseDC(hwnd, dc);
    }
    return dpi ? dpi : 96;
}

std::wstring EncodingLabel(Encoding encoding) {
    switch (encoding) {
        case Encoding::Utf8Bom: return L"UTF-8 BOM";
        case Encoding::Utf16LE: return L"UTF-16 LE";
        default: return L"UTF-8";
    }
}

int ParseInt(const std::wstring& value, int fallback) {
    try {
        size_t used = 0;
        int v = std::stoi(value, &used);
        return used == 0 ? fallback : v;
    } catch (...) {
        return fallback;
    }
}

float ParseFloat(const std::wstring& value, float fallback) {
    try {
        size_t used = 0;
        float v = std::stof(value, &used);
        return used == 0 ? fallback : v;
    } catch (...) {
        return fallback;
    }
}

bool ParseBool(const std::wstring& value, bool fallback) {
    if (IEquals(value, L"1") || IEquals(value, L"true") || IEquals(value, L"yes")) return true;
    if (IEquals(value, L"0") || IEquals(value, L"false") || IEquals(value, L"no")) return false;
    return fallback;
}

} // namespace

App::~App() {
    if (process_) {
        process_->Stop();
        process_.reset();
    }
    renderer_.Shutdown();
}

// ----------------------------------------------------------------- lifecycle

bool App::Create(HINSTANCE hInstance, int nCmdShow) {
    hinst_ = hInstance;
    LoadSettings();
    darkTheme_ = settings_.darkTheme;
    theme_ = darkTheme_ ? MakeDarkTheme() : MakeLightTheme();

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        for (int i = 1; i < argc; ++i) startupPaths_.push_back(argv[i]);
        LocalFree(argv);
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &App::StaticWndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(kIconResource));
    if (!wc.hIcon) wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hIconSm = wc.hIcon;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kWindowClass;
    RegisterClassExW(&wc);
    icon_ = wc.hIcon;

    RECT workArea{};
    if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0)) {
        workArea = RECT{0, 0, 1920, 1080};
    }
    int availW = workArea.right - workArea.left;
    int availH = workArea.bottom - workArea.top;
    int width = std::min(1480, std::max(900, availW - 100));
    int height = std::min(940, std::max(600, availH - 100));
    int x = workArea.left + (availW - width) / 2;
    int y = workArea.top + (availH - height) / 2;

    hwnd_ = CreateWindowExW(
        WS_EX_APPWINDOW,
        kWindowClass, L"NeoIDEs",
        WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU | WS_CLIPCHILDREN,
        x, y, width, height,
        nullptr, nullptr, hInstance, this);
    if (!hwnd_) return false;

    if (icon_) {
        SendMessageW(hwnd_, WM_SETICON, ICON_BIG, (LPARAM)icon_);
        SendMessageW(hwnd_, WM_SETICON, ICON_SMALL, (LPARAM)icon_);
    }

    ShowWindow(hwnd_, nCmdShow);
    UpdateWindow(hwnd_);
    SetForegroundWindow(hwnd_);
    return true;
}

int App::Run() {
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.hwnd == hwnd_ && msg.message == WM_KEYDOWN) {
            // handled through DispatchMessage
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}

LRESULT CALLBACK App::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    App* app = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        app = reinterpret_cast<App*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    if (app) return app->WndProc(hwnd, msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void App::OnCreate() {
    hwnd_ = hwnd_;
    dpi_ = DpiForWindowSafe(hwnd_) / 96.0f;
    if (!renderer_.Init(hwnd_)) {
        MessageBoxW(hwnd_, L"Could not initialize Direct2D / DirectWrite.", L"NeoIDEs", MB_ICONERROR);
        PostQuitMessage(1);
        return;
    }
    ApplyTheme(settings_.darkTheme);
    InitCommands();
    InitMenus();
    RECT client{};
    GetClientRect(hwnd_, &client);
    width_ = client.right - client.left;
    height_ = client.bottom - client.top;
    renderer_.Resize((UINT)width_, (UINT)height_);
    UpdateLayout();
    UpdateEditorMetrics();

    RestoreStartup();
    blinkResetTick_ = GetTickCount64();
    SetTimer(hwnd_, kTimerId, 50, nullptr);
    DragAcceptFiles(hwnd_, TRUE);
    SetStatus(L"Ready", 0);
    UpdateWindowTitle();
    Invalidate();
}

void App::OnDestroy() {
    KillTimer(hwnd_, kTimerId);
    DragAcceptFiles(hwnd_, FALSE);
    if (process_) {
        process_->Stop();
        process_.reset();
    }
    if (auto* doc = ActiveDoc()) doc->EndEditGroup();
    SaveSettings();
    PostQuitMessage(0);
}

void App::LoadSettings() {
    Settings defaults;
    settings_ = defaults;
    std::wstring file = SettingsFilePath();
    std::string bytes;
    settingsLoaded_ = true;
    if (!ReadBinaryFile(file, bytes) || bytes.empty()) return;
    std::wstring content = Utf8ToWide(bytes);
    std::map<int, std::wstring> recents;
    std::map<int, std::wstring> openFiles;
    int maxRecent = -1;
    int maxOpen = -1;
    size_t pos = 0;
    while (pos <= content.size()) {
        size_t nl = content.find(L'\n', pos);
        std::wstring line = content.substr(pos, nl == std::wstring::npos ? std::wstring::npos : nl - pos);
        pos = nl == std::wstring::npos ? content.size() + 1 : nl + 1;
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        line = Trim(line);
        if (line.empty() || line[0] == L'#') continue;
        size_t eq = line.find(L'=');
        if (eq == std::wstring::npos) continue;
        std::wstring key = Trim(line.substr(0, eq));
        std::wstring value = line.substr(eq + 1);
        if (key == L"theme") settings_.darkTheme = !IEquals(value, L"light");
        else if (key == L"font") settings_.editorFontSize = std::clamp(ParseInt(value, 15), 9, 40);
        else if (key == L"sidebarVisible") settings_.sidebarVisible = ParseBool(value, true);
        else if (key == L"sidebarWidth") settings_.sidebarWidth = std::clamp(ParseFloat(value, 264.0f), 200.0f, 520.0f);
        else if (key == L"panelVisible") settings_.panelVisible = ParseBool(value, false);
        else if (key == L"panelHeight") settings_.panelHeight = std::clamp(ParseFloat(value, 220.0f), 100.0f, 700.0f);
        else if (key == L"showHidden") settings_.showHidden = ParseBool(value, false);
        else if (key == L"restoreSession") settings_.restoreSession = ParseBool(value, true);
        else if (key == L"wordWrap") settings_.wordWrap = ParseBool(value, false);
        else if (key == L"terminalUtf8") settings_.terminalUtf8 = ParseBool(value, true);
        else if (key == L"project") settings_.projectRoot = value;
        else if (key == L"activeFile") settings_.activeFile = ParseInt(value, 0);
        else if (IStartsWith(key, L"recent.")) {
            int index = ParseInt(key.substr(7), -1);
            if (index >= 0) {
                recents[index] = value;
                maxRecent = std::max(maxRecent, index);
            }
        } else if (IStartsWith(key, L"open.")) {
            int index = ParseInt(key.substr(5), -1);
            if (index >= 0) {
                openFiles[index] = value;
                maxOpen = std::max(maxOpen, index);
            }
        }
    }
    settings_.recentProjects.clear();
    for (int i = 0; i <= maxRecent; ++i) {
        auto it = recents.find(i);
        if (it != recents.end() && !it->second.empty()) settings_.recentProjects.push_back(it->second);
    }
    settings_.openFiles.clear();
    for (int i = 0; i <= maxOpen; ++i) {
        auto it = openFiles.find(i);
        if (it != openFiles.end() && !it->second.empty()) settings_.openFiles.push_back(it->second);
    }
}

void App::SaveSettings() {
    if (!settingsLoaded_) return;
    settings_.darkTheme = darkTheme_;
    std::vector<std::wstring> openFiles;
    int activeFileIndex = 0;
    for (const auto& doc : docs_) {
        if (!doc->path.empty()) {
            if (active_ >= 0 && docs_[(size_t)active_].get() == doc.get()) {
                activeFileIndex = (int)openFiles.size();
            }
            openFiles.push_back(doc->path);
            if (openFiles.size() >= 20) break;
        }
    }
    settings_.openFiles = openFiles;
    settings_.activeFile = activeFileIndex;
    settings_.projectRoot = projectRoot_;

    std::wostringstream out;
    out << L"# NeoIDEs settings\n";
    out << L"theme=" << (darkTheme_ ? L"dark" : L"light") << L"\n";
    out << L"font=" << settings_.editorFontSize << L"\n";
    out << L"sidebarVisible=" << (settings_.sidebarVisible ? 1 : 0) << L"\n";
    out << L"sidebarWidth=" << (int)std::lround(settings_.sidebarWidth) << L"\n";
    out << L"panelVisible=" << (settings_.panelVisible ? 1 : 0) << L"\n";
    out << L"panelHeight=" << (int)std::lround(settings_.panelHeight) << L"\n";
    out << L"showHidden=" << (settings_.showHidden ? 1 : 0) << L"\n";
    out << L"restoreSession=" << (settings_.restoreSession ? 1 : 0) << L"\n";
    out << L"wordWrap=" << (settings_.wordWrap ? 1 : 0) << L"\n";
    out << L"terminalUtf8=" << (settings_.terminalUtf8 ? 1 : 0) << L"\n";
    out << L"project=" << projectRoot_ << L"\n";
    out << L"activeFile=" << settings_.activeFile << L"\n";
    for (size_t i = 0; i < settings_.recentProjects.size() && i < 8; ++i) {
        out << L"recent." << i << L"=" << settings_.recentProjects[i] << L"\n";
    }
    for (size_t i = 0; i < settings_.openFiles.size() && i < 20; ++i) {
        out << L"open." << i << L"=" << settings_.openFiles[i] << L"\n";
    }
    std::wstring text = out.str();
    std::ofstream f(std::filesystem::path(SettingsFilePath()), std::ios::binary | std::ios::trunc);
    if (!f) return;
    std::string bytes = WideToUtf8(text);
    f.write(bytes.data(), (std::streamsize)bytes.size());
}

std::wstring App::SettingsFilePath() const {
    return AppDataDirectory() + L"\\settings.ini";
}

void App::ApplyTheme(bool dark) {
    darkTheme_ = dark;
    settings_.darkTheme = dark;
    theme_ = dark ? MakeDarkTheme() : MakeLightTheme();
    if (hwnd_) {
        UpdateDwmStyle();
        Invalidate();
    }
}

void App::UpdateDwmStyle() {
    if (!hwnd_) return;
    BOOL dark = darkTheme_ ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd_, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    int corner = 2; // DWMWCP_ROUND
    DwmSetWindowAttribute(hwnd_, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
    COLORREF border = RGB(
        (BYTE)std::lround(std::clamp(theme_.border.r, 0.0f, 1.0f) * 255.0f),
        (BYTE)std::lround(std::clamp(theme_.border.g, 0.0f, 1.0f) * 255.0f),
        (BYTE)std::lround(std::clamp(theme_.border.b, 0.0f, 1.0f) * 255.0f));
    DwmSetWindowAttribute(hwnd_, DWMWA_BORDER_COLOR, &border, sizeof(border));
}

void App::UpdateWindowTitle() {
    if (!hwnd_) return;
    std::wstring title = L"NeoIDEs";
    if (!projectRoot_.empty()) title += L" — " + BaseName(projectRoot_);
    SetWindowTextW(hwnd_, title.c_str());
}

void App::SetProject(const std::wstring& path, bool saveRecent) {
    std::wstring normalized = NormalizePath(path);
    std::error_code ec;
    if (!std::filesystem::exists(normalized, ec) ||
        !std::filesystem::is_directory(normalized, ec)) {
        SetStatus(L"Folder no longer exists.", 3);
        return;
    }
    projectRoot_ = normalized;
    tree_.SetRoot(projectRoot_, settings_.showHidden);
    treeSelectedPath_.clear();
    treeScroll_ = 0;
    quickFilesDirty_ = true;
    settings_.projectRoot = projectRoot_;
    if (saveRecent) AddRecentProject(projectRoot_);
    UpdateWindowTitle();
    SetStatus(L"Opened folder " + BaseName(projectRoot_), 1);
    Invalidate();
}

void App::AddRecentProject(const std::wstring& path) {
    std::wstring normalized = NormalizePath(path);
    settings_.recentProjects.erase(
        std::remove_if(settings_.recentProjects.begin(), settings_.recentProjects.end(),
                       [&](const std::wstring& p) { return SamePath(p, normalized); }),
        settings_.recentProjects.end());
    settings_.recentProjects.insert(settings_.recentProjects.begin(), normalized);
    if (settings_.recentProjects.size() > 8) settings_.recentProjects.resize(8);
}

void App::RestoreStartup() {
    std::vector<std::wstring> paths;
    for (const auto& path : startupPaths_) {
        std::error_code ec;
        if (std::filesystem::exists(path, ec)) paths.push_back(path);
    }
    if (!paths.empty()) {
        for (const auto& path : paths) {
            std::error_code ec;
            if (std::filesystem::is_directory(path, ec)) SetProject(path);
            else {
                if (projectRoot_.empty()) SetProject(DirName(path), false);
                OpenFile(path, active_ < 0);
            }
        }
        return;
    }

    if (settings_.restoreSession) {
        bool projectOpened = false;
        std::error_code ec;
        if (!settings_.projectRoot.empty() && std::filesystem::is_directory(settings_.projectRoot, ec)) {
            SetProject(settings_.projectRoot, false);
            projectOpened = true;
        }
        int opened = 0;
        for (const auto& file : settings_.openFiles) {
            std::error_code fileError;
            if (!std::filesystem::exists(file, fileError)) continue;
            if (OpenFile(file, false)) ++opened;
        }
        if (!settings_.openFiles.empty() && active_ < 0 && !docs_.empty()) {
            int wanted = std::clamp(settings_.activeFile, 0, (int)docs_.size() - 1);
            ActivateDoc(wanted);
        }
        (void)projectOpened;
        (void)opened;
    }
}

void App::UpdateLayout() {
    if (width_ <= 0 || height_ <= 0) return;
    float w = (float)width_;
    float h = (float)height_;
    float titleH = S(40.0f);
    float statusH = S(26.0f);
    float railW = S(52.0f);
    bool hasDocs = !docs_.empty();
    bool sidebarVisible = settings_.sidebarVisible;
    float sidebarW = sidebarVisible ? std::clamp(S(settings_.sidebarWidth), S(210.0f), std::min(S(500.0f), w * 0.46f)) : 0.0f;
    float contentTop = titleH;
    float contentBottom = h - statusH;
    float panelH = 0.0f;
    if (settings_.panelVisible && hasDocs) {
        panelH = std::clamp(S(settings_.panelHeight), S(110.0f), std::max(S(120.0f), (contentBottom - contentTop) * 0.62f));
    }
    float tabH = hasDocs ? S(38.0f) : 0.0f;

    L_ = LayoutInfo{};
    L_.titleBar = MakeRect(0, 0, w, titleH);
    L_.statusBar = MakeRect(0, h - statusH, w, h);
    L_.rail = MakeRect(0, contentTop, railW, contentBottom);
    L_.sidebar = MakeRect(railW, contentTop, railW + sidebarW, contentBottom);
    L_.sidebarSplitter = sidebarW > 0 ? MakeRect(railW + sidebarW - S(2.0f), contentTop,
                                                 railW + sidebarW + S(3.0f), contentBottom)
                                      : Rect{};
    float mainLeft = railW + sidebarW;
    L_.main = MakeRect(mainLeft, contentTop, w, contentBottom);
    L_.tabBar = MakeRect(mainLeft, contentTop, w, contentTop + tabH);
    L_.editor = MakeRect(mainLeft, contentTop + tabH, w, contentBottom - panelH);
    L_.panel = panelH > 0 ? MakeRect(mainLeft, contentBottom - panelH, w, contentBottom) : Rect{};
    L_.panelSplitter = panelH > 0 ? MakeRect(mainLeft, contentBottom - panelH - S(3.0f), w,
                                             contentBottom - panelH + S(3.0f))
                                  : Rect{};

    float btnH = S(28.0f);
    float btnY = (titleH - btnH) * 0.5f;
    L_.closeButton = MakeRect(w - S(46.0f), 0, w, titleH);
    L_.maxButton = MakeRect(w - S(92.0f), 0, w - S(46.0f), titleH);
    L_.minButton = MakeRect(w - S(138.0f), 0, w - S(92.0f), titleH);
    float terminalRight = L_.minButton.left - S(12.0f);
    L_.terminalButton = MakeRect(terminalRight - S(104.0f), btnY, terminalRight, btnY + btnH);

    TextStyle titleStyle;
    titleStyle.size = S(14.0f);
    titleStyle.weight = 650;
    float nameWidth = renderer_.Measure(L"NeoIDEs", titleStyle).width;
    float menuX = S(14.0f) + S(24.0f) + S(10.0f) + nameWidth + S(22.0f);
    if (!menus_.empty()) {
        for (size_t i = 0; i < menus_.size() && i < 5; ++i) {
            TextStyle menuStyle;
            menuStyle.size = S(12.5f);
            menuStyle.weight = 500;
            float labelW = renderer_.Measure(menus_[i].title, menuStyle).width + S(20.0f);
            L_.menuLabels[i] = MakeRect(menuX, S(7.0f), menuX + labelW, titleH - S(7.0f));
            menuX += labelW + S(2.0f);
        }
    }
}

void App::UpdateEditorMetrics() {
    editorFontPx_ = std::max(9.0f, S((float)settings_.editorFontSize));
    lineHeight_ = std::round(editorFontPx_ * 1.58f);
    TextStyle mono;
    mono.size = editorFontPx_;
    mono.family = renderer_.MonoFont();
    mono.weight = 400;
    mono.ellipsis = false;
    D2D1_SIZE_F m = renderer_.Measure(L"M", mono);
    monoCharWidth_ = m.width > 0 ? m.width : editorFontPx_ * 0.6f;
    int digits = 1;
    const Document* doc = ActiveDoc();
    int lines = doc ? doc->LineCount() : 1;
    digits = (int)std::to_wstring(std::max(1, lines)).size();
    gutterWidth_ = std::max(S(54.0f), digits * monoCharWidth_ + S(26.0f));
    if (lineHeight_ < 1) lineHeight_ = S(20.0f);
}

void App::Invalidate() {
    if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
}

// ------------------------------------------------------------------ painting

void App::Paint(HDC) {
    if (!renderer_.DWrite()) return;
    renderer_.BeginDraw();
    if (!renderer_.IsReady()) return;
    UpdateLayout();
    UpdateEditorMetrics();

    renderer_.Clear(theme_.windowBg);
    renderer_.FillRect(MakeRect(0, 0, (float)width_, (float)height_), theme_.windowBg);
    renderer_.FillRect(L_.titleBar, theme_.titleBar);
    renderer_.FillRect(L_.rail, theme_.railBg);
    if (settings_.sidebarVisible) renderer_.FillRect(L_.sidebar, theme_.sidebarBg);
    renderer_.FillRect(MakeRect(L_.main.left, L_.main.top, L_.main.right, L_.main.bottom), theme_.editorBg);
    if (!docs_.empty()) {
        renderer_.FillRect(L_.tabBar, theme_.tabBarBg);
        renderer_.FillRect(L_.editor, theme_.editorBg);
        if (settings_.panelVisible) renderer_.FillRect(L_.panel, theme_.panelBg);
    }
    renderer_.FillRect(L_.statusBar, theme_.statusBg);

    DrawRail();
    if (settings_.sidebarVisible) DrawSidebar();
    if (docs_.empty()) {
        DrawWelcome();
    } else {
        DrawTabs();
        DrawEditor();
        if (settings_.panelVisible) DrawOutputPanel();
    }
    DrawStatusBar();
    DrawTitleBar();
    if (openMenu_ >= 0) DrawMenus();
    DrawOverlay();
    if (modalOpen_) DrawModal();

    renderer_.EndDraw();
}

void App::DrawTitleBar() {
    float titleH = RectHeight(L_.titleBar);
    renderer_.FillRect(L_.titleBar, theme_.titleBar);
    renderer_.FillRect(MakeRect(0, titleH - S(1.0f), (float)width_, titleH), theme_.border);

    float logoSize = S(24.0f);
    DrawLogo(S(14.0f), (titleH - logoSize) * 0.5f, logoSize);

    TextStyle nameStyle;
    nameStyle.size = S(14.0f);
    nameStyle.weight = 650;
    nameStyle.vAlign = 1;
    TextStyle measured = nameStyle;
    float nameWidth = renderer_.Measure(L"NeoIDEs", measured).width;
    renderer_.Text(L"NeoIDEs", MakeRect(S(14.0f) + logoSize + S(10.0f), 0,
                                        S(14.0f) + logoSize + S(10.0f) + nameWidth + S(2.0f), titleH),
                   theme_.text, nameStyle);

    TextStyle menuStyle;
    menuStyle.size = S(12.5f);
    menuStyle.weight = 500;
    menuStyle.hAlign = 1;
    menuStyle.vAlign = 1;
    for (size_t i = 0; i < menus_.size() && i < 5; ++i) {
        const Rect& r = L_.menuLabels[i];
        bool hover = Contains(r, mouseX_, mouseY_) || openMenu_ == (int)i;
        if (hover) {
            renderer_.FillRoundedRect(r, S(6.0f), openMenu_ == (int)i ? theme_.accentSoft : theme_.hover);
        }
        renderer_.Text(menus_[i].title, r,
                       openMenu_ == (int)i ? theme_.text : theme_.textDim, menuStyle);
    }

    bool terminalHover = Contains(L_.terminalButton, mouseX_, mouseY_);
    if (settings_.panelVisible) {
        renderer_.FillRoundedRect(L_.terminalButton, S(7.0f), theme_.accentSoft);
        renderer_.StrokeRoundedRect(L_.terminalButton, S(7.0f),
                                    WithAlpha(theme_.accent, 0.45f), 1.0f);
    } else if (terminalHover) {
        renderer_.FillRoundedRect(L_.terminalButton, S(7.0f), theme_.hover);
        renderer_.StrokeRoundedRect(L_.terminalButton, S(7.0f), theme_.borderSoft, 1.0f);
    } else {
        renderer_.StrokeRoundedRect(L_.terminalButton, S(7.0f), theme_.borderSoft, 1.0f);
    }
    TextStyle terminalStyle;
    terminalStyle.size = S(11.5f);
    terminalStyle.weight = 600;
    terminalStyle.hAlign = 1;
    terminalStyle.vAlign = 1;
    TextStyle terminalIcon;
    terminalIcon.family = renderer_.IconFont();
    terminalIcon.size = S(13.0f);
    terminalIcon.vAlign = 1;
    renderer_.Text(L"\uE756",
                   MakeRect(L_.terminalButton.left + S(10.0f), L_.terminalButton.top,
                            L_.terminalButton.left + S(28.0f), L_.terminalButton.bottom),
                   settings_.panelVisible ? theme_.accent : theme_.textDim, terminalIcon);
    renderer_.Text(L"Terminal",
                   MakeRect(L_.terminalButton.left + S(30.0f), L_.terminalButton.top,
                            L_.terminalButton.right - S(8.0f), L_.terminalButton.bottom),
                   settings_.panelVisible ? theme_.accent : theme_.textDim, terminalStyle);

    auto drawWindowButton = [&](const Rect& r, bool close, int type) {
        bool hover = Contains(r, mouseX_, mouseY_);
        Color bg = close && hover ? theme_.error : (hover ? theme_.hover : Color{0, 0, 0, 0});
        if (bg.a > 0.01f) renderer_.FillRect(r, bg);
        Color fg = close && hover ? Col(0xFFFFFF) : theme_.textDim;
        float cx = (r.left + r.right) * 0.5f;
        float cy = (r.top + r.bottom) * 0.5f;
        float d = S(5.0f);
        if (type == 0) {
            renderer_.DrawLine(cx - d, cy, cx + d, cy, fg, S(1.2f));
        } else if (type == 1) {
            if (IsZoomed(hwnd_)) {
                renderer_.StrokeRect(MakeRect(cx - d, cy - d + S(1.5f), cx + d - S(1.5f), cy + d),
                                     fg, S(1.2f));
                renderer_.StrokeRect(MakeRect(cx - d + S(1.5f), cy - d, cx + d, cy + d - S(1.5f)),
                                     fg, S(1.2f));
            } else {
                renderer_.StrokeRect(MakeRect(cx - d, cy - d, cx + d, cy + d), fg, S(1.2f));
            }
        } else {
            renderer_.DrawLine(cx - d, cy - d, cx + d, cy + d, fg, S(1.2f));
            renderer_.DrawLine(cx + d, cy - d, cx - d, cy + d, fg, S(1.2f));
        }
    };
    drawWindowButton(L_.minButton, false, 0);
    drawWindowButton(L_.maxButton, false, 1);
    drawWindowButton(L_.closeButton, true, 2);
}

void App::DrawLogo(float x, float y, float size) {
    Rect outer = MakeRect(x, y, x + size, y + size);
    renderer_.FillRoundedRect(outer, size * 0.27f, theme_.accent);
    TextStyle style;
    style.size = size * 0.62f;
    style.weight = 800;
    style.hAlign = 1;
    style.vAlign = 1;
    style.family = renderer_.UiFont();
    renderer_.Text(L"N", outer, Col(0xFFFFFF), style);
    float dot = std::max(2.0f, size * 0.10f);
    renderer_.DrawEllipse(x + size * 0.76f, y + size * 0.24f, dot, dot,
                          WithAlpha(Col(0xFFFFFF), 0.9f), true);
}

void App::DrawRail() {
    renderer_.FillRect(L_.rail, theme_.railBg);
    renderer_.FillRect(MakeRect(L_.rail.right - 1, L_.rail.top, L_.rail.right, L_.rail.bottom),
                       theme_.borderSoft);
    float bx = (L_.rail.left + L_.rail.right) * 0.5f;
    float btn = S(40.0f);
    float gap = S(46.0f);
    float top = L_.rail.top + S(9.0f);
    struct RailItem {
        const wchar_t* glyph;
        int action;
        bool active;
    };
    RailItem items[3] = {
        {L"\uE8B7", 0, settings_.sidebarVisible},
        {L"\uE721", 1, overlay_ == Overlay::Find},
        {L"\uE768", 2, settings_.panelVisible},
    };
    TextStyle icon;
    icon.size = S(19.0f);
    icon.family = renderer_.IconFont();
    icon.hAlign = 1;
    icon.vAlign = 1;
    TextStyle smallIcon = icon;
    smallIcon.size = S(20.0f);
    for (int i = 0; i < 3; ++i) {
        Rect r = MakeRect(bx - btn * 0.5f, top + i * gap, bx + btn * 0.5f, top + i * gap + btn);
        bool hover = Contains(r, mouseX_, mouseY_);
        if (items[i].active) {
            renderer_.FillRoundedRect(r, S(9.0f), theme_.accentSoft);
            renderer_.FillRoundedRect(MakeRect(r.left - S(9.0f), r.top + S(10.0f),
                                               r.left - S(6.0f), r.bottom - S(10.0f)),
                                      S(1.5f), theme_.accent);
        } else if (hover) {
            renderer_.FillRoundedRect(r, S(9.0f), theme_.hover);
        }
        Color color = items[i].active ? theme_.accent : (hover ? theme_.text : theme_.textDim);
        renderer_.Text(items[i].glyph, r, color, smallIcon);
    }
    Rect settingsRect = MakeRect(bx - btn * 0.5f, L_.rail.bottom - S(58.0f),
                                 bx + btn * 0.5f, L_.rail.bottom - S(18.0f));
    bool hover = Contains(settingsRect, mouseX_, mouseY_);
    if (overlay_ == Overlay::Settings) {
        renderer_.FillRoundedRect(settingsRect, S(9.0f), theme_.accentSoft);
    } else if (hover) {
        renderer_.FillRoundedRect(settingsRect, S(9.0f), theme_.hover);
    }
    renderer_.Text(L"\uE713", settingsRect,
                   overlay_ == Overlay::Settings ? theme_.accent : (hover ? theme_.text : theme_.textDim),
                   icon);
}

void App::DrawStatusBar() {
    renderer_.FillRect(L_.statusBar, theme_.statusBg);
    renderer_.FillRect(MakeRect(0, L_.statusBar.top, (float)width_, L_.statusBar.top + S(1.0f)),
                       theme_.borderSoft);
    float cy = (L_.statusBar.top + L_.statusBar.bottom) * 0.5f;
    renderer_.DrawEllipse(S(14.0f), cy, S(3.0f), S(3.0f), StatusColor(), true);
    TextStyle status;
    status.size = S(11.5f);
    status.vAlign = 1;
    float leftWidth = std::max(S(160.0f), (float)width_ * 0.5f);
    renderer_.Text(statusText_, MakeRect(S(24.0f), L_.statusBar.top, leftWidth, L_.statusBar.bottom),
                   theme_.textDim, status);

    std::vector<std::wstring> items;
    std::vector<Color> colors;
    if (const Document* doc = ActiveDoc()) {
        items.push_back(L"Ln " + std::to_wstring(doc->caret.line + 1) + L", Col " +
                        std::to_wstring(doc->caret.col + 1));
        colors.push_back(theme_.textDim);
        items.push_back(L"Spaces: 4");
        colors.push_back(theme_.textDim);
        items.push_back(EncodingLabel(doc->encoding));
        colors.push_back(theme_.textDim);
        items.push_back(doc->eol == L"\r\n" ? L"CRLF" : L"LF");
        colors.push_back(theme_.textDim);
        items.push_back(LanguageName(doc->language));
        colors.push_back(theme_.textDim);
    }
    TextStyle itemStyle;
    itemStyle.size = S(11.5f);
    itemStyle.hAlign = 2;
    itemStyle.vAlign = 1;
    float x = (float)width_ - S(14.0f);
    for (size_t i = 0; i < items.size(); ++i) {
        float w = renderer_.Measure(items[i], itemStyle).width;
        renderer_.Text(items[i], MakeRect(x - w - S(6.0f), L_.statusBar.top, x, L_.statusBar.bottom),
                       colors[i], itemStyle);
        x -= w + S(22.0f);
        if (x < width_ * 0.55f) break;
    }
}

void App::DrawMenus() {
    if (openMenu_ < 0 || openMenu_ >= (int)menus_.size()) return;
    Rect drop{};
    std::vector<Rect> rects = MenuEntryRects(openMenu_, &drop);
    renderer_.FillRoundedRect(OffsetRect(drop, S(2.0f), S(5.0f)), S(9.0f), theme_.shadow);
    renderer_.FillRoundedRect(drop, S(9.0f), theme_.panelBg);
    renderer_.StrokeRoundedRect(drop, S(9.0f), theme_.border, 1.0f);

    const auto& entries = menus_[(size_t)openMenu_].entries;
    TextStyle labelStyle;
    labelStyle.size = S(12.5f);
    labelStyle.vAlign = 1;
    TextStyle shortcutStyle = labelStyle;
    shortcutStyle.size = S(10.5f);
    shortcutStyle.hAlign = 2;
    for (size_t i = 0; i < entries.size() && i < rects.size(); ++i) {
        const auto& entry = entries[i];
        const Rect& r = rects[i];
        if (entry.separator) {
            renderer_.FillRect(MakeRect(r.left + S(10.0f), (r.top + r.bottom) * 0.5f,
                                        r.right - S(10.0f), (r.top + r.bottom) * 0.5f + 1.0f),
                               theme_.borderSoft);
            continue;
        }
        const CommandInfo* info = CommandFor(entry.cmd);
        if (!info) continue;
        bool enabled = CanRunCommand(entry.cmd);
        bool hover = Contains(r, mouseX_, mouseY_) && enabled;
        if (hover) {
            renderer_.FillRoundedRect(Inset(r, S(3.0f)), S(5.0f), theme_.accentSoft);
            renderer_.FillRoundedRect(MakeRect(r.left + S(3.0f), r.top + S(6.0f),
                                                r.left + S(5.0f), r.bottom - S(6.0f)),
                                      S(1.0f), theme_.accent);
        }
        renderer_.Text(info->label,
                       MakeRect(r.left + S(14.0f), r.top, r.right - S(82.0f), r.bottom),
                       enabled ? (hover ? theme_.text : theme_.textDim) : theme_.textFaint, labelStyle);
        renderer_.Text(info->shortcut,
                       MakeRect(r.right - S(78.0f), r.top, r.right - S(12.0f), r.bottom),
                       enabled ? theme_.textFaint : WithAlpha(theme_.textFaint, 0.55f), shortcutStyle);
    }
}


