#pragma once

#include "canvas_visibility.h"
#include "context.h"
#include "interaction.h"
#include "layout_store.h"
#include "render_host.h"
#include "render_hud.h"
#include "settings.h"
#include "thumbnails.h"
#include "windows.h"

static constexpr wchar_t kHostClassName[] = L"AlephCanvasHostWindow";
static constexpr wchar_t kHudClassName[] = L"AlephCanvasHudWindow";

inline void SyncHudZOrder() {
    if (!context.visible || context.hudWindow == nullptr) {
        return;
    }
    SetWindowPos(context.hudWindow, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

inline void RefreshCanvasGeometry() {
    if (UpdateChromeDpi()) {
        ReleaseTextFormats();
    }
    PlaceCanvasWindows();
}

inline LRESULT CALLBACK CanvasWindowProc(HWND window, UINT message,
                                          WPARAM wParam, LPARAM lParam) {
    if (window == context.hudWindow) {
        if (message == WM_NCHITTEST) {
            return HTTRANSPARENT;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }
    switch (message) {
        case WM_PAINT: {
            PAINTSTRUCT paint = {};
            HDC dc = BeginPaint(window, &paint);
            DrawHostScene(dc);
            EndPaint(window, &paint);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_MOUSEMOVE: {
            TRACKMOUSEEVENT tracking = {sizeof(tracking), TME_LEAVE, window, 0};
            TrackMouseEvent(&tracking);
            const POINT cursor = CursorInCanvas();
            HandleMouseMove(cursor);
            return 0;
        }
        case WM_MOUSELEAVE:
            HandleMouseLeave();
            return 0;
        case WM_LBUTTONDOWN:
            HandleLeftButtonDown(CursorInCanvas());
            return 0;
        case WM_LBUTTONDBLCLK:
            BeginDive(HitTestCard(Vec2{static_cast<float>(CursorInCanvas().x),
                                      static_cast<float>(CursorInCanvas().y)}));
            return 0;
        case WM_LBUTTONUP:
            EndPointerDrag();
            return 0;
        case WM_MBUTTONDOWN:
            if (context.diving) return 0;
            StartPan(CursorInCanvas());
            SetCapture(window);
            return 0;
        case WM_MBUTTONUP:
        case WM_CAPTURECHANGED:
            EndPointerDrag();
            return 0;
        case WM_MOUSEWHEEL:
            HandleMouseWheel(wParam, lParam);
            return 0;
        case WM_KEYDOWN:
            HandleKeyDown(wParam);
            return 0;
        case WM_DISPLAYCHANGE:
        case WM_DPICHANGED:
            RefreshCanvasGeometry();
            return 0;
        case WM_WINDOWPOSCHANGED:
            SyncHudZOrder();
            return DefWindowProcW(window, message, wParam, lParam);
        case WM_CLOSE:
            HideCanvasWindows();
            return 0;
        default:
            return DefWindowProcW(window, message, wParam, lParam);
    }
}

inline bool RegisterCanvasWindows(HINSTANCE instance) {
    WNDCLASSEXW hostClass = {};
    hostClass.cbSize = sizeof(hostClass);
    hostClass.lpfnWndProc = CanvasWindowProc;
    hostClass.hInstance = instance;
    hostClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    hostClass.lpszClassName = kHostClassName;
    hostClass.style = CS_DBLCLKS;
    if (RegisterClassExW(&hostClass) == 0) {
        return false;
    }
    WNDCLASSEXW hudClass = hostClass;
    hudClass.lpszClassName = kHudClassName;
    if (RegisterClassExW(&hudClass) == 0) {
        UnregisterClassW(kHostClassName, instance);
        return false;
    }
    context.virtualScreen = ReadVirtualScreen();
    context.hostWindow = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW, kHostClassName, L"Aleph Canvas",
        WS_POPUP, context.virtualScreen.left, context.virtualScreen.top,
        VirtualScreenWidth(), VirtualScreenHeight(), nullptr, nullptr, instance,
        nullptr);
    context.hudWindow = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW |
            WS_EX_NOACTIVATE,
        kHudClassName, L"Aleph Canvas HUD", WS_POPUP,
        context.virtualScreen.left, context.virtualScreen.top,
        VirtualScreenWidth(), VirtualScreenHeight(), nullptr, nullptr, instance,
        nullptr);
    if (context.hostWindow == nullptr || context.hudWindow == nullptr) {
        if (context.hudWindow != nullptr) DestroyWindow(context.hudWindow);
        if (context.hostWindow != nullptr) DestroyWindow(context.hostWindow);
        context.hudWindow = nullptr;
        context.hostWindow = nullptr;
        UnregisterClassW(kHudClassName, instance);
        UnregisterClassW(kHostClassName, instance);
        return false;
    }
    return true;
}

inline bool RegisterCanvasHotkey() {
    UINT modifiers = 0;
    UINT key = 0;
    if (!ParseHotkeyText(context.settings.toggleHotkey, modifiers, key)) {
        Wh_Log(L"Aleph: invalid toggle hotkey");
        return false;
    }
    if (context.hotkeyRegistered && context.hotkeyModifiers == modifiers &&
        context.hotkeyKey == key) {
        return true;
    }
    const UINT nextId = context.hotkeyId == kToggleHotkeyId
                            ? kToggleHotkeyId + 1 : kToggleHotkeyId;
    if (!RegisterHotKey(nullptr, nextId, modifiers | MOD_NOREPEAT, key)) {
        Wh_Log(L"Aleph: RegisterHotKey failed: %lu", GetLastError());
        return false;
    }
    if (context.hotkeyRegistered) {
        UnregisterHotKey(nullptr, context.hotkeyId);
    }
    context.hotkeyId = nextId;
    context.hotkeyModifiers = modifiers;
    context.hotkeyKey = key;
    context.hotkeyRegistered = true;
    return true;
}

inline void CALLBACK OnWindowDestroyed(HWINEVENTHOOK, DWORD, HWND window,
                                       LONG objectId, LONG childId, DWORD, DWORD) {
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

inline void ScanAndMerge(IVirtualDesktopManager* desktopManager, DWORD now) {
    if (!context.visible || context.diving ||
        (!context.scanRequested && now - context.lastRescanTick <
             static_cast<DWORD>(context.settings.rescanIntervalMs))) {
        return;
    }
    const std::vector<ScannedWindow> scanned = ScanWindows(desktopManager);
    context.sceneDirty = MergeScan(scanned) || context.sceneDirty;
    if (HasDueThumbnailRetry(now) || ConsumeDueIconRetries(now)) {
        context.sceneDirty = true;
    }
    if (!context.cameraInitialized) {
        FitAllCards();
        context.camera.snapToTarget();
        context.cameraInitialized = true;
    }
    context.lastRescanTick = now;
    context.scanRequested = false;
}

inline void RenderCanvasFrame() {
    if (!context.visible) {
        return;
    }
    UpdateThumbnails();
    InvalidateRect(context.hostWindow, nullptr, FALSE);
    UpdateWindow(context.hostWindow);
    SubmitHudFrame();
    context.sceneDirty = false;
}

inline void UpdateCameraAndDive(DWORD now) {
    const DWORD elapsed = now - context.lastFrameTick;
    context.lastFrameTick = now;
    const float seconds = static_cast<float>(elapsed) / 1000.0f;
    AdvanceDive(now);
    if (!context.diving) {
        AdvanceCamera(context.camera, seconds);
    }
    context.cameraMotion = !context.camera.settled();
}

inline void ProcessCanvasMessage(const MSG& message) {
    if (message.message == WM_HOTKEY && message.wParam == context.hotkeyId) {
        ToggleCanvas();
        return;
    }
    TranslateMessage(&message);
    DispatchMessageW(&message);
}

inline void CleanupCanvas(HINSTANCE instance) {
    if (context.destroyHook != nullptr) {
        UnhookWinEvent(context.destroyHook);
        context.destroyHook = nullptr;
    }
    UnregisterHotKey(nullptr, context.hotkeyId);
    context.hotkeyRegistered = false;
    HideCanvasWindows();
    ReleaseAllThumbnails();
    ClearCards();
    if (context.hudWindow != nullptr) DestroyWindow(context.hudWindow);
    if (context.hostWindow != nullptr) DestroyWindow(context.hostWindow);
    context.hudWindow = nullptr;
    context.hostWindow = nullptr;
    ReleaseGraphics();
    UnregisterClassW(kHudClassName, instance);
    UnregisterClassW(kHostClassName, instance);
}

inline bool DrainCanvasMessages() {
    MSG message = {};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) {
            return false;
        }
        ProcessCanvasMessage(message);
    }
    return true;
}

inline void ApplyPendingSettings() {
    if (!context.settingsDirty.exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    LoadSettings();
    RegisterCanvasHotkey();
    context.camera.targetZoom = ClampZoom(context.camera.targetZoom);
    context.camera.zoomAnchored = false;
    context.scanRequested = true;
    context.sceneDirty = true;
}

inline DWORD CanvasWaitTimeout(DWORD now) {
    DWORD timeout = INFINITE;
    if (context.visible) {
        const DWORD interval = static_cast<DWORD>(context.settings.rescanIntervalMs);
        const DWORD elapsed = now - context.lastRescanTick;
        timeout = elapsed >= interval ? 0 : interval - elapsed;
    }
    if (context.layoutDirty) {
        const DWORD elapsed = now - context.layoutDirtyTick;
        const DWORD remaining = elapsed >= kLayoutSaveDelayMs
                                    ? 0 : kLayoutSaveDelayMs - elapsed;
        timeout = std::min(timeout, remaining);
    }
    return timeout;
}

inline void AdvanceCanvasFrame(IVirtualDesktopManager* manager, DWORD now) {
    ApplyPendingSettings();
    ScanAndMerge(manager, now);
    UpdateCameraAndDive(now);
    if (context.visible && (context.sceneDirty || context.cameraMotion ||
                            context.diving)) {
        RenderCanvasFrame();
    }
    if (context.diving && now - context.diveStartTick >=
                              static_cast<DWORD>(context.settings.animationMs)) {
        FinishDive();
    }
    FlushLayoutIfDue(now);
}

inline DWORD PacedCanvasTimeout(DWORD now) {
    if (!context.visible || (!context.cameraMotion && !context.diving)) {
        return CanvasWaitTimeout(now);
    }
    const DWORD started = GetTickCount();
    const HRESULT result = DwmFlush();
    return FAILED(result) || GetTickCount() == started ? kFrameIntervalMs : 0;
}

inline void RunCanvasLoop(IVirtualDesktopManager* manager) {
    const HANDLE events[2] = {context.stopEvent, context.settingsEvent};
    while (WaitForSingleObject(context.stopEvent, 0) != WAIT_OBJECT_0) {
        if (!DrainCanvasMessages()) {
            return;
        }
        const DWORD now = GetTickCount();
        AdvanceCanvasFrame(manager, now);
        const DWORD wait = MsgWaitForMultipleObjectsEx(
            2, events, PacedCanvasTimeout(now), QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        if (wait == WAIT_OBJECT_0 || wait == WAIT_FAILED) {
            return;
        }
    }
}

inline DWORD WINAPI CanvasWorkerThread(LPVOID) {
    const DPI_AWARENESS_CONTEXT previousDpi =
        SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(comResult)) {
        if (previousDpi != nullptr) SetThreadDpiAwarenessContext(previousDpi);
        return 1;
    }
    HINSTANCE instance = GetModuleHandleW(nullptr);
    if (!RegisterCanvasWindows(instance)) {
        CoUninitialize();
        if (previousDpi != nullptr) SetThreadDpiAwarenessContext(previousDpi);
        return 1;
    }
    if (!RegisterCanvasHotkey()) {
        CleanupCanvas(instance);
        CoUninitialize();
        if (previousDpi != nullptr) SetThreadDpiAwarenessContext(previousDpi);
        return 1;
    }
    context.destroyHook = SetWinEventHook(
        EVENT_OBJECT_DESTROY, EVENT_OBJECT_DESTROY, nullptr, OnWindowDestroyed,
        0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    if (context.destroyHook == nullptr) {
        CleanupCanvas(instance);
        CoUninitialize();
        if (previousDpi != nullptr) SetThreadDpiAwarenessContext(previousDpi);
        return 1;
    }
    IVirtualDesktopManager* desktopManager = nullptr;
    CoCreateInstance(CLSID_VirtualDesktopManager, nullptr, CLSCTX_INPROC_SERVER,
                    IID_PPV_ARGS(&desktopManager));
    if (context.settings.openOnStart) {
        ToggleCanvas();
    }
    context.startupSucceeded = true;
    SetEvent(context.readyEvent);
    RunCanvasLoop(desktopManager);
    if (desktopManager != nullptr) {
        desktopManager->Release();
    }
    if (context.layoutDirty) {
        SaveLayoutNow();
    }
    CleanupCanvas(instance);
    CoUninitialize();
    if (previousDpi != nullptr) SetThreadDpiAwarenessContext(previousDpi);
    return 0;
}
