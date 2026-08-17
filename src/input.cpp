#include "input.h"

#include <windowsx.h>

namespace input {

Action TranslateMessage(UINT msg, WPARAM wParam, LPARAM lParam, HWND hwnd,
                        WheelInfo* wheelOut, PanInfo* panOut) {
    switch (msg) {
    case WM_KEYDOWN:
        switch (static_cast<UINT>(wParam)) {
        case VK_ESCAPE: return Action::Close;
        case VK_F11:    return Action::ToggleMode;
        case VK_LEFT:   return Action::PrevImage;
        case VK_RIGHT:  return Action::NextImage;
        case '1':       return Action::Toggle100;
        default:        return Action::None;
        }
    case WM_LBUTTONDBLCLK: return Action::ToggleMode;
    case WM_RBUTTONUP:     return Action::Close;
    case WM_LBUTTONDOWN:
        if (panOut) panOut->clientPos = POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        return Action::PanBegin;
    case WM_MOUSEMOVE:
        if (panOut) panOut->clientPos = POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        return (wParam & MK_LBUTTON) ? Action::PanMove : Action::None;
    case WM_LBUTTONUP:
        if (panOut) panOut->clientPos = POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        return Action::PanEnd;
    case WM_MOUSEWHEEL:
        if (wheelOut) {
            wheelOut->delta = GET_WHEEL_DELTA_WPARAM(wParam);
            POINT screen{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &screen);
            wheelOut->clientPos = screen;
        }
        return (wheelOut && wheelOut->delta > 0) ? Action::ZoomIn : Action::ZoomOut;
    case WM_CONTEXTMENU:
        return Action::None; // swallowed: right-click is a direct close action
    default:
        return Action::None;
    }
}

} // namespace input
