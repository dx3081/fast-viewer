#include "renderer.h"

#include <d2d1helper.h>
#include <algorithm>
#include <cwchar>

bool Renderer::Initialize(HWND hwnd) {
    hwnd_ = hwnd;

    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                   IID_PPV_ARGS(&factory_));
    if (FAILED(hr)) return false;

    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                             reinterpret_cast<IUnknown**>(dwriteFactory_.GetAddressOf()));
    if (FAILED(hr)) return false;

    RECT rc{};
    GetClientRect(hwnd_, &rc);
    width_ = static_cast<UINT>(rc.right - rc.left);
    height_ = static_cast<UINT>(rc.bottom - rc.top);
    EnsureTarget();
    return target_ != nullptr;
}

void Renderer::Shutdown() {
    target_.Reset();
    textBrush_.Reset();
    textFormat_.Reset();
    dwriteFactory_.Reset();
    factory_.Reset();
    hwnd_ = nullptr;
}

void Renderer::EnsureTarget() {
    if (target_) {
        const D2D1_SIZE_U size = target_->GetPixelSize();
        if (size.width == width_ && size.height == height_) return;
        // Resize the existing HWND target in place instead of resetting and
        // recreating it. A fresh CreateHwndRenderTarget during a mode toggle
        // can leave the presented surface stale (the window then shows the
        // previous/intermediate frame with black borders until some unrelated
        // full repaint). Resize keeps the same target/surface; the caller
        // redraws immediately afterwards.
        const HRESULT hr = target_->Resize(D2D1::SizeU(width_, height_));
        if (hr == D2DERR_RECREATE_TARGET) {
            ResetDeviceResources();
            EnsureTarget();
        }
        return;
    }
    if (width_ == 0 || height_ == 0 || !hwnd_ || !factory_) return;

    // 96 DPI render target: 1 DIP == 1 physical pixel, so all view math is in pixels.
    const D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
        96.0f, 96.0f);

    // PRESENT_OPTIONS_IMMEDIATELY: do not block the UI thread waiting for the
    // display refresh. Measured on the dev machine: the default (vsync-wait)
    // present took 30-45 ms per frame, immediate takes < 1 ms. The window is
    // composed by DWM, so this does not cause tearing.
    HRESULT hr = factory_->CreateHwndRenderTarget(
        props, D2D1::HwndRenderTargetProperties(hwnd_, D2D1::SizeU(width_, height_), D2D1_PRESENT_OPTIONS_IMMEDIATELY),
        &target_);
    if (SUCCEEDED(hr)) {
        target_.As(&dc_); // Windows 8+: always available on Windows 10
        target_->CreateSolidColorBrush(D2D1::ColorF(0.94f, 0.94f, 0.95f, 1.0f), &textBrush_);
        target_->CreateSolidColorBrush(D2D1::ColorF(0.11f, 0.11f, 0.13f, 0.96f), &stripBgBrush_);
        target_->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.10f), &placeholderBrush_);
        target_->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.28f), &borderBrush_);
        target_->CreateSolidColorBrush(D2D1::ColorF(0.96f, 0.96f, 0.97f, 1.0f), &currentBorderBrush_);
        target_->CreateSolidColorBrush(D2D1::ColorF(0.10f, 0.10f, 0.12f, 0.72f), &buttonBrush_);
        target_->CreateSolidColorBrush(D2D1::ColorF(0.30f, 0.30f, 0.34f, 0.90f), &buttonHoverBrush_);
        target_->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.85f), &shadowBrush_);
        const D2D1_GRADIENT_STOP stops[] = {
            { 0.0f, D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f) },
            { 0.45f, D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.45f) },
            { 1.0f, D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.78f) } };
        Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> coll;
        if (SUCCEEDED(target_->CreateGradientStopCollection(stops, 3, &coll))) {
            target_->CreateLinearGradientBrush(
                D2D1::LinearGradientBrushProperties(D2D1::Point2F(0, 0), D2D1::Point2F(0, 1)),
                coll.Get(), &barShadeBrush_);
        }
    }
}

