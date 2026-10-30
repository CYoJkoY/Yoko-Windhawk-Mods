#pragma once

// Layered-window backbuffer growth, binding and D2D resource setup.

#include "platform.h"
#include "context.h"
#include "forward.h"
#include "render.h"

static int GrowBackbufferDimension(int required, int current) {
    if (required <= current) return current;

    const int64_t rounded = (
        (static_cast<int64_t>(required) + kBackbufferGrowthGranularity - 1)
        / kBackbufferGrowthGranularity
    ) * kBackbufferGrowthGranularity;
    return rounded <= std::numeric_limits<int>::max()
        ? static_cast<int>(rounded)
        : required;
}

// Growing swaps in a fresh DC and DIB, then rebinds the existing Direct2D
// target instead of rebuilding every brush, layer, and device bitmap. The new
// surface is built first, so the old one stays usable if anything fails.
static bool SwapBackbufferSurface(int newWidth, int newHeight, HDC referenceDc) {
    HDC newDc = CreateCompatibleDC(referenceDc);
    if (!newDc) return false;
    HBITMAP newBitmap = Create32BitDIB(newWidth, newHeight);
    if (!newBitmap) {
        DeleteDC(newDc);
        return false;
    }
    const HGDIOBJ newOriginalBitmap = SelectObject(newDc, newBitmap);
    if (!newOriginalBitmap || newOriginalBitmap == HGDI_ERROR) {
        DeleteObject(newBitmap);
        DeleteDC(newDc);
        return false;
    }

    if (context.renderTarget) {
        const RECT bindRect = {0, 0, newWidth, newHeight};
        if (FAILED(context.renderTarget->BindDC(newDc, &bindRect))) {
            // A failed rebind leaves the old binding in an unknown state, so
            // rebuild the target on the next frame and keep the new surface.
            ReleaseRenderResources();
        } else {
            context.dcBound = true;
        }
    }

    HDC oldDc = context.backBufferDc;
    HBITMAP oldBitmap = context.backBufferBitmap;
    HGDIOBJ oldOriginalBitmap = context.originalBitmap;
    context.backBufferDc = newDc;
    context.backBufferBitmap = newBitmap;
    context.originalBitmap = newOriginalBitmap;
    if (oldDc) {
        if (oldOriginalBitmap) SelectObject(oldDc, oldOriginalBitmap);
        DeleteDC(oldDc);
    }
    if (oldBitmap) DeleteObject(oldBitmap);
    return true;
}

bool EnsureBackbuffer(int width, int height, HDC referenceDc) {
    if (width <= 0 || height <= 0 || !referenceDc) return false;
    if (context.backBufferDc && context.backBufferBitmap && width <= context.cachedWidth && height <= context.cachedHeight) return true;
    const int newWidth = GrowBackbufferDimension(width, context.cachedWidth);
    const int newHeight = GrowBackbufferDimension(height, context.cachedHeight);
    if (!SwapBackbufferSurface(newWidth, newHeight, referenceDc)) return false;
    context.cachedWidth = newWidth;
    context.cachedHeight = newHeight;
    context.backbufferIdleSince = 0;
    return true;
}

static void MaybeReleaseIdleBackbuffer(DWORD now, LONGLONG nowQpc) {
    if (context.isSmearing || context.historyCount > 0 || HasActiveRipples(nowQpc) || OrnamentActive()) {
        context.backbufferIdleSince = 0;
        return;
    }
    if (!context.backBufferDc || !context.backBufferBitmap) {
        context.backbufferIdleSince = 0;
        return;
    }
    if (context.backbufferIdleSince == 0) {
        context.backbufferIdleSince = now;
        return;
    }
    if (now - context.backbufferIdleSince >= kBackbufferIdleReleaseDelayMs) {
        ReleaseBackbuffer();
    }
}

