// ==WindhawkMod==
// @id              cursor-tail
// @name            Cursor Tail
// @description     Adaptive motion-blur trail for the mouse pointer, colored by sampling the cursor image.
// @version         3.4
// @author          CYoJkoY
// @github          https://github.com/CYoJkoY
// @license         MIT
// @include         windhawk.exe
// @compilerOptions -ld2d1 -ldwmapi -lole32 -lgdi32 -lshell32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Cursor Tail
Replaces your standard Windows cursor with a smooth, tapered motion-blur trail
when moving at high speeds. Hardware accelerated via Direct2D.

![Demonstration](https://i.imgur.com/mbiW6QU.gif)

### Features
* **Adaptive Trail Color:** Pick a fixed hex color, or let the mod sample the
  current cursor image and choose the most visible color against the live
  screen background (WCAG contrast validation).
* **Independent Outline Color:** Auto-derive the outer ring color from the
  core color, or pick it manually.
* **Dynamic 2D Ribbon:** Procedurally generates a continuous, overlapping-free
  polygon ribbon with a rounded head baked into the path.
* **Direct2D Rendering:** Buttery smooth sub-pixel anti-aliasing on the GPU.
* **Zero Input Lag:** Draws directly off the hardware cursor coordinates.
* **Aggressive Game Detection:** Multi-signal detection (exclusive D3D
  fullscreen, DWM composition disabled, fullscreen-window heuristic, cursor
  clipping, cursor hiding), re-checked every 100ms and immediately on
  trail-trigger edges. Covers borderless and windowed fullscreen games.
* **Taskbar Friendly:** Retains native edge-detection for auto-hiding taskbars.
* **Dynamic Rendering:** Idles at 0% CPU usage when the mouse is stationary.

### Color Modes
- **Manual** — supply any `#RRGGBB` color for the core.
- **Auto** — the mod reads the cursor bitmap, builds a color histogram, and
  walks it from most-frequent to least-frequent, choosing the first candidate
  whose contrast against the screen background near the cursor meets the
  WCAG AA large-element threshold (3.0). If none qualifies, it falls back to
  white.

### Outline (Ring) Modes
- **Auto** — bright core → black ring, dark core → white ring.
- **Manual** — always uses the specified `#RRGGBB`.

### Auto Sampling Frequency
By default the cursor image is only re-sampled when it actually changes
(switching to I-beam, hand, busy spinner, etc.). Movement does not trigger
re-sampling. Set **Auto Resample Interval** to a non-zero value only if you
want periodic re-evaluation (e.g. when dragging the cursor across very
different backgrounds).

### Game Detection
The trail is suppressed automatically when a fullscreen game is detected.
Signals checked, from strongest to weakest:
1. `SHQueryUserNotificationState` reports D3D fullscreen.
2. DWM composition is disabled (typical of exclusive fullscreen).
3. Foreground window covers the whole monitor (2px tolerance).
4. Cursor is clipped to less than the virtual screen.
5. Cursor is hidden.

Detection runs on a 100ms cadence, drops to 30ms while the pointer is moving
faster than twice the trigger velocity, and runs once immediately on every
trail-trigger edge so no frame of the trail can escape.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- trigger_velocity: 25
  $name: Trigger Velocity
  $description: How fast the mouse needs to move to start the trail (pixels per frame).
- stop_velocity: 10
  $name: Stop Velocity
  $description: Velocity threshold to fade the trail. Must be lower than Trigger Velocity.
- tail_offset_x: 6
  $name: Tail Offset X
  $description: X-axis offset from the cursor hotspot to the head of the ribbon.
- tail_offset_y: 10
  $name: Tail Offset Y
  $description: Y-axis offset from the cursor hotspot to the head of the ribbon.
- tail_length: 10
  $name: Tail Length
  $description: How many historical cursor samples the trail retains (2-64).
- trail_color_mode: 0
  $name: Trail Color Mode
  $description: 0 = Manual hex color. 1 = Auto sample the cursor image and pick a visible color.
- trail_color_manual: "#FFFFFF"
  $name: Manual Trail Color
  $description: Core (inner) trail color in Manual mode. Format "#RRGGBB".
- outline_color_mode: 0
  $name: Outline Color Mode
  $description: 0 = Auto derive from the core luminance. 1 = Manual hex color.
- outline_color_manual: "#000000"
  $name: Manual Outline Color
  $description: Outer ring color when Outline Color Mode is Manual. Format "#RRGGBB".
- auto_resample_interval: 0
  $name: Auto Resample Interval
  $description: How often to re-sample the cursor color in Auto mode (ms). 0 = only when the cursor image changes (recommended). Range 0-10000.
*/
// ==/WindhawkModSettings==

#include <windows.h>
#include <d2d1.h>
#include <dwmapi.h>
#include <math.h>
#include <shellapi.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <unordered_map>
#include <vector>

// -----------------------------------------------------------------------------
// Constants
// -----------------------------------------------------------------------------

constexpr UINT_PTR kTimerId = 1;
constexpr UINT kSettingsChangedMessage = WM_APP + 1;

constexpr float kDefaultTriggerVelocity = 25.0f;
constexpr float kDefaultStopVelocity = 10.0f;

constexpr int kDefaultTailOffsetX = 6;
constexpr int kDefaultTailOffsetY = 10;
constexpr int kDefaultTailLength = 10;

constexpr float kOuterWidth = 10.0f;
constexpr float kCoreWidth = 6.0f;

constexpr float kTrailAlpha = 0.86f;

// Padding around the generated geometry so antialiasing never gets clipped.
constexpr int kRenderPadding = 2;

// Circle-to-bezier magic constant (4/3 * (sqrt(2) - 1)).
constexpr float kArcMagic = 0.5522847498f;

// Ring buffer capacity. tail_length is clamped to this.
constexpr int kMaxTailLength = 64;
constexpr int kHistoryCapacity = kMaxTailLength;

// Timer coalescing tolerance (ms).
constexpr ULONG kTimerCoalescingTolerance = 5;

// Auto color picking.
constexpr int kAutoResampleIntervalDefaultMs = 0;
constexpr int kAutoResampleIntervalMinMs = 0;
constexpr int kAutoResampleIntervalMaxMs = 10000;

// When auto_resample_interval == 0 we re-sample on cursor change only.
// This floor prevents animated cursors (e.g. the busy spinner) from
// triggering a full re-sample on every frame.
constexpr DWORD kMinCursorChangeResampleIntervalMs = 100;

constexpr int kBackgroundSampleRadius = 24;
constexpr int kBackgroundSampleGrid = 5;      // 5x5 grid sampled from the grabbed patch
constexpr float kMinColorContrast = 3.0f;     // WCAG AA large text threshold

constexpr uint8_t kCursorAlphaThreshold = 96; // Ignore nearly-transparent pixels
constexpr uint32_t kFallbackCoreColor = 0x00FFFFFF; // White
constexpr uint32_t kFallbackOuterColor = 0x00000000; // Black

// Game detection cadence.
constexpr DWORD kGameCheckIntervalMs      = 100;  // idle cadence
constexpr DWORD kGameCheckBurstIntervalMs = 30;   // while the pointer is fast
constexpr float kGameCheckBurstVelocityFactor = 2.0f;

// Fullscreen heuristic tolerance, in screen pixels. Covers the few pixels
// by which a borderless DWM window can miss the exact monitor rectangle.
constexpr LONG kFullscreenTolerancePx = 2;

// -----------------------------------------------------------------------------
// Global state
// -----------------------------------------------------------------------------

std::atomic<HWND> g_overlayHwnd{nullptr};
HANDLE g_threadHandle = nullptr;
DWORD g_threadId = 0;

// Fixed-size ring buffer for cursor history. head = index of the newest sample.
POINT g_history[kHistoryCapacity];
int g_historyHead = 0;
int g_historyCount = 0;

POINT g_lastPos = {0, 0};

bool g_isSmearing = false;
int g_lowVelocityFrames = 0;

// -----------------------------------------------------------------------------
// Direct2D
// -----------------------------------------------------------------------------

ID2D1Factory* g_pD2DFactory = nullptr;
ID2D1DCRenderTarget* g_pDCRenderTarget = nullptr;

// Outer ("outline") and core brushes. Their colors are updated every frame
// via SetColor() so we don't have to recreate them when the trail color
// changes.
ID2D1SolidColorBrush* g_pOuterBrush = nullptr;
ID2D1SolidColorBrush* g_pCoreBrush = nullptr;

bool g_dcBound = false;

// -----------------------------------------------------------------------------
// Cached backbuffer
// -----------------------------------------------------------------------------

HDC g_hdcMem = nullptr;
HBITMAP g_hBitmap = nullptr;
HGDIOBJ g_originalBitmap = nullptr;
int g_cachedWidth = 0;
int g_cachedHeight = 0;

// Cached screen DC. Released only on thread shutdown.
HDC g_hdcScreen = nullptr;

// -----------------------------------------------------------------------------
// Settings
// -----------------------------------------------------------------------------

float g_triggerVelocity = kDefaultTriggerVelocity;
float g_stopVelocity = kDefaultStopVelocity;

int g_tailOffsetX = kDefaultTailOffsetX;
int g_tailOffsetY = kDefaultTailOffsetY;
int g_tailLength = kDefaultTailLength;

// Color settings.
int g_trailColorMode = 0;                       // 0 = manual, 1 = auto
uint32_t g_manualColorRGB = 0x00FFFFFF;         // 0x00RRGGBB
int g_outlineColorMode = 0;                     // 0 = auto, 1 = manual
uint32_t g_manualOutlineRGB = 0x00000000;       // 0x00RRGGBB
int g_autoResampleInterval = kAutoResampleIntervalDefaultMs;

// -----------------------------------------------------------------------------
// Current active trail colors (0x00RRGGBB).
// -----------------------------------------------------------------------------

uint32_t g_currentCoreRGB = kFallbackCoreColor;
uint32_t g_currentOuterRGB = kFallbackOuterColor;

// -----------------------------------------------------------------------------
// Render cache
// -----------------------------------------------------------------------------

struct TrailRenderCache {
    std::vector<D2D1_POINT_2F> smoothed;
    std::vector<D2D1_POINT_2F> subdivision;
    std::vector<D2D1_POINT_2F> leftOutline;
    std::vector<D2D1_POINT_2F> rightOutline;
    std::vector<D2D1_POINT_2F> leftCore;
    std::vector<D2D1_POINT_2F> rightCore;

    void ReserveForTailLength(int tailLength) {
        const size_t baseCount = static_cast<size_t>(std::max(tailLength, 2));
        const size_t smoothedCount = baseCount * 4 - 3;

        smoothed.reserve(smoothedCount);
        subdivision.reserve(smoothedCount);
        leftOutline.reserve(smoothedCount);
        rightOutline.reserve(smoothedCount);
        leftCore.reserve(smoothedCount);
        rightCore.reserve(smoothedCount);
    }

    void Clear() {
        smoothed.clear();
        subdivision.clear();
        leftOutline.clear();
        rightOutline.clear();
        leftCore.clear();
        rightCore.clear();
    }
};

TrailRenderCache g_renderCache;

// -----------------------------------------------------------------------------
// Overlay state
// -----------------------------------------------------------------------------

bool g_windowVisible = false;

// -----------------------------------------------------------------------------
// Color utilities
// -----------------------------------------------------------------------------

// WCAG relative luminance, 0..1.
static inline float RgbLuminance(uint8_t r, uint8_t g, uint8_t b) {
    auto f = [](float c) {
        c /= 255.0f;
        return c <= 0.03928f ? c / 12.92f
                             : powf((c + 0.055f) / 1.055f, 2.4f);
    };
    return 0.2126f * f(static_cast<float>(r)) +
           0.7152f * f(static_cast<float>(g)) +
           0.0722f * f(static_cast<float>(b));
}

static inline float RgbLuminance(uint32_t rgb) {
    return RgbLuminance((rgb >> 16) & 0xFF,
                        (rgb >> 8) & 0xFF,
                        rgb & 0xFF);
}

static inline float ContrastRatio(float lum1, float lum2) {
    const float lo = std::min(lum1, lum2);
    const float hi = std::max(lum1, lum2);
    return (hi + 0.05f) / (lo + 0.05f);
}

static inline D2D1_COLOR_F ToColorF(uint32_t rgb, float alpha) {
    return D2D1::ColorF(
        ((rgb >> 16) & 0xFF) / 255.0f,
        ((rgb >> 8) & 0xFF) / 255.0f,
        (rgb & 0xFF) / 255.0f,
        alpha);
}

// Parse "#RRGGBB", "RRGGBB", "0xRRGGBB" -> 0x00RRGGBB.
static bool ParseHexColor(const wchar_t* str, uint32_t& out) {
    if (!str) return false;

    while (*str == L' ' || *str == L'\t') str++;
    if (*str == L'#') str++;
    else if (str[0] == L'0' && (str[1] == L'x' || str[1] == L'X')) str += 2;

    uint32_t value = 0;
    int digits = 0;
    while (digits < 6) {
        wchar_t c = str[digits];
        int d;
        if (c >= L'0' && c <= L'9') d = c - L'0';
        else if (c >= L'a' && c <= L'f') d = c - L'a' + 10;
        else if (c >= L'A' && c <= L'F') d = c - L'A' + 10;
        else break;
        value = (value << 4) | static_cast<uint32_t>(d);
        digits++;
    }

    if (digits != 6) return false;
    out = value;
    return true;
}

// -----------------------------------------------------------------------------
// Utility
// -----------------------------------------------------------------------------

void ReleaseRenderResources() {
    if (g_pCoreBrush) {
        g_pCoreBrush->Release();
        g_pCoreBrush = nullptr;
    }
    if (g_pOuterBrush) {
        g_pOuterBrush->Release();
        g_pOuterBrush = nullptr;
    }
    if (g_pDCRenderTarget) {
        g_pDCRenderTarget->Release();
        g_pDCRenderTarget = nullptr;
    }
    g_dcBound = false;
}

void ReleaseBackbuffer() {
    ReleaseRenderResources();

    if (g_hdcMem) {
        if (g_originalBitmap) {
            SelectObject(g_hdcMem, g_originalBitmap);
            g_originalBitmap = nullptr;
        }
        DeleteDC(g_hdcMem);
        g_hdcMem = nullptr;
    }

    if (g_hBitmap) {
        DeleteObject(g_hBitmap);
        g_hBitmap = nullptr;
    }

    g_cachedWidth = 0;
    g_cachedHeight = 0;
}

HBITMAP Create32BitDIB(int width, int height) {
    if (width <= 0 || height <= 0) return nullptr;

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    return CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
}

HDC GetScreenDC() {
    if (!g_hdcScreen) {
        g_hdcScreen = GetDC(nullptr);
    }
    return g_hdcScreen;
}

void ReleaseScreenDC() {
    if (g_hdcScreen) {
        ReleaseDC(nullptr, g_hdcScreen);
        g_hdcScreen = nullptr;
    }
}

void HideOverlay() {
    HWND hwnd = g_overlayHwnd.load();
    if (!hwnd || !IsWindow(hwnd) || !g_windowVisible)
        return;

    ShowWindow(hwnd, SW_HIDE);
    g_windowVisible = false;
}

void ShowOverlay() {
    HWND hwnd = g_overlayHwnd.load();
    if (!hwnd || !IsWindow(hwnd) || g_windowVisible)
        return;

    ShowWindow(hwnd, SW_SHOWNA);
    g_windowVisible = true;
}

// -----------------------------------------------------------------------------
// History ring buffer
// -----------------------------------------------------------------------------

void HistoryClear() {
    g_historyHead = 0;
    g_historyCount = 0;
}

void HistoryPushFront(const POINT& p) {
    if (g_historyCount == kHistoryCapacity) {
        g_historyCount--;
    }
    g_historyHead = (g_historyHead - 1 + kHistoryCapacity) % kHistoryCapacity;
    g_history[g_historyHead] = p;
    g_historyCount++;
}

void HistoryPopBack() {
    if (g_historyCount > 0) {
        g_historyCount--;
    }
}

const POINT& HistoryAt(int index) {
    return g_history[(g_historyHead + index) % kHistoryCapacity];
}

// -----------------------------------------------------------------------------
// Settings
// -----------------------------------------------------------------------------

void LoadSettings() {
    g_triggerVelocity = static_cast<float>(
        std::clamp(Wh_GetIntSetting(L"trigger_velocity"), 1, 500));

    g_stopVelocity = static_cast<float>(
        std::clamp(Wh_GetIntSetting(L"stop_velocity"), 1, 500));

    g_tailOffsetX = std::clamp(Wh_GetIntSetting(L"tail_offset_x"), -64, 64);
    g_tailOffsetY = std::clamp(Wh_GetIntSetting(L"tail_offset_y"), -64, 64);

    g_tailLength = std::clamp(Wh_GetIntSetting(L"tail_length"), 2, kMaxTailLength);

    if (g_stopVelocity >= g_triggerVelocity) {
        g_stopVelocity = g_triggerVelocity * 0.5f;
        if (g_stopVelocity <= 0.0f) {
            g_stopVelocity = 1.0f;
        }
    }

    // Trail (core) color mode.
    g_trailColorMode = std::clamp(
        Wh_GetIntSetting(L"trail_color_mode"), 0, 1);

    // Manual core color.
    PCWSTR colorStr = Wh_GetStringSetting(L"trail_color_manual");
    uint32_t parsed = 0;
    if (colorStr && ParseHexColor(colorStr, parsed)) {
        g_manualColorRGB = parsed;
    } else {
        g_manualColorRGB = kFallbackCoreColor;
    }
    if (colorStr) Wh_FreeStringSetting(colorStr);

    // Outline color mode.
    g_outlineColorMode = std::clamp(
        Wh_GetIntSetting(L"outline_color_mode"), 0, 1);

    // Manual outline color.
    PCWSTR outlineStr = Wh_GetStringSetting(L"outline_color_manual");
    parsed = 0;
    if (outlineStr && ParseHexColor(outlineStr, parsed)) {
        g_manualOutlineRGB = parsed;
    } else {
        g_manualOutlineRGB = kFallbackOuterColor;
    }
    if (outlineStr) Wh_FreeStringSetting(outlineStr);

    // Auto resample interval. 0 = only on cursor change.
    g_autoResampleInterval = std::clamp(
        Wh_GetIntSetting(L"auto_resample_interval"),
        kAutoResampleIntervalMinMs,
        kAutoResampleIntervalMaxMs);

    g_renderCache.ReserveForTailLength(g_tailLength);
}

// -----------------------------------------------------------------------------
// Game detection
// -----------------------------------------------------------------------------

// Multi-signal fullscreen-game detector.
//
// Order is deliberate: the cheapest and strongest signals are checked first
// so the common cases bail out early. All signals are cheap Win32 calls
// (no GetPixel, no enumeration), so this can run every 100ms without cost.
bool IsGameRunning() {
    HWND hwnd = GetForegroundWindow();

    if (!hwnd || hwnd == GetDesktopWindow()) {
        return false;
    }

    // Shell window (desktop / taskbar host) is never a game.
    // GetShellWindow() is much cheaper than FindWindowW over class names.
    if (hwnd == GetShellWindow()) {
        return false;
    }

    // Signal 1: exclusive D3D fullscreen (the OS itself tells us).
    QUERY_USER_NOTIFICATION_STATE state;
    if (SUCCEEDED(SHQueryUserNotificationState(&state))) {
        if (state == QUNS_RUNNING_D3D_FULL_SCREEN) {
            return true;
        }
    }

    // Signal 2: DWM composition disabled. Almost only true for exclusive
    // fullscreen DirectX apps, and false for borderless-windowed games.
    BOOL dwmEnabled = TRUE;
    if (SUCCEEDED(DwmIsCompositionEnabled(&dwmEnabled)) && !dwmEnabled) {
        return true;
    }

    // The remaining signals all require that the foreground window covers
    // the whole monitor. Verify that first so we can bail out cheaply for
    // normal windowed apps.
    RECT rcApp;
    if (!GetWindowRect(hwnd, &rcApp)) {
        return false;
    }

    HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {sizeof(mi)};
    if (!GetMonitorInfo(hMonitor, &mi)) {
        return false;
    }

    const bool isFullscreen =
        rcApp.left   <= mi.rcMonitor.left   + kFullscreenTolerancePx &&
        rcApp.top    <= mi.rcMonitor.top    + kFullscreenTolerancePx &&
        rcApp.right  >= mi.rcMonitor.right  - kFullscreenTolerancePx &&
        rcApp.bottom >= mi.rcMonitor.bottom - kFullscreenTolerancePx;

    if (!isFullscreen) {
        return false;
    }

    // Signal 3: cursor is clipped to something smaller than the full virtual
    // screen. Games almost always call ClipCursor to lock the pointer.
    RECT rcClip;
    if (GetClipCursor(&rcClip)) {
        const int virtualWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        const int virtualHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);

        if ((rcClip.right - rcClip.left) < virtualWidth ||
            (rcClip.bottom - rcClip.top) < virtualHeight) {
            return true;
        }
    }

    // Signal 4: cursor is hidden. If a fullscreen window hides the cursor
    // and doesn't clip it, it's still almost certainly a game.
    CURSORINFO ci = {sizeof(ci)};
    if (GetCursorInfo(&ci) && ci.flags == 0) {
        return true;
    }

    return false;
}

