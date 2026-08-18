#pragma once
#include <windows.h>
#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// Asynchronous scan of the direct parent directory (no recursion), natural
// filename ordering, restricted to the supported extensions
// (jpg/jpeg/png/bmp/tif/tiff/webp).
class Navigation {
public:
    struct Result {
        std::vector<std::wstring> files; // full paths, natural sorted
        int index = -1;                  // position of the launched image
        UINT64 scanMicros = 0;
    };

    ~Navigation() { Stop(); }

    // Starts a background scan. When finished, PostMessage(hwnd, message, 0, 0)
    // is sent; the result is then available via TakeResult().
    void Start(HWND hwnd, UINT message, const std::wstring& imagePath);

    // Valid only after the notify message has been received.
    std::shared_ptr<Result> TakeResult() { return result_; }

    void Stop();

private:
    std::thread thread_;
    std::atomic<bool> running_{ false };
    std::shared_ptr<Result> result_;
};
