#include "document.h"

#include <cerrno>

namespace {

std::wstring ErrorText(DWORD code) {
    wchar_t* buffer = nullptr;
    DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                  FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD len = FormatMessageW(flags, nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                               (LPWSTR)&buffer, 0, nullptr);
    std::wstring message;
    if (len && buffer) {
        message.assign(buffer, len);
        while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n' ||
                                    message.back() == L' ')) {
            message.pop_back();
        }
    }
    if (buffer) LocalFree(buffer);
    if (message.empty()) message = L"Windows error " + std::to_wstring(code);
    return message;
}

int VisualColumns(const std::wstring& text) {
    int col = 0;
    for (wchar_t c : text) {
        if (c == L'\t') col += 4 - (col % 4);
        else ++col;
    }
    return col;
}

} // namespace

Line::Line(Line&& o) noexcept
    : text(std::move(o.text)),
      tokens(std::move(o.tokens)),
      stateIn(o.stateIn),
      stateOut(o.stateOut),
      layout(o.layout),
      display(std::move(o.display)),
      displayMap(std::move(o.displayMap)),
      layoutFontSize(o.layoutFontSize),
      layoutDirty(o.layoutDirty) {
    o.layout = nullptr;
    o.layoutDirty = true;
}

Line& Line::operator=(Line&& o) noexcept {
    if (this == &o) return *this;
    ReleaseLayout();
    text = std::move(o.text);
    tokens = std::move(o.tokens);
    stateIn = o.stateIn;
    stateOut = o.stateOut;
    layout = o.layout;
    o.layout = nullptr;
    display = std::move(o.display);
    displayMap = std::move(o.displayMap);
    layoutFontSize = o.layoutFontSize;
    layoutDirty = o.layoutDirty;
    o.layoutDirty = true;
    return *this;
}

Document::Document() {
    lines.emplace_back();
    savedHash_ = std::hash<std::wstring>{}(L"");
}

void Document::NewEmpty(const std::wstring& displayName) {
    for (auto& line : lines) line.ReleaseLayout();
    lines.clear();
    lines.emplace_back();
    path.clear();
    name = displayName;
    untitled = true;
    caret = {};
    anchor = {};
    desiredCol = -1;
    scrollX = 0;
    scrollY = 0;
    dirty = false;
    language = Language::Plain;
    encoding = Encoding::Utf8;
    eol = L"\n";
    lexedThrough = 0;
    maxColumns = 0;
    undoStack_.clear();
    redoStack_.clear();
    undoChars_ = 0;
    groupActive_ = false;
    savedHash_ = std::hash<std::wstring>{}(L"");
    hasSavedHash_ = true;
}

const std::wstring& Document::LineText(int line) const {
    static const std::wstring empty;
    if (line < 0 || line >= (int)lines.size()) return empty;
    return lines[(size_t)line].text;
}

int Document::ClampColumn(int line, int col) const {
    if (line < 0) return 0;
    if (line >= (int)lines.size()) return 0;
    return std::clamp(col, 0, (int)lines[(size_t)line].text.size());
}

Cursor Document::ClampCursor(Cursor c) const {
    if (lines.empty()) return {};
    c.line = std::clamp(c.line, 0, (int)lines.size() - 1);
    c.col = ClampColumn(c.line, c.col);
    return c;
}

std::pair<Cursor, Cursor> Document::SelectionRange() const {
    if (anchor < caret) return {anchor, caret};
    return {caret, anchor};
}

void Document::SetCaret(Cursor c, bool extend) {
    c = ClampCursor(c);
    caret = c;
    if (!extend) anchor = c;
    desiredCol = -1;
}

void Document::SelectAll() {
    if (lines.empty()) return;
    anchor = Cursor{0, 0};
    caret = Cursor{(int)lines.size() - 1, (int)lines.back().text.size()};
    desiredCol = -1;
}

