// ==WindhawkMod==
// @id              cursor-tail
// @name            Cursor Tail
// @description     Adds a smooth, speed-reactive motion-blur trail to the mouse cursor.
// @version         3.8
// @author          CYoJkoY
// @github          https://github.com/CYoJkoY
// @license         MIT
// @include         windhawk.exe
// @compilerOptions -ld2d1 -ldwmapi -lole32 -lgdi32 -lshell32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Cursor Tail

Cursor Tail adds a smooth, tapered motion-blur trail behind the mouse pointer
when it moves quickly. The trail is rendered in a transparent overlay window
with Direct2D, so it works independently of the application under the pointer.

## Features

- **Speed-reactive trail:** Width, opacity, and effective length respond to
  pointer velocity.
- **Smooth motion:** Historical cursor samples can be smoothed with Chaikin
  subdivision for a continuous ribbon.
- **Custom appearance:** Configure trail color, outline color, gradient, and
  optional glow.
- **Adaptive color:** Automatically sample the active cursor image and prefer
  colors with sufficient contrast against the screen background.
- **Fade-out:** Smoothly fade the trail after the pointer slows down.
- **Per-app rules:** Enable or disable the trail for selected executables.
- **Hotkey toggle:** Optionally use `Ctrl+Alt+T` to suspend or resume the trail.
- **Fullscreen suppression:** Automatically hide the trail for exclusive or
  borderless fullscreen applications and restore it after fullscreen ends.

## Usage

Move the pointer quickly enough to cross the configured trigger speed. The
trail length, width, smoothing, and opacity can then be tuned in the settings.
Lower the trigger speed for a more sensitive effect; increase it for fewer
trails on the desktop.

## Color modes

**Manual** mode uses the configured hexadecimal trail color. **Auto** mode
samples the current cursor image and chooses a visible color based on the
luminance around the pointer. Automatic resampling can be disabled or limited
to a custom interval.

The outline can either be derived automatically from the trail luminance or set
to a fixed hexadecimal color.

## Per-app rules

Use one rule per line:

`program.exe=on`

`program.exe=off`

Lines beginning with `#` are comments. Executable names are matched
case-insensitively.

## Fullscreen behavior

The trail is suppressed when the foreground application reports a D3D
fullscreen state or when a borderless, captionless window covers its monitor.
Entering fullscreen suppresses the overlay immediately. Leaving fullscreen
must remain stable for several samples before the trail is shown again, which
reduces flicker during application transitions.

## Performance

Cursor state is sampled at 125 Hz so velocity, trail history, and fade timing
retain their existing behavior. Layered-window submission is paced separately:
60 FPS while actively moving, 30 FPS while fading, and no layered-window
updates while the trail is idle. The back buffer and Direct2D resources are
reused between frames and resized only when the required trail bounds grow
beyond the current buffer.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- trigger_velocity: 25
  $name: Trigger speed
  $description: >-
    Minimum pointer speed in pixels per sample required to start the trail.

- stop_velocity: 10
  $name: Stop speed
  $description: >-
    Pointer speed below which an active trail begins fading. If this is set to
    the trigger speed or higher, it is automatically reduced to half the trigger speed.

- tail_length: 10
  $name: Tail length
  $description: >-
    Number of cursor samples kept for the trail. Higher values make the trail longer.

- tail_offset_x: 6
  $name: Trail X offset
  $description: >-
    Horizontal offset from the cursor hotspot to the trail head, in pixels.

- tail_offset_y: 10
  $name: Trail Y offset
  $description: >-
    Vertical offset from the cursor hotspot to the trail head, in pixels.

- speed_scaling: true
  $name: Speed-reactive shape
  $description: >-
    Increase or decrease trail width, opacity, and effective length according to pointer speed.

- width_min: 4
  $name: Minimum trail width
  $description: >-
    Half-width of the outer trail at lower speeds, in pixels.

- width_max: 14
  $name: Maximum trail width
  $description: >-
    Half-width of the outer trail at higher speeds, in pixels.

- core_width_min: 2
  $name: Minimum core width
  $description: >-
    Half-width of the inner core at lower speeds, in pixels.

- core_width_max: 9
  $name: Maximum core width
  $description: >-
    Half-width of the inner core at higher speeds, in pixels.

- alpha_min: 45
  $name: Minimum opacity
  $description: Trail opacity at lower speeds, from 0 to 100 percent.

- alpha_max: 90
  $name: Maximum opacity
  $description: Trail opacity at higher speeds, from 0 to 100 percent.

- taper_power: 10
  $name: Tail taper
  $description: >-
    Controls how quickly the trail narrows toward its tail. 10 is linear; larger values
    produce a sharper taper. Range 5-30.

- smooth_iterations: 2
  $name: Smoothing iterations
  $description: >-
    Number of Chaikin subdivision passes. Higher values produce a smoother trail
    at the cost of more points to render. Range 0-4.

- gradient_enabled: false
  $name: Tail gradient
  $description: Fade the core color toward the configured tail color.

- gradient_tail_color: "#FF00FF"
  $name: Gradient tail color
  $description: >-
    Hexadecimal RGB color used at the tail end when Tail gradient is enabled.

- glow_enabled: false
  $name: Glow
  $description: Draw an additional soft glow around the trail.

- glow_color: "#FFFFFF"
  $name: Glow color
  $description: Hexadecimal RGB color used for the glow.

- glow_width_factor: 18
  $name: Glow width
  $description: >-
    Glow width relative to the outer trail width. 18 means 1.8 times the outer width.

- glow_alpha: 25
  $name: Glow opacity
  $description: Glow opacity, from 0 to 100 percent.

- fade_enabled: true
  $name: Fade-out
  $description: Smoothly fade the trail after the pointer slows down.

- fade_decay: 90
  $name: Fade speed
  $description: >-
    Controls how quickly the trail fades. 90 means the remaining opacity is multiplied
    by 0.90 for each simulation sample.

- trail_color_mode: manual
  $name: Trail color mode
  $description: Choose a fixed trail color or automatically sample the cursor image.
  $options:
    - manual: Manual color
    - auto: Auto-sample cursor colors

- trail_color_manual: "#FFFFFF"
  $name: Manual trail color
  $description: >-
    Hexadecimal RGB color used when Trail color mode is Manual.

- outline_color_mode: auto
  $name: Outline color mode
  $description: Choose an automatic outline color or a fixed manual color.
  $options:
    - auto: Automatic outline
    - manual: Manual color

- outline_color_manual: "#000000"
  $name: Manual outline color
  $description: >-
    Hexadecimal RGB color used when Outline color mode is Manual.

- auto_resample_interval: 0
  $name: Auto-color refresh interval
  $description: >-
    How often Auto color mode resamples the cursor color, in milliseconds. Set to 0 to
    resample only when the cursor image changes.

- app_rules: ""
  $name: Per-app rules
  $description: >-
    One rule per line using `exe=on` or `exe=off`. Lines beginning with `#` are comments.
    Executable names are matched case-insensitively.

- hotkey_enabled: false
  $name: Enable hotkey
  $description: Register Ctrl+Alt+T to temporarily suspend or resume the trail.
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

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002u
#endif

// -----------------------------------------------------------------------------
// Constants
// -----------------------------------------------------------------------------

constexpr UINT kSettingsChangedMessage = WM_APP + 1;
constexpr int kHotkeyId = 0xCAFE;

constexpr int kTargetFrameRate = 125;
constexpr int kActiveRenderFrameRate = 60;
constexpr int kFadeRenderFrameRate = 30;

constexpr DWORD kFullscreenStrongCheckIntervalMs = 100;
constexpr int kFullscreenExitConfirmSamples = 3;
constexpr LONG kFullscreenTolerancePx = 2;

constexpr int kMaxTailLength = 64;
constexpr int kHistoryCapacity = kMaxTailLength;
constexpr int kRenderPadding = 2;

constexpr float kTrailAlpha = 0.86f;
constexpr float kFadeCutoff = 0.05f;

constexpr DWORD kMinCursorChangeResampleIntervalMs = 100;
constexpr int kAutoResampleIntervalMaxMs = 10000;
constexpr int kBackgroundSampleRadius = 24;
constexpr int kBackgroundSampleGrid = 5;
constexpr float kMinColorContrast = 3.0f;
constexpr uint8_t kCursorAlphaThreshold = 96;

constexpr uint32_t kFallbackCoreColor = 0x00FFFFFF;
constexpr uint32_t kFallbackOuterColor = 0x00000000;

// -----------------------------------------------------------------------------
// Runtime state
// -----------------------------------------------------------------------------

