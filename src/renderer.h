#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <string>

// Screen-space placement of the image: image px -> screen px mapping.
struct ViewTransform {
    float scale = 1.0f;   // image pixels -> screen pixels
    float offsetX = 0.0f; // screen-space position of image origin
    float offsetY = 0.0f;
};

// Direct2D renderer: owns the hwnd render target and draws the image
// (or a simple failure text) with a dark background.
class Renderer {
public:
    bool Initialize(HWND hwnd);
    void Shutdown();
    void Resize(UINT widthPx, UINT heightPx);
    UINT Width() const { return width_; }
    UINT Height() const { return height_; }

    Microsoft::WRL::ComPtr<ID2D1Bitmap> CreateBitmap(IWICBitmapSource* source,
                                                     HRESULT* outHr = nullptr);

    void Render(ID2D1Bitmap* image, const ViewTransform& view,
                bool hasError, const std::wstring& errorText);

private:
    void EnsureTarget();
    void EnsureText();

    HWND hwnd_ = nullptr;
    UINT width_ = 0;
    UINT height_ = 0;
    Microsoft::WRL::ComPtr<ID2D1Factory> factory_;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> target_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> textBrush_;
    Microsoft::WRL::ComPtr<IDWriteFactory> dwriteFactory_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> textFormat_;
};
