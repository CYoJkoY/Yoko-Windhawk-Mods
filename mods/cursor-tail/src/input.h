#pragma once

// Raw input registration, integrity levels and polling fallback.

#include "platform.h"
#include "context.h"
#include "overlay.h"
#include "forward.h"

static bool RegisterMouseRawInput(HWND hwnd, bool enable) {
    RAWINPUTDEVICE mouse = {};
    mouse.usUsagePage = 0x01;  // HID_USAGE_PAGE_GENERIC
    mouse.usUsage = 0x02;      // HID_USAGE_GENERIC_MOUSE
    mouse.dwFlags = enable ? RIDEV_INPUTSINK : RIDEV_REMOVE;
    mouse.hwndTarget = enable ? hwnd : nullptr;

    if (!RegisterRawInputDevices(&mouse, 1, sizeof(mouse))) {
        Wh_Log(L"RegisterRawInputDevices(%d) failed: %lu", enable, GetLastError());
        return false;
    }
    return true;
}

static bool ReadTokenIntegrityRid(HANDLE tokenHandle, DWORD& integrityRid) {
    DWORD requiredSize = 0;
    if (GetTokenInformation(
            tokenHandle,
            TokenIntegrityLevel,
            nullptr,
            0,
            &requiredSize)
        || GetLastError() != ERROR_INSUFFICIENT_BUFFER
        || requiredSize < sizeof(TOKEN_MANDATORY_LABEL)) {
        return false;
    }

    std::vector<BYTE> tokenInfo(requiredSize);
    if (!GetTokenInformation(
            tokenHandle,
            TokenIntegrityLevel,
            tokenInfo.data(),
            requiredSize,
            &requiredSize)) {
        return false;
    }

    const auto* mandatoryLabel =
        reinterpret_cast<const TOKEN_MANDATORY_LABEL*>(tokenInfo.data());
    PSID integritySid = mandatoryLabel->Label.Sid;
    if (!integritySid || !IsValidSid(integritySid)) return false;

    PUCHAR subAuthorityCount = GetSidSubAuthorityCount(integritySid);
    if (!subAuthorityCount || *subAuthorityCount == 0) return false;
    PDWORD ridValue = GetSidSubAuthority(
        integritySid,
        static_cast<DWORD>(*subAuthorityCount - 1));
    if (!ridValue) return false;

    integrityRid = *ridValue;
    return true;
}

static void CacheCurrentIntegrityLevel() {
    if (context.currentIntegrityLevelAttempted) return;
    context.currentIntegrityLevelAttempted = true;

    HANDLE tokenHandle = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tokenHandle)) {
        Wh_Log(L"OpenProcessToken(current process) failed: %lu", GetLastError());
        return;
    }

    context.currentIntegrityLevelKnown =
        ReadTokenIntegrityRid(tokenHandle, context.currentIntegrityLevelRid);
    CloseHandle(tokenHandle);
    if (!context.currentIntegrityLevelKnown) {
        Wh_Log(L"Unable to read current process token integrity level");
    }
}

// Raw input is normally unavailable while a higher-integrity window has
// focus. Compare token integrity levels and use bounded sampling when the
// target integrity cannot be established safely.
static bool IsHigherRightsWindow(HWND hwnd) {
    CacheCurrentIntegrityLevel();
    if (!context.currentIntegrityLevelKnown) return true;
    if (!hwnd) return false;

    DWORD processId = 0;
    if (!GetWindowThreadProcessId(hwnd, &processId) || !processId) return false;

    HANDLE processHandle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (!processHandle) return true;

    HANDLE tokenHandle = nullptr;
    if (!OpenProcessToken(processHandle, TOKEN_QUERY, &tokenHandle)) {
        CloseHandle(processHandle);
        return true;
    }

    DWORD foregroundIntegrityRid = 0;
    const bool integrityKnown = ReadTokenIntegrityRid(tokenHandle, foregroundIntegrityRid);
    CloseHandle(tokenHandle);
    CloseHandle(processHandle);
    return !integrityKnown || foregroundIntegrityRid > context.currentIntegrityLevelRid;
}

static void CALLBACK ForegroundChanged(
    HWINEVENTHOOK,
    DWORD,
    HWND,
    LONG,
    LONG,
    DWORD,
    DWORD) {
    context.foregroundChangePending = true;
    if (context.inputWakeEvent) SetEvent(context.inputWakeEvent);
}

static void ResetFallbackPollTracking() {
    POINT point = {};
    context.fallbackCursorPositionValid = GetCursorPos(&point) != FALSE;
    if (context.fallbackCursorPositionValid) context.fallbackCursorPosition = point;
    context.fallbackLastMovementTick = GetTickCount() - kFallbackIdleDelayMs;
}

static LONGLONG GetFallbackPollIntervalTicks() {
    POINT point = {};
    const DWORD now = GetTickCount();
    if (GetCursorPos(&point)) {
        if (!context.fallbackCursorPositionValid) {
            context.fallbackCursorPosition = point;
            context.fallbackCursorPositionValid = true;
            context.fallbackLastMovementTick = now - kFallbackIdleDelayMs;
        } else if (point.x != context.fallbackCursorPosition.x
            || point.y != context.fallbackCursorPosition.y) {
            context.fallbackCursorPosition = point;
            context.fallbackLastMovementTick = now;
        }
    }

    const DWORD intervalMs = now - context.fallbackLastMovementTick < kFallbackIdleDelayMs
        ? kFallbackActivePollIntervalMs
        : kFallbackIdlePollIntervalMs;
    return std::max<LONGLONG>(
        1, context.qpcFrequency * static_cast<LONGLONG>(intervalMs) / 1000);
}

static void RefreshForegroundInputState() {
    const bool wasPollingFallback = context.pollingFallback;
    context.elevatedForeground = IsHigherRightsWindow(GetForegroundWindow());
    context.pollingFallback = !context.rawInputRegistered || !context.foregroundEventHook || context.elevatedForeground;
    if (context.pollingFallback && !wasPollingFallback) ResetFallbackPollTracking();
    context.foregroundChangePending = false;
}