// Cached game state with adaptive cadence.
//
//   - force == true always re-checks (used on the trail-trigger edge so
//     the first frame of a new trail can never leak through).
//   - burst == true uses the shorter interval, appropriate while the
//     pointer is moving fast (typical of in-game camera pans).
bool UpdateGameState(DWORD now, bool burst, bool force) {
    static DWORD lastCheck = 0;
    static bool isGameCached = false;

    const DWORD interval = burst ? kGameCheckBurstIntervalMs
                                 : kGameCheckIntervalMs;

    if (force || (now - lastCheck) >= interval) {
        isGameCached = IsGameRunning();
        lastCheck = now;
    }

    return isGameCached;
}

// -----------------------------------------------------------------------------
// Backbuffer
// -----------------------------------------------------------------------------

bool EnsureBackbuffer(int width, int height, HDC referenceDC) {
    if (width <= 0 || height <= 0 || !referenceDC) {
        return false;
    }

    if (g_hdcMem &&
        g_hBitmap &&
        width <= g_cachedWidth &&
        height <= g_cachedHeight) {
        return true;
    }

    if (!g_hdcMem) {
        g_hdcMem = CreateCompatibleDC(referenceDC);
        if (!g_hdcMem) {
            Wh_Log(L"CreateCompatibleDC failed");
            return false;
        }
    }

    const int newWidth = std::max(width, g_cachedWidth);
    const int newHeight = std::max(height, g_cachedHeight);

    ReleaseRenderResources();

    HBITMAP newBitmap = Create32BitDIB(newWidth, newHeight);
    if (!newBitmap) {
        Wh_Log(L"Create32BitDIB failed: %dx%d", newWidth, newHeight);
        return false;
    }

    HGDIOBJ oldBitmap = SelectObject(g_hdcMem, newBitmap);

    if (!g_originalBitmap) {
        // First successful SelectObject: keep the DC's original 1x1 bitmap
        // so it can be restored on teardown.
        g_originalBitmap = oldBitmap;
    } else if (oldBitmap && oldBitmap != g_originalBitmap) {
        // Every subsequent call swaps out the previous DIB.
        DeleteObject(oldBitmap);
    }

    g_hBitmap = newBitmap;
    g_cachedWidth = newWidth;
    g_cachedHeight = newHeight;

    // g_cachedWidth / g_cachedHeight are the authoritative backbuffer
    // dimensions and are used verbatim by BindDC() in RenderTrail().
    return true;
}

