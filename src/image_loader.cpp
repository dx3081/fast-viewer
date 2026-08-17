#include "image_loader.h"

#include <propidl.h>
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

} // namespace

std::shared_ptr<DecodedPixels> DecodeToPixels(IWICImagingFactory* factory,
                                              const std::wstring& path,
                                              uint64_t maxBytes,
                                              HRESULT* outHr,
                                              bool* outBudgetExceeded) {
    if (outHr) *outHr = S_OK;
    if (outBudgetExceeded) *outBudgetExceeded = false;
    if (!factory) return nullptr;
    auto result = std::make_shared<DecodedPixels>();

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = factory->CreateDecoderFromFilename(
        path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr)) { if (outHr) *outHr = hr; return nullptr; }

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) { if (outHr) *outHr = hr; return nullptr; }

    UINT sw = 0, sh = 0;
    hr = frame->GetSize(&sw, &sh);
    if (FAILED(hr)) { if (outHr) *outHr = hr; return nullptr; }
    if (SizeRejected(sw, sh, maxBytes, outHr, outBudgetExceeded)) return nullptr;

    result->srcWidth = sw;
    result->srcHeight = sh;
    result->orientation = ReadOrientation(frame.Get());

    // Apply EXIF orientation through the WIC flip rotator.
    Microsoft::WRL::ComPtr<IWICBitmapSource> oriented;
    if (result->orientation != 1) {
        Microsoft::WRL::ComPtr<IWICBitmapFlipRotator> rotator;
        hr = factory->CreateBitmapFlipRotator(&rotator);
        if (FAILED(hr)) { if (outHr) *outHr = hr; return nullptr; }
        hr = rotator->Initialize(frame.Get(), OrientationTransform(result->orientation));
        if (FAILED(hr)) { if (outHr) *outHr = hr; return nullptr; }
        oriented = rotator;
    } else {
        oriented = frame;
    }

    // Canonical WIC -> Direct2D format: 32bpp premultiplied BGRA.
    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) { if (outHr) *outHr = hr; return nullptr; }
    hr = converter->Initialize(oriented.Get(), GUID_WICPixelFormat32bppPBGRA,
                               WICBitmapDitherTypeNone, nullptr, 0.0,
                               WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) { if (outHr) *outHr = hr; return nullptr; }

    UINT ow = 0, oh = 0;
    hr = converter->GetSize(&ow, &oh);
    if (FAILED(hr)) { if (outHr) *outHr = hr; return nullptr; }
    // Re-validate the oriented size before allocating the pixel buffer.
    if (SizeRejected(ow, oh, maxBytes, outHr, outBudgetExceeded)) return nullptr;

    result->width = ow;
    result->height = oh;
    result->stride = ow * 4;
    result->estimateBytes = static_cast<uint64_t>(ow) * oh * 4ULL;

    try {
        result->pixels.resize(static_cast<size_t>(result->estimateBytes));
    } catch (const std::bad_alloc&) {
        if (outHr) *outHr = E_OUTOFMEMORY;
        return nullptr;
    }

    hr = converter->CopyPixels(nullptr, result->stride,
                               static_cast<UINT>(result->pixels.size()),
                               result->pixels.data());
    if (FAILED(hr)) { if (outHr) *outHr = hr; return nullptr; }
    return result;
}

bool IsSupportedExtension(const std::wstring& path) {
    const size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos) return false;
    const std::wstring ext = ToLower(path.substr(dot + 1));
    return ext == L"jpg" || ext == L"jpeg" || ext == L"png" ||
           ext == L"bmp" || ext == L"tif" || ext == L"tiff";
}

std::shared_ptr<DecodedPixels> DecodeThumbnail(IWICImagingFactory* factory,
                                               const std::wstring& path, UINT maxDim) {
    if (!factory || maxDim == 0) return nullptr;

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = factory->CreateDecoderFromFilename(
        path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
    if (FAILED(hr)) return nullptr;

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) return nullptr;

    UINT sw = 0, sh = 0;
    hr = frame->GetSize(&sw, &sh);
    if (FAILED(hr) || sw == 0 || sh == 0 || sw > kMaxImageDimension || sh > kMaxImageDimension) {
        return nullptr;
    }

    // Apply EXIF orientation so thumbnails display upright.
    Microsoft::WRL::ComPtr<IWICBitmapSource> oriented;
    const int orientation = ReadOrientation(frame.Get());
    if (orientation != 1) {
        Microsoft::WRL::ComPtr<IWICBitmapFlipRotator> rotator;
        hr = factory->CreateBitmapFlipRotator(&rotator);
        if (FAILED(hr)) return nullptr;
        hr = rotator->Initialize(frame.Get(), OrientationTransform(orientation));
        if (FAILED(hr)) return nullptr;
        oriented = rotator;
        rotator->GetSize(&sw, &sh);
    } else {
        oriented = frame;
    }

    // Scale to fit maxDim x maxDim, preserving aspect ratio.
    UINT tw = sw, th = sh;
    if (sw > maxDim || sh > maxDim) {
        if (sw >= sh) {
            tw = maxDim;
            th = static_cast<UINT>(static_cast<uint64_t>(sh) * maxDim / sw);
        } else {
            th = maxDim;
            tw = static_cast<UINT>(static_cast<uint64_t>(sw) * maxDim / sh);
        }
        if (tw == 0) tw = 1;
        if (th == 0) th = 1;
    }

    Microsoft::WRL::ComPtr<IWICBitmapScaler> scaler;
    hr = factory->CreateBitmapScaler(&scaler);
    if (FAILED(hr)) return nullptr;
    hr = scaler->Initialize(oriented.Get(), tw, th, WICBitmapInterpolationModeFant);
    if (FAILED(hr)) return nullptr;

    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) return nullptr;
    hr = converter->Initialize(scaler.Get(), GUID_WICPixelFormat32bppPBGRA,
                               WICBitmapDitherTypeNone, nullptr, 0.0,
                               WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) return nullptr;

    auto result = std::make_shared<DecodedPixels>();
    result->width = tw;
    result->height = th;
    result->stride = tw * 4;
    result->srcWidth = sw;
    result->srcHeight = sh;
    result->orientation = orientation;
    result->estimateBytes = static_cast<uint64_t>(tw) * th * 4ULL;
    try {
        result->pixels.resize(static_cast<size_t>(result->estimateBytes));
    } catch (const std::bad_alloc&) {
        return nullptr;
    }
    hr = converter->CopyPixels(nullptr, result->stride,
                               static_cast<UINT>(result->pixels.size()),
                               result->pixels.data());
    if (FAILED(hr)) return nullptr;
    return result;
}
