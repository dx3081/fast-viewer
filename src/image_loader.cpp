#include "image_loader.h"

#include <propidl.h>
#include <algorithm>
#include <cmath>
#include <cwctype>
#include <new>

namespace {

std::wstring ToLower(std::wstring s) {
    for (auto& c : s) c = static_cast<wchar_t>(std::towlower(c));
    return s;
}

// Reads EXIF orientation (1..8) from the frame metadata. Defaults to 1.
int ReadOrientation(IWICBitmapFrameDecode* frame) {
    Microsoft::WRL::ComPtr<IWICMetadataQueryReader> reader;
    if (FAILED(frame->GetMetadataQueryReader(&reader))) return 1;

    PROPVARIANT value;
    PropVariantInit(&value);
    int orientation = 1;
    HRESULT hr = reader->GetMetadataByName(L"System.Photo.Orientation", &value);
    if (SUCCEEDED(hr) && value.vt == VT_UI2 && value.uiVal >= 1 && value.uiVal <= 8) {
        orientation = value.uiVal;
    }
    PropVariantClear(&value);
    return orientation;
}

WICBitmapTransformOptions OrientationTransform(int orientation) {
    switch (orientation) {
        case 2: return WICBitmapTransformFlipHorizontal;
        case 3: return WICBitmapTransformRotate180;
        case 4: return WICBitmapTransformFlipVertical;
        case 5: return static_cast<WICBitmapTransformOptions>(
                    WICBitmapTransformRotate270 | WICBitmapTransformFlipHorizontal);
        case 6: return WICBitmapTransformRotate90;
        case 7: return static_cast<WICBitmapTransformOptions>(
                    WICBitmapTransformRotate90 | WICBitmapTransformFlipHorizontal);
        case 8: return WICBitmapTransformRotate270;
        default: return WICBitmapTransformRotate0;
    }
}

bool SizeRejected(UINT w, UINT h, uint64_t maxBytes, HRESULT* outHr,
                  bool* outBudgetExceeded) {
    if (w == 0 || h == 0 || w > kMaxImageDimension || h > kMaxImageDimension) {
        if (outHr) *outHr = HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
        return true;
    }
    const uint64_t pixels = static_cast<uint64_t>(w) * static_cast<uint64_t>(h);
    if (pixels / static_cast<uint64_t>(w) != static_cast<uint64_t>(h)) {
        if (outHr) *outHr = HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
        return true;
    }
    const uint64_t bytes = pixels * 4ULL;
    if (bytes > maxBytes) {
        if (outBudgetExceeded) *outBudgetExceeded = true;
        if (outHr) *outHr = HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
        return true;
    }
    return false;
}

// Asks the codec itself for a reduced-size decode (JPEG can decode at 1/2,
// 1/4 or 1/8 size directly from its DCT data, several times faster than a
// full decode). Returns the smallest native size that is still at least
// tw x th, or `frame` unchanged when the codec cannot scale natively.
Microsoft::WRL::ComPtr<IWICBitmapSource> NativeDownscale(IWICImagingFactory* factory,
                                                         IWICBitmapFrameDecode* frame,
                                                         UINT sw, UINT sh, UINT tw, UINT th) {
    Microsoft::WRL::ComPtr<IWICBitmapSource> original = frame;
    Microsoft::WRL::ComPtr<IWICBitmapSourceTransform> transform;
    if (FAILED(frame->QueryInterface(IID_PPV_ARGS(&transform)))) return original;

    UINT bestW = 0, bestH = 0;
    for (UINT div : { 8u, 4u, 2u }) {
        UINT w = (sw + div - 1) / div, h = (sh + div - 1) / div;
        if (w < tw || h < th) continue;
        if (FAILED(transform->GetClosestSize(&w, &h))) continue;
        if (w >= tw && h >= th && w < sw) { bestW = w; bestH = h; break; }
    }
    if (bestW == 0) return original;

    WICPixelFormatGUID fmt{};
    if (FAILED(frame->GetPixelFormat(&fmt)) || FAILED(transform->GetClosestPixelFormat(&fmt))) {
        return original;
    }
    Microsoft::WRL::ComPtr<IWICBitmap> bitmap;
    if (FAILED(factory->CreateBitmap(bestW, bestH, fmt, WICBitmapCacheOnLoad, &bitmap))) {
        return original;
    }
    {
        Microsoft::WRL::ComPtr<IWICBitmapLock> lock;
        WICRect all{ 0, 0, static_cast<INT>(bestW), static_cast<INT>(bestH) };
        if (FAILED(bitmap->Lock(&all, WICBitmapLockWrite, &lock))) return original;
        UINT stride = 0, size = 0;
        BYTE* data = nullptr;
        if (FAILED(lock->GetStride(&stride)) || FAILED(lock->GetDataPointer(&size, &data))) {
            return original;
        }
        if (FAILED(transform->CopyPixels(nullptr, bestW, bestH, &fmt,
                                         WICBitmapTransformRotate0, stride, size, data))) {
            return original;
        }
    }
    return bitmap;
}

// Camera and phone JPEGs carry a small preview (typically 160x120) in their
// EXIF data. When it is big enough for a filmstrip cell, use it: no image
// data has to be decoded at all.
std::shared_ptr<DecodedPixels> DecodeEmbeddedThumbnail(IWICImagingFactory* factory,
                                                       const std::wstring& path, UINT maxDim) {
    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                                  WICDecodeMetadataCacheOnDemand, &decoder))) {
        return nullptr;
    }
    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame))) return nullptr;
    Microsoft::WRL::ComPtr<IWICBitmapSource> thumb;
    if (FAILED(frame->GetThumbnail(&thumb)) || !thumb) return nullptr;

    UINT fw = 0, fh = 0, tw = 0, th = 0;
    if (FAILED(frame->GetSize(&fw, &fh)) || FAILED(thumb->GetSize(&tw, &th))) return nullptr;
    if (tw == 0 || th == 0 || std::max(tw, th) < maxDim) return nullptr; // too small: blurry
    // Reject previews whose shape does not match the photo (e.g. letterboxed).
    const double fa = static_cast<double>(fw) / fh, ta = static_cast<double>(tw) / th;
    if (std::abs(fa - ta) > 0.05 * fa) return nullptr;

    const int orientation = ReadOrientation(frame.Get());
    const bool swapsAxes = orientation >= 5;
    UINT ow = swapsAxes ? th : tw, oh = swapsAxes ? tw : th;
    const double sc = std::min(1.0, std::min(static_cast<double>(maxDim) / ow,
                                             static_cast<double>(maxDim) / oh));
    ow = std::max(1u, static_cast<UINT>(ow * sc + 0.5));
    oh = std::max(1u, static_cast<UINT>(oh * sc + 0.5));

    Microsoft::WRL::ComPtr<IWICBitmapScaler> scaler;
    if (FAILED(factory->CreateBitmapScaler(&scaler))) return nullptr;
    if (FAILED(scaler->Initialize(thumb.Get(), swapsAxes ? oh : ow, swapsAxes ? ow : oh,
                                  WICBitmapInterpolationModeFant))) {
        return nullptr;
    }
    Microsoft::WRL::ComPtr<IWICBitmapSource> source = scaler;
    if (orientation != 1) {
        Microsoft::WRL::ComPtr<IWICBitmap> copy;
        if (FAILED(factory->CreateBitmapFromSource(scaler.Get(), WICBitmapCacheOnLoad, &copy))) {
            return nullptr;
        }
        Microsoft::WRL::ComPtr<IWICBitmapFlipRotator> rotator;
        if (FAILED(factory->CreateBitmapFlipRotator(&rotator)) ||
            FAILED(rotator->Initialize(copy.Get(), OrientationTransform(orientation)))) {
            return nullptr;
        }
        source = rotator;
    }
    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    if (FAILED(factory->CreateFormatConverter(&converter)) ||
        FAILED(converter->Initialize(source.Get(), GUID_WICPixelFormat32bppPBGRA,
                                     WICBitmapDitherTypeNone, nullptr, 0.0,
                                     WICBitmapPaletteTypeCustom))) {
        return nullptr;
    }
    auto result = std::make_shared<DecodedPixels>();
    result->width = ow;
    result->height = oh;
    result->stride = ow * 4;
    result->fullWidth = swapsAxes ? fh : fw;
    result->fullHeight = swapsAxes ? fw : fh;
    result->orientation = orientation;
    result->estimateBytes = static_cast<uint64_t>(ow) * oh * 4ULL;
    result->pixels.resize(static_cast<size_t>(result->estimateBytes));
    if (FAILED(converter->CopyPixels(nullptr, result->stride,
                                     static_cast<UINT>(result->pixels.size()),
                                     result->pixels.data()))) {
        return nullptr;
    }
    return result;
}

} // namespace