// -----------------------------------------------------------------------------
// Direct2D initialization
// -----------------------------------------------------------------------------

bool EnsureD2DResources() {
    if (!g_pD2DFactory) {
        return false;
    }

    if (g_pDCRenderTarget) {
        return true;
    }

    const D2D1_RENDER_TARGET_PROPERTIES props =
        D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(
                DXGI_FORMAT_B8G8R8A8_UNORM,
                D2D1_ALPHA_MODE_PREMULTIPLIED),
            0,
            0,
            D2D1_RENDER_TARGET_USAGE_NONE,
            D2D1_FEATURE_LEVEL_DEFAULT);

    HRESULT hr = g_pD2DFactory->CreateDCRenderTarget(&props, &g_pDCRenderTarget);

    if (FAILED(hr) || !g_pDCRenderTarget) {
        Wh_Log(L"CreateDCRenderTarget failed: 0x%08X", hr);
        g_pDCRenderTarget = nullptr;
        return false;
    }

    hr = g_pDCRenderTarget->CreateSolidColorBrush(
        ToColorF(g_currentOuterRGB, kTrailAlpha), &g_pOuterBrush);

    if (FAILED(hr) || !g_pOuterBrush) {
        Wh_Log(L"Create outer brush failed: 0x%08X", hr);
        ReleaseRenderResources();
        return false;
    }

    hr = g_pDCRenderTarget->CreateSolidColorBrush(
        ToColorF(g_currentCoreRGB, kTrailAlpha), &g_pCoreBrush);

    if (FAILED(hr) || !g_pCoreBrush) {
        Wh_Log(L"Create core brush failed: 0x%08X", hr);
        ReleaseRenderResources();
        return false;
    }

    g_dcBound = false;
    return true;
}

