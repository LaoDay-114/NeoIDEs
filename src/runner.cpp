#include "runner.h"

namespace {

void CloseHandleSafe(HANDLE& handle) {
    if (handle && handle != INVALID_HANDLE_VALUE) {
        CloseHandle(handle);
        handle = nullptr;
    }
}

std::string Win32Error(DWORD code) {
    wchar_t* buffer = nullptr;
    DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                  FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD len = FormatMessageW(flags, nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                               (LPWSTR)&buffer, 0, nullptr);
    std::string text;
    if (len && buffer) {
        text = WideToUtf8(std::wstring(buffer, len));
        while (!text.empty() && (text.back() == '\r' || text.back() == '\n' || text.back() == ' ')) {
            text.pop_back();
        }
    }
    if (buffer) LocalFree(buffer);
    if (text.empty()) text = "Windows error " + std::to_string(code);
    return text;
}

std::wstring DecodeBytes(const std::string& bytes, UINT codePage) {
    if (bytes.empty()) return {};
    if (codePage == CP_UTF8) return Utf8ToWide(bytes);
    int len = MultiByteToWideChar(codePage, 0, bytes.data(), (int)bytes.size(), nullptr, 0);
    if (len <= 0) return {};
    std::wstring wide((size_t)len, L'\0');
    MultiByteToWideChar(codePage, 0, bytes.data(), (int)bytes.size(), wide.data(), len);
    return wide;
}

std::string EncodeText(const std::wstring& text, UINT codePage) {
    if (text.empty()) return {};
    if (codePage == CP_UTF8) return WideToUtf8(text);
    int len = WideCharToMultiByte(codePage, 0, text.c_str(), (int)text.size(),
                                  nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string bytes((size_t)len, '\0');
    WideCharToMultiByte(codePage, 0, text.c_str(), (int)text.size(),
                        bytes.data(), len, nullptr, nullptr);
    return bytes;
}

} // namespace

std::wstring StripAnsiSequences(const std::wstring& text) {
    std::wstring out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == 0x1B && i + 1 < text.size()) {
            if (text[i + 1] == L'[') {
                i += 2;
                while (i < text.size()) {
                    wchar_t c = text[i];
                    if ((c >= L'@' && c <= L'~')) break;
                    ++i;
                }
                continue;
            }
            if (text[i + 1] == L']') {
                i += 2;
                while (i < text.size() && text[i] != 0x07 && text[i] != 0x1B) ++i;
                if (i < text.size() && text[i] == 0x1B) ++i;
                continue;
            }
            ++i;
            continue;
        }
        out.push_back(text[i]);
    }
    return out;
}

std::wstring SearchExecutable(const wchar_t* name) {
    if (!name || !*name) return {};
    wchar_t buffer[MAX_PATH * 2]{};
    DWORD len = SearchPathW(nullptr, name, nullptr, (DWORD)std::size(buffer), buffer, nullptr);
    if (len > 0 && len < std::size(buffer)) {
        return std::wstring(buffer, len);
    }
    wchar_t module[MAX_PATH]{};
    DWORD moduleLen = GetModuleFileNameW(nullptr, module, (DWORD)std::size(module));
    if (moduleLen > 0) {
        std::filesystem::path path(module);
        auto sibling = path.parent_path() / name;
        std::error_code ec;
        if (std::filesystem::exists(sibling, ec)) return sibling.wstring();
        auto bin = path.parent_path() / L"bin" / name;
        if (std::filesystem::exists(bin, ec)) return bin.wstring();
    }
    return {};
}

Process::~Process() {
    Stop();
    if (reader_.joinable()) reader_.join();
    CloseHandleSafe(stdin_);
    CloseHandleSafe(process_);
}