std::atomic<HWND> g_overlayHwnd{nullptr};
HANDLE g_threadHandle = nullptr;

POINT g_history[kHistoryCapacity] = {};
int g_historyHead = 0;
int g_historyCount = 0;

POINT g_lastPos = {0, 0};
bool g_isSmearing = false;
int g_lowVelocityFrames = 0;
float g_fadeAlpha = 0.0f;
float g_currentVelocity = 0.0f;
float g_smoothedSpeedNorm = 0.0f;
float g_frozenSpeedNorm = 0.5f;

bool g_hotkeySuspended = false;
bool g_windowVisible = false;

// -----------------------------------------------------------------------------
// Direct2D / GDI resources
// -----------------------------------------------------------------------------

ID2D1Factory* g_d2dFactory = nullptr;
ID2D1DCRenderTarget* g_renderTarget = nullptr;
ID2D1SolidColorBrush* g_outerBrush = nullptr;
ID2D1SolidColorBrush* g_coreBrush = nullptr;
ID2D1SolidColorBrush* g_glowBrush = nullptr;
ID2D1LinearGradientBrush* g_gradientBrush = nullptr;
ID2D1GradientStopCollection* g_gradientStops = nullptr;
ID2D1StrokeStyle* g_strokeStyle = nullptr;

uint32_t g_gradientHeadCache = 0xFFFFFFFFu;
uint32_t g_gradientTailCache = 0xFFFFFFFFu;
bool g_dcBound = false;

HDC g_backBufferDc = nullptr;
HBITMAP g_backBufferBitmap = nullptr;
HGDIOBJ g_originalBitmap = nullptr;
int g_cachedWidth = 0;
int g_cachedHeight = 0;
HDC g_screenDc = nullptr;

// -----------------------------------------------------------------------------
// Settings
// -----------------------------------------------------------------------------

float g_triggerVelocity = 25.0f;
float g_stopVelocity = 10.0f;
int g_tailOffsetX = 6;
int g_tailOffsetY = 10;
int g_tailLength = 10;

int g_speedScaling = 1;
float g_widthMin = 4.0f;
float g_widthMax = 14.0f;
float g_coreWidthMin = 2.0f;
float g_coreWidthMax = 9.0f;
float g_alphaMin = 0.45f;
float g_alphaMax = 0.90f;
float g_taperPower = 1.0f;
int g_smoothIterations = 2;

int g_gradientEnabled = 0;
uint32_t g_gradientTailRGB = 0x00FF00FF;

int g_glowEnabled = 0;
uint32_t g_glowRGB = 0x00FFFFFF;
float g_glowWidthFactor = 1.8f;
float g_glowAlpha = 0.25f;

int g_fadeEnabled = 1;
float g_fadeDecay = 0.90f;

int g_trailColorMode = 0;
uint32_t g_manualColorRGB = 0x00FFFFFF;
int g_outlineColorMode = 0;
uint32_t g_manualOutlineRGB = 0x00000000;
int g_autoResampleInterval = 0;

int g_hotkeyEnabled = 0;
uint32_t g_currentCoreRGB = kFallbackCoreColor;
uint32_t g_currentOuterRGB = kFallbackOuterColor;

// -----------------------------------------------------------------------------
// App rules
// -----------------------------------------------------------------------------

struct AppRule {
    std::wstring exe;
    bool enabled;
};

std::vector<AppRule> g_appRules;
HWND g_cachedForegroundWindow = nullptr;
int g_cachedAppRule = 0;

// -----------------------------------------------------------------------------
// Fullscreen state
// -----------------------------------------------------------------------------

bool g_fullscreenSuppressed = false;
int g_fullscreenFalseSamples = 0;
HWND g_fullscreenForegroundWindow = nullptr;
DWORD g_lastFullscreenStrongCheck = 0;
bool g_fullscreenStrongSignal = false;

// -----------------------------------------------------------------------------
// Render cache
// -----------------------------------------------------------------------------

struct TrailRenderCache {
    std::vector<D2D1_POINT_2F> smoothed;
    std::vector<D2D1_POINT_2F> subdivision;
    std::vector<float> taper;

    void ReserveForTailLength(int tailLength) {
        const size_t baseCount = static_cast<size_t>(std::max(tailLength, 2));
        const size_t maxPointCount = baseCount * 16 - 15;

        smoothed.reserve(maxPointCount);
        subdivision.reserve(maxPointCount);
        taper.reserve(maxPointCount);
    }

    void PrepareTaper() {
        taper.resize(smoothed.size());
        if (smoothed.size() <= 1) {
            if (!taper.empty()) taper[0] = 0.0f;
            return;
        }

        const float denominator = static_cast<float>(smoothed.size() - 1);
        for (size_t i = 0; i + 1 < smoothed.size(); ++i) {
            const float ratio = static_cast<float>(i) / denominator;
            taper[i] = powf(
                std::max(0.0f, 1.0f - ratio),
                g_taperPower);
        }
        taper.back() = 0.0f;
    }
};

TrailRenderCache g_renderCache;

// -----------------------------------------------------------------------------
// Color utilities
// -----------------------------------------------------------------------------

static inline float RgbLuminance(uint8_t r, uint8_t g, uint8_t b) {
    const auto linear = [](float channel) {
        channel /= 255.0f;
        return channel <= 0.03928f
            ? channel / 12.92f
            : powf((channel + 0.055f) / 1.055f, 2.4f);
    };

    return 0.2126f * linear(r)
        + 0.7152f * linear(g)
        + 0.0722f * linear(b);
}

static inline float RgbLuminance(uint32_t rgb) {
    return RgbLuminance(
        static_cast<uint8_t>((rgb >> 16) & 0xFF),
        static_cast<uint8_t>((rgb >> 8) & 0xFF),
        static_cast<uint8_t>(rgb & 0xFF));
}

static inline float ContrastRatio(float luminanceA, float luminanceB) {
    const float lo = std::min(luminanceA, luminanceB);
    const float hi = std::max(luminanceA, luminanceB);
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

    while (*str == L' ' || *str == L'\t') ++str;

    if (*str == L'#') {
        ++str;
    } else if (str[0] == L'0' && (str[1] == L'x' || str[1] == L'X')) {
        str += 2;
    }

    uint32_t value = 0;
    int digits = 0;
    while (digits < 6) {
        const wchar_t c = str[digits];
        int digit = -1;

        if (c >= L'0' && c <= L'9') {
            digit = c - L'0';
        } else if (c >= L'a' && c <= L'f') {
            digit = c - L'a' + 10;
        } else if (c >= L'A' && c <= L'F') {
            digit = c - L'A' + 10;
        }

        if (digit < 0) break;

        value = (value << 4) | static_cast<uint32_t>(digit);
        ++digits;
    }

    if (digits != 6) return false;

    out = value;
    return true;
}

static std::wstring ToLowerW(std::wstring value) {
    for (wchar_t& c : value) {
        c = static_cast<wchar_t>(towlower(c));
    }
    return value;
}

// -----------------------------------------------------------------------------
// Resource lifetime
// -----------------------------------------------------------------------------

void ReleaseRenderResources() {
    if (g_gradientBrush) {
        g_gradientBrush->Release();
        g_gradientBrush = nullptr;
    }

    if (g_gradientStops) {
        g_gradientStops->Release();
        g_gradientStops = nullptr;
    }

    if (g_strokeStyle) {
        g_strokeStyle->Release();
        g_strokeStyle = nullptr;
    }

    if (g_glowBrush) {
        g_glowBrush->Release();
        g_glowBrush = nullptr;
    }

    if (g_coreBrush) {
        g_coreBrush->Release();
        g_coreBrush = nullptr;
    }

    if (g_outerBrush) {
        g_outerBrush->Release();
        g_outerBrush = nullptr;
    }

    if (g_renderTarget) {
        g_renderTarget->Release();
        g_renderTarget = nullptr;
    }

    g_gradientHeadCache = 0xFFFFFFFFu;
    g_gradientTailCache = 0xFFFFFFFFu;
    g_dcBound = false;
}

void ReleaseBackbuffer() {
    ReleaseRenderResources();

    if (g_backBufferDc) {
        if (g_originalBitmap) {
            SelectObject(g_backBufferDc, g_originalBitmap);
            g_originalBitmap = nullptr;
        }

        DeleteDC(g_backBufferDc);
        g_backBufferDc = nullptr;
    }

    if (g_backBufferBitmap) {
        DeleteObject(g_backBufferBitmap);
        g_backBufferBitmap = nullptr;
    }

    g_cachedWidth = 0;
    g_cachedHeight = 0;
}

