#include "renderer.h"

#include <d2d1helper.h>

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
        target_.Reset();
        textBrush_.Reset();
        stripBgBrush_.Reset();
        placeholderBrush_.Reset();
        borderBrush_.Reset();
        currentBorderBrush_.Reset();
    }
    if (width_ == 0 || height_ == 0 || !hwnd_ || !factory_) return;

    // 96 DPI render target: 1 DIP == 1 physical pixel, so all view math is in pixels.
    const D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
        96.0f, 96.0f);

    HRESULT hr = factory_->CreateHwndRenderTarget(
        props, D2D1::HwndRenderTargetProperties(hwnd_, D2D1::SizeU(width_, height_)),
        &target_);
    if (SUCCEEDED(hr)) {
        target_->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::LightGray), &textBrush_);
        target_->CreateSolidColorBrush(D2D1::ColorF(0.11f, 0.11f, 0.13f, 0.96f), &stripBgBrush_);
        target_->CreateSolidColorBrush(D2D1::ColorF(0.20f, 0.20f, 0.23f, 1.0f), &placeholderBrush_);
        target_->CreateSolidColorBrush(D2D1::ColorF(0.42f, 0.42f, 0.46f, 1.0f), &borderBrush_);
        target_->CreateSolidColorBrush(D2D1::ColorF(0.93f, 0.93f, 0.95f, 1.0f), &currentBorderBrush_);
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
                      const FilmstripDraw& filmstrip) {
    if (!target_) return;

    target_->BeginDraw();
    target_->SetTransform(D2D1::Matrix3x2F::Identity());
    target_->Clear(D2D1::ColorF(0.07f, 0.07f, 0.08f, 1.0f)); // simple dark background

    if (image) {
        const D2D1_SIZE_F size = image->GetSize();
        const D2D1_MATRIX_3X2_F m =
            D2D1::Matrix3x2F::Scale(view.scale, view.scale) *
            D2D1::Matrix3x2F::Translation(view.offsetX, view.offsetY);
        target_->SetTransform(m);
        target_->DrawBitmap(image, D2D1::RectF(0, 0, size.width, size.height), 1.0f,
                            D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
        target_->SetTransform(D2D1::Matrix3x2F::Identity());
    }

    if (hasError) {
        EnsureText();
        const D2D1_RECT_F rc = D2D1::RectF(24.0f, 24.0f,
                                           static_cast<FLOAT>(width_) - 24.0f,
                                           static_cast<FLOAT>(height_) - 24.0f);
        target_->DrawTextW(errorText.c_str(), static_cast<UINT32>(errorText.size()),
                           textFormat_.Get(), rc, textBrush_.Get());
    }

    if (filmstrip.visible) {
        DrawFilmstrip(filmstrip);
    }

    const HRESULT hr = target_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        target_.Reset();
        textBrush_.Reset();
        stripBgBrush_.Reset();
        placeholderBrush_.Reset();
        borderBrush_.Reset();
        currentBorderBrush_.Reset();
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
    if (!stripBgBrush_ || !placeholderBrush_ || !borderBrush_ || !currentBorderBrush_) {
        return;
    }
    // Flat dark background + subtle top separator.
    target_->FillRectangle(fs.stripRect, stripBgBrush_.Get());
    target_->DrawLine(D2D1::Point2F(fs.stripRect.left, fs.stripRect.top),
                      D2D1::Point2F(fs.stripRect.right, fs.stripRect.top),
                      borderBrush_.Get(), 1.0f);

    for (const auto& cell : fs.cells) {
        if (cell.bitmap) {
            const D2D1_SIZE_F size = cell.bitmap->GetSize();
            target_->DrawBitmap(cell.bitmap, FitRect(cell.rect,
                                                     static_cast<UINT>(size.width),
                                                     static_cast<UINT>(size.height)),
                                1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
        } else {
            // Neutral placeholder while the thumbnail loads (or for failures).
            target_->FillRectangle(cell.rect, placeholderBrush_.Get());
        }
        target_->DrawRectangle(cell.rect,
                               cell.isCurrent ? currentBorderBrush_.Get() : borderBrush_.Get(),
                               cell.isCurrent ? 2.0f : 1.0f);
    }

    if (!fs.infoText.empty()) {
        EnsureText();
        const D2D1_RECT_F rc = D2D1::RectF(fs.stripRect.left + 12.0f,
                                           fs.stripRect.top - 24.0f,
                                           fs.stripRect.right - 12.0f,
                                           fs.stripRect.top - 2.0f);
        if (rc.top >= 0.0f) {
            target_->DrawTextW(fs.infoText.c_str(),
                               static_cast<UINT32>(fs.infoText.size()),
                               textFormat_.Get(), rc, textBrush_.Get());
        }
    }
}

void Renderer::EnsureText() {
    if (textFormat_ || !dwriteFactory_) return;
    dwriteFactory_->CreateTextFormat(
        L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"", &textFormat_);
    if (textFormat_) {
        textFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        textFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
}
