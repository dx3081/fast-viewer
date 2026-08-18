#include "app.h"

#include <windowsx.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <format>
#include <string_view>

#include "image_loader.h"
#include "input.h"

namespace {

constexpr UINT kMsgScanDone = WM_APP + 1;
constexpr UINT_PTR kFilmstripHideTimer = 2;
constexpr float kZoomFactorPerNotch = 1.25f;
constexpr float kMaxZoom = 16.0f;
constexpr float kMinZoomSafety = 0.01f;
constexpr double kMiB = 1024.0 * 1024.0;
constexpr uint64_t kThumbCacheBudget = 24ULL * 1024 * 1024; // 24 MB thumbnail cache
constexpr size_t kMaxThumbFailed = 512;

bool ThumbFailedContains(const std::vector<std::wstring>& v, const std::wstring& path) {
    return std::find(v.begin(), v.end(), path) != v.end();
}

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
    decoder_->Stop();
    thumbLoader_->Stop();
    DebugMark(L"app dtor end");
}

bool App::Initialize(HINSTANCE inst, const std::wstring& imagePath) {
    inst_ = inst;
    t0Micros_ = NowMicros();
    InitTimingLog();

    if (!window_.Create(inst, FileNameOf(imagePath), this)) return false;
    hwnd_ = window_.Hwnd();

    renderer_ = std::make_unique<Renderer>();
    if (!renderer_->Initialize(hwnd_)) return false;

    decoder_ = std::make_unique<ImageDecoder>();
    decoder_->Start(hwnd_);
    cache_ = std::make_unique<ImageCache>();
    nav_ = std::make_unique<Navigation>();
    thumbCache_ = std::make_unique<ImageCache>(kThumbCacheBudget);
    thumbLoader_ = std::make_unique<ThumbnailLoader>();
    thumbLoader_->Start(hwnd_);

    // First image display must not wait for directory discovery or any decode
    // on the UI thread: the initial decode runs on the decode worker.
    RequestUserDecode(imagePath, -1, false);
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
        if (wParam == SIZE_MINIMIZED) {
            if (!minimized_) {
                minimized_ = true;
                // Invalidate any in-flight preload; do not start new ones.
                latestPreloadId_ = ++nextRequestId_;
                preloadPathPending_.clear();
                Log(L"minimized");
            }
        } else if (wParam == SIZE_RESTORED && minimized_) {
            minimized_ = false;
            Log(L"restored");
            SchedulePreload();
        }
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
    case kMsgDecodeDone:
        OnDecodeDone(static_cast<uint64_t>(wParam));
        return 0;
    case kMsgPreloadDone:
        OnPreloadDone(static_cast<uint64_t>(wParam));
        return 0;
    case kMsgThumbDone:
        OnThumbDone(static_cast<uint64_t>(wParam));
        return 0;
    case WM_TIMER:
        if (wParam == kFilmstripHideTimer) {
            KillTimer(hwnd_, kFilmstripHideTimer);
            hideTimerRunning_ = false;
            HideFilmstrip();
            return 0;
        }
        break;
    case WM_MOUSEMOVE:
        TrackFilmstripPointer(POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });
        break; // fall through to input translation (pan moves)
    case WM_MOUSEWHEEL: {
        POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hwnd_, &pt);
        if (filmstrip_.Visible() && filmstrip_.IsOverStrip(pt)) {
            // wheel up = previous, wheel down = next (conventional filmstrip direction)
            Navigate(GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? -1 : +1);
            return 0;
        }
        break;
    }
    case WM_LBUTTONDOWN: {
        POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (filmstrip_.Visible()) {
            const int idx = filmstrip_.HitTest(pt);
            if (idx >= 0) {
                GoToIndex(idx);
                return 0;
            }
            if (filmstrip_.IsOverStrip(pt)) return 0; // strip background: no pan
        }
        break;
    }
    case WM_LBUTTONDBLCLK: {
        POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (filmstrip_.Visible() && filmstrip_.IsOverStrip(pt)) return 0;
        break;
    }
    case WM_NCLBUTTONDBLCLK: {
        // In normal window mode the title bar's close button (X, HTCLOSE) is
        // live. A double-click on it would make DefWindowProc generate
        // SC_CLOSE, closing the viewer. Fast Viewer only closes via Esc,
        // right-click, or a deliberate single click on the X; swallow the
        // double-click so it cannot close. Single-click X close is preserved
        // (handled by the WM_NCLBUTTONDOWN/UP default path).
        if (static_cast<int>(wParam) == HTCLOSE) return 0;
        break;
    }
    case WM_DESTROY:
        DebugMark(L"wm_destroy");
        KillTimer(hwnd_, kFilmstripHideTimer);
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