HBITMAP Create32BitDIB(int width, int height) {
    if (width <= 0 || height <= 0) return nullptr;

    BITMAPINFO bitmapInfo = {};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = width;
    bitmapInfo.bmiHeader.biHeight = -height;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    return CreateDIBSection(
        nullptr,
        &bitmapInfo,
        DIB_RGB_COLORS,
        &bits,
        nullptr,
        0);
}

HDC GetScreenDC() {
    if (!g_screenDc) {
        g_screenDc = GetDC(nullptr);
    }

    return g_screenDc;
}

void ReleaseScreenDC() {
    if (!g_screenDc) return;

    ReleaseDC(nullptr, g_screenDc);
    g_screenDc = nullptr;
}

void HideOverlay() {
    const HWND hwnd = g_overlayHwnd.load();
    if (!hwnd || !IsWindow(hwnd) || !g_windowVisible) return;

    ShowWindow(hwnd, SW_HIDE);
    g_windowVisible = false;
}

void ShowOverlay() {
    const HWND hwnd = g_overlayHwnd.load();
    if (!hwnd || !IsWindow(hwnd) || g_windowVisible) return;

    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    g_windowVisible = true;
}

// -----------------------------------------------------------------------------
// History
// -----------------------------------------------------------------------------

void HistoryClear() {
    g_historyHead = 0;
    g_historyCount = 0;
}

void HistoryPushFront(const POINT& point, int maxLength) {
    if (g_historyCount == kHistoryCapacity) {
        --g_historyCount;
    }

    g_historyHead = (g_historyHead - 1 + kHistoryCapacity) % kHistoryCapacity;
    g_history[g_historyHead] = point;
    ++g_historyCount;

    if (g_historyCount > maxLength) {
        g_historyCount = maxLength;
    }
}

void HistoryPopBack() {
    if (g_historyCount > 0) {
        --g_historyCount;
    }
}

const POINT& HistoryAt(int index) {
    return g_history[(g_historyHead + index) % kHistoryCapacity];
}

// -----------------------------------------------------------------------------
// Per-app rules
// -----------------------------------------------------------------------------

static bool GetProcessExeName(DWORD pid, std::wstring& out) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return false;

    WCHAR path[512] = {};
    DWORD size = ARRAYSIZE(path);
    const BOOL success = QueryFullProcessImageNameW(
        process,
        0,
        path,
        &size);
    CloseHandle(process);

    if (!success) return false;

    const wchar_t* name = wcsrchr(path, L'\\');
    out = ToLowerW(name ? name + 1 : path);
    return true;
}

static void ParseAppRules(const wchar_t* text) {
    g_appRules.clear();
    if (!text) return;

    const std::wstring input(text);
    size_t start = 0;

    while (start <= input.size()) {
        size_t end = input.find_first_of(L"\r\n", start);
        if (end == std::wstring::npos) {
            end = input.size();
        }

        std::wstring line = input.substr(start, end - start);
        const size_t comment = line.find(L'#');
        if (comment != std::wstring::npos) {
            line.resize(comment);
        }

        const size_t equals = line.find(L'=');
        if (equals != std::wstring::npos) {
            std::wstring exe = line.substr(0, equals);
            std::wstring value = line.substr(equals + 1);

            const auto trim = [](std::wstring& value) {
                const size_t first = value.find_first_not_of(L" \t");
                if (first == std::wstring::npos) {
                    value.clear();
                    return;
                }

                const size_t last = value.find_last_not_of(L" \t");
                value = value.substr(first, last - first + 1);
            };

            trim(exe);
            trim(value);

            if (!exe.empty()) {
                g_appRules.push_back({
                    ToLowerW(exe),
                    value == L"on" || value == L"1"
                        || value == L"true" || value == L"yes"
                });
            }
        }

        if (end == input.size()) break;

        start = input.find_first_not_of(L"\r\n", end);
        if (start == std::wstring::npos) break;
    }
}

static int CheckAppRuleCached(HWND foreground) {
    if (foreground == g_cachedForegroundWindow) {
        return g_cachedAppRule;
    }

    g_cachedForegroundWindow = foreground;
    g_cachedAppRule = 0;

    if (!foreground || g_appRules.empty()) {
        return 0;
    }

    DWORD pid = 0;
    GetWindowThreadProcessId(foreground, &pid);
    if (!pid) return 0;

    std::wstring exe;
    if (!GetProcessExeName(pid, exe)) return 0;

    for (const auto& rule : g_appRules) {
        if (rule.exe != exe) continue;

        g_cachedAppRule = rule.enabled ? 1 : -1;
        break;
    }

    return g_cachedAppRule;
}

// -----------------------------------------------------------------------------
// Settings loading
// -----------------------------------------------------------------------------

void LoadSettings() {
    g_triggerVelocity = static_cast<float>(
        std::clamp(Wh_GetIntSetting(L"trigger_velocity"), 1, 500));
    g_stopVelocity = static_cast<float>(
        std::clamp(Wh_GetIntSetting(L"stop_velocity"), 1, 500));

    if (g_stopVelocity >= g_triggerVelocity) {
        g_stopVelocity = std::max(1.0f, g_triggerVelocity * 0.5f);
    }

    g_tailOffsetX = std::clamp(
        Wh_GetIntSetting(L"tail_offset_x"), -64, 64);
    g_tailOffsetY = std::clamp(
        Wh_GetIntSetting(L"tail_offset_y"), -64, 64);
    g_tailLength = std::clamp(
        Wh_GetIntSetting(L"tail_length"), 2, kMaxTailLength);

    g_speedScaling = std::clamp(
        Wh_GetIntSetting(L"speed_scaling"), 0, 1);
    g_widthMin = static_cast<float>(
        std::clamp(Wh_GetIntSetting(L"width_min"), 1, 40));
    g_widthMax = static_cast<float>(
        std::clamp(Wh_GetIntSetting(L"width_max"), 1, 60));
    g_coreWidthMin = static_cast<float>(
        std::clamp(Wh_GetIntSetting(L"core_width_min"), 1, 40));
    g_coreWidthMax = static_cast<float>(
        std::clamp(Wh_GetIntSetting(L"core_width_max"), 1, 60));
    g_alphaMin = std::clamp(
        Wh_GetIntSetting(L"alpha_min"), 0, 100) / 100.0f;
    g_alphaMax = std::clamp(
        Wh_GetIntSetting(L"alpha_max"), 0, 100) / 100.0f;
    g_taperPower = std::clamp(
        Wh_GetIntSetting(L"taper_power"), 5, 30) / 10.0f;

    if (g_widthMax < g_widthMin) {
        std::swap(g_widthMin, g_widthMax);
    }
    if (g_coreWidthMax < g_coreWidthMin) {
        std::swap(g_coreWidthMin, g_coreWidthMax);
    }

    g_smoothIterations = std::clamp(
        Wh_GetIntSetting(L"smooth_iterations"), 0, 4);

    g_gradientEnabled = std::clamp(
        Wh_GetIntSetting(L"gradient_enabled"), 0, 1);
    {
        PCWSTR value = Wh_GetStringSetting(L"gradient_tail_color");
        uint32_t parsed = 0;
        g_gradientTailRGB =
            value && ParseHexColor(value, parsed) ? parsed : 0x00FF00FF;
        if (value) Wh_FreeStringSetting(value);
    }

    g_glowEnabled = std::clamp(
        Wh_GetIntSetting(L"glow_enabled"), 0, 1);
    {
        PCWSTR value = Wh_GetStringSetting(L"glow_color");
        uint32_t parsed = 0;
        g_glowRGB =
            value && ParseHexColor(value, parsed) ? parsed : 0x00FFFFFF;
        if (value) Wh_FreeStringSetting(value);
    }
    g_glowWidthFactor = std::clamp(
        Wh_GetIntSetting(L"glow_width_factor"), 10, 30) / 10.0f;
    g_glowAlpha = std::clamp(
        Wh_GetIntSetting(L"glow_alpha"), 0, 100) / 100.0f;

    g_fadeEnabled = std::clamp(
        Wh_GetIntSetting(L"fade_enabled"), 0, 1);
    g_fadeDecay = std::clamp(
        Wh_GetIntSetting(L"fade_decay"), 50, 99) / 100.0f;

    {
        PCWSTR value = Wh_GetStringSetting(L"trail_color_mode");
        g_trailColorMode = value && _wcsicmp(value, L"auto") == 0 ? 1 : 0;
        if (value) Wh_FreeStringSetting(value);
    }
    {
        PCWSTR value = Wh_GetStringSetting(L"trail_color_manual");
        uint32_t parsed = 0;
        g_manualColorRGB =
            value && ParseHexColor(value, parsed) ? parsed : kFallbackCoreColor;
        if (value) Wh_FreeStringSetting(value);
    }
    {
        PCWSTR value = Wh_GetStringSetting(L"outline_color_mode");
        g_outlineColorMode =
            value && _wcsicmp(value, L"manual") == 0 ? 1 : 0;
        if (value) Wh_FreeStringSetting(value);
    }
    {
        PCWSTR value = Wh_GetStringSetting(L"outline_color_manual");
        uint32_t parsed = 0;
        g_manualOutlineRGB =
            value && ParseHexColor(value, parsed) ? parsed : kFallbackOuterColor;
        if (value) Wh_FreeStringSetting(value);
    }

    g_autoResampleInterval = std::clamp(
        Wh_GetIntSetting(L"auto_resample_interval"),
        0,
        kAutoResampleIntervalMaxMs);

    {
        PCWSTR value = Wh_GetStringSetting(L"app_rules");
        ParseAppRules(value);
        if (value) Wh_FreeStringSetting(value);
    }

    g_cachedForegroundWindow = nullptr;
    g_cachedAppRule = 0;
    g_hotkeyEnabled = std::clamp(
        Wh_GetIntSetting(L"hotkey_enabled"), 0, 1);

    g_renderCache.ReserveForTailLength(g_tailLength);
}

