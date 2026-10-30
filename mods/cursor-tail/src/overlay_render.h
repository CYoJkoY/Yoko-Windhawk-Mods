#pragma once

// One full overlay frame: trail layers, ripples and ornaments.

#include "platform.h"
#include "context.h"
#include "types.h"
#include "trail.h"
#include "ornaments.h"
#include "color.h"
#include "backbuffer.h"
#include "math_utils.h"
#include "interaction.h"

bool RenderOverlay(HWND hwnd, LONGLONG nowQpc) {
    // Node transforms feed only the ornament draw path, so skip them entirely
    // while the ornament is disabled.
    if (context.ornamentEnabled) UpdateOrnamentDrawStates();
    const bool ornamentVisible = OrnamentVisible();
    bool drawTrail = context.historyCount >= 2;
    const float speed = context.speedScaling ? (context.isSmearing ? context.smoothedSpeedNorm : context.frozenSpeedNorm) : 1.0f;
    const int renderLength = GetEffectiveTailLength(speed);
    const float outerHalf = (context.widthMin + (context.widthMax - context.widthMin) * speed) * context.currentDpiScale;
    const float coreHalf = (context.coreWidthMin + (context.coreWidthMax - context.coreWidthMin) * speed) * context.currentDpiScale;
    const float alphaScale = context.alphaMin + (context.alphaMax - context.alphaMin) * speed;
    const float trailAlpha = alphaScale * context.fadeAlpha;

    if (drawTrail) {
        SmoothTrail(context.smoothIterations, renderLength);
        drawTrail = context.renderCache.pointCount >= 2 && trailAlpha >= 0.02f;
    }

    const int activeRippleCount = UpdateRippleState(nowQpc);
    const int activeMotionAccentCount = UpdateMotionAccentState(nowQpc);
    if (!drawTrail
        && activeRippleCount == 0
        && activeMotionAccentCount == 0
        && !ornamentVisible) {
        return false;
    }

    RECT bounds = {};
    bool hasBounds = false;
    const auto includeBounds = [&bounds, &hasBounds](const RECT& candidate) {
        if (!hasBounds) {
            bounds = candidate;
            hasBounds = true;
            return;
        }
        bounds.left = std::min(bounds.left, candidate.left);
        bounds.top = std::min(bounds.top, candidate.top);
        bounds.right = std::max(bounds.right, candidate.right);
        bounds.bottom = std::max(bounds.bottom, candidate.bottom);
    };

    if (drawTrail) {
        const float boundsHalf = context.glowEnabled
            ? outerHalf * std::max(1.0f, context.glowWidthFactor)
            : outerHalf;
        includeBounds(CalculateTrailBounds(boundsHalf));
    }

    // These values originate from integer settings, so no floating-point round-up is needed.
    const float scaledRippleRadius = context.rippleRadius * context.currentDpiScale;
    const float scaledRippleStrokeWidth = context.rippleStrokeWidth * context.currentDpiScale;
    const float scaledWheelArrowSize = context.wheelArrowSize * context.currentDpiScale;
    const float scaledWheelArrowDistance = context.wheelArrowDistance * context.currentDpiScale;
    const LONG ripplePadding = static_cast<LONG>(
        scaledRippleRadius + scaledRippleStrokeWidth + kRenderPadding * context.currentDpiScale + 1.0f);
    const LONG wheelArrowPadding = static_cast<LONG>(ceilf(
        scaledWheelArrowDistance * 0.5f
        + scaledWheelArrowSize * 0.65f
        + scaledRippleStrokeWidth
        + kRenderPadding * context.currentDpiScale
        + 2.0f));
    const MotionAccent* newestMotionAccent = nullptr;
    for (const MotionAccent& accent : context.motionAccents) {
        if (accent.active && (!newestMotionAccent || accent.startQpc > newestMotionAccent->startQpc)) {
            newestMotionAccent = &accent;
        }
    }
    const float scaledMotionAccentSize = context.motionAccentSize * context.currentDpiScale;
    const LONG motionAccentPadding = static_cast<LONG>(
        ceilf(scaledMotionAccentSize * 3.8f + kRenderPadding * context.currentDpiScale + 3.0f));
    for (const MotionAccent& accent : context.motionAccents) {
        if (!accent.active) continue;
        if (newestMotionAccent) {
            const int64_t deltaX = static_cast<int64_t>(accent.center.x) - newestMotionAccent->center.x;
            const int64_t deltaY = static_cast<int64_t>(accent.center.y) - newestMotionAccent->center.y;
            if (deltaX < -kMaxRippleClusterRadiusPx || deltaX > kMaxRippleClusterRadiusPx
                || deltaY < -kMaxRippleClusterRadiusPx || deltaY > kMaxRippleClusterRadiusPx) {
                continue;
            }
        }
        const RECT accentBounds = {
            accent.center.x - motionAccentPadding,
            accent.center.y - motionAccentPadding,
            accent.center.x + motionAccentPadding,
            accent.center.y + motionAccentPadding,
        };
        includeBounds(accentBounds);
    }

    const RippleEffect* newestRipple = nullptr;
    for (const RippleEffect& ripple : context.ripples) {
        if (ripple.active && (!newestRipple || ripple.startQpc > newestRipple->startQpc)) {
            newestRipple = &ripple;
        }
    }
    for (const RippleEffect& ripple : context.ripples) {
        if (!ripple.active) continue;
        // A single layered window must submit one rectangular bitmap. Avoid a
        // huge allocation if interactions occur on distant monitors in quick
        // succession; keep rendering the most recent local cluster instead.
        if (newestRipple) {
            const int64_t deltaX = static_cast<int64_t>(ripple.center.x) - newestRipple->center.x;
            const int64_t deltaY = static_cast<int64_t>(ripple.center.y) - newestRipple->center.y;
            if (deltaX < -kMaxRippleClusterRadiusPx || deltaX > kMaxRippleClusterRadiusPx
                || deltaY < -kMaxRippleClusterRadiusPx || deltaY > kMaxRippleClusterRadiusPx) {
                continue;
            }
        }
        const LONG effectPadding = ripple.kind == RippleKind::Click
            ? ripplePadding
            : wheelArrowPadding;
        const RECT rippleBounds = {
            ripple.center.x - effectPadding,
            ripple.center.y - effectPadding,
            ripple.center.x + effectPadding,
            ripple.center.y + effectPadding,
        };
        includeBounds(rippleBounds);
    }
    if (ornamentVisible) includeBounds(CalculateOrnamentBounds());
    if (!hasBounds) return false;

    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    HDC screen = GetScreenDC();
    if (!screen || !EnsureBackbuffer(width, height, screen) || !EnsureD2DResources()) return false;
    if (!context.dcBound) {
        const RECT bindRect = {0, 0, context.cachedWidth, context.cachedHeight};
        if (FAILED(context.renderTarget->BindDC(context.backBufferDc, &bindRect))) return false;
        context.dcBound = true;
    }

    if (ornamentVisible) EnsureOrnamentBitmaps();

    context.renderTarget->BeginDraw();
    context.renderTarget->SetTransform(D2D1::Matrix3x2F::Translation(
        -static_cast<float>(bounds.left), -static_cast<float>(bounds.top)));
    const D2D1_RECT_F clipBounds = D2D1::RectF(
        static_cast<float>(bounds.left),
        static_cast<float>(bounds.top),
        static_cast<float>(bounds.right),
        static_cast<float>(bounds.bottom));
    context.renderTarget->PushAxisAlignedClip(clipBounds, D2D1_ANTIALIAS_MODE_ALIASED);
    context.renderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));

    if (drawTrail) {
        const D2D1_LAYER_PARAMETERS trailLayerParameters = D2D1::LayerParameters(
            clipBounds,
            nullptr,
            D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
            D2D1::IdentityMatrix(),
            std::clamp(trailAlpha, 0.0f, 1.0f));
        context.renderTarget->PushLayer(&trailLayerParameters, context.trailOpacityLayer);

        if (context.glowEnabled) {
            const D2D1_LAYER_PARAMETERS glowLayerParameters = D2D1::LayerParameters(
                clipBounds,
                nullptr,
                D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
                D2D1::IdentityMatrix(),
                std::clamp(context.glowAlpha, 0.0f, 1.0f));
            context.renderTarget->PushLayer(&glowLayerParameters, context.glowOpacityLayer);
            ApplySolidBrushColor(context.glowBrush, context.glowRGB, context.glowBrushColorCache);
            DrawTrailLayer(
                context.renderTarget,
                context.glowBrush,
                outerHalf * std::max(1.0f, context.glowWidthFactor));
            context.renderTarget->PopLayer();
        }

        ApplySolidBrushColor(context.outerBrush, context.currentOuterRGB, context.outerBrushColorCache);
        ApplySolidBrushColor(context.coreBrush, context.currentCoreRGB, context.coreBrushColorCache);
        DrawTrailLayer(context.renderTarget, context.outerBrush, outerHalf);

        ID2D1Brush* coreBrush = context.coreBrush;
        if (context.gradientEnabled && EnsureGradientBrush()) {
            context.gradientBrush->SetStartPoint(context.renderCache.activePoints[0]);
            context.gradientBrush->SetEndPoint(
                context.renderCache.activePoints[context.renderCache.pointCount - 1]);
            context.gradientBrush->SetOpacity(1.0f);
            coreBrush = context.gradientBrush;
        }
        DrawTrailLayer(context.renderTarget, coreBrush, coreHalf);
        context.renderTarget->PopLayer();
    }

    const LONGLONG accentDurationTicks = std::max<LONGLONG>(
        1, context.qpcFrequency * static_cast<LONGLONG>(context.motionAccentDurationMs) / 1000);
    for (const MotionAccent& accent : context.motionAccents) {
        if (!accent.active) continue;
        const LONGLONG deltaX = newestMotionAccent
            ? static_cast<LONGLONG>(accent.center.x) - newestMotionAccent->center.x
            : 0;
        const LONGLONG deltaY = newestMotionAccent
            ? static_cast<LONGLONG>(accent.center.y) - newestMotionAccent->center.y
            : 0;
        if (newestMotionAccent
            && (deltaX < -kMaxRippleClusterRadiusPx || deltaX > kMaxRippleClusterRadiusPx
                || deltaY < -kMaxRippleClusterRadiusPx || deltaY > kMaxRippleClusterRadiusPx)) {
            continue;
        }
        const LONGLONG elapsedTicks = nowQpc - accent.startQpc;
        if (elapsedTicks < 0 || elapsedTicks >= accentDurationTicks) continue;
        const float progress = std::clamp(
            static_cast<float>(elapsedTicks) / static_cast<float>(accentDurationTicks),
            0.0f,
            1.0f);
        const float remaining = 1.0f - progress;
        const float alpha = context.motionAccentOpacity * accent.intensity * remaining * remaining;
        if (alpha < 0.005f) continue;

        const float directionX = accent.directionX;
        const float directionY = accent.directionY;
        const float normalX = -directionY;
        const float normalY = directionX;
        const D2D1_POINT_2F center = D2D1::Point2F(
            static_cast<float>(accent.center.x),
            static_cast<float>(accent.center.y));
        const auto pointAlong = [&](float along, float normal) {
            return D2D1::Point2F(
                center.x + directionX * along + normalX * normal,
                center.y + directionY * along + normalY * normal);
        };
        const float size = scaledMotionAccentSize
            * (0.82f + 0.18f * remaining)
            * std::clamp(accent.intensity, 0.5f, 1.8f);

        // Each cue uses a heavy contrast silhouette with a saturated color core.
        // This keeps the shape readable without sampling screen pixels every frame.
        const float stroke = std::max(1.5f, context.currentDpiScale * (2.2f - 0.6f * progress));
        const float outlineExtra = std::max(1.5f, context.currentDpiScale * 1.5f);
        const float visibleAlpha = std::clamp(alpha, 0.0f, 1.0f);
        const float accentLuminance =
            0.2126f * static_cast<float>((context.motionAccentRGB >> 16) & 0xFF)
            + 0.7152f * static_cast<float>((context.motionAccentRGB >> 8) & 0xFF)
            + 0.0722f * static_cast<float>(context.motionAccentRGB & 0xFF);
        const uint32_t outlineRGB = accentLuminance >= 128.0f ? 0x00000000 : 0x00FFFFFF;
        const auto drawAccentLine = [&](const D2D1_POINT_2F& start,
                                        const D2D1_POINT_2F& end,
                                        float lineWidth) {
            context.rippleBrush->SetColor(ToColorF(outlineRGB, visibleAlpha));
            context.renderTarget->DrawLine(
                start, end, context.rippleBrush, lineWidth + outlineExtra, context.strokeStyle);
            context.rippleBrush->SetColor(ToColorF(context.motionAccentRGB, visibleAlpha));
            context.renderTarget->DrawLine(
                start, end, context.rippleBrush, lineWidth, context.strokeStyle);
        };

        if (accent.kind == MotionAccentKind::Acceleration) {
            // The forward arrow communicates travel direction; the three
            // trailing lines expand backward so acceleration reads as a burst.
            const float tailExpansion = size * (0.16f + 0.58f * progress);
            for (int streak = -1; streak <= 1; ++streak) {
                const float lateral = static_cast<float>(streak)
                    * size * (0.34f + 0.05f * progress);
                const float rearOffset = static_cast<float>(streak == 0 ? 0 : 1);
                const float startAlong = -size * (0.10f + 0.12f * rearOffset)
                    - size * 0.12f * progress;
                const float endAlong = -size * (0.78f + 0.24f * rearOffset) - tailExpansion;
                const float lineWidth = stroke * (streak == 0 ? 1.2f : 0.88f);
                drawAccentLine(
                    pointAlong(startAlong, lateral),
                    pointAlong(endAlong, lateral),
                    lineWidth);
            }

            const D2D1_POINT_2F arrowTip = pointAlong(size * 0.42f, 0.0f);
            const D2D1_POINT_2F arrowBase = pointAlong(size * 0.08f, 0.0f);
            const D2D1_POINT_2F arrowWingA = pointAlong(size * 0.06f, size * 0.30f);
            const D2D1_POINT_2F arrowWingB = pointAlong(size * 0.06f, -size * 0.30f);
            drawAccentLine(arrowBase, arrowTip, stroke * 1.12f);
            drawAccentLine(arrowTip, arrowWingA, stroke * 1.12f);
            drawAccentLine(arrowTip, arrowWingB, stroke * 1.12f);
        } else {
            // A turn is drawn as the actual change of path, not a generic
            // chevron: the incoming leg arrives at the pivot and the outgoing
            // leg terminates in an arrowhead pointing along the new direction.
            const float previousLength = sqrtf(
                accent.previousDirectionX * accent.previousDirectionX
                + accent.previousDirectionY * accent.previousDirectionY);
            const float incomingExtent = size * (0.34f + 0.36f * progress);
            const float outgoingExtent = size * (0.30f + 0.38f * progress);
            if (previousLength > 0.5f) {
                const D2D1_POINT_2F incomingStart = D2D1::Point2F(
                    center.x - accent.previousDirectionX / previousLength * incomingExtent,
                    center.y - accent.previousDirectionY / previousLength * incomingExtent);
                drawAccentLine(incomingStart, center, stroke * 1.18f);
            } else {
                drawAccentLine(pointAlong(-incomingExtent, 0.0f), center, stroke * 1.18f);
            }

            const D2D1_POINT_2F arrowTip = pointAlong(outgoingExtent, 0.0f);
            const D2D1_POINT_2F arrowBase = pointAlong(outgoingExtent - size * 0.28f, 0.0f);
            const D2D1_POINT_2F arrowWingA = pointAlong(
                outgoingExtent - size * 0.30f, size * 0.22f);
            const D2D1_POINT_2F arrowWingB = pointAlong(
                outgoingExtent - size * 0.30f, -size * 0.22f);
            drawAccentLine(center, arrowBase, stroke * 1.18f);
            drawAccentLine(arrowBase, arrowTip, stroke * 1.18f);
            drawAccentLine(arrowTip, arrowWingA, stroke * 1.18f);
            drawAccentLine(arrowTip, arrowWingB, stroke * 1.18f);
        }
    }

    const LONGLONG durationTicks = std::max<LONGLONG>(
        1, context.qpcFrequency * static_cast<LONGLONG>(context.rippleDurationMs) / 1000);
    for (const RippleEffect& ripple : context.ripples) {
        if (!ripple.active) continue;
        const LONGLONG elapsedTicks = nowQpc - ripple.startQpc;
        if (elapsedTicks < 0 || elapsedTicks >= durationTicks) continue;
        const float progress = std::clamp(
            static_cast<float>(elapsedTicks) / static_cast<float>(durationTicks),
            0.0f,
            1.0f);
        const float easedProgress = progress * (2.0f - progress);
        const float radius = (kRippleInitialRadius
            + (context.rippleRadius - kRippleInitialRadius) * easedProgress) * context.currentDpiScale;
        const float remaining = 1.0f - progress;
        const float alpha = context.rippleOpacity * remaining * remaining;
        if (alpha < 0.005f) continue;

        if (ripple.kind != RippleKind::Click) {
            const float directionX = ripple.kind == RippleKind::WheelLeft
                ? -1.0f
                : ripple.kind == RippleKind::WheelRight ? 1.0f : 0.0f;
            const float directionY = ripple.kind == RippleKind::WheelUp
                ? -1.0f
                : ripple.kind == RippleKind::WheelDown ? 1.0f : 0.0f;
            const float arrowCenterX = static_cast<float>(ripple.center.x)
                + directionX * scaledWheelArrowDistance * (easedProgress - 0.5f);
            const float arrowCenterY = static_cast<float>(ripple.center.y)
                + directionY * scaledWheelArrowDistance * (easedProgress - 0.5f);
            const float arrowSize = scaledWheelArrowSize * (0.88f + 0.12f * remaining);
            const D2D1_POINT_2F arrowTip = D2D1::Point2F(
                arrowCenterX + directionX * arrowSize * 0.5f,
                arrowCenterY + directionY * arrowSize * 0.5f);
            const float perpendicularX = -directionY;
            const float perpendicularY = directionX;
            const D2D1_POINT_2F arrowBase = D2D1::Point2F(
                arrowTip.x - directionX * arrowSize * 0.29f,
                arrowTip.y - directionY * arrowSize * 0.29f);
            const D2D1_POINT_2F arrowLeft = D2D1::Point2F(
                arrowBase.x - perpendicularX * arrowSize * 0.31f,
                arrowBase.y - perpendicularY * arrowSize * 0.31f);
            const D2D1_POINT_2F arrowRight = D2D1::Point2F(
                arrowBase.x + perpendicularX * arrowSize * 0.31f,
                arrowBase.y + perpendicularY * arrowSize * 0.31f);
            const D2D1_POINT_2F shaftStart = D2D1::Point2F(
                arrowCenterX - directionX * arrowSize * 0.42f,
                arrowCenterY - directionY * arrowSize * 0.42f);
            const D2D1_POINT_2F shaftEnd = D2D1::Point2F(
                arrowCenterX + directionX * arrowSize * 0.20f,
                arrowCenterY + directionY * arrowSize * 0.20f);

            context.rippleBrush->SetColor(ToColorF(ripple.color, alpha));
            context.renderTarget->DrawLine(
                shaftStart, shaftEnd, context.rippleBrush, scaledRippleStrokeWidth, context.strokeStyle);
            context.renderTarget->DrawLine(
                arrowTip, arrowLeft, context.rippleBrush, scaledRippleStrokeWidth, context.strokeStyle);
            context.renderTarget->DrawLine(
                arrowTip, arrowRight, context.rippleBrush, scaledRippleStrokeWidth, context.strokeStyle);
            continue;
        }

        context.rippleBrush->SetColor(ToColorF(ripple.color, alpha));
        context.renderTarget->DrawEllipse(
            D2D1::Ellipse(
                D2D1::Point2F(
                    static_cast<float>(ripple.center.x),
                    static_cast<float>(ripple.center.y)),
                radius,
                radius),
            context.rippleBrush,
            scaledRippleStrokeWidth,
            context.strokeStyle);
        context.rippleBrush->SetColor(ToColorF(ripple.color, alpha * 0.58f));
        const float innerRadius = radius * 0.58f;
        context.renderTarget->DrawEllipse(
            D2D1::Ellipse(
                D2D1::Point2F(
                    static_cast<float>(ripple.center.x),
                    static_cast<float>(ripple.center.y)),
                innerRadius,
                innerRadius),
            context.rippleBrush,
            std::max(context.currentDpiScale * 1.5f, scaledRippleStrokeWidth * 0.78f),
            context.strokeStyle);
    }

    if (ornamentVisible) DrawOrnament(bounds);

    context.renderTarget->PopAxisAlignedClip();
    context.renderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
    const HRESULT drawResult = context.renderTarget->EndDraw();
    if (drawResult == D2DERR_RECREATE_TARGET) {
        ReleaseRenderResources();
        return false;
    }
    if (FAILED(drawResult)) return false;

    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    POINT position = {bounds.left, bounds.top};
    SIZE size = {width, height};
    POINT bufferOrigin = {0, 0};
    return UpdateLayeredWindow(
        hwnd,
        screen,
        &position,
        &size,
        context.backBufferDc,
        &bufferOrigin,
        0,
        &blend,
        ULW_ALPHA) != FALSE;
}
