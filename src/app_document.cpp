#include "app.h"

#include <shobjidl.h>

namespace {

bool AnyDirty(const std::vector<std::unique_ptr<Document>>& docs) {
    for (const auto& doc : docs) {
        if (doc->dirty) return true;
    }
    return false;
}

std::wstring TrimTrailingSeparators(std::wstring path) {
    while (path.size() > 3 && (path.back() == L'\\' || path.back() == L'/')) path.pop_back();
    return path;
}

} // namespace

Document* App::ActiveDoc() {
    if (active_ < 0 || active_ >= (int)docs_.size()) return nullptr;
    return docs_[(size_t)active_].get();
}

const Document* App::ActiveDoc() const {
    if (active_ < 0 || active_ >= (int)docs_.size()) return nullptr;
    return docs_[(size_t)active_].get();
}

int App::FindDocIndexByPath(const std::wstring& path) const {
    if (path.empty()) return -1;
    for (size_t i = 0; i < docs_.size(); ++i) {
        if (!docs_[i]->path.empty() && SamePath(docs_[i]->path, path)) return (int)i;
    }
    return -1;
}

void App::ActivateDoc(int index) {
    if (index < 0 || index >= (int)docs_.size()) return;
    if (active_ == index) {
        EnsureTabVisible(index);
        return;
    }
    if (auto* previous = ActiveDoc()) previous->EndEditGroup();
    active_ = index;
    Document* doc = ActiveDoc();
    if (doc) {
        treeSelectedPath_ = doc->path;
        if (!doc->path.empty()) TreeScrollToPath(doc->path);
    }
    Overlay keep = overlay_;
    if (keep == Overlay::Find) {
        // Keep the find bar open, but recompute its matches for the new document.
        findOrigin_ = doc ? doc->caret : Cursor{};
        findCurrent_ = -1;
        UpdateFindMatches(false);
    }
    EnsureTabVisible(index);
    UpdateLayout();
    UpdateEditorMetrics();
    ResetBlink();
    Invalidate();
}

int App::NewUntitled() {
    auto doc = std::make_unique<Document>();
    std::wstring name = L"Untitled-" + std::to_wstring(untitledCounter_++);
    doc->NewEmpty(name);
    // Leave the language as Plain until the first line of code gives the
    // detector something to work with; the tab shows the default icon.
    docs_.push_back(std::move(doc));
    active_ = (int)docs_.size() - 1;
    settings_.panelVisible = settings_.panelVisible && !docs_.empty();
    UpdateLayout();
    UpdateEditorMetrics();
    ResetBlink();
    SetStatus(L"Created " + name, 0);
    Invalidate();
    return active_;
}

void App::SwitchTab(int delta) {
    if (docs_.empty()) return;
    int count = (int)docs_.size();
    int next = (active_ + delta) % count;
    if (next < 0) next += count;
    ActivateDoc(next);
}

void App::ComputeTabs() {
    tabViews_.clear();
    if (docs_.empty()) {
        tabNewButton_ = Rect{};
        tabContentWidth_ = 0;
        return;
    }
    float baseline = L_.tabBar.bottom - S(1.0f);
    float x = L_.tabBar.left + S(6.0f) - tabScroll_;
    TextStyle measureStyle;
    measureStyle.size = S(12.0f);
    measureStyle.weight = 500;
    for (size_t i = 0; i < docs_.size(); ++i) {
        Document& doc = *docs_[i];
        std::wstring title = doc.name.empty() ? L"Untitled" : doc.name;
        measureStyle.uiText = doc.name.empty();
        float textWidth = renderer_.Measure(title, measureStyle).width;
        float w = std::clamp(textWidth + S(58.0f), S(118.0f), S(222.0f));
        TabViewInfo info;
        info.doc = (int)i;
        info.rect = MakeRect(x, L_.tabBar.top, x + w, baseline);
        info.close = MakeRect(info.rect.right - S(26.0f), info.rect.top + S(11.0f),
                              info.rect.right - S(9.0f), info.rect.bottom - S(9.0f));
        tabViews_.push_back(info);
        x += w + S(2.0f);
    }
    tabContentWidth_ = x - (L_.tabBar.left + S(6.0f) - tabScroll_) + S(40.0f);
    tabNewButton_ = MakeRect(x + S(3.0f), L_.tabBar.top + S(8.0f),
                             x + S(31.0f), L_.tabBar.top + S(30.0f));
}