// -----------------------------------------------------------------------------
// Fullscreen detection
// -----------------------------------------------------------------------------

static bool CoversMonitor(HWND hwnd) {
    const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    if ((style & WS_MAXIMIZE) != 0 || (style & WS_CAPTION) != 0) {
        return false;
    }

    RECT windowRect = {};
    if (!GetWindowRect(hwnd, &windowRect)) return false;

    const HMONITOR monitor = MonitorFromWindow(
        hwnd,
        MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo = {sizeof(monitorInfo)};
    if (!GetMonitorInfoW(monitor, &monitorInfo)) return false;

    return windowRect.left <= monitorInfo.rcMonitor.left + kFullscreenTolerancePx
        && windowRect.top <= monitorInfo.rcMonitor.top + kFullscreenTolerancePx
        && windowRect.right >= monitorInfo.rcMonitor.right - kFullscreenTolerancePx
        && windowRect.bottom >= monitorInfo.rcMonitor.bottom - kFullscreenTolerancePx;
}

static bool CheckStrongFullscreenSignal(HWND hwnd) {
    if (!hwnd) return false;

    QUERY_USER_NOTIFICATION_STATE state = QUNS_NOT_PRESENT;
    return SUCCEEDED(SHQueryUserNotificationState(&state))
        && state == QUNS_RUNNING_D3D_FULL_SCREEN;
}

static bool IsFullscreenCandidate(DWORD now, HWND hwnd) {
    if (!hwnd
        || hwnd == GetDesktopWindow()
        || hwnd == GetShellWindow()
        || IsIconic(hwnd)
        || !IsWindowVisible(hwnd)) {
        return false;
    }

    if (hwnd != g_fullscreenForegroundWindow) {
        g_fullscreenForegroundWindow = hwnd;
        g_lastFullscreenStrongCheck = 0;
        g_fullscreenStrongSignal = false;
        g_fullscreenFalseSamples = 0;
    }

    if (now - g_lastFullscreenStrongCheck >= kFullscreenStrongCheckIntervalMs) {
        g_lastFullscreenStrongCheck = now;
        g_fullscreenStrongSignal = CheckStrongFullscreenSignal(hwnd);
    }

    return g_fullscreenStrongSignal || CoversMonitor(hwnd);
}

static bool UpdateFullscreenState(DWORD now, HWND foreground) {
    const bool candidate = IsFullscreenCandidate(now, foreground);

    if (candidate) {
        g_fullscreenFalseSamples = 0;

        if (!g_fullscreenSuppressed) {
            g_fullscreenSuppressed = true;
            HistoryClear();
            g_isSmearing = false;
            g_lowVelocityFrames = 0;
            g_fadeAlpha = 0.0f;
            HideOverlay();
        }

        return true;
    }

    if (g_fullscreenSuppressed) {
        if (++g_fullscreenFalseSamples >= kFullscreenExitConfirmSamples) {
            g_fullscreenSuppressed = false;
            g_fullscreenFalseSamples = 0;
            HistoryClear();
            g_isSmearing = false;
            g_lowVelocityFrames = 0;
            g_fadeAlpha = 0.0f;
            HideOverlay();
        }

        return g_fullscreenSuppressed;
    }

    g_fullscreenFalseSamples = 0;
    return false;
}

// -----------------------------------------------------------------------------
// Back buffer and Direct2D
// -----------------------------------------------------------------------------

bool EnsureBackbuffer(int width, int height, HDC referenceDc) {
    if (width <= 0 || height <= 0 || !referenceDc) return false;

    if (g_backBufferDc && g_backBufferBitmap
        && width <= g_cachedWidth && height <= g_cachedHeight) {
        return true;
    }

    if (!g_backBufferDc) {
        g_backBufferDc = CreateCompatibleDC(referenceDc);
        if (!g_backBufferDc) {
            Wh_Log(L"CreateCompatibleDC failed");
            return false;
        }
    }

    const int newWidth = std::max(width, g_cachedWidth);
    const int newHeight = std::max(height, g_cachedHeight);

    ReleaseRenderResources();

    HBITMAP bitmap = Create32BitDIB(newWidth, newHeight);
    if (!bitmap) {
        Wh_Log(L"Create32BitDIB failed: %dx%d", newWidth, newHeight);
        return false;
    }

    const HGDIOBJ oldBitmap = SelectObject(g_backBufferDc, bitmap);

    if (!g_originalBitmap) {
        g_originalBitmap = oldBitmap;
    } else if (oldBitmap && oldBitmap != g_originalBitmap) {
        DeleteObject(oldBitmap);
    }

    g_backBufferBitmap = bitmap;
    g_cachedWidth = newWidth;
    g_cachedHeight = newHeight;
    return true;
}

bool EnsureD2DResources() {
    if (!g_d2dFactory) return false;
    if (g_renderTarget) return true;

    const D2D1_RENDER_TARGET_PROPERTIES properties =
        D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(
                DXGI_FORMAT_B8G8R8A8_UNORM,
                D2D1_ALPHA_MODE_PREMULTIPLIED));

    HRESULT hr = g_d2dFactory->CreateDCRenderTarget(
        &properties,
        &g_renderTarget);
    if (FAILED(hr) || !g_renderTarget) {
        Wh_Log(L"CreateDCRenderTarget failed: 0x%08X", hr);
        g_renderTarget = nullptr;
        return false;
    }

    hr = g_renderTarget->CreateSolidColorBrush(
        ToColorF(g_currentOuterRGB, kTrailAlpha),
        &g_outerBrush);
    if (FAILED(hr) || !g_outerBrush) {
        Wh_Log(L"Create outer brush failed: 0x%08X", hr);
        ReleaseRenderResources();
        return false;
    }

    hr = g_renderTarget->CreateSolidColorBrush(
        ToColorF(g_currentCoreRGB, kTrailAlpha),
        &g_coreBrush);
    if (FAILED(hr) || !g_coreBrush) {
        Wh_Log(L"Create core brush failed: 0x%08X", hr);
        ReleaseRenderResources();
        return false;
    }

    hr = g_renderTarget->CreateSolidColorBrush(
        ToColorF(g_glowRGB, g_glowAlpha),
        &g_glowBrush);
    if (FAILED(hr) || !g_glowBrush) {
        Wh_Log(L"Create glow brush failed: 0x%08X", hr);
        ReleaseRenderResources();
        return false;
    }

    D2D1_STROKE_STYLE_PROPERTIES strokeProperties = {};
    strokeProperties.startCap = D2D1_CAP_STYLE_ROUND;
    strokeProperties.endCap = D2D1_CAP_STYLE_ROUND;
    strokeProperties.lineJoin = D2D1_LINE_JOIN_ROUND;

    hr = g_d2dFactory->CreateStrokeStyle(
        strokeProperties,
        nullptr,
        0,
        &g_strokeStyle);
    if (FAILED(hr) || !g_strokeStyle) {
        Wh_Log(L"CreateStrokeStyle failed: 0x%08X", hr);
        ReleaseRenderResources();
        return false;
    }

    return true;
}

