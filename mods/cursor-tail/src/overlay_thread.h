#pragma once

// The dedicated overlay worker thread.

#include "platform.h"
#include "context.h"
#include "hooks/overlay_window.h"
#include "frame.h"
#include "backbuffer.h"
#include "fullscreen.h"
#include "input.h"
#include "interaction.h"
#include "ornaments.h"
#include "overlay.h"
#include "math_utils.h"
#include "forward.h"
#include "dpi.h"
#include "color.h"
#include "types.h"
#include "history.h"
#include "settings.h"

DWORD WINAPI OverlayThreadProc(LPVOID) {
    const HRESULT coResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(coResult)) return 0;
    LARGE_INTEGER qpcFrequency = {};
    if (!QueryPerformanceFrequency(&qpcFrequency) || qpcFrequency.QuadPart <= 0) {
        CoUninitialize();
        return 0;
    }
    context.qpcFrequency = qpcFrequency.QuadPart;
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &context.d2dFactory);
    if (FAILED(hr) || !context.d2dFactory) {
        CoUninitialize();
        return 0;
    }

    HINSTANCE instance = GetModuleHandle(nullptr);
    const wchar_t className[] = L"CursorTailOverlayWindowClass";
    WNDCLASSW windowClass = {};
    windowClass.lpfnWndProc = OverlayWndProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = className;
    const ATOM atom = RegisterClassW(&windowClass);
    if (!atom) {
        context.d2dFactory->Release();
        context.d2dFactory = nullptr;
        CoUninitialize();
        return 0;
    }

    HWND hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        className,
        L"CursorTailOverlay",
        WS_POPUP,
        0,
        0,
        1,
        1,
        nullptr,
        nullptr,
        instance,
        nullptr);
    if (!hwnd) {
        UnregisterClassW(className, instance);
        context.d2dFactory->Release();
        context.d2dFactory = nullptr;
        CoUninitialize();
        return 0;
    }

    context.overlayHwnd.store(hwnd, std::memory_order_release);
    ApplyOverlayCaptureAffinity();
    HideOverlay();
    POINT initialCursorPosition = {};
    if (GetCursorPos(&initialCursorPosition)) {
        UpdateCursorMonitorDpi(
            MonitorFromPoint(initialCursorPosition, MONITOR_DEFAULTTONEAREST));
        context.lastPos = initialCursorPosition;
    }
    context.lastSampleQpc = QpcNow();
    context.fullscreenForegroundWindow = nullptr;
    context.lastFullscreenStrongCheck = 0;
    context.fullscreenStrongSignal = false;
    context.fullscreenSuppressed = false;
    context.fullscreenFalseSince = 0;
    if (context.settingsDirty.exchange(false, std::memory_order_acq_rel)) LoadSettings();

    HANDLE frameTimer = CreateWaitableTimerExW(
        nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!frameTimer) frameTimer = CreateWaitableTimerW(nullptr, FALSE, nullptr);
    if (!frameTimer) {
        DestroyWindow(hwnd);
        context.overlayHwnd.store(nullptr, std::memory_order_release);
        UnregisterClassW(className, instance);
        context.d2dFactory->Release();
        context.d2dFactory = nullptr;
        CoUninitialize();
        return 0;
    }

    context.rawInputRegistered = RegisterMouseRawInput(hwnd, true);
    context.foregroundEventHook = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND,
        EVENT_SYSTEM_FOREGROUND,
        nullptr,
        ForegroundChanged,
        0,
        0,
        WINEVENT_OUTOFCONTEXT);
    if (!context.foregroundEventHook) {
        Wh_Log(L"SetWinEventHook(EVENT_SYSTEM_FOREGROUND) failed: %lu", GetLastError());
    }
    RefreshForegroundInputState();
    EnsureOrnamentAssetsLoaded();
    SmearFrame(hwnd, GetTickCount(), true);

    const LONGLONG simulationTicks =
        FrameIntervalTicks(qpcFrequency.QuadPart, kTargetFrameRate);
    const LONGLONG activeRenderTicks =
        FrameIntervalTicks(qpcFrequency.QuadPart, kActiveRenderFrameRate);
    const LONGLONG fadeRenderTicks =
        FrameIntervalTicks(qpcFrequency.QuadPart, kFadeRenderFrameRate);
    LONGLONG nextRenderDeadline = QpcNow();
    MSG message = {};
    bool running = true;

    while (running) {
        if (context.settingsDirty.exchange(false, std::memory_order_acq_rel)) {
            LoadSettings();
            EnsureOrnamentAssetsLoaded();
            SmearFrame(hwnd, GetTickCount(), true);
        }

        const LONGLONG currentQpc = QpcNow();
        const bool trailActive = context.isSmearing || context.historyCount >= 2;
        const bool rippleActive = HasActiveRipples(currentQpc) || HasActiveMotionAccents(currentQpc);
        const bool ornamentActive = OrnamentActive();
        const LONGLONG inputWakeInterval = std::max<LONGLONG>(
            1,
            context.qpcFrequency * static_cast<LONGLONG>(kInputWakeThrottleMs) / 1000);
        const bool inputCatchUpNeeded =
            context.rawMouseMovementPending && !context.rawInputWakePending && !ornamentActive;
        const bool sampleTimerNeeded =
            trailActive || rippleActive || ornamentActive || context.pollingFallback || inputCatchUpNeeded;
        bool trimTimerNeeded = false;

        LONGLONG timerDeadline = 0;
        if (sampleTimerNeeded) {
            if (inputCatchUpNeeded && !trailActive && !rippleActive && !ornamentActive && !context.pollingFallback) {
                // If the input wake was coalesced by the throttle, schedule one
                // bounded wake rather than waiting indefinitely and losing the
                // final sub-interval of a short movement burst.
                context.nextSampleDeadline = std::max(
                    currentQpc + 1,
                    context.lastRawInputWakeQpc + inputWakeInterval);
            } else if (context.nextSampleDeadline <= currentQpc) {
                const LONGLONG interval = (trailActive || rippleActive || ornamentActive)
                    ? simulationTicks
                    : GetFallbackPollIntervalTicks();
                context.nextSampleDeadline = currentQpc + interval;
            }
            timerDeadline = context.nextSampleDeadline;
        } else if (context.backBufferDc && context.backBufferBitmap) {
            MaybeReleaseIdleBackbuffer(GetTickCount(), currentQpc);
            if (context.backBufferDc && context.backBufferBitmap) {
                const DWORD nowTicks = GetTickCount();
                const DWORD elapsed = nowTicks - context.backbufferIdleSince;
                const DWORD remaining = elapsed >= kBackbufferIdleReleaseDelayMs
                    ? 1
                    : kBackbufferIdleReleaseDelayMs - elapsed;
                timerDeadline = QpcNow()
                    + std::max<LONGLONG>(
                        1,
                        context.qpcFrequency * static_cast<LONGLONG>(remaining) / 1000);
                trimTimerNeeded = true;
            }
        }

        const bool timerNeeded = sampleTimerNeeded || trimTimerNeeded;
        if (timerNeeded) {
            if (!ArmFrameTimer(frameTimer, timerDeadline, qpcFrequency.QuadPart)) break;
        } else {
            CancelWaitableTimer(frameTimer);
        }

        HANDLE handles[] = {context.stopEvent, context.inputWakeEvent, frameTimer};
        const DWORD handleCount = timerNeeded ? 3 : 2;
        const DWORD waitResult = MsgWaitForMultipleObjectsEx(
            handleCount,
            handles,
            INFINITE,
            QS_ALLINPUT,
            MWMO_INPUTAVAILABLE);

        if (waitResult == WAIT_OBJECT_0) {
            running = false;
            continue;
        }

        if (waitResult == WAIT_OBJECT_0 + 1) {
            context.rawInputWakePending = false;
            const bool foregroundChanged = context.foregroundChangePending;
            const bool interactionTriggered = context.interactionPending;
            const bool rawMovement = context.rawMouseMovementPending;
            context.interactionPending = false;

            if (foregroundChanged) {
                RefreshForegroundInputState();
                // Recheck the new foreground immediately. UpdateFullscreenState
                // retains normal same-monitor exit hysteresis but releases
                // suppression for the desktop shell or a different cursor monitor.
                context.fullscreenForegroundWindow = nullptr;
                context.lastFullscreenStrongCheck = 0;
            }

            if (foregroundChanged || interactionTriggered) {
                context.rawMouseMovementPending = false;
                const LONGLONG eventQpc = QpcNow();
                const bool forceRender = interactionTriggered;
                const bool rendered = SmearFrame(hwnd, GetTickCount(), forceRender);
                const bool activeAfterEvent =
                    context.isSmearing || context.historyCount >= 2
                    || HasActiveRipples(QpcNow())
                    || HasActiveMotionAccents(QpcNow())
                    || OrnamentActive();
                if (activeAfterEvent) {
                    const bool fastActive = context.isSmearing || context.historyCount >= 2 || OrnamentActive();
                    nextRenderDeadline = eventQpc
                        + (fastActive
                            ? activeRenderTicks
                            : fadeRenderTicks);
                    context.nextSampleDeadline = eventQpc + simulationTicks;
                } else {
                    context.nextSampleDeadline = context.pollingFallback
                        ? eventQpc + GetFallbackPollIntervalTicks()
                        : 0;
                }
                (void)rendered;
            } else if (rawMovement) {
                context.rawMouseMovementPending = false;
                const LONGLONG movementQpc = QpcNow();
                const bool activeBeforeMovement =
                    context.isSmearing || context.historyCount >= 2
                    || HasActiveRipples(movementQpc)
                    || HasActiveMotionAccents(movementQpc);

                if (!activeBeforeMovement && !context.pollingFallback) {
                    // Bound the first post-idle interval instead of dividing by
                    // the entire idle duration. This retains the first movement
                    // burst even when no second hardware report follows it.
                    const LONGLONG initialSampleTicks = std::max<LONGLONG>(
                        1,
                        context.qpcFrequency * static_cast<LONGLONG>(kInputWakeThrottleMs) / 2000);
                    context.lastSampleQpc = movementQpc - initialSampleTicks;
                    const bool renderFrame = movementQpc >= nextRenderDeadline;
                    SmearFrame(hwnd, GetTickCount(), renderFrame);
                    const LONGLONG sampledQpc = QpcNow();
                    const bool activeAfterMovement =
                        context.isSmearing || context.historyCount >= 2
                        || HasActiveRipples(sampledQpc)
                        || HasActiveMotionAccents(sampledQpc)
                        || OrnamentActive();
                    context.nextSampleDeadline = sampledQpc
                        + (activeAfterMovement ? simulationTicks : 0);
                    if (activeAfterMovement && (renderFrame || sampledQpc >= nextRenderDeadline)) {
                        nextRenderDeadline = sampledQpc
                            + ((context.isSmearing || context.historyCount >= 2 || OrnamentActive())
                                ? activeRenderTicks
                                : fadeRenderTicks);
                    }
                } else if (movementQpc >= context.nextSampleDeadline || !activeBeforeMovement) {
                    const bool renderFrame = movementQpc >= nextRenderDeadline;
                    SmearFrame(hwnd, GetTickCount(), renderFrame);
                    const LONGLONG sampledQpc = QpcNow();
                    const bool activeAfterMovement =
                        context.isSmearing || context.historyCount >= 2
                        || HasActiveRipples(sampledQpc)
                        || HasActiveMotionAccents(sampledQpc)
                        || OrnamentActive();
                    context.nextSampleDeadline = sampledQpc
                        + (activeAfterMovement
                            ? simulationTicks
                            : (context.pollingFallback ? GetFallbackPollIntervalTicks() : 0));
                    if (activeAfterMovement && (renderFrame || sampledQpc >= nextRenderDeadline)) {
                        nextRenderDeadline = sampledQpc
                            + ((context.isSmearing || context.historyCount >= 2 || OrnamentActive())
                                ? activeRenderTicks
                                : fadeRenderTicks);
                    }
                }
            }
            continue;
        }

        if (timerNeeded && waitResult == WAIT_OBJECT_0 + 2) {
            if (sampleTimerNeeded) {
                const LONGLONG sampleNow = QpcNow();
                const bool interactionActiveBeforeFrame =
                    HasActiveRipples(sampleNow)
                    || HasActiveMotionAccents(sampleNow)
                    || OrnamentActive();
                const bool renderFrame =
                    (context.isSmearing || context.historyCount >= 2 || interactionActiveBeforeFrame || OrnamentActive())
                    && sampleNow >= nextRenderDeadline;
                const bool rendered = SmearFrame(hwnd, GetTickCount(), renderFrame);
                context.rawMouseMovementPending = false;

                const LONGLONG sampledQpc = QpcNow();
                const bool interactionActive =
                    HasActiveRipples(sampledQpc)
                    || HasActiveMotionAccents(sampledQpc)
                    || OrnamentActive();
                const bool active = context.isSmearing || OrnamentActive();
                const bool fading = !active && context.historyCount >= 2;
                if (active || fading || interactionActive) {
                    const LONGLONG interval = (active || interactionActive)
                        ? activeRenderTicks
                        : fadeRenderTicks;
                    if (rendered || sampledQpc >= nextRenderDeadline) {
                        nextRenderDeadline = sampledQpc + interval;
                    }
                    context.nextSampleDeadline = sampledQpc + simulationTicks;
                } else if (context.pollingFallback) {
                    context.nextSampleDeadline = sampledQpc + GetFallbackPollIntervalTicks();
                } else {
                    context.nextSampleDeadline = 0;
                }
            } else if (trimTimerNeeded) {
                MaybeReleaseIdleBackbuffer(GetTickCount(), QpcNow());
                context.nextSampleDeadline = 0;
            }
            continue;
        }

        if (waitResult == WAIT_OBJECT_0 + handleCount) {
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                if (message.message == WM_QUIT) {
                    running = false;
                    break;
                }
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            continue;
        }

        break;
    }

    CancelWaitableTimer(frameTimer);
    CloseHandle(frameTimer);
    if (context.foregroundEventHook) {
        if (!UnhookWinEvent(context.foregroundEventHook)) {
            Wh_Log(L"UnhookWinEvent failed: %lu", GetLastError());
        }
        context.foregroundEventHook = nullptr;
    }
    if (context.rawInputRegistered && hwnd) {
        RegisterMouseRawInput(hwnd, false);
        context.rawInputRegistered = false;
    }
    ClearRipples();
    HideOverlay();
    ReleaseOrnamentAssets();
    ReleaseBackbuffer();
    ReleaseBackgroundSampler();
    ReleaseScreenDC();
    if (context.d2dFactory) {
        context.d2dFactory->Release();
        context.d2dFactory = nullptr;
    }
    context.overlayHwnd.store(nullptr, std::memory_order_release);
    DestroyWindow(hwnd);
    UnregisterClassW(className, instance);
    CoUninitialize();
    return 0;
}
