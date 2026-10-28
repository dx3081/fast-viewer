#pragma once
#include <windows.h>
#include <d2d1.h>
#include <string>
#include <vector>

// A visible filmstrip thumbnail cell (screen-space).
struct ThumbCell {
    int index = -1;
    D2D1_RECT_F rect{};      // cell rect (screen px)
    bool isCurrent = false;
};

// Picasa-style control buttons in the bottom bar.
enum class BarAction { None, ZoomIn, ZoomOut, Toggle100, RotateCCW, RotateCW, Prev, Next };

struct BarButton {
    BarAction action = BarAction::None;
    D2D1_RECT_F rect{};
    const wchar_t* glyph = L"";
    bool iconFont = false; // Segoe MDL2 Assets glyph (else ordinary text)
};

// Filmstrip state + layout (UI thread only; rendering happens in Renderer).
// Constants are centralized here as future viewer.conf candidates.
// RC polish: smaller strip (68 px thumbnails), current cell visually centered.
class Filmstrip {
public:
    static constexpr float kHotZoneH = 24.0f;  // bottom activation zone, logical px
    static constexpr float kThumbH = 68.0f;    // thumbnail cell height, logical px
    static constexpr float kThumbW = 92.0f;    // thumbnail cell width, logical px
    static constexpr float kGap = 6.0f;
    static constexpr float kMargin = 12.0f;
    static constexpr float kPadding = 6.0f;    // strip padding above/below cells
    static constexpr UINT kHideDelayMs = 600;
    static constexpr int kMaxVisible = 11;
    static constexpr int kMinVisible = 5;
    static constexpr float kButtonW = 34.0f;     // control button, logical px
    static constexpr float kButtonH = 28.0f;
    static constexpr float kButtonGap = 6.0f;
    static constexpr float kArrowW = 28.0f;      // previous/next arrows beside the thumbnails
    static constexpr float kArrowH = 44.0f;
    static constexpr float kSideGroupsMinW = 900.0f; // narrower windows: thumbnails + arrows only

    void Update(UINT clientW, UINT clientH, float dpiScale, int count, int currentIndex);

    void Show() { visible_ = true; }
    void Hide() { visible_ = false; }
    bool Visible() const { return visible_; }

    bool IsOverStrip(POINT pt) const;
    // Returns the index under pt, or -1 when not over a thumbnail cell.
    int HitTest(POINT pt) const;

    int VisibleStart() const { return visibleStart_; }
    int VisibleCount() const { return static_cast<int>(cells_.size()); }
    int VisibleWidth() const { return visibleWidth_; }
    const std::vector<ThumbCell>& Cells() const { return cells_; }
    D2D1_RECT_F StripRect() const { return stripRect_; }
    int CurrentIndex() const { return currentIndex_; }
    const std::vector<BarButton>& Buttons() const { return buttons_; }
    BarAction HitButton(POINT pt) const;
    BarAction Hover() const { return hover_; }
    bool SetHover(BarAction a) { const bool changed = a != hover_; hover_ = a; return changed; }
    float DpiScale() const { return dpiScale_; }

private:
    bool visible_ = false;
    float dpiScale_ = 1.0f;
    int count_ = 0;
    int currentIndex_ = 0;
    int visibleStart_ = 0;   // logical first cell index (may be negative at edges)
    int visibleWidth_ = 0;   // logical row width (cells skipped at edges)
    D2D1_RECT_F stripRect_{};
    std::vector<ThumbCell> cells_;
    std::vector<BarButton> buttons_;
    BarAction hover_ = BarAction::None;
};
