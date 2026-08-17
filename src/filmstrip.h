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

// Filmstrip state + layout (UI thread only; rendering happens in Renderer).
// Constants are centralized here as future viewer.conf candidates.
class Filmstrip {
public:
    static constexpr float kHotZoneH = 24.0f;  // bottom activation zone, logical px
    static constexpr float kThumbH = 96.0f;    // thumbnail cell height, logical px
    static constexpr float kThumbW = 120.0f;   // thumbnail cell width, logical px
    static constexpr float kGap = 8.0f;
    static constexpr float kMargin = 16.0f;
    static constexpr float kPadding = 8.0f;    // strip padding above/below cells
    static constexpr UINT kHideDelayMs = 600;
    static constexpr int kMaxVisible = 11;
    static constexpr int kMinVisible = 5;

    void Update(UINT clientW, UINT clientH, float dpiScale, int count, int currentIndex);

    void Show() { visible_ = true; }
    void Hide() { visible_ = false; }
    bool Visible() const { return visible_; }

    bool IsOverStrip(POINT pt) const;
    // Returns the index under pt, or -1 when not over a thumbnail cell.
    int HitTest(POINT pt) const;

    int VisibleStart() const { return visibleStart_; }
    int VisibleCount() const { return static_cast<int>(cells_.size()); }
    const std::vector<ThumbCell>& Cells() const { return cells_; }
    D2D1_RECT_F StripRect() const { return stripRect_; }
    int CurrentIndex() const { return currentIndex_; }
    float DpiScale() const { return dpiScale_; }

private:
    bool visible_ = false;
    float dpiScale_ = 1.0f;
    int count_ = 0;
    int currentIndex_ = 0;
    int visibleStart_ = 0;
    D2D1_RECT_F stripRect_{};
    std::vector<ThumbCell> cells_;
};
