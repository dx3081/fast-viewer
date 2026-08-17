#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <string>
#include <vector>

// Screen-space placement of the image: image px -> screen px mapping.
struct ViewTransform {
    float scale = 1.0f;   // image pixels -> screen pixels
    float offsetX = 0.0f; // screen-space position of image origin
    float offsetY = 0.0f;
};

// A filmstrip thumbnail cell to draw (UI thread provides the bitmap).
struct ThumbCellDraw {
    int index = -1;
    D2D1_RECT_F rect{};
    ID2D1Bitmap* bitmap = nullptr; // nullptr = placeholder
    bool isCurrent = false;
};

// Filmstrip overlay description for one frame.
struct FilmstripDraw {
    bool visible = false;
    D2D1_RECT_F stripRect{};
    std::vector<ThumbCellDraw> cells;
    std::wstring infoText;
};

// Direct2D renderer: owns the hwnd render target and draws the image
// (or a simple failure text) with a dark background, plus the optional
// filmstrip overlay.
class Renderer {
public:
    bool Initialize(HWND hwnd);
    void Shutdown();
    void Resize(UINT widthPx, UINT heightPx);
    UINT Width() const { return width_; }
    UINT Height() const { return height_; }

    Microsoft::WRL::ComPtr<ID2D1Bitmap> CreateBitmapFromPixels(UINT width, UINT height,
                                                               const void* data, UINT stride);

    void Render(ID2D1Bitmap* image, const ViewTransform& view,
                bool hasError, const std::wstring& errorText,
                const FilmstripDraw& filmstrip);

private:
    void EnsureTarget();
    void EnsureText();
    void DrawFilmstrip(const FilmstripDraw& fs);

    HWND hwnd_ = nullptr;
    UINT width_ = 0;
    UINT height_ = 0;
    Microsoft::WRL::ComPtr<ID2D1Factory> factory_;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> target_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> textBrush_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> stripBgBrush_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> placeholderBrush_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> borderBrush_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> currentBorderBrush_;
    Microsoft::WRL::ComPtr<IDWriteFactory> dwriteFactory_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> textFormat_;
};
