#include "app.h"

#include <windowsx.h>
#include <shlobj.h>
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
constexpr UINT_PTR kAnimTimer = 3;
constexpr UINT_PTR kToastTimer = 4;
constexpr float kZoomFactorPerNotch = 1.25f;
constexpr float kMaxZoom = 16.0f;
constexpr float kMinZoomSafety = 0.01f;
constexpr double kMiB = 1024.0 * 1024.0;
constexpr uint64_t kThumbCacheBudget = 24ULL * 1024 * 1024; // 24 MB thumbnail cache
constexpr size_t kMaxThumbFailed = 512;

// UI text follows the Windows display language: Chinese or English.
const wchar_t* Text(const wchar_t* zh, const wchar_t* en) {
    static const bool chinese = PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_CHINESE;
    return chinese ? zh : en;
}

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
    // Initialize() may fail before these workers are created; stop only if
    // they exist. (Other members are unique_ptrs and are null-safe.)
    if (decoder_) decoder_->Stop();
    if (thumbLoader_) thumbLoader_->Stop();
    DebugMark(L"app dtor end");
}

bool App::Initialize(HINSTANCE inst, const std::wstring& imagePath) {
    inst_ = inst;
    t0Micros_ = NowMicros();
    InitTimingLog();

    // Start decoding the first image right away, in parallel with creating the
    // window and the Direct2D target (together ~100-250 ms on a cold start).
    // The window opens on the monitor under the cursor, so that monitor's
    // work area is the decode box.
    {
        POINT pt{};
        GetCursorPos(&pt);
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfoW(MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST), &mi)) {
            boxW_ = static_cast<UINT>(mi.rcWork.right - mi.rcWork.left);
            boxH_ = static_cast<UINT>(mi.rcWork.bottom - mi.rcWork.top);
        }
    }
    decoder_ = std::make_unique<ImageDecoder>();
    decoder_->Start(nullptr);
    RequestUserDecode(imagePath, -1, false);

    if (!window_.Create(inst, FileNameOf(imagePath), this)) return false;
    hwnd_ = window_.Hwnd();

    renderer_ = std::make_unique<Renderer>();
    if (!renderer_->Initialize(hwnd_)) return false;
    UpdateDecodeBox();
    decoder_->SetNotifyWindow(hwnd_);

    cache_ = std::make_unique<ImageCache>();
    nav_ = std::make_unique<Navigation>();
    thumbCache_ = std::make_unique<ImageCache>(kThumbCacheBudget);
    thumbLoader_ = std::make_unique<ThumbnailLoader>();
    thumbLoader_->Start(hwnd_);

    // First image display must not wait for directory discovery or any decode
    // on the UI thread: the initial decode is already running on the worker.
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
                StopAnimation();
                Log(L"minimized");
            }
        } else if (wParam == SIZE_RESTORED && minimized_) {
            minimized_ = false;
            Log(L"restored");
            SchedulePreload();
            StartAnimation();
        }
        if (renderer_) {
            renderer_->Resize(w, h);
            if (viewMode_ == ViewMode::Fit) {
                ResetViewToFit();
            } else {
                ClampPan(); // reclamp pan to the new viewport bounds
            }
            // Defer the expensive draw during a mode switch; ToggleMode()
            // repaints once after the transition completes.
            if (!suppressResizeRender_) {
                LogView();
                DrawNow();
                RequestDetailIfNeeded();
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
        UpdateDecodeBox();
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
        if (wParam == kAnimTimer) {
            OnAnimationTick();
            return 0;
        }
        if (wParam == kToastTimer) {
            KillTimer(hwnd_, kToastTimer);
            toast_.clear();
            DrawNow();
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
            if (const BarAction a = filmstrip_.HitButton(pt); a != BarAction::None) {
                DoBarAction(a);
                return 0;
            }
            const int idx = filmstrip_.HitTest(pt);
            if (idx >= 0) {
                GoToIndex(idx);
                return 0;
            }
            if (filmstrip_.IsOverStrip(pt)) return 0; // strip background: no pan
        }
        break;
    }
    case WM_NCLBUTTONDBLCLK: {
        // In normal window mode the caption double-click must toggle the mode
        // exactly like the client-area double-click / F11. The system's default
        // caption gesture (SC_MAXIMIZE / SC_RESTORE) must not run; HTCLOSE is
        // deliberately untouched so the title-bar close button keeps standard X
        // behavior. This never applies in immersive (no caption is present).
        if (wParam == HTCAPTION && !immersive_) {
            ToggleMode();
            return 0;
        }
        break;
    }
    case WM_LBUTTONDBLCLK: {
        POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (filmstrip_.Visible() && filmstrip_.IsOverStrip(pt)) {
            // The second click of a fast double click still counts on a button.
            if (const BarAction a = filmstrip_.HitButton(pt); a != BarAction::None) DoBarAction(a);
            return 0;
        }
        break;
    }
    case WM_DESTROY:
        DebugMark(L"wm_destroy");
        KillTimer(hwnd_, kFilmstripHideTimer);
        StopAnimation();
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
    case input::Action::RotateCW:   Rotate(+1); break;
    case input::Action::RotateCCW:  Rotate(-1); break;
    case input::Action::CopyImage:  CopyToClipboard(); break;
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
    const float iw = EffW();
    const float ih = EffH();
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
        const float iw = EffW();
        const float ih = EffH();
        view.offsetX = (vw - iw * scale_) / 2.0f + panX_;
        view.offsetY = (vh - ih * scale_) / 2.0f + panY_;
        view.imageW = static_cast<float>(current_->width);
        view.imageH = static_cast<float>(current_->height);
        view.quarterTurns = rotation_;
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
    const float iw = EffW();
    const float ih = EffH();

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
    ID2D1Bitmap* shown = nullptr;
    if (detail_) {
        shown = detail_->bitmap.Get();
    } else if (current_) {
        shown = (current_->frames.size() > 1 && animFrame_ < current_->frames.size())
                    ? current_->frames[animFrame_].Get()
                    : current_->bitmap.Get();
    }
    renderer_->Render(shown, view,
                      errorState_, errorText_, fs, toast_);
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
                         ? (renderer_->Width() - EffW() * scale_) / 2.0f + panX_
                         : 0.0f),
                    (hasImage_ && current_
                         ? (renderer_->Height() - EffH() * scale_) / 2.0f + panY_
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
        // This navigation supersedes any user decode still in flight: its
        // result must not replace the image shown now.
        const uint64_t id = ++nextRequestId_;
        latestUserRequestId_ = id;
        requestIndex_.clear();
        DisplayImage(cached, target, true, id);
    } else {
        Log(std::format(L"cache: miss {}", path));
        RequestUserDecode(path, target, true);
    }
}

