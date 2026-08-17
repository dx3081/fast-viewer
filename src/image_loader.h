#pragma once
#include <windows.h>
#include <d2d1.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <memory>
#include <string>

// Result of decoding an image file through WIC.
// `source` is the EXIF-oriented bitmap source, ready for Direct2D conversion.
struct DecodedSource {
    Microsoft::WRL::ComPtr<IWICBitmapSource> source;
    UINT width = 0;    // oriented dimensions, pixels
    UINT height = 0;
    UINT srcWidth = 0; // pre-orientation dimensions
    UINT srcHeight = 0;
    int orientation = 1;
};

// M0.1 safety limits:
// - Direct2D bitmaps are capped at 16384 px per side on common D3D feature levels.
// - Decoded-memory budget: conservative overflow-safe estimate of the decoded
//   32bpp BGRA buffer (width * height * 4). The actual WIC/Direct2D path can
//   transiently hold the decoder frame + format converter + D2D bitmap at once,
//   so this raw-estimate budget deliberately keeps full decodes well below the
//   ~1 GB-class of working set. 512 MB rejects e.g. 16000x12000 (~768 MB raw)
//   while accepting ordinary photos up to ~10000x8000 (~320 MB raw).
constexpr UINT kMaxImageDimension = 16384;
constexpr UINT64 kMaxDecodedBytes = 512ULL * 1024 * 1024; // 512 MB decoded estimate

// Decodes `path` into a WIC source with EXIF orientation applied.
// Returns nullptr for corrupt / unsupported / oversized files. Never throws.
// `outHr` receives the failing HRESULT on failure (optional).
std::shared_ptr<DecodedSource> DecodeImage(IWICImagingFactory* factory,
                                           const std::wstring& path,
                                           HRESULT* outHr = nullptr);

// True if `path`'s extension is in the M0 supported set (jpg/jpeg/png/bmp/tif/tiff).
bool IsSupportedExtension(const std::wstring& path);
