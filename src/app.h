#pragma once

#include "common.h"
#include "renderer.h"
#include "document.h"
#include "file_tree.h"
#include "runner.h"

enum class Cmd {
    None,
    NewFile,
    NewFileInFolder,
    OpenFile,
    OpenFolder,
    Save,
    SaveAs,
    SaveAll,
    CloseTab,
    CloseAll,
    Exit,
    Undo,
    Redo,
    Cut,
    Copy,
    Paste,
    Find,
    FindNext,
    FindPrev,
    GoToLine,
    CommandPalette,
    QuickOpen,
    ToggleSidebar,
    TogglePanel,
    ClearOutput,
    ZoomIn,
    ZoomOut,
    ZoomReset,
    ToggleTheme,
    Settings,
    Terminal,
    Stop,
    RefreshExplorer,
    About
};

struct CommandInfo {
    Cmd cmd = Cmd::None;
    std::wstring label;
    std::wstring shortcut;
    std::wstring category;
};

struct MenuEntryDef {
    Cmd cmd = Cmd::None;
    bool separator = false;
};

struct MenuDef {
    std::wstring title;
    std::vector<MenuEntryDef> entries;
};

struct Settings {
    bool darkTheme = true;
    int editorFontSize = 15;
    float sidebarWidth = 264.0f;
    float panelHeight = 220.0f;
    bool sidebarVisible = true;
    bool panelVisible = false;
    bool showHidden = false;
    bool restoreSession = true;
    bool wordWrap = false;
    bool terminalUtf8 = true;
    bool chineseUi = false;
    std::wstring projectRoot;
    std::vector<std::wstring> recentProjects;
    std::vector<std::wstring> openFiles;
    int activeFile = 0;
};

struct TextField {
    std::wstring text;
    int caret = 0;
    int anchor = 0;
    float scroll = 0.0f;

    bool HasSelection() const { return caret != anchor; }
    std::pair<int, int> Selection() const {
        return caret < anchor ? std::make_pair(caret, anchor) : std::make_pair(anchor, caret);
    }
    void Clamp() {
        caret = std::clamp(caret, 0, (int)text.size());
        anchor = std::clamp(anchor, 0, (int)text.size());
        if (caret > 0 && caret < (int)text.size() &&
            IsHighSurrogate(text[(size_t)caret - 1]) && IsLowSurrogate(text[(size_t)caret])) {
            --caret;
        }
        if (anchor > 0 && anchor < (int)text.size() &&
            IsHighSurrogate(text[(size_t)anchor - 1]) && IsLowSurrogate(text[(size_t)anchor])) {
            --anchor;
        }
    }
    void SetText(const std::wstring& value) {
        text = value;
        caret = (int)text.size();
        anchor = caret;
        scroll = 0;
    }
    void SelectAll() {
        anchor = 0;
        caret = (int)text.size();
    }
    void Insert(const std::wstring& value);
    void Backspace(bool word);
    void DeleteForward(bool word);
    void Move(int delta, bool extend, bool word);
    void Home(bool extend, bool document) { (void)document; anchor = extend ? anchor : 0; caret = 0; }
    void End(bool extend) { if (!extend) anchor = (int)text.size(); caret = (int)text.size(); }
    std::wstring Selected() const;
    void DeleteSelection();
};

struct LayoutInfo {
    Rect titleBar{};
    Rect rail{};
    Rect sidebar{};
    Rect sidebarSplitter{};
    Rect main{};
    Rect tabBar{};
    Rect editor{};
    Rect panel{};
    Rect panelSplitter{};
    Rect statusBar{};
    Rect menuLabels[5]{};
    Rect terminalButton{};
    Rect minButton{};
    Rect maxButton{};
    Rect closeButton{};
};

struct TabViewInfo {
    int doc = -1;
    Rect rect{};
    Rect close{};
};

struct TreeRowView {
    TreeNode* node = nullptr;
    int depth = 0;
    Rect rect{};
};

struct FindMatch {
    int line = 0;
    int start = 0;
    int end = 0;
};

