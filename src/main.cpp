#include "app.h"

#include <objbase.h>

namespace {

// Opt in to per-monitor DPI awareness v2 when the OS supports it. Doing this
// before creating any window lets Direct2D render at native resolution and
// keeps the custom chrome crisp when the window moves between monitors.
void EnableDpiAwareness() {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) return;
    using SetContextFn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
    auto setContext = reinterpret_cast<SetContextFn>(
        GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
    if (setContext && setContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) return;
    using SetAwarenessFn = BOOL(WINAPI*)();
    auto setAwareness = reinterpret_cast<SetAwarenessFn>(
        GetProcAddress(user32, "SetProcessDPIAware"));
    if (setAwareness) setAwareness();
}

} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    EnableDpiAwareness();

    HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    bool comInitialized = SUCCEEDED(com);
    if (!comInitialized && com == RPC_E_CHANGED_MODE) {
        // Another component already initialized COM with a different model;
        // shell dialogs still work in that case.
        comInitialized = false;
    }

    int exitCode = 0;
    {
        App app;
        if (!app.Create(hInstance, nCmdShow)) {
            MessageBoxW(nullptr, L"NeoIDEs could not create its main window.",
                        L"NeoIDEs", MB_ICONERROR | MB_OK);
            exitCode = 1;
        } else {
            exitCode = app.Run();
        }
    }

    if (comInitialized) CoUninitialize();
    return exitCode;
}