void App::RequestUserDecode(const std::wstring& path, int index, bool navLog) {
    targetIndex_ = index;
    const uint64_t id = ++nextRequestId_;
    latestUserRequestId_ = id;
    detailRequestId_ = 0; // a navigation supersedes any pending zoom detail
    requestT0Micros_ = NowMicros();
    navLogPending_ = navLog;
    decoder_->RequestDecode(id, path, boxW_, boxH_);
    requestIndex_[id] = index;
    Log(std::format(L"decode: request id={} idx={}", id, index));
}

void App::OnScanComplete() {
    navResult_ = nav_->TakeResult();
    navReady_ = true;
    if (displayIndex_ < 0) displayIndex_ = navResult_->index;
    std::wstring exts;
    for (const auto& e : SupportedExtensions()) exts += e + L" ";
    Log(L"formats: " + exts);
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

    if (id != 0 && id == detailRequestId_) {
        OnDetailDone(res);
        return;
    }
    int resultIdx = -1;
    if (auto it = requestIndex_.find(id); it != requestIndex_.end()) {
        resultIdx = it->second;
        requestIndex_.erase(requestIndex_.begin(), std::next(it)); // drop it and older ids
    }
    if (id < shownRequestId_) {
        Log(std::format(L"decode: stale id={} ignored (newer image on screen)", id));
        return;
    }
    if (id != latestUserRequestId_) {
        // Superseded by a newer request (the user kept pressing). Still show it
        // while the newest one decodes, so holding an arrow key scrolls through
        // the folder instead of freezing until the key is released.
        if (!res->failed && id > shownRequestId_ && resultIdx >= 0) {
            double upMs = 0.0;
            if (auto img = MakeImage(*res, &upMs)) {
                Log(std::format(L"decode: interim id={} idx={} shown", id, resultIdx));
                DisplayImage(img, resultIdx, false, id, true);
                return;
            }
        }
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

    double uploadMs = 0.0;
    auto img = MakeImage(*res, &uploadMs);
    if (!img) {
        if (targetIdx >= 0) displayIndex_ = targetIdx;
        navLogPending_ = false;
        ShowFailure(res->path, E_FAIL, res->decodeMicros);
        return;
    }
    const auto& px = res->pixels;
    Log(std::format(L"decode_ms={:.1f} {}x{} (full {}x{}) orient={} {} upload_ms={:.1f}",
                    res->decodeMicros / 1000.0, px->width, px->height, px->fullWidth,
                    px->fullHeight, px->orientation, res->path, uploadMs));
    DisplayImage(img, targetIdx, navLogPending_, id);
}

void App::DisplayImage(const std::shared_ptr<DecodedImage>& img, int index,
                       bool navLog, uint64_t id, bool interim) {
    if (id > shownRequestId_) shownRequestId_ = id;
    cache_->UnpinAll();
    cache_->Insert(img, true); // current image pinned
    const uint64_t evicted = cache_->EvictOld();
    if (evicted) {
        Log(std::format(L"cache: evict {:.0f}MB cache={:.0f}MB evicts={}", evicted / kMiB,
                        cache_->Bytes() / kMiB, cache_->Evictions()));
    }

    current_ = img;
    rotation_ = 0;
    detail_.reset();
    detailFailedPath_.clear();
    detailRequestId_ = 0;
    currentPath_ = img->path;
    hasImage_ = true;
    errorState_ = false;
    errorText_.clear();

    if (index >= 0) displayIndex_ = index;
    if (navResult_ && displayIndex_ < 0) displayIndex_ = navResult_->index;
    if (!interim) targetIndex_ = -1; // an interim image keeps the newest request in flight

    if (!firstRenderLogged_) {
        firstRenderLogged_ = true;
        Log(std::format(L"first_render_ms={:.1f}", ElapsedMs()));
    }

    ResetViewToFit();
    StartAnimation();
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
    if (res->budgetExceeded || res->failed) {
        // A budget skip depends on how full the cache is right now; only a
        // real decode failure is remembered for the session.
        if (!res->budgetExceeded) RememberDecodeFailure(res->path);
        Log(std::format(L"preload: {} id={} {}",
                        res->budgetExceeded ? L"skipped budget" : L"failed", id, res->path));
        SchedulePreload();
        return;
    }
    if (!IsNearby(res->path)) {
        Log(std::format(L"preload: dropped (moved away) id={}", id));
        SchedulePreload();
        return;
    }

    double uploadMs = 0.0;
    auto img = MakeImage(*res, &uploadMs);
    if (!img) {
        Log(std::format(L"preload: bitmap create failed id={}", id));
        RememberDecodeFailure(res->path);
        SchedulePreload();
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
    SchedulePreload(); // keep going until the small neighborhood is ready
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

    // Small neighborhood, most likely next image first: the direction of
    // travel gets two images ahead, the other side one.
    const int dir = lastNavDirection_ < 0 ? -1 : 1;
    const int order[3] = { displayIndex_ + dir, displayIndex_ + 2 * dir, displayIndex_ - dir };
    int candidates[3];
    int nc = 0;
    for (int i : order) {
        if (i >= 0 && i < count) candidates[nc++] = i;
    }

    for (int i = 0; i < nc; ++i) {
        const int idx = candidates[i];
        const std::wstring& path = navResult_->files[idx];
        if (cache_->Contains(path)) continue;
        if (!preloadPathPending_.empty() && preloadPathPending_ == path) return; // in flight
        if (DecodeFailedBefore(path)) continue;
        const uint64_t id = ++nextRequestId_;
        latestPreloadId_ = id;
        preloadPathPending_ = path;
        decoder_->RequestPreload(id, path, cache_->Bytes(),
                                 current_ ? current_->estimateBytes : 0, boxW_, boxH_);
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
    std::wstring ext;
    if (const size_t dot = path.find_last_of(L'.'); dot != std::wstring::npos) ext = path.substr(dot);
    for (auto& c : ext) c = static_cast<wchar_t>(towlower(c));
    std::wstring hint;
    if (ext == L".heic" || ext == L".heif" || ext == L".hif") {
        hint = Text(L"\n\n这台电脑需要微软商店的“HEVC 视频扩展”才能显示 HEIC 照片。",
                    L"\n\nHEIC photos need the \"HEVC Video Extensions\" from the Microsoft Store.");
    } else if (ext == L".avif") {
        hint = Text(L"\n\n这台电脑需要微软商店的“AV1 视频扩展”才能显示 AVIF 图片。",
                    L"\n\nAVIF images need the \"AV1 Video Extension\" from the Microsoft Store.");
    }
    errorText_ = std::format(L"{}\n{}{}", Text(L"无法显示这张图片。", L"Cannot display this image."),
                             path, hint);
    currentPath_ = path;
    current_.reset();
    detail_.reset();
    detailRequestId_ = 0;
    StopAnimation();
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
        if (filmstrip_.SetHover(filmstrip_.HitButton(pt))) DrawNow();
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
    filmstrip_.SetHover(BarAction::None);
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
    std::wstring dims;
    if (hasImage_ && current_) {
        dims = (rotation_ % 2) ? std::format(L"({} x {})", current_->height, current_->width)
                               : std::format(L"({} x {})", current_->width, current_->height);
    }
    const int idx = displayIndex_ + 1;
    const int total = (navReady_ && navResult_)
                          ? static_cast<int>(navResult_->files.size())
                          : 0;
    if (total <= 0) return std::format(L"{} {}", FileNameOf(currentPath_), dims);
    return std::format(L"{} {}     {} / {}", FileNameOf(currentPath_), dims, idx, total);
}

void App::BuildFilmstripDraw(FilmstripDraw& out) {
    out.visible = filmstrip_.Visible();
    out.dpi = DpiScale();
    if (!out.visible) return;
    out.stripRect = filmstrip_.StripRect();
    for (const auto& b : filmstrip_.Buttons()) {
        out.buttons.push_back({ b.rect, b.glyph, b.iconFont, b.action == filmstrip_.Hover() });
    }
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

void App::DoBarAction(BarAction a) {
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    const POINT center{ (rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2 };
    switch (a) {
    case BarAction::ZoomIn:    ZoomAt(center, +WHEEL_DELTA); break;
    case BarAction::ZoomOut:   ZoomAt(center, -WHEEL_DELTA); break;
    case BarAction::Toggle100: Toggle100Percent(); break;
    case BarAction::RotateCCW: Rotate(-1); break;
    case BarAction::RotateCW:  Rotate(+1); break;
    case BarAction::Prev:      Navigate(-1); break;
    case BarAction::Next:      Navigate(+1); break;
    case BarAction::None:      break;
    }
}

// --- actions ---------------------------------------------------------------

void App::Close() {
    DestroyWindow(hwnd_);
}

void App::ToggleMode() {
    immersive_ = !immersive_;
    // Make the style/geometry switch visually atomic: suppress intermediate
    // renders during the transition (they are slow at high zoom and leave a
    // visible "borderless window" half-state).
    suppressResizeRender_ = true;
    window_.SetImmersive(immersive_);
    suppressResizeRender_ = false;
    // The final client size / render target / view state are now settled.
    // Refresh the view state for the final geometry, then force a synchronous
    // WM_PAINT (UpdateWindow) so the recreated D2D target is presented inside
    // a proper paint context — a bare DrawNow() outside WM_PAINT left the
    // visible surface stale (one-step-behind frame with a gray/black band).
    if (renderer_) {
        if (viewMode_ == ViewMode::Fit) {
            ResetViewToFit();
        } else {
            ClampPan();
        }
        LogView();
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
    UpdateWindow(hwnd_);
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
        RequestDetailIfNeeded();
    }
}

void App::ZoomAt(POINT clientPt, int wheelDelta) {
    if (!hasImage_ || !current_) return;
    const float vw = static_cast<float>(renderer_->Width());
    const float vh = static_cast<float>(renderer_->Height());
    if (vw <= 0.0f || vh <= 0.0f) return;
    const float iw = EffW();
    const float ih = EffH();

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
    RequestDetailIfNeeded();
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

// --- rotate / copy / toast --------------------------------------------------

float App::EffW() const {
    if (!current_) return 0.0f;
    return static_cast<float>((rotation_ % 2) ? current_->height : current_->width);
}

float App::EffH() const {
    if (!current_) return 0.0f;
    return static_cast<float>((rotation_ % 2) ? current_->width : current_->height);
}

void App::Rotate(int quarterTurns) {
    if (!hasImage_ || !current_) return;
    rotation_ = ((rotation_ + quarterTurns) % 4 + 4) % 4;
    ResetViewToFit(); // a rotated image is re-fitted (simple and predictable)
    DrawNow();
    Log(std::format(L"rotate: {} deg", rotation_ * 90));
}

void App::ShowToast(const std::wstring& text) {
    toast_ = text;
    SetTimer(hwnd_, kToastTimer, 1200, nullptr);
    DrawNow();
}

namespace {

// 32-bit bottom-up DIB from premultiplied BGRA (un-premultiplied for apps
// that read the alpha channel).
HGLOBAL MakeDib(const DecodedPixels& px) {
    const size_t rowBytes = static_cast<size_t>(px.width) * 4;
    const size_t bytes = sizeof(BITMAPINFOHEADER) + rowBytes * px.height;
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!mem) return nullptr;
    auto* p = static_cast<BYTE*>(GlobalLock(mem));
    BITMAPINFOHEADER bih{};
    bih.biSize = sizeof(bih);
    bih.biWidth = static_cast<LONG>(px.width);
    bih.biHeight = static_cast<LONG>(px.height); // positive = bottom-up
    bih.biPlanes = 1;
    bih.biBitCount = 32;
    bih.biCompression = BI_RGB;
    bih.biSizeImage = static_cast<DWORD>(rowBytes * px.height);
    memcpy(p, &bih, sizeof(bih));
    BYTE* dst = p + sizeof(bih);
    for (UINT y = 0; y < px.height; ++y) {
        const BYTE* s = px.pixels.data() + static_cast<size_t>(px.height - 1 - y) * px.stride;
        BYTE* d = dst + rowBytes * y;
        for (UINT x = 0; x < px.width; ++x, s += 4, d += 4) {
            const BYTE a = s[3];
            if (a == 255 || a == 0) {
                d[0] = s[0]; d[1] = s[1]; d[2] = s[2];
            } else {
                d[0] = static_cast<BYTE>(std::min(255, s[0] * 255 / a));
                d[1] = static_cast<BYTE>(std::min(255, s[1] * 255 / a));
                d[2] = static_cast<BYTE>(std::min(255, s[2] * 255 / a));
            }
            d[3] = a;
        }
    }
    GlobalUnlock(mem);
    return mem;
}

HGLOBAL MakeHdrop(const std::wstring& path) {
    const size_t bytes = sizeof(DROPFILES) + (path.size() + 2) * sizeof(wchar_t);
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, bytes);
    if (!mem) return nullptr;
    auto* df = static_cast<DROPFILES*>(GlobalLock(mem));
    df->pFiles = sizeof(DROPFILES);
    df->fWide = TRUE;
    memcpy(reinterpret_cast<BYTE*>(df) + sizeof(DROPFILES), path.c_str(),
           path.size() * sizeof(wchar_t)); // double-null terminated by ZEROINIT
    GlobalUnlock(mem);
    return mem;
}

} // namespace

// Ctrl+C: the image (as shown: EXIF orientation + view rotation, full
// resolution) for pasting into chats/documents, plus the file itself for
// pasting into Explorer.
void App::CopyToClipboard() {
    if (!hasImage_ || currentPath_.empty()) return;
    Microsoft::WRL::ComPtr<IWICImagingFactory> wic;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&wic)))) {
        return;
    }
    std::shared_ptr<DecodedPixels> px = DecodeToPixels(wic.Get(), currentPath_, kMaxDecodedBytes, 0, 0);
    if (px && rotation_ != 0) {
        Microsoft::WRL::ComPtr<IWICBitmap> mem;
        Microsoft::WRL::ComPtr<IWICBitmapFlipRotator> rot;
        const WICBitmapTransformOptions opt = rotation_ == 1 ? WICBitmapTransformRotate90
                                            : rotation_ == 2 ? WICBitmapTransformRotate180
                                                             : WICBitmapTransformRotate270;
        if (SUCCEEDED(wic->CreateBitmapFromMemory(px->width, px->height, GUID_WICPixelFormat32bppPBGRA,
                                                  px->stride, static_cast<UINT>(px->pixels.size()),
                                                  px->pixels.data(), &mem)) &&
            SUCCEEDED(wic->CreateBitmapFlipRotator(&rot)) &&
            SUCCEEDED(rot->Initialize(mem.Get(), opt))) {
            auto out = std::make_shared<DecodedPixels>();
            rot->GetSize(&out->width, &out->height);
            out->stride = out->width * 4;
            out->pixels.resize(static_cast<size_t>(out->stride) * out->height);
            if (SUCCEEDED(rot->CopyPixels(nullptr, out->stride, static_cast<UINT>(out->pixels.size()),
                                          out->pixels.data()))) {
                px = out;
            }
        }
    }
    if (!OpenClipboard(hwnd_)) return;
    EmptyClipboard();
    bool ok = false;
    if (px) {
        if (HGLOBAL dib = MakeDib(*px)) {
            if (SetClipboardData(CF_DIB, dib)) ok = true; else GlobalFree(dib);
        }
    }
    if (HGLOBAL drop = MakeHdrop(currentPath_)) {
        if (SetClipboardData(CF_HDROP, drop)) ok = true; else GlobalFree(drop);
    }
    CloseClipboard();
    Log(std::format(L"copy: {} {}", ok ? L"ok" : L"failed", px ? L"image+file" : L"file"));
    if (ok) ShowToast(Text(L"已复制", L"Copied"));
}