struct FindRects {
    Rect bar{};
    Rect input{};
    Rect prev{};
    Rect next{};
    Rect caseToggle{};
    Rect close{};
    Rect count{};
};

enum class LogKind {
    Normal,
    Command,
    Error,
    Warning,
    Success,
    Muted,
    Info
};

struct OutputEntry {
    std::wstring text;
    LogKind kind = LogKind::Normal;
    std::wstring navPath;
    int navLine = 0;
    int navCol = 0;
    IDWriteTextLayout* layout = nullptr;
    float height = 0;
    float width = 0;

    OutputEntry() = default;
    ~OutputEntry() {
        if (layout) layout->Release();
    }
    OutputEntry(const OutputEntry&) = delete;
    OutputEntry& operator=(const OutputEntry&) = delete;
};

struct QuickFileInfo {
    std::wstring path;
    std::wstring relative;
    std::wstring name;
    std::wstring dir;
};

struct ModalButtonInfo {
    std::wstring label;
    int result = 0;
    bool primary = false;
};

struct PromptState {
    std::wstring title;
    std::wstring label;
    std::wstring confirmLabel = L"Create";
    TextField field;
    std::wstring error;
    std::function<bool(const std::wstring&)> onAccept;
};

struct SettingsRects {
    Rect card{};
    Rect close{};
    Rect darkButton{};
    Rect lightButton{};
    Rect languageEnglish{};
    Rect languageChinese{};
    Rect fontMinus{};
    Rect fontPlus{};
    Rect sessionToggle{};
    Rect hiddenToggle{};
    Rect wordWrapToggle{};
    Rect terminalUtf8Toggle{};
};

class App {
public:
    App() = default;
    ~App();

    bool Create(HINSTANCE hInstance, int nCmdShow);
    int Run();
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    enum class Overlay {
        None,
        Find,
        Palette,
        QuickOpen,
        Goto,
        Prompt,
        Settings,
        About
    };

    enum class DragMode {
        None,
        Sidebar,
        Panel,
        EditorSelect,
        VScroll,
        HScroll,
        OutputScroll
    };

    static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // ---------------------------------------------------------- lifecycle
    void OnCreate();
    void OnDestroy();
    void LoadSettings();
    void SaveSettings();
    void ApplyTheme(bool dark);
    void UpdateDwmStyle();
    void UpdateWindowTitle();
    void RestoreStartup();
    void SetProject(const std::wstring& path, bool saveRecent = true);
    void AddRecentProject(const std::wstring& path);
    std::wstring SettingsFilePath() const;

    // ---------------------------------------------------------- layout/paint
    void UpdateLayout();
    void UpdateEditorMetrics();
    void Invalidate();
    void Paint(HDC hdc);
    void DrawTitleBar();
    void DrawRail();
    void DrawStatusBar();
    void DrawMenus();
    void DrawEditor();
    void DrawCompletionPopup();
    void DrawFindBar();
    void DrawTabs();
    void DrawSidebar();
    void DrawWelcome();
    void DrawOutputPanel();
    void DrawOverlay();
    void DrawModal();
    void DrawPalette();
    void DrawQuickOpen();
    void DrawGotoLine();
    void DrawPrompt();
    void DrawSettings();
    void DrawAbout();
    void DrawLogo(float x, float y, float size);

    // ---------------------------------------------------------- commands/menus
    void InitCommands();
    void InitMenus();
    void RunCommand(Cmd cmd);
    bool CanRunCommand(Cmd cmd) const;
    const CommandInfo* CommandFor(Cmd cmd) const;
    Rect MenuDropRect(int menuIndex) const;
    std::vector<Rect> MenuEntryRects(int menuIndex, Rect* dropRect) const;
    void ExecuteSelectedPaletteItem();
    void ExecuteSelectedQuickFile();
    void ExecuteGotoLine();
    void ExecutePrompt();
    void CloseOverlay();

