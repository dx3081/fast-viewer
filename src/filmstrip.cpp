#include "filmstrip.h"

#include <algorithm>

void Filmstrip::Update(UINT clientW, UINT clientH, float dpiScale, int count,
                       int currentIndex) {
    dpiScale_ = dpiScale;
    count_ = count;
    currentIndex_ = currentIndex;
    cells_.clear();

    const float cellW = kThumbW * dpiScale;
    const float cellH = kThumbH * dpiScale;
    const float gap = kGap * dpiScale;
    const float margin = kMargin * dpiScale;
    const float pad = kPadding * dpiScale;
    const float stripH = cellH + 2.0f * pad;
    const float pitch = cellW + gap;

    stripRect_ = D2D1::RectF(0.0f, static_cast<FLOAT>(clientH) - stripH,
                             static_cast<FLOAT>(clientW), static_cast<FLOAT>(clientH));

    buttons_.clear();
    const float bw = kButtonW * dpiScale, bh = kButtonH * dpiScale, bgap = kButtonGap * dpiScale;
    const float cy = (stripRect_.top + stripRect_.bottom) / 2.0f;
    const bool sideGroups = static_cast<float>(clientW) >= kSideGroupsMinW * dpiScale;
    if (sideGroups) {
        // Left: zoom in, zoom out, 1:1.  Right: rotate left, rotate right.
        float x = margin;
        auto add = [&](BarAction a, const wchar_t* g, bool icon) {
            buttons_.push_back({ a, D2D1::RectF(x, cy - bh / 2, x + bw, cy + bh / 2), g, icon });
            x += bw + bgap;
        };
        add(BarAction::ZoomIn, L"\xE8A3", true);
        add(BarAction::ZoomOut, L"\xE71F", true);
        add(BarAction::Toggle100, L"1:1", false);
        x = static_cast<float>(clientW) - margin - 2 * bw - bgap;
        add(BarAction::RotateCCW, L"\x21BA", false);
        add(BarAction::RotateCW, L"\x21BB", false);
    }

    if (count <= 0 || currentIndex < 0) {
        return;
    }

    const float aw = kArrowW * dpiScale, ah = kArrowH * dpiScale;
    const float sideW = (sideGroups ? 3 * (bw + bgap) : 0.0f) + aw + 2 * bgap;
    const float usable = static_cast<float>(clientW) - 2.0f * (margin + sideW);
    int visible = static_cast<int>((usable + gap) / pitch);
    visible = std::clamp(visible, kMinVisible, kMaxVisible);
    visible = std::min(visible, count);

    // RC centering rule (permanent center): the current image always occupies
    // the cell whose center aligns with the viewport's horizontal center.
    // Neighbors fill the row around it; where a neighbor would fall outside
    // the directory (index < 0 or >= count) the space is simply empty
    // filmstrip background — no fake cells, no placeholder boxes, no wrap.
    const int p = (visible - 1) / 2; // current position when centered
    const int logicalStart = currentIndex - p; // may be negative at the start
    visibleStart_ = logicalStart;
    visibleWidth_ = visible;
    const float rowX = static_cast<float>(clientW) / 2.0f - cellW / 2.0f -
                       p * pitch;

    const float top = stripRect_.top + pad;
    for (int i = 0; i < visible; ++i) {
        const int index = logicalStart + i;
        if (index < 0 || index >= count) continue; // empty edge space
        ThumbCell cell;
        cell.index = index;
        const float x = rowX + i * pitch;
        cell.rect = D2D1::RectF(x, top, x + cellW, top + cellH);
        cell.isCurrent = (cell.index == currentIndex);
        cells_.push_back(cell);
    }

    // Previous / next arrows just outside the (logical) thumbnail row.
    const float rowEnd = rowX + visible * pitch - gap;
    buttons_.push_back({ BarAction::Prev,
                         D2D1::RectF(rowX - 2 * bgap - aw, cy - ah / 2, rowX - 2 * bgap, cy + ah / 2),
                         L"\xE76B", true });
    buttons_.push_back({ BarAction::Next,
                         D2D1::RectF(rowEnd + 2 * bgap, cy - ah / 2, rowEnd + 2 * bgap + aw, cy + ah / 2),
                         L"\xE76C", true });
}

BarAction Filmstrip::HitButton(POINT pt) const {
    if (!visible_) return BarAction::None;
    const float x = static_cast<float>(pt.x), y = static_cast<float>(pt.y);
    for (const auto& b : buttons_) {
        if (x >= b.rect.left && x <= b.rect.right && y >= b.rect.top && y <= b.rect.bottom) {
            return b.action;
        }
    }
    return BarAction::None;
}

bool Filmstrip::IsOverStrip(POINT pt) const {
    if (!visible_) return false;
    const float x = static_cast<float>(pt.x);
    const float y = static_cast<float>(pt.y);
    return x >= stripRect_.left && x <= stripRect_.right &&
           y >= stripRect_.top && y <= stripRect_.bottom;
}

int Filmstrip::HitTest(POINT pt) const {
    if (!IsOverStrip(pt)) return -1;
    const float x = static_cast<float>(pt.x);
    const float y = static_cast<float>(pt.y);
    for (const auto& cell : cells_) {
        if (x >= cell.rect.left && x <= cell.rect.right &&
            y >= cell.rect.top && y <= cell.rect.bottom) {
            return cell.index;
        }
    }
    return -1;
}
