#include <windows.h>
#include <shellapi.h>
#include <string>

#include "app.h"

namespace {

void SetDpiAwareness() {
    // Per-monitor v2 (Windows 10 1703+). Fallback for older builds.
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    auto setDpiContext = reinterpret_cast<BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT)>(
        GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
    if (setDpiContext) {
        setDpiContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        return;
    }
    SetProcessDPIAware();
}

} // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int) {
    SetDpiAwareness();

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return 1;

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::wstring imagePath;
    if (argv && argc >= 2) {
        imagePath = argv[1];
    }
    if (argv) LocalFree(argv);

    // No usable image path: exit cleanly. No home screen, no file picker.
    DWORD attrs = GetFileAttributesW(imagePath.c_str());
    if (imagePath.empty() || attrs == INVALID_FILE_ATTRIBUTES ||
        (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        CoUninitialize();
        return 0;
    }

    App app;
    if (!app.Initialize(inst, imagePath)) {
        CoUninitialize();
        return 1;
    }
    DebugMark(L"run begin");
    const int rc = app.Run();
    DebugMark(L"run returned");
    DebugMark(L"before CoUninitialize");
    CoUninitialize();
    DebugMark(L"after CoUninitialize");
    return rc;
}