void App::EnsureTabVisible(int index) {
    if (index < 0 || index >= (int)docs_.size()) return;
    ComputeTabs();
    if (index >= (int)tabViews_.size()) return;
    const Rect& r = tabViews_[(size_t)index].rect;
    float visibleLeft = L_.tabBar.left + S(6.0f);
    float visibleRight = L_.tabBar.right - S(42.0f);
    if (r.left < visibleLeft) {
        tabScroll_ = std::max(0.0f, tabScroll_ - (visibleLeft - r.left));
    } else if (r.right > visibleRight) {
        tabScroll_ += r.right - visibleRight;
    }
    float maxScroll = std::max(0.0f, tabContentWidth_ - RectWidth(L_.tabBar));
    tabScroll_ = std::clamp(tabScroll_, 0.0f, maxScroll);
    ComputeTabs();
}

void App::CloseDocumentNow(Document* doc) {
    if (!doc) return;
    int index = -1;
    for (size_t i = 0; i < docs_.size(); ++i) {
        if (docs_[i].get() == doc) {
            index = (int)i;
            break;
        }
    }
    if (index < 0) return;
    if (overlay_ == Overlay::Find) CloseFind();
    docs_.erase(docs_.begin() + index);
    if (docs_.empty()) {
        active_ = -1;
    } else if (index <= active_) {
        active_ = std::max(0, std::min(active_ - (index == active_ ? 1 : 0), (int)docs_.size() - 1));
    } else {
        active_ = std::min(active_, (int)docs_.size() - 1);
    }
    if (active_ >= 0) {
        Document* next = ActiveDoc();
        treeSelectedPath_ = next ? next->path : L"";
    } else {
        treeSelectedPath_.clear();
    }
    UpdateLayout();
    UpdateEditorMetrics();
    ComputeTabs();
    ResetBlink();
    SetStatus(L"Editor closed", 0);
    Invalidate();
}

void App::CloseDocument(Document* doc) {
    if (!doc) return;
    if (!doc->dirty) {
        CloseDocumentNow(doc);
        return;
    }
    std::wstring title = doc->name.empty() ? L"Untitled" : doc->name;
    ShowModal(L"Save Changes?",
              L"\"" + title + L"\" has unsaved changes. Do you want to save them before closing?",
              {{L"Cancel", 0, false}, {L"Don't Save", 1, false}, {L"Save", 2, true}},
              2, 0,
              [this, doc](int result) {
                  if (result == 0) return;
                  if (result == 2) {
                      if (!SaveDocument(*doc, false)) return;
                  }
                  CloseDocumentNow(doc);
              });
}

void App::RequestCloseAll() {
    if (docs_.empty()) return;
    if (!AnyDirty(docs_)) {
        while (!docs_.empty()) CloseDocumentNow(docs_.back().get());
        return;
    }
    ShowModal(L"Unsaved Changes",
              L"Several open files contain unsaved changes. Save all of them before closing?",
              {{L"Cancel", 0, false}, {L"Don't Save", 1, false}, {L"Save All", 2, true}},
              2, 0,
              [this](int result) {
                  if (result == 0) return;
                  if (result == 2 && !SaveAllDocuments()) return;
                  while (!docs_.empty()) CloseDocumentNow(docs_.back().get());
              });
}

void App::RequestExit() {
    if (modalOpen_) return;
    if (!AnyDirty(docs_)) {
        if (process_) {
            process_->Stop();
            process_.reset();
        }
        DestroyWindow(hwnd_);
        return;
    }
    ShowModal(L"Unsaved Changes",
              L"You have unsaved files. Save them before exiting NeoIDEs?",
              {{L"Cancel", 0, false}, {L"Exit Without Saving", 1, false}, {L"Save and Exit", 2, true}},
              2, 0,
              [this](int result) {
                  if (result == 0) return;
                  if (result == 2 && !SaveAllDocuments()) return;
                  if (process_) {
                      process_->Stop();
                      process_.reset();
                  }
                  DestroyWindow(hwnd_);
              });
}