std::wstring Document::SelectedText() const {
    if (!HasSelection()) return {};
    auto [a, b] = SelectionRange();
    if (a.line == b.line) {
        return lines[(size_t)a.line].text.substr((size_t)a.col, (size_t)(b.col - a.col));
    }
    std::wstring out;
    out.append(lines[(size_t)a.line].text.substr((size_t)a.col));
    for (int i = a.line + 1; i < b.line; ++i) {
        out.push_back(L'\n');
        out += lines[(size_t)i].text;
    }
    out.push_back(L'\n');
    out.append(lines[(size_t)b.line].text.substr(0, (size_t)b.col));
    return out;
}

void Document::SetSelection(Cursor a, Cursor c) {
    anchor = ClampCursor(a);
    caret = ClampCursor(c);
    desiredCol = -1;
}

void Document::BeginEdit(bool typing) {
    ULONGLONG now = GetTickCount64();
    if (groupActive_ && typing && groupTyping_ && now - lastEditTick_ < 700) {
        lastEditTick_ = now;
        return;
    }
    PushSnapshot();
    redoStack_.clear();
    groupActive_ = true;
    groupTyping_ = typing;
    lastEditTick_ = now;
}

void Document::EndEditGroup() {
    groupActive_ = false;
}

void Document::PushSnapshot() {
    Snapshot snapshot = Capture();
    undoChars_ += snapshot.text.size() + 1;
    undoStack_.push_back(std::move(snapshot));
    while (undoStack_.size() > 200 ||
           (undoChars_ > 48ull * 1024ull * 1024ull && undoStack_.size() > 1)) {
        undoChars_ -= undoStack_.front().text.size() + 1;
        undoStack_.erase(undoStack_.begin());
    }
}

Document::Snapshot Document::Capture() const {
    Snapshot s;
    s.text = Content();
    s.caret = caret;
    s.anchor = anchor;
    return s;
}

void Document::Undo() {
    if (undoStack_.empty()) return;
    groupActive_ = false;
    Snapshot current = Capture();
    redoStack_.push_back(std::move(current));
    if (redoStack_.size() > 200) redoStack_.erase(redoStack_.begin());
    Snapshot snapshot = std::move(undoStack_.back());
    undoChars_ -= std::min(undoChars_, snapshot.text.size() + 1);
    undoStack_.pop_back();
    RestoreSnapshot(snapshot);
}

void Document::Redo() {
    if (redoStack_.empty()) return;
    groupActive_ = false;
    Snapshot current = Capture();
    undoChars_ += current.text.size() + 1;
    undoStack_.push_back(std::move(current));
    Snapshot snapshot = std::move(redoStack_.back());
    redoStack_.pop_back();
    RestoreSnapshot(snapshot);
}

void Document::RestoreSnapshot(const Snapshot& snapshot) {
    SetLinesFromText(snapshot.text);
    caret = ClampCursor(snapshot.caret);
    anchor = ClampCursor(snapshot.anchor);
    desiredCol = -1;
    dirty = RecomputeDirtyState();
    RecomputeMaxColumns();
}

void Document::TouchLine(int line) {
    if (line < 0 || line >= (int)lines.size()) return;
    lines[(size_t)line].ReleaseLayout();
    lexedThrough = std::min(lexedThrough, line);
    dirty = true;
    UpdateMaxColumnsForLine(line);
}

void Document::InsertRaw(Cursor pos, const std::wstring& text) {
    if (text.empty()) return;
    pos = ClampCursor(pos);
    std::wstring clean;
    clean.reserve(text.size());
    for (wchar_t c : text) {
        if (c == L'\r') continue;
        clean.push_back(c);
    }
    if (clean.empty()) return;

    std::vector<std::wstring> parts;
    size_t start = 0;
    while (true) {
        size_t nl = clean.find(L'\n', start);
        if (nl == std::wstring::npos) {
            parts.push_back(clean.substr(start));
            break;
        }
        parts.push_back(clean.substr(start, nl - start));
        start = nl + 1;
    }

    Line& line = lines[(size_t)pos.line];
    if (parts.size() == 1) {
        line.text.insert((size_t)pos.col, parts[0]);
        caret = Cursor{pos.line, pos.col + (int)parts[0].size()};
        anchor = caret;
        desiredCol = -1;
        TouchLine(pos.line);
        return;
    }

    std::wstring prefix = line.text.substr(0, (size_t)pos.col);
    std::wstring suffix = line.text.substr((size_t)pos.col);
    line.ReleaseLayout();
    line.text = prefix + parts[0];

    std::vector<Line> inserted;
    inserted.reserve(parts.size() - 1);
    for (size_t i = 1; i < parts.size(); ++i) {
        Line newLine;
        newLine.text = parts[i];
        if (i + 1 == parts.size()) newLine.text += suffix;
        inserted.push_back(std::move(newLine));
    }
    lines.insert(lines.begin() + pos.line + 1,
                 std::make_move_iterator(inserted.begin()),
                 std::make_move_iterator(inserted.end()));
    caret = Cursor{pos.line + (int)parts.size() - 1, (int)parts.back().size()};
    anchor = caret;
    desiredCol = -1;
    lexedThrough = std::min(lexedThrough, pos.line);
    dirty = true;
    for (int i = pos.line; i <= caret.line; ++i) UpdateMaxColumnsForLine(i);
}