bool EnsureD2DResources() {
    if (!context.d2dFactory) return false;
    if (context.renderTarget) return true;
    const D2D1_RENDER_TARGET_PROPERTIES properties = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    HRESULT hr = context.d2dFactory->CreateDCRenderTarget(&properties, &context.renderTarget);
    if (FAILED(hr) || !context.renderTarget) return false;
    hr = context.renderTarget->CreateSolidColorBrush(ToColorF(context.currentOuterRGB, 1.0f), &context.outerBrush);
    if (FAILED(hr)) { ReleaseRenderResources(); return false; }
    hr = context.renderTarget->CreateSolidColorBrush(ToColorF(context.currentCoreRGB, 1.0f), &context.coreBrush);
    if (FAILED(hr)) { ReleaseRenderResources(); return false; }
    hr = context.renderTarget->CreateSolidColorBrush(ToColorF(context.glowRGB, 1.0f), &context.glowBrush);
    if (FAILED(hr)) { ReleaseRenderResources(); return false; }
    context.outerBrushColorCache = context.currentOuterRGB;
    context.coreBrushColorCache = context.currentCoreRGB;
    context.glowBrushColorCache = context.glowRGB;
    hr = context.renderTarget->CreateLayer(nullptr, &context.trailOpacityLayer);
    if (FAILED(hr) || !context.trailOpacityLayer) { ReleaseRenderResources(); return false; }
    hr = context.renderTarget->CreateLayer(nullptr, &context.glowOpacityLayer);
    if (FAILED(hr) || !context.glowOpacityLayer) { ReleaseRenderResources(); return false; }
    hr = context.renderTarget->CreateSolidColorBrush(ToColorF(context.leftClickRGB, 1.0f), &context.rippleBrush);
    if (FAILED(hr)) { ReleaseRenderResources(); return false; }
    hr = context.renderTarget->CreateSolidColorBrush(ToColorF(context.currentCoreRGB, 1.0f), &context.ornamentBrush);
    if (FAILED(hr)) { ReleaseRenderResources(); return false; }
    D2D1_STROKE_STYLE_PROPERTIES strokeProperties = {};
    strokeProperties.startCap = D2D1_CAP_STYLE_ROUND;
    strokeProperties.endCap = D2D1_CAP_STYLE_ROUND;
    strokeProperties.lineJoin = D2D1_LINE_JOIN_ROUND;
    hr = context.d2dFactory->CreateStrokeStyle(strokeProperties, nullptr, 0, &context.strokeStyle);
    if (FAILED(hr)) { ReleaseRenderResources(); return false; }
    return true;
}

bool EnsureGradientBrush() {
    if (!context.gradientEnabled) {
        if (context.gradientBrush) { context.gradientBrush->Release(); context.gradientBrush = nullptr; }
        if (context.gradientStops) { context.gradientStops->Release(); context.gradientStops = nullptr; }
        context.gradientHeadCache = 0xFFFFFFFFu;
        context.gradientTailCache = 0xFFFFFFFFu;
        return false;
    }
    if (!context.renderTarget) return false;
    if (context.gradientBrush && context.gradientStops && context.gradientHeadCache == context.currentCoreRGB && context.gradientTailCache == context.gradientTailRGB) return true;
    if (context.gradientBrush) { context.gradientBrush->Release(); context.gradientBrush = nullptr; }
    if (context.gradientStops) { context.gradientStops->Release(); context.gradientStops = nullptr; }
    D2D1_GRADIENT_STOP stops[2] = {};
    stops[0].position = 0.0f;
    stops[0].color = ToColorF(context.currentCoreRGB, 1.0f);
    stops[1].position = 1.0f;
    stops[1].color = ToColorF(context.gradientTailRGB, 1.0f);
    HRESULT hr = context.renderTarget->CreateGradientStopCollection(stops, 2, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, &context.gradientStops);
    if (FAILED(hr)) return false;
    const D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES properties = {};
    hr = context.renderTarget->CreateLinearGradientBrush(properties, context.gradientStops, &context.gradientBrush);
    if (FAILED(hr)) { context.gradientStops->Release(); context.gradientStops = nullptr; return false; }
    context.gradientHeadCache = context.currentCoreRGB;
    context.gradientTailCache = context.gradientTailRGB;
    return true;
}
