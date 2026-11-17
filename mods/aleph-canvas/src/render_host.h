#pragma once

#include "context.h"
#include "math_utils.h"
#include "types.h"

inline COLORREF ColorRef(uint32_t color) {
    return RGB((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
}

inline void DrawHostScene(HDC dc) {
    RECT client = {0, 0, context.virtualScreen.right - context.virtualScreen.left,
                   context.virtualScreen.bottom - context.virtualScreen.top};
    HBRUSH background = CreateSolidBrush(ColorRef(context.settings.backgroundColor));
    FillRect(dc, &client, background);
    DeleteObject(background);
    if (!context.settings.showGrid) {
        return;
    }
    const float spacing = static_cast<float>(context.settings.gridSpacing) *
                          context.camera.zoom;
    if (spacing < kMinGridSpacingPx) {
        return;
    }
    HPEN pen = CreatePen(PS_SOLID, 1, ColorRef(context.settings.gridColor));
    HGDIOBJ oldPen = SelectObject(dc, pen);
    const RectF view = ViewportWorld(context.camera);
    const float startX = std::floor(view.left / context.settings.gridSpacing) *
                         context.settings.gridSpacing;
    const float startY = std::floor(view.top / context.settings.gridSpacing) *
                         context.settings.gridSpacing;
    const int lineCountX = std::min(kMaxGridLines, static_cast<int>(
        std::ceil(static_cast<float>(context.virtualScreen.right - context.virtualScreen.left) / spacing) + 2.0f));
    const int lineCountY = std::min(kMaxGridLines, static_cast<int>(
        std::ceil(static_cast<float>(context.virtualScreen.bottom - context.virtualScreen.top) / spacing) + 2.0f));
    for (int index = 0; index < lineCountX; ++index) {
        const float worldX = startX + static_cast<float>(index) * context.settings.gridSpacing;
        const float screenX = (worldX - context.camera.centerX) * context.camera.zoom +
                              static_cast<float>(client.right) * 0.5f;
        MoveToEx(dc, static_cast<int>(screenX), 0, nullptr);
        LineTo(dc, static_cast<int>(screenX), client.bottom);
    }
    for (int index = 0; index < lineCountY; ++index) {
        const float worldY = startY + static_cast<float>(index) * context.settings.gridSpacing;
        const float screenY = (worldY - context.camera.centerY) * context.camera.zoom +
                              static_cast<float>(client.bottom) * 0.5f;
        MoveToEx(dc, 0, static_cast<int>(screenY), nullptr);
        LineTo(dc, client.right, static_cast<int>(screenY));
    }
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}
