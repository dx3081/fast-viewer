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

// M0 safety limits: Direct2D bitmaps are capped at 16384 px per side on
// common D3D feature levels, and we reject absurd total pixel counts before
// any large allocation is attempted.
constexpr UINT kMaxImageDimension = 16384;
constexpr UINT64 kMaxImagePixels = 200000000ULL; // ~200 megapixels

// Decodes `path` into a WIC source with EXIF orientation applied.
// Returns nullptr for corrupt / unsupported / oversized files. Never throws.
// `outHr` receives the failing HRESULT on failure (optional).
std::shared_ptr<DecodedSource> DecodeImage(IWICImagingFactory* factory,
                                           const std::wstring& path,
                                           HRESULT* outHr = nullptr);

// True if `path`'s extension is in the M0 supported set (jpg/jpeg/png/bmp/tif/tiff).
bool IsSupportedExtension(const std::wstring& path);