void App::ComputeFit() {
    fitScale_ = 1.0f;
    if (!hasImage_ || !current_ || !current_->bitmap) return;
    const float vw = static_cast<float>(renderer_->Width());
    const float vh = static_cast<float>(renderer_->Height());
    const float iw = static_cast<float>(current_->width);
    const float ih = static_cast<float>(current_->height);
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

void App::ComputeTransform(ViewTransform& view) const {
    view.scale = scale_;
    if (hasImage_ && current_ && current_->bitmap) {
        const float vw = static_cast<float>(renderer_->Width());
        const float vh = static_cast<float>(renderer_->Height());
        const float iw = static_cast<float>(current_->width);
        const float ih = static_cast<float>(current_->height);
        view.offsetX = (vw - iw * scale_) / 2.0f + panX_;
        view.offsetY = (vh - ih * scale_) / 2.0f + panY_;
    } else {
        view.offsetX = 0.0f;
        view.offsetY = 0.0f;
    }
}

// M0.1: deterministic pan clamping. Per axis: if the scaled image fits the
// viewport it stays centered (axis locked); otherwise pan is limited so every
// image edge/corner is reachable but overscroll into empty background is not.
void App::ClampPan() {
    if (!hasImage_ || !current_ || !current_->bitmap) {
        panX_ = 0.0f;
        panY_ = 0.0f;
        return;
    }
    const float vw = static_cast<float>(renderer_->Width());
    const float vh = static_cast<float>(renderer_->Height());
    const float iw = static_cast<float>(current_->width);
    const float ih = static_cast<float>(current_->height);

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

void App::DrawNow() {
    if (!renderer_) return;
    const int count = (navReady_ && navResult_)
                          ? static_cast<int>(navResult_->files.size())
                          : 0;
    filmstrip_.Update(renderer_->Width(), renderer_->Height(), DpiScale(), count,
                      displayIndex_);
    ViewTransform view;
    ComputeTransform(view);
    FilmstripDraw fs;
    BuildFilmstripDraw(fs);
    renderer_->Render(current_ ? current_->bitmap.Get() : nullptr, view,
                      errorState_, errorText_, fs);
    if (fs.visible) {
        if (fs.infoText != lastInfoText_) {
            lastInfoText_ = fs.infoText;
            Log(std::format(L"filmstrip: info {}", fs.infoText));
        }
        float curCx = -1.0f;
        for (const auto& c : filmstrip_.Cells()) {
            if (c.isCurrent) curCx = (c.rect.left + c.rect.right) / 2.0f;
        }
        const float dpi = DpiScale();
        // Log the first real cell index (>= 0): the logical row start may be
        // negative when the current image is near the directory start.
        const int firstReal = std::max(0, filmstrip_.VisibleStart());
        const std::wstring key = std::format(L"{}|{}|{:.0f}", firstReal,
                                             filmstrip_.VisibleCount(), curCx);
        if (key != lastGeomKey_) {
            lastGeomKey_ = key;
            Log(std::format(
                L"filmstrip: geometry start={} count={} cellw={:.0f} cellh={:.0f} gap={:.0f} margin={:.0f} top={:.0f} striph={:.0f} curcx={:.1f}",
                firstReal, filmstrip_.VisibleCount(),
                Filmstrip::kThumbW * dpi, Filmstrip::kThumbH * dpi,
                Filmstrip::kGap * dpi, Filmstrip::kMargin * dpi, fs.stripRect.top,
                fs.stripRect.bottom - fs.stripRect.top, curCx));
        }
    }
}

void App::LogView() {
    Log(std::format(L"view: scale={:.4f} offset=({:.1f},{:.1f}) pan=({:.1f},{:.1f}) mode={}",
                    scale_,
                    (hasImage_ && current_
                         ? (renderer_->Width() - static_cast<float>(current_->width) * scale_) / 2.0f + panX_
                         : 0.0f),
                    (hasImage_ && current_
                         ? (renderer_->Height() - static_cast<float>(current_->height) * scale_) / 2.0f + panY_
                         : 0.0f),
                    panX_, panY_,
                    viewMode_ == ViewMode::Fit ? L"Fit"
                    : viewMode_ == ViewMode::Percent100 ? L"100"
                                                        : L"Custom"));
}

// --- navigation / async decode --------------------------------------------

void App::Navigate(int delta) {
    if (!navReady_ || !navResult_ || navResult_->files.empty()) {
        pendingDelta_ += delta; // applied when the directory scan completes
        return;
    }
    const int idx = navResult_->index;
    if (idx < 0) return; // launched file is not in the navigable set

    const int base = (targetIndex_ >= 0) ? targetIndex_
                    : (displayIndex_ >= 0) ? displayIndex_
                                           : idx;
    const int target = base + delta;
    if (target < 0 || target >= static_cast<int>(navResult_->files.size())) {
        return; // no wrap-around
    }
    lastNavDirection_ = delta;
    StartNavigation(navResult_->files[target], target);
}

void App::StartNavigation(const std::wstring& path, int target) {
    if (auto cached = cache_->Get(path)) {
        Log(std::format(L"cache: hit {}", path));
        navLogPending_ = true;
        requestT0Micros_ = NowMicros();
        DisplayImage(cached, target, true, 0);
    } else {
        Log(std::format(L"cache: miss {}", path));
        RequestUserDecode(path, target, true);
    }
}

void App::RequestUserDecode(const std::wstring& path, int index, bool navLog) {
    targetIndex_ = index;
    const uint64_t id = ++nextRequestId_;
    latestUserRequestId_ = id;
    requestT0Micros_ = NowMicros();
    navLogPending_ = navLog;
    decoder_->RequestDecode(id, path);
    Log(std::format(L"decode: request id={} idx={}", id, index));
}

void App::OnScanComplete() {
    navResult_ = nav_->TakeResult();
    navReady_ = true;
    if (displayIndex_ < 0) displayIndex_ = navResult_->index;
    Log(std::format(L"scan: count={} index={} ms={:.1f}",
                    static_cast<int>(navResult_->files.size()), navResult_->index,
                    navResult_->scanMicros / 1000.0));
    if (pendingDelta_ != 0) {
        const int d = pendingDelta_;
        pendingDelta_ = 0;
        const int base = (targetIndex_ >= 0) ? targetIndex_ : displayIndex_;
        const int target = base + d;
        if (target >= 0 && target < static_cast<int>(navResult_->files.size())) {
            lastNavDirection_ = d > 0 ? 1 : -1;
            StartNavigation(navResult_->files[target], target);
        }
    } else {
        SchedulePreload();
    }

    // RC fast fix: if the filmstrip was revealed before the directory scan
    // completed, its first drawn frame used count=0 (no navigation state yet)
    // and showed only the empty strip surface. Recompute the layout with the
    // now-final range so a visible strip jumps straight to the correct
    // centered/edge position instead of waiting for an unrelated redraw.
    if (filmstrip_.Visible()) {
        DrawNow();
        ScheduleThumbs();
    }
}

void App::OnDecodeDone(uint64_t id) {
    auto res = decoder_->TakeResult(id);
    if (!res) return;

    if (id != latestUserRequestId_) {
        Log(std::format(L"decode: stale id={} ignored", id));
        return;
    }
    const int targetIdx = targetIndex_;
    targetIndex_ = -1;

    if (res->failed) {
        if (targetIdx >= 0) displayIndex_ = targetIdx;
        ShowFailure(res->path, res->hr, res->decodeMicros);
        if (navLogPending_) {
            navLogPending_ = false;
            Log(std::format(L"nav: {} idx={} id={} total_ms={:.1f} (failed)",
                            lastNavDirection_ > 0 ? L"next" : L"prev", displayIndex_, id,
                            (NowMicros() - requestT0Micros_) / 1000.0));
        }
        SchedulePreload();
        return;
    }

    auto& px = res->pixels;
    auto img = std::make_shared<DecodedImage>();
    img->path = res->path;
    img->width = px->width;
    img->height = px->height;
    img->estimateBytes = px->estimateBytes;
    const long long tUp = NowMicros();
    img->bitmap = renderer_->CreateBitmapFromPixels(px->width, px->height,
                                                    px->pixels.data(), px->stride);
    const double uploadMs = (NowMicros() - tUp) / 1000.0;
    if (!img->bitmap) {
        ShowFailure(res->path, E_FAIL, res->decodeMicros);
        return;
    }

    Log(std::format(L"decode_ms={:.1f} {}x{} orient={} {} upload_ms={:.1f}",
                    res->decodeMicros / 1000.0, px->width, px->height, px->orientation,
                    res->path, uploadMs));
    DisplayImage(img, targetIdx, navLogPending_, id);
}

void App::DisplayImage(const std::shared_ptr<DecodedImage>& img, int index,
                       bool navLog, uint64_t id) {
    cache_->UnpinAll();
    cache_->Insert(img, true); // current image pinned
    const uint64_t evicted = cache_->EvictOld();
    if (evicted) {
        Log(std::format(L"cache: evict {:.0f}MB cache={:.0f}MB evicts={}", evicted / kMiB,
                        cache_->Bytes() / kMiB, cache_->Evictions()));
    }

    current_ = img;
    currentPath_ = img->path;
    hasImage_ = true;
    errorState_ = false;
    errorText_.clear();

    if (index >= 0) displayIndex_ = index;
    if (navResult_ && displayIndex_ < 0) displayIndex_ = navResult_->index;
    targetIndex_ = -1;

    if (!firstRenderLogged_) {
        firstRenderLogged_ = true;
        Log(std::format(L"first_render_ms={:.1f}", ElapsedMs()));
    }

    ResetViewToFit();
    const long long tDraw = NowMicros();
    DrawNow(); // presentation: swap + render the new image
    const double renderMs = (NowMicros() - tDraw) / 1000.0;
    if (navLog) {
        navLogPending_ = false;
        // total_ms covers the complete path: navigation request -> presented.
        Log(std::format(L"nav: {} idx={} id={} total_ms={:.1f} render_ms={:.1f}",
                        lastNavDirection_ > 0 ? L"next" : L"prev", displayIndex_, id,
                        (NowMicros() - requestT0Micros_) / 1000.0, renderMs));
    }
    SchedulePreload();
    ScheduleThumbs();
}

void App::OnPreloadDone(uint64_t id) {
    auto res = decoder_->TakeResult(id);
    if (!res) return;
    if (id != latestPreloadId_) {
        Log(std::format(L"preload: stale id={} dropped", id));
        return;
    }
    preloadPathPending_.clear();
    if (res->budgetExceeded) {
        Log(L"preload: skipped budget (estimate exceeds usable budget)");
        return;
    }
    if (res->failed) {
        Log(std::format(L"preload: failed id={} {}", id, res->path));
        return;
    }
    if (!IsNearby(res->path)) {
        Log(std::format(L"preload: dropped (moved away) id={}", id));
        return;
    }

    auto& px = res->pixels;
    auto img = std::make_shared<DecodedImage>();
    img->path = res->path;
    img->width = px->width;
    img->height = px->height;
    img->estimateBytes = px->estimateBytes;
    const long long tUp = NowMicros();
    img->bitmap = renderer_->CreateBitmapFromPixels(px->width, px->height,
                                                    px->pixels.data(), px->stride);
    const double uploadMs = (NowMicros() - tUp) / 1000.0;
    if (!img->bitmap) {
        Log(std::format(L"preload: bitmap create failed id={}", id));
        return;
    }
    cache_->Insert(img, false);
    const uint64_t evicted = cache_->EvictOld();
    if (evicted) {
        Log(std::format(L"cache: evict {:.0f}MB cache={:.0f}MB evicts={}", evicted / kMiB,
                        cache_->Bytes() / kMiB, cache_->Evictions()));
    }
    Log(std::format(L"preload: done id={} est={:.0f}MB upload_ms={:.1f} cache={:.0f}MB evicts={} {}",
                    id, img->estimateBytes / kMiB, uploadMs, cache_->Bytes() / kMiB,
                    cache_->Evictions(), res->path));
}

void App::SchedulePreload() {
    if (minimized_) {
        Log(L"preload: skipped minimized");
        return;
    }
    if (targetIndex_ >= 0) return; // a user request is in flight
    if (!navReady_ || !navResult_ || displayIndex_ < 0) return;
    const int count = static_cast<int>(navResult_->files.size());
    if (count == 0) return;

    int candidates[3];
    int nc = 0;
    auto add = [&](int i) {
        if (i >= 0 && i < count) candidates[nc++] = i;
    };
    add(displayIndex_ - 1);
    add(displayIndex_ + 1);
    if (lastNavDirection_ > 0) add(displayIndex_ + 2);
    if (lastNavDirection_ < 0) add(displayIndex_ - 2);

    for (int i = 0; i < nc; ++i) {
        const int idx = candidates[i];
        const std::wstring& path = navResult_->files[idx];
        if (cache_->Contains(path)) continue;
        if (!preloadPathPending_.empty() && preloadPathPending_ == path) continue;
        const uint64_t id = ++nextRequestId_;
        latestPreloadId_ = id;
        preloadPathPending_ = path;
        decoder_->RequestPreload(id, path, cache_->Bytes(),
                                 current_ ? current_->estimateBytes : 0);
        Log(std::format(L"preload: request id={} idx={} cache={:.0f}MB", id, idx,
                        cache_->Bytes() / kMiB));
        return; // one preload at a time
    }
}

bool App::IsNearby(const std::wstring& path) const {
    if (!navReady_ || !navResult_ || displayIndex_ < 0) return false;
    const int count = static_cast<int>(navResult_->files.size());
    for (int d = -2; d <= 2; ++d) {
        const int i = displayIndex_ + d;
        if (i >= 0 && i < count && navResult_->files[i] == path) return true;
    }
    return false;
}

void App::ShowFailure(const std::wstring& path, HRESULT hr, uint64_t decodeMicros) {
    hasImage_ = false;
    errorState_ = true;
    errorText_ = std::format(L"Cannot display this image.\n{}", path);
    currentPath_ = path;
    current_.reset();
    Log(std::format(L"error: {} hr=0x{:08X} decode_ms={:.1f}", path,
                    static_cast<unsigned>(hr), decodeMicros / 1000.0));
    DrawNow();
    SchedulePreload();
    ScheduleThumbs();
}

// --- filmstrip ------------------------------------------------------------

float App::DpiScale() const {
    const UINT dpi = GetDpiForWindow(hwnd_);
    return dpi ? static_cast<float>(dpi) / 96.0f : 1.0f;
}

void App::TrackFilmstripPointer(POINT pt) {
    if (panning_) return; // preserve active drag; hot zone resumes after release
    const LONG h = static_cast<LONG>(renderer_->Height());
    if (filmstrip_.Visible()) {
        if (filmstrip_.IsOverStrip(pt)) {
            if (hideTimerRunning_) {
                KillTimer(hwnd_, kFilmstripHideTimer);
                hideTimerRunning_ = false;
            }
        } else if (!hideTimerRunning_) {
            SetTimer(hwnd_, kFilmstripHideTimer, Filmstrip::kHideDelayMs, nullptr);
            hideTimerRunning_ = true;
        }
    } else {
        const LONG zone = static_cast<LONG>(Filmstrip::kHotZoneH * DpiScale());
        if (pt.y >= h - zone && pt.y <= h) RevealFilmstrip();
    }
}

void App::RevealFilmstrip() {
    filmstrip_.Show();
    lastGeomKey_.clear();
    Log(L"filmstrip: show");
    ScheduleThumbs();
    DrawNow();
}

void App::HideFilmstrip() {
    filmstrip_.Hide();
    ++thumbGen_; // in-flight thumbnail results become stale
    Log(L"filmstrip: hide");
    DrawNow();
}

void App::GoToIndex(int target) {
    if (!navReady_ || !navResult_ || target < 0 ||
        target >= static_cast<int>(navResult_->files.size())) {
        return;
    }
    if (target == displayIndex_ && targetIndex_ < 0) return; // already there
    lastNavDirection_ = (target > displayIndex_) ? 1 : -1;
    StartNavigation(navResult_->files[target], target);
}

void App::ScheduleThumbs() {
    if (!filmstrip_.Visible() || !navReady_ || !navResult_ || displayIndex_ < 0) return;
    const int count = static_cast<int>(navResult_->files.size());
    if (count == 0) return;
    const UINT maxDim = static_cast<UINT>(Filmstrip::kThumbH * DpiScale());
    // Logical row [start, start+width); start may be negative at the start of
    // the directory. Walk the row plus one neighbor on each side; indices
    // outside the directory are skipped (edge cells simply do not exist).
    const int start = filmstrip_.VisibleStart();
    const int width = filmstrip_.VisibleWidth();
    for (int i = start - 1; i < start + width + 1; ++i) {
        if (i < 0 || i >= count) continue;
        const std::wstring& path = navResult_->files[i];
        if (thumbCache_->Contains(path) || ThumbFailedContains(thumbFailed_, path)) continue;
        const uint64_t gen = ++thumbGen_;
        thumbLoader_->RequestThumb(gen, path, maxDim);
        Log(std::format(L"thumb: request gen={} idx={}", gen, i));
        return; // one thumbnail job at a time (latest-wins)
    }
}

void App::OnThumbDone(uint64_t gen) {
    auto res = thumbLoader_->TakeResult(gen);
    if (!res) return;
    if (gen != thumbGen_) {
        Log(std::format(L"thumb: stale gen={} ignored", gen));
        return;
    }
    if (res->failed || !res->pixels) {
        // Record the failure so a broken file is never retried forever.
        if (thumbFailed_.size() >= kMaxThumbFailed) thumbFailed_.erase(thumbFailed_.begin());
        thumbFailed_.push_back(res->path);
        Log(std::format(L"thumb: failed gen={} {}", gen, res->path));
        ScheduleThumbs();
        return;
    }
    auto& px = res->pixels;
    auto img = std::make_shared<DecodedImage>();
    img->path = res->path;
    img->width = px->width;
    img->height = px->height;
    img->estimateBytes = px->estimateBytes;
    img->bitmap = renderer_->CreateBitmapFromPixels(px->width, px->height,
                                                    px->pixels.data(), px->stride);
    if (!img->bitmap) {
        Log(std::format(L"thumb: bitmap failed gen={}", gen));
        ScheduleThumbs();
        return;
    }
    thumbCache_->Insert(img, false);
    const uint64_t evicted = thumbCache_->EvictOld();
    Log(std::format(L"thumb: done gen={} {} est={:.0f}KB cache={:.1f}MB evicts={}",
                    gen, FileNameOf(res->path), img->estimateBytes / 1024.0,
                    thumbCache_->Bytes() / kMiB, thumbCache_->Evictions()));
    if (evicted) Log(std::format(L"thumb: evict {:.0f}KB", evicted / 1024.0));
    DrawNow();
    ScheduleThumbs();
}

std::wstring App::BuildInfoText() {
    std::wstring dims = L"?x?";
    if (hasImage_ && current_) {
        dims = std::format(L"{}x{}", current_->width, current_->height);
    }
    const int idx = displayIndex_ + 1;
    const int total = (navReady_ && navResult_)
                          ? static_cast<int>(navResult_->files.size())
                          : 0;
    return std::format(L"{}  {}  {} / {}", FileNameOf(currentPath_), dims, idx, total);
}

void App::BuildFilmstripDraw(FilmstripDraw& out) {
    out.visible = filmstrip_.Visible();
    if (!out.visible) return;
    out.stripRect = filmstrip_.StripRect();
    out.infoText = BuildInfoText();
    const auto& cells = filmstrip_.Cells();
    out.cells.reserve(cells.size());
    for (const auto& c : cells) {
        ThumbCellDraw d;
        d.index = c.index;
        d.rect = c.rect;
        d.isCurrent = c.isCurrent;
        if (navReady_ && navResult_ && c.index >= 0 &&
            c.index < static_cast<int>(navResult_->files.size())) {
            auto cached = thumbCache_->Get(navResult_->files[c.index]);
            if (cached) d.bitmap = cached->bitmap.Get();
        }
        out.cells.push_back(d);
    }
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
    if (!hasImage_ || !current_) return;
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
    if (!hasImage_ || !current_) return;
    const float vw = static_cast<float>(renderer_->Width());
    const float vh = static_cast<float>(renderer_->Height());
    if (vw <= 0.0f || vh <= 0.0f) return;
    const float iw = static_cast<float>(current_->width);
    const float ih = static_cast<float>(current_->height);

    const float curOffX = (vw - iw * scale_) / 2.0f + panX_;
    const float curOffY = (vh - ih * scale_) / 2.0f + panY_;
    const float imgX = (static_cast<float>(clientPt.x) - curOffX) / scale_;
    const float imgY = (static_cast<float>(clientPt.y) - curOffY) / scale_;

    const float notches = static_cast<float>(wheelDelta) / 120.0f;
    float newScale = scale_ * std::pow(kZoomFactorPerNotch, notches);
    const float minScale = std::max(fitScale_, kMinZoomSafety); // fit is the lower bound
    const float maxScale = std::max(kMaxZoom, fitScale_ * 8.0f);
    newScale = std::clamp(newScale, minScale, maxScale);

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
    if (!hasImage_ || !current_) return;
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