void Renderer::Resize(UINT widthPx, UINT heightPx) {
    width_ = widthPx;
    height_ = heightPx;
    EnsureTarget();
}

// Creates a Direct2D bitmap from an immutable 32bpp premultiplied BGRA buffer
// (worker output). UI thread only; the buffer is never shared for mutation.
Microsoft::WRL::ComPtr<ID2D1Bitmap> Renderer::CreateBitmapFromPixels(
    UINT width, UINT height, const void* data, UINT stride) {
    Microsoft::WRL::ComPtr<ID2D1Bitmap> bitmap;
    if (!target_ || !data || width == 0 || height == 0) return bitmap;
    const D2D1_BITMAP_PROPERTIES props = D2D1::BitmapProperties(
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f, 96.0f);
    if (FAILED(target_->CreateBitmap(D2D1::SizeU(width, height), data, stride, props,
                                     &bitmap))) {
        bitmap.Reset();
    }
    return bitmap;
}

void Renderer::Render(ID2D1Bitmap* image, const ViewTransform& view,
                      bool hasError, const std::wstring& errorText,
                      const FilmstripDraw& filmstrip, const std::wstring& toast) {
    if (!target_) return;

    target_->BeginDraw();
    target_->SetTransform(D2D1::Matrix3x2F::Identity());
    target_->Clear(D2D1::ColorF(0.07f, 0.07f, 0.08f, 1.0f)); // simple dark background

    if (image) {
        const D2D1_SIZE_F size = image->GetSize();
        const float w = view.imageW > 0.0f ? view.imageW : size.width;
        const float h = view.imageH > 0.0f ? view.imageH : size.height;
        // Rotate about the image center, then place the rotated box at the
        // origin; scale and offset then work on the rotated (on-screen) box.
        const int q = ((view.quarterTurns % 4) + 4) % 4;
        const float ew = (q % 2) ? h : w;
        const float eh = (q % 2) ? w : h;
        const D2D1_MATRIX_3X2_F m =
            D2D1::Matrix3x2F::Translation(-w / 2.0f, -h / 2.0f) *
            D2D1::Matrix3x2F::Rotation(90.0f * static_cast<float>(q)) *
            D2D1::Matrix3x2F::Translation(ew / 2.0f, eh / 2.0f) *
            D2D1::Matrix3x2F::Scale(view.scale, view.scale) *
            D2D1::Matrix3x2F::Translation(view.offsetX, view.offsetY);
        target_->SetTransform(m);
        const D2D1_RECT_F dst = D2D1::RectF(0, 0, w, h);
        // Bilinear filtering is fine down to about half size; below that it
        // aliases (jaggies, shimmer), so strong reductions of a full-resolution
        // bitmap use high-quality cubic. Screen-size copies are drawn at about
        // 1:1 and take the cheap path.
        const bool strongShrink = view.scale * w < size.width * 0.5f;
        if (dc_ && strongShrink) {
            dc_->DrawBitmap(image, &dst, 1.0f, D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC,
                            nullptr, nullptr);
        } else {
            target_->DrawBitmap(image, dst, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
        }
        target_->SetTransform(D2D1::Matrix3x2F::Identity());
    }

    if (hasError) {
        EnsureText(filmstrip.dpi);
        const D2D1_RECT_F rc = D2D1::RectF(24.0f, 24.0f,
                                           static_cast<FLOAT>(width_) - 24.0f,
                                           static_cast<FLOAT>(height_) - 24.0f);
        target_->DrawTextW(errorText.c_str(), static_cast<UINT32>(errorText.size()),
                           textFormat_.Get(), rc, textBrush_.Get());
    }

    if (filmstrip.visible) {
        DrawFilmstrip(filmstrip);
    }

    if (!toast.empty()) {
        const float d = filmstrip.dpi;
        EnsureText(d);
        // Small rounded label in the upper middle of the window.
        const float bw = 200.0f * d, bh = 40.0f * d;
        const float x = (static_cast<float>(width_) - bw) / 2.0f;
        const D2D1_ROUNDED_RECT box{ D2D1::RectF(x, 48.0f * d, x + bw, 48.0f * d + bh), 10.0f * d,
                                     10.0f * d };
        target_->FillRoundedRectangle(box, buttonBrush_.Get());
        target_->DrawTextW(toast.c_str(), static_cast<UINT32>(toast.size()), textFormat_.Get(),
                           box.rect, textBrush_.Get());
    }

    const HRESULT hr = target_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        ResetDeviceResources();
        EnsureTarget();
    }
}