bool App::OpenFile(const std::wstring& path, bool activate) {
    if (path.empty()) return false;
    std::wstring normalized = NormalizePath(path);
    std::error_code ec;
    if (!std::filesystem::exists(normalized, ec) || std::filesystem::is_directory(normalized, ec)) {
        SetStatus(L"Could not open " + BaseName(path), 3);
        return false;
    }
    int existing = FindDocIndexByPath(normalized);
    if (existing >= 0) {
        if (activate) ActivateDoc(existing);
        return true;
    }
    auto doc = std::make_unique<Document>();
    std::wstring error;
    if (!doc->Load(normalized, error)) {
        ShowModal(L"Could Not Open File", error + L"\n\n" + normalized,
                  {{L"OK", 1, true}}, 1, 1, nullptr);
        SetStatus(L"Open failed: " + BaseName(normalized), 3);
        return false;
    }
    docs_.push_back(std::move(doc));
    int newIndex = (int)docs_.size() - 1;
    if (activate) {
        active_ = newIndex;
        treeSelectedPath_ = normalized;
        TreeScrollToPath(normalized);
        EnsureTabVisible(newIndex);
        UpdateLayout();
        UpdateEditorMetrics();
        ResetBlink();
    }
    SetStatus(L"Opened " + BaseName(normalized), 0);
    Invalidate();
    return true;
}

bool App::SaveDocument(Document& doc, bool saveAs, bool showErrors) {
    std::wstring target = doc.path;
    if (saveAs || target.empty()) {
        wchar_t fileName[MAX_PATH]{};
        std::wstring initial = doc.name.empty() ? L"untitled.cpp" : doc.name;
        if (initial.size() < MAX_PATH) wcscpy_s(fileName, initial.c_str());
        std::wstring initialDir = doc.path.empty() ? projectRoot_ : DirName(doc.path);
        if (initialDir.empty()) initialDir = L"";

        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwnd_;
        ofn.lpstrFilter = L"Source Files\0*.c;*.cc;*.cpp;*.cxx;*.h;*.hpp;*.hxx\0"
                          L"Text Files\0*.txt;*.md;*.json\0"
                          L"All Files\0*.*\0\0";
        ofn.lpstrFile = fileName;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrInitialDir = initialDir.empty() ? nullptr : initialDir.c_str();
        ofn.lpstrTitle = L"Save File";
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        ofn.lpstrDefExt = L"cpp";
        if (!GetSaveFileNameW(&ofn)) return false;
        target = fileName;
        if (SamePath(target, doc.path)) {
            // No-op: user selected the same file.
        }
    }
    std::wstring error;
    if (!doc.SaveAs(target, error)) {
        SetStatus(L"Save failed: " + error, 3);
        if (showErrors) {
            ShowModal(L"Could Not Save File", error + L"\n\n" + target,
                      {{L"OK", 1, true}}, 1, 1, nullptr);
        }
        return false;
    }
    if (!projectRoot_.empty() && IStartsWith(NormalizePath(target), NormalizePath(projectRoot_))) {
        tree_.Refresh();
    }
    SetStatus(L"Saved " + doc.name, 1);
    UpdateWindowTitle();
    Invalidate();
    return true;
}

bool App::SaveAllDocuments() {
    for (auto& doc : docs_) {
        if (!doc->dirty) continue;
        if (!SaveDocument(*doc, false)) return false;
    }
    SetStatus(L"All files saved", 1);
    return true;
}

void App::PromptOpenFiles() {
    IFileOpenDialog* dialog = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&dialog));
    if (FAILED(hr) || !dialog) {
        // Fallback to the classic dialog.
        std::vector<wchar_t> buffer(64 * 1024, L'\0');
        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwnd_;
        ofn.lpstrFilter = L"Source Files\0*.c;*.cc;*.cpp;*.cxx;*.h;*.hpp;*.hxx\0"
                          L"All Files\0*.*\0\0";
        ofn.lpstrFile = buffer.data();
        ofn.nMaxFile = (DWORD)buffer.size();
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_ALLOWMULTISELECT |
                    OFN_EXPLORER | OFN_NOCHANGEDIR;
        ofn.lpstrTitle = L"Open Files";
        if (GetOpenFileNameW(&ofn)) {
            const wchar_t* p = buffer.data();
            std::wstring dir = p;
            p += dir.size() + 1;
            if (*p == 0) {
                OpenFile(dir, true);
            } else {
                while (*p) {
                    std::wstring file = p;
                    p += file.size() + 1;
                    OpenFile(dir + L"\\" + file, false);
                }
                if (active_ < 0 && !docs_.empty()) ActivateDoc((int)docs_.size() - 1);
            }
        }
        return;
    }
    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_ALLOWMULTISELECT | FOS_FILEMUSTEXIST |
                       FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM);
    dialog->SetTitle(L"Open Files");
    hr = dialog->Show(hwnd_);
    if (SUCCEEDED(hr)) {
        IShellItemArray* items = nullptr;
        if (SUCCEEDED(dialog->GetResults(&items)) && items) {
            DWORD count = 0;
            items->GetCount(&count);
            for (DWORD i = 0; i < count; ++i) {
                IShellItem* item = nullptr;
                if (FAILED(items->GetItemAt(i, &item)) || !item) continue;
                PWSTR filePath = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &filePath)) && filePath) {
                    OpenFile(filePath, active_ < 0);
                    CoTaskMemFree(filePath);
                }
                item->Release();
            }
            if (active_ < 0 && !docs_.empty()) ActivateDoc((int)docs_.size() - 1);
            items->Release();
        }
    }
    dialog->Release();
}

