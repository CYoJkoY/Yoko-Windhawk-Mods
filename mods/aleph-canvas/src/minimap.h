#pragma once

#include "cards.h"
#include "context.h"
#include "math_utils.h"

inline RectF MinimapInnerRect(const RectF& minimap) {
    const float pad = kMinimapPaddingPx * ChromeScale();
    return RectF{minimap.left + pad, minimap.top + pad, minimap.right - pad,
                 minimap.bottom - pad};
}

inline RectF CanvasWorldBounds() {
    RectF bounds{};
    for (const Card& card : context.cards) {
        bounds = UnionRect(bounds, card.world);
    }
    if (bounds.empty()) {
        return RectF{-500.0f, -500.0f, 500.0f, 500.0f};
    }
    const float margin = 100.0f;
    return GrowRect(bounds, margin);
}

inline RectF CalculateMinimapRect() {
    const float scale = ChromeScale();
    const float size = static_cast<float>(context.settings.minimapSize) * scale;
    const float margin = kMinimapMarginPx * scale;
    const float width = std::min(size, static_cast<float>(VirtualScreenWidth()) - margin * 2.0f);
    const float height = std::min(size, static_cast<float>(VirtualScreenHeight()) - margin * 2.0f);
    return MakeRectF(static_cast<float>(VirtualScreenWidth()) - width - margin,
                     static_cast<float>(VirtualScreenHeight()) - height - margin,
                     width, height);
}

inline RectF MinimapWorldToScreen(const RectF& world, const RectF& minimap,
                                 const RectF& bounds) {
    const RectF inner = MinimapInnerRect(minimap);
    const float scale = std::min(inner.width() / bounds.width(),
                                 inner.height() / bounds.height());
    const float offsetX = inner.left + (inner.width() - bounds.width() * scale) * 0.5f;
    const float offsetY = inner.top + (inner.height() - bounds.height() * scale) * 0.5f;
    return RectF{offsetX + (world.left - bounds.left) * scale,
                 offsetY + (world.top - bounds.top) * scale,
                 offsetX + (world.right - bounds.left) * scale,
                 offsetY + (world.bottom - bounds.top) * scale};
}

// Clicking or dragging inside the minimap centres the camera on the world
// point under the pointer.
inline void PanCameraFromMinimap(Vec2 point) {
    const RectF bounds = CanvasWorldBounds();
    const RectF inner = MinimapInnerRect(context.minimapRect);
    if (inner.empty()) {
        return;
    }
    const float scale = std::min(inner.width() / bounds.width(),
                                 inner.height() / bounds.height());
    if (scale <= 0.0f) {
        return;
    }
    const float offsetX = inner.left + (inner.width() - bounds.width() * scale) * 0.5f;
    const float offsetY = inner.top + (inner.height() - bounds.height() * scale) * 0.5f;
    CenterOnPoint(context.camera,
                  Vec2{bounds.left + (point.x - offsetX) / scale,
                       bounds.top + (point.y - offsetY) / scale});
}