// --- animated GIF ------------------------------------------------------------

void App::StartAnimation() {
    StopAnimation();
    animFrame_ = 0;
    if (!current_ || current_->frames.size() < 2 || minimized_) return;
    SetTimer(hwnd_, kAnimTimer, current_->delaysMs[0], nullptr);
    animTimerRunning_ = true;
    Log(std::format(L"anim: frames={} first_delay_ms={}", current_->frames.size(), current_->delaysMs[0]));
}

void App::StopAnimation() {
    if (animTimerRunning_) KillTimer(hwnd_, kAnimTimer);
    animTimerRunning_ = false;
}

void App::OnAnimationTick() {
    if (!current_ || current_->frames.size() < 2 || minimized_) {
        StopAnimation();
        return;
    }
    animFrame_ = (animFrame_ + 1) % current_->frames.size();
    SetTimer(hwnd_, kAnimTimer, current_->delaysMs[animFrame_], nullptr);
    DrawNow();
}

// --- screen-size decode / zoom detail --------------------------------------

// Builds a render-ready image from worker pixels. width/height are the
// original (oriented) dimensions: all view math works in original-image
// pixels, and the renderer stretches the (possibly smaller) bitmap over them.
std::shared_ptr<DecodedImage> App::MakeImage(const DecodeResult& res, double* uploadMs) {
    const auto& px = res.pixels;
    if (!px) return nullptr;
    auto img = std::make_shared<DecodedImage>();
    img->path = res.path;
    img->width = px->fullWidth;
    img->height = px->fullHeight;
    img->estimateBytes = px->estimateBytes;
    const long long tUp = NowMicros();
    img->bitmap = renderer_->CreateBitmapFromPixels(px->width, px->height,
                                                    px->pixels.data(), px->stride);
    if (img->bitmap && !px->extraFrames.empty()) {
        img->frames.push_back(img->bitmap);
        for (const auto& f : px->extraFrames) {
            auto b = renderer_->CreateBitmapFromPixels(px->width, px->height, f.data(), px->stride);
            if (!b) break;
            img->frames.push_back(b);
        }
        img->delaysMs = px->delaysMs;
        if (img->frames.size() != img->delaysMs.size()) {
            img->frames.clear(); // incomplete upload: show it as a still image
            img->delaysMs.clear();
        }
    }
    if (uploadMs) *uploadMs = (NowMicros() - tUp) / 1000.0;
    return img->bitmap ? img : nullptr;
}

