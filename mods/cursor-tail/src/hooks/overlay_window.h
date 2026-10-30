#pragma once

// Overlay window procedure and frame timer helpers.

#include "../platform.h"
#include "../context.h"
#include "../frame.h"
#include "../interaction.h"
#include "../input.h"
#include "../ornaments.h"
#include "../backbuffer.h"
#include "../color.h"
#include "../math_utils.h"
#include "../forward.h"
#include "../types.h"

LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case kSettingsChangedMessage:
        context.settingsDirty.store(true, std::memory_order_release);
        return 0;
    case WM_INPUT: {
        RAWINPUT rawInput = {};
        UINT rawInputSize = sizeof(rawInput);
        const UINT readResult = GetRawInputData(
            reinterpret_cast<HRAWINPUT>(lParam),
            RID_INPUT,
            &rawInput,
            &rawInputSize,
            sizeof(RAWINPUTHEADER));
        if (readResult != static_cast<UINT>(-1)
            && rawInput.header.dwType == RIM_TYPEMOUSE) {
            const RAWMOUSE& mouse = rawInput.data.mouse;
            const USHORT buttons = mouse.usButtonFlags;
            const USHORT clickDownMask = RI_MOUSE_LEFT_BUTTON_DOWN
                | RI_MOUSE_RIGHT_BUTTON_DOWN
                | RI_MOUSE_MIDDLE_BUTTON_DOWN;
            const bool clickDown = (buttons & clickDownMask) != 0;
            const bool wheelDown = (buttons & (RI_MOUSE_WHEEL | RI_MOUSE_HWHEEL)) != 0;
            const bool clickEvent = context.clickFeedbackEnabled && clickDown;
            const bool wheelEvent = context.wheelFeedbackEnabled && wheelDown;
            if (clickEvent || wheelEvent) {
                POINT interactionPoint = {};
                if (GetCursorPos(&interactionPoint)) {
                    if (context.clickFeedbackEnabled) {
                        if (buttons & RI_MOUSE_LEFT_BUTTON_DOWN) {
                            AddRipple(interactionPoint, context.leftClickRGB);
                        }
                        if (buttons & RI_MOUSE_RIGHT_BUTTON_DOWN) {
                            AddRipple(interactionPoint, context.rightClickRGB);
                        }
                        if (buttons & RI_MOUSE_MIDDLE_BUTTON_DOWN) {
                            AddRipple(interactionPoint, context.middleClickRGB);
                        }
                    }
                    if (wheelEvent) {
                        const SHORT wheelDelta = static_cast<SHORT>(mouse.usButtonData);
                        if (wheelDelta != 0) {
                            const bool horizontalWheel = (buttons & RI_MOUSE_HWHEEL) != 0;
                            const bool shiftPressed = !horizontalWheel
                                && (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
                            const RippleKind kind = horizontalWheel
                                ? (wheelDelta > 0
                                    ? RippleKind::WheelRight
                                    : RippleKind::WheelLeft)
                                : shiftPressed
                                    ? (wheelDelta > 0
                                        ? RippleKind::WheelLeft
                                        : RippleKind::WheelRight)
                                    : (wheelDelta > 0
                                        ? RippleKind::WheelUp
                                        : RippleKind::WheelDown);
                            AddRipple(
                                interactionPoint,
                                wheelDelta > 0 ? context.wheelUpRGB : context.wheelDownRGB,
                                kind);
                        }
                    }
                }
            }

            // Pendant reactions do not depend on the ripple feedback settings.
            if (context.ornamentEnabled && (clickDown || wheelDown)) {
                const LONGLONG reactionQpc = QpcNow();
                if (clickDown) {
                    QueueOrnamentImpulse(-context.ornamentClickImpulse, reactionQpc);
                }
                if ((buttons & RI_MOUSE_WHEEL) != 0) {
                    const SHORT wheelDelta = static_cast<SHORT>(mouse.usButtonData);
                    if (wheelDelta != 0) {
                        QueueOrnamentImpulse(
                            wheelDelta > 0
                                ? -context.ornamentWheelImpulse
                                : context.ornamentWheelImpulse,
                            reactionQpc);
                    }
                }
            }

            const bool moved = mouse.lLastX != 0
                || mouse.lLastY != 0
                || (mouse.usFlags & MOUSE_MOVE_ABSOLUTE) != 0;
            if (moved) {
                context.rawMouseMovementPending = true;
                const LONGLONG currentQpc = QpcNow();
                const LONGLONG wakeInterval = std::max<LONGLONG>(
                    1, context.qpcFrequency * kInputWakeThrottleMs / 1000);
                if (!context.rawInputWakePending
                    && (context.lastRawInputWakeQpc == 0
                        || currentQpc - context.lastRawInputWakeQpc >= wakeInterval)) {
                    context.lastRawInputWakeQpc = currentQpc;
                    context.rawInputWakePending = true;
                    if (context.inputWakeEvent) SetEvent(context.inputWakeEvent);
                }
            }
        }
        // Windows requires DefWindowProc for foreground RIM_INPUT cleanup.
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    case WM_DISPLAYCHANGE:
        ReleaseBackbuffer();
        ReleaseBackgroundSampler();
        ReleaseScreenDC();
        context.lastFullscreenStrongCheck = 0;
        context.nextSampleDeadline = 0;
        context.cursorMonitor = nullptr;
        context.cursorDpi = 96;
        context.currentDpiScale = 1.0f;
        context.ornamentPhysicsInitialized = false;
        ResetAutoColorSamplingCache();
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}
static bool ArmFrameTimer(HANDLE timer, LONGLONG deadline, LONGLONG frequency) {
    const LONGLONG now = QpcNow();
    LONGLONG ticks = deadline - now;
    if (ticks < 1) ticks = 1;
    const LONGLONG due100ns = std::max<LONGLONG>(1, ticks * 10000000LL / frequency);
    LARGE_INTEGER due = {};
    due.QuadPart = -due100ns;
    return SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE) != FALSE;
}

static LONGLONG FrameIntervalTicks(LONGLONG frequency, int frameRate) {
    return std::max<LONGLONG>(1, frequency / frameRate);
}
