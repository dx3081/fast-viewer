#include "navigation.h"

#include <shlwapi.h>
#include <algorithm>

#include "image_loader.h"

namespace {

UINT64 NowMicros() {
    static LARGE_INTEGER freq{};
    if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    return static_cast<UINT64>(now.QuadPart * 1000000LL / freq.QuadPart);
}

std::wstring ParentDirectory(const std::wstring& path) {
    const size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return {};
    return path.substr(0, slash);
}

} // namespace

void Navigation::Start(HWND hwnd, UINT message, const std::wstring& imagePath) {
    if (running_.load()) return;

    const std::wstring parent = ParentDirectory(imagePath);
    if (parent.empty()) return;

    running_.store(true);
    thread_ = std::thread([this, hwnd, message, parent, imagePath]() {
        const UINT64 t0 = NowMicros();
        auto result = std::make_shared<Result>();

        const std::wstring pattern = parent + L"\\*";
        WIN32_FIND_DATAW fd{};
        HANDLE find = FindFirstFileW(pattern.c_str(), &fd);
        if (find != INVALID_HANDLE_VALUE) {
            do {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                std::wstring full = parent + L"\\" + fd.cFileName;
                if (IsSupportedExtension(full)) {
                    result->files.push_back(std::move(full));
                }
            } while (FindNextFileW(find, &fd));
            FindClose(find);
        }

        std::sort(result->files.begin(), result->files.end(),
                  [](const std::wstring& a, const std::wstring& b) {
                      return StrCmpLogicalW(a.c_str(), b.c_str()) < 0;
                  });

        // Locate the launched image (case-insensitive full path).
        for (size_t i = 0; i < result->files.size(); ++i) {
            if (lstrcmpiW(result->files[i].c_str(), imagePath.c_str()) == 0) {
                result->index = static_cast<int>(i);
                break;
            }
        }

        result->scanMicros = NowMicros() - t0;
        result_ = result;           // written before PostMessage -> safe ordering
        running_.store(false);
        PostMessageW(hwnd, message, 0, 0);
    });
}

void Navigation::Stop() {
    if (thread_.joinable()) thread_.join();
}
