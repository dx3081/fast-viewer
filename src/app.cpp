#include "app.h"

#include <windowsx.h>
#include <algorithm>
#include <cmath>
#include <format>
#include <string_view>

#include "image_loader.h"
#include "input.h"

namespace {

constexpr UINT kMsgScanDone = WM_APP + 1;
constexpr float kZoomFactorPerNotch = 1.25f;
constexpr float kMaxZoom = 16.0f;
constexpr float kMinZoomSafety = 0.01f;

} // namespace

// Exit-path instrumentation (also visible in the timing log when enabled).
std::wstring g_timingPath;

void DebugMark(const wchar_t* s) {
    OutputDebugStringW(s);
    OutputDebugStringW(L"\n");
    if (g_timingPath.empty()) return;
    FILE* f = _wfsopen(g_timingPath.c_str(), L"a, ccs=UTF-8", _SH_DENYNO);
    if (f) {
        fwprintf(f, L"MARK %ls\n", s);
        fclose(f);
    }
}

long long NowMicros() {
    static LARGE_INTEGER freq{};
    if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    return now.QuadPart * 1000000LL / freq.QuadPart;
}

std::wstring FileNameOf(const std::wstring& path) {
    const size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? path : path.substr(slash + 1);
}

App::~App() {
    DebugMark(L"app dtor begin");
    DebugMark(L"app dtor end");
}

bool App::Initialize(HINSTANCE inst, const std::wstring& imagePath) {
    inst_ = inst;
    t0Micros_ = NowMicros();
    InitTimingLog();

    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&wic_));
    if (FAILED(hr)) return false;

    if (!window_.Create(inst, FileNameOf(imagePath), this)) return false;
    hwnd_ = window_.Hwnd();

    renderer_ = std::make_unique<Renderer>();
    if (!renderer_->Initialize(hwnd_)) return false;

    nav_ = std::make_unique<Navigation>();

    // First image display must not wait for directory discovery.
    LoadImage(imagePath);
    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);

    nav_->Start(hwnd_, kMsgScanDone, imagePath);

    Log(std::format(L"init_ms={:.1f}", ElapsedMs()));
    return true;
}

