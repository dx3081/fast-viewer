#include "image_loader.h"

#include <propidl.h>
#include <algorithm>
#include <cmath>
#include <mutex>
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

UINT ReadUShort(IWICMetadataQueryReader* reader, const wchar_t* name, UINT fallback) {
    PROPVARIANT v;
    PropVariantInit(&v);
    UINT out = fallback;
    if (SUCCEEDED(reader->GetMetadataByName(name, &v))) {
        if (v.vt == VT_UI2) out = v.uiVal;
        else if (v.vt == VT_UI1) out = v.bVal;
    }
    PropVariantClear(&v);
    return out;
}

// Decodes every frame of an animated GIF into full-canvas premultiplied BGRA
// images, applying each frame's position and disposal the way browsers do.
// Returns nullptr (caller falls back to a still first frame) if the
// animation would exceed `maxBytes`.
std::shared_ptr<DecodedPixels> DecodeGifAnimationImpl(IWICImagingFactory* factory,
                                                      IWICBitmapDecoder* decoder, UINT count,
                                                      uint64_t maxBytes) {
    Microsoft::WRL::ComPtr<IWICMetadataQueryReader> global;
    UINT cw = 0, ch = 0;
    if (SUCCEEDED(decoder->GetMetadataQueryReader(&global))) {
        cw = ReadUShort(global.Get(), L"/logscrdesc/Width", 0);
        ch = ReadUShort(global.Get(), L"/logscrdesc/Height", 0);
    }
    if (cw == 0 || ch == 0) {
        Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> f0;
        if (FAILED(decoder->GetFrame(0, &f0)) || FAILED(f0->GetSize(&cw, &ch))) return nullptr;
    }
    if (cw == 0 || ch == 0 || cw > kMaxImageDimension || ch > kMaxImageDimension) return nullptr;
    const uint64_t frameBytes = static_cast<uint64_t>(cw) * ch * 4ULL;
    if (frameBytes * count > maxBytes) return nullptr;

    const UINT stride = cw * 4;
    std::vector<BYTE> canvas(static_cast<size_t>(frameBytes), 0);
    std::vector<BYTE> saved;
    auto result = std::make_shared<DecodedPixels>();
    result->width = result->fullWidth = result->srcWidth = cw;
    result->height = result->fullHeight = result->srcHeight = ch;
    result->stride = stride;
    result->estimateBytes = frameBytes * count;

    for (UINT i = 0; i < count; ++i) {
        Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
        if (FAILED(decoder->GetFrame(i, &frame))) break;
        UINT left = 0, top = 0, delay = 10, disposal = 0;
        Microsoft::WRL::ComPtr<IWICMetadataQueryReader> meta;
        if (SUCCEEDED(frame->GetMetadataQueryReader(&meta))) {
            left = ReadUShort(meta.Get(), L"/imgdesc/Left", 0);
            top = ReadUShort(meta.Get(), L"/imgdesc/Top", 0);
            delay = ReadUShort(meta.Get(), L"/grctlext/Delay", 10);
            disposal = ReadUShort(meta.Get(), L"/grctlext/Disposal", 0);
        }
        UINT fw = 0, fh = 0;
        if (FAILED(frame->GetSize(&fw, &fh))) break;
        // A frame larger than the canvas is malformed (or hostile): stop here.
        if (fw == 0 || fh == 0 || fw > cw || fh > ch) break;
        Microsoft::WRL::ComPtr<IWICFormatConverter> conv;
        if (FAILED(factory->CreateFormatConverter(&conv)) ||
            FAILED(conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA,
                                    WICBitmapDitherTypeNone, nullptr, 0.0,
                                    WICBitmapPaletteTypeCustom))) {
            break;
        }
        std::vector<BYTE> px(static_cast<size_t>(fw) * fh * 4);
        if (FAILED(conv->CopyPixels(nullptr, fw * 4, static_cast<UINT>(px.size()), px.data()))) break;

        if (disposal == 3) saved = canvas; // restore-to-previous: remember state
        // Draw the frame: GIF transparency is all-or-nothing, so copy opaque pixels.
        for (UINT y = 0; y < fh && top + y < ch; ++y) {
            const BYTE* s = px.data() + static_cast<size_t>(y) * fw * 4;
            BYTE* d = canvas.data() + static_cast<size_t>(top + y) * stride + static_cast<size_t>(left) * 4;
            for (UINT x = 0; x < fw && left + x < cw; ++x, s += 4, d += 4) {
                if (s[3] != 0) { d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3]; }
            }
        }
        if (i == 0) result->pixels = canvas;
        else result->extraFrames.push_back(canvas);
        // Browsers treat 0-1 (x10 ms) delays as 100 ms.
        result->delaysMs.push_back(delay <= 1 ? 100u : delay * 10u);

        if (disposal == 2) { // restore to background: clear the frame's rectangle
            for (UINT y = 0; y < fh && top + y < ch; ++y) {
                BYTE* d = canvas.data() + static_cast<size_t>(top + y) * stride + static_cast<size_t>(left) * 4;
                const UINT n = std::min(fw, cw > left ? cw - left : 0u);
                std::fill(d, d + static_cast<size_t>(n) * 4, BYTE{ 0 });
            }
        } else if (disposal == 3 && !saved.empty()) {
            canvas.swap(saved);
        }
    }
    if (result->pixels.empty()) return nullptr;
    result->estimateBytes = frameBytes * (1 + result->extraFrames.size());
    result->delaysMs.resize(1 + result->extraFrames.size());
    return result;
}