// -----------------------------------------------------------------------------
// Chaikin subdivision
// -----------------------------------------------------------------------------

void SmoothTrail() {
    auto& current = g_renderCache.smoothed;
    auto& next = g_renderCache.subdivision;

    current.clear();
    next.clear();

    // Buffers are sized once by ReserveForTailLength(); no per-frame reserve
    // is required here.

    for (int i = 0; i < g_historyCount; ++i) {
        const POINT& p = HistoryAt(i);
        current.push_back(
            D2D1::Point2F(
                static_cast<float>(p.x + g_tailOffsetX),
                static_cast<float>(p.y + g_tailOffsetY)));
    }

    for (int iteration = 0; iteration < 2; ++iteration) {
        if (current.size() < 3) break;

        next.clear();
        next.push_back(current.front());

        for (size_t i = 0; i + 1 < current.size(); ++i) {
            const D2D1_POINT_2F& p0 = current[i];
            const D2D1_POINT_2F& p1 = current[i + 1];

            next.push_back(D2D1::Point2F(
                0.75f * p0.x + 0.25f * p1.x,
                0.75f * p0.y + 0.25f * p1.y));

            next.push_back(D2D1::Point2F(
                0.25f * p0.x + 0.75f * p1.x,
                0.25f * p0.y + 0.75f * p1.y));
        }

        next.push_back(current.back());
        current.swap(next);
    }
}

// -----------------------------------------------------------------------------
// Calculate trail bounding box in screen coordinates.
// -----------------------------------------------------------------------------

RECT CalculateTrailBounds() {
    const auto& points = g_renderCache.smoothed;

    if (points.empty()) {
        return {0, 0, 0, 0};
    }

    float minX = points.front().x;
    float minY = points.front().y;
    float maxX = points.front().x;
    float maxY = points.front().y;

    for (const auto& p : points) {
        minX = std::min(minX, p.x);
        minY = std::min(minY, p.y);
        maxX = std::max(maxX, p.x);
        maxY = std::max(maxY, p.y);
    }

    minX -= kOuterWidth + static_cast<float>(kRenderPadding);
    minY -= kOuterWidth + static_cast<float>(kRenderPadding);
    maxX += kOuterWidth + static_cast<float>(kRenderPadding);
    maxY += kOuterWidth + static_cast<float>(kRenderPadding);

    RECT result;
    result.left = static_cast<LONG>(floorf(minX));
    result.top = static_cast<LONG>(floorf(minY));
    result.right = static_cast<LONG>(ceilf(maxX));
    result.bottom = static_cast<LONG>(ceilf(maxY));

    if (result.right <= result.left) result.right = result.left + 1;
    if (result.bottom <= result.top) result.bottom = result.top + 1;

    return result;
}

// -----------------------------------------------------------------------------
// Build procedural ribbon geometry.
//
// NOTE: We intentionally create a *fresh* ID2D1PathGeometry per frame and
// release it after use. Reusing a geometry across frames is unreliable with
// ID2D1DCRenderTarget (see v2.4 -> v2.5 regression notes).
// -----------------------------------------------------------------------------

static bool BuildRibbonGeometry(
    const std::vector<D2D1_POINT_2F>& leftPts,
    const std::vector<D2D1_POINT_2F>& rightPts,
    float headRadius,
    float headNx, float headNy,
    float headDx, float headDy,
    ID2D1PathGeometry** ppGeometry)
{
    *ppGeometry = nullptr;

    if (leftPts.size() < 2 || rightPts.size() < 2) {
        return false;
    }

    HRESULT hr = g_pD2DFactory->CreatePathGeometry(ppGeometry);
    if (FAILED(hr) || !*ppGeometry) {
        return false;
    }

    ID2D1GeometrySink* pSink = nullptr;
    hr = (*ppGeometry)->Open(&pSink);
    if (FAILED(hr) || !pSink) {
        (*ppGeometry)->Release();
        *ppGeometry = nullptr;
        return false;
    }

    pSink->SetFillMode(D2D1_FILL_MODE_WINDING);
    pSink->BeginFigure(leftPts[0], D2D1_FIGURE_BEGIN_FILLED);

    for (size_t i = 1; i < leftPts.size(); ++i) {
        pSink->AddLine(leftPts[i]);
    }
    for (size_t i = rightPts.size(); i-- > 0;) {
        pSink->AddLine(rightPts[i]);
    }

    const float cx = (leftPts[0].x + rightPts[0].x) * 0.5f;
    const float cy = (leftPts[0].y + rightPts[0].y) * 0.5f;

    const float k = kArcMagic * headRadius;

    const float backX = cx - headDx * headRadius;
    const float backY = cy - headDy * headRadius;

    {
        D2D1_POINT_2F c1 = D2D1::Point2F(
            rightPts[0].x - headDx * k,
            rightPts[0].y - headDy * k);
        D2D1_POINT_2F c2 = D2D1::Point2F(
            backX - headNx * k,
            backY - headNy * k);
        D2D1_POINT_2F end = D2D1::Point2F(backX, backY);
        pSink->AddBezier(D2D1::BezierSegment(c1, c2, end));
    }

    {
        D2D1_POINT_2F c1 = D2D1::Point2F(
            backX + headNx * k,
            backY + headNy * k);
        D2D1_POINT_2F c2 = D2D1::Point2F(
            leftPts[0].x - headDx * k,
            leftPts[0].y - headDy * k);
        pSink->AddBezier(D2D1::BezierSegment(c1, c2, leftPts[0]));
    }

    pSink->EndFigure(D2D1_FIGURE_END_CLOSED);

    hr = pSink->Close();
    pSink->Release();

    if (FAILED(hr)) {
        (*ppGeometry)->Release();
        *ppGeometry = nullptr;
        return false;
    }

    return true;
}

