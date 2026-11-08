#pragma once

#include "platform.h"
#include "context.h"
#include "settings.h"
#include "canvas.h"
#include "picker.h"

static constexpr int kResetHotkeyId = 1;
static constexpr int kJumpHotkeyId = 2;
static constexpr UINT_PTR kCanvasTimerId = 1;

static void RegisterCanvasHotkey() {
    if (!context.controlWindow) return;
    UnregisterHotKey(context.controlWindow, kResetHotkeyId);
    if (!RegisterHotKey(context.controlWindow, kResetHotkeyId,
                        context.resetModifiers | MOD_NOREPEAT, context.resetKey)) {
        Wh_Log(L"RegisterHotKey failed: %lu", GetLastError());
    }
    UnregisterHotKey(context.controlWindow, kJumpHotkeyId);
    if (!context.jumpEnabled) return;
    // A collision with the reset hotkey is a user choice, so a rejected
    // registration only disables the picker instead of failing the mod.
    if (!RegisterHotKey(context.controlWindow, kJumpHotkeyId,
                        context.jumpModifiers | MOD_NOREPEAT, context.jumpKey)) {
        Wh_Log(L"RegisterHotKey quick jump failed: %lu", GetLastError());
    }
}

static void RegisterRawMouseInput() {
    RAWINPUTDEVICE device = {};
    device.usUsagePage = 0x01;
    device.usUsage = 0x02;
    device.dwFlags = RIDEV_INPUTSINK;
    device.hwndTarget = context.controlWindow;
    if (!RegisterRawInputDevices(&device, 1, sizeof(device))) {
        Wh_Log(L"RegisterRawInputDevices failed: %lu", GetLastError());
    }
}

static LRESULT CALLBACK CanvasWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_HOTKEY:
            if (wParam == kResetHotkeyId) context.resetRequested = true;
            if (wParam == kJumpHotkeyId) OpenWindowPicker();
            return 0;
        case WM_TIMER:
            if (wParam == kCanvasTimerId) TickCanvas();
            return 0;
        case WM_INPUT: {
            RAWINPUT input = {};
            UINT size = sizeof(input);
            if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT,
                                &input, &size, sizeof(RAWINPUTHEADER)) == size &&
                input.header.dwType == RIM_TYPEMOUSE &&
                (input.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == 0 &&
                (input.data.mouse.lLastX != 0 || input.data.mouse.lLastY != 0) &&
                context.edgeActive && !context.panning) {
                context.edgeSince = GetTickCount();
            }
            // DefWindowProc performs raw input cleanup for RIM_INPUT messages.
            return DefWindowProcW(hwnd, message, wParam, lParam);
        }
        case WM_DISPLAYCHANGE:
            ResetCanvas();
            context.layoutReady = false;
            return 0;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            UnregisterHotKey(hwnd, kResetHotkeyId);
            UnregisterHotKey(hwnd, kJumpHotkeyId);
            KillTimer(hwnd, kCanvasTimerId);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

static DWORD WINAPI CanvasThreadProc(LPVOID) {
    if (SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE) != S_OK) {
        SetProcessDPIAware();
    }
    WNDCLASSW windowClass = {};
    windowClass.lpfnWndProc = CanvasWndProc;
    windowClass.hInstance = GetModuleHandle(nullptr);
    windowClass.lpszClassName = L"InfiniteWindowCanvasControl";
    if (!RegisterClassW(&windowClass)) return 0;

    context.controlWindow = CreateWindowExW(
        WS_EX_TOOLWINDOW, windowClass.lpszClassName, L"Infinite Window Canvas",
        WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, windowClass.hInstance, nullptr);
    if (!context.controlWindow) {
        UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);
        return 0;
    }
    RegisterCanvasHotkey();
    RegisterRawMouseInput();
    SetTimer(context.controlWindow, kCanvasTimerId, 16, nullptr);
    HANDLE handles[] = {context.stopEvent};
    MSG message = {};
    bool running = true;
    while (running) {
        const DWORD result = MsgWaitForMultipleObjectsEx(1, handles, 16, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        if (result == WAIT_OBJECT_0) break;
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                running = false;
                break;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (context.settingsDirty.exchange(false, std::memory_order_acq_rel)) {
            LoadSettings();
            RegisterCanvasHotkey();
            ReconcileWindows();
        }
    }
    UnregisterHotKey(context.controlWindow, kResetHotkeyId);
    UnregisterHotKey(context.controlWindow, kJumpHotkeyId);
    KillTimer(context.controlWindow, kCanvasTimerId);
    DestroyWindow(context.controlWindow);
    context.controlWindow = nullptr;
    UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);
    return 0;
}