std::shared_ptr<DecodedPixels> DecodeGifAnimation(IWICImagingFactory* factory,
                                                  IWICBitmapDecoder* decoder, UINT count,
                                                  uint64_t maxBytes) {
    try {
        return DecodeGifAnimationImpl(factory, decoder, count, maxBytes);
    } catch (const std::bad_alloc&) {
        return nullptr; // falls back to a still first frame
    }
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

    // Animated GIF: all frames (only for full-size decodes of a reasonable
    // size; otherwise it falls through to a still first frame).
    GUID container{};
    UINT frameCount = 0;
    if (SUCCEEDED(decoder->GetContainerFormat(&container)) &&
        container == GUID_ContainerFormatGif &&
        SUCCEEDED(decoder->GetFrameCount(&frameCount)) && frameCount > 1 &&
        !(boxW > 0 && boxH > 0 && boxW < 160)) { // not for filmstrip thumbnails
        constexpr uint64_t kMaxAnimationBytes = 256ULL * 1024 * 1024;
        if (auto anim = DecodeGifAnimation(factory, decoder.Get(), frameCount,
                                           std::min<uint64_t>(maxBytes, kMaxAnimationBytes))) {
            return anim;
        }
    }

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
        // Camera RAW files carry a camera-made JPEG preview: showing it is
        // instant, while developing the RAW data takes seconds. Use it for the
        // screen-size view when it is big enough and has the photo's shape;
        // zooming in still develops the real RAW (full-resolution decode).
        Microsoft::WRL::ComPtr<IWICBitmapSource> reduced;
        Microsoft::WRL::ComPtr<IWICBitmapSource> preview;
        UINT pw = 0, ph = 0;
        if (SUCCEEDED(decoder->GetPreview(&preview)) && preview &&
            SUCCEEDED(preview->GetSize(&pw, &ph)) && pw > 0 && ph > 0 &&
            pw * 4 >= tw * 3 && ph * 4 >= th * 3 &&
            std::abs(static_cast<double>(pw) / ph - static_cast<double>(sw) / sh) <
                0.05 * static_cast<double>(sw) / sh) {
            reduced = preview;
        } else {
            reduced = NativeDownscale(factory, frame.Get(), sw, sh, tw, th);
        }
        hr = scaler->Initialize(reduced.Get(), tw, th, WICBitmapInterpolationModeFant);
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

const std::vector<std::wstring>& SupportedExtensions() {
    static std::vector<std::wstring> exts;
    static std::once_flag once;
    std::call_once(once, [] {
        // Callable from any thread: initialize COM here if the caller has not.
        const HRESULT init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        { // COM objects must be released before CoUninitialize
        Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
        Microsoft::WRL::ComPtr<IEnumUnknown> en;
        if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                       IID_PPV_ARGS(&factory))) &&
            SUCCEEDED(factory->CreateComponentEnumerator(WICDecoder, WICComponentEnumerateDefault,
                                                         &en))) {
            Microsoft::WRL::ComPtr<IUnknown> item;
            ULONG fetched = 0;
            while (en->Next(1, &item, &fetched) == S_OK) {
                Microsoft::WRL::ComPtr<IWICBitmapCodecInfo> info;
                if (SUCCEEDED(item.As(&info))) {
                    UINT len = 0;
                    if (SUCCEEDED(info->GetFileExtensions(0, nullptr, &len)) && len > 1) {
                        std::wstring list(len, L'\0');
                        if (SUCCEEDED(info->GetFileExtensions(len, list.data(), &len))) {
                            list.resize(wcslen(list.c_str()));
                            size_t pos = 0;
                            while (pos < list.size()) {
                                size_t comma = list.find(L',', pos);
                                if (comma == std::wstring::npos) comma = list.size();
                                std::wstring e = ToLower(list.substr(pos, comma - pos));
                                if (!e.empty() && e[0] != L'.') e.insert(e.begin(), L'.');
                                if (e.size() > 1 &&
                                    std::find(exts.begin(), exts.end(), e) == exts.end()) {
                                    exts.push_back(e);
                                }
                                pos = comma + 1;
                            }
                        }
                    }
                }
                item.Reset();
            }
        }
        }
        // Safety net: the formats Fast Viewer has always supported.
        for (const wchar_t* e : { L".jpg", L".jpeg", L".png", L".bmp", L".tif", L".tiff",
                                  L".webp", L".gif" }) {
            if (std::find(exts.begin(), exts.end(), e) == exts.end()) exts.push_back(e);
        }
        if (SUCCEEDED(init)) CoUninitialize();
    });
    return exts;
}

bool IsSupportedExtension(const std::wstring& path) {
    const size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos) return false;
    const size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos && slash > dot) return false;
    const std::wstring ext = ToLower(path.substr(dot));
    const auto& exts = SupportedExtensions();
    return std::find(exts.begin(), exts.end(), ext) != exts.end();
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
