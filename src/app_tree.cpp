#include "app.h"

namespace {

constexpr float kSidebarHeader = 44.0f;
constexpr float kTreeRowHeight = 26.0f;
constexpr size_t kMaxQuickFiles = 6000;
constexpr int kMaxQuickDepth = 12;

bool QuickSkipDirectory(const std::wstring& name) {
    static const std::unordered_set<std::wstring> skip = {
        L".git", L".hg", L".svn", L".neoide", L".vs", L".idea", L"node_modules",
        L"__pycache__", L"build", L"dist", L"out", L"$RECYCLE.BIN",
        L"System Volume Information", L".cache"
    };
    return skip.count(name) > 0;
}

void CollectQuickFiles(const std::filesystem::path& dir, const std::filesystem::path& root,
                       int depth, std::vector<QuickFileInfo>& files) {
    if (depth > kMaxQuickDepth || files.size() >= kMaxQuickFiles) return;
    std::error_code ec;
    std::filesystem::directory_iterator it(dir,
        std::filesystem::directory_options::skip_permission_denied, ec);
    if (ec) return;
    for (const auto& entry : it) {
        if (files.size() >= kMaxQuickFiles) break;
        std::error_code entryError;
        std::wstring name = entry.path().filename().wstring();
        if (name.empty()) continue;
        if (!name.empty() && name[0] == L'.') continue;
        bool isDir = entry.is_directory(entryError);
        if (entryError) continue;
        if (isDir) {
            if (QuickSkipDirectory(name)) continue;
            CollectQuickFiles(entry.path(), root, depth + 1, files);
        } else {
            std::error_code relError;
            std::filesystem::path rel = std::filesystem::relative(entry.path(), root, relError);
            QuickFileInfo info;
            info.path = entry.path().wstring();
            info.relative = relError ? entry.path().wstring() : rel.wstring();
            info.name = name;
            info.dir = relError ? L"" : rel.parent_path().wstring();
            files.push_back(std::move(info));
        }
    }
}

std::wstring TreePathKey(const std::wstring& path) {
    return ToLower(path);
}

} // namespace

void App::BuildTreeRows() {
    if (settings_.sidebarVisible) {
        treeViewRect_ = MakeRect(L_.sidebar.left + S(4.0f),
                                 L_.sidebar.top + S(kSidebarHeader),
                                 L_.sidebar.right - S(4.0f),
                                 L_.sidebar.bottom);
        float top = L_.sidebar.top + S(7.0f);
        explorerNewButton_ = MakeRect(L_.sidebar.right - S(58.0f), top,
                                      L_.sidebar.right - S(32.0f), top + S(26.0f));
        explorerRefreshButton_ = MakeRect(L_.sidebar.right - S(30.0f), top,
                                          L_.sidebar.right - S(6.0f), top + S(26.0f));
    } else {
        treeViewRect_ = Rect{};
        explorerNewButton_ = Rect{};
        explorerRefreshButton_ = Rect{};
    }

    treeRows_.clear();
    if (!projectRoot_.empty() && tree_.rootNode) {
        std::vector<TreeRow> rows;
        tree_.Flatten(rows);
        float rowH = S(kTreeRowHeight);
        treeRows_.reserve(rows.size());
        for (size_t i = 0; i < rows.size(); ++i) {
            TreeRowView view;
            view.node = rows[i].node;
            view.depth = rows[i].depth;
            float y = treeViewRect_.top - treeScroll_ + (float)i * rowH;
            view.rect = MakeRect(treeViewRect_.left, y, treeViewRect_.right, y + rowH);
            treeRows_.push_back(view);
        }
        treeContentHeight_ = rows.size() * rowH;
    } else {
        treeContentHeight_ = 0;
    }
}

void App::HandleTreeClick(float x, float y) {
    if (!Contains(treeViewRect_, x, y)) return;
    BuildTreeRows();
    for (const auto& row : treeRows_) {
        if (!row.node || !Contains(row.rect, x, y)) continue;
        TreeNode* node = row.node;
        treeSelectedPath_ = node->path;
        if (node->isDir) {
            tree_.Toggle(*node);
            BuildTreeRows();
            Invalidate();
        } else {
            OpenFile(node->path);
        }
        return;
    }
}

void App::TreeScrollToPath(const std::wstring& path) {
    if (projectRoot_.empty() || !tree_.rootNode || path.empty()) return;
    std::wstring normalized = NormalizePath(path);
    std::wstring rootNormalized = NormalizePath(projectRoot_);
    std::wstring relative = normalized;
    if (IStartsWith(normalized, rootNormalized)) {
        relative = normalized.substr(rootNormalized.size());
    }
    std::vector<std::wstring> parts;
    size_t start = 0;
    while (start < relative.size()) {
        while (start < relative.size() && (relative[start] == L'\\' || relative[start] == L'/')) ++start;
        size_t end = start;
        while (end < relative.size() && relative[end] != L'\\' && relative[end] != L'/') ++end;
        if (end > start) parts.push_back(relative.substr(start, end - start));
        start = end;
    }
    TreeNode* node = tree_.rootNode.get();
    for (const auto& part : parts) {
        if (!node || !node->isDir) {
            node = nullptr;
            break;
        }
        tree_.LoadChildren(*node);
        node->expanded = true;
        TreeNode* child = nullptr;
        for (auto& candidate : node->children) {
            if (IEquals(candidate->name, part)) {
                child = candidate.get();
                break;
            }
        }
        if (!child) {
            node = nullptr;
            break;
        }
        node = child;
    }
    if (!node) return;
    node->expanded = node->isDir;
    treeSelectedPath_ = normalized;
    BuildTreeRows();

    float rowH = S(kTreeRowHeight);
    for (size_t i = 0; i < treeRows_.size(); ++i) {
        if (treeRows_[i].node != node) continue;
        float rowTop = treeViewRect_.top + i * rowH;
        float rowBottom = rowTop + rowH;
        float viewTop = treeViewRect_.top;
        float viewBottom = treeViewRect_.bottom;
        if (rowTop < viewTop) {
            treeScroll_ = std::max(0.0f, treeScroll_ - (viewTop - rowTop));
        } else if (rowBottom > viewBottom) {
            treeScroll_ += rowBottom - viewBottom;
        }
        float maxScroll = std::max(0.0f, treeContentHeight_ - RectHeight(treeViewRect_));
        treeScroll_ = std::clamp(treeScroll_, 0.0f, maxScroll);
        BuildTreeRows();
        break;
    }
}

void App::BuildQuickFiles() {
    quickFiles_.clear();
    quickFilesDirty_ = false;
    if (projectRoot_.empty()) return;
    std::filesystem::path root(projectRoot_);
    std::error_code ec;
    if (!std::filesystem::is_directory(root, ec)) return;
    CollectQuickFiles(root, root, 0, quickFiles_);
    std::sort(quickFiles_.begin(), quickFiles_.end(), [](const QuickFileInfo& a, const QuickFileInfo& b) {
        return ToLower(a.relative) < ToLower(b.relative);
    });
}

void App::RefreshQuickIndex() {
    if (quickFilesDirty_) BuildQuickFiles();
}