std::shared_ptr<DecodedPixels> DecodeToPixels(IWICImagingFactory* factory,
                                              const std::wstring& path,
                                              uint64_t maxBytes,
                                              UINT boxW, UINT boxH,
                                              HRESULT* outHr,
                                              bool* outBudgetExceeded) {
    if (outHr) *outHr = S_OK;
    if (outBudgetExceeded) *outBudgetExceeded = false;
    if (!factory) return nullptr;
    auto result = std::make_shared<DecodedPixels>();
    auto fail = [&](HRESULT h) -> std::shared_ptr<DecodedPixels> {
        if (outHr) *outHr = h;
        return nullptr;
    };

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = factory->CreateDecoderFromFilename(
        path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr)) return fail(hr);

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) return fail(hr);

    UINT sw = 0, sh = 0;
    hr = frame->GetSize(&sw, &sh);
    if (FAILED(hr)) return fail(hr);
    if (sw == 0 || sh == 0 || sw > kMaxImageDimension || sh > kMaxImageDimension) {
        return fail(HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE));
    }

    result->srcWidth = sw;
    result->srcHeight = sh;
    result->orientation = ReadOrientation(frame.Get());
    const bool swapsAxes = result->orientation >= 5; // 5..8 rotate by 90/270
    result->fullWidth = swapsAxes ? sh : sw;
    result->fullHeight = swapsAxes ? sw : sh;

    // Output size (oriented): fit inside the box, never upscale.
    UINT ow = result->fullWidth, oh = result->fullHeight;
    const bool scaled = boxW > 0 && boxH > 0 && (ow > boxW || oh > boxH);
    if (scaled) {
        const double s = std::min(static_cast<double>(boxW) / ow,
                                  static_cast<double>(boxH) / oh);
        ow = std::max(1u, static_cast<UINT>(ow * s + 0.5));
        oh = std::max(1u, static_cast<UINT>(oh * s + 0.5));
    }
    // A rotated image is decoded into memory first and rotated there, so it
    // transiently needs two buffers.
    const uint64_t budget = (result->orientation != 1) ? maxBytes / 2 : maxBytes;
    if (SizeRejected(ow, oh, budget, outHr, outBudgetExceeded)) return nullptr;

    // Pipeline: frame -> [scaler] -> convert -> [in-memory copy -> flip/rotate].
    // Never put the flip/rotator directly on the decoder: a 90-degree rotation
    // reads its source column by column, which re-decodes the JPEG over and
    // over (measured: 2 s for a 12 MP portrait phone photo instead of 0.1 s).
    Microsoft::WRL::ComPtr<IWICBitmapSource> source = frame;
    if (scaled) {
        Microsoft::WRL::ComPtr<IWICBitmapScaler> scaler;
        hr = factory->CreateBitmapScaler(&scaler);
        if (FAILED(hr)) return fail(hr);
        const UINT tw = swapsAxes ? oh : ow; // scaler works in source orientation
        const UINT th = swapsAxes ? ow : oh;
        auto reduced = NativeDownscale(factory, frame.Get(), sw, sh, tw, th);
        hr = scaler->Initialize(reduced.Get(), tw, th, WICBitmapInterpolationModeHighQualityCubic);
        if (FAILED(hr)) return fail(hr);
        source = scaler;
    }

    // Canonical WIC -> Direct2D format: 32bpp premultiplied BGRA.
    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) return fail(hr);
    hr = converter->Initialize(source.Get(), GUID_WICPixelFormat32bppPBGRA,
                               WICBitmapDitherTypeNone, nullptr, 0.0,
                               WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) return fail(hr);
    source = converter;

    if (result->orientation != 1) {
        Microsoft::WRL::ComPtr<IWICBitmap> decodedCopy;
        hr = factory->CreateBitmapFromSource(converter.Get(), WICBitmapCacheOnLoad, &decodedCopy);
        if (FAILED(hr)) return fail(hr);
        Microsoft::WRL::ComPtr<IWICBitmapFlipRotator> rotator;
        hr = factory->CreateBitmapFlipRotator(&rotator);
        if (FAILED(hr)) return fail(hr);
        hr = rotator->Initialize(decodedCopy.Get(), OrientationTransform(result->orientation));
        if (FAILED(hr)) return fail(hr);
        source = rotator;
    }

    UINT cw = 0, ch = 0;
    hr = source->GetSize(&cw, &ch);
    if (FAILED(hr)) return fail(hr);
    // Re-validate the actual output size before allocating the pixel buffer.
    if (SizeRejected(cw, ch, budget, outHr, outBudgetExceeded)) return nullptr;

    result->width = cw;
    result->height = ch;
    result->stride = cw * 4;
    result->estimateBytes = static_cast<uint64_t>(cw) * ch * 4ULL;

    try {
        result->pixels.resize(static_cast<size_t>(result->estimateBytes));
    } catch (const std::bad_alloc&) {
        return fail(E_OUTOFMEMORY);
    }

    hr = source->CopyPixels(nullptr, result->stride,
                            static_cast<UINT>(result->pixels.size()),
                            result->pixels.data());
    if (FAILED(hr)) return fail(hr);
    return result;
}

bool IsSupportedExtension(const std::wstring& path) {
    const size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos) return false;
    const std::wstring ext = ToLower(path.substr(dot + 1));
    return ext == L"jpg" || ext == L"jpeg" || ext == L"png" ||
           ext == L"bmp" || ext == L"tif" || ext == L"tiff" ||
           ext == L"webp";
}

std::shared_ptr<DecodedPixels> DecodeThumbnail(IWICImagingFactory* factory,
                                               const std::wstring& path, UINT maxDim) {
    if (!factory || maxDim == 0) return nullptr;
    // Same pipeline as a screen-size decode with a tiny box: the codec decodes
    // at 1/8 size where it can (JPEG), so a 24 MP photo no longer needs a full
    // decode just to produce a 68 px filmstrip thumbnail.
    constexpr uint64_t kThumbMaxBytes = 4ULL * 1024 * 1024;
    if (auto embedded = DecodeEmbeddedThumbnail(factory, path, maxDim)) return embedded;
    return DecodeToPixels(factory, path, kThumbMaxBytes, maxDim, maxDim);
}