    // ---------------------------------------------------------- documents/editor
    Document* ActiveDoc();
    const Document* ActiveDoc() const;
    int FindDocIndexByPath(const std::wstring& path) const;
    void ActivateDoc(int index);
    int NewUntitled();
    void SwitchTab(int delta);
    void EnsureTabVisible(int index);
    void ComputeTabs();
    void CloseDocument(Document* doc);
    void CloseDocumentNow(Document* doc);
    void RequestCloseAll();
    void RequestExit();
    bool OpenFile(const std::wstring& path, bool activate = true);
    bool SaveDocument(Document& doc, bool saveAs, bool showErrors = true);
    bool SaveAllDocuments();
    void PromptOpenFiles();
    void PromptOpenFolder();
    void NewFileInProject();
    void EnsureCaretVisible();
    void ClampScroll(Document& doc);

    void EditorTypeCharacter(wchar_t ch);
    void OpenCompletion();
    // Opens completions automatically after a word character is typed.
    void AutoOpenCompletion();
    // Returns true when the click was consumed by the completion popup.
    bool HandleCompletionMouseDown(float x, float y);
    void AcceptCompletion();
    bool HandleCompletionKey(WPARAM key);
    bool NavigateCompletionPage(int delta);
    bool EditorKeyDown(WPARAM key, bool ctrl, bool shift, bool alt);
    void EditorMouseDown(float x, float y, int clicks);
    void EditorMouseMove(float x, float y);
    void EditorMouseUp();
    bool EditorHitTest(float x, float y, Cursor& out, bool clampOutside = false);
    void EditorWheel(float x, float y, float delta, bool ctrl, bool shift);
    float LineTop(const Document& doc, int line) const;
    IDWriteTextLayout* GetLineLayout(Document& doc, int line);
    float CaretXFor(Document& doc, int line, int col);
    int DisplayColumnToDoc(const Line& line, int displayColumn);
    int DocColumnToDisplay(const Line& line, int col);
    int DocColumnToTextIndex(const Line& line, int col);
    void EditorEnsureCaretVisible();
    void EditorCopy(bool cut);
    void EditorPaste();
    void EditorToggleComment();
    void EditorIndent(bool outdent);
    void EditorEnter();
    void EditorTab(bool outdent);
    void EditorBackspace();
    void EditorDelete();
    void EditorMoveHorizontal(int direction, bool extend, bool word);
    void EditorMoveVertical(int direction, bool extend);
    void EditorHome(bool extend, bool document);
    void EditorEnd(bool extend, bool document);
    void UpdateVisibleScrollbars();

    // ---------------------------------------------------------- find
    void OpenFind();
    void CloseFind();
    void UpdateFindMatches(bool selectCurrent);
    void GotoFindMatch(int index);
    void FindNext(int direction);
    FindRects GetFindRects() const;
    bool HandleFindKey(WPARAM key, bool ctrl, bool shift);
    void FindQueryChanged();

    // ---------------------------------------------------------- project/tree
    void BuildTreeRows();
    void HandleTreeClick(float x, float y);
    void TreeScrollToPath(const std::wstring& path);
    void RefreshQuickIndex();
    void BuildQuickFiles();

    // ---------------------------------------------------------- output/process
    void AppendOutput(const std::wstring& line, LogKind kind = LogKind::Normal,
                      const std::wstring& navPath = {}, int navLine = 0, int navCol = 0);
    void ClearOutput();
    void OutputScrollToBottom();
    void PollProcess();
    void ToggleTerminal();
    void FocusTerminal();
    bool StartTerminal();
    void StopProcess();
    void SubmitTerminalLine();
    bool HandleTerminalKey(WPARAM key, bool ctrl, bool shift);
    std::wstring TerminalWorkingDirectory() const;
    void RebuildOutputLayouts(float width);
    void HandleOutputClick(float x, float y);

    // ---------------------------------------------------------- overlays
    void OpenPalette();
    void OpenQuickOpen();
    void OpenGotoLine();
    void OpenPrompt(const std::wstring& title, const std::wstring& label,
                    const std::wstring& initial, const std::wstring& confirmLabel,
                    std::function<bool(const std::wstring&)> onAccept);
    void OpenSettings();
    void CloseSettings();

    void RefreshPaletteFilter();
    void RefreshQuickFilter();
    int FuzzyScore(const std::wstring& text, const std::wstring& query, int* positions = nullptr) const;

