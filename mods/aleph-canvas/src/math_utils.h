#pragma once

// Camera projections and small numeric helpers.

#include "platform.h"
#include "constants.h"
#include "context.h"
#include "types.h"

inline float ClampFloat(float value, float minimum, float maximum) {
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

inline float ClampZoom(float zoom) {
    const float minimum =
        static_cast<float>(context.settings.minZoomPercent) / 100.0f;
    const float maximum =
        static_cast<float>(context.settings.maxZoomPercent) / 100.0f;
    return ClampFloat(zoom, minimum, std::max(minimum, maximum));
}

// Window chrome uses physical pixels but should stay proportional to the
// display that hosts the canvas, so every fixed metric is scaled by the DPI of
// the monitor under the cursor.
inline float ChromeScale() {
    return yoko::win32::DpiScaleForDpi(static_cast<UINT>(context.chromeDpi));
}

inline float CardHeaderHeight() {
    return std::clamp(kCardHeaderPx * ChromeScale() * context.camera.zoom,
                      kCardHeaderMinPx * ChromeScale(),
                      kCardHeaderMaxPx * ChromeScale());
}

inline float ScreenCenterX() {
    return static_cast<float>(context.virtualScreen.right -
                              context.virtualScreen.left) *
           0.5f;
}

inline float ScreenCenterY() {
    return static_cast<float>(context.virtualScreen.bottom -
                              context.virtualScreen.top) *
           0.5f;
}

// World units are virtual-screen pixels, so a card's stored rectangle matches
// the window's own frame rectangle until the user rearranges the canvas.
inline RectF WorldToScreen(const RectF& world, const Camera& camera) {
    return RectF{
        (world.left - camera.centerX) * camera.zoom + ScreenCenterX(),
        (world.top - camera.centerY) * camera.zoom + ScreenCenterY(),
        (world.right - camera.centerX) * camera.zoom + ScreenCenterX(),
        (world.bottom - camera.centerY) * camera.zoom + ScreenCenterY()};
}

inline RectF CardScreenBounds(const Card& card) {
    RectF rect = WorldToScreen(StackWorldRect(card), context.camera);
    rect.top -= CardHeaderHeight();
    return rect;
}

inline Vec2 ScreenToWorld(Vec2 point, const Camera& camera) {
    return Vec2{(point.x - ScreenCenterX()) / camera.zoom + camera.centerX,
                (point.y - ScreenCenterY()) / camera.zoom + camera.centerY};
}

// Keeps the world point under the cursor fixed while the zoom changes.
inline void ZoomAtPoint(Camera& camera, float newZoom, Vec2 cursor) {
    const float clamped = ClampZoom(newZoom);
    const Vec2 anchor = ScreenToWorld(cursor, camera);
    camera.targetZoom = clamped;
    camera.targetCenterX = anchor.x - (cursor.x - ScreenCenterX()) / clamped;
    camera.targetCenterY = anchor.y - (cursor.y - ScreenCenterY()) / clamped;
    camera.zoomAnchorWorld = anchor;
    camera.zoomAnchorScreen = cursor;
    camera.zoomAnchored = true;
}

inline void CenterOnPoint(Camera& camera, Vec2 worldPoint) {
    camera.zoomAnchored = false;
    camera.targetCenterX = worldPoint.x;
    camera.targetCenterY = worldPoint.y;
}

inline float SmoothTowards(float current, float target, float factor) {
    const float blended = current + (target - current) * factor;
    return std::fabs(target - blended) < kCameraEpsilonPx ? target : blended;
}

inline void AdvanceCamera(Camera& camera, float seconds) {
    const float factor =
        1.0f - std::exp(-kCameraSmoothingPerSecond * std::max(seconds, 0.0f));
    const float zoomDelta = camera.targetZoom - camera.zoom;
    camera.zoom = std::fabs(zoomDelta) < kZoomEpsilon
                      ? camera.targetZoom
                      : camera.zoom + zoomDelta * factor;
    if (camera.zoomAnchored) {
        camera.centerX = camera.zoomAnchorWorld.x -
            (camera.zoomAnchorScreen.x - ScreenCenterX()) / camera.zoom;
        camera.centerY = camera.zoomAnchorWorld.y -
            (camera.zoomAnchorScreen.y - ScreenCenterY()) / camera.zoom;
        return;
    }
    camera.centerX = SmoothTowards(camera.centerX, camera.targetCenterX, factor);
    camera.centerY = SmoothTowards(camera.centerY, camera.targetCenterY, factor);
}

inline float EaseOutCubic(float t) {
    const float clamped = ClampFloat(t, 0.0f, 1.0f);
    const float inverse = 1.0f - clamped;
    return 1.0f - inverse * inverse * inverse;
}

inline RectF ViewportWorld(const Camera& camera) {
    const Vec2 topLeft = ScreenToWorld(Vec2{0.0f, 0.0f}, camera);
    const Vec2 bottomRight =
        ScreenToWorld(Vec2{static_cast<float>(context.virtualScreen.right -
                                              context.virtualScreen.left),
                           static_cast<float>(context.virtualScreen.bottom -
                                              context.virtualScreen.top)},
                      camera);
    return RectF{topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};
}

inline bool ParseHexColor(const wchar_t* text, uint32_t& out) {
    if (text == nullptr) {
        return false;
    }
    const wchar_t* cursor = text;
    while (*cursor == L' ' || *cursor == L'#') {
        ++cursor;
    }
    if (wcslen(cursor) != 6) {
        return false;
    }
    uint32_t value = 0;
    for (int index = 0; index < 6; ++index) {
        const wchar_t digit = cursor[index];
        uint32_t nibble = 0;
        if (digit >= L'0' && digit <= L'9') {
            nibble = static_cast<uint32_t>(digit - L'0');
        } else if (digit >= L'a' && digit <= L'f') {
            nibble = static_cast<uint32_t>(digit - L'a' + 10);
        } else if (digit >= L'A' && digit <= L'F') {
            nibble = static_cast<uint32_t>(digit - L'A' + 10);
        } else {
            return false;
        }
        value = (value << 4) | nibble;
    }
    out = value;
    return true;
}
