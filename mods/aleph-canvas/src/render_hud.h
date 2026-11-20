#pragma once

#include "card_chrome.h"
#include "card_stacks.h"
#include "context.h"
#include "localization.h"
#include "minimap.h"
#include "resources.h"
#include "thumbnails.h"

inline void SetHudColor(uint32_t rgb, float alpha = 1.0f) {
    context.hudBrush->SetColor(ToColorF(rgb, alpha));
}

inline D2D1_ROUNDED_RECT RoundedRect(const RectF& rect, float radius) {
    const float fitted = std::max(
        0.0f, std::min(radius, std::min(rect.width(), rect.height()) * 0.5f));
    return D2D1::RoundedRect(D2D1::RectF(rect.left, rect.top, rect.right, rect.bottom),
                             fitted, fitted);
}

inline void DrawCardTitle(const Card& card, const RectF& rect, float scale) {
    const float padding = kCardTitlePaddingPx * scale;
    if (context.titleFormat == nullptr || rect.width() <= padding * 2.0f ||
        rect.empty()) {
        return;
    }
    const D2D1_RECT_F titleRect = D2D1::RectF(
        rect.left + padding, rect.top, rect.right - padding, rect.bottom);
    SetHudColor(context.settings.textColor);
    context.hudTarget->DrawTextW(card.title.c_str(),
                                 static_cast<UINT32>(card.title.size()),
                                 context.titleFormat, titleRect,
                                 context.hudBrush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

inline void DrawHeaderButtonBackdrop(const RectF& rect, bool highlighted,
                                     float scale) {
    if (rect.empty() || !highlighted) {
        return;
    }
    SetHudColor(context.settings.accentColor, 0.22f);
    context.hudTarget->FillRoundedRectangle(RoundedRect(rect, 4.0f * scale),
                                            context.hudBrush);
}

inline void DrawCloseGlyph(const RectF& rect, float scale) {
    if (rect.empty()) {
        return;
    }
    const float inset = rect.width() * 0.3f;
    SetHudColor(context.settings.textColor, 0.9f);
    context.hudTarget->DrawLine(
        D2D1::Point2F(rect.left + inset, rect.top + inset),
        D2D1::Point2F(rect.right - inset, rect.bottom - inset),
        context.hudBrush, 1.2f * scale);
    context.hudTarget->DrawLine(
        D2D1::Point2F(rect.right - inset, rect.top + inset),
        D2D1::Point2F(rect.left + inset, rect.bottom - inset),
        context.hudBrush, 1.2f * scale);
}

inline void DrawPinGlyph(const RectF& rect, bool pinned, float scale) {
    if (rect.empty()) {
        return;
    }
    const float centerX = rect.centerX();
    const float head = rect.top + rect.height() * 0.24f;
    const float shoulder = rect.top + rect.height() * 0.58f;
    const float needle = rect.top + rect.height() * 0.78f;
    const float halfWidth = rect.width() * 0.22f;
    const float stroke = (pinned ? 1.6f : 1.1f) * scale;
    SetHudColor(pinned ? context.settings.accentColor : context.settings.textColor,
                pinned ? 1.0f : 0.85f);
    context.hudTarget->DrawLine(D2D1::Point2F(centerX, head),
                                D2D1::Point2F(centerX - halfWidth, shoulder),
                                context.hudBrush, stroke);
    context.hudTarget->DrawLine(D2D1::Point2F(centerX, head),
                                D2D1::Point2F(centerX + halfWidth, shoulder),
                                context.hudBrush, stroke);
    context.hudTarget->DrawLine(
        D2D1::Point2F(centerX - halfWidth, shoulder),
        D2D1::Point2F(centerX + halfWidth, shoulder), context.hudBrush, stroke);
    context.hudTarget->DrawLine(D2D1::Point2F(centerX, shoulder),
                                D2D1::Point2F(centerX, needle),
                                context.hudBrush, stroke);
}

inline void DrawCardHeader(const Card& card, int index, const RectF& header,
                           float scale) {
    if (header.empty()) {
        return;
    }
    SetHudColor(context.settings.backgroundColor, 0.9f);
    context.hudTarget->FillRectangle(
        D2D1::RectF(header.left, header.top, header.right, header.bottom),
        context.hudBrush);

    const RectF pin = CardPinButtonRect(header, scale);
    const RectF close = CardCloseButtonRect(header, scale);
    RectF title = header;
    if (!pin.empty()) {
        title.right = pin.left;
    }
    DrawCardTitle(card, title, scale);

    const bool hovered = index == context.hoverCard;
    DrawHeaderButtonBackdrop(pin, hovered &&
                                      context.hoverHeaderAction ==
                                          CardHeaderAction::Pin,
                             scale);
    DrawHeaderButtonBackdrop(close, hovered &&
                                        context.hoverHeaderAction ==
                                            CardHeaderAction::Close,
                             scale);
    DrawPinGlyph(pin, card.pinned, scale);
    DrawCloseGlyph(close, scale);
}

inline RectF CardBadgeBounds(const RectF& body, float scale) {
    if (context.badgeFormat == nullptr) {
        return {};
    }
    const float padding = kCardTitlePaddingPx * scale;
    const float height = context.badgeFormat->GetFontSize() * 2.0f;
    if (body.width() <= padding * 2.0f ||
        body.height() <= height + padding * 2.0f) {
        return {};
    }
    return RectF{body.left + padding, body.bottom - padding - height,
                 body.right - padding, body.bottom - padding};
}

inline void DrawCardIcon(const Card& card, const RectF& area) {
    const float size = std::min(
        kCardIconMaxPx * ChromeScale(), std::min(area.width(), area.height()));
    if (area.empty() || size <= 0.0f) {
        return;
    }
    ID2D1Bitmap* icon =
        LoadIconBitmap(card.exePath, card.windowClass, card.hwnd);
    if (icon == nullptr) {
        return;
    }
    const float half = size * 0.5f;
    context.hudTarget->DrawBitmap(
        icon, D2D1::RectF(area.centerX() - half, area.centerY() - half,
                          area.centerX() + half, area.centerY() + half),
        0.9f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
}

[[nodiscard]] inline CanvasTextId CardBadgeTextId(WindowVisualState state) {
    switch (state) {
        case WindowVisualState::Minimized:
            return CanvasTextId::MinimizedBadge;
        case WindowVisualState::OtherDesktop:
            return CanvasTextId::OtherDesktopBadge;
        case WindowVisualState::Live:
            break;
    }
    return CanvasTextId::PreviewUnavailableBadge;
}

inline void DrawCardBadge(const Card& card, const RectF& rect) {
    if (rect.empty() || context.badgeFormat == nullptr) {
        return;
    }
    const CanvasTextId badgeId = CardBadgeTextId(card.state);
    SetHudColor(context.settings.mutedTextColor);
    context.hudTarget->DrawTextW(
        CanvasText(badgeId, context.settings.language),
        static_cast<UINT32>(
            CanvasTextLength(badgeId, context.settings.language)),
        context.badgeFormat,
        D2D1::RectF(rect.left, rect.top, rect.right, rect.bottom),
        context.hudBrush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

inline void DrawCardBody(const Card& card, const RectF& body, float scale) {
    if (body.empty() || (card.state == WindowVisualState::Live &&
                         card.thumbnail.valid() && card.thumbnailVisible)) {
        return;
    }
    SetHudColor(context.settings.cardFillColor);
    context.hudTarget->FillRoundedRectangle(
        RoundedRect(body, kCardCornerRadiusPx * scale), context.hudBrush);
    const RectF badge = CardBadgeBounds(body, scale);
    const float padding = kCardTitlePaddingPx * scale;
    RectF iconArea = GrowRect(body, -padding);
    if (!badge.empty()) {
        iconArea.bottom = badge.top - padding;
    }
    DrawCardIcon(card, iconArea);
    DrawCardBadge(card, badge);
}

inline void DrawCard(const Card& card, int index, float scale) {
    if (card.stack.opacity <= kCardOpacityEpsilon) {
        return;
    }
    const RectF rect = CardScreenBounds(card);
    if (rect.empty()) {
        return;
    }
    // Cull on the rotated/scaled quad, not the unrotated bounds: a fanned
    // stack member's corners extend past its own rectangle.
    const std::array<Vec2, 4> quad = CardScreenQuad(rect, card.stack);
    const RectF quadBounds = CardQuadBounds(quad);
    if (quadBounds.right < 0.0f || quadBounds.bottom < 0.0f ||
        quadBounds.left > static_cast<float>(VirtualScreenWidth()) ||
        quadBounds.top > static_cast<float>(VirtualScreenHeight())) {
        return;
    }
    const float radius = kCardCornerRadiusPx * scale;
    const bool emphasized = index == context.hoverCard || index == context.selectedCard;
    const RectF body = WorldToScreen(StackWorldRect(card), context.camera);
    const RectF header = CardHeaderRect(card);
    const bool transformed = card.stack.rotationRadians != 0.0f ||
                             card.stack.scale != 1.0f;
    D2D1_MATRIX_3X2_F savedTransform = D2D1::Matrix3x2F::Identity();
    if (transformed) {
        const Vec2 center{rect.centerX(), rect.centerY()};
        const float degrees = card.stack.rotationRadians *
                              (180.0f / 3.141592653589793f);
        context.hudTarget->GetTransform(&savedTransform);
        context.hudTarget->SetTransform(
            D2D1::Matrix3x2F::Scale(card.stack.scale, card.stack.scale,
                                     D2D1::Point2F(center.x, center.y)) *
            D2D1::Matrix3x2F::Rotation(degrees, D2D1::Point2F(center.x, center.y)));
    }
    D2D1_LAYER_PARAMETERS layerParameters = {};
    layerParameters.contentBounds = D2D1::InfiniteRect();
    layerParameters.maskAntialiasMode = D2D1_ANTIALIAS_MODE_PER_PRIMITIVE;
    layerParameters.maskTransform = D2D1::IdentityMatrix();
    layerParameters.opacity = card.stack.opacity;
    layerParameters.layerOptions = D2D1_LAYER_OPTIONS_NONE;
    // ID2D1DeviceContext::PushLayer returns void, so the layer is always
    // popped on the matching path below.
    context.hudTarget->PushLayer(&layerParameters, nullptr);
    DrawCardBody(card, body, scale);
    DrawCardHeader(card, index, header, scale);
    SetHudColor(emphasized ? context.settings.accentColor : context.settings.gridColor);
    context.hudTarget->DrawRoundedRectangle(
        RoundedRect(rect, radius), context.hudBrush,
        emphasized ? 2.0f * scale : 1.0f * scale);
    context.hudTarget->PopLayer();
    if (transformed) {
        context.hudTarget->SetTransform(savedTransform);
    }
}

// A live preview is composited inside the host window, which sits below the HUD
// layer, so HUD artwork is always above every preview. A card may therefore
// only yield the preview rectangles of cards painted after it: reserving a
// lower card's preview would punch that card through this one, and the upright
// card at the front of a stack would show the cards stacked beneath it.
inline std::vector<RectF> HigherThumbnailRects(
    const std::vector<Card>& cards, const std::vector<size_t>& order,
    size_t position) {
    std::vector<RectF> thumbnails;
    for (size_t slot = position + 1; slot < order.size(); ++slot) {
        const size_t index = order[slot];
        if (index >= cards.size()) {
            continue;
        }
        const Card& card = cards[index];
        if (card.state != WindowVisualState::Live || !card.thumbnail.valid() ||
            !card.thumbnailVisible) {
            continue;
        }
        const RectF destination = RectFromRect(card.thumbnailDestination);
        if (!destination.empty()) {
            thumbnails.push_back(destination);
        }
    }
    return thumbnails;
}

// One card's paint request: the card, its index for hover/selection emphasis,
// and the chrome scale shared by every card in the frame.
struct HudCardPaint {
    const Card& card;
    int index;
    float scale;
};

inline void DrawCardWithThumbnailClips(const HudCardPaint& paint,
                                       const CardDrawClips& clips) {
    if (!clips.clipped) {
        DrawCard(paint.card, paint.index, paint.scale);
        return;
    }
    for (const RectF& region : clips.regions) {
        context.hudTarget->PushAxisAlignedClip(
            D2D1::RectF(region.left, region.top, region.right, region.bottom),
            D2D1_ANTIALIAS_MODE_ALIASED);
        DrawCard(paint.card, paint.index, paint.scale);
        context.hudTarget->PopAxisAlignedClip();
    }
}

inline CardDrawClips CardThumbnailDrawClips(
    const std::vector<Card>& cards, const std::vector<size_t>& order,
    size_t position) {
    const size_t index = order[position];
    if (index >= cards.size()) {
        return CardDrawClips{};
    }
    const RectF bounds = CardScreenBounds(cards[index]);
    const std::array<Vec2, 4> quad = CardScreenQuad(bounds, cards[index].stack);
    const RectF viewport{0.0f, 0.0f, static_cast<float>(VirtualScreenWidth()),
                         static_cast<float>(VirtualScreenHeight())};
    return CardDrawRegions(viewport, quad,
                           HigherThumbnailRects(cards, order, position));
}

inline void DrawMinimapContents(const RectF& map, float scale) {
    const RectF bounds = CanvasWorldBounds();
    const RectF inner = MinimapInnerRect(map);
    if (bounds.empty() || inner.empty()) {
        return;
    }
    context.hudTarget->PushAxisAlignedClip(
        D2D1::RectF(inner.left, inner.top, inner.right, inner.bottom),
        D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    for (const Card& card : context.cards) {
        const RectF projected = MinimapWorldToScreen(StackWorldRect(card), map, bounds);
        SetHudColor(card.state == WindowVisualState::Live
                        ? context.settings.accentColor
                        : context.settings.mutedTextColor);
        context.hudTarget->FillRectangle(
            D2D1::RectF(projected.left, projected.top, projected.right, projected.bottom),
            context.hudBrush);
    }
    const RectF view = MinimapWorldToScreen(ViewportWorld(context.camera), map, bounds);
    SetHudColor(context.settings.textColor, 0.85f);
    context.hudTarget->DrawRectangle(
        D2D1::RectF(view.left, view.top, view.right, view.bottom), context.hudBrush,
        1.0f * scale);
    context.hudTarget->PopAxisAlignedClip();
}

inline void DrawMinimap(float scale) {
    context.minimapRect = {};
    if (!context.settings.minimapEnabled) {
        return;
    }
    const RectF map = CalculateMinimapRect();
    if (map.empty()) {
        return;
    }
    context.minimapRect = map;
    SetHudColor(context.settings.backgroundColor, 0.9f);
    context.hudTarget->FillRoundedRectangle(RoundedRect(map, 6.0f * scale),
                                            context.hudBrush);
    SetHudColor(context.settings.gridColor);
    context.hudTarget->DrawRoundedRectangle(RoundedRect(map, 6.0f * scale),
                                            context.hudBrush, 1.0f * scale);
    DrawMinimapContents(map, scale);
}

inline void DrawEmptyCanvas(float scale) {
    if (!context.cards.empty()) {
        return;
    }
    const D2D1_RECT_F emptyRect = D2D1::RectF(
        24.0f * scale, 24.0f * scale,
        static_cast<float>(VirtualScreenWidth()) - 24.0f * scale,
        static_cast<float>(VirtualScreenHeight()) - 24.0f * scale);
    SetHudColor(context.settings.mutedTextColor);
    context.hudTarget->DrawTextW(
        CanvasText(CanvasTextId::EmptyCanvas, context.settings.language),
        CanvasTextLength(CanvasTextId::EmptyCanvas, context.settings.language),
        context.titleFormat,
        emptyRect, context.hudBrush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

inline void DrawAlignmentGuides(float scale) {
    if (context.hasGuideVertical) {
        SetHudColor(context.settings.accentColor, 0.8f);
        context.hudTarget->DrawLine(D2D1::Point2F(context.guideVertical.left, 0.0f),
                                    D2D1::Point2F(context.guideVertical.left,
                                                  static_cast<float>(VirtualScreenHeight())),
                                    context.hudBrush, 1.0f * scale);
    }
    if (context.hasGuideHorizontal) {
        SetHudColor(context.settings.accentColor, 0.8f);
        context.hudTarget->DrawLine(D2D1::Point2F(0.0f, context.guideHorizontal.top),
                                    D2D1::Point2F(static_cast<float>(VirtualScreenWidth()),
                                                  context.guideHorizontal.top),
                                    context.hudBrush, 1.0f * scale);
    }
}

inline bool PresentHudFrame() {
    POINT destination = {context.virtualScreen.left, context.virtualScreen.top};
    POINT origin = {0, 0};
    SIZE size = {VirtualScreenWidth(), VirtualScreenHeight()};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    return UpdateLayeredWindow(context.hudWindow, nullptr, &destination, &size,
                               context.hudDc, &origin, 0, &blend, ULW_ALPHA) != FALSE;
}

inline bool SubmitHudFrame() {
    const int width = VirtualScreenWidth();
    const int height = VirtualScreenHeight();
    if (!EnsureGraphics() || !CreateHudSurface(width, height)) {
        return false;
    }
    const RECT bounds = {0, 0, width, height};
    if (FAILED(context.hudTarget->BindDC(context.hudDc, &bounds))) {
        ReleaseHudSurface();
        return false;
    }
    context.hudTarget->BeginDraw();
    context.hudTarget->Clear(D2D1::ColorF(0, 0.0f));
    const float scale = ChromeScale();
    const std::vector<size_t> order = CardDrawOrder(context.cards);
    for (size_t position = 0; position < order.size(); ++position) {
        const size_t index = order[position];
        const CardDrawClips clips =
            CardThumbnailDrawClips(context.cards, order, position);
        DrawCardWithThumbnailClips(
            HudCardPaint{context.cards[index], static_cast<int>(index), scale},
            clips);
    }
    DrawEmptyCanvas(scale);
    DrawMinimap(scale);
    DrawAlignmentGuides(scale);
    if (FAILED(context.hudTarget->EndDraw())) {
        ReleaseHudSurface();
        return false;
    }
    return PresentHudFrame();
}