// Decode box = work area of the monitor the window is on (= the immersive
// window size), so the screen-size copy is drawn 1:1 in immersive mode and a
// little smaller in normal window mode; zooming past it fetches full detail.
void App::UpdateDecodeBox() {
    HMONITOR mon = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(mon, &mi)) return;
    const UINT w = static_cast<UINT>(mi.rcWork.right - mi.rcWork.left);
    const UINT h = static_cast<UINT>(mi.rcWork.bottom - mi.rcWork.top);
    if (w != boxW_ || h != boxH_) {
        boxW_ = w;
        boxH_ = h;
        Log(std::format(L"decode box {}x{}", boxW_, boxH_));
    }
}

// When the image is shown larger than its screen-size copy (zoom, 100%, a
// bigger monitor), fetch the full-resolution version once for the current
// image. Never competes with a navigation in flight.
void App::RequestDetailIfNeeded() {
    if (!hasImage_ || !current_ || !current_->bitmap || detail_) return;
    if (current_->frames.size() > 1) return;
    if (detailFailedPath_ == currentPath_) return;
    if (detailRequestId_ != 0 || targetIndex_ >= 0) return;
    const float bmpW = static_cast<float>(current_->bitmap->GetPixelSize().width);
    if (bmpW >= static_cast<float>(current_->width)) return; // already full resolution
    if (scale_ * static_cast<float>(current_->width) <= bmpW + 0.5f) return;
    detailRequestId_ = ++nextRequestId_;
    decoder_->RequestDecode(detailRequestId_, currentPath_, 0, 0);
    Log(std::format(L"detail: request id={} {}", detailRequestId_, currentPath_));
}

