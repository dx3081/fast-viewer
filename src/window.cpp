#include "window.h"

#include "app.h"

namespace {

const wchar_t kClassName[] = L"FastViewerWindow";
const wchar_t kRegKey[] = L"Software\\FastViewer";
const wchar_t kRegValue[] = L"NormalRect";

bool RectIsSane(const RECT& rc) {
    if (rc.right - rc.left < 320 || rc.bottom - rc.top < 240) return false;
    const RECT vs{
        GetSystemMetrics(SM_XVIRTUALSCREEN),
        GetSystemMetrics(SM_YVIRTUALSCREEN),
        GetSystemMetrics(SM_XVIRTUALSCREEN) + GetSystemMetrics(SM_CXVIRTUALSCREEN),
        GetSystemMetrics(SM_YVIRTUALSCREEN) + GetSystemMetrics(SM_CYVIRTUALSCREEN) };
    RECT inter{};
    return IntersectRect(&inter, &rc, &vs) != FALSE;
}

} // namespace

Window::~Window() {
    Destroy();
}

bool Window::Create(HINSTANCE inst, const std::wstring& title, App* handler) {
    handler_ = handler;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &Window::WndProcThunk;
    wc.hInstance = inst;
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kClassName;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    // Start immersive: fill the work area of the monitor containing the cursor.
    POINT pt{};
    GetCursorPos(&pt);
    HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(mon, &mi);
    const RECT rc = mi.rcWork;

    hwnd_ = CreateWindowExW(0, kClassName, title.c_str(), WS_POPUP,
                            rc.left, rc.top,
                            rc.right - rc.left, rc.bottom - rc.top,
                            nullptr, nullptr, inst, nullptr);
    if (!hwnd_) return false;

    SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    immersive_ = true;
    return true;
}

void Window::Destroy() {
    if (hwnd_ && IsWindow(hwnd_)) {
        DestroyWindow(hwnd_);
    }
    hwnd_ = nullptr;
}

RECT Window::ClientRect() const {
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    return rc;
}

void Window::SetImmersive(bool immersive) {
    if (immersive == immersive_) return;
    if (!immersive_ && !hasNormalRect_) {
        // leaving normal mode: keep the last geometry we tracked
    }
    immersive_ = immersive;
    ApplyStyleAndPosition();
}

void Window::ApplyStyleAndPosition() {
    RECT rc{};
    if (immersive_) {
        HMONITOR mon = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        GetMonitorInfoW(mon, &mi);
        rc = mi.rcWork;
    } else {
        if (!LoadNormalRect(rc)) {
            HMONITOR mon = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi{};
            mi.cbSize = sizeof(mi);
            GetMonitorInfoW(mon, &mi);
            const int w = static_cast<int>((mi.rcWork.right - mi.rcWork.left) * 0.72f);
            const int h = static_cast<int>((mi.rcWork.bottom - mi.rcWork.top) * 0.78f);
            const int x = mi.rcWork.left + ((mi.rcWork.right - mi.rcWork.left) - w) / 2;
            const int y = mi.rcWork.top + ((mi.rcWork.bottom - mi.rcWork.top) - h) / 2;
            rc = { x, y, x + w, y + h };
        }
        normalRect_ = rc;
        hasNormalRect_ = true;
    }

    // A window that was maximized (or carries a stale restore-to-maximized
    // placement) must be normalized before the style switch, otherwise the
    // system can reapply maximize/restore geometry and the toggle lands in a
    // half-maximized "weird window" state.
    WINDOWPLACEMENT wp{};
    wp.length = sizeof(wp);
    if (GetWindowPlacement(hwnd_, &wp) &&
        (wp.showCmd == SW_SHOWMAXIMIZED || (wp.flags & WPF_RESTORETOMAXIMIZED))) {
        wp.showCmd = SW_SHOWNORMAL;
        wp.flags = 0;
        SetWindowPlacement(hwnd_, &wp);
    }

    // Move to the target rect while the old frame is still present, then swap
    // the style and reframe in place. The user never sees the "borderless
    // window at the old position" half-state.
    SetWindowPos(hwnd_, nullptr, rc.left, rc.top,
                 rc.right - rc.left, rc.bottom - rc.top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowLongPtrW(hwnd_, GWL_STYLE,
                      (immersive_ ? WS_POPUP : WS_OVERLAPPEDWINDOW) | WS_VISIBLE);
    SetWindowPos(hwnd_, nullptr, rc.left, rc.top,
                 rc.right - rc.left, rc.bottom - rc.top,
                 SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOACTIVATE);
}

void Window::ApplyDpiChanged(const RECT& suggestedRect) {
    SetWindowPos(hwnd_, nullptr, suggestedRect.left, suggestedRect.top,
                 suggestedRect.right - suggestedRect.left,
                 suggestedRect.bottom - suggestedRect.top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

void Window::UpdateNormalRect() {
    // Only a genuinely restored normal-window placement is a valid NormalRect.
    // A maximized or minimized window reports a non-restored rect here (and
    // maximized adds WS_MAXIMIZE); persisting it would corrupt the stored
    // geometry. Immersive is additionally excluded via the immersive_ guard.
    if (immersive_) return;
    if (IsZoomed(hwnd_) || IsIconic(hwnd_)) return;
    GetWindowRect(hwnd_, &normalRect_);
    hasNormalRect_ = true;
}

void Window::SaveNormalRect() {
    if (!hasNormalRect_) return;
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegKey, 0, nullptr, 0, KEY_WRITE,
                        nullptr, &key, nullptr) != ERROR_SUCCESS) {
        return;
    }
    const DWORD data[4] = {
        static_cast<DWORD>(normalRect_.left),
        static_cast<DWORD>(normalRect_.top),
        static_cast<DWORD>(normalRect_.right - normalRect_.left),
        static_cast<DWORD>(normalRect_.bottom - normalRect_.top) };
    RegSetValueExW(key, kRegValue, 0, REG_BINARY,
                   reinterpret_cast<const BYTE*>(data), sizeof(data));
    RegCloseKey(key);
}

bool Window::LoadNormalRect(RECT& out) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegKey, 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return false;
    }
    DWORD data[4] = {};
    DWORD size = sizeof(data);
    const LONG r = RegQueryValueExW(key, kRegValue, nullptr, nullptr,
                                    reinterpret_cast<LPBYTE>(data), &size);
    RegCloseKey(key);
    if (r != ERROR_SUCCESS || size != sizeof(data)) return false;

    RECT rc{ static_cast<LONG>(data[0]), static_cast<LONG>(data[1]),
             static_cast<LONG>(data[0]) + static_cast<LONG>(data[2]),
             static_cast<LONG>(data[1]) + static_cast<LONG>(data[3]) };
    if (!RectIsSane(rc)) return false;
    out = rc;
    return true;
}

LRESULT CALLBACK Window::WndProcThunk(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self && self->handler_) {
        return self->handler_->HandleMessage(msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