    TextField* ActiveTextField();
    bool HandleTextFieldKey(TextField& field, WPARAM key, bool ctrl, bool shift);
    void DrawTextField(TextField& field, const Rect& rect, float fontSize, Color color,
                       const std::wstring& placeholder, bool focused);
    bool IsOverlayTextFieldActive() const;
    void OnOverlayKey(WPARAM key, bool ctrl, bool shift);
    void OnOverlayChar(wchar_t ch);
    void OnOverlayMouseDown(float x, float y);
    Rect PaletteCardRect() const;
    Rect QuickCardRect() const;

    bool HandleModalKey(WPARAM key);
    void ShowModal(const std::wstring& title, const std::wstring& message,
                   std::vector<ModalButtonInfo> buttons, int enterResult, int escapeResult,
                   std::function<void(int)> callback);
    void FinishModal(int result);
    Rect GetModalCardRect();
    SettingsRects GetSettingsRects() const;

    // ---------------------------------------------------------- input
    void OnMouseDown(float x, float y, int clicks);
    void OnMouseMove(float x, float y, bool leftDown);
    void OnMouseUp();
    void OnMouseWheel(float x, float y, float delta, bool ctrl, bool shift);
    void OnKeyDown(WPARAM key, bool ctrl, bool shift, bool alt);
    void OnChar(wchar_t ch);
    void InsertTextAtCaret(const std::wstring& input);
    void OnOverlayText(const std::wstring& text);
    void HandleImeComposition(LPARAM flags);
    void UpdateImeCompositionWindow();
    void OnTimer();
    void ResetBlink();
    void HandleDropFiles(HDROP drop);
    void HandleMenuMouseMove(float x, float y);
    bool HandleMenuClick(float x, float y);

    // ---------------------------------------------------------- helpers
    float S(float value) const { return value * dpi_; }
    Color StatusColor() const;
    void SetStatus(const std::wstring& text, int kind = 0);
    int TitleControlAt(float x, float y) const;
    int MenuLabelAt(float x, float y) const;
    bool FindBarHit(float x, float y, FindRects& rects) const;
    Rect GetAboutCardRect() const;
    Rect GetPromptCardRect() const;
    Rect GetGotoCardRect() const;
    void UpdateHoverForMouse(float x, float y);
    int VisibleLineCount() const;

    // ---------------------------------------------------------- state
    HINSTANCE hinst_ = nullptr;
    HWND hwnd_ = nullptr;
    HICON icon_ = nullptr;
    HICON iconSmall_ = nullptr;
    Renderer renderer_;
    ThemeColors theme_;
    Settings settings_;
    bool darkTheme_ = true;
    std::vector<std::wstring> startupPaths_;

    int width_ = 0;
    int height_ = 0;
    float dpi_ = 1.0f;
    bool windowActive_ = true;
    bool settingsLoaded_ = false;
    LayoutInfo L_;

    std::vector<std::unique_ptr<Document>> docs_;
    int active_ = -1;
    int untitledCounter_ = 1;

    std::wstring projectRoot_;
    FileTree tree_;
    std::wstring treeSelectedPath_;
    std::vector<TreeRowView> treeRows_;
    Rect treeViewRect_{};
    Rect explorerNewButton_{};
    Rect explorerRefreshButton_{};
    float treeScroll_ = 0.0f;
    float treeContentHeight_ = 0.0f;

    std::vector<TabViewInfo> tabViews_;
    Rect tabNewButton_{};
    float tabScroll_ = 0.0f;
    float tabContentWidth_ = 0.0f;

    std::vector<CommandInfo> commands_;
    std::vector<MenuDef> menus_;
    int openMenu_ = -1;

    float editorFontPx_ = 15.0f;
    float lineHeight_ = 23.0f;
    float monoCharWidth_ = 8.0f;
    float gutterWidth_ = 56.0f;
    Rect editorTextViewport_{};
    Rect vScrollTrack_{};
    Rect vScrollThumb_{};
    Rect hScrollTrack_{};
    Rect hScrollThumb_{};
    bool showVScroll_ = false;
    bool showHScroll_ = false;

