#pragma once

#include "common.h"
#include "syntax.h"

struct Cursor {
    int line = 0;
    int col = 0;

    bool operator==(const Cursor& o) const { return line == o.line && col == o.col; }
    bool operator!=(const Cursor& o) const { return !(*this == o); }
    bool operator<(const Cursor& o) const {
        return line < o.line || (line == o.line && col < o.col);
    }
};

struct Line {
    std::wstring text;
    std::vector<Token> tokens;
    LexState stateIn = LexState::Normal;
    LexState stateOut = LexState::Normal;

    IDWriteTextLayout* layout = nullptr;
    std::wstring display;
    std::vector<int> displayMap;
    float layoutFontSize = 0.0f;
    bool layoutDirty = true;

    Line() = default;
    ~Line() { ReleaseLayout(); }
    Line(const Line&) = delete;
    Line& operator=(const Line&) = delete;
    Line(Line&& o) noexcept;
    Line& operator=(Line&& o) noexcept;

    void ReleaseLayout() {
        if (layout) {
            layout->Release();
            layout = nullptr;
        }
        display.clear();
        displayMap.clear();
        layoutFontSize = 0;
        layoutDirty = true;
    }
};

enum class Encoding {
    Utf8,
    Utf8Bom,
    Utf16LE
};

class Document {
public:
    std::vector<Line> lines;
    std::wstring path;
    std::wstring name;
    bool untitled = true;

    Cursor caret{};
    Cursor anchor{};
    int desiredCol = -1;

    float scrollX = 0.0f;
    float scrollY = 0.0f;

    bool dirty = false;
    Language language = Language::Plain;
    std::wstring languageId;
    // While true, the opening code decides the language until the document is
    // saved under a recognized extension. Untitled files start in this mode.
    bool detectLanguageFromContent = true;
    Encoding encoding = Encoding::Utf8;
    std::wstring eol = L"\n";
    int lexedThrough = 0;
    int maxColumns = 0;

    Document();

    void NewEmpty(const std::wstring& displayName);
    bool Load(const std::wstring& filePath, std::wstring& error);
    bool Save(std::wstring& error);
    bool SaveAs(const std::wstring& filePath, std::wstring& error);

    int LineCount() const { return (int)lines.size(); }
    const std::wstring& LineText(int line) const;
    int ClampColumn(int line, int col) const;
    Cursor ClampCursor(Cursor c) const;

    bool HasSelection() const { return caret != anchor; }
    std::pair<Cursor, Cursor> SelectionRange() const;
    std::wstring SelectedText() const;
    void SetCaret(Cursor c, bool extend);
    void EndEditGroup();
    void SelectAll();

    void BeginEdit(bool typing = true);
    void InsertRaw(Cursor pos, const std::wstring& text);
    void DeleteRaw(Cursor a, Cursor b);
    void DeleteSelectionRaw();
    void SetSelection(Cursor a, Cursor c);

    bool CanUndo() const { return !undoStack_.empty(); }
    bool CanRedo() const { return !redoStack_.empty(); }
    void Undo();
    void Redo();

    void EnsureTokenization(int upToLine);
    void InvalidateLayouts();
    void SetLanguage(Language lang, const std::wstring& id = std::wstring());
    // Re-predicts the language from the first few kilobytes. Cheap enough to
    // run after every edit; no work is done when detection is locked by a
    // recognized file extension.
    void UpdateDetectedLanguage();
    void RecomputeMaxColumns();
    std::wstring Content() const;
    size_t ContentHash() const;
    bool HasDiskVersion() const { return !untitled; }

    void MarkDirty();

    void SetContentFromText(const std::wstring& text, bool resetUndo);

private:
    struct Snapshot {
        std::wstring text;
        Cursor caret{};
        Cursor anchor{};
    };

    std::vector<Snapshot> undoStack_;
    std::vector<Snapshot> redoStack_;
    size_t undoChars_ = 0;
    bool groupActive_ = false;
    bool groupTyping_ = false;
    ULONGLONG lastEditTick_ = 0;
    size_t savedHash_ = 0;
    bool hasSavedHash_ = true;

    void PushSnapshot();
    void RestoreSnapshot(const Snapshot& snapshot);
    void SetLinesFromText(const std::wstring& text);
    void TouchLine(int line);
    void UpdateMaxColumnsForLine(int line);
    size_t RecomputeDirtyState();
    Snapshot Capture() const;
    void SplitToLines(const std::wstring& text, std::vector<Line>& out);
};