void Document::DeleteRaw(Cursor a, Cursor b) {
    a = ClampCursor(a);
    b = ClampCursor(b);
    if (b < a) std::swap(a, b);
    if (a == b) return;
    if (a.line == b.line) {
        lines[(size_t)a.line].text.erase((size_t)a.col, (size_t)(b.col - a.col));
        caret = a;
        anchor = a;
        desiredCol = -1;
        TouchLine(a.line);
        return;
    }
    Line& first = lines[(size_t)a.line];
    std::wstring suffix = lines[(size_t)b.line].text.substr((size_t)b.col);
    first.ReleaseLayout();
    first.text = first.text.substr(0, (size_t)a.col) + suffix;
    lines.erase(lines.begin() + a.line + 1, lines.begin() + b.line + 1);
    if (lines.empty()) lines.emplace_back();
    caret = a;
    anchor = a;
    desiredCol = -1;
    lexedThrough = std::min(lexedThrough, a.line);
    dirty = true;
    UpdateMaxColumnsForLine(a.line);
}

void Document::DeleteSelectionRaw() {
    if (!HasSelection()) return;
    auto [a, b] = SelectionRange();
    DeleteRaw(a, b);
}

void Document::SetLinesFromText(const std::wstring& text) {
    for (auto& line : lines) line.ReleaseLayout();
    lines.clear();
    SplitToLines(text, lines);
    lexedThrough = 0;
    for (auto& line : lines) line.layoutDirty = true;
}

void Document::SplitToLines(const std::wstring& text, std::vector<Line>& out) {
    size_t start = 0;
    while (true) {
        size_t nl = text.find(L'\n', start);
        if (nl == std::wstring::npos) {
            Line line;
            line.text = text.substr(start);
            out.push_back(std::move(line));
            break;
        }
        Line line;
        line.text = text.substr(start, nl - start);
        out.push_back(std::move(line));
        start = nl + 1;
    }
}

std::wstring Document::Content() const {
    std::wstring text;
    size_t total = 0;
    for (const auto& line : lines) total += line.text.size() + 1;
    text.reserve(total);
    for (size_t i = 0; i < lines.size(); ++i) {
        text += lines[i].text;
        if (i + 1 < lines.size()) text.push_back(L'\n');
    }
    return text;
}

size_t Document::ContentHash() const {
    return std::hash<std::wstring>{}(Content());
}

size_t Document::RecomputeDirtyState() {
    if (!hasSavedHash_) return true;
    return ContentHash() != savedHash_;
}

void Document::MarkDirty() {
    dirty = true;
}

void Document::EnsureTokenization(int upToLine) {
    if (lines.empty()) return;
    upToLine = std::clamp(upToLine, 0, (int)lines.size() - 1);
    if (lexedThrough > (int)lines.size()) lexedThrough = (int)lines.size();
    if (lexedThrough > upToLine) return;
    int start = std::max(0, lexedThrough);
    LexState state = start > 0 ? lines[(size_t)start - 1].stateOut : LexState::Normal;
    for (int i = start; i <= upToLine; ++i) {
        Line& line = lines[(size_t)i];
        line.stateIn = state;
        HighlightLine(line.text, language, state, line.tokens, state);
        line.stateOut = state;
        if (line.layout) line.ReleaseLayout();
        line.layoutDirty = true;
    }
    lexedThrough = upToLine + 1;
}

