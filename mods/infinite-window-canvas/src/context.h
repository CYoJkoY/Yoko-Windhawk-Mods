#pragma once

#include "platform.h"
#include "types.h"

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
    DWORD lastReconcile = 0;

    int edgeDistance = 48;
    int dwellTime = 350;
    int maxSpeed = 900;
    int easing = 35;
    bool constrainBounds = true;
    UINT resetKey = 'R';
    UINT resetModifiers = MOD_CONTROL | MOD_ALT | MOD_WIN;
    std::vector<std::wstring> processBlacklist;
};

inline ModContext context;
