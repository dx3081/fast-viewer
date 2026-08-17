#pragma once
#include <windows.h>
#include <d2d1.h>
#include <wrl/client.h>
#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <string>

// A render-ready decoded image owned by the UI thread (Direct2D bitmap).
struct DecodedImage {
    std::wstring path;
    Microsoft::WRL::ComPtr<ID2D1Bitmap> bitmap;
    UINT width = 0;
    UINT height = 0;
    uint64_t estimateBytes = 0; // width * height * 4
};

// Small budget-based decoded-image cache (UI thread only).
// LRU ordering; the current image is pinned and never evicted.
// Used for both full images (256 MB) and thumbnails (24 MB) with a per-cache
// budget. Keys are file paths.
class ImageCache {
public:
    static constexpr uint64_t kSoftBudget = 256ULL * 1024 * 1024; // 256 MB decoded cache

    explicit ImageCache(uint64_t budget = kSoftBudget) : budget_(budget) {}

    void Insert(std::shared_ptr<DecodedImage> image, bool pinned);
    std::shared_ptr<DecodedImage> Get(const std::wstring& path); // promotes LRU; nullptr if absent
    bool Contains(const std::wstring& path) const;
    void UnpinAll();
    uint64_t EvictOld(); // evict oldest unpinned until bytes <= budget; returns bytes evicted
    void Clear();

    uint64_t Bytes() const { return bytes_; }
    uint64_t Entries() const { return map_.size(); }
    uint64_t Evictions() const { return evictions_; }
    uint64_t Budget() const { return budget_; }

private:
    struct Entry {
        std::shared_ptr<DecodedImage> image;
        uint64_t bytes = 0;
        bool pinned = false;
    };
    std::list<std::wstring> lru_; // front = most recently used
    std::map<std::wstring, Entry> map_;
    uint64_t budget_;
    uint64_t bytes_ = 0;
    uint64_t evictions_ = 0;
};
