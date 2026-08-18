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

    if (count <= 0 || currentIndex < 0) {
        return;
    }

    const float usable = static_cast<float>(clientW) - 2.0f * margin;
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
