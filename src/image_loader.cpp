#include "image_loader.h"

#include <propidl.h>
#include <cwctype>

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

} // namespace

std::shared_ptr<DecodedSource> DecodeImage(IWICImagingFactory* factory,
                                           const std::wstring& path,
                                           HRESULT* outHr) {
    if (outHr) *outHr = S_OK;
    if (!factory) return nullptr;
    auto result = std::make_shared<DecodedSource>();

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = factory->CreateDecoderFromFilename(
        path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr)) { if (outHr) *outHr = hr; return nullptr; }

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) { if (outHr) *outHr = hr; return nullptr; }

    UINT sw = 0, sh = 0;
    hr = frame->GetSize(&sw, &sh);
    if (FAILED(hr) || sw == 0 || sh == 0) { if (outHr) *outHr = hr; return nullptr; }

    // Huge-image safety: reject before any allocation is attempted.
    const UINT64 pixels = static_cast<UINT64>(sw) * static_cast<UINT64>(sh);
    if (sw > kMaxImageDimension || sh > kMaxImageDimension || pixels > kMaxImagePixels) {
        if (outHr) *outHr = HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
        return nullptr;
    }

    result->srcWidth = sw;
    result->srcHeight = sh;
    result->orientation = ReadOrientation(frame.Get());

    if (result->orientation != 1) {
        Microsoft::WRL::ComPtr<IWICBitmapFlipRotator> rotator;
        hr = factory->CreateBitmapFlipRotator(&rotator);
        if (FAILED(hr)) { if (outHr) *outHr = hr; return nullptr; }
        hr = rotator->Initialize(frame.Get(), OrientationTransform(result->orientation));
        if (FAILED(hr)) { if (outHr) *outHr = hr; return nullptr; }
        result->source = rotator;
        rotator->GetSize(&result->width, &result->height);
    } else {
        result->source = frame;
        result->width = sw;
        result->height = sh;
    }

    // Direct2D requires a supported pixel format: convert to 32bpp
    // premultiplied BGRA (the canonical WIC -> Direct2D pipeline).
    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) { if (outHr) *outHr = hr; return nullptr; }
    hr = converter->Initialize(result->source.Get(), GUID_WICPixelFormat32bppPBGRA,
                               WICBitmapDitherTypeNone, nullptr, 0.0,
                               WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) { if (outHr) *outHr = hr; return nullptr; }
    result->source = converter;
    return result;
}

bool IsSupportedExtension(const std::wstring& path) {
    const size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos) return false;
    const std::wstring ext = ToLower(path.substr(dot + 1));
    return ext == L"jpg" || ext == L"jpeg" || ext == L"png" ||
           ext == L"bmp" || ext == L"tif" || ext == L"tiff";
}