bool EnsureGradientBrush() {
    if (!g_gradientEnabled || !g_renderTarget) return false;

    if (g_gradientBrush && g_gradientStops
        && g_gradientHeadCache == g_currentCoreRGB
        && g_gradientTailCache == g_gradientTailRGB) {
        return true;
    }

    if (g_gradientBrush) {
        g_gradientBrush->Release();
        g_gradientBrush = nullptr;
    }
    if (g_gradientStops) {
        g_gradientStops->Release();
        g_gradientStops = nullptr;
    }

    D2D1_GRADIENT_STOP stops[2] = {};
    stops[0].position = 0.0f;
    stops[0].color = ToColorF(g_currentCoreRGB, 1.0f);
    stops[1].position = 1.0f;
    stops[1].color = ToColorF(g_gradientTailRGB, 0.15f);

    HRESULT hr = g_renderTarget->CreateGradientStopCollection(
        stops,
        2,
        D2D1_GAMMA_2_2,
        D2D1_EXTEND_MODE_CLAMP,
        &g_gradientStops);
    if (FAILED(hr) || !g_gradientStops) {
        Wh_Log(L"CreateGradientStopCollection failed: 0x%08X", hr);
        return false;
    }

    const D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES properties = {};
    hr = g_renderTarget->CreateLinearGradientBrush(
        properties,
        g_gradientStops,
        &g_gradientBrush);
    if (FAILED(hr) || !g_gradientBrush) {
        Wh_Log(L"CreateLinearGradientBrush failed: 0x%08X", hr);
        g_gradientStops->Release();
        g_gradientStops = nullptr;
        return false;
    }

    g_gradientHeadCache = g_currentCoreRGB;
    g_gradientTailCache = g_gradientTailRGB;
    return true;
}

// -----------------------------------------------------------------------------
// Trail processing
// -----------------------------------------------------------------------------

void SmoothTrail(int iterations) {
    auto& current = g_renderCache.smoothed;
    auto& next = g_renderCache.subdivision;

    current.clear();
    next.clear();

    for (int i = 0; i < g_historyCount; ++i) {
        const POINT& point = HistoryAt(i);
        current.push_back(D2D1::Point2F(
            static_cast<float>(point.x + g_tailOffsetX),
            static_cast<float>(point.y + g_tailOffsetY)));
    }

    for (int iteration = 0;
         iteration < iterations && current.size() >= 3;
         ++iteration) {
        next.clear();
        next.reserve(current.size() * 2);
        next.push_back(current.front());

        for (size_t i = 0; i + 1 < current.size(); ++i) {
            const auto& a = current[i];
            const auto& b = current[i + 1];

            next.push_back(D2D1::Point2F(
                0.75f * a.x + 0.25f * b.x,
                0.75f * a.y + 0.25f * b.y));
            next.push_back(D2D1::Point2F(
                0.25f * a.x + 0.75f * b.x,
                0.25f * a.y + 0.75f * b.y));
        }

        next.push_back(current.back());
        current.swap(next);
    }

    g_renderCache.PrepareTaper();
}

RECT CalculateTrailBounds(float maxHalfWidth) {
    if (g_renderCache.smoothed.empty()) {
        return {0, 0, 0, 0};
    }

    float minX = g_renderCache.smoothed.front().x;
    float minY = g_renderCache.smoothed.front().y;
    float maxX = minX;
    float maxY = minY;

    for (const auto& point : g_renderCache.smoothed) {
        minX = std::min(minX, point.x);
        minY = std::min(minY, point.y);
        maxX = std::max(maxX, point.x);
        maxY = std::max(maxY, point.y);
    }

    const float padding = maxHalfWidth + kRenderPadding;
    RECT result = {
        static_cast<LONG>(floorf(minX - padding)),
        static_cast<LONG>(floorf(minY - padding)),
        static_cast<LONG>(ceilf(maxX + padding)),
        static_cast<LONG>(ceilf(maxY + padding))
    };

    if (result.right <= result.left) {
        result.right = result.left + 1;
    }
    if (result.bottom <= result.top) {
        result.bottom = result.top + 1;
    }

    return result;
}

static void DrawTrailStroke(
    ID2D1RenderTarget* target,
    ID2D1Brush* brush,
    float halfWidth,
    bool drawHead) {
    const auto& points = g_renderCache.smoothed;
    const auto& taper = g_renderCache.taper;
    if (points.size() < 2 || taper.size() < points.size() || !brush) {
        return;
    }

    for (size_t i = 0; i + 1 < points.size(); ++i) {
        const float width = std::max(
            0.1f,
            halfWidth * taper[i] * 2.0f);
        target->DrawLine(
            points[i],
            points[i + 1],
            brush,
            width,
            g_strokeStyle);
    }

    if (drawHead) {
        target->FillEllipse(
            D2D1::Ellipse(points.front(), halfWidth, halfWidth),
            brush);
    }
}

void UpdateTrailState(const POINT& point, float velocity) {
    if (velocity > g_triggerVelocity && !g_isSmearing) {
        g_isSmearing = true;
        g_lowVelocityFrames = 0;
        g_fadeAlpha = 1.0f;
        g_smoothedSpeedNorm = 0.0f;
    } else if (velocity < g_stopVelocity && g_isSmearing) {
        if (++g_lowVelocityFrames > 2) {
            g_isSmearing = false;
            g_frozenSpeedNorm = g_smoothedSpeedNorm;
        }
    } else if (g_isSmearing) {
        g_lowVelocityFrames = 0;
    }

    if (g_isSmearing) {
        const float target = std::clamp(
            (velocity - g_stopVelocity)
                / (g_triggerVelocity * 2.0f),
            0.0f,
            1.0f);

        g_smoothedSpeedNorm = g_smoothedSpeedNorm * 0.55f
            + target * 0.45f;

        int effectiveLength = g_tailLength;
        if (g_speedScaling) {
            effectiveLength = std::max(
                2,
                static_cast<int>(
                    g_tailLength
                    * (0.55f + 0.45f * g_smoothedSpeedNorm)));
        }

        HistoryPushFront(point, effectiveLength);
        while (g_historyCount > g_tailLength) {
            HistoryPopBack();
        }
    } else if (g_fadeEnabled) {
        g_fadeAlpha *= g_fadeDecay;

        if (g_fadeAlpha < kFadeCutoff || g_historyCount < 2) {
            HistoryClear();
            g_fadeAlpha = 0.0f;
        }
    } else {
        if (g_historyCount > 0) HistoryPopBack();
        if (g_historyCount > 0) HistoryPopBack();
        g_fadeAlpha = 1.0f;
    }
}

// -----------------------------------------------------------------------------
// Cursor color sampling
// -----------------------------------------------------------------------------

struct ColorCandidate {
    uint32_t rgb;
    int count;
};

static bool ExtractCursorColorCandidates(
    std::vector<ColorCandidate>& out) {
    out.clear();

    CURSORINFO cursorInfo = {sizeof(cursorInfo)};
    if (!GetCursorInfo(&cursorInfo)
        || !(cursorInfo.flags & CURSOR_SHOWING)
        || !cursorInfo.hCursor) {
        return false;
    }

    ICONINFO iconInfo = {};
    if (!GetIconInfo(cursorInfo.hCursor, &iconInfo)) return false;

    const auto cleanup = [&] {
        if (iconInfo.hbmColor) DeleteObject(iconInfo.hbmColor);
        if (iconInfo.hbmMask) DeleteObject(iconInfo.hbmMask);
    };

    if (iconInfo.hbmColor) {
        BITMAP bitmap = {};
        if (GetObject(
                iconInfo.hbmColor,
                sizeof(bitmap),
                &bitmap)
            && bitmap.bmWidth > 0
            && bitmap.bmHeight > 0
            && bitmap.bmWidth <= 256
            && bitmap.bmHeight <= 256) {
            const int width = bitmap.bmWidth;
            const int height = bitmap.bmHeight;

            BITMAPINFO bitmapInfo = {};
            bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bitmapInfo.bmiHeader.biWidth = width;
            bitmapInfo.bmiHeader.biHeight = -height;
            bitmapInfo.bmiHeader.biPlanes = 1;
            bitmapInfo.bmiHeader.biBitCount = 32;
            bitmapInfo.bmiHeader.biCompression = BI_RGB;

            std::vector<uint32_t> pixels(
                static_cast<size_t>(width) * height);
            HDC screen = GetScreenDC();

            if (screen
                && GetDIBits(
                    screen,
                    iconInfo.hbmColor,
                    0,
                    height,
                    pixels.data(),
                    &bitmapInfo,
                    DIB_RGB_COLORS) == height) {
                std::unordered_map<uint32_t, int> histogram;
                histogram.reserve(64);

                for (uint32_t pixel : pixels) {
                    if (((pixel >> 24) & 0xFF) < kCursorAlphaThreshold) {
                        continue;
                    }

                    ++histogram[pixel & 0x00FFFFFF];
                }

                out.reserve(histogram.size());
                for (const auto& entry : histogram) {
                    out.push_back({entry.first, entry.second});
                }

                std::sort(
                    out.begin(),
                    out.end(),
                    [](const ColorCandidate& a, const ColorCandidate& b) {
                        return a.count > b.count;
                    });

                cleanup();
                return !out.empty();
            }
        }
    }

    cleanup();
    out.push_back({kFallbackCoreColor, 1});
    return true;
}