bool BuildTrailGeometry(
    ID2D1PathGeometry** ppOutlineGeom,
    ID2D1PathGeometry** ppCoreGeom)
{
    *ppOutlineGeom = nullptr;
    *ppCoreGeom = nullptr;

    const auto& smoothed = g_renderCache.smoothed;

    if (smoothed.size() < 2) {
        return false;
    }

    auto& leftOutline = g_renderCache.leftOutline;
    auto& rightOutline = g_renderCache.rightOutline;
    auto& leftCore = g_renderCache.leftCore;
    auto& rightCore = g_renderCache.rightCore;

    leftOutline.clear();
    rightOutline.clear();
    leftCore.clear();
    rightCore.clear();

    const size_t count = smoothed.size();

    leftOutline.reserve(count);
    rightOutline.reserve(count);
    leftCore.reserve(count);
    rightCore.reserve(count);

    float headNx = 1.0f, headNy = 0.0f;
    float headDx = 1.0f, headDy = 0.0f;

    for (size_t i = 0; i < count; ++i) {
        float dx, dy;

        if (i == 0) {
            dx = smoothed[0].x - smoothed[1].x;
            dy = smoothed[0].y - smoothed[1].y;
        } else if (i == count - 1) {
            dx = smoothed[i - 1].x - smoothed[i].x;
            dy = smoothed[i - 1].y - smoothed[i].y;
        } else {
            dx = smoothed[i - 1].x - smoothed[i + 1].x;
            dy = smoothed[i - 1].y - smoothed[i + 1].y;
        }

        const float length = sqrtf(dx * dx + dy * dy);

        if (length > 0.0f) {
            dx /= length;
            dy /= length;
        } else {
            dx = 1.0f;
            dy = 0.0f;
        }

        if (i == 0) {
            headDx = dx;
            headDy = dy;
            headNx = -dy;
            headNy = dx;
        }

        const float nx = -dy;
        const float ny = dx;

        const float ratio = (count > 1)
            ? static_cast<float>(i) / static_cast<float>(count - 1)
            : 0.0f;

        float outerWidth = kOuterWidth - (kOuterWidth * ratio);
        float coreWidth = kCoreWidth - (kCoreWidth * ratio);

        if (i == count - 1) {
            outerWidth = 0.0f;
            coreWidth = 0.0f;
        }

        leftOutline.push_back(D2D1::Point2F(
            smoothed[i].x + nx * outerWidth,
            smoothed[i].y + ny * outerWidth));
        rightOutline.push_back(D2D1::Point2F(
            smoothed[i].x - nx * outerWidth,
            smoothed[i].y - ny * outerWidth));
        leftCore.push_back(D2D1::Point2F(
            smoothed[i].x + nx * coreWidth,
            smoothed[i].y + ny * coreWidth));
        rightCore.push_back(D2D1::Point2F(
            smoothed[i].x - nx * coreWidth,
            smoothed[i].y - ny * coreWidth));
    }

    if (!BuildRibbonGeometry(leftOutline, rightOutline, kOuterWidth,
                             headNx, headNy, headDx, headDy, ppOutlineGeom)) {
        return false;
    }

    if (!BuildRibbonGeometry(leftCore, rightCore, kCoreWidth,
                             headNx, headNy, headDx, headDy, ppCoreGeom)) {
        (*ppOutlineGeom)->Release();
        *ppOutlineGeom = nullptr;
        return false;
    }

    return true;
}

// =============================================================================
// Cursor color sampling
// =============================================================================

struct ColorCandidate {
    uint32_t rgb; // 0x00RRGGBB
    int count;
};

// Extract the color histogram of the current cursor image.
// Returns candidates sorted by descending frequency. Falls back to
// {white, black} when the cursor is monochrome or extraction fails.
static bool ExtractCursorColorCandidates(std::vector<ColorCandidate>& out) {
    out.clear();

    CURSORINFO ci = {sizeof(ci)};
    if (!GetCursorInfo(&ci)) return false;
    if (!(ci.flags & CURSOR_SHOWING) || !ci.hCursor) return false;

    ICONINFO ii = {};
    if (!GetIconInfo(ci.hCursor, &ii)) return false;

    auto cleanup = [&]() {
        if (ii.hbmColor) { DeleteObject(ii.hbmColor); ii.hbmColor = nullptr; }
        if (ii.hbmMask)  { DeleteObject(ii.hbmMask);  ii.hbmMask  = nullptr; }
    };

    bool extracted = false;

    if (ii.hbmColor) {
        BITMAP bm = {};
        if (GetObject(ii.hbmColor, sizeof(bm), &bm) &&
            bm.bmWidth > 0 && bm.bmHeight > 0 &&
            bm.bmWidth <= 256 && bm.bmHeight <= 256) {

            const int w = bm.bmWidth;
            const int h = bm.bmHeight;

            BITMAPINFO bmi = {};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = w;
            bmi.bmiHeader.biHeight = -h;   // top-down
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;

            std::vector<uint32_t> pixels(static_cast<size_t>(w) * h, 0);

            HDC hdc = GetScreenDC();
            int got = 0;
            if (hdc) {
                got = GetDIBits(hdc, ii.hbmColor, 0, h,
                                pixels.data(), &bmi, DIB_RGB_COLORS);
            }

            if (got == h) {
                std::unordered_map<uint32_t, int> hist;
                // Most cursors contain only a handful of distinct colors;
                // a small reserve avoids over-allocating a large bucket
                // array for the common case.
                hist.reserve(64);

                for (uint32_t px : pixels) {
                    const uint8_t a = (px >> 24) & 0xFF;
                    if (a < kCursorAlphaThreshold) continue;

                    // Little-endian DIB: memory is B G R A, so
                    // px & 0x00FFFFFF == 0x00RRGGBB.
                    const uint32_t rgb = px & 0x00FFFFFF;
                    hist[rgb]++;
                }

                if (!hist.empty()) {
                    out.reserve(hist.size());
                    for (const auto& kv : hist) {
                        out.push_back({kv.first, kv.second});
                    }

                    std::sort(out.begin(), out.end(),
                              [](const ColorCandidate& a, const ColorCandidate& b) {
                                  return a.count > b.count;
                              });
                    extracted = true;
                }
            }
        }
    }

    cleanup();

    if (!extracted) {
        out.push_back({kFallbackCoreColor, 1});
        out.push_back({kFallbackOuterColor, 1});
    }

    return true;
}