    ULONGLONG blinkResetTick_ = 0;
    int lastBlinkPhase_ = -1;

    std::vector<std::unique_ptr<OutputEntry>> output_;
    float outputScroll_ = 0.0f;
    float outputContentHeight_ = 0.0f;
    float outputLayoutWidth_ = -1.0f;
    Rect outputBodyRect_{};
    Rect outputClearButton_{};
    Rect outputCloseButton_{};
    bool outputAutoScroll_ = true;
    std::vector<std::pair<Rect, int>> outputHitRects_;

    Overlay overlay_ = Overlay::None;
    TextField paletteField_;
    std::vector<int> paletteFiltered_;
    int paletteSel_ = 0;
    int paletteOffset_ = 0;
    Rect paletteCard_{};
    std::vector<Rect> paletteRowRects_;
    int paletteRowStart_ = 0;

    TextField quickField_;
    std::vector<QuickFileInfo> quickFiles_;
    bool quickFilesDirty_ = true;
    std::vector<int> quickFiltered_;
    int quickSel_ = 0;
    int quickOffset_ = 0;
    Rect quickCard_{};
    std::vector<Rect> quickRowRects_;
    int quickRowStart_ = 0;

    TextField gotoField_;
    Rect gotoCard_{};
    Rect gotoInput_{};
    Rect gotoAccept_{};
    Rect gotoCancel_{};

    TextField findField_;
    std::vector<FindMatch> findMatches_;
    int findCurrent_ = -1;
    bool findCaseSensitive_ = false;
    Cursor findOrigin_{};

    PromptState prompt_;
    Rect promptCard_{};
    Rect promptInput_{};
    Rect promptAccept_{};
    Rect promptCancel_{};

    bool modalOpen_ = false;
    std::wstring modalTitle_;
    std::wstring modalMessage_;
    std::vector<ModalButtonInfo> modalButtons_;
    std::vector<Rect> modalButtonRects_;
    Rect modalCard_{};
    std::function<void(int)> modalCallback_;
    int modalEnterResult_ = 0;
    int modalEscResult_ = 0;

    std::vector<std::pair<Rect, std::wstring>> welcomeRecentRects_;
    Rect welcomeOpenFolder_{};
    Rect welcomeOpenFile_{};
    Rect welcomeNewFile_{};
    Rect welcomePalette_{};

    float mouseX_ = 0.0f;
    float mouseY_ = 0.0f;
    DragMode drag_ = DragMode::None;
    float dragStart_ = 0.0f;
    float dragInitial_ = 0.0f;
    ULONGLONG lastClickTick_ = 0;

    // Integrated terminal (cmd.exe by default).
    std::unique_ptr<Process> process_;
    bool terminalRunning_ = false;
    bool terminalInputFocused_ = false;
    TextField terminalInput_;
    std::vector<std::wstring> terminalHistory_;
    int terminalHistoryIndex_ = -1;
    wchar_t pendingHighSurrogate_ = 0;
    bool imeComposing_ = false;
    std::wstring imeComposition_;
    int imeSuppressChars_ = 0; // skip duplicate WM_CHAR/WM_IME_CHAR after commit
    Rect terminalInputRect_{};

    bool completionVisible_ = false;
    std::vector<std::wstring> completionItems_;
    int completionSelected_ = 0;
    int completionStartCol_ = 0;
    Rect completionRect_{};
    int completionPage_ = 0;                // current page (5 candidates per page)
    bool completionClickDismissed_ = false; // hide until the next word is typed
    std::vector<Rect> completionRowRects_;  // hit rects for mouse selection
    Rect completionPrevRect_{};
    Rect completionNextRect_{};
    bool suppressNextBacktickChar_ = false; // key confirmed completion; ignore its WM_CHAR
    static constexpr int kCompletionPageSize = 5;

    std::wstring statusText_ = L"Ready";
    int statusKind_ = 0;  // 0 info, 1 success, 2 warning, 3 error
    Language lastObservedLanguage_ = Language::Plain;

    std::wstring findPlaceholder_ = L"Find";
};