void Document::InvalidateLayouts() {
    for (auto& line : lines) line.ReleaseLayout();
}

void Document::SetLanguage(Language lang) {
    if (language == lang) return;
    language = lang;
    lexedThrough = 0;
    for (auto& line : lines) {
        line.tokens.clear();
        if (line.layout) line.ReleaseLayout();
        line.layoutDirty = true;
    }
}

void Document::UpdateMaxColumnsForLine(int line) {
    if (line < 0 || line >= (int)lines.size()) return;
    int cols = VisualColumns(lines[(size_t)line].text);
    if (cols > maxColumns) maxColumns = cols;
    else if (line == (int)lines.size() - 1 && maxColumns > 0) {
        RecomputeMaxColumns();
    }
}

void Document::RecomputeMaxColumns() {
    maxColumns = 0;
    for (const auto& line : lines) {
        maxColumns = std::max(maxColumns, VisualColumns(line.text));
    }
}

bool Document::Load(const std::wstring& filePath, std::wstring& error) {
    std::error_code fsError;
    auto fsPath = std::filesystem::path(filePath);
    if (!std::filesystem::exists(fsPath, fsError) || !std::filesystem::is_regular_file(fsPath, fsError)) {
        error = L"File does not exist or is not a regular file.";
        return false;
    }
    std::string bytes;
    if (!ReadBinaryFile(filePath, bytes)) {
        error = L"Could not read the file.";
        return false;
    }
    if (bytes.size() > 100ull * 1024ull * 1024ull) {
        error = L"The file is larger than 100 MB.";
        return false;
    }

    std::wstring text;
    Encoding enc = Encoding::Utf8;
    bool utf16 = false;
    if (bytes.size() >= 2 && (unsigned char)bytes[0] == 0xFF && (unsigned char)bytes[1] == 0xFE) {
        utf16 = true;
        enc = Encoding::Utf16LE;
        if (bytes.size() % 2 != 0) {
            error = L"Invalid UTF-16 file.";
            return false;
        }
        size_t count = (bytes.size() - 2) / 2;
        text.resize(count);
        const unsigned char* data = (const unsigned char*)bytes.data() + 2;
        for (size_t i = 0; i < count; ++i) {
            text[i] = (wchar_t)(data[i * 2] | ((wchar_t)data[(size_t)i * 2 + 1] << 8));
        }
    } else if (bytes.size() >= 2 && (unsigned char)bytes[0] == 0xFE && (unsigned char)bytes[1] == 0xFF) {
        utf16 = true;
        enc = Encoding::Utf16LE;
        if (bytes.size() % 2 != 0) {
            error = L"Invalid UTF-16 file.";
            return false;
        }
        size_t count = (bytes.size() - 2) / 2;
        text.resize(count);
        const unsigned char* data = (const unsigned char*)bytes.data() + 2;
        for (size_t i = 0; i < count; ++i) {
            text[i] = (wchar_t)(data[(size_t)i * 2 + 1] | ((wchar_t)data[(size_t)i * 2] << 8));
        }
    } else {
        size_t offset = 0;
        if (bytes.size() >= 3 && (unsigned char)bytes[0] == 0xEF &&
            (unsigned char)bytes[1] == 0xBB && (unsigned char)bytes[2] == 0xBF) {
            enc = Encoding::Utf8Bom;
            offset = 3;
        }
        for (size_t i = offset; i < bytes.size(); ++i) {
            if (bytes[i] == '\0') {
                error = L"This looks like a binary file.";
                return false;
            }
        }
        text = Utf8ToWide(std::string_view(bytes.data() + offset, bytes.size() - offset));
    }

    if (text.find(L'\0') != std::wstring::npos) {
        error = L"This looks like a binary file.";
        return false;
    }
    if (!text.empty() && text[0] == 0xFEFF) text.erase(text.begin());

    size_t crlf = 0;
    size_t lf = 0;
    size_t cr = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == L'\r') {
            if (i + 1 < text.size() && text[i + 1] == L'\n') {
                ++crlf;
                ++i;
            } else {
                ++cr;
            }
        } else if (text[i] == L'\n') {
            ++lf;
        }
    }
    std::wstring eolStyle = (crlf > lf + cr) ? L"\r\n" : L"\n";

    std::wstring normalized;
    normalized.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        wchar_t c = text[i];
        if (c == L'\r') {
            if (i + 1 < text.size() && text[i + 1] == L'\n') ++i;
            normalized.push_back(L'\n');
        } else {
            normalized.push_back(c);
        }
    }

    setlocale(LC_ALL, ""); // harmless; keeps iswspace sane for the current locale
    SetLinesFromText(normalized);
    path = NormalizePath(filePath);
    name = BaseName(path);
    untitled = false;
    language = LanguageFromPath(path);
    encoding = enc;
    eol = eolStyle;
    (void)utf16;
    caret = {};
    anchor = {};
    desiredCol = -1;
    scrollX = 0;
    scrollY = 0;
    dirty = false;
    undoStack_.clear();
    redoStack_.clear();
    undoChars_ = 0;
    groupActive_ = false;
    savedHash_ = std::hash<std::wstring>{}(normalized);
    hasSavedHash_ = true;
    RecomputeMaxColumns();
    return true;
}

