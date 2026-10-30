#pragma once

// Render target, brush and screen DC lifetime helpers.

#include "platform.h"
#include "context.h"
#include "forward.h"

void ReleaseRenderResources() {
    ReleaseOrnamentBitmaps();
    if (context.ornamentBrush) { context.ornamentBrush->Release(); context.ornamentBrush = nullptr; }
    if (context.glowOpacityLayer) { context.glowOpacityLayer->Release(); context.glowOpacityLayer = nullptr; }
    if (context.trailOpacityLayer) { context.trailOpacityLayer->Release(); context.trailOpacityLayer = nullptr; }
    if (context.gradientBrush) { context.gradientBrush->Release(); context.gradientBrush = nullptr; }
    if (context.gradientStops) { context.gradientStops->Release(); context.gradientStops = nullptr; }
    if (context.strokeStyle) { context.strokeStyle->Release(); context.strokeStyle = nullptr; }
    if (context.rippleBrush) { context.rippleBrush->Release(); context.rippleBrush = nullptr; }
    if (context.glowBrush) { context.glowBrush->Release(); context.glowBrush = nullptr; }
    if (context.coreBrush) { context.coreBrush->Release(); context.coreBrush = nullptr; }
    if (context.outerBrush) { context.outerBrush->Release(); context.outerBrush = nullptr; }
    if (context.renderTarget) { context.renderTarget->Release(); context.renderTarget = nullptr; }
    context.gradientHeadCache = 0xFFFFFFFFu;
    context.gradientTailCache = 0xFFFFFFFFu;
    context.outerBrushColorCache = 0xFFFFFFFFu;
    context.coreBrushColorCache = 0xFFFFFFFFu;
    context.glowBrushColorCache = 0xFFFFFFFFu;
    context.dcBound = false;
}

void ReleaseBackbuffer() {
    ReleaseRenderResources();
    if (context.backBufferDc) {
        if (context.originalBitmap) { SelectObject(context.backBufferDc, context.originalBitmap); context.originalBitmap = nullptr; }
        DeleteDC(context.backBufferDc);
        context.backBufferDc = nullptr;
    }
    if (context.backBufferBitmap) { DeleteObject(context.backBufferBitmap); context.backBufferBitmap = nullptr; }
    context.cachedWidth = 0;
    context.cachedHeight = 0;
    context.backbufferIdleSince = 0;
}

HBITMAP Create32BitDIB(int width, int height) {
    if (width <= 0 || height <= 0) return nullptr;
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    return CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
}

HDC GetScreenDC() {
    if (!context.screenDc) context.screenDc = GetDC(nullptr);
    return context.screenDc;
}

void ReleaseScreenDC() {
    if (!context.screenDc) return;
    ReleaseDC(nullptr, context.screenDc);
    context.screenDc = nullptr;
}
