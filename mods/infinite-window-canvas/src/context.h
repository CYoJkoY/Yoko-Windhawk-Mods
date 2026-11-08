#pragma once

#include "platform.h"
#include "types.h"

// Declared defaults. They mirror the settings block in metadata.h so a missing
// key still produces a usable canvas instead of a zero-speed one.
inline constexpr int kDefaultEdgeDistance = 48;
inline constexpr int kDefaultDwellTime = 250;
inline constexpr int kDefaultMaxSpeed = 1800;
inline constexpr int kDefaultEasing = 20;
inline constexpr bool kDefaultConstrainBounds = false;

struct ModContext {
    HANDLE stopEvent = nullptr;
    HANDLE threadHandle = nullptr;
    HWND controlWindow = nullptr;
    HANDLE toolMutex = nullptr;
    bool isLauncher = false;
    std::atomic_bool settingsDirty{false};

    HMONITOR monitor = nullptr;
    RECT monitorRect = {};
    std::vector<ManagedWindow> windows;
    POINT canvasOffset = {};
    bool layoutReady = false;
    bool edgeActive = false;
    bool panning = false;
    DWORD edgeSince = 0;
    DWORD lastTick = 0;
    bool resetRequested = false;
    bool pickerOpen = false;
    DWORD lastReconcile = 0;

    int edgeDistance = kDefaultEdgeDistance;
    int dwellTime = kDefaultDwellTime;
    int maxSpeed = kDefaultMaxSpeed;
    int easing = kDefaultEasing;
    bool constrainBounds = kDefaultConstrainBounds;
    UINT resetKey = 'R';
    UINT resetModifiers = MOD_CONTROL | MOD_ALT | MOD_WIN;
    bool jumpEnabled = true;
    UINT jumpKey = 'Q';
    UINT jumpModifiers = MOD_CONTROL | MOD_ALT | MOD_WIN;
    std::vector<std::wstring> processBlacklist;
};

inline ModContext context;