HDC g_backgroundSamplerDc = nullptr;
HBITMAP g_backgroundSamplerBitmap = nullptr;
HGDIOBJ g_backgroundSamplerOriginal = nullptr;
uint32_t* g_backgroundSamplerPixels = nullptr;
int g_backgroundSamplerSize = 0;

static void ReleaseBackgroundSampler() {
    if (g_backgroundSamplerDc) {
        if (g_backgroundSamplerOriginal) {
            SelectObject(
                g_backgroundSamplerDc,
                g_backgroundSamplerOriginal);
        }

        DeleteDC(g_backgroundSamplerDc);
        g_backgroundSamplerDc = nullptr;
    }

    if (g_backgroundSamplerBitmap) {
        DeleteObject(g_backgroundSamplerBitmap);
        g_backgroundSamplerBitmap = nullptr;
    }

    g_backgroundSamplerOriginal = nullptr;
    g_backgroundSamplerPixels = nullptr;
    g_backgroundSamplerSize = 0;
}

static float SampleBackgroundLuminance(POINT center) {
    const int size = kBackgroundSampleRadius * 2 + 1;
    HDC screen = GetScreenDC();
    if (!screen) return 0.5f;

    if (g_backgroundSamplerSize != size
        || !g_backgroundSamplerDc
        || !g_backgroundSamplerBitmap
        || !g_backgroundSamplerPixels) {
        ReleaseBackgroundSampler();

        BITMAPINFO bitmapInfo = {};
        bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmapInfo.bmiHeader.biWidth = size;
        bitmapInfo.bmiHeader.biHeight = -size;
        bitmapInfo.bmiHeader.biPlanes = 1;
        bitmapInfo.bmiHeader.biBitCount = 32;
        bitmapInfo.bmiHeader.biCompression = BI_RGB;

        void* bits = nullptr;
        g_backgroundSamplerBitmap = CreateDIBSection(
            nullptr,
            &bitmapInfo,
            DIB_RGB_COLORS,
            &bits,
            nullptr,
            0);
        if (!g_backgroundSamplerBitmap || !bits) {
            ReleaseBackgroundSampler();
            return 0.5f;
        }

        g_backgroundSamplerPixels = static_cast<uint32_t*>(bits);
        g_backgroundSamplerDc = CreateCompatibleDC(screen);
        if (!g_backgroundSamplerDc) {
            ReleaseBackgroundSampler();
            return 0.5f;
        }

        g_backgroundSamplerOriginal = SelectObject(
            g_backgroundSamplerDc,
            g_backgroundSamplerBitmap);
        g_backgroundSamplerSize = size;
    }

    if (!BitBlt(
            g_backgroundSamplerDc,
            0,
            0,
            size,
            size,
            screen,
            center.x - kBackgroundSampleRadius,
            center.y - kBackgroundSampleRadius,
            SRCCOPY)) {
        return 0.5f;
    }

    double sum = 0.0;
    int count = 0;

    for (int yIndex = 0; yIndex < kBackgroundSampleGrid; ++yIndex) {
        const int y = (size - 1) * yIndex
            / (kBackgroundSampleGrid - 1);

        for (int xIndex = 0; xIndex < kBackgroundSampleGrid; ++xIndex) {
            const int x = (size - 1) * xIndex
                / (kBackgroundSampleGrid - 1);
            const uint32_t pixel =
                g_backgroundSamplerPixels[y * size + x];

            sum += RgbLuminance(
                static_cast<uint8_t>((pixel >> 16) & 0xFF),
                static_cast<uint8_t>((pixel >> 8) & 0xFF),
                static_cast<uint8_t>(pixel & 0xFF));
            ++count;
        }
    }

    return count
        ? static_cast<float>(sum / count)
        : 0.5f;
}

static uint32_t ResolveOutlineColor(uint32_t core) {
    if (g_outlineColorMode == 1) {
        return g_manualOutlineRGB;
    }

    return RgbLuminance(core) > 0.5f
        ? kFallbackOuterColor
        : kFallbackCoreColor;
}

static void UpdateTrailColorIfNeeded(DWORD now) {
    static DWORD lastUpdate = 0;
    static HCURSOR lastCursor = nullptr;
    static bool initialized = false;

    if (g_trailColorMode == 0) {
        static uint32_t lastCore = 0xFFFFFFFFu;
        static uint32_t lastOutline = 0xFFFFFFFFu;
        static int lastOutlineMode = -1;

        if (lastCore != g_manualColorRGB
            || lastOutline != g_manualOutlineRGB
            || lastOutlineMode != g_outlineColorMode) {
            g_currentCoreRGB = g_manualColorRGB;
            g_currentOuterRGB = ResolveOutlineColor(g_currentCoreRGB);
            lastCore = g_manualColorRGB;
            lastOutline = g_manualOutlineRGB;
            lastOutlineMode = g_outlineColorMode;
        }

        initialized = true;
        return;
    }

    bool resample = !initialized;
    CURSORINFO cursorInfo = {sizeof(cursorInfo)};

    if (!resample && GetCursorInfo(&cursorInfo)) {
        if (g_autoResampleInterval > 0) {
            resample =
                now - lastUpdate >= static_cast<DWORD>(
                    g_autoResampleInterval);
        } else if (cursorInfo.hCursor != lastCursor) {
            resample =
                now - lastUpdate >= kMinCursorChangeResampleIntervalMs;
        }
    }

    if (!resample) return;

    if (GetCursorInfo(&cursorInfo)) {
        lastCursor = cursorInfo.hCursor;
    }
    lastUpdate = now;
    initialized = true;

    POINT point = {};
    if (!GetCursorPos(&point)) return;

    std::vector<ColorCandidate> candidates;
    if (!ExtractCursorColorCandidates(candidates) || candidates.empty()) {
        return;
    }

    const float background = SampleBackgroundLuminance(point);
    for (const auto& candidate : candidates) {
        if (ContrastRatio(
                RgbLuminance(candidate.rgb),
                background) >= kMinColorContrast) {
            g_currentCoreRGB = candidate.rgb;
            g_currentOuterRGB = ResolveOutlineColor(candidate.rgb);
            return;
        }
    }

    g_currentCoreRGB = kFallbackCoreColor;
    g_currentOuterRGB = ResolveOutlineColor(g_currentCoreRGB);
}

// -----------------------------------------------------------------------------
// Rendering
// -----------------------------------------------------------------------------