// Background sampler: cached DIB used by SampleBackgroundLuminance. Kept
// at file scope so the overlay thread can release it on shutdown.
static HDC       g_bgSamplerDC = nullptr;
static HBITMAP   g_bgSamplerBitmap = nullptr;
static HGDIOBJ   g_bgSamplerOriginal = nullptr;
static uint32_t* g_bgSamplerPixels = nullptr;
static int       g_bgSamplerSize = 0;

static void ReleaseBackgroundSampler() {
    if (g_bgSamplerDC) {
        if (g_bgSamplerOriginal) {
            SelectObject(g_bgSamplerDC, g_bgSamplerOriginal);
            g_bgSamplerOriginal = nullptr;
        }
        DeleteDC(g_bgSamplerDC);
        g_bgSamplerDC = nullptr;
    }
    if (g_bgSamplerBitmap) {
        DeleteObject(g_bgSamplerBitmap);
        g_bgSamplerBitmap = nullptr;
    }
    g_bgSamplerPixels = nullptr;
    g_bgSamplerSize = 0;
}

// Sample the average relative luminance of the screen near the cursor.
// Uses BitBlt to grab a small patch into a 32bpp DIB in a single call,
// then samples a 5x5 grid directly from the pixel buffer. This is much
// faster than the equivalent sequence of GetPixel() calls.
static float SampleBackgroundLuminance(POINT center, int radius) {
    const int size = radius * 2 + 1;

    HDC hdcScreen = GetScreenDC();
    if (!hdcScreen) return 0.5f;

    if (g_bgSamplerSize != size || !g_bgSamplerDC || !g_bgSamplerBitmap || !g_bgSamplerPixels) {
        ReleaseBackgroundSampler();

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = size;
        bmi.bmiHeader.biHeight = -size;   // top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* bits = nullptr;
        g_bgSamplerBitmap = CreateDIBSection(
            nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);

        if (!g_bgSamplerBitmap || !bits) {
            ReleaseBackgroundSampler();
            return 0.5f;
        }
        g_bgSamplerPixels = static_cast<uint32_t*>(bits);

        g_bgSamplerDC = CreateCompatibleDC(hdcScreen);
        if (!g_bgSamplerDC) {
            ReleaseBackgroundSampler();
            return 0.5f;
        }
        g_bgSamplerOriginal = SelectObject(g_bgSamplerDC, g_bgSamplerBitmap);
        g_bgSamplerSize = size;
    }

    if (!BitBlt(g_bgSamplerDC, 0, 0, size, size,
                hdcScreen, center.x - radius, center.y - radius, SRCCOPY)) {
        return 0.5f;
    }

    const int grid = kBackgroundSampleGrid;
    double sum = 0.0;
    int n = 0;

    for (int iy = 0; iy < grid; ++iy) {
        const int y = (size - 1) * iy / (grid - 1);
        for (int ix = 0; ix < grid; ++ix) {
            const int x = (size - 1) * ix / (grid - 1);
            const uint32_t px = g_bgSamplerPixels[y * size + x];
            // DIB memory layout on little-endian: B G R A.
            const uint8_t b = static_cast<uint8_t>(px & 0xFF);
            const uint8_t g = static_cast<uint8_t>((px >> 8) & 0xFF);
            const uint8_t r = static_cast<uint8_t>((px >> 16) & 0xFF);
            sum += RgbLuminance(r, g, b);
            n++;
        }
    }

    return n > 0 ? static_cast<float>(sum / n) : 0.5f;
}

// Try to pick a trail core color from the cursor image, honoring contrast
// against the live background. Returns false if nothing passed the check.
static bool AutoPickTrailColor(uint32_t& outCoreRGB) {
    POINT pt;
    if (!GetCursorPos(&pt)) return false;

    std::vector<ColorCandidate> candidates;
    if (!ExtractCursorColorCandidates(candidates)) return false;
    if (candidates.empty()) return false;

    const float bgLum = SampleBackgroundLuminance(pt, kBackgroundSampleRadius);

    for (const auto& c : candidates) {
        const float lum = RgbLuminance(c.rgb);
        if (ContrastRatio(lum, bgLum) >= kMinColorContrast) {
            outCoreRGB = c.rgb;
            return true;
        }
    }

    return false;
}

// Derive an outline color from a core color's relative luminance.
static uint32_t DeriveOutlineColor(uint32_t coreRGB) {
    return (RgbLuminance(coreRGB) > 0.5f) ? kFallbackOuterColor
                                          : kFallbackCoreColor;
}

// Compute the actual outline color according to settings.
static uint32_t ResolveOutlineColor(uint32_t coreRGB) {
    if (g_outlineColorMode == 1) {
        return g_manualOutlineRGB;
    }
    return DeriveOutlineColor(coreRGB);
}

// Refresh g_currentCoreRGB / g_currentOuterRGB according to settings.
//
// In Auto mode we re-sample the cursor only when its image actually changes
// (switching from arrow to I-beam, to hand, etc.), unless the user has opted
// into a periodic resample interval. Moving the mouse does not change the
// cursor image, so in the common case we do zero sampling per frame.
static void UpdateTrailColorIfNeeded(DWORD now) {
    static DWORD   s_lastUpdate = 0;
    static HCURSOR s_lastCursor = nullptr;
    static bool    s_initialized = false;

    // Manual core: just apply the configured color, derive or use the
    // configured outline. The result only changes when the settings
    // actually change, so cache the last resolved values and skip the
    // (pow-heavy) luminance computation on unchanged frames.
    if (g_trailColorMode == 0) {
        static uint32_t s_lastManualCore = 0xFFFFFFFFu;
        static uint32_t s_lastManualOutline = 0xFFFFFFFFu;
        static int      s_lastOutlineMode = -1;

        if (g_manualColorRGB     != s_lastManualCore ||
            g_manualOutlineRGB   != s_lastManualOutline ||
            g_outlineColorMode   != s_lastOutlineMode)
        {
            g_currentCoreRGB  = g_manualColorRGB;
            g_currentOuterRGB = ResolveOutlineColor(g_currentCoreRGB);

            s_lastManualCore    = g_manualColorRGB;
            s_lastManualOutline = g_manualOutlineRGB;
            s_lastOutlineMode   = g_outlineColorMode;
        }

        s_initialized = true;
        return;
    }

    // Auto core: decide whether a re-sample is needed this tick.
    bool needResample = false;

    if (!s_initialized) {
        needResample = true;
    } else if (g_autoResampleInterval > 0) {
        // Periodic mode.
        needResample =
            (now - s_lastUpdate) >= static_cast<DWORD>(g_autoResampleInterval);
    } else {
        // Cursor-change mode. Guard against animated cursors (e.g. the busy
        // spinner) with a small floor so we never sample on every frame.
        CURSORINFO ci = {sizeof(ci)};
        if (GetCursorInfo(&ci) && ci.hCursor != s_lastCursor) {
            if ((now - s_lastUpdate) >= kMinCursorChangeResampleIntervalMs) {
                needResample = true;
            }
        }
    }

    if (!needResample) return;

    // Snapshot the cursor handle so we can detect changes later.
    {
        CURSORINFO ci = {sizeof(ci)};
        if (GetCursorInfo(&ci)) {
            s_lastCursor = ci.hCursor;
        }
    }
    s_lastUpdate = now;
    s_initialized = true;

    uint32_t core = kFallbackCoreColor;
    if (!AutoPickTrailColor(core)) {
        core = kFallbackCoreColor;
    }
    g_currentCoreRGB = core;
    g_currentOuterRGB = ResolveOutlineColor(core);
}

