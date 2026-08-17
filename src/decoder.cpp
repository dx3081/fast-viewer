#include "decoder.h"

#include <chrono>

namespace {

constexpr uint64_t kPreloadMaxBytes = 128ULL * 1024 * 1024; // speculative cap: 128 MB estimated
constexpr uint64_t kTotalDecodedBudget = 512ULL * 1024 * 1024;
constexpr uint64_t kTempHeadroom = 64ULL * 1024 * 1024;

uint64_t NowMicros() {
    static LARGE_INTEGER freq{};
    if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    return static_cast<uint64_t>(now.QuadPart * 1000000LL / freq.QuadPart);
}

Microsoft::WRL::ComPtr<IWICImagingFactory> CreateWicFactory() {
    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                     IID_PPV_ARGS(&factory));
    return factory;
}

} // namespace

void ImageDecoder::Start(HWND hwnd) {
    hwnd_ = hwnd;
    if (!threadDecode_.joinable()) {
        threadDecode_ = std::thread([this] { DecodeWorkerMain(); });
    }
    if (!threadPreload_.joinable()) {
        threadPreload_ = std::thread([this] { PreloadWorkerMain(); });
    }
}

void ImageDecoder::Stop() {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (stop_) return;
        stop_ = true;
        pendingDecode_.reset();
        pendingPreload_.reset();
    }
    cvDecode_.notify_all();
    cvPreload_.notify_all();
    if (threadDecode_.joinable()) threadDecode_.join();
    if (threadPreload_.joinable()) threadPreload_.join();
}

void ImageDecoder::RequestDecode(uint64_t id, std::wstring path) {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        pendingDecode_ = DecodeJob{ id, std::move(path) };
    }
    cvDecode_.notify_one();
}

void ImageDecoder::RequestPreload(uint64_t id, std::wstring path,
                                  uint64_t cacheBytes, uint64_t currentImageBytes) {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        pendingPreload_ = PreloadJob{ id, std::move(path), cacheBytes, currentImageBytes };
    }
    cvPreload_.notify_one();
}

std::shared_ptr<DecodeResult> ImageDecoder::TakeResult(uint64_t id) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = results_.find(id);
    if (it == results_.end()) return nullptr;
    auto res = it->second;
    results_.erase(it);
    return res;
}

void ImageDecoder::DecodeWorkerMain() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    auto wic = CreateWicFactory();
    for (;;) {
        DecodeJob job;
        {
            std::unique_lock<std::mutex> lock(mtx_);
            cvDecode_.wait(lock, [this] { return stop_ || pendingDecode_.has_value(); });
            if (!pendingDecode_.has_value()) {
                if (stop_) break;
                continue;
            }
            job = std::move(*pendingDecode_);
            pendingDecode_.reset();
        }

        auto result = std::make_shared<DecodeResult>();
        result->id = job.id;
        result->isPreload = false;
        result->path = std::move(job.path);
        const uint64_t t0 = NowMicros();
        result->pixels = DecodeToPixels(wic.Get(), result->path, kMaxDecodedBytes,
                                        &result->hr, &result->budgetExceeded);
        result->failed = !result->pixels;
        result->decodeMicros = NowMicros() - t0;

        {
            std::lock_guard<std::mutex> lock(mtx_);
            ++completedDecodes_;
            results_[result->id] = result;
        }
        PostMessageW(hwnd_, kMsgDecodeDone, static_cast<WPARAM>(result->id), 0);
    }
    CoUninitialize();
}

void ImageDecoder::PreloadWorkerMain() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    auto wic = CreateWicFactory();
    for (;;) {
        PreloadJob job;
        {
            std::unique_lock<std::mutex> lock(mtx_);
            cvPreload_.wait(lock, [this] { return stop_ || pendingPreload_.has_value(); });
            if (!pendingPreload_.has_value()) {
                if (stop_) break;
                continue;
            }
            job = std::move(*pendingPreload_);
            pendingPreload_.reset();
        }

        preloadBusy_.store(true);
        auto result = std::make_shared<DecodeResult>();
        result->id = job.id;
        result->isPreload = true;
        result->path = std::move(job.path);

        // Memory-pressure gate: usable budget = total - current cache - current
        // image - headroom; speculative decodes additionally never exceed the
        // preload cap. Large images therefore reduce or disable preloading.
        uint64_t usable = kTotalDecodedBudget;
        usable = (job.cacheBytes < usable) ? usable - job.cacheBytes : 0;
        usable = (job.currentBytes < usable) ? usable - job.currentBytes : 0;
        usable = (usable > kTempHeadroom) ? usable - kTempHeadroom : 0;
        const uint64_t maxBytes = std::min(kPreloadMaxBytes, usable);

        const uint64_t t0 = NowMicros();
        result->pixels = DecodeToPixels(wic.Get(), result->path, maxBytes,
                                        &result->hr, &result->budgetExceeded);
        result->failed = !result->pixels;
        result->decodeMicros = NowMicros() - t0;

        {
            std::lock_guard<std::mutex> lock(mtx_);
            ++completedPreloads_;
            results_[result->id] = result;
        }
        PostMessageW(hwnd_, kMsgPreloadDone, static_cast<WPARAM>(result->id), 0);
        preloadBusy_.store(false);
    }
    CoUninitialize();
}