namespace {
D2D1_RECT_F FitRect(const D2D1_RECT_F& cell, UINT iw, UINT ih) {
    const float cw = cell.right - cell.left;
    const float ch = cell.bottom - cell.top;
    const float s = std::min(cw / static_cast<float>(iw), ch / static_cast<float>(ih));
    const float w = static_cast<float>(iw) * s;
    const float h = static_cast<float>(ih) * s;
    const float x = cell.left + (cw - w) / 2.0f;
    const float y = cell.top + (ch - h) / 2.0f;
    return D2D1::RectF(x, y, x + w, y + h);
}
} // namespace

void Renderer::DrawFilmstrip(const FilmstripDraw& fs) {
    if (!placeholderBrush_ || !currentBorderBrush_ || !barShadeBrush_) {
        return;
    }
    const float d = fs.dpi;
    EnsureText(d);
    // Picasa-like: no solid bar. A soft shade rises from the bottom edge so the
    // controls stay readable while the photo shows through.
    const float shadeTop = fs.stripRect.top - 34.0f * d;
    barShadeBrush_->SetStartPoint(D2D1::Point2F(0.0f, shadeTop));
    barShadeBrush_->SetEndPoint(D2D1::Point2F(0.0f, fs.stripRect.bottom));
    target_->FillRectangle(D2D1::RectF(fs.stripRect.left, shadeTop, fs.stripRect.right,
                                       fs.stripRect.bottom),
                           barShadeBrush_.Get());

    for (const auto& cell : fs.cells) {
        // The current image's thumbnail is a little larger, with a white frame.
        const float grow = cell.isCurrent ? 5.0f * d : 0.0f;
        const D2D1_RECT_F r = D2D1::RectF(cell.rect.left - grow, cell.rect.top - grow,
                                          cell.rect.right + grow, cell.rect.bottom + grow);
        D2D1_RECT_F pic = r;
        if (cell.bitmap) {
            const D2D1_SIZE_F size = cell.bitmap->GetSize();
            pic = FitRect(r, static_cast<UINT>(size.width), static_cast<UINT>(size.height));
            target_->DrawBitmap(cell.bitmap, pic, cell.isCurrent ? 1.0f : 0.85f,
                                D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
        } else {
            target_->FillRoundedRectangle(D2D1::RoundedRect(r, 3.0f * d, 3.0f * d),
                                          placeholderBrush_.Get());
        }
        if (cell.isCurrent) {
            target_->DrawRectangle(pic, currentBorderBrush_.Get(), 2.0f * d);
        }
    }

    for (const auto& b : fs.buttons) {
        const float rad = (b.rect.bottom - b.rect.top) / 2.0f;
        const D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(b.rect, std::min(rad, 8.0f * d),
                                                       std::min(rad, 8.0f * d));
        target_->FillRoundedRectangle(rr, b.hovered ? buttonHoverBrush_.Get() : buttonBrush_.Get());
        target_->DrawRoundedRectangle(rr, borderBrush_.Get(), 1.0f);
        IDWriteTextFormat* f = b.iconFont ? iconFormat_.Get()
                             : (wcslen(b.glyph) == 1) ? symbolFormat_.Get()
                                                      : buttonFormat_.Get();
        if (f) {
            target_->DrawTextW(b.glyph, static_cast<UINT32>(wcslen(b.glyph)), f, b.rect,
                               textBrush_.Get());
        }
    }

    if (!fs.infoText.empty() && infoFormat_) {
        // Caption centered above the thumbnails, with a 1 px shadow so it reads
        // on bright photos too.
        const D2D1_RECT_F rc = D2D1::RectF(fs.stripRect.left + 8.0f * d,
                                           fs.stripRect.top - 26.0f * d,
                                           fs.stripRect.right - 8.0f * d,
                                           fs.stripRect.top - 4.0f * d);
        if (rc.top >= 0.0f) {
            const UINT32 n = static_cast<UINT32>(fs.infoText.size());
            const D2D1_RECT_F sh = D2D1::RectF(rc.left + d, rc.top + d, rc.right + d, rc.bottom + d);
            target_->DrawTextW(fs.infoText.c_str(), n, infoFormat_.Get(), sh, shadowBrush_.Get());
            target_->DrawTextW(fs.infoText.c_str(), n, infoFormat_.Get(), rc, textBrush_.Get());
        }
    }
}

void Renderer::ResetDeviceResources() {
    target_.Reset();
    dc_.Reset();
    textBrush_.Reset();
    stripBgBrush_.Reset();
    placeholderBrush_.Reset();
    borderBrush_.Reset();
    currentBorderBrush_.Reset();
    buttonBrush_.Reset();
    buttonHoverBrush_.Reset();
    shadowBrush_.Reset();
    barShadeBrush_.Reset();
}

void Renderer::EnsureText(float dpi) {
    if (!dwriteFactory_) return;
    if (textFormat_ && textDpi_ == dpi) return;
    textDpi_ = dpi;
    textFormat_.Reset();
    infoFormat_.Reset();
    iconFormat_.Reset();
    buttonFormat_.Reset();
    symbolFormat_.Reset();
    auto make = [&](const wchar_t* family, DWRITE_FONT_WEIGHT weight, float size,
                    DWRITE_TEXT_ALIGNMENT align, DWRITE_PARAGRAPH_ALIGNMENT para,
                    Microsoft::WRL::ComPtr<IDWriteTextFormat>& out) {
        dwriteFactory_->CreateTextFormat(family, nullptr, weight, DWRITE_FONT_STYLE_NORMAL,
                                         DWRITE_FONT_STRETCH_NORMAL, size * dpi, L"", &out);
        if (out) {
            out->SetTextAlignment(align);
            out->SetParagraphAlignment(para);
            out->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        }
    };
    make(L"Segoe UI", DWRITE_FONT_WEIGHT_NORMAL, 15.0f, DWRITE_TEXT_ALIGNMENT_CENTER,
         DWRITE_PARAGRAPH_ALIGNMENT_CENTER, textFormat_);
    if (textFormat_) textFormat_->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP); // error text wraps
    make(L"Segoe UI", DWRITE_FONT_WEIGHT_SEMI_BOLD, 13.0f, DWRITE_TEXT_ALIGNMENT_CENTER,
         DWRITE_PARAGRAPH_ALIGNMENT_CENTER, infoFormat_);
    make(L"Segoe MDL2 Assets", DWRITE_FONT_WEIGHT_NORMAL, 13.0f, DWRITE_TEXT_ALIGNMENT_CENTER,
         DWRITE_PARAGRAPH_ALIGNMENT_CENTER, iconFormat_);
    make(L"Segoe UI", DWRITE_FONT_WEIGHT_SEMI_BOLD, 13.0f, DWRITE_TEXT_ALIGNMENT_CENTER,
         DWRITE_PARAGRAPH_ALIGNMENT_CENTER, buttonFormat_);
    make(L"Segoe UI Symbol", DWRITE_FONT_WEIGHT_BOLD, 19.0f, DWRITE_TEXT_ALIGNMENT_CENTER,
         DWRITE_PARAGRAPH_ALIGNMENT_CENTER, symbolFormat_);
}
