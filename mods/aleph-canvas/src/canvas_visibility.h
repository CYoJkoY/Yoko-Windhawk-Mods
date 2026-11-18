#pragma once

#include "cards.h"
#include "context.h"
#include "layout_store.h"
#include "platform.h"
#include "resources.h"
#include "thumbnails.h"
#include "types.h"

inline RECT ReadVirtualScreen() {
    const int left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    return RECT{left, top, left + width, top + height};
}

inline void PlaceCanvasWindows() {
    context.virtualScreen = ReadVirtualScreen();
    const HWND windows[2] = {context.hostWindow, context.hudWindow};
    for (const HWND window : windows) {
        if (window == nullptr) {
            continue;
        }
        SetWindowPos(window, HWND_TOPMOST, context.virtualScreen.left,
                     context.virtualScreen.top, VirtualScreenWidth(),
                     VirtualScreenHeight(), SWP_NOACTIVATE);
    }
    ReleaseHudSurface();
    context.sceneDirty = true;
    context.scanRequested = true;
}

inline void HideCanvasWindows() {
    context.visible = false;
    if (GetCapture() == context.hostWindow) {
        ReleaseCapture();
    }
    if (context.hudWindow != nullptr) {
        ShowWindow(context.hudWindow, SW_HIDE);
    }
    if (context.hostWindow != nullptr) {
        ShowWindow(context.hostWindow, SW_HIDE);
    }
    ReleaseAllThumbnails();
    context.dismissedFingerprints.clear();
    context.panning = false;
    context.draggingMinimap = false;
    context.draggedCard = -1;
    context.diving = false;
    context.hasGuideVertical = false;
    context.hasGuideHorizontal = false;
    context.hoverCard = -1;
    context.minimapHover = false;
    context.camera.snapToTarget();
    context.sceneDirty = true;
}

inline void ShowCanvasWindows() {
    if (context.visible) {
        return;
    }
    if (UpdateChromeDpi()) {
        ReleaseTextFormats();
    }
    PlaceCanvasWindows();
    if (!context.layoutLoaded) {
        LoadLayout();
        context.layoutLoaded = true;
    }
    context.visible = true;
    context.scanRequested = true;
    context.lastFrameTick = GetTickCount();
    SetForegroundWindow(context.hostWindow);
    ShowWindow(context.hostWindow, SW_SHOWNOACTIVATE);
    ShowWindow(context.hudWindow, SW_SHOWNOACTIVATE);
    SetForegroundWindow(context.hostWindow);
    SetFocus(context.hostWindow);
    context.sceneDirty = true;
}

inline void ToggleCanvas() {
    if (context.visible) {
        HideCanvasWindows();
        return;
    }
    ShowCanvasWindows();
}
