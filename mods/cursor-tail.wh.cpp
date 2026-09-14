// ==WindhawkMod==
// @id              cursor-tail
// @name            Cursor Tail
// @description     Adaptive motion-blur trail for the mouse pointer, colored by sampling the cursor image.
// @version         3.5
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
* **Speed-Reactive Shape:** Trail width, alpha and length can scale with
  pointer velocity for a more physical feel.
* **Gradient / Glow:** Optional fade toward the tail and an outer glow pass.
* **Smooth Fade-Out:** Trail fades gracefully when the pointer stops instead
  of snapping off.
* **Per-App Rules:** Whitelist or blacklist specific executables so the trail
  can be forced on (e.g. video players) or suppressed entirely.
* **Hotkey Toggle:** Ctrl+Alt+T temporarily suspends/restores the trail.
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

### Per-App Rules
Format: one rule per line, `exe=on` or `exe=off`, `#` starts a comment.
Example:
- chrome.exe = off.
- mpv.exe = on.


### Game Detection
The trail is suppressed automatically when a fullscreen game is detected.
Signals checked, from strongest to weakest:
1. `SHQueryUserNotificationState` reports D3D fullscreen.
2. DWM composition is disabled (typical of exclusive fullscreen).
3. Foreground window covers the whole monitor (2px tolerance).
4. Cursor is clipped to less than the virtual screen.
5. Cursor is hidden.
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

- speed_scaling: 1
  $name: Speed-Reactive Shape
  $description: Scale width, alpha and length with pointer velocity (0 = off, 1 = on).
- width_min: 4
  $name: Outer Width (min)
  $description: Outer ribbon half-width at low speed, in pixels.
- width_max: 14
  $name: Outer Width (max)
  $description: Outer ribbon half-width at high speed, in pixels.
- core_width_min: 2
  $name: Core Width (min)
  $description: Inner ribbon half-width at low speed, in pixels.
- core_width_max: 9
  $name: Core Width (max)
  $description: Inner ribbon half-width at high speed, in pixels.
- alpha_min: 45
  $name: Alpha (min)
  $description: Trail opacity at low speed, 0-100.
- alpha_max: 90
  $name: Alpha (max)
  $description: Trail opacity at high speed, 0-100.
- taper_power: 10
  $name: Taper Power (x0.1)
  $description: 10 = linear taper, higher = sharper tip. Range 5-30 (0.5-3.0).

- smooth_iterations: 2
  $name: Smooth Iterations
  $description: Chaikin subdivision passes. 0 = raw polyline, 4 = very smooth.

- gradient_enabled: 0
  $name: Gradient Tail
  $description: Fade the core color toward a second color at the tail.
- gradient_tail_color: "#FF00FF"
  $name: Gradient Tail Color
  $description: Core color at the tail end when Gradient is enabled. Format "#RRGGBB".

- glow_enabled: 0
  $name: Glow
  $description: Draw an outer soft glow ring behind the trail.
- glow_color: "#FFFFFF"
  $name: Glow Color
  $description: Color of the glow ring. Format "#RRGGBB".
- glow_width_factor: 18
  $name: Glow Width Factor (x0.1)
  $description: Glow width relative to the outer ribbon width, x0.1. 18 = 1.8x.
- glow_alpha: 25
  $name: Glow Alpha
  $description: Glow opacity, 0-100.

- fade_enabled: 1
  $name: Fade-Out
  $description: Fade the trail out smoothly when the pointer stops instead of snapping off.
- fade_decay: 90
  $name: Fade Decay (x0.01)
  $description: Per-frame alpha multiplier during fade-out, x0.01. 90 = 0.90.

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

- app_rules: ""
  $name: Per-App Rules
  $description: One rule per line, "exe=on" or "exe=off". "#" starts a comment.
- hotkey_enabled: 0
  $name: Enable Hotkey
  $description: Register Ctrl+Alt+T to temporarily suspend/resume the trail.
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
#include <string>
#include <unordered_map>
#include <vector>

// -----------------------------------------------------------------------------
// Constants
// -----------------------------------------------------------------------------

constexpr UINT_PTR kTimerId = 1;
constexpr UINT kSettingsChangedMessage = WM_APP + 1;
constexpr int  kHotkeyId = 0xCAFE;

constexpr float kDefaultTriggerVelocity = 25.0f;
constexpr float kDefaultStopVelocity = 10.0f;

constexpr int kDefaultTailOffsetX = 6;
constexpr int kDefaultTailOffsetY = 10;
constexpr int kDefaultTailLength = 10;

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

constexpr DWORD kMinCursorChangeResampleIntervalMs = 100;

constexpr int kBackgroundSampleRadius = 24;
constexpr int kBackgroundSampleGrid = 5;
constexpr float kMinColorContrast = 3.0f;

constexpr uint8_t kCursorAlphaThreshold = 96;
constexpr uint32_t kFallbackCoreColor = 0x00FFFFFF;
constexpr uint32_t kFallbackOuterColor = 0x00000000;

// Game detection cadence.
constexpr DWORD kGameCheckIntervalMs      = 100;
constexpr DWORD kGameCheckBurstIntervalMs = 30;
constexpr float kGameCheckBurstVelocityFactor = 2.0f;

constexpr LONG kFullscreenTolerancePx = 2;

// Fade-out cutoff below which we clear the history.
constexpr float kFadeCutoff = 0.05f;

// -----------------------------------------------------------------------------
// Global state
// -----------------------------------------------------------------------------

std::atomic<HWND> g_overlayHwnd{nullptr};
HANDLE g_threadHandle = nullptr;

POINT g_history[kHistoryCapacity];
int g_historyHead = 0;
int g_historyCount = 0;

POINT g_lastPos = {0, 0};

bool g_isSmearing = false;
int g_lowVelocityFrames = 0;

float g_fadeAlpha = 0.0f;

float g_currentVelocity = 0.0f;
float g_smoothedSpeedNorm = 0.0f;   // 0..1
float g_frozenSpeedNorm = 0.5f;     // used while fading out

