#pragma once

// Fullscreen detection and suppression state.

#include "platform.h"
#include "context.h"
#include "forward.h"

static bool IsDesktopShellWindow(HWND hwnd) {
    wchar_t className[64] = {};
    const int length = GetClassNameW(hwnd, className, ARRAYSIZE(className));
    if (length <= 0) return false;
    return _wcsicmp(className, L"Progman") == 0
        || _wcsicmp(className, L"WorkerW") == 0;
}

static bool CoversMonitor(HWND hwnd, HMONITOR cursorMonitor, HMONITOR windowMonitor) {
    const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    if ((style & WS_CAPTION) != 0) return false;
    RECT windowRect = {};
    if (!GetWindowRect(hwnd, &windowRect)) return false;
    if (!cursorMonitor || windowMonitor != cursorMonitor) return false;
    MONITORINFO monitorInfo = {};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (!GetMonitorInfoW(windowMonitor, &monitorInfo)) return false;
    return windowRect.left <= monitorInfo.rcMonitor.left + kFullscreenTolerancePx
        && windowRect.top <= monitorInfo.rcMonitor.top + kFullscreenTolerancePx
        && windowRect.right >= monitorInfo.rcMonitor.right - kFullscreenTolerancePx
        && windowRect.bottom >= monitorInfo.rcMonitor.bottom - kFullscreenTolerancePx;
}

// SHQueryUserNotificationState reports session-wide state, not per-window state.
static bool CheckStrongFullscreenSignal() {
    QUERY_USER_NOTIFICATION_STATE state = QUNS_NOT_PRESENT;
    return SUCCEEDED(SHQueryUserNotificationState(&state)) && state == QUNS_RUNNING_D3D_FULL_SCREEN;
}

static bool IsFullscreenCandidate(
    DWORD now,
    HWND hwnd,
    HMONITOR cursorMonitor,
    HMONITOR windowMonitor) {
    // The caller already rejected the desktop shell and null handles. Avoid
    // repeating those user32 queries on every input sample.
    if (IsIconic(hwnd) || !IsWindowVisible(hwnd)) return false;

    if (!cursorMonitor || windowMonitor != cursorMonitor) return false;

    if (hwnd != context.fullscreenForegroundWindow) {
        context.fullscreenForegroundWindow = hwnd;
        context.lastFullscreenStrongCheck = 0;
        context.fullscreenStrongSignal = false;
        context.fullscreenCoversMonitor = false;
        context.fullscreenFalseSince = 0;
    }

    // Window geometry and the shell's D3D state do not need to be queried on
    // every input report, even though fullscreen transitions are event-driven.
    if (now - context.lastFullscreenStrongCheck >= kFullscreenStrongCheckIntervalMs) {
        context.lastFullscreenStrongCheck = now;
        context.fullscreenStrongSignal = CheckStrongFullscreenSignal();
        context.fullscreenCoversMonitor = CoversMonitor(hwnd, cursorMonitor, windowMonitor);
    }
    return context.fullscreenStrongSignal || context.fullscreenCoversMonitor;
}

static bool UpdateFullscreenState(DWORD now, HWND foreground, HMONITOR cursorMonitor) {
    const HMONITOR foregroundMonitor = foreground
        ? MonitorFromWindow(foreground, MONITOR_DEFAULTTONEAREST)
        : nullptr;
    const bool isDesktopShell = !foreground
        || foreground == GetDesktopWindow()
        || foreground == GetShellWindow()
        || IsDesktopShellWindow(foreground);
    const bool pointerLeftFullscreenMonitor =
        cursorMonitor && foregroundMonitor && cursorMonitor != foregroundMonitor;

    // The desktop shell is never a fullscreen app. Likewise, a fullscreen
    // window on another monitor must not suppress an overlay under the pointer.
    // These transitions are immediate; ordinary same-monitor fullscreen exit
    // still uses the 500 ms confirmation below to avoid flicker.
    if (isDesktopShell || pointerLeftFullscreenMonitor) {
        context.fullscreenSuppressed = false;
        context.fullscreenFalseSince = 0;
        return false;
    }

    const bool candidate = IsFullscreenCandidate(
        now, foreground, cursorMonitor, foregroundMonitor);
    if (candidate) {
        context.fullscreenFalseSince = 0;
        context.fullscreenSuppressed = true;
        return true;
    }

    if (context.fullscreenSuppressed) {
        if (context.fullscreenFalseSince == 0) context.fullscreenFalseSince = now;
        if (now - context.fullscreenFalseSince >= kFullscreenExitConfirmMs) {
            context.fullscreenSuppressed = false;
            context.fullscreenFalseSince = 0;
        }
        return context.fullscreenSuppressed;
    }

    context.fullscreenFalseSince = 0;
    return false;
}
