#pragma once
#include <windows.h>
#include <d2d1.h>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "cache.h"
#include "decoder.h"
#include "filmstrip.h"
#include "navigation.h"
#include "renderer.h"
#include "thumbnail.h"
#include "window.h"

// Exit-path instrumentation helper (declared here for main.cpp).
void DebugMark(const wchar_t* s);

// Owns the viewer behavior: window messages, view state (fit/100%/zoom/pan),
// asynchronous decode/cache/preload (M1), navigation and lightweight timing
// instrumentation. All window/Direct2D work happens on the UI thread; decode
// work happens on the ImageDecoder worker threads.
class App {
public:
    ~App();

    bool Initialize(HINSTANCE inst, const std::wstring& imagePath);
    int Run();

private:
    friend class Window;

    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    // image / view
    void ComputeFit();
    void ResetViewToFit();
    void ComputeTransform(ViewTransform& view) const;
    void ClampPan();
    void DrawNow();
    void LogView();

    // navigation / async decode
    void Navigate(int delta);
    void GoToIndex(int target);
    void OnScanComplete();
    void OnDecodeDone(uint64_t id);
    void OnPreloadDone(uint64_t id);
    void StartNavigation(const std::wstring& path, int target);
    void DisplayImage(const std::shared_ptr<DecodedImage>& img, int index,
                      bool navLog, uint64_t id, bool interim = false);
    void RequestUserDecode(const std::wstring& path, int index, bool navLog);
    void SchedulePreload();
    bool IsNearby(const std::wstring& path) const;
    void ShowFailure(const std::wstring& path, HRESULT hr, uint64_t decodeMicros);
    std::shared_ptr<DecodedImage> MakeImage(const DecodeResult& res, double* uploadMs);
    void UpdateDecodeBox();
    void RequestDetailIfNeeded();
    void OnDetailDone(const std::shared_ptr<DecodeResult>& res);
    void StartAnimation();
    void StopAnimation();
    void OnAnimationTick();
    bool DecodeFailedBefore(const std::wstring& path) const;
    void RememberDecodeFailure(const std::wstring& path);

    // filmstrip
    void TrackFilmstripPointer(POINT pt);
    void RevealFilmstrip();
    void HideFilmstrip();
    void ScheduleThumbs();
    void OnThumbDone(uint64_t gen);
    void BuildFilmstripDraw(FilmstripDraw& out);
    std::wstring BuildInfoText();
    float DpiScale() const;

    // actions (dispatched from the centralized input mapping)
    void Close();
    void ToggleMode();
    void Toggle100Percent();
    void Rotate(int quarterTurns);
    void CopyToClipboard();
    void ShowToast(const std::wstring& text);
    float EffW() const; // image size as shown (after view rotation)
    float EffH() const;
    void ZoomAt(POINT clientPt, int wheelDelta);
    void PanBegin(POINT clientPt);
    void PanMove(POINT clientPt);
    void PanEnd();

    // instrumentation
    void InitTimingLog();
    void Log(const std::wstring& line);
    double ElapsedMs() const;

    HINSTANCE inst_ = nullptr;
    HWND hwnd_ = nullptr;
    Window window_;
    std::unique_ptr<Renderer> renderer_;
    std::unique_ptr<ImageDecoder> decoder_;
    std::unique_ptr<ImageCache> cache_;
    std::unique_ptr<Navigation> nav_;
    std::unique_ptr<ImageCache> thumbCache_;
    std::unique_ptr<ThumbnailLoader> thumbLoader_;
    Filmstrip filmstrip_;
    uint64_t thumbGen_ = 0;
    bool hideTimerRunning_ = false;
    std::wstring lastGeomKey_;
    std::wstring lastInfoText_;
    std::vector<std::wstring> thumbFailed_; // bounded set: no infinite retry of broken files

    // displayed state
    std::shared_ptr<DecodedImage> current_; // screen-size copy (what the cache holds)
    std::shared_ptr<DecodedImage> detail_;  // full-resolution copy of current_, on zoom only
    uint64_t detailRequestId_ = 0;          // in-flight full-resolution decode (0 = none)
    size_t animFrame_ = 0;                  // animated GIF: frame on screen
    bool animTimerRunning_ = false;
    UINT boxW_ = 0;                         // screen-size decode box = monitor size, px
    UINT boxH_ = 0;
    std::vector<std::wstring> decodeFailed_; // bounded: never re-preload a broken file
    std::wstring currentPath_;
    bool hasImage_ = false;
    bool errorState_ = false;
    std::wstring errorText_;

    bool immersive_ = true;
    int rotation_ = 0;     // view-only rotation in clockwise quarter turns, reset per image
    std::wstring toast_;   // short on-screen confirmation ("Copied"), cleared by a timer
    enum class ViewMode { Fit, Percent100, Custom };
    ViewMode viewMode_ = ViewMode::Fit;
    float scale_ = 1.0f;
    float panX_ = 0.0f;
    float panY_ = 0.0f;
    float fitScale_ = 1.0f;
    bool panning_ = false;
    POINT panLast_{};
    bool suppressResizeRender_ = false; // defer draws during a mode switch

    // navigation / request state
    std::shared_ptr<Navigation::Result> navResult_;
    bool navReady_ = false;
    int pendingDelta_ = 0;
    int displayIndex_ = -1; // currently displayed image index
    int targetIndex_ = -1;  // index being decoded (latest user intent)
    uint64_t nextRequestId_ = 0;       // one globally-unique id source (user + preload)
    uint64_t latestUserRequestId_ = 0; // for stale-result detection
    uint64_t latestPreloadId_ = 0;     // for stale-preload detection
    uint64_t shownRequestId_ = 0;      // newest request whose image is on screen
    std::map<uint64_t, int> requestIndex_; // in-flight user decode id -> image index
    std::wstring preloadPathPending_;
    int lastNavDirection_ = 0;
    bool navLogPending_ = false;
    uint64_t requestT0Micros_ = 0;
    bool minimized_ = false;

    // timing
    long long t0Micros_ = 0;
    bool firstRenderLogged_ = false;
    std::wstring logPath_;
};