bool RenderTrail(HWND hwnd) {
    if (g_historyCount < 2) return false;

    SmoothTrail(g_smoothIterations);
    if (g_renderCache.smoothed.size() < 2) return false;

    const float speed = g_speedScaling
        ? (g_isSmearing ? g_smoothedSpeedNorm : g_frozenSpeedNorm)
        : 1.0f;

    const float outerHalf = g_widthMin
        + (g_widthMax - g_widthMin) * speed;
    const float coreHalf = g_coreWidthMin
        + (g_coreWidthMax - g_coreWidthMin) * speed;

    const float alphaScale = g_alphaMin
        + (g_alphaMax - g_alphaMin) * speed;
    const float alpha = kTrailAlpha * alphaScale * g_fadeAlpha;
    if (alpha < 0.02f) return false;

    const float boundsHalf = g_glowEnabled
        ? outerHalf * std::max(1.0f, g_glowWidthFactor)
        : outerHalf;

    const RECT bounds = CalculateTrailBounds(boundsHalf);
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;

    HDC screen = GetScreenDC();
    if (!screen) return false;
    if (!EnsureBackbuffer(width, height, screen)) return false;
    if (!EnsureD2DResources()) return false;

    if (!g_dcBound) {
        const RECT bindRect = {
            0,
            0,
            g_cachedWidth,
            g_cachedHeight
        };

        if (FAILED(g_renderTarget->BindDC(g_backBufferDc, &bindRect))) {
            return false;
        }

        g_dcBound = true;
    }

    g_renderTarget->BeginDraw();
    g_renderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));
    g_renderTarget->SetTransform(
        D2D1::Matrix3x2F::Translation(
            -static_cast<float>(bounds.left),
            -static_cast<float>(bounds.top)));

    g_outerBrush->SetColor(ToColorF(g_currentOuterRGB, alpha));
    g_glowBrush->SetColor(
        ToColorF(g_glowRGB, g_glowAlpha * alpha));
    g_coreBrush->SetColor(
        ToColorF(
            g_currentCoreRGB,
            std::min(1.0f, alpha * 1.02f)));

    if (g_glowEnabled) {
        DrawTrailStroke(
            g_renderTarget,
            g_glowBrush,
            outerHalf * std::max(1.0f, g_glowWidthFactor),
            true);
    }

    DrawTrailStroke(
        g_renderTarget,
        g_outerBrush,
        outerHalf,
        true);

    ID2D1Brush* coreBrush = g_coreBrush;
    if (g_gradientEnabled && EnsureGradientBrush()) {
        g_gradientBrush->SetStartPoint(
            g_renderCache.smoothed.front());
        g_gradientBrush->SetEndPoint(
            g_renderCache.smoothed.back());
        g_gradientBrush->SetOpacity(alpha);
        coreBrush = g_gradientBrush;
    }

    DrawTrailStroke(
        g_renderTarget,
        coreBrush,
        coreHalf,
        true);

    g_renderTarget->SetTransform(
        D2D1::Matrix3x2F::Identity());

    const HRESULT hr = g_renderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        ReleaseRenderResources();
        return false;
    }
    if (FAILED(hr)) return false;

    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    POINT position = {bounds.left, bounds.top};
    SIZE size = {width, height};
    POINT source = {0, 0};

    return UpdateLayeredWindow(
        hwnd,
        screen,
        &position,
        &size,
        g_backBufferDc,
        &source,
        0,
        &blend,
        ULW_ALPHA) != FALSE;
}

// -----------------------------------------------------------------------------
// Frame processing
// -----------------------------------------------------------------------------

void SmearFrame(HWND hwnd, DWORD now, bool renderFrame) {
    if (g_hotkeySuspended) return;

    POINT point = {};
    if (!GetCursorPos(&point)) return;

    const HWND foreground = GetForegroundWindow();
    const int appRule = CheckAppRuleCached(foreground);

    if (appRule < 0) {
        HistoryClear();
        g_isSmearing = false;
        g_lowVelocityFrames = 0;
        g_fadeAlpha = 0.0f;
        HideOverlay();
        return;
    }

    if (appRule == 0 && UpdateFullscreenState(now, foreground)) {
        return;
    }

    const bool wasSmearing = g_isSmearing;
    const int previousHistoryCount = g_historyCount;
    const int dx = point.x - g_lastPos.x;
    const int dy = point.y - g_lastPos.y;
    const float velocity = sqrtf(
        static_cast<float>(dx * dx + dy * dy));

    g_currentVelocity = velocity;
    g_lastPos = point;

    UpdateTrailState(point, velocity);

    if (g_isSmearing || g_historyCount >= 2) {
        UpdateTrailColorIfNeeded(now);
    }

    const bool stateChanged =
        wasSmearing != g_isSmearing
        || (previousHistoryCount < 2) != (g_historyCount >= 2);

    if (g_historyCount < 2) {
        HideOverlay();
        return;
    }

    if (!renderFrame && !stateChanged) return;

    if (!RenderTrail(hwnd)) {
        HideOverlay();
        return;
    }

    ShowOverlay();
}

// -----------------------------------------------------------------------------
// Overlay window
// -----------------------------------------------------------------------------

LRESULT CALLBACK OverlayWndProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam) {
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
        }
        return 0;

    case WM_DISPLAYCHANGE:
        ReleaseScreenDC();
        g_lastFullscreenStrongCheck = 0;
        return 0;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
}

// -----------------------------------------------------------------------------
// High-resolution frame timer
// -----------------------------------------------------------------------------

static LONGLONG QpcNow() {
    LARGE_INTEGER value = {};
    QueryPerformanceCounter(&value);
    return value.QuadPart;
}

static bool ArmFrameTimer(
    HANDLE timer,
    LONGLONG deadline,
    LONGLONG frequency) {
    const LONGLONG now = QpcNow();
    LONGLONG ticks = deadline - now;
    if (ticks < 1) ticks = 1;

    const LONGLONG due100ns = std::max<LONGLONG>(
        1,
        ticks * 10000000LL / frequency);
    LARGE_INTEGER due = {};
    due.QuadPart = -due100ns;

    return SetWaitableTimer(
        timer,
        &due,
        0,
        nullptr,
        nullptr,
        FALSE) != FALSE;
}

static LONGLONG FrameIntervalTicks(
    LONGLONG frequency,
    int frameRate) {
    return std::max<LONGLONG>(1, frequency / frameRate);
}

// -----------------------------------------------------------------------------
// Worker thread
// -----------------------------------------------------------------------------

DWORD WINAPI OverlayThreadProc(LPVOID) {
    const HRESULT coResult = CoInitializeEx(
        nullptr,
        COINIT_APARTMENTTHREADED);
    if (FAILED(coResult)) {
        Wh_Log(L"CoInitializeEx failed: 0x%08X", coResult);
        return 0;
    }

    SetThreadDpiAwarenessContext(
        DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    LARGE_INTEGER qpcFrequency = {};
    if (!QueryPerformanceFrequency(&qpcFrequency)
        || qpcFrequency.QuadPart <= 0) {
        Wh_Log(L"QueryPerformanceFrequency failed");
        CoUninitialize();
        return 0;
    }

    HRESULT hr = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        &g_d2dFactory);
    if (FAILED(hr) || !g_d2dFactory) {
        Wh_Log(L"D2D1CreateFactory failed: 0x%08X", hr);
        CoUninitialize();
        return 0;
    }

    HINSTANCE instance = GetModuleHandle(nullptr);
    const wchar_t className[] = L"SmearFrameOverlayClass";

    WNDCLASSW windowClass = {};
    windowClass.lpfnWndProc = OverlayWndProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = className;

    const ATOM atom = RegisterClassW(&windowClass);
    if (!atom && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        Wh_Log(L"RegisterClass failed: %lu", GetLastError());
        g_d2dFactory->Release();
        g_d2dFactory = nullptr;
        CoUninitialize();
        return 0;
    }

    HWND hwnd = CreateWindowExW(
        WS_EX_LAYERED
            | WS_EX_TRANSPARENT
            | WS_EX_TOPMOST
            | WS_EX_TOOLWINDOW
            | WS_EX_NOACTIVATE,
        className,
        L"SmearOverlay",
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
        Wh_Log(L"CreateWindowEx failed: %lu", GetLastError());
        UnregisterClassW(className, instance);
        g_d2dFactory->Release();
        g_d2dFactory = nullptr;
        CoUninitialize();
        return 0;
    }

    g_overlayHwnd.store(hwnd);
    HideOverlay();
    GetCursorPos(&g_lastPos);
    g_fullscreenForegroundWindow = GetForegroundWindow();
    g_lastFullscreenStrongCheck = 0;
    g_fullscreenStrongSignal = false;
    g_fullscreenSuppressed = false;

    bool hotkeyRegistered = false;
    if (g_hotkeyEnabled) {
        hotkeyRegistered = RegisterHotKey(
            hwnd,
            kHotkeyId,
            MOD_CONTROL | MOD_ALT | MOD_NOREPEAT,
            'T') != FALSE;
        if (!hotkeyRegistered) {
            Wh_Log(L"RegisterHotKey failed: %lu", GetLastError());
        }
    }

    HANDLE frameTimer = CreateWaitableTimerExW(
        nullptr,
        nullptr,
        CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
        TIMER_ALL_ACCESS);
    if (!frameTimer) {
        frameTimer = CreateWaitableTimerW(
            nullptr,
            FALSE,
            nullptr);
    }

    if (!frameTimer) {
        Wh_Log(L"CreateWaitableTimer failed: %lu", GetLastError());
        if (hotkeyRegistered) {
            UnregisterHotKey(hwnd, kHotkeyId);
        }
        DestroyWindow(hwnd);
        g_overlayHwnd.store(nullptr);
        UnregisterClassW(className, instance);
        g_d2dFactory->Release();
        g_d2dFactory = nullptr;
        CoUninitialize();
        return 0;
    }

    const LONGLONG simulationTicks = FrameIntervalTicks(
        qpcFrequency.QuadPart,
        kTargetFrameRate);
    const LONGLONG activeRenderTicks = FrameIntervalTicks(
        qpcFrequency.QuadPart,
        kActiveRenderFrameRate);
    const LONGLONG fadeRenderTicks = FrameIntervalTicks(
        qpcFrequency.QuadPart,
        kFadeRenderFrameRate);

    LONGLONG nextSimulationDeadline = QpcNow();
    LONGLONG nextRenderDeadline = nextSimulationDeadline;
    MSG message = {};
    bool running = true;

    while (running) {
        nextSimulationDeadline += simulationTicks;

        const LONGLONG nowBeforeWait = QpcNow();
        if (nextSimulationDeadline <= nowBeforeWait) {
            const LONGLONG skipped =
                (nowBeforeWait - nextSimulationDeadline)
                    / simulationTicks + 1;
            nextSimulationDeadline += skipped * simulationTicks;
        }

        if (!ArmFrameTimer(
                frameTimer,
                nextSimulationDeadline,
                qpcFrequency.QuadPart)) {
            break;
        }

        HANDLE handles[] = {frameTimer};
        const DWORD waitResult = MsgWaitForMultipleObjectsEx(
            1,
            handles,
            INFINITE,
            QS_ALLINPUT,
            MWMO_INPUTAVAILABLE);

        if (waitResult == WAIT_OBJECT_0) {
            const LONGLONG sampleNow = QpcNow();
            bool renderFrame = false;

            if (g_isSmearing || g_historyCount >= 2) {
                renderFrame = sampleNow >= nextRenderDeadline;
            }

            SmearFrame(hwnd, GetTickCount(), renderFrame);

            const bool active = g_isSmearing;
            const bool fading = !active && g_historyCount >= 2;

            if (active || fading) {
                const LONGLONG interval = active
                    ? activeRenderTicks
                    : fadeRenderTicks;

                if (renderFrame) {
                    nextRenderDeadline = sampleNow + interval;
                } else if (sampleNow >= nextRenderDeadline) {
                    nextRenderDeadline = sampleNow + interval;
                }
            } else {
                nextRenderDeadline = sampleNow + fadeRenderTicks;
            }
        } else if (waitResult == WAIT_OBJECT_0 + 1) {
            while (PeekMessageW(
                &message,
                nullptr,
                0,
                0,
                PM_REMOVE)) {
                if (message.message == WM_QUIT) {
                    running = false;
                    break;
                }

                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
        } else {
            break;
        }
    }

    CancelWaitableTimer(frameTimer);
    CloseHandle(frameTimer);

    if (hotkeyRegistered) {
        UnregisterHotKey(hwnd, kHotkeyId);
    }

    HideOverlay();
    ReleaseBackbuffer();
    ReleaseBackgroundSampler();
    ReleaseScreenDC();

    if (g_d2dFactory) {
        g_d2dFactory->Release();
        g_d2dFactory = nullptr;
    }

    DestroyWindow(hwnd);
    g_overlayHwnd.store(nullptr);
    UnregisterClassW(className, instance);
    CoUninitialize();
    return 0;
}

// -----------------------------------------------------------------------------
// Windhawk tool-mod process
// -----------------------------------------------------------------------------

BOOL WhTool_ModInit() {
    LoadSettings();

    g_threadHandle = CreateThread(
        nullptr,
        0,
        OverlayThreadProc,
        nullptr,
        0,
        nullptr);
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
        const DWORD waitResult = WaitForSingleObject(
            g_threadHandle,
            3000);
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
        PostMessageW(
            hwnd,
            kSettingsChangedMessage,
            0,
            0);
    }
}

