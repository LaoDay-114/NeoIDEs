#pragma once

#include "common.h"

inline std::wstring TranslateUiText(const std::wstring& text, bool chinese) {
    if (!chinese) return text;
    static const std::unordered_map<std::wstring, std::wstring> translations = {
        {L"File", L"文件"}, {L"Edit", L"编辑"}, {L"View", L"视图"},
        {L"Terminal", L"终端"}, {L"Help", L"帮助"},
        {L"New File", L"新建文件"}, {L"New File in Folder…", L"在文件夹中新建文件…"},
        {L"Open File…", L"打开文件…"}, {L"Open Folder…", L"打开文件夹…"},
        {L"Open File", L"打开文件"}, {L"Open Folder", L"打开文件夹"},
        {L"Save", L"保存"}, {L"Save As…", L"另存为…"}, {L"Save All", L"全部保存"},
        {L"Close Editor", L"关闭编辑器"}, {L"Close All Editors", L"关闭所有编辑器"},
        {L"Exit", L"退出"}, {L"Undo", L"撤销"}, {L"Redo", L"重做"},
        {L"Cut", L"剪切"}, {L"Copy", L"复制"}, {L"Paste", L"粘贴"},
        {L"Find in File…", L"在文件中查找…"}, {L"Find Next", L"查找下一个"},
        {L"Find Previous", L"查找上一个"}, {L"Go to Line…", L"转到行…"},
        {L"Command Palette…", L"命令面板…"}, {L"Quick Open File…", L"快速打开文件…"},
        {L"Toggle Sidebar", L"切换侧边栏"}, {L"Toggle Terminal", L"切换终端"},
        {L"Focus Terminal", L"聚焦终端"}, {L"Clear Terminal", L"清空终端"},
        {L"Zoom In", L"放大"}, {L"Zoom Out", L"缩小"}, {L"Reset Zoom", L"重置缩放"},
        {L"Toggle Light / Dark Theme", L"切换浅色/深色主题"},
        {L"Settings…", L"设置…"}, {L"Stop Terminal", L"停止终端"},
        {L"Refresh Explorer", L"刷新资源管理器"}, {L"About NeoIDEs", L"关于 NeoIDEs"},
        {L"Settings", L"设置"}, {L"Appearance, editor behavior and terminal", L"外观、编辑器行为和终端"},
        {L"Untitled", L"未命名"}, {L"Untitled-", L"未命名-"},
        {L"APPEARANCE", L"外观"}, {L"EDITOR", L"编辑器"}, {L"WORKSPACE", L"工作区"},
        {L"TERMINAL", L"终端"}, {L"Theme", L"主题"}, {L"Dark", L"深色"},
        {L"Light", L"浅色"}, {L"Language", L"界面语言"}, {L"English", L"English"},
        {L"中文", L"中文"}, {L"Font size", L"字体大小"},
        {L"Ctrl + mouse wheel also zooms", L"按住 Ctrl 并滚动鼠标滚轮也可缩放"},
        {L"Word wrap", L"自动换行"}, {L"Show hidden files", L"显示隐藏文件"},
        {L"Restore last session", L"恢复上次会话"}, {L"UTF-8 output", L"UTF-8 输出"},
        {L"Runs cmd with chcp 65001 so Chinese text displays correctly.",
         L"使用 chcp 65001 运行 cmd，以正确显示中文文本。"},
        {L"Changes are saved automatically.    Esc to close.", L"更改会自动保存。按 Esc 关闭。"},
        {L"Version 1.0.0", L"版本 1.0.0"},
        {L"A modern, lightweight IDE for C and C++, built with the native Windows API, Direct2D and DirectWrite. No Electron. No web runtime. Just a fast native editor.",
         L"一款使用原生 Windows API、Direct2D 和 DirectWrite 构建的轻量 C/C++ IDE。无需 Electron 或 Web 运行时，专注于快速原生编辑体验。"},
        {L"Win32  •  Direct2D  •  DirectWrite  •  C++20",
         L"Win32  •  Direct2D  •  DirectWrite  •  C++20"},
        {L"ESSENTIAL SHORTCUTS", L"常用快捷键"},
        {L"Ctrl+P  Quick open        Ctrl+Shift+P  Command palette\nCtrl+F  Find in file      F5  Terminal         Ctrl+J  Toggle panel",
         L"Ctrl+P  快速打开        Ctrl+Shift+P  命令面板\nCtrl+F  文件内查找      F5  终端         Ctrl+J  切换面板"},
        {L"EXPLORER", L"资源管理器"}, {L"No Folder", L"未打开文件夹"},
        {L"No folder is open", L"尚未打开文件夹"}, {L"Open Folder", L"打开文件夹"},
        {L"Empty folder", L"空文件夹"},
        {L"Command Palette", L"命令面板"},
        {L"Refresh", L"刷新"}, {L"Open a folder to search its files", L"打开文件夹以搜索文件"},
        {L"No matching files", L"没有匹配的文件"}, {L"No matching commands", L"没有匹配的命令"},
        {L"Type a command…", L"输入命令…"}, {L"Type a file name…", L"输入文件名…"},
        {L"Search files by name…", L"按名称搜索文件…"},
        {L"↑↓ Navigate     Enter Run     Esc Close", L"↑↓ 导航    Enter 运行    Esc 关闭"},
        {L"↑↓ Navigate     Enter Open     Esc Close", L"↑↓ 导航    Enter 打开    Esc 关闭"},
        {L"Go to Line", L"转到行"}, {L"Enter a line number between 1 and ", L"请输入 1 到 "},
        {L"Line number", L"行号"}, {L"Cancel", L"取消"}, {L"Go", L"转到"},
        {L"Create", L"创建"}, {L"File name", L"文件名"}, {L"Close", L"关闭"},
        {L"Find", L"查找"}, {L"No results", L"无结果"},
        {L"Ready", L"就绪"}, {L"Explorer refreshed", L"资源管理器已刷新"},
        {L"Editor closed", L"编辑器已关闭"}, {L"Created", L"已创建"},
        {L"Unsaved Changes", L"未保存的更改"}, {L"Save Changes?", L"保存更改？"},
        {L"Don't Save", L"不保存"},
        {L"You have unsaved files. Save them before exiting NeoIDEs?",
         L"有文件尚未保存。退出 NeoIDEs 前是否保存？"},
        {L"Several open files contain unsaved changes. Save all of them before closing?",
         L"多个打开的文件有未保存的更改。关闭前是否全部保存？"},
        {L"A fast, focused editor for C and C++.", L"快速、专注的 C/C++ 编辑器。"},
        {L"RECENT PROJECTS", L"最近项目"},
        {L"Open a folder to start exploring your project. Drag a file or folder here anytime.",
         L"打开文件夹以浏览项目。也可以随时将文件或文件夹拖到此处。"},
        {L"Tip: press Ctrl+P to jump to a file, or Ctrl+Shift+P for every command.",
         L"提示：按 Ctrl+P 快速打开文件，或按 Ctrl+Shift+P 查看所有命令。"},
        {L"ESSENTIAL SHORTCUTS", L"常用快捷键"}, {L"Type a command below and press Enter.",
         L"在下方输入命令并按 Enter。"},
        {L"Terminal ready", L"终端已就绪"}, {L"Terminal stopped", L"终端已停止"},
        {L"Terminal stopped.", L"终端已停止。"}, {L"Terminal exited", L"终端已退出"},
        {L"Terminal exited with code ", L"终端已退出，代码："},
        {L"Could not start the terminal", L"无法启动终端"},
        {L"Type a line number between 1 and ", L"请输入 1 到 "},
        {L"Please enter a file name.", L"请输入文件名。"},
        {L"That file already exists.", L"该文件已存在。"},
        {L"Could not create the file.", L"无法创建文件。"},
        {L"Could not initialize Direct2D / DirectWrite.", L"无法初始化 Direct2D / DirectWrite。"},
        {L"Copied", L"已复制"},
        {L"OK", L"确定"}, {L"Don't Save", L"不保存"}, {L"Save Changes?", L"保存更改？"},
        {L"Several open files contain unsaved changes. Save all of them before closing?",
         L"多个打开的文件有未保存的更改。关闭前是否全部保存？"},
        {L"Exit Without Saving", L"不保存并退出"}, {L"Save and Exit", L"保存并退出"},
        {L"Could Not Open File", L"无法打开文件"}, {L"Could Not Save File", L"无法保存文件"},
        {L"Save File", L"保存文件"}, {L"Open Files", L"打开文件"},
        {L"Text Files", L"文本文件"}, {L"Source Files", L"源文件"}, {L"All Files", L"所有文件"},
        {L"Enter a file name, relative to the project root:",
         L"请输入相对于项目根目录的文件名："},
        {L"Please enter a file name.", L"请输入文件名。"},
        {L"That file already exists.", L"该文件已存在。"},
        {L"Could not create the file.", L"无法创建文件。"},
        {L"All files saved", L"所有文件已保存"},
        {L"Folder no longer exists.", L"文件夹已不存在。"},
        {L"UTF-8", L"UTF-8"}, {L"UTF-8 BOM", L"UTF-8 BOM"}, {L"UTF-16 LE", L"UTF-16 LE"},
        {L"Plain Text", L"纯文本"},
        {L"Get started", L"开始使用"},
        {L"RECENT PROJECTS", L"最近项目"},
        {L"Tab", L"标签页"}
    };
    auto it = translations.find(text);
    if (it != translations.end()) return it->second;
    const std::pair<const wchar_t*, const wchar_t*> prefixes[] = {
        {L"Created ", L"已创建 "}, {L"Opened folder ", L"已打开文件夹 "},
        {L"Opened ", L"已打开 "}, {L"Saved ", L"已保存 "},
        {L"Could not open ", L"无法打开 "}, {L"Open failed: ", L"打开失败："},
        {L"Save failed: ", L"保存失败："}, {L"Source file not found: ", L"找不到源文件："},
        {L"Enter a line number between 1 and ", L"请输入 1 到 "},
        {L"Untitled-", L"未命名-"}, {L"Spaces: ", L"空格："}
    };
    for (const auto& [english, chinesePrefix] : prefixes) {
        if (IStartsWith(text, english)) {
            return std::wstring(chinesePrefix) + text.substr(std::wstring(english).size());
        }
        if (IStartsWith(text, L"Ln ")) {
            size_t column = text.find(L", Col ");
            if (column != std::wstring::npos) {
                return L"行 " + text.substr(3, column - 3) + L"，列 " +
                       text.substr(column + 6);
            }
        }
    }
    return text;
}
