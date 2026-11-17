#pragma once

#include "cards.h"
#include "context.h"
#include "math_utils.h"

inline void FitAllCards() {
    if (context.cards.empty()) {
        CenterOnPoint(context.camera, Vec2{ScreenCenterX(), ScreenCenterY()});
        return;
    }
    RectF bounds{};
    for (const Card& card : context.cards) {
        bounds = UnionRect(bounds, card.world);
    }
    const float width = static_cast<float>(std::max(VirtualScreenWidth(), 1));
    const float height = static_cast<float>(std::max(VirtualScreenHeight(), 1));
    const float zoom = std::min((width * 0.8f) / std::max(bounds.width(), 1.0f),
                                (height * 0.8f) / std::max(bounds.height(), 1.0f));
    context.camera.targetZoom = ClampZoom(zoom);
    CenterOnPoint(context.camera, Vec2{bounds.centerX(), bounds.centerY()});
}

inline void CenterOnCard(int index) {
    if (index < 0 || static_cast<size_t>(index) >= context.cards.size()) {
        return;
    }
    const RectF& world = context.cards[static_cast<size_t>(index)].world;
    CenterOnPoint(context.camera, Vec2{world.centerX(), world.centerY()});
}

inline Camera DiveCameraForCard(const Card& card) {
    Camera target = context.camera;
    target.targetZoom = 1.0f;
    const float frameCenterX =
        static_cast<float>(card.framePx.left - context.virtualScreen.left) +
        static_cast<float>(card.framePx.right - card.framePx.left) * 0.5f;
    const float frameCenterY =
        static_cast<float>(card.framePx.top - context.virtualScreen.top) +
        static_cast<float>(card.framePx.bottom - card.framePx.top) * 0.5f;
    target.targetCenterX = card.world.centerX() - frameCenterX + ScreenCenterX();
    target.targetCenterY = card.world.centerY() - frameCenterY + ScreenCenterY();
    return target;
}