bool g_isToolModProcessLauncher = false;
HANDLE g_toolModProcessMutex = nullptr;

void WINAPI EntryPoint_Hook() {
    ExitThread(0);
}

// -----------------------------------------------------------------------------
// Windhawk launcher
// -----------------------------------------------------------------------------

BOOL Wh_ModInit() {
    DWORD sessionId = 0;
    if (ProcessIdToSessionId(
            GetCurrentProcessId(),
            &sessionId)
        && sessionId == 0) {
        return FALSE;
    }

    bool isExcluded = false;
    bool isToolModProcess = false;
    bool isCurrentToolModProcess = false;

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(
        GetCommandLineW(),
        &argc);
    if (!argv) {
        Wh_Log(L"CommandLineToArgvW failed");
        return FALSE;
    }

    for (int i = 1; i < argc; ++i) {
        if (wcscmp(argv[i], L"-service") == 0
            || wcscmp(argv[i], L"-service-start") == 0
            || wcscmp(argv[i], L"-service-stop") == 0) {
            isExcluded = true;
            break;
        }
    }

    for (int i = 1; i + 1 < argc; ++i) {
        if (wcscmp(argv[i], L"-tool-mod") != 0) continue;

        isToolModProcess = true;
        isCurrentToolModProcess =
            wcscmp(argv[i + 1], WH_MOD_ID) == 0;
        break;
    }

    LocalFree(argv);

    if (isExcluded) return FALSE;

    if (isCurrentToolModProcess) {
        g_toolModProcessMutex = CreateMutexW(
            nullptr,
            TRUE,
            L"windhawk-tool-mod_" WH_MOD_ID);
        if (!g_toolModProcessMutex
            || GetLastError() == ERROR_ALREADY_EXISTS) {
            ExitProcess(1);
        }

        if (!WhTool_ModInit()) {
            ExitProcess(1);
        }

        IMAGE_DOS_HEADER* dosHeader =
            reinterpret_cast<IMAGE_DOS_HEADER*>(
                GetModuleHandle(nullptr));
        IMAGE_NT_HEADERS* ntHeaders =
            reinterpret_cast<IMAGE_NT_HEADERS*>(
                reinterpret_cast<BYTE*>(dosHeader)
                    + dosHeader->e_lfanew);
        void* entryPoint =
            reinterpret_cast<BYTE*>(dosHeader)
                + ntHeaders->OptionalHeader.AddressOfEntryPoint;

        Wh_SetFunctionHook(
            entryPoint,
            reinterpret_cast<void*>(EntryPoint_Hook),
            nullptr);
        return TRUE;
    }

    if (isToolModProcess) return FALSE;

    g_isToolModProcessLauncher = true;
    return TRUE;
}

void Wh_ModAfterInit() {
    if (!g_isToolModProcessLauncher) return;

    WCHAR currentProcessPath[MAX_PATH] = {};
    const DWORD pathLength = GetModuleFileNameW(
        nullptr,
        currentProcessPath,
        ARRAYSIZE(currentProcessPath));
    if (pathLength == 0 || pathLength == ARRAYSIZE(currentProcessPath)) {
        Wh_Log(L"GetModuleFileName failed");
        return;
    }

    WCHAR commandLine[MAX_PATH + 64] = {};
    swprintf_s(
        commandLine,
        L"\"%s\" -tool-mod \"%s\"",
        currentProcessPath,
        WH_MOD_ID);

    HMODULE kernelModule = GetModuleHandleW(L"kernelbase.dll");
    if (!kernelModule) {
        kernelModule = GetModuleHandleW(L"kernel32.dll");
    }
    if (!kernelModule) {
        Wh_Log(L"No kernelbase.dll/kernel32.dll");
        return;
    }

    using CreateProcessInternalW_t = BOOL(WINAPI*)(
        HANDLE,
        LPCWSTR,
        LPWSTR,
        LPSECURITY_ATTRIBUTES,
        LPSECURITY_ATTRIBUTES,
        WINBOOL,
        DWORD,
        LPVOID,
        LPCWSTR,
        LPSTARTUPINFO,
        LPPROCESS_INFORMATION,
        PHANDLE);

    auto createProcessInternal =
        reinterpret_cast<CreateProcessInternalW_t>(
            GetProcAddress(
                kernelModule,
                "CreateProcessInternalW"));

    STARTUPINFO startupInfo = {};
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_FORCEOFFFEEDBACK;

    PROCESS_INFORMATION processInfo = {};
    BOOL created = FALSE;

    if (createProcessInternal) {
        created = createProcessInternal(
            nullptr,
            currentProcessPath,
            commandLine,
            nullptr,
            nullptr,
            FALSE,
            NORMAL_PRIORITY_CLASS,
            nullptr,
            nullptr,
            &startupInfo,
            &processInfo,
            nullptr);
    }

    if (!created) {
        created = CreateProcessW(
            nullptr,
            commandLine,
            nullptr,
            nullptr,
            FALSE,
            NORMAL_PRIORITY_CLASS,
            nullptr,
            nullptr,
            &startupInfo,
            &processInfo);
    }

    if (!created) {
        Wh_Log(L"CreateProcess failed: %lu", GetLastError());
        return;
    }

    CloseHandle(processInfo.hProcess);
    CloseHandle(processInfo.hThread);
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
