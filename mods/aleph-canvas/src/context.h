#pragma once

// The single mutable canvas state owned by the worker thread.

#include "platform.h"
#include "constants.h"
#include "types.h"

struct CanvasContext {
    // Tool mod process ownership.
    bool isToolModProcessLauncher = false;
    HANDLE toolModProcessMutex = nullptr;

    // Worker thread plumbing.
    HANDLE workerThread = nullptr;
    HANDLE stopEvent = nullptr;
    HANDLE settingsEvent = nullptr;
    HANDLE readyEvent = nullptr;
    bool startupSucceeded = false;
    DWORD workerThreadId = 0;
    std::atomic_bool settingsDirty{false};
    HWINEVENTHOOK destroyHook = nullptr;
    UINT hotkeyModifiers = 0;
    UINT hotkeyKey = 0;
    bool hotkeyRegistered = false;
    UINT hotkeyId = kToggleHotkeyId;

    // Windows. The host carries the DWM thumbnails and the grid; the HUD is a
    // click-through layered window drawn above it.
    HWND hostWindow = nullptr;
    HWND hudWindow = nullptr;
    bool visible = false;
    RECT virtualScreen = {};
    int chromeDpi = 96;
    HMONITOR chromeMonitor = nullptr;

    // Cards and camera.
    std::vector<Card> cards;
    Camera camera{};
    bool cameraMotion = false;
    DWORD lastRescanTick = 0;
    DWORD lastFrameTick = 0;
    bool layoutLoaded = false;
    bool cameraInitialized = false;
    bool layoutDirty = false;
    DWORD layoutDirtyTick = 0;
    bool scanRequested = false;
    bool sceneDirty = true;

    // Interaction state.
    bool panning = false;
    POINT panAnchorCursor = {};
    Vec2 panAnchorWorld = {};
    int draggedCard = -1;
    POINT dragGrabOffset = {};
    POINT dragCursor = {};
    int hoverCard = -1;
    int selectedCard = -1;
    CardHeaderAction hoverHeaderAction = CardHeaderAction::None;
    bool draggingMinimap = false;
    bool minimapHover = false;
    RectF minimapRect = {};
    RectF guideVertical = {};
    RectF guideHorizontal = {};
    bool hasGuideVertical = false;
    bool hasGuideHorizontal = false;

    // Dive-in transition.
    bool diving = false;
    DWORD diveStartTick = 0;
    int diveCard = -1;
    Camera diveStart{};
    Camera diveTarget{};

    // Runtime resources.
    ID2D1Factory* d2dFactory = nullptr;
    ID2D1DCRenderTarget* hudTarget = nullptr;
    ID2D1SolidColorBrush* hudBrush = nullptr;
    IDWriteFactory* writeFactory = nullptr;
    IDWriteTextFormat* titleFormat = nullptr;
    IDWriteTextFormat* badgeFormat = nullptr;
    IWICImagingFactory* wicFactory = nullptr;
    HDC hudDc = nullptr;
    HBITMAP hudBitmap = nullptr;
    HGDIOBJ hudOriginalBitmap = nullptr;
    int hudWidth = 0;
    int hudHeight = 0;
    std::unordered_map<std::wstring, ID2D1Bitmap*> iconCache;
    std::unordered_map<std::wstring, DWORD> iconRetryTick;
    std::unordered_set<std::wstring> dismissedFingerprints;

    // Settings and persisted layout.
    CanvasSettings settings{};
    LayoutSnapshot layout{};
};

extern CanvasContext context;