bool g_hotkeySuspended = false;

// -----------------------------------------------------------------------------
// Direct2D
// -----------------------------------------------------------------------------

ID2D1Factory* g_pD2DFactory = nullptr;
ID2D1DCRenderTarget* g_pDCRenderTarget = nullptr;

ID2D1SolidColorBrush* g_pOuterBrush = nullptr;
ID2D1SolidColorBrush* g_pCoreBrush = nullptr;
ID2D1SolidColorBrush* g_pGlowBrush = nullptr;

ID2D1LinearGradientBrush* g_pGradientBrush = nullptr;
ID2D1GradientStopCollection* g_pGradientStops = nullptr;

// Cache of the colors last baked into the gradient stop collection.
uint32_t g_gradStopsHead = 0xFFFFFFFFu;
uint32_t g_gradStopsTail = 0xFFFFFFFFu;

bool g_dcBound = false;

// -----------------------------------------------------------------------------
// Cached backbuffer
// -----------------------------------------------------------------------------

HDC g_hdcMem = nullptr;
HBITMAP g_hBitmap = nullptr;
HGDIOBJ g_originalBitmap = nullptr;
int g_cachedWidth = 0;
int g_cachedHeight = 0;

HDC g_hdcScreen = nullptr;

// -----------------------------------------------------------------------------
// Settings
// -----------------------------------------------------------------------------

float g_triggerVelocity = kDefaultTriggerVelocity;
float g_stopVelocity = kDefaultStopVelocity;

int g_tailOffsetX = kDefaultTailOffsetX;
int g_tailOffsetY = kDefaultTailOffsetY;
int g_tailLength = kDefaultTailLength;

// Speed-reactive shape.
int   g_speedScaling = 1;
float g_widthMin = 4.0f;
float g_widthMax = 14.0f;
float g_coreWidthMin = 2.0f;
float g_coreWidthMax = 9.0f;
float g_alphaMin = 0.45f;
float g_alphaMax = 0.90f;
float g_taperPower = 1.0f;

int g_smoothIterations = 2;

// Gradient.
int      g_gradientEnabled = 0;
uint32_t g_gradientTailRGB = 0x00FF00FF;

// Glow.
int      g_glowEnabled = 0;
uint32_t g_glowRGB = 0x00FFFFFF;
float    g_glowWidthFactor = 1.8f;
float    g_glowAlpha = 0.25f;

// Fade.
int   g_fadeEnabled = 1;
float g_fadeDecay = 0.90f;

// Colors.
int g_trailColorMode = 0;
uint32_t g_manualColorRGB = 0x00FFFFFF;
int g_outlineColorMode = 0;
uint32_t g_manualOutlineRGB = 0x00000000;
int g_autoResampleInterval = kAutoResampleIntervalDefaultMs;

uint32_t g_currentCoreRGB = kFallbackCoreColor;
uint32_t g_currentOuterRGB = kFallbackOuterColor;

// Per-app rules.
struct AppRule {
    std::wstring exe;   // lowercase, e.g. "chrome.exe"
    bool enabled;       // true = force on, false = force off
};
std::vector<AppRule> g_appRules;
bool g_appRulesCachedValue = false;   // last CheckAppRule result
int  g_appRulesCachedKind = 0;        // -1 off, 0 none, 1 on

int g_hotkeyEnabled = 0;

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
        const size_t smoothedCount = baseCount * 16 - 15;  // up to 4 iterations

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

bool g_windowVisible = false;

// -----------------------------------------------------------------------------
// Color utilities
// -----------------------------------------------------------------------------

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

static inline std::wstring ToLowerW(std::wstring s) {
    for (auto& c : s) c = static_cast<wchar_t>(towlower(c));
    return s;
}

// -----------------------------------------------------------------------------
// Utility
// -----------------------------------------------------------------------------