bool Process::Start(const std::wstring& executable, const std::wstring& arguments,
                    const std::wstring& workingDirectory, std::wstring& error) {
    if (reader_.joinable()) {
        error = L"A process is already attached.";
        return false;
    }
    if (done_.load()) {
        error = L"This Process object has already been used.";
        return false;
    }

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = nullptr;
    sa.bInheritHandle = TRUE;

    HANDLE stdinRead = nullptr;
    HANDLE stdinWrite = nullptr;
    HANDLE stdoutRead = nullptr;
    HANDLE stdoutWrite = nullptr;
    if (!CreatePipe(&stdinRead, &stdinWrite, &sa, 0) ||
        !CreatePipe(&stdoutRead, &stdoutWrite, &sa, 0)) {
        error = L"Could not create process pipes: " + Utf8ToWide(Win32Error(GetLastError()));
        CloseHandleSafe(stdinRead);
        CloseHandleSafe(stdinWrite);
        CloseHandleSafe(stdoutRead);
        CloseHandleSafe(stdoutWrite);
        return false;
    }
    SetHandleInformation(stdinWrite, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stdoutRead, HANDLE_FLAG_INHERIT, 0);

    std::wstring commandLine = QuoteArg(executable);
    if (!arguments.empty()) {
        commandLine += L" ";
        commandLine += arguments;
    }
    std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
    mutableCommand.push_back(L'\0');

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdInput = stdinRead;
    si.hStdOutput = stdoutWrite;
    si.hStdError = stdoutWrite;

    PROCESS_INFORMATION pi{};
    std::wstring cwd = workingDirectory;
    BOOL ok = CreateProcessW(
        executable.c_str(),
        mutableCommand.data(),
        nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
        nullptr,
        cwd.empty() ? nullptr : cwd.c_str(),
        &si, &pi);
    CloseHandleSafe(stdinRead);
    CloseHandleSafe(stdoutWrite);

    if (!ok) {
        CloseHandleSafe(stdinWrite);
        DWORD code = GetLastError();
        error = L"Could not start \"" + executable + L"\": " + Utf8ToWide(Win32Error(code));
        CloseHandleSafe(stdoutRead);
        return false;
    }

    CloseHandleSafe(pi.hThread);
    stdin_ = stdinWrite;
    process_ = pi.hProcess;
    done_.store(false);
    stopRequested_.store(false);
    exitCode_.store(-1);
    reader_ = std::thread(&Process::ReaderLoop, this, stdoutRead);
    return true;
}

void Process::ReaderLoop(HANDLE readPipe) {
    char buffer[8192];
    while (true) {
        DWORD read = 0;
        BOOL ok = ReadFile(readPipe, buffer, sizeof(buffer), &read, nullptr);
        if (!ok || read == 0) break;
        std::lock_guard<std::mutex> lock(mutex_);
        lineBuffer_.append(buffer, read);
        lastByteTick_ = GetTickCount64();
    }
    CloseHandleSafe(readPipe);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!lineBuffer_.empty()) {
            pending_.push_back(StripAnsiSequences(DecodeBytes(lineBuffer_, codePage_)));
            lineBuffer_.clear();
        }
    }
    if (process_) {
        WaitForSingleObject(process_, INFINITE);
        DWORD code = 0;
        if (GetExitCodeProcess(process_, &code)) exitCode_.store((int)code);
        else exitCode_.store(-1);
    }
    done_.store(true);
}

void Process::Stop() {
    CloseHandleSafe(stdin_);
    if (!process_ || done_.load()) return;
    stopRequested_.store(true);
    TerminateProcess(process_, 1);
}

void Process::WriteLine(const std::wstring& text) {
    if (!stdin_ || stdin_ == INVALID_HANDLE_VALUE) return;
    std::string bytes = EncodeText(text, codePage_);
    bytes += "\r\n";
    DWORD written = 0;
    WriteFile(stdin_, bytes.data(), (DWORD)bytes.size(), &written, nullptr);
}

bool Process::PollLines(std::vector<std::wstring>& lines) {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t start = 0;
    while (true) {
        size_t nl = lineBuffer_.find('\n', start);
        if (nl == std::string::npos) break;
        std::string lineBytes = lineBuffer_.substr(start, nl - start);
        if (!lineBytes.empty() && lineBytes.back() == '\r') lineBytes.pop_back();
        pending_.push_back(StripAnsiSequences(DecodeBytes(lineBytes, codePage_)));
        start = nl + 1;
    }
    if (start > 0) lineBuffer_.erase(0, start);
    // Console prompts have no trailing newline. Flush the tail once output has
    // been idle for a moment so the prompt is visible.
    if (!lineBuffer_.empty() && GetTickCount64() - lastByteTick_ >= 80) {
        pending_.push_back(StripAnsiSequences(DecodeBytes(lineBuffer_, codePage_)));
        lineBuffer_.clear();
    }
    if (pending_.empty()) return false;
    lines.insert(lines.end(), std::make_move_iterator(pending_.begin()),
                 std::make_move_iterator(pending_.end()));
    pending_.clear();
    return true;
}

bool Process::PollDone(bool& done, int& exitCode) {
    if (!done_.load()) return false;
    done = true;
    exitCode = exitCode_.load();
    return true;
}

bool LaunchInConsole(const std::wstring& executable, const std::wstring& arguments,
                     const std::wstring& workingDirectory, std::wstring& error) {
    std::wstring commandLine = QuoteArg(executable);
    if (!arguments.empty()) {
        commandLine += L" ";
        commandLine += arguments;
    }
    std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
    mutableCommand.push_back(L'\0');

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::wstring cwd = workingDirectory;
    BOOL ok = CreateProcessW(
        executable.c_str(), mutableCommand.data(), nullptr, nullptr, FALSE,
        CREATE_NEW_CONSOLE | CREATE_UNICODE_ENVIRONMENT, nullptr,
        cwd.empty() ? nullptr : cwd.c_str(), &si, &pi);
    if (!ok) {
        error = L"Could not launch the program: " + Utf8ToWide(Win32Error(GetLastError()));
        return false;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}