// =============================================================================
// Render current trail.
// =============================================================================

bool RenderTrail(HWND hwnd) {
    if (g_historyCount < 2) {
        return false;
    }

    SmoothTrail();

    if (g_renderCache.smoothed.size() < 2) {
        return false;
    }

    const RECT bounds = CalculateTrailBounds();
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;

    HDC hdcScreen = GetScreenDC();
    if (!hdcScreen) return false;

    if (!EnsureBackbuffer(width, height, hdcScreen)) return false;
    if (!EnsureD2DResources()) return false;

    // Bind the render target to the *full* backbuffer, not just the current
    // frame's trail bounds. D2D clips drawing to the BindDC rectangle; if
    // that rectangle is smaller than the buffer, any trail geometry that
    // extends past it on later frames gets clipped (visible symptom: the
    // tail appears cut off). g_cachedWidth/g_cachedHeight are always the
    // actual buffer dimensions, so this rectangle is always complete.
    if (!g_dcBound) {
        RECT bindRect = {0, 0, g_cachedWidth, g_cachedHeight};
        HRESULT hr = g_pDCRenderTarget->BindDC(g_hdcMem, &bindRect);
        if (FAILED(hr)) return false;
        g_dcBound = true;
    }

    g_pDCRenderTarget->BeginDraw();
    g_pDCRenderTarget->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

    // Apply the current trail colors to the brushes.
    g_pOuterBrush->SetColor(ToColorF(g_currentOuterRGB, kTrailAlpha));
    g_pCoreBrush->SetColor(ToColorF(g_currentCoreRGB, kTrailAlpha));

    g_pDCRenderTarget->SetTransform(
        D2D1::Matrix3x2F::Translation(
            -static_cast<float>(bounds.left),
            -static_cast<float>(bounds.top)));

    ID2D1PathGeometry* pOutlineGeom = nullptr;
    ID2D1PathGeometry* pCoreGeom = nullptr;

    if (BuildTrailGeometry(&pOutlineGeom, &pCoreGeom)) {
        g_pDCRenderTarget->FillGeometry(pOutlineGeom, g_pOuterBrush);
        g_pDCRenderTarget->FillGeometry(pCoreGeom, g_pCoreBrush);
    }

    if (pCoreGeom) pCoreGeom->Release();
    if (pOutlineGeom) pOutlineGeom->Release();

    g_pDCRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

    HRESULT hr = g_pDCRenderTarget->EndDraw();

    if (hr == D2DERR_RECREATE_TARGET) {
        ReleaseRenderResources();
        return false;
    }
    if (FAILED(hr)) return false;

    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    POINT windowPos = {bounds.left, bounds.top};
    SIZE windowSize = {width, height};
    POINT sourcePos = {0, 0};

    const BOOL updated = UpdateLayeredWindow(
        hwnd, hdcScreen, &windowPos, &windowSize,
        g_hdcMem, &sourcePos, 0, &blend, ULW_ALPHA);

    return updated != FALSE;
}

// -----------------------------------------------------------------------------
// Trail update
// -----------------------------------------------------------------------------

void UpdateTrailState(const POINT& pt, float velocity) {
    if (velocity > g_triggerVelocity && !g_isSmearing) {
        g_isSmearing = true;
        g_lowVelocityFrames = 0;
    } else if (velocity < g_stopVelocity && g_isSmearing) {
        ++g_lowVelocityFrames;
        if (g_lowVelocityFrames > 2) {
            g_isSmearing = false;
        }
    } else if (velocity >= g_stopVelocity && g_isSmearing) {
        g_lowVelocityFrames = 0;
    }

    if (g_isSmearing) {
        HistoryPushFront({pt.x, pt.y});
        while (g_historyCount > g_tailLength) {
            HistoryPopBack();
        }
    } else {
        if (g_historyCount > 0) {
            HistoryPopBack();
            if (g_historyCount > 0) {
                HistoryPopBack();
            }
        }
    }
}

// -----------------------------------------------------------------------------
// Window procedure
// -----------------------------------------------------------------------------

LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case kSettingsChangedMessage:
            LoadSettings();
            return 0;

        case WM_DISPLAYCHANGE:
            ReleaseScreenDC();
            return 0;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            KillTimer(hwnd, kTimerId);
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }
}

// -----------------------------------------------------------------------------
// Timer
// -----------------------------------------------------------------------------

VOID CALLBACK SmearTimerProc(HWND hwnd, UINT, UINT_PTR, DWORD dwTime) {
    POINT pt;
    if (!GetCursorPos(&pt)) return;

    const bool moved = (pt.x != g_lastPos.x) || (pt.y != g_lastPos.y);

    if (!moved && !g_isSmearing && g_historyCount == 0) {
        if (g_windowVisible) HideOverlay();
        return;
    }

    const int dx = pt.x - g_lastPos.x;
    const int dy = pt.y - g_lastPos.y;

    const float fdx = static_cast<float>(dx);
    const float fdy = static_cast<float>(dy);
    const float velocity = sqrtf(fdx * fdx + fdy * fdy);

    g_lastPos = pt;

    // A "new trail" starts the moment we cross the trigger threshold while
    // not already smearing. Force a fresh game check right now so the first
    // frame of a new trail can never leak into a game.
    const bool newlyTriggered =
        (velocity > g_triggerVelocity) && !g_isSmearing;

    // While the pointer is moving fast (typical of in-game camera pans),
    // tighten the game-check cadence.
    const bool burst =
        velocity > (g_triggerVelocity * kGameCheckBurstVelocityFactor);

    if (UpdateGameState(dwTime, burst, newlyTriggered)) {
        HistoryClear();
        g_isSmearing = false;
        g_lowVelocityFrames = 0;
        HideOverlay();
        return;
    }

    UpdateTrailState(pt, velocity);
    UpdateTrailColorIfNeeded(dwTime);

    if (g_historyCount < 2) {
        HideOverlay();
        return;
    }

    if (!RenderTrail(hwnd)) {
        HideOverlay();
        return;
    }

    ShowOverlay();
}

// -----------------------------------------------------------------------------
// Overlay thread
// -----------------------------------------------------------------------------

DWORD WINAPI OverlayThreadProc(LPVOID) {
    g_threadId = GetCurrentThreadId();

    HRESULT coResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(coResult)) {
        Wh_Log(L"CoInitializeEx failed: 0x%08X", coResult);
        return 0;
    }

    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_pD2DFactory);
    if (FAILED(hr) || !g_pD2DFactory) {
        Wh_Log(L"D2D1CreateFactory failed: 0x%08X", hr);
        CoUninitialize();
        return 0;
    }

    HINSTANCE hInstance = GetModuleHandle(nullptr);
    const wchar_t className[] = L"SmearFrameOverlayClass";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = OverlayWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = className;

    ATOM atom = RegisterClassW(&wc);
    if (!atom && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        Wh_Log(L"RegisterClass failed: %lu", GetLastError());
        g_pD2DFactory->Release();
        g_pD2DFactory = nullptr;
        CoUninitialize();
        return 0;
    }

    HWND hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        className, L"SmearOverlay", WS_POPUP,
        0, 0, 1, 1, nullptr, nullptr, hInstance, nullptr);

    if (!hwnd) {
        Wh_Log(L"CreateWindowEx failed: %lu", GetLastError());
        UnregisterClassW(className, hInstance);
        g_pD2DFactory->Release();
        g_pD2DFactory = nullptr;
        CoUninitialize();
        return 0;
    }

    g_overlayHwnd.store(hwnd);
    HideOverlay();
    GetCursorPos(&g_lastPos);

    if (!SetCoalescableTimer(hwnd, kTimerId, USER_TIMER_MINIMUM,
                             SmearTimerProc, kTimerCoalescingTolerance)) {
        Wh_Log(L"SetCoalescableTimer failed: %lu", GetLastError());
        DestroyWindow(hwnd);
        g_overlayHwnd.store(nullptr);
        UnregisterClassW(className, hInstance);
        g_pD2DFactory->Release();
        g_pD2DFactory = nullptr;
        CoUninitialize();
        return 0;
    }

    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    KillTimer(hwnd, kTimerId);
    HideOverlay();
    ReleaseBackbuffer();
    ReleaseBackgroundSampler();
    ReleaseScreenDC();

    if (g_pD2DFactory) {
        g_pD2DFactory->Release();
        g_pD2DFactory = nullptr;
    }

    DestroyWindow(hwnd);
    g_overlayHwnd.store(nullptr);

    UnregisterClassW(className, hInstance);
    CoUninitialize();

    return 0;
}

