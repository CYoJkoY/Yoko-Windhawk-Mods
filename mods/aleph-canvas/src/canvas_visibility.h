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

inline void CALLBACK OnWindowDestroyed(HWINEVENTHOOK, DWORD, HWND window,
                                       LONG objectId, LONG childId, DWORD,
                                       DWORD) {
    if (objectId != OBJID_WINDOW || childId != CHILDID_SELF) {
        return;
    }
    for (Card& card : context.cards) {
        if (card.hwnd == window) {
            card.destroyed = true;
            card.thumbnail.reset();
            context.scanRequested = true;
            context.sceneDirty = true;
        }
    }
}

inline void InstallDestroyHook() {
    if (context.destroyHook != nullptr) {
        return;
    }
    context.destroyHook = SetWinEventHook(
        EVENT_OBJECT_DESTROY, EVENT_OBJECT_DESTROY, nullptr, OnWindowDestroyed,
        0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    if (context.destroyHook == nullptr) {
        Wh_Log(L"Aleph: SetWinEventHook failed: %lu", GetLastError());
    }
}

inline void UninstallDestroyHook() {
    if (context.destroyHook == nullptr) {
        return;
    }
    UnhookWinEvent(context.destroyHook);
    context.destroyHook = nullptr;
}

inline void HideCanvasWindows() {
    context.visible = false;
    UninstallDestroyHook();
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
    InstallDestroyHook();
}

inline void ToggleCanvas() {
    if (context.visible) {
        HideCanvasWindows();
        return;
    }
    ShowCanvasWindows();
}
