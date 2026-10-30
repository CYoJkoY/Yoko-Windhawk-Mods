#pragma once

// Overlay visibility transitions and capture affinity.

#include "platform.h"
#include "context.h"

void HideOverlay() {
    const HWND hwnd = context.overlayHwnd.load();
    if (!hwnd || !IsWindow(hwnd) || !context.windowVisible) return;
    ShowWindow(hwnd, SW_HIDE);
    context.windowVisible = false;
}

void ShowOverlay() {
    const HWND hwnd = context.overlayHwnd.load();
    if (!hwnd || !IsWindow(hwnd) || context.windowVisible) return;
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    context.windowVisible = true;
}
// The overlay only needs to be hidden from screen capture while Auto color mode
// is sampling the screen background, otherwise the sample would pick up the
// trail's own pixels. In Manual mode the overlay stays capturable so the trail
// shows up in screenshots and screen shares.
static void ApplyOverlayCaptureAffinity() {
    const HWND hwnd = context.overlayHwnd.load(std::memory_order_acquire);
    if (!hwnd || !IsWindow(hwnd)) return;
    if (!SetWindowDisplayAffinity(hwnd, context.trailColorMode == 1 ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE)) {
        Wh_Log(L"SetWindowDisplayAffinity failed: %lu", GetLastError());
    }
}
