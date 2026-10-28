#pragma once
#include <windows.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <string>
#include <vector>

// Screen-space placement of the image: image px -> screen px mapping.
struct ViewTransform {
    float scale = 1.0f;   // image pixels -> screen pixels
    float offsetX = 0.0f; // screen-space position of image origin
    float offsetY = 0.0f;
    float imageW = 0.0f;  // original image size in image px; the bitmap may be a
    float imageH = 0.0f;  // smaller screen-size copy stretched over this rect
    int quarterTurns = 0; // view rotation, clockwise, 0..3 (offsets are for the rotated box)
};

// A filmstrip thumbnail cell to draw (UI thread provides the bitmap).
struct ThumbCellDraw {
    int index = -1;
    D2D1_RECT_F rect{};
    ID2D1Bitmap* bitmap = nullptr; // nullptr = placeholder
    bool isCurrent = false;
};

// A control button to draw in the bottom bar.
struct BarButtonDraw {
    D2D1_RECT_F rect{};
    const wchar_t* glyph = L"";
    bool iconFont = false;
    bool hovered = false;
};

// Filmstrip overlay description for one frame.
struct FilmstripDraw {
    bool visible = false;
    float dpi = 1.0f;          // DPI scale (text and line sizes)
    D2D1_RECT_F stripRect{};
    std::vector<ThumbCellDraw> cells;
    std::vector<BarButtonDraw> buttons;
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
                const FilmstripDraw& filmstrip, const std::wstring& toast = {});

private:
    void EnsureTarget();
    void EnsureText(float dpi);
    void DrawFilmstrip(const FilmstripDraw& fs);
    void ResetDeviceResources();
    float dpi_ = 1.0f;

    HWND hwnd_ = nullptr;
    UINT width_ = 0;
    UINT height_ = 0;
    Microsoft::WRL::ComPtr<ID2D1Factory> factory_;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> target_;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> dc_; // same target; high-quality scaling
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> textBrush_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> stripBgBrush_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> placeholderBrush_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> borderBrush_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> currentBorderBrush_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> buttonBrush_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> buttonHoverBrush_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> shadowBrush_;
    Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> barShadeBrush_;
    Microsoft::WRL::ComPtr<IDWriteFactory> dwriteFactory_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> textFormat_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> infoFormat_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> iconFormat_;   // Segoe MDL2 Assets
    Microsoft::WRL::ComPtr<IDWriteTextFormat> buttonFormat_; // button text labels ("1:1")
    Microsoft::WRL::ComPtr<IDWriteTextFormat> symbolFormat_; // single-symbol buttons (rotate)
    float textDpi_ = 0.0f;
};