// -----------------------------------------------------------------------------
// Windhawk Tool Mod implementation
// -----------------------------------------------------------------------------

BOOL WhTool_ModInit() {
    LoadSettings();

    g_threadHandle = CreateThread(nullptr, 0, OverlayThreadProc, nullptr, 0, &g_threadId);
    if (!g_threadHandle) {
        Wh_Log(L"CreateThread failed: %lu", GetLastError());
        return FALSE;
    }

    return TRUE;
}

void WhTool_ModUninit() {
    HWND hwnd = g_overlayHwnd.load();
    if (hwnd && IsWindow(hwnd)) {
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
    }

    if (g_threadHandle) {
        DWORD waitResult = WaitForSingleObject(g_threadHandle, 3000);
        if (waitResult == WAIT_TIMEOUT) {
            Wh_Log(L"Overlay thread did not exit in time, forcing termination.");
            TerminateThread(g_threadHandle, 0);
        }
        CloseHandle(g_threadHandle);
        g_threadHandle = nullptr;
    }

    g_overlayHwnd.store(nullptr);
    g_threadId = 0;
}

void WhTool_ModSettingsChanged() {
    HWND hwnd = g_overlayHwnd.load();
    if (hwnd && IsWindow(hwnd)) {
        PostMessageW(hwnd, kSettingsChangedMessage, 0, 0);
    }
}

// -----------------------------------------------------------------------------
// Windhawk Tool Mod launcher implementation
// -----------------------------------------------------------------------------

bool g_isToolModProcessLauncher = false;
HANDLE g_toolModProcessMutex = nullptr;

void WINAPI EntryPoint_Hook() {
    Wh_Log(L">");
    ExitThread(0);
}

BOOL Wh_ModInit() {
    DWORD sessionId;
    if (ProcessIdToSessionId(GetCurrentProcessId(), &sessionId) && sessionId == 0) {
        return FALSE;
    }

    bool isExcluded = false;
    bool isToolModProcess = false;
    bool isCurrentToolModProcess = false;

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) {
        Wh_Log(L"CommandLineToArgvW failed");
        return FALSE;
    }

    for (int i = 1; i < argc; ++i) {
        if (wcscmp(argv[i], L"-service") == 0 ||
            wcscmp(argv[i], L"-service-start") == 0 ||
            wcscmp(argv[i], L"-service-stop") == 0) {
            isExcluded = true;
            break;
        }
    }

    for (int i = 1; i < argc - 1; ++i) {
        if (wcscmp(argv[i], L"-tool-mod") == 0) {
            isToolModProcess = true;
            if (wcscmp(argv[i + 1], WH_MOD_ID) == 0) {
                isCurrentToolModProcess = true;
            }
            break;
        }
    }

    LocalFree(argv);

    if (isExcluded) return FALSE;

    if (isCurrentToolModProcess) {
        g_toolModProcessMutex = CreateMutexW(nullptr, TRUE, L"windhawk-tool-mod_" WH_MOD_ID);
        if (!g_toolModProcessMutex) {
            Wh_Log(L"CreateMutex failed");
            ExitProcess(1);
        }

        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            Wh_Log(L"Tool mod already running (%s)", WH_MOD_ID);
            ExitProcess(1);
        }

        if (!WhTool_ModInit()) {
            ExitProcess(1);
        }

        IMAGE_DOS_HEADER* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(GetModuleHandle(nullptr));
        IMAGE_NT_HEADERS* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(reinterpret_cast<BYTE*>(dosHeader) + dosHeader->e_lfanew);
        DWORD entryPointRVA = ntHeaders->OptionalHeader.AddressOfEntryPoint;
        void* entryPoint = reinterpret_cast<BYTE*>(dosHeader) + entryPointRVA;

        Wh_SetFunctionHook(entryPoint, reinterpret_cast<void*>(EntryPoint_Hook), nullptr);
        return TRUE;
    }

    if (isToolModProcess) return FALSE;

    g_isToolModProcessLauncher = true;
    return TRUE;
}

void Wh_ModAfterInit() {
    if (!g_isToolModProcessLauncher) return;

    WCHAR currentProcessPath[MAX_PATH];
    const DWORD pathLength = GetModuleFileNameW(nullptr, currentProcessPath, ARRAYSIZE(currentProcessPath));
    if (pathLength == 0 || pathLength == ARRAYSIZE(currentProcessPath)) {
        Wh_Log(L"GetModuleFileName failed");
        return;
    }

    WCHAR commandLine[MAX_PATH + 64];
    swprintf_s(commandLine, L"\"%s\" -tool-mod \"%s\"", currentProcessPath, WH_MOD_ID);

    HMODULE kernelModule = GetModuleHandleW(L"kernelbase.dll");
    if (!kernelModule) {
        kernelModule = GetModuleHandleW(L"kernel32.dll");
        if (!kernelModule) {
            Wh_Log(L"No kernelbase.dll/kernel32.dll");
            return;
        }
    }

    using CreateProcessInternalW_t = BOOL(WINAPI*)(
        HANDLE, LPCWSTR, LPWSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES,
        WINBOOL, DWORD, LPVOID, LPCWSTR, LPSTARTUPINFOW, LPPROCESS_INFORMATION, PHANDLE);

    auto pCreateProcessInternalW = reinterpret_cast<CreateProcessInternalW_t>(
        GetProcAddress(kernelModule, "CreateProcessInternalW"));

    STARTUPINFO si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_FORCEOFFFEEDBACK;

    PROCESS_INFORMATION pi = {};
    BOOL processCreated = FALSE;

    if (pCreateProcessInternalW) {
        processCreated = pCreateProcessInternalW(
            nullptr, currentProcessPath, commandLine, nullptr, nullptr, FALSE,
            NORMAL_PRIORITY_CLASS, nullptr, nullptr, &si, &pi, nullptr);
    }

    if (!processCreated) {
        ZeroMemory(&pi, sizeof(pi));
        processCreated = CreateProcessW(
            nullptr, commandLine, nullptr, nullptr, FALSE,
            NORMAL_PRIORITY_CLASS, nullptr, nullptr, &si, &pi);
    }

    if (!processCreated) {
        Wh_Log(L"CreateProcess failed: %lu", GetLastError());
        return;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

void Wh_ModSettingsChanged() {
    if (g_isToolModProcessLauncher) return;
    WhTool_ModSettingsChanged();
}

void Wh_ModUninit() {
    if (g_isToolModProcessLauncher) return;

    WhTool_ModUninit();

    if (g_toolModProcessMutex) {
        CloseHandle(g_toolModProcessMutex);
        g_toolModProcessMutex = nullptr;
    }

    ExitProcess(0);
}