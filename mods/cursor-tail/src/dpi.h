#pragma once

// Cursor monitor DPI tracking.

#include "platform.h"
#include "context.h"
#include "overlay.h"
#include "forward.h"

static bool UpdateCursorMonitorDpi(HMONITOR monitor) {
    if (!monitor || monitor == context.cursorMonitor) return false;

    UINT dpiX = 96;
    UINT dpiY = 96;
    const HRESULT dpiResult = GetDpiForMonitor(
        monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
    if (FAILED(dpiResult) || dpiX == 0) {
        Wh_Log(L"GetDpiForMonitor failed: 0x%08X", static_cast<unsigned int>(dpiResult));
        dpiX = 96;
    }

    context.cursorMonitor = monitor;
    ResetAutoColorSamplingCache();
    const UINT newDpi = dpiX > 0 ? dpiX : 96;
    if (newDpi == context.cursorDpi) return false;

    context.cursorDpi = newDpi;
    context.currentDpiScale = yoko::win32::DpiScaleForDpi(context.cursorDpi);
    HideOverlay();
    return true;
}
