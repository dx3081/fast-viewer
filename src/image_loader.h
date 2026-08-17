#pragma once
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Immutable CPU-side decode result: 32bpp premultiplied BGRA pixels.
// Produced on a worker thread; consumed by the UI thread to create the
// Direct2D bitmap. Never mutated or shared across threads.
struct DecodedPixels {
    std::vector<BYTE> pixels;
    UINT width = 0;    // oriented dimensions, pixels
    UINT height = 0;
    UINT stride = 0;   // width * 4
    UINT srcWidth = 0; // pre-orientation dimensions
    UINT srcHeight = 0;
    int orientation = 1;
    uint64_t estimateBytes = 0; // width * height * 4
};

// M1 safety limits (per-image rule unchanged from M0.1):
// - Direct2D bitmaps are capped at 16384 px per side.
// - Decoded-memory budget: conservative overflow-safe estimate (w*h*4).
//   The real WIC path transiently holds the decoder frame + converter, so this
//   budget keeps full decodes well below the ~1 GB-class of working set.
constexpr UINT kMaxImageDimension = 16384;
constexpr UINT64 kMaxDecodedBytes = 512ULL * 1024 * 1024; // 512 MB per image

// Decodes `path` to raw 32bpp premultiplied BGRA pixels with EXIF orientation
// applied. `maxBytes` is the decoded-bytes ceiling for this request
// (kMaxDecodedBytes for user requests; a smaller preload budget otherwise).
// Returns nullptr on corrupt/unsupported/oversized/budget-exceeded failures;
// never throws. `outHr` receives the failing HRESULT and `outBudgetExceeded`
// reports a pre-allocation memory-budget rejection.
std::shared_ptr<DecodedPixels> DecodeToPixels(IWICImagingFactory* factory,
                                              const std::wstring& path,
                                              uint64_t maxBytes,
                                              HRESULT* outHr = nullptr,
                                              bool* outBudgetExceeded = nullptr);

// True if `path`'s extension is in the supported set (jpg/jpeg/png/bmp/tif/tiff).
bool IsSupportedExtension(const std::wstring& path);

// Decodes a small EXIF-oriented thumbnail that fits within `maxDim` x `maxDim`
// pixels, using the WIC scaler (no full-size allocation beyond the frame the
// codec decodes). Returns nullptr on failure; never throws.
std::shared_ptr<DecodedPixels> DecodeThumbnail(IWICImagingFactory* factory,
                                               const std::wstring& path, UINT maxDim);
