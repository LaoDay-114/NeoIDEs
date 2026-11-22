#include "file_tree.h"

namespace {

bool IsExcludedDir(const std::wstring& name) {
    static const std::unordered_set<std::wstring> excluded = {
        L".git", L".hg", L".svn", L".neoide", L".vs", L"node_modules",
        L"__pycache__", L"$RECYCLE.BIN", L"System Volume Information"
    };
    return excluded.count(name) > 0;
}

} // namespace

void FileTree::Clear() {
    root.clear();
    rootNode.reset();
}

void FileTree::SetRoot(const std::wstring& newRoot, bool hidden) {
    showHidden = hidden;
    std::set<std::wstring> expanded = ExpandedPaths();
    root = NormalizePath(newRoot);
    rootNode = std::make_unique<TreeNode>();
    rootNode->path = root;
    rootNode->name = BaseName(root);
    rootNode->isDir = true;
    rootNode->expanded = true;
    rootNode->loaded = false;
    std::error_code ec;
    if (!std::filesystem::exists(std::filesystem::path(root), ec) ||
        !std::filesystem::is_directory(std::filesystem::path(root), ec)) {
        rootNode->loaded = false;
        return;
    }
    LoadChildren(*rootNode);
    RestoreExpanded(expanded);
}

void FileTree::Refresh() {
    if (root.empty()) return;
    SetRoot(root, showHidden);
}

void FileTree::LoadChildren(TreeNode& node) {
    if (!node.isDir || node.loaded) return;
    node.children.clear();
    std::error_code ec;
    std::filesystem::directory_iterator it(std::filesystem::path(node.path),
                                           std::filesystem::directory_options::skip_permission_denied, ec);
    if (!ec) {
        for (const auto& entry : it) {
            std::error_code entryError;
            std::wstring name = entry.path().filename().wstring();
            if (name.empty()) continue;
            if (!showHidden && name[0] == L'.') continue;
            bool dir = entry.is_directory(entryError);
            if (entryError) continue;
            if (dir && IsExcludedDir(name)) continue;
            auto child = std::make_unique<TreeNode>();
            child->path = entry.path().wstring();
            child->name = name;
            child->isDir = dir;
            node.children.push_back(std::move(child));
        }
    }
    SortChildren(node);
    node.loaded = true;
}

void FileTree::SortChildren(TreeNode& node) {
    std::sort(node.children.begin(), node.children.end(), [](const auto& a, const auto& b) {
        if (a->isDir != b->isDir) return a->isDir > b->isDir;
        return IEquals(a->name, b->name) ? a->name < b->name : ToLower(a->name) < ToLower(b->name);
    });
}

void FileTree::Toggle(TreeNode& node) {
    if (!node.isDir) return;
    if (!node.expanded) {
        LoadChildren(node);
        node.expanded = true;
    } else {
        node.expanded = false;
    }
}

void FileTree::Flatten(std::vector<TreeRow>& rows) const {
    rows.clear();
    if (!rootNode) return;
    for (const auto& child : rootNode->children) {
        FlattenNode(*child, 0, rows);
    }
}

void FileTree::FlattenNode(const TreeNode& node, int depth, std::vector<TreeRow>& rows) const {
    rows.push_back(TreeRow{const_cast<TreeNode*>(&node), depth});
    if (node.isDir && node.expanded) {
        for (const auto& child : node.children) {
            FlattenNode(*child, depth + 1, rows);
        }
    }
}

std::set<std::wstring> FileTree::ExpandedPaths() const {
    std::set<std::wstring> paths;
    if (rootNode) CollectExpanded(*rootNode, paths);
    return paths;
}

void FileTree::CollectExpanded(const TreeNode& node, std::set<std::wstring>& out) const {
    if (node.isDir && node.expanded) out.insert(ToLower(node.path));
    for (const auto& child : node.children) {
        CollectExpanded(*child, out);
    }
}

void FileTree::RestoreExpanded(const std::set<std::wstring>& paths) {
    if (!rootNode || paths.empty()) return;
    for (const auto& path : paths) {
        if (IEquals(path, ToLower(root))) continue;
    }
    RestoreExpandedRecursive(*rootNode, paths);
}

bool FileTree::RestoreExpandedRecursive(TreeNode& node, const std::set<std::wstring>& paths) {
    if (!node.isDir) return false;
    bool any = false;
    if (!node.loaded) LoadChildren(node);
    for (auto& child : node.children) {
        if (!child->isDir) continue;
        std::wstring key = ToLower(child->path);
        if (paths.count(key)) {
            if (!child->loaded) LoadChildren(*child);
            child->expanded = true;
            any = true;
            RestoreExpandedRecursive(*child, paths);
        } else {
            // A nested path can still be restored even if a parent was collapsed
            // while loading; recurse only when it has saved descendants.
            for (const auto& path : paths) {
                if (IStartsWith(path, key + L"\\") || IStartsWith(path, key + L"/")) {
                    child->expanded = true;
                    RestoreExpandedRecursive(*child, paths);
                    any = true;
                    break;
                }
            }
        }
    }
    return any;
}

TreeNode* FileTree::FindNode(const std::wstring& path) const {
    if (!rootNode) return nullptr;
    std::wstring wanted = ToLower(path);
    return FindRecursive(*rootNode, wanted);
}

TreeNode* FileTree::FindRecursive(TreeNode& node, const std::wstring& path) const {
    if (ToLower(node.path) == path) return &node;
    for (auto& child : node.children) {
        if (auto found = FindRecursive(*child, path)) return found;
    }
    return nullptr;
}
