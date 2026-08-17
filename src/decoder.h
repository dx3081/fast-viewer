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

// Completion notification messages posted to the UI thread.
inline constexpr UINT kMsgDecodeDone = WM_APP + 2;
inline constexpr UINT kMsgPreloadDone = WM_APP + 3;

// Result of a worker decode handed to the UI thread (immutable pixels).
struct DecodeResult {
    uint64_t id = 0;
    bool isPreload = false;
    std::wstring path;
    bool failed = false;
    bool budgetExceeded = false;
    HRESULT hr = S_OK;
    uint64_t decodeMicros = 0;
    std::shared_ptr<DecodedPixels> pixels;
};

// Small purpose-built async decoder (M1):
// - 1 decode worker (user requests, normal priority), latest-wins queue.
// - 1 preload worker (speculative, THREAD_PRIORITY_BELOW_NORMAL), latest-wins
//   queue with a memory-budget gate evaluated after reading dimensions.
// Workers own thread-local COM/WIC factories and produce immutable pixel
// buffers; results are handed to the UI thread via PostMessage + a keyed slot.
class ImageDecoder {
public:
    ImageDecoder() = default;
    ~ImageDecoder() { Stop(); }

    void Start(HWND notifyHwnd);
    void Stop();

    // UI thread API. Each call replaces the pending job (latest-wins, so the
    // queue can never grow unbounded).
    void RequestDecode(uint64_t id, std::wstring path);
    void RequestPreload(uint64_t id, std::wstring path,
                        uint64_t cacheBytes, uint64_t currentImageBytes);

    // UI thread: take a completed result by id (erases the slot).
    std::shared_ptr<DecodeResult> TakeResult(uint64_t id);

private:
    void DecodeWorkerMain();
    void PreloadWorkerMain();

    HWND hwnd_ = nullptr;
    std::mutex mtx_;
    std::condition_variable cvDecode_;
    std::condition_variable cvPreload_;
    struct DecodeJob { uint64_t id; std::wstring path; };
    struct PreloadJob { uint64_t id; std::wstring path; uint64_t cacheBytes; uint64_t currentBytes; };
    std::optional<DecodeJob> pendingDecode_;
    std::optional<PreloadJob> pendingPreload_;
    bool stop_ = false;
    std::map<uint64_t, std::shared_ptr<DecodeResult>> results_;
    std::thread threadDecode_;
    std::thread threadPreload_;
    std::atomic<bool> preloadBusy_{ false };
    uint64_t completedDecodes_ = 0;
    uint64_t completedPreloads_ = 0;
};
