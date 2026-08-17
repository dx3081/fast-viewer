#pragma once
#include <windows.h>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include "image_loader.h"

// Thumbnail completion notification (wParam = generation id).
inline constexpr UINT kMsgThumbDone = WM_APP + 4;

// Result of a thumbnail decode handed to the UI thread.
struct ThumbResult {
    uint64_t gen = 0;
    std::wstring path;
    bool failed = false;
    std::shared_ptr<DecodedPixels> pixels; // small EXIF-oriented thumbnail
};

// One low-priority thumbnail worker (THREAD_PRIORITY_LOWEST), latest-wins
// single-slot queue. Thumbnail work never blocks full-image decode/preload or
// the UI thread; stale generations are dropped by the UI.
class ThumbnailLoader {
public:
    ThumbnailLoader() = default;
    ~ThumbnailLoader() { Stop(); }

    void Start(HWND notifyHwnd);
    void Stop();

    // UI thread API. Replaces the pending job (latest-wins).
    void RequestThumb(uint64_t gen, std::wstring path, UINT maxDim);

    // UI thread: take the completed result for `gen` (erases the slot).
    std::shared_ptr<ThumbResult> TakeResult(uint64_t gen);

private:
    void WorkerMain();

    HWND hwnd_ = nullptr;
    std::mutex mtx_;
    std::condition_variable cv_;
    struct Job { uint64_t gen; std::wstring path; UINT maxDim; };
    std::optional<Job> pending_;
    bool stop_ = false;
    std::map<uint64_t, std::shared_ptr<ThumbResult>> results_;
    std::thread thread_;
    std::atomic<bool> busy_{ false };
};
