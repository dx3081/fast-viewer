#pragma once
#include <windows.h>

namespace input {

// Centralized input mapping. The app executes actions; this translation layer
// is the single place where mouse/keyboard triggers are bound to actions, so
// mappings can be changed without restructuring the viewer.
enum class Action {
    None,
    ZoomIn,      // wheel up
    ZoomOut,     // wheel down
    PanBegin,
    PanMove,
    PanEnd,
    ToggleMode,  // F11 / double-click
    Close,       // Esc / right-click
    PrevImage,   // Left
    NextImage,   // Right
    Toggle100,   // '1'
};

struct WheelInfo {
    int delta = 0;
    POINT clientPos{};
};

struct PanInfo {
    POINT clientPos{};
};

// Translates a raw window message into an action (+ parameters).
Action TranslateMessage(UINT msg, WPARAM wParam, LPARAM lParam, HWND hwnd,
                        WheelInfo* wheelOut = nullptr, PanInfo* panOut = nullptr);

} // namespace input