void ReleaseRenderResources() {
    if (g_pGradientBrush) { g_pGradientBrush->Release(); g_pGradientBrush = nullptr; }
    if (g_pGradientStops) { g_pGradientStops->Release(); g_pGradientStops = nullptr; }
    g_gradStopsHead = 0xFFFFFFFFu;
    g_gradStopsTail = 0xFFFFFFFFu;

    if (g_pGlowBrush) { g_pGlowBrush->Release(); g_pGlowBrush = nullptr; }
    if (g_pCoreBrush) { g_pCoreBrush->Release(); g_pCoreBrush = nullptr; }
    if (g_pOuterBrush) { g_pOuterBrush->Release(); g_pOuterBrush = nullptr; }
    if (g_pDCRenderTarget) { g_pDCRenderTarget->Release(); g_pDCRenderTarget = nullptr; }
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

void HistoryPushFront(const POINT& p, int maxLen) {
    if (g_historyCount == kHistoryCapacity) {
        g_historyCount--;
    }
    g_historyHead = (g_historyHead - 1 + kHistoryCapacity) % kHistoryCapacity;
    g_history[g_historyHead] = p;
    g_historyCount++;

    if (maxLen > 0 && g_historyCount > maxLen) {
        g_historyCount = maxLen;
    }
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
// Per-app rules
// -----------------------------------------------------------------------------

static bool GetProcessExeName(DWORD pid, std::wstring& out) {
    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProc) return false;

    WCHAR buf[512];
    DWORD size = ARRAYSIZE(buf);
    BOOL ok = QueryFullProcessImageNameW(hProc, 0, buf, &size);
    CloseHandle(hProc);
    if (!ok) return false;

    const wchar_t* name = wcsrchr(buf, L'\\');
    out = ToLowerW(name ? name + 1 : buf);
    return true;
}

// Return -1 = force off, 0 = no rule, 1 = force on.
static int CheckAppRule() {
    if (g_appRules.empty()) return 0;

    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return 0;

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (!pid) return 0;

    std::wstring exe;
    if (!GetProcessExeName(pid, exe)) return 0;

    for (const auto& r : g_appRules) {
        if (r.exe == exe) return r.enabled ? 1 : -1;
    }
    return 0;
}

// -----------------------------------------------------------------------------
// Settings
// -----------------------------------------------------------------------------

static void ParseAppRules(const wchar_t* text) {
    g_appRules.clear();
    if (!text) return;

    std::wstring line;
    const wchar_t* p = text;
    while (true) {
        if (*p == L'\n' || *p == L'\r' || *p == L'\0') {
            // trim
            size_t a = 0, b = line.size();
            while (a < b && (line[a] == L' ' || line[a] == L'\t')) a++;
            while (b > a && (line[b - 1] == L' ' || line[b - 1] == L'\t' ||
                             line[b - 1] == L'\r')) b--;
            std::wstring t = line.substr(a, b - a);
            line.clear();

            if (!t.empty() && t[0] != L'#') {
                size_t eq = t.find(L'=');
                if (eq != std::wstring::npos) {
                    std::wstring exe = t.substr(0, eq);
                    std::wstring val = t.substr(eq + 1);
                    // trim
                    size_t ea = 0, eb = exe.size();
                    while (ea < eb && (exe[ea] == L' ' || exe[ea] == L'\t')) ea++;
                    while (eb > ea && (exe[eb - 1] == L' ' || exe[eb - 1] == L'\t')) eb--;
                    exe = exe.substr(ea, eb - ea);

                    size_t va = 0, vb = val.size();
                    while (va < vb && (val[va] == L' ' || val[va] == L'\t')) va++;
                    while (vb > va && (val[vb - 1] == L' ' || val[vb - 1] == L'\t')) vb--;
                    val = val.substr(va, vb - va);

                    if (!exe.empty()) {
                        AppRule r;
                        r.exe = ToLowerW(exe);
                        r.enabled = (val == L"on" || val == L"1" ||
                                     val == L"true" || val == L"yes");
                        g_appRules.push_back(std::move(r));
                    }
                }
            }
        } else {
            line.push_back(*p);
        }

        if (*p == L'\0') break;
        p++;
    }
}

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
        if (g_stopVelocity <= 0.0f) g_stopVelocity = 1.0f;
    }

    // Speed-reactive shape.
    g_speedScaling = std::clamp(Wh_GetIntSetting(L"speed_scaling"), 0, 1);
    g_widthMin     = static_cast<float>(std::clamp(Wh_GetIntSetting(L"width_min"), 1, 40));
    g_widthMax     = static_cast<float>(std::clamp(Wh_GetIntSetting(L"width_max"), 1, 60));
    g_coreWidthMin = static_cast<float>(std::clamp(Wh_GetIntSetting(L"core_width_min"), 1, 40));
    g_coreWidthMax = static_cast<float>(std::clamp(Wh_GetIntSetting(L"core_width_max"), 1, 60));
    g_alphaMin     = std::clamp(Wh_GetIntSetting(L"alpha_min"), 0, 100) / 100.0f;
    g_alphaMax     = std::clamp(Wh_GetIntSetting(L"alpha_max"), 0, 100) / 100.0f;
    g_taperPower   = std::clamp(Wh_GetIntSetting(L"taper_power"), 5, 30) / 10.0f;

    if (g_widthMax < g_widthMin) std::swap(g_widthMax, g_widthMin);
    if (g_coreWidthMax < g_coreWidthMin) std::swap(g_coreWidthMax, g_coreWidthMin);

    g_smoothIterations = std::clamp(Wh_GetIntSetting(L"smooth_iterations"), 0, 4);

    // Gradient.
    g_gradientEnabled = std::clamp(Wh_GetIntSetting(L"gradient_enabled"), 0, 1);
    {
        PCWSTR s = Wh_GetStringSetting(L"gradient_tail_color");
        uint32_t parsed = 0;
        g_gradientTailRGB = (s && ParseHexColor(s, parsed)) ? parsed : 0x00FF00FF;
        if (s) Wh_FreeStringSetting(s);
    }

    // Glow.
    g_glowEnabled = std::clamp(Wh_GetIntSetting(L"glow_enabled"), 0, 1);
    {
        PCWSTR s = Wh_GetStringSetting(L"glow_color");
        uint32_t parsed = 0;
        g_glowRGB = (s && ParseHexColor(s, parsed)) ? parsed : 0x00FFFFFF;
        if (s) Wh_FreeStringSetting(s);
    }
    g_glowWidthFactor = std::clamp(Wh_GetIntSetting(L"glow_width_factor"), 10, 30) / 10.0f;
    g_glowAlpha       = std::clamp(Wh_GetIntSetting(L"glow_alpha"), 0, 100) / 100.0f;

    // Fade.
    g_fadeEnabled = std::clamp(Wh_GetIntSetting(L"fade_enabled"), 0, 1);
    g_fadeDecay   = std::clamp(Wh_GetIntSetting(L"fade_decay"), 50, 99) / 100.0f;

    // Color modes.
    g_trailColorMode = std::clamp(Wh_GetIntSetting(L"trail_color_mode"), 0, 1);

    {
        PCWSTR colorStr = Wh_GetStringSetting(L"trail_color_manual");
        uint32_t parsed = 0;
        g_manualColorRGB = (colorStr && ParseHexColor(colorStr, parsed))
                               ? parsed : kFallbackCoreColor;
        if (colorStr) Wh_FreeStringSetting(colorStr);
    }

    g_outlineColorMode = std::clamp(Wh_GetIntSetting(L"outline_color_mode"), 0, 1);

    {
        PCWSTR outlineStr = Wh_GetStringSetting(L"outline_color_manual");
        uint32_t parsed = 0;
        g_manualOutlineRGB = (outlineStr && ParseHexColor(outlineStr, parsed))
                                 ? parsed : kFallbackOuterColor;
        if (outlineStr) Wh_FreeStringSetting(outlineStr);
    }

    g_autoResampleInterval = std::clamp(
        Wh_GetIntSetting(L"auto_resample_interval"),
        kAutoResampleIntervalMinMs,
        kAutoResampleIntervalMaxMs);

    // Per-app rules.
    {
        PCWSTR rules = Wh_GetStringSetting(L"app_rules");
        ParseAppRules(rules);
        if (rules) Wh_FreeStringSetting(rules);
    }

    g_hotkeyEnabled = std::clamp(Wh_GetIntSetting(L"hotkey_enabled"), 0, 1);

    // Prepare cache for the largest plausible subdivision count.
    g_renderCache.ReserveForTailLength(g_tailLength);
}

