#include "thumbnail.h"

#include <wincodec.h>
#include <wrl/client.h>

namespace {
Microsoft::WRL::ComPtr<IWICImagingFactory> CreateWicFactory() {
    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                     IID_PPV_ARGS(&factory));
    return factory;
}
} // namespace

void ThumbnailLoader::Start(HWND hwnd) {
    hwnd_ = hwnd;
    if (!thread_.joinable()) {
        thread_ = std::thread([this] { WorkerMain(); });
    }
}

void ThumbnailLoader::Stop() {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (stop_) return;
        stop_ = true;
        pending_.reset();
    }
    cv_.notify_all();
    if (thread_.joinable()) thread_.join();
}

void ThumbnailLoader::RequestThumb(uint64_t gen, std::wstring path, UINT maxDim) {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        pending_ = Job{ gen, std::move(path), maxDim };
    }
    cv_.notify_one();
}

std::shared_ptr<ThumbResult> ThumbnailLoader::TakeResult(uint64_t gen) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = results_.find(gen);
    if (it == results_.end()) return nullptr;
    auto res = it->second;
    results_.erase(it);
    return res;
}

void ThumbnailLoader::WorkerMain() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
    auto wic = CreateWicFactory();
    for (;;) {
        Job job;
        {
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [this] { return stop_ || pending_.has_value(); });
            if (!pending_.has_value()) {
                if (stop_) break;
                continue;
            }
            job = std::move(*pending_);
            pending_.reset();
        }
        busy_.store(true);
        auto result = std::make_shared<ThumbResult>();
        result->gen = job.gen;
        result->path = job.path;
        result->pixels = DecodeThumbnail(wic.Get(), job.path, job.maxDim);
        result->failed = !result->pixels;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            results_[job.gen] = result;
        }
        PostMessageW(hwnd_, kMsgThumbDone, static_cast<WPARAM>(job.gen), 0);
        busy_.store(false);
    }
    CoUninitialize();
}
