#pragma once
#include <windows.h>
#include <d2d1.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <cstdio>
#include <memory>
#include <string>

#include "navigation.h"
#include "renderer.h"
#include "window.h"

struct DecodedSource;

// Exit-path instrumentation helper (declared here for main.cpp).
void DebugMark(const wchar_t* s);

// Owns the viewer behavior: window messages, view state (fit/100%/zoom/pan),
// image loading, navigation and lightweight timing instrumentation.
class App {
public:
    ~App();

    bool Initialize(HINSTANCE inst, const std::wstring& imagePath);
    int Run();

private:
    friend class Window;

    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    // image / view
    void LoadImage(const std::wstring& path);
    void ComputeFit();
    void ResetViewToFit();
    void ComputeTransform(ViewTransform& view) const;
    void DrawNow();
    void LogView();

    // actions (dispatched from the centralized input mapping)
    void Close();
    void ToggleMode();
    void Toggle100Percent();
    void ZoomAt(POINT clientPt, int wheelDelta);
    void PanBegin(POINT clientPt);
    void PanMove(POINT clientPt);
    void PanEnd();
    void Navigate(int delta);
    void OnScanComplete();

    // instrumentation
    void InitTimingLog();
    void Log(const std::wstring& line);
    double ElapsedMs() const;
    long long StartMicros() const { return t0Micros_; }

    HINSTANCE inst_ = nullptr;
    HWND hwnd_ = nullptr;
    Window window_;
    std::unique_ptr<Renderer> renderer_;
    Microsoft::WRL::ComPtr<IWICImagingFactory> wic_;

    std::shared_ptr<DecodedSource> loaded_;
    Microsoft::WRL::ComPtr<ID2D1Bitmap> image_;
    std::wstring currentPath_;
    bool hasImage_ = false;
    bool errorState_ = false;
    std::wstring errorText_;

    bool immersive_ = true;
    enum class ViewMode { Fit, Percent100, Custom };
    ViewMode viewMode_ = ViewMode::Fit;
    float scale_ = 1.0f;
    float panX_ = 0.0f;
    float panY_ = 0.0f;
    float fitScale_ = 1.0f;

    bool panning_ = false;
    POINT panLast_{};

    std::unique_ptr<Navigation> nav_;
    std::shared_ptr<Navigation::Result> navResult_;
    bool navReady_ = false;
    int pendingDelta_ = 0;

    // timing
    long long t0Micros_ = 0;
    bool firstRenderLogged_ = false;
    std::wstring logPath_; // timing log (set only when FAST_VIEWER_TIMING is set)
};
