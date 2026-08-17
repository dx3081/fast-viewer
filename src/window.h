#pragma once
#include <windows.h>
#include <string>

class App;

// Thin Win32 window wrapper: class registration, creation, immersive/normal
// mode switching, and normal-mode geometry persistence (tiny registry key).
class Window {
public:
    ~Window();

    bool Create(HINSTANCE inst, const std::wstring& title, App* handler);
    void Destroy();

    HWND Hwnd() const { return hwnd_; }
    bool IsImmersive() const { return immersive_; }

    void SetImmersive(bool immersive);
    void ApplyDpiChanged(const RECT& suggestedRect);
    RECT ClientRect() const; // physical pixels

    void UpdateNormalRect(); // track normal-mode geometry (move/resize)
    void SaveNormalRect();   // persist to HKCU\Software\FastViewer

private:
    static LRESULT CALLBACK WndProcThunk(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void ApplyStyleAndPosition();
    bool LoadNormalRect(RECT& out);

    HWND hwnd_ = nullptr;
    App* handler_ = nullptr;
    bool immersive_ = false;
    RECT normalRect_{ 0, 0, 0, 0 };
    bool hasNormalRect_ = false;
};
