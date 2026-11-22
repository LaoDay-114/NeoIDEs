#pragma once

#include "common.h"

struct TreeNode {
    std::wstring path;
    std::wstring name;
    bool isDir = false;
    bool expanded = false;
    bool loaded = false;
    std::vector<std::unique_ptr<TreeNode>> children;
};

struct TreeRow {
    TreeNode* node = nullptr;
    int depth = 0;
};

class FileTree {
public:
    std::wstring root;
    std::unique_ptr<TreeNode> rootNode;
    bool showHidden = false;

    void SetRoot(const std::wstring& newRoot, bool hidden);
    void Refresh();
    void Clear();
    void Toggle(TreeNode& node);
    void LoadChildren(TreeNode& node);
    void Flatten(std::vector<TreeRow>& rows) const;
    TreeNode* FindNode(const std::wstring& path) const;

private:
    std::set<std::wstring> ExpandedPaths() const;
    void CollectExpanded(const TreeNode& node, std::set<std::wstring>& out) const;
    void RestoreExpanded(const std::set<std::wstring>& paths);
    bool RestoreExpandedRecursive(TreeNode& node, const std::set<std::wstring>& paths);
    void FlattenNode(const TreeNode& node, int depth, std::vector<TreeRow>& rows) const;
    void SortChildren(TreeNode& node);
    TreeNode* FindRecursive(TreeNode& node, const std::wstring& path) const;
};
