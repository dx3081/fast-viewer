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
    }
}

void Renderer::Resize(UINT widthPx, UINT heightPx) {
    width_ = widthPx;
    height_ = heightPx;
    EnsureTarget();
}

Microsoft::WRL::ComPtr<ID2D1Bitmap> Renderer::CreateBitmap(IWICBitmapSource* source,
                                                           HRESULT* outHr) {
    Microsoft::WRL::ComPtr<ID2D1Bitmap> bitmap;
    if (outHr) *outHr = S_OK;
    if (!target_ || !source) {
        if (outHr) *outHr = E_FAIL;
        return bitmap;
    }
    const HRESULT hr = target_->CreateBitmapFromWicBitmap(source, nullptr, &bitmap);
    if (FAILED(hr)) {
        if (outHr) *outHr = hr;
        bitmap.Reset();
    }
    return bitmap;
}

void Renderer::Render(ID2D1Bitmap* image, const ViewTransform& view,
                      bool hasError, const std::wstring& errorText) {
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

    const HRESULT hr = target_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        target_.Reset();
        textBrush_.Reset();
        EnsureTarget();
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