bool Document::Save(std::wstring& error) {
    if (path.empty()) {
        error = L"Give the file a name first.";
        return false;
    }
    return SaveAs(path, error);
}

bool Document::SaveAs(const std::wstring& filePath, std::wstring& error) {
    if (filePath.empty()) {
        error = L"Empty file name.";
        return false;
    }
    std::filesystem::path target(filePath);
    std::error_code ec;
    if (!target.parent_path().empty()) {
        std::filesystem::create_directories(target.parent_path(), ec);
    }

    std::wstring content = Content();
    std::wstring serialized;
    serialized.reserve(content.size() + content.size() / 16 + 3);
    for (wchar_t c : content) {
        if (c == L'\n') serialized += eol;
        else serialized.push_back(c);
    }

    std::string bytes;
    if (encoding == Encoding::Utf16LE) {
        bytes.reserve(serialized.size() * 2 + 2);
        bytes.push_back((char)0xFF);
        bytes.push_back((char)0xFE);
        for (wchar_t c : serialized) {
            bytes.push_back((char)((uint16_t)c & 0xFF));
            bytes.push_back((char)(((uint16_t)c >> 8) & 0xFF));
        }
    } else {
        if (encoding == Encoding::Utf8Bom) {
            bytes.push_back((char)0xEF);
            bytes.push_back((char)0xBB);
            bytes.push_back((char)0xBF);
        }
        bytes += WideToUtf8(serialized);
    }

    std::wstring temp = target.wstring() + L".neotmp" + std::to_wstring(GetCurrentProcessId());
    DeleteFileW(temp.c_str());
    {
        std::ofstream out(std::filesystem::path(temp), std::ios::binary | std::ios::trunc);
        if (!out) {
            error = L"Could not create a temporary file near the target.";
            return false;
        }
        out.write(bytes.data(), (std::streamsize)bytes.size());
        out.flush();
        if (!out.good()) {
            error = L"Could not write the file.";
            out.close();
            DeleteFileW(temp.c_str());
            return false;
        }
    }
    if (!MoveFileExW(temp.c_str(), target.wstring().c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DWORD code = GetLastError();
        error = L"Could not replace the file: " + ErrorText(code);
        DeleteFileW(temp.c_str());
        return false;
    }

    Language oldLanguage = language;
    std::wstring oldPath = path;
    path = NormalizePath(filePath);
    name = BaseName(path);
    untitled = false;
    language = LanguageFromPath(path);
    if (language != oldLanguage) {
        lexedThrough = 0;
        for (auto& line : lines) {
            line.tokens.clear();
            if (line.layout) line.ReleaseLayout();
            line.layoutDirty = true;
        }
    }
    if (path != oldPath) {
        hasSavedHash_ = true;
    }
    savedHash_ = std::hash<std::wstring>{}(Content());
    hasSavedHash_ = true;
    dirty = false;
    EndEditGroup();
    return true;
}
