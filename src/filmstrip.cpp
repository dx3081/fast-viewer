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

    stripRect_ = D2D1::RectF(0.0f, static_cast<FLOAT>(clientH) - stripH,
                             static_cast<FLOAT>(clientW), static_cast<FLOAT>(clientH));

    if (count <= 0 || currentIndex < 0) {
        return;
    }

    const float usable = static_cast<float>(clientW) - 2.0f * margin;
    int visible = static_cast<int>((usable + gap) / (cellW + gap));
    visible = std::clamp(visible, kMinVisible, kMaxVisible);
    visible = std::min(visible, count);

    // Center the current image; at directory ends the sequence aligns to the
    // available side (no fake empty slots).
    int start = currentIndex - (visible - 1) / 2;
    start = std::clamp(start, 0, std::max(0, count - visible));
    visibleStart_ = start;

    const float top = stripRect_.top + pad;
    for (int i = 0; i < visible; ++i) {
        ThumbCell cell;
        cell.index = start + i;
        const float x = margin + i * (cellW + gap);
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
