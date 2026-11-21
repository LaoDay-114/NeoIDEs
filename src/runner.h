#pragma once

#include "common.h"

class Process {
public:
    Process() = default;
    ~Process();

    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;

    bool Start(const std::wstring& executable, const std::wstring& arguments,
               const std::wstring& workingDirectory, std::wstring& error);
    void Stop();

    // Send a line to the child's stdin. Used by the integrated terminal.
    void WriteLine(const std::wstring& text);
    // Code page used to decode stdout and encode stdin. UTF-8 by default.
    void SetCodePage(UINT codePage) { codePage_ = codePage; }

    bool PollLines(std::vector<std::wstring>& lines);
    bool PollDone(bool& done, int& exitCode);
    bool IsDone() const { return done_.load(); }
    bool WasStopped() const { return stopRequested_.load(); }
    int ExitCode() const { return exitCode_.load(); }

private:
    void ReaderLoop(HANDLE readPipe);

    HANDLE process_ = nullptr;
    HANDLE stdin_ = nullptr;
    UINT codePage_ = CP_UTF8;
    std::thread reader_;
    mutable std::mutex mutex_;
    std::string lineBuffer_;
    ULONGLONG lastByteTick_ = 0;
    std::vector<std::wstring> pending_;
    std::atomic<bool> done_{false};
    std::atomic<bool> stopRequested_{false};
    std::atomic<int> exitCode_{-1};
};

std::wstring SearchExecutable(const wchar_t* name);
bool LaunchInConsole(const std::wstring& executable, const std::wstring& arguments,
                     const std::wstring& workingDirectory, std::wstring& error);
std::wstring StripAnsiSequences(const std::wstring& text);