// -----------------------------------------------------------------------------
// Game detection
// -----------------------------------------------------------------------------

bool IsGameRunning() {
    HWND hwnd = GetForegroundWindow();

    if (!hwnd || hwnd == GetDesktopWindow()) return false;
    if (hwnd == GetShellWindow()) return false;

    QUERY_USER_NOTIFICATION_STATE state;
    if (SUCCEEDED(SHQueryUserNotificationState(&state))) {
        if (state == QUNS_RUNNING_D3D_FULL_SCREEN) return true;
    }

    BOOL dwmEnabled = TRUE;
    if (SUCCEEDED(DwmIsCompositionEnabled(&dwmEnabled)) && !dwmEnabled) {
        return true;
    }

    RECT rcApp;
    if (!GetWindowRect(hwnd, &rcApp)) return false;

    HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {sizeof(mi)};
    if (!GetMonitorInfo(hMonitor, &mi)) return false;

    const bool isFullscreen =
        rcApp.left   <= mi.rcMonitor.left   + kFullscreenTolerancePx &&
        rcApp.top    <= mi.rcMonitor.top    + kFullscreenTolerancePx &&
        rcApp.right  >= mi.rcMonitor.right  - kFullscreenTolerancePx &&
        rcApp.bottom >= mi.rcMonitor.bottom - kFullscreenTolerancePx;

    if (!isFullscreen) return false;

    RECT rcClip;
    if (GetClipCursor(&rcClip)) {
        const int virtualWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        const int virtualHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);

        if ((rcClip.right - rcClip.left) < virtualWidth ||
            (rcClip.bottom - rcClip.top) < virtualHeight) {
            return true;
        }
    }

    CURSORINFO ci = {sizeof(ci)};
    if (GetCursorInfo(&ci) && ci.flags == 0) return true;

    return false;
}

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
    if (width <= 0 || height <= 0 || !referenceDC) return false;

    if (g_hdcMem && g_hBitmap &&
        width <= g_cachedWidth && height <= g_cachedHeight) {
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
        g_originalBitmap = oldBitmap;
    } else if (oldBitmap && oldBitmap != g_originalBitmap) {
        DeleteObject(oldBitmap);
    }

    g_hBitmap = newBitmap;
    g_cachedWidth = newWidth;
    g_cachedHeight = newHeight;
    return true;
}

// -----------------------------------------------------------------------------
// Direct2D initialization
// -----------------------------------------------------------------------------

bool EnsureD2DResources() {
    if (!g_pD2DFactory) return false;
    if (g_pDCRenderTarget) return true;

    const D2D1_RENDER_TARGET_PROPERTIES props =
        D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(
                DXGI_FORMAT_B8G8R8A8_UNORM,
                D2D1_ALPHA_MODE_PREMULTIPLIED),
            0, 0,
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

    hr = g_pDCRenderTarget->CreateSolidColorBrush(
        ToColorF(g_glowRGB, g_glowAlpha), &g_pGlowBrush);
    if (FAILED(hr) || !g_pGlowBrush) {
        Wh_Log(L"Create glow brush failed: 0x%08X", hr);
        ReleaseRenderResources();
        return false;
    }

    g_dcBound = false;
    return true;
}

// Build/refresh the linear gradient brush used by the core when the
// gradient feature is enabled. Called lazily once per frame with the
// currently-resolved head/tail colors.
bool EnsureGradientBrush(uint32_t headRGB, uint32_t tailRGB) {
    if (!g_pDCRenderTarget) return false;

    if (g_pGradientBrush && g_pGradientStops &&
        headRGB == g_gradStopsHead && tailRGB == g_gradStopsTail) {
        return true;
    }

    if (g_pGradientBrush) { g_pGradientBrush->Release(); g_pGradientBrush = nullptr; }
    if (g_pGradientStops) { g_pGradientStops->Release(); g_pGradientStops = nullptr; }

    D2D1_GRADIENT_STOP stops[2] = {};
    stops[0].position = 0.0f;
    stops[0].color = ToColorF(headRGB, 1.0f);
    stops[1].position = 1.0f;
    stops[1].color = ToColorF(tailRGB, 0.0f);

    HRESULT hr = g_pDCRenderTarget->CreateGradientStopCollection(
        stops, 2, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP,
        &g_pGradientStops);
    if (FAILED(hr) || !g_pGradientStops) {
        Wh_Log(L"CreateGradientStopCollection failed: 0x%08X", hr);
        g_pGradientStops = nullptr;
        return false;
    }

    D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES props = {};
    props.startPoint = D2D1::Point2F(0, 0);
    props.endPoint   = D2D1::Point2F(1, 0);

    hr = g_pDCRenderTarget->CreateLinearGradientBrush(
        props, g_pGradientStops, &g_pGradientBrush);
    if (FAILED(hr) || !g_pGradientBrush) {
        Wh_Log(L"CreateLinearGradientBrush failed: 0x%08X", hr);
        g_pGradientStops->Release();
        g_pGradientStops = nullptr;
        return false;
    }

    g_gradStopsHead = headRGB;
    g_gradStopsTail = tailRGB;
    return true;
}

// -----------------------------------------------------------------------------
// Chaikin subdivision
// -----------------------------------------------------------------------------

