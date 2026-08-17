#include "cache.h"

void ImageCache::Insert(std::shared_ptr<DecodedImage> image, bool pinned) {
    if (!image) return;
    const std::wstring& path = image->path;
    const uint64_t bytes = image->estimateBytes;

    auto it = map_.find(path);
    if (it != map_.end()) {
        bytes_ -= it->second.bytes;
        map_.erase(it);
        lru_.remove(path);
    }
    map_[path] = Entry{ std::move(image), bytes, pinned };
    lru_.push_front(path);
    bytes_ += bytes;
    // Note: eviction is done explicitly by callers (so they can log it).
}

std::shared_ptr<DecodedImage> ImageCache::Get(const std::wstring& path) {
    auto it = map_.find(path);
    if (it == map_.end()) return nullptr;
    lru_.remove(path);
    lru_.push_front(path);
    return it->second.image;
}

bool ImageCache::Contains(const std::wstring& path) const {
    return map_.find(path) != map_.end();
}

void ImageCache::UnpinAll() {
    for (auto& kv : map_) kv.second.pinned = false;
}

uint64_t ImageCache::EvictOld() {
    uint64_t evictedBytes = 0;
    while (bytes_ > kSoftBudget && !lru_.empty()) {
        std::wstring victim;
        for (auto it = lru_.rbegin(); it != lru_.rend(); ++it) {
            auto mit = map_.find(*it);
            if (mit != map_.end() && !mit->second.pinned) {
                victim = *it;
                break;
            }
        }
        if (victim.empty()) break; // everything left is pinned (current protected)
        auto mit = map_.find(victim);
        evictedBytes += mit->second.bytes;
        bytes_ -= mit->second.bytes;
        ++evictions_;
        map_.erase(mit);
        lru_.remove(victim);
    }
    return evictedBytes;
}

void ImageCache::Clear() {
    map_.clear();
    lru_.clear();
    bytes_ = 0;
}
