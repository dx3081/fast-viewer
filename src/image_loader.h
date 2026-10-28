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
    UINT fullWidth = 0;  // oriented dimensions of the original image; equal to
    UINT fullHeight = 0; // width/height unless the decode was scaled down
    int orientation = 1;
    uint64_t estimateBytes = 0; // width * height * 4 (all frames for animations)
    // Animated GIF: frames 1..n-1 (frame 0 is `pixels`), fully composed, same
    // size and stride as frame 0; delaysMs has one entry per frame.
    std::vector<std::vector<BYTE>> extraFrames;
    std::vector<UINT> delaysMs;
};

// M1 safety limits (per-image rule unchanged from M0.1):
// - Direct2D bitmaps are capped at 16384 px per side.
// - Decoded-memory budget: conservative overflow-safe estimate (w*h*4).
//   The real WIC path transiently holds the decoder frame + converter, so this
//   budget keeps full decodes well below the ~1 GB-class of working set.
constexpr UINT kMaxImageDimension = 16384;
constexpr UINT64 kMaxDecodedBytes = 512ULL * 1024 * 1024; // 512 MB per image

// Decodes `path` to raw 32bpp premultiplied BGRA pixels with EXIF orientation
// applied. `maxBytes` is the decoded-bytes ceiling for this request.
// `boxW`/`boxH` (both > 0) request a screen-size decode: an image larger than
// the box is scaled down (codec-native reduction + Fant filter) to fit inside it, which is far
// faster and smaller than a full decode; fullWidth/fullHeight still report the
// original size. Pass 0/0 for a full-resolution decode.
// Returns nullptr on corrupt/unsupported/oversized/budget-exceeded failures;
// never throws. `outHr` receives the failing HRESULT and `outBudgetExceeded`
// reports a pre-allocation memory-budget rejection.
std::shared_ptr<DecodedPixels> DecodeToPixels(IWICImagingFactory* factory,
                                              const std::wstring& path,
                                              uint64_t maxBytes,
                                              UINT boxW, UINT boxH,
                                              HRESULT* outHr = nullptr,
                                              bool* outBudgetExceeded = nullptr);

// True if `path`'s extension can be decoded on this machine. The set is asked
// from Windows Imaging Component once (every installed decoder: JPEG, PNG, GIF,
// BMP, TIFF, WebP, plus HEIC/AVIF/RAW when their Windows extensions are
// installed), so new system codecs work without a code change.
bool IsSupportedExtension(const std::wstring& path);

// The supported extensions, lowercase with dot (".jpg"), for diagnostics.
const std::vector<std::wstring>& SupportedExtensions();

// Decodes a small EXIF-oriented thumbnail that fits within `maxDim` x `maxDim`
// pixels (reduced-size codec decode + high-quality scaler). Returns nullptr on
// failure; never throws.
std::shared_ptr<DecodedPixels> DecodeThumbnail(IWICImagingFactory* factory,
                                               const std::wstring& path, UINT maxDim);