void SmoothTrail(int iterations) {
    auto& current = g_renderCache.smoothed;
    auto& next = g_renderCache.subdivision;

    current.clear();
    next.clear();

    for (int i = 0; i < g_historyCount; ++i) {
        const POINT& p = HistoryAt(i);
        current.push_back(D2D1::Point2F(
            static_cast<float>(p.x + g_tailOffsetX),
            static_cast<float>(p.y + g_tailOffsetY)));
    }

    for (int iteration = 0; iteration < iterations; ++iteration) {
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
// Bounding box
// -----------------------------------------------------------------------------

RECT CalculateTrailBounds(float maxHalfWidth) {
    const auto& points = g_renderCache.smoothed;
    if (points.empty()) return {0, 0, 0, 0};

    float minX = points.front().x, minY = points.front().y;
    float maxX = points.front().x, maxY = points.front().y;

    for (const auto& p : points) {
        minX = std::min(minX, p.x);
        minY = std::min(minY, p.y);
        maxX = std::max(maxX, p.x);
        maxY = std::max(maxY, p.y);
    }

    const float pad = maxHalfWidth + static_cast<float>(kRenderPadding);
    minX -= pad; minY -= pad; maxX += pad; maxY += pad;

    RECT result;
    result.left   = static_cast<LONG>(floorf(minX));
    result.top    = static_cast<LONG>(floorf(minY));
    result.right  = static_cast<LONG>(ceilf(maxX));
    result.bottom = static_cast<LONG>(ceilf(maxY));

    if (result.right <= result.left) result.right = result.left + 1;
    if (result.bottom <= result.top) result.bottom = result.top + 1;
    return result;
}

// -----------------------------------------------------------------------------
// Ribbon geometry
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
    if (leftPts.size() < 2 || rightPts.size() < 2) return false;

    HRESULT hr = g_pD2DFactory->CreatePathGeometry(ppGeometry);
    if (FAILED(hr) || !*ppGeometry) return false;

    ID2D1GeometrySink* pSink = nullptr;
    hr = (*ppGeometry)->Open(&pSink);
    if (FAILED(hr) || !pSink) {
        (*ppGeometry)->Release();
        *ppGeometry = nullptr;
        return false;
    }

    pSink->SetFillMode(D2D1_FILL_MODE_WINDING);
    pSink->BeginFigure(leftPts[0], D2D1_FIGURE_BEGIN_FILLED);

    for (size_t i = 1; i < leftPts.size(); ++i) pSink->AddLine(leftPts[i]);
    for (size_t i = rightPts.size(); i-- > 0;) pSink->AddLine(rightPts[i]);

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

// Build the offset curves (left/right at a given half-width) for the ribbon.
static void BuildOffsetCurves(
    const std::vector<D2D1_POINT_2F>& pts,
    float headHalfWidth,
    float tailHalfWidth,
    float taperPower,
    std::vector<D2D1_POINT_2F>& left,
    std::vector<D2D1_POINT_2F>& right,
    float& outHeadNx, float& outHeadNy,
    float& outHeadDx, float& outHeadDy)
{
    left.clear();
    right.clear();

    const size_t count = pts.size();
    left.reserve(count);
    right.reserve(count);

    outHeadNx = 1.0f; outHeadNy = 0.0f;
    outHeadDx = 1.0f; outHeadDy = 0.0f;

    for (size_t i = 0; i < count; ++i) {
        float dx, dy;

        if (i == 0) {
            dx = pts[0].x - pts[1].x;
            dy = pts[0].y - pts[1].y;
        } else if (i == count - 1) {
            dx = pts[i - 1].x - pts[i].x;
            dy = pts[i - 1].y - pts[i].y;
        } else {
            dx = pts[i - 1].x - pts[i + 1].x;
            dy = pts[i - 1].y - pts[i + 1].y;
        }

        const float len = sqrtf(dx * dx + dy * dy);
        if (len > 0.0f) { dx /= len; dy /= len; }
        else { dx = 1.0f; dy = 0.0f; }

        if (i == 0) {
            outHeadDx = dx; outHeadDy = dy;
            outHeadNx = -dy; outHeadNy = dx;
        }

        const float nx = -dy;
        const float ny = dx;

        const float ratio = (count > 1)
            ? static_cast<float>(i) / static_cast<float>(count - 1)
            : 0.0f;

        // taper: 1 at head, 0 at tail
        float taper = powf(std::max(0.0f, 1.0f - ratio), taperPower);
        if (i == count - 1) taper = 0.0f;

        const float hw = tailHalfWidth + (headHalfWidth - tailHalfWidth) * taper;

        left.push_back(D2D1::Point2F(pts[i].x + nx * hw, pts[i].y + ny * hw));
        right.push_back(D2D1::Point2F(pts[i].x - nx * hw, pts[i].y - ny * hw));
    }
}

bool BuildTrailGeometry(
    ID2D1PathGeometry** ppGlowGeom,
    ID2D1PathGeometry** ppOutlineGeom,
    ID2D1PathGeometry** ppCoreGeom,
    float outerHeadHalf,
    float coreHeadHalf)
{
    *ppGlowGeom = nullptr;
    *ppOutlineGeom = nullptr;
    *ppCoreGeom = nullptr;

    const auto& smoothed = g_renderCache.smoothed;
    if (smoothed.size() < 2) return false;

    auto& leftOutline = g_renderCache.leftOutline;
    auto& rightOutline = g_renderCache.rightOutline;
    auto& leftCore = g_renderCache.leftCore;
    auto& rightCore = g_renderCache.rightCore;

    float headNx, headNy, headDx, headDy;

    BuildOffsetCurves(smoothed, outerHeadHalf, 0.0f, g_taperPower,
                      leftOutline, rightOutline,
                      headNx, headNy, headDx, headDy);
    BuildOffsetCurves(smoothed, coreHeadHalf, 0.0f, g_taperPower,
                      leftCore, rightCore,
                      headNx, headNy, headDx, headDy);

    if (!BuildRibbonGeometry(leftOutline, rightOutline, outerHeadHalf,
                             headNx, headNy, headDx, headDy, ppOutlineGeom)) {
        return false;
    }
    if (!BuildRibbonGeometry(leftCore, rightCore, coreHeadHalf,
                             headNx, headNy, headDx, headDy, ppCoreGeom)) {
        (*ppOutlineGeom)->Release();
        *ppOutlineGeom = nullptr;
        return false;
    }

    // Optional glow: build an oversized outline pass.
    if (g_glowEnabled && g_glowWidthFactor > 1.0f) {
        std::vector<D2D1_POINT_2F> lg, rg;
        float gNx, gNy, gDx, gDy;
        BuildOffsetCurves(smoothed,
                          outerHeadHalf * g_glowWidthFactor, 0.0f,
                          g_taperPower,
                          lg, rg,
                          gNx, gNy, gDx, gDy);

        if (!BuildRibbonGeometry(lg, rg,
                                 outerHeadHalf * g_glowWidthFactor,
                                 gNx, gNy, gDx, gDy, ppGlowGeom)) {
            // Non-fatal: just skip the glow this frame.
            *ppGlowGeom = nullptr;
        }
    }

    return true;
}

// =============================================================================
// Cursor color sampling
// =============================================================================

struct ColorCandidate {
    uint32_t rgb;
    int count;
};

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

            const int w = bm.bmWidth, h = bm.bmHeight;

            BITMAPINFO bmi = {};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = w;
            bmi.bmiHeader.biHeight = -h;
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
                hist.reserve(64);

                for (uint32_t px : pixels) {
                    const uint8_t a = (px >> 24) & 0xFF;
                    if (a < kCursorAlphaThreshold) continue;
                    hist[px & 0x00FFFFFF]++;
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

static float SampleBackgroundLuminance(POINT center, int radius) {
    const int size = radius * 2 + 1;

    HDC hdcScreen = GetScreenDC();
    if (!hdcScreen) return 0.5f;

    if (g_bgSamplerSize != size || !g_bgSamplerDC ||
        !g_bgSamplerBitmap || !g_bgSamplerPixels) {
        ReleaseBackgroundSampler();

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = size;
        bmi.bmiHeader.biHeight = -size;
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
            const uint8_t b = static_cast<uint8_t>(px & 0xFF);
            const uint8_t g = static_cast<uint8_t>((px >> 8) & 0xFF);
            const uint8_t r = static_cast<uint8_t>((px >> 16) & 0xFF);
            sum += RgbLuminance(r, g, b);
            n++;
        }
    }

    return n > 0 ? static_cast<float>(sum / n) : 0.5f;
}

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

static uint32_t DeriveOutlineColor(uint32_t coreRGB) {
    return (RgbLuminance(coreRGB) > 0.5f) ? kFallbackOuterColor
                                          : kFallbackCoreColor;
}

static uint32_t ResolveOutlineColor(uint32_t coreRGB) {
    if (g_outlineColorMode == 1) return g_manualOutlineRGB;
    return DeriveOutlineColor(coreRGB);
}

static void UpdateTrailColorIfNeeded(DWORD now) {
    static DWORD   s_lastUpdate = 0;
    static HCURSOR s_lastCursor = nullptr;
    static bool    s_initialized = false;

    if (g_trailColorMode == 0) {
        static uint32_t s_lastManualCore = 0xFFFFFFFFu;
        static uint32_t s_lastManualOutline = 0xFFFFFFFFu;
        static int      s_lastOutlineMode = -1;

        if (g_manualColorRGB   != s_lastManualCore ||
            g_manualOutlineRGB != s_lastManualOutline ||
            g_outlineColorMode != s_lastOutlineMode) {
            g_currentCoreRGB  = g_manualColorRGB;
            g_currentOuterRGB = ResolveOutlineColor(g_currentCoreRGB);

            s_lastManualCore    = g_manualColorRGB;
            s_lastManualOutline = g_manualOutlineRGB;
            s_lastOutlineMode   = g_outlineColorMode;
        }
        s_initialized = true;
        return;
    }

    bool needResample = false;

    if (!s_initialized) {
        needResample = true;
    } else if (g_autoResampleInterval > 0) {
        needResample = (now - s_lastUpdate) >=
                       static_cast<DWORD>(g_autoResampleInterval);
    } else {
        CURSORINFO ci = {sizeof(ci)};
        if (GetCursorInfo(&ci) && ci.hCursor != s_lastCursor) {
            if ((now - s_lastUpdate) >= kMinCursorChangeResampleIntervalMs) {
                needResample = true;
            }
        }
    }

    if (!needResample) return;

    {
        CURSORINFO ci = {sizeof(ci)};
        if (GetCursorInfo(&ci)) s_lastCursor = ci.hCursor;
    }
    s_lastUpdate = now;
    s_initialized = true;

    uint32_t core = kFallbackCoreColor;
    if (!AutoPickTrailColor(core)) core = kFallbackCoreColor;
    g_currentCoreRGB  = core;
    g_currentOuterRGB = ResolveOutlineColor(core);
}

// =============================================================================
// Render
// =============================================================================

bool RenderTrail(HWND hwnd) {
    if (g_historyCount < 2) return false;

    SmoothTrail(g_smoothIterations);
    if (g_renderCache.smoothed.size() < 2) return false;

    // Resolve width scale from the frozen speed sample. This keeps fading
    // trails at their last active width instead of collapsing.
    float speedNorm = 0.0f;
    if (g_speedScaling) {
        if (g_isSmearing) speedNorm = g_smoothedSpeedNorm;
        else              speedNorm = g_frozenSpeedNorm;
    } else {
        speedNorm = 1.0f;   // full width at all times
    }

    const float outerHeadHalf = g_widthMin +
        (g_widthMax - g_widthMin) * speedNorm;
    const float coreHeadHalf  = g_coreWidthMin +
        (g_coreWidthMax - g_coreWidthMin) * speedNorm;

    const float alphaScale = g_alphaMin + (g_alphaMax - g_alphaMin) * speedNorm;
    const float finalAlpha = kTrailAlpha * alphaScale * g_fadeAlpha;

    if (finalAlpha < 0.02f) return false;

    const float boundsHalfWidth = g_glowEnabled
        ? outerHeadHalf * std::max(1.0f, g_glowWidthFactor)
        : outerHeadHalf;

    const RECT bounds = CalculateTrailBounds(boundsHalfWidth);
    const int width  = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;

    HDC hdcScreen = GetScreenDC();
    if (!hdcScreen) return false;

    if (!EnsureBackbuffer(width, height, hdcScreen)) return false;
    if (!EnsureD2DResources()) return false;

    if (!g_dcBound) {
        RECT bindRect = {0, 0, g_cachedWidth, g_cachedHeight};
        HRESULT hr = g_pDCRenderTarget->BindDC(g_hdcMem, &bindRect);
        if (FAILED(hr)) return false;
        g_dcBound = true;
    }

    g_pDCRenderTarget->BeginDraw();
    g_pDCRenderTarget->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

    g_pOuterBrush->SetColor(ToColorF(g_currentOuterRGB, finalAlpha));
    g_pCoreBrush->SetColor(ToColorF(g_currentCoreRGB, finalAlpha * 1.02f));
    if (g_pGlowBrush) {
        g_pGlowBrush->SetColor(ToColorF(g_glowRGB, g_glowAlpha * finalAlpha));
    }

    g_pDCRenderTarget->SetTransform(
        D2D1::Matrix3x2F::Translation(
            -static_cast<float>(bounds.left),
            -static_cast<float>(bounds.top)));

    ID2D1PathGeometry* pGlowGeom = nullptr;
    ID2D1PathGeometry* pOutlineGeom = nullptr;
    ID2D1PathGeometry* pCoreGeom = nullptr;

    if (BuildTrailGeometry(&pGlowGeom, &pOutlineGeom, &pCoreGeom,
                           outerHeadHalf, coreHeadHalf)) {

        if (pGlowGeom && g_pGlowBrush) {
            g_pDCRenderTarget->FillGeometry(pGlowGeom, g_pGlowBrush);
        }

        g_pDCRenderTarget->FillGeometry(pOutlineGeom, g_pOuterBrush);

        // Gradient brush for the core if enabled and available; otherwise
        // fall back to the flat core brush.
        ID2D1Brush* coreBrush = g_pCoreBrush;
        bool gradientInUse = false;

        if (g_gradientEnabled && EnsureGradientBrush(g_currentCoreRGB,
                                                     g_gradientTailRGB)) {
            // Brush coordinates live in the same world space as the geometry
            // (both go through the world transform above).
            g_pGradientBrush->SetStartPoint(g_renderCache.smoothed.front());
            g_pGradientBrush->SetEndPoint(g_renderCache.smoothed.back());
            // Re-tint the gradient to reflect the current alpha by modulating
            // the endpoint colors. The linear gradient brush alpha in
            // D2D is per-stop; we baked alpha 1.0/0.0 above and rely on the
            // composite alpha of the geometry fill below.
            coreBrush = g_pGradientBrush;
            gradientInUse = true;
        }

        if (gradientInUse) {
            // The gradient stops were created with alpha 1.0 -> 0.0; to apply
            // the current fade/alpha, use the render target opacity by
            // multiplying through a wrapper. D2D has no per-draw opacity, so
            // we instead re-create stops on demand: cheap enough given the
            // gradient only refreshes when colors change.
            D2D1_GRADIENT_STOP stops[2] = {};
            stops[0].position = 0.0f;
            stops[0].color = ToColorF(g_currentCoreRGB, finalAlpha);
            stops[1].position = 1.0f;
            stops[1].color = ToColorF(g_gradientTailRGB, finalAlpha * 0.15f);

            ID2D1GradientStopCollection* newStops = nullptr;
            if (SUCCEEDED(g_pDCRenderTarget->CreateGradientStopCollection(
                    stops, 2, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP,
                    &newStops)) && newStops) {

                D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES props = {};
                props.startPoint = g_renderCache.smoothed.front();
                props.endPoint   = g_renderCache.smoothed.back();

                ID2D1LinearGradientBrush* newBrush = nullptr;
                if (SUCCEEDED(g_pDCRenderTarget->CreateLinearGradientBrush(
                        props, newStops, &newBrush)) && newBrush) {
                    g_pDCRenderTarget->FillGeometry(pCoreGeom, newBrush);
                    newBrush->Release();
                } else {
                    g_pDCRenderTarget->FillGeometry(pCoreGeom, g_pCoreBrush);
                }
                newStops->Release();
            } else {
                g_pDCRenderTarget->FillGeometry(pCoreGeom, g_pCoreBrush);
            }
        } else {
            g_pDCRenderTarget->FillGeometry(pCoreGeom, coreBrush);
        }
    }

    if (pCoreGeom)    pCoreGeom->Release();
    if (pOutlineGeom) pOutlineGeom->Release();
    if (pGlowGeom)    pGlowGeom->Release();

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
// Trail state
// -----------------------------------------------------------------------------

void UpdateTrailState(const POINT& pt, float velocity) {
    if (velocity > g_triggerVelocity && !g_isSmearing) {
        g_isSmearing = true;
        g_lowVelocityFrames = 0;
        g_fadeAlpha = 1.0f;
        g_smoothedSpeedNorm = 0.0f;   // rebuild from scratch on new trail
    } else if (velocity < g_stopVelocity && g_isSmearing) {
        ++g_lowVelocityFrames;
        if (g_lowVelocityFrames > 2) {
            g_isSmearing = false;
            g_frozenSpeedNorm = g_smoothedSpeedNorm;
        }
    } else if (velocity >= g_stopVelocity && g_isSmearing) {
        g_lowVelocityFrames = 0;
    }

    // Update smoothed speed norm while we're actively smearing.
    if (g_isSmearing) {
        float target = (velocity - g_stopVelocity) /
                       (g_triggerVelocity * 2.0f);
        target = std::clamp(target, 0.0f, 1.0f);
        g_smoothedSpeedNorm = g_smoothedSpeedNorm * 0.55f + target * 0.45f;
    }

    if (g_isSmearing) {
        // Effective length optionally shortens at low speed.
        int effectiveLen = g_tailLength;
        if (g_speedScaling) {
            const float lenScale = 0.55f + 0.45f * g_smoothedSpeedNorm;
            effectiveLen = std::max(2, static_cast<int>(g_tailLength * lenScale));
        }
        HistoryPushFront({pt.x, pt.y}, effectiveLen);
        while (g_historyCount > g_tailLength) HistoryPopBack();
    } else {
        if (g_fadeEnabled) {
            g_fadeAlpha *= g_fadeDecay;
            if (g_fadeAlpha < kFadeCutoff || g_historyCount < 2) {
                HistoryClear();
                g_fadeAlpha = 0.0f;
            }
        } else {
            if (g_historyCount > 0) {
                HistoryPopBack();
                if (g_historyCount > 0) HistoryPopBack();
            }
            g_fadeAlpha = 1.0f;
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

        case WM_HOTKEY:
            if (wParam == kHotkeyId) {
                g_hotkeySuspended = !g_hotkeySuspended;
                if (g_hotkeySuspended) {
                    HistoryClear();
                    g_isSmearing = false;
                    g_fadeAlpha = 0.0f;
                    HideOverlay();
                }
                Wh_Log(L"Trail %s", g_hotkeySuspended ? L"suspended" : L"resumed");
            }
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
    if (g_hotkeySuspended) return;

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
    g_currentVelocity = velocity;

    g_lastPos = pt;

    const bool newlyTriggered =
        (velocity > g_triggerVelocity) && !g_isSmearing;
    const bool burst =
        velocity > (g_triggerVelocity * kGameCheckBurstVelocityFactor);

    // Per-app rules: negative overrides game detection (force off), positive
    // overrides game detection (force on).
    const int appRule = CheckAppRule();

    bool suppress = false;
    if (appRule < 0) {
        suppress = true;
    } else if (appRule > 0) {
        suppress = false;
    } else if (UpdateGameState(dwTime, burst, newlyTriggered)) {
        suppress = true;
    }

    if (suppress) {
        HistoryClear();
        g_isSmearing = false;
        g_lowVelocityFrames = 0;
        g_fadeAlpha = 0.0f;
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
    HRESULT coResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(coResult)) {
        Wh_Log(L"CoInitializeEx failed: 0x%08X", coResult);
        return 0;
    }

    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                   &g_pD2DFactory);
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
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST |
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
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

    // Optional hotkey.
    bool hotkeyRegistered = false;
    if (g_hotkeyEnabled) {
        if (RegisterHotKey(hwnd, kHotkeyId,
                           MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'T')) {
            hotkeyRegistered = true;
        } else {
            Wh_Log(L"RegisterHotKey failed: %lu", GetLastError());
        }
    }

    if (!SetCoalescableTimer(hwnd, kTimerId, USER_TIMER_MINIMUM,
                             SmearTimerProc, kTimerCoalescingTolerance)) {
        Wh_Log(L"SetCoalescableTimer failed: %lu", GetLastError());
        if (hotkeyRegistered) UnregisterHotKey(hwnd, kHotkeyId);
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
    if (hotkeyRegistered) UnregisterHotKey(hwnd, kHotkeyId);
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
// Windhawk Tool Mod
// -----------------------------------------------------------------------------

BOOL WhTool_ModInit() {
    LoadSettings();

    g_threadHandle = CreateThread(nullptr, 0, OverlayThreadProc,
                                  nullptr, 0, nullptr);
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
}

void WhTool_ModSettingsChanged() {
    HWND hwnd = g_overlayHwnd.load();
    if (hwnd && IsWindow(hwnd)) {
        PostMessageW(hwnd, kSettingsChangedMessage, 0, 0);
    }
}

// -----------------------------------------------------------------------------
// Windhawk Tool Mod launcher
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
        g_toolModProcessMutex = CreateMutexW(nullptr, TRUE,
                                             L"windhawk-tool-mod_" WH_MOD_ID);
        if (!g_toolModProcessMutex) {
            Wh_Log(L"CreateMutex failed");
            ExitProcess(1);
        }

        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            Wh_Log(L"Tool mod already running (%s)", WH_MOD_ID);
            ExitProcess(1);
        }

        if (!WhTool_ModInit()) ExitProcess(1);

        IMAGE_DOS_HEADER* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(
            GetModuleHandle(nullptr));
        IMAGE_NT_HEADERS* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(
            reinterpret_cast<BYTE*>(dosHeader) + dosHeader->e_lfanew);
        DWORD entryPointRVA = ntHeaders->OptionalHeader.AddressOfEntryPoint;
        void* entryPoint = reinterpret_cast<BYTE*>(dosHeader) + entryPointRVA;

        Wh_SetFunctionHook(entryPoint,
                           reinterpret_cast<void*>(EntryPoint_Hook), nullptr);
        return TRUE;
    }

    if (isToolModProcess) return FALSE;

    g_isToolModProcessLauncher = true;
    return TRUE;
}

void Wh_ModAfterInit() {
    if (!g_isToolModProcessLauncher) return;

    WCHAR currentProcessPath[MAX_PATH];
    const DWORD pathLength = GetModuleFileNameW(
        nullptr, currentProcessPath, ARRAYSIZE(currentProcessPath));
    if (pathLength == 0 || pathLength == ARRAYSIZE(currentProcessPath)) {
        Wh_Log(L"GetModuleFileName failed");
        return;
    }

    WCHAR commandLine[MAX_PATH + 64];
    swprintf_s(commandLine, L"\"%s\" -tool-mod \"%s\"",
               currentProcessPath, WH_MOD_ID);

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
        WINBOOL, DWORD, LPVOID, LPCWSTR, LPSTARTUPINFOW,
        LPPROCESS_INFORMATION, PHANDLE);

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