void App::PromptOpenFolder() {
    IFileOpenDialog* dialog = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&dialog));
    if (FAILED(hr) || !dialog) {
        BROWSEINFOW browse{};
        browse.hwndOwner = hwnd_;
        browse.lpszTitle = L"Open Folder";
        browse.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
        LPITEMIDLIST idl = SHBrowseForFolderW(&browse);
        if (idl) {
            wchar_t path[MAX_PATH]{};
            if (SHGetPathFromIDListW(idl, path)) SetProject(path);
            CoTaskMemFree(idl);
        }
        return;
    }
    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM);
    dialog->SetTitle(L"Open Folder");
    hr = dialog->Show(hwnd_);
    if (SUCCEEDED(hr)) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item)) && item) {
            PWSTR folder = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &folder)) && folder) {
                SetProject(folder);
                CoTaskMemFree(folder);
            }
            item->Release();
        }
    }
    dialog->Release();
}

void App::NewFileInProject() {
    if (projectRoot_.empty()) {
        NewUntitled();
        return;
    }
    OpenPrompt(L"New File", L"Enter a file name, relative to the project root:",
               L"untitled.cpp", L"Create",
               [this](const std::wstring& value) {
                   std::wstring name = Trim(value);
                   if (name.empty()) {
                       prompt_.error = L"Please enter a file name.";
                       return false;
                   }
                   if (name.find(L'\\') != std::wstring::npos && name.find(L':') != std::wstring::npos) {
                       // Absolute paths are accepted.
                   }
                   std::filesystem::path target(name);
                   if (target.is_relative()) target = std::filesystem::path(projectRoot_) / target;
                   std::wstring normalized = NormalizePath(target.wstring());
                   std::error_code ec;
                   if (std::filesystem::exists(normalized, ec)) {
                       prompt_.error = L"That file already exists.";
                       return false;
                   }
                   if (!target.parent_path().empty()) {
                       std::filesystem::create_directories(target.parent_path(), ec);
                   }
                   std::ofstream out(target, std::ios::binary | std::ios::trunc);
                   if (!out) {
                       prompt_.error = L"Could not create the file.";
                       return false;
                   }
                   out.close();
                   if (!projectRoot_.empty()) {
                       tree_.Refresh();
                   }
                   OpenFile(normalized);
                   treeSelectedPath_ = normalized;
                   TreeScrollToPath(normalized);
                   return true;
               });
}

void App::EnsureCaretVisible() {
    if (auto* doc = ActiveDoc()) {
        EnsureTabVisible(active_);
        EditorEnsureCaretVisible();
        (void)doc;
    }
}

void App::ClampScroll(Document& doc) {
    float viewW = RectWidth(editorTextViewport_);
    float viewH = RectHeight(editorTextViewport_);
    if (viewW <= 0) viewW = std::max(0.0f, RectWidth(L_.editor) - gutterWidth_ - S(14.0f));
    if (viewH <= 0) viewH = RectHeight(L_.editor);
    float contentH = doc.LineCount() * lineHeight_;
    float maxY = std::max(0.0f, contentH - viewH + S(16.0f));
    float contentW = doc.maxColumns * monoCharWidth_ + S(48.0f);
    float maxX = std::max(0.0f, contentW - viewW + S(24.0f));
    doc.scrollY = std::clamp(doc.scrollY, 0.0f, maxY);
    if (!settings_.wordWrap) {
        doc.scrollX = std::clamp(doc.scrollX, 0.0f, maxX);
    } else {
        doc.scrollX = 0.0f;
    }
}