void App::OnDetailDone(const std::shared_ptr<DecodeResult>& res) {
    detailRequestId_ = 0;
    if (res->failed && res->path == currentPath_) detailFailedPath_ = res->path;
    if (res->failed || res->path != currentPath_ || !current_) {
        Log(std::format(L"detail: unavailable hr=0x{:08X} {}", static_cast<unsigned>(res->hr),
                        res->path));
        return; // keep showing the screen-size copy
    }
    double uploadMs = 0.0;
    auto img = MakeImage(*res, &uploadMs);
    if (!img) {
        detailFailedPath_ = res->path; // e.g. larger than the GPU's maximum bitmap size
        return;
    }
    detail_ = img;
    Log(std::format(L"detail: done decode_ms={:.1f} upload_ms={:.1f}",
                    res->decodeMicros / 1000.0, uploadMs));
    DrawNow();
}

bool App::DecodeFailedBefore(const std::wstring& path) const {
    return std::find(decodeFailed_.begin(), decodeFailed_.end(), path) != decodeFailed_.end();
}

void App::RememberDecodeFailure(const std::wstring& path) {
    if (DecodeFailedBefore(path)) return;
    if (decodeFailed_.size() >= kMaxThumbFailed) decodeFailed_.erase(decodeFailed_.begin());
    decodeFailed_.push_back(path);
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