int App::Run() {
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

LRESULT App::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SIZE: {
        const UINT w = LOWORD(lParam);
        const UINT h = HIWORD(lParam);
        if (renderer_) {
            renderer_->Resize(w, h);
            if (viewMode_ == ViewMode::Fit) {
                ResetViewToFit();
            } else {
                ClampPan(); // reclamp pan to the new viewport bounds
                LogView();
                DrawNow();
            }
        }
        if (!immersive_) window_.UpdateNormalRect();
        return 0;
    }
    case WM_MOVE:
        if (!immersive_) window_.UpdateNormalRect();
        return 0;
    case WM_DPICHANGED: {
        const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
        if (immersive_) {
            HMONITOR mon = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi{};
            mi.cbSize = sizeof(mi);
            GetMonitorInfoW(mon, &mi);
            SetWindowPos(hwnd_, nullptr, mi.rcWork.left, mi.rcWork.top,
                         mi.rcWork.right - mi.rcWork.left,
                         mi.rcWork.bottom - mi.rcWork.top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        } else {
            window_.ApplyDpiChanged(*suggested);
        }
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        BeginPaint(hwnd_, &ps);
        DrawNow();
        EndPaint(hwnd_, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1; // avoid background flicker; D2D clears the target
    case kMsgScanDone:
        OnScanComplete();
        return 0;
    case WM_DESTROY:
        DebugMark(L"wm_destroy");
        window_.SaveNormalRect();
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }

    input::WheelInfo wheel;
    input::PanInfo pan;
    const input::Action action =
        input::TranslateMessage(msg, wParam, lParam, hwnd_, &wheel, &pan);

    switch (action) {
    case input::Action::Close:      Close(); break;
    case input::Action::ToggleMode: ToggleMode(); break;
    case input::Action::Toggle100:  Toggle100Percent(); break;
    case input::Action::ZoomIn:
    case input::Action::ZoomOut:    ZoomAt(wheel.clientPos, wheel.delta); break;
    case input::Action::PanBegin:   PanBegin(pan.clientPos); break;
    case input::Action::PanMove:    PanMove(pan.clientPos); break;
    case input::Action::PanEnd:     PanEnd(); break;
    case input::Action::PrevImage:  Navigate(-1); break;
    case input::Action::NextImage:  Navigate(+1); break;
    case input::Action::None:       break;
    }

    return DefWindowProcW(hwnd_, msg, wParam, lParam);
}

// --- image / view ---------------------------------------------------------

void App::LoadImage(const std::wstring& path) {
    const long long t = NowMicros();

    HRESULT hr = S_OK;
    auto decoded = DecodeImage(wic_.Get(), path, &hr);
    if (!decoded) {
        hasImage_ = false;
        errorState_ = true;
        errorText_ = std::format(L"Cannot display this image.\n{}", path);
        currentPath_ = path;
        image_.Reset();
        loaded_.reset();
        Log(std::format(L"error: {} hr=0x{:08X}", path, static_cast<unsigned>(hr)));
        DrawNow();
        return;
    }

    image_ = renderer_->CreateBitmap(decoded->source.Get(), &hr);
    if (!image_) {
        hasImage_ = false;
        errorState_ = true;
        errorText_ = std::format(L"Cannot display this image.\n{}", path);
        currentPath_ = path;
        loaded_.reset();
        Log(std::format(L"error: {} bitmap hr=0x{:08X}", path, static_cast<unsigned>(hr)));
        DrawNow();
        return;
    }

    loaded_ = std::move(decoded);
    hasImage_ = true;
    errorState_ = false;
    errorText_.clear();
    currentPath_ = path;

    Log(std::format(L"decode_ms={:.1f} {}x{} orient={} {}", (NowMicros() - t) / 1000.0,
                    loaded_->width, loaded_->height, loaded_->orientation, path));

    ResetViewToFit();
    DrawNow();
}

void App::ComputeFit() {
    fitScale_ = 1.0f;
    if (!hasImage_ || !image_) return;
    const float vw = static_cast<float>(renderer_->Width());
    const float vh = static_cast<float>(renderer_->Height());
    const D2D1_SIZE_F size = image_->GetSize();
    const float iw = size.width;
    const float ih = size.height;
    if (vw <= 0.0f || vh <= 0.0f || iw <= 0.0f || ih <= 0.0f) return;
    const float s = std::min(vw / iw, vh / ih);
    fitScale_ = std::min(s, 1.0f); // never upscale small images
}

void App::ResetViewToFit() {
    viewMode_ = ViewMode::Fit;
    ComputeFit();
    scale_ = std::max(fitScale_, kMinZoomSafety);
    panX_ = 0.0f;
    panY_ = 0.0f;
    ClampPan();
    LogView();
}

// M0.1: deterministic pan clamping. Per axis: if the scaled image fits the
// viewport it stays centered (axis locked); otherwise pan is limited so every
// image edge/corner is reachable but overscroll into empty background is not.
void App::ClampPan() {
    if (!hasImage_ || !image_) {
        panX_ = 0.0f;
        panY_ = 0.0f;
        return;
    }
    const float vw = static_cast<float>(renderer_->Width());
    const float vh = static_cast<float>(renderer_->Height());
    const D2D1_SIZE_F size = image_->GetSize();
    const float iw = size.width;
    const float ih = size.height;

    const float excessX = iw * scale_ - vw;
    const float excessY = ih * scale_ - vh;
    if (excessX <= 0.0f) {
        panX_ = 0.0f;
    } else {
        panX_ = std::clamp(panX_, -excessX / 2.0f, excessX / 2.0f);
    }
    if (excessY <= 0.0f) {
        panY_ = 0.0f;
    } else {
        panY_ = std::clamp(panY_, -excessY / 2.0f, excessY / 2.0f);
    }
}

void App::ComputeTransform(ViewTransform& view) const {
    view.scale = scale_;
    if (hasImage_ && image_) {
        const D2D1_SIZE_F size = image_->GetSize();
        const float vw = static_cast<float>(renderer_->Width());
        const float vh = static_cast<float>(renderer_->Height());
        view.offsetX = (vw - size.width * scale_) / 2.0f + panX_;
        view.offsetY = (vh - size.height * scale_) / 2.0f + panY_;
    } else {
        view.offsetX = 0.0f;
        view.offsetY = 0.0f;
    }
}

void App::DrawNow() {
    if (!renderer_) return;
    ViewTransform view;
    ComputeTransform(view);
    renderer_->Render(image_.Get(), view, errorState_, errorText_);

    if (!firstRenderLogged_ && hasImage_) {
        firstRenderLogged_ = true;
        Log(std::format(L"first_render_ms={:.1f}", ElapsedMs()));
    }
}

void App::LogView() {
    Log(std::format(L"view: scale={:.4f} offset=({:.1f},{:.1f}) pan=({:.1f},{:.1f}) mode={}",
                    scale_,
                    (hasImage_ ? (renderer_->Width() - image_->GetSize().width * scale_) / 2.0f + panX_ : 0.0f),
                    (hasImage_ ? (renderer_->Height() - image_->GetSize().height * scale_) / 2.0f + panY_ : 0.0f),
                    panX_, panY_,
                    viewMode_ == ViewMode::Fit ? L"Fit"
                    : viewMode_ == ViewMode::Percent100 ? L"100"
                                                        : L"Custom"));
}

// --- actions ---------------------------------------------------------------

void App::Close() {
    DestroyWindow(hwnd_);
}

void App::ToggleMode() {
    immersive_ = !immersive_;
    window_.SetImmersive(immersive_);
    Log(std::format(L"mode={}", immersive_ ? L"immersive" : L"normal"));
}

void App::Toggle100Percent() {
    if (!hasImage_ || !image_) return;
    if (viewMode_ == ViewMode::Percent100) {
        ResetViewToFit();
    } else {
        viewMode_ = ViewMode::Percent100;
        scale_ = 1.0f;
        panX_ = 0.0f;
        panY_ = 0.0f;
        ClampPan();
        LogView();
        DrawNow();
    }
}

void App::ZoomAt(POINT clientPt, int wheelDelta) {
    if (!hasImage_ || !image_) return;
    const float vw = static_cast<float>(renderer_->Width());
    const float vh = static_cast<float>(renderer_->Height());
    if (vw <= 0.0f || vh <= 0.0f) return;
    const D2D1_SIZE_F size = image_->GetSize();
    const float iw = size.width;
    const float ih = size.height;

    // Current screen position of the image origin.
    const float curOffX = (vw - iw * scale_) / 2.0f + panX_;
    const float curOffY = (vh - ih * scale_) / 2.0f + panY_;

    // Image-space point under the cursor.
    const float imgX = (static_cast<float>(clientPt.x) - curOffX) / scale_;
    const float imgY = (static_cast<float>(clientPt.y) - curOffY) / scale_;

    const float notches = static_cast<float>(wheelDelta) / 120.0f;
    float newScale = scale_ * std::pow(kZoomFactorPerNotch, notches);
    const float minScale = std::max(fitScale_, kMinZoomSafety); // fit is the lower bound
    const float maxScale = std::max(kMaxZoom, fitScale_ * 8.0f);
    newScale = std::clamp(newScale, minScale, maxScale);

    // Keep the image point under the cursor fixed.
    const float newOffX = static_cast<float>(clientPt.x) - imgX * newScale;
    const float newOffY = static_cast<float>(clientPt.y) - imgY * newScale;

    scale_ = newScale;
    panX_ = newOffX - (vw - iw * newScale) / 2.0f;
    panY_ = newOffY - (vh - ih * newScale) / 2.0f;

    constexpr float kEps = 1e-3f;
    viewMode_ = (std::fabs(scale_ - fitScale_) < kEps) ? ViewMode::Fit : ViewMode::Custom;
    ClampPan();
    LogView();
    DrawNow();
}

void App::PanBegin(POINT clientPt) {
    if (!hasImage_ || !image_) return;
    panning_ = true;
    panLast_ = clientPt;
    SetCapture(hwnd_);
}

void App::PanMove(POINT clientPt) {
    if (!panning_ || !hasImage_) return;
    panX_ += static_cast<float>(clientPt.x - panLast_.x);
    panY_ += static_cast<float>(clientPt.y - panLast_.y);
    panLast_ = clientPt;
    ClampPan();
    LogView();
    DrawNow();
}

void App::PanEnd() {
    if (!panning_) return;
    panning_ = false;
    ReleaseCapture();
}

void App::Navigate(int delta) {
    if (!navReady_ || !navResult_ || navResult_->files.empty()) {
        pendingDelta_ += delta; // applied when the directory scan completes
        return;
    }
    const int idx = navResult_->index;
    if (idx < 0) return; // launched file is not in the navigable set

    const int target = idx + delta;
    if (target < 0 || target >= static_cast<int>(navResult_->files.size())) {
        return; // no wrap-around
    }

    const long long t = NowMicros();
    navResult_->index = target;
    LoadImage(navResult_->files[target]);
    Log(std::format(L"nav: {} idx={} total_ms={:.1f}",
                    delta > 0 ? L"next" : L"prev", target,
                    (NowMicros() - t) / 1000.0));
}

void App::OnScanComplete() {
    navResult_ = nav_->TakeResult();
    navReady_ = true;
    Log(std::format(L"scan: count={} index={} ms={:.1f}",
                    static_cast<int>(navResult_->files.size()), navResult_->index,
                    navResult_->scanMicros / 1000.0));
    if (pendingDelta_ != 0) {
        const int d = pendingDelta_;
        pendingDelta_ = 0;
        Navigate(d);
    }
}

// --- instrumentation -------------------------------------------------------

void App::InitTimingLog() {
    wchar_t buf[MAX_PATH] = {};
    if (GetEnvironmentVariableW(L"FAST_VIEWER_TIMING", buf, MAX_PATH) == 0) return;
    if (wcscmp(buf, L"1") == 0) {
        wchar_t tmp[MAX_PATH] = {};
        if (!GetTempPathW(MAX_PATH, tmp)) return;
        logPath_ = std::wstring(tmp) + L"fast_viewer_timings.log";
    } else {
        logPath_ = buf; // FAST_VIEWER_TIMING may name an explicit log file path
    }
    g_timingPath = logPath_;
    _wremove(logPath_.c_str()); // fresh log per run
}

void App::Log(const std::wstring& line) {
    OutputDebugStringW(line.c_str());
    OutputDebugStringW(L"\n");
    if (logPath_.empty()) return;
    // Open-append-close with full sharing so external readers (test harness)
    // can read the log concurrently.
    FILE* f = _wfsopen(logPath_.c_str(), L"a, ccs=UTF-8", _SH_DENYNO);
    if (f) {
        fwprintf(f, L"[%6.1fms] %ls\n", ElapsedMs(), line.c_str());
        fclose(f);
    }
}

double App::ElapsedMs() const {
    return static_cast<double>(NowMicros() - t0Micros_) / 1000.0;
}
