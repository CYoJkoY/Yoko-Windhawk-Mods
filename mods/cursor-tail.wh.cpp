// ==WindhawkMod==
// @id              cursor-tail
// @name            Cursor Tail
// @description     Adaptive motion-blur trail for the mouse pointer, colored by sampling the cursor image.
// @version         3.7
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
  screen background.
* **Independent Outline Color:** Auto-derive the outer ring color from the
  core color, or pick it manually.
* **Dynamic 2D Ribbon:** Procedurally generates a continuous ribbon with a
  rounded head.
* **Direct2D Rendering:** Uses Direct2D anti-aliased strokes without rebuilding
  COM geometry objects every frame.
* **Speed-Reactive Shape:** Trail width, alpha and length scale with speed.
* **Gradient / Glow:** Optional tail gradient and outer glow.
* **Smooth Fade-Out:** Trail fades when the pointer stops.
* **Per-App Rules:** Force the trail on or off for specific executables.
* **Hotkey Toggle:** Ctrl+Alt+T temporarily suspends/restores the trail.
* **Stable Fullscreen Suppression:** Fullscreen state is detected quickly for
  borderless monitor-covering windows and confirmed with stronger fullscreen
  signals without repeatedly showing and hiding the overlay.

### Game / Fullscreen Detection
The trail is suppressed automatically when fullscreen software is detected.
Borderless fullscreen is recognized for non-maximized, captionless windows that
cover the whole monitor. Exclusive/D3D fullscreen is additionally recognized
through Windows' D3D fullscreen state. Suppression enters immediately and leaves
only after several consecutive non-fullscreen samples to avoid flicker.
*/
// ==WindhawkModReadme==

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
  $description: Trail fades smoothly when the pointer stops instead of snapping off.
- fade_decay: 90
  $name: Fade Decay (x0.01)
  $description: Per-frame alpha multiplier during fade-out, x0.01. 90 = 0.90.

- trail_color_mode: 0
  $name: Trail Color Mode
  $description: 0 = Manual hex color. 1 = Auto sample the cursor image and pick a visible color.
- trail_color_manual: "#FFFFFF"
  $name: Manual Trail Color
  $description: Core trail color in Manual mode. Format "#RRGGBB".
- outline_color_mode: 0
  $name: Outline Color Mode
  $description: 0 = Auto derive from the core luminance. 1 = Manual hex color.
- outline_color_manual: "#000000"
  $name: Manual Outline Color
  $description: Outer ring color when Outline Color Mode is Manual. Format "#RRGGBB".
- auto_resample_interval: 0
  $name: Auto Resample Interval
  $description: How often to re-sample the cursor color in Auto mode (ms). 0 = only when the cursor image changes.

- app_rules: ""
  $name: Per-App Rules
  $description: One rule per line, "exe=on" or "exe=off". "#" starts a comment.
- hotkey_enabled: 0
  $name: Enable Hotkey
  $description: Register Ctrl+Alt+T to temporarily suspend/resume the trail.
*/
// ==WindhawkModSettings==

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

constexpr UINT kSettingsChangedMessage = WM_APP + 1;
constexpr int kHotkeyId = 0xCAFE;
constexpr int kTargetFrameRate = 125;
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
float g_smoothedSpeedNorm = 0.0f;
float g_frozenSpeedNorm = 0.5f;
bool g_hotkeySuspended = false;
bool g_windowVisible = false;

ID2D1Factory* g_pD2DFactory = nullptr;
ID2D1DCRenderTarget* g_pDCRenderTarget = nullptr;
ID2D1SolidColorBrush* g_pOuterBrush = nullptr;
ID2D1SolidColorBrush* g_pCoreBrush = nullptr;
ID2D1SolidColorBrush* g_pGlowBrush = nullptr;
ID2D1LinearGradientBrush* g_pGradientBrush = nullptr;
ID2D1GradientStopCollection* g_pGradientStops = nullptr;
ID2D1StrokeStyle* g_pStrokeStyle = nullptr;
uint32_t g_gradientHeadCache = 0xFFFFFFFFu;
uint32_t g_gradientTailCache = 0xFFFFFFFFu;
bool g_dcBound = false;

HDC g_hdcMem = nullptr;
HBITMAP g_hBitmap = nullptr;
HGDIOBJ g_originalBitmap = nullptr;
int g_cachedWidth = 0;
int g_cachedHeight = 0;
HDC g_hdcScreen = nullptr;

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

struct AppRule { std::wstring exe; bool enabled; };
std::vector<AppRule> g_appRules;
HWND g_cachedForegroundWindow = nullptr;
int g_cachedAppRule = 0;

bool g_fullscreenSuppressed = false;
bool g_fullscreenCandidate = false;
int g_fullscreenFalseSamples = 0;
HWND g_fullscreenForegroundWindow = nullptr;
DWORD g_lastFullscreenStrongCheck = 0;
bool g_fullscreenStrongSignal = false;

struct TrailRenderCache {
    std::vector<D2D1_POINT_2F> smoothed;
    std::vector<D2D1_POINT_2F> subdivision;
    void ReserveForTailLength(int tailLength) {
        const size_t base = static_cast<size_t>(std::max(tailLength, 2));
        const size_t count = base * 16 - 15;
        smoothed.reserve(count);
        subdivision.reserve(count);
    }
};
TrailRenderCache g_renderCache;

static inline float RgbLuminance(uint8_t r, uint8_t g, uint8_t b) {
    auto linear = [](float c) { c /= 255.0f; return c <= 0.03928f ? c / 12.92f : powf((c + 0.055f) / 1.055f, 2.4f); };
    return 0.2126f * linear(r) + 0.7152f * linear(g) + 0.0722f * linear(b);
}
static inline float RgbLuminance(uint32_t rgb) { return RgbLuminance(static_cast<uint8_t>((rgb >> 16) & 0xFF), static_cast<uint8_t>((rgb >> 8) & 0xFF), static_cast<uint8_t>(rgb & 0xFF)); }
static inline float ContrastRatio(float a, float b) { const float lo = std::min(a, b), hi = std::max(a, b); return (hi + 0.05f) / (lo + 0.05f); }
static inline D2D1_COLOR_F ToColorF(uint32_t rgb, float alpha) { return D2D1::ColorF(((rgb >> 16) & 0xFF) / 255.0f, ((rgb >> 8) & 0xFF) / 255.0f, (rgb & 0xFF) / 255.0f, alpha); }
static bool ParseHexColor(const wchar_t* str, uint32_t& out) {
    if (!str) return false;
    while (*str == L' ' || *str == L'\t') ++str;
    if (*str == L'#') ++str; else if (str[0] == L'0' && (str[1] == L'x' || str[1] == L'X')) str += 2;
    uint32_t value = 0; int digits = 0;
    while (digits < 6) { const wchar_t c = str[digits]; int d = -1; if (c >= L'0' && c <= L'9') d = c - L'0'; else if (c >= L'a' && c <= L'f') d = c - L'a' + 10; else if (c >= L'A' && c <= L'F') d = c - L'A' + 10; if (d < 0) break; value = (value << 4) | static_cast<uint32_t>(d); ++digits; }
    if (digits != 6) return false; out = value; return true;
}
static std::wstring ToLowerW(std::wstring value) { for (wchar_t& c : value) c = static_cast<wchar_t>(towlower(c)); return value; }

void ReleaseRenderResources() {
    if (g_pGradientBrush) { g_pGradientBrush->Release(); g_pGradientBrush = nullptr; }
    if (g_pGradientStops) { g_pGradientStops->Release(); g_pGradientStops = nullptr; }
    if (g_pStrokeStyle) { g_pStrokeStyle->Release(); g_pStrokeStyle = nullptr; }
    if (g_pGlowBrush) { g_pGlowBrush->Release(); g_pGlowBrush = nullptr; }
    if (g_pCoreBrush) { g_pCoreBrush->Release(); g_pCoreBrush = nullptr; }
    if (g_pOuterBrush) { g_pOuterBrush->Release(); g_pOuterBrush = nullptr; }
    if (g_pDCRenderTarget) { g_pDCRenderTarget->Release(); g_pDCRenderTarget = nullptr; }
    g_gradientHeadCache = 0xFFFFFFFFu; g_gradientTailCache = 0xFFFFFFFFu; g_dcBound = false;
}
void ReleaseBackbuffer() {
    ReleaseRenderResources();
    if (g_hdcMem) { if (g_originalBitmap) { SelectObject(g_hdcMem, g_originalBitmap); g_originalBitmap = nullptr; } DeleteDC(g_hdcMem); g_hdcMem = nullptr; }
    if (g_hBitmap) { DeleteObject(g_hBitmap); g_hBitmap = nullptr; }
    g_cachedWidth = 0; g_cachedHeight = 0;
}
HBITMAP Create32BitDIB(int width, int height) {
    if (width <= 0 || height <= 0) return nullptr;
    BITMAPINFO bmi = {}; bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); bmi.bmiHeader.biWidth = width; bmi.bmiHeader.biHeight = -height; bmi.bmiHeader.biPlanes = 1; bmi.bmiHeader.biBitCount = 32; bmi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr; return CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
}
HDC GetScreenDC() { if (!g_hdcScreen) g_hdcScreen = GetDC(nullptr); return g_hdcScreen; }
void ReleaseScreenDC() { if (g_hdcScreen) { ReleaseDC(nullptr, g_hdcScreen); g_hdcScreen = nullptr; } }
void HideOverlay() { HWND hwnd = g_overlayHwnd.load(); if (!hwnd || !IsWindow(hwnd) || !g_windowVisible) return; ShowWindow(hwnd, SW_HIDE); g_windowVisible = false; }
void ShowOverlay() { HWND hwnd = g_overlayHwnd.load(); if (!hwnd || !IsWindow(hwnd) || g_windowVisible) return; ShowWindow(hwnd, SW_SHOWNOACTIVATE); g_windowVisible = true; }
void HistoryClear() { g_historyHead = 0; g_historyCount = 0; }
void HistoryPushFront(const POINT& p, int maxLen) { if (g_historyCount == kHistoryCapacity) --g_historyCount; g_historyHead = (g_historyHead - 1 + kHistoryCapacity) % kHistoryCapacity; g_history[g_historyHead] = p; ++g_historyCount; if (g_historyCount > maxLen) g_historyCount = maxLen; }
void HistoryPopBack() { if (g_historyCount > 0) --g_historyCount; }
const POINT& HistoryAt(int index) { return g_history[(g_historyHead + index) % kHistoryCapacity]; }

static bool GetProcessExeName(DWORD pid, std::wstring& out) {
    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid); if (!hProc) return false;
    WCHAR path[512] = {}; DWORD size = ARRAYSIZE(path); const BOOL ok = QueryFullProcessImageNameW(hProc, 0, path, &size); CloseHandle(hProc); if (!ok) return false;
    const wchar_t* name = wcsrchr(path, L'\\'); out = ToLowerW(name ? name + 1 : path); return true;
}
static void ParseAppRules(const wchar_t* text) {
    g_appRules.clear(); if (!text) return; const std::wstring input(text); size_t start = 0;
    while (start <= input.size()) {
        size_t end = input.find_first_of(L"\r\n", start); if (end == std::wstring::npos) end = input.size(); std::wstring line = input.substr(start, end - start);
        const size_t hash = line.find(L'#'); if (hash != std::wstring::npos) line.resize(hash); const size_t eq = line.find(L'=');
        if (eq != std::wstring::npos) {
            std::wstring exe = line.substr(0, eq), value = line.substr(eq + 1);
            auto trim = [](std::wstring& s) { const size_t a = s.find_first_not_of(L" \t"); if (a == std::wstring::npos) { s.clear(); return; } const size_t b = s.find_last_not_of(L" \t"); s = s.substr(a, b - a + 1); };
            trim(exe); trim(value); if (!exe.empty()) g_appRules.push_back({ToLowerW(exe), value == L"on" || value == L"1" || value == L"true" || value == L"yes"});
        }
        if (end == input.size()) break; start = input.find_first_not_of(L"\r\n", end); if (start == std::wstring::npos) break;
    }
}
static int CheckAppRuleCached(HWND foreground) {
    if (foreground == g_cachedForegroundWindow) return g_cachedAppRule;
    g_cachedForegroundWindow = foreground; g_cachedAppRule = 0; if (!foreground || g_appRules.empty()) return 0;
    DWORD pid = 0; GetWindowThreadProcessId(foreground, &pid); if (!pid) return 0; std::wstring exe; if (!GetProcessExeName(pid, exe)) return 0;
    for (const auto& rule : g_appRules) if (rule.exe == exe) { g_cachedAppRule = rule.enabled ? 1 : -1; break; }
    return g_cachedAppRule;
}

void LoadSettings() {
    g_triggerVelocity = static_cast<float>(std::clamp(Wh_GetIntSetting(L"trigger_velocity"), 1, 500)); g_stopVelocity = static_cast<float>(std::clamp(Wh_GetIntSetting(L"stop_velocity"), 1, 500)); if (g_stopVelocity >= g_triggerVelocity) g_stopVelocity = std::max(1.0f, g_triggerVelocity * 0.5f);
    g_tailOffsetX = std::clamp(Wh_GetIntSetting(L"tail_offset_x"), -64, 64); g_tailOffsetY = std::clamp(Wh_GetIntSetting(L"tail_offset_y"), -64, 64); g_tailLength = std::clamp(Wh_GetIntSetting(L"tail_length"), 2, kMaxTailLength);
    g_speedScaling = std::clamp(Wh_GetIntSetting(L"speed_scaling"), 0, 1); g_widthMin = static_cast<float>(std::clamp(Wh_GetIntSetting(L"width_min"), 1, 40)); g_widthMax = static_cast<float>(std::clamp(Wh_GetIntSetting(L"width_max"), 1, 60));
    g_coreWidthMin = static_cast<float>(std::clamp(Wh_GetIntSetting(L"core_width_min"), 1, 40)); g_coreWidthMax = static_cast<float>(std::clamp(Wh_GetIntSetting(L"core_width_max"), 1, 60)); g_alphaMin = std::clamp(Wh_GetIntSetting(L"alpha_min"), 0, 100) / 100.0f; g_alphaMax = std::clamp(Wh_GetIntSetting(L"alpha_max"), 0, 100) / 100.0f;
    g_taperPower = std::clamp(Wh_GetIntSetting(L"taper_power"), 5, 30) / 10.0f; if (g_widthMax < g_widthMin) std::swap(g_widthMin, g_widthMax); if (g_coreWidthMax < g_coreWidthMin) std::swap(g_coreWidthMin, g_coreWidthMax); g_smoothIterations = std::clamp(Wh_GetIntSetting(L"smooth_iterations"), 0, 4);
    g_gradientEnabled = std::clamp(Wh_GetIntSetting(L"gradient_enabled"), 0, 1);
    { PCWSTR value = Wh_GetStringSetting(L"gradient_tail_color"); uint32_t parsed = 0; g_gradientTailRGB = value && ParseHexColor(value, parsed) ? parsed : 0x00FF00FF; if (value) Wh_FreeStringSetting(value); }
    g_glowEnabled = std::clamp(Wh_GetIntSetting(L"glow_enabled"), 0, 1);
    { PCWSTR value = Wh_GetStringSetting(L"glow_color"); uint32_t parsed = 0; g_glowRGB = value && ParseHexColor(value, parsed) ? parsed : 0x00FFFFFF; if (value) Wh_FreeStringSetting(value); }
    g_glowWidthFactor = std::clamp(Wh_GetIntSetting(L"glow_width_factor"), 10, 30) / 10.0f; g_glowAlpha = std::clamp(Wh_GetIntSetting(L"glow_alpha"), 0, 100) / 100.0f; g_fadeEnabled = std::clamp(Wh_GetIntSetting(L"fade_enabled"), 0, 1); g_fadeDecay = std::clamp(Wh_GetIntSetting(L"fade_decay"), 50, 99) / 100.0f;
    g_trailColorMode = std::clamp(Wh_GetIntSetting(L"trail_color_mode"), 0, 1);
    { PCWSTR value = Wh_GetStringSetting(L"trail_color_manual"); uint32_t parsed = 0; g_manualColorRGB = value && ParseHexColor(value, parsed) ? parsed : kFallbackCoreColor; if (value) Wh_FreeStringSetting(value); }
    g_outlineColorMode = std::clamp(Wh_GetIntSetting(L"outline_color_mode"), 0, 1);
    { PCWSTR value = Wh_GetStringSetting(L"outline_color_manual"); uint32_t parsed = 0; g_manualOutlineRGB = value && ParseHexColor(value, parsed) ? parsed : kFallbackOuterColor; if (value) Wh_FreeStringSetting(value); }
    g_autoResampleInterval = std::clamp(Wh_GetIntSetting(L"auto_resample_interval"), 0, kAutoResampleIntervalMaxMs);
    { PCWSTR value = Wh_GetStringSetting(L"app_rules"); ParseAppRules(value); if (value) Wh_FreeStringSetting(value); }
    g_cachedForegroundWindow = nullptr; g_cachedAppRule = 0; g_hotkeyEnabled = std::clamp(Wh_GetIntSetting(L"hotkey_enabled"), 0, 1); g_renderCache.ReserveForTailLength(g_tailLength);
}

static bool CoversMonitor(HWND hwnd) {
    const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    if ((style & WS_MAXIMIZE) != 0 || (style & WS_CAPTION) != 0) return false;
    RECT app = {}; if (!GetWindowRect(hwnd, &app)) return false;
    const HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST); MONITORINFO info = {sizeof(info)};
    if (!GetMonitorInfoW(monitor, &info)) return false;
    return app.left <= info.rcMonitor.left + kFullscreenTolerancePx && app.top <= info.rcMonitor.top + kFullscreenTolerancePx && app.right >= info.rcMonitor.right - kFullscreenTolerancePx && app.bottom >= info.rcMonitor.bottom - kFullscreenTolerancePx;
}
static bool CheckStrongFullscreenSignal(HWND hwnd) {
    if (!hwnd) return false;
    QUERY_USER_NOTIFICATION_STATE state = QUNS_NOT_PRESENT;
    return SUCCEEDED(SHQueryUserNotificationState(&state)) && state == QUNS_RUNNING_D3D_FULL_SCREEN;
}
static bool IsFullscreenCandidate(DWORD now, HWND hwnd) {
    if (!hwnd || hwnd == GetDesktopWindow() || hwnd == GetShellWindow() || IsIconic(hwnd) || !IsWindowVisible(hwnd)) return false;
    if (hwnd != g_fullscreenForegroundWindow) {
        g_fullscreenForegroundWindow = hwnd; g_lastFullscreenStrongCheck = 0; g_fullscreenStrongSignal = false; g_fullscreenCandidate = false; g_fullscreenFalseSamples = 0;
    }
    if (now - g_lastFullscreenStrongCheck >= kFullscreenStrongCheckIntervalMs) {
        g_lastFullscreenStrongCheck = now; g_fullscreenStrongSignal = CheckStrongFullscreenSignal(hwnd);
    }
    return g_fullscreenStrongSignal || CoversMonitor(hwnd);
}
static bool UpdateFullscreenState(DWORD now, HWND foreground) {
    const bool candidate = IsFullscreenCandidate(now, foreground);
    if (candidate) {
        g_fullscreenCandidate = true; g_fullscreenFalseSamples = 0;
        if (!g_fullscreenSuppressed) {
            g_fullscreenSuppressed = true; HistoryClear(); g_isSmearing = false; g_lowVelocityFrames = 0; g_fadeAlpha = 0.0f; HideOverlay();
        }
        return true;
    }
    g_fullscreenCandidate = false;
    if (g_fullscreenSuppressed) {
        if (++g_fullscreenFalseSamples >= kFullscreenExitConfirmSamples) {
            g_fullscreenSuppressed = false; g_fullscreenFalseSamples = 0; HistoryClear(); g_isSmearing = false; g_lowVelocityFrames = 0; g_fadeAlpha = 0.0f; HideOverlay();
        }
        return g_fullscreenSuppressed;
    }
    g_fullscreenFalseSamples = 0; return false;
}

bool EnsureBackbuffer(int width, int height, HDC referenceDC) {
    if (width <= 0 || height <= 0 || !referenceDC) return false;
    if (g_hdcMem && g_hBitmap && width <= g_cachedWidth && height <= g_cachedHeight) return true;
    if (!g_hdcMem) { g_hdcMem = CreateCompatibleDC(referenceDC); if (!g_hdcMem) return false; }
    const int newWidth = std::max(width, g_cachedWidth), newHeight = std::max(height, g_cachedHeight);
    ReleaseRenderResources(); HBITMAP bitmap = Create32BitDIB(newWidth, newHeight); if (!bitmap) return false;
    const HGDIOBJ oldBitmap = SelectObject(g_hdcMem, bitmap);
    if (!g_originalBitmap) g_originalBitmap = oldBitmap; else if (oldBitmap && oldBitmap != g_originalBitmap) DeleteObject(oldBitmap);
    g_hBitmap = bitmap; g_cachedWidth = newWidth; g_cachedHeight = newHeight; return true;
}

bool EnsureD2DResources() {
    if (!g_pD2DFactory) return false; if (g_pDCRenderTarget) return true;
    const auto properties = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    HRESULT hr = g_pD2DFactory->CreateDCRenderTarget(&properties, &g_pDCRenderTarget); if (FAILED(hr) || !g_pDCRenderTarget) return false;
    if (FAILED(g_pDCRenderTarget->CreateSolidColorBrush(ToColorF(g_currentOuterRGB, kTrailAlpha), &g_pOuterBrush)) || FAILED(g_pDCRenderTarget->CreateSolidColorBrush(ToColorF(g_currentCoreRGB, kTrailAlpha), &g_pCoreBrush)) || FAILED(g_pDCRenderTarget->CreateSolidColorBrush(ToColorF(g_glowRGB, g_glowAlpha), &g_pGlowBrush))) { ReleaseRenderResources(); return false; }
    D2D1_STROKE_STYLE_PROPERTIES stroke = {};
    stroke.startCap = D2D1_CAP_STYLE_ROUND; stroke.endCap = D2D1_CAP_STYLE_ROUND; stroke.lineJoin = D2D1_LINE_JOIN_ROUND;
    if (FAILED(g_pD2DFactory->CreateStrokeStyle(stroke, nullptr, 0, &g_pStrokeStyle))) { ReleaseRenderResources(); return false; }
    return true;
}

bool EnsureGradientBrush() {
    if (!g_gradientEnabled || !g_pDCRenderTarget) return false;
    if (g_pGradientBrush && g_pGradientStops && g_gradientHeadCache == g_currentCoreRGB && g_gradientTailCache == g_gradientTailRGB) return true;
    if (g_pGradientBrush) { g_pGradientBrush->Release(); g_pGradientBrush = nullptr; }
    if (g_pGradientStops) { g_pGradientStops->Release(); g_pGradientStops = nullptr; }
    D2D1_GRADIENT_STOP stops[2] = {};
    stops[0].position = 0.0f; stops[0].color = ToColorF(g_currentCoreRGB, 1.0f); stops[1].position = 1.0f; stops[1].color = ToColorF(g_gradientTailRGB, 0.15f);
    if (FAILED(g_pDCRenderTarget->CreateGradientStopCollection(stops, 2, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, &g_pGradientStops))) return false;
    const D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES props = {};
    if (FAILED(g_pDCRenderTarget->CreateLinearGradientBrush(props, g_pGradientStops, &g_pGradientBrush))) { g_pGradientStops->Release(); g_pGradientStops = nullptr; return false; }
    g_gradientHeadCache = g_currentCoreRGB; g_gradientTailCache = g_gradientTailRGB; return true;
}

void SmoothTrail(int iterations) {
    auto& current = g_renderCache.smoothed; auto& next = g_renderCache.subdivision; current.clear(); next.clear();
    for (int i = 0; i < g_historyCount; ++i) { const POINT& p = HistoryAt(i); current.push_back(D2D1::Point2F(static_cast<float>(p.x + g_tailOffsetX), static_cast<float>(p.y + g_tailOffsetY))); }
    for (int iteration = 0; iteration < iterations && current.size() >= 3; ++iteration) {
        next.clear(); next.reserve(current.size() * 2); next.push_back(current.front());
        for (size_t i = 0; i + 1 < current.size(); ++i) { const auto& a = current[i]; const auto& b = current[i + 1]; next.push_back(D2D1::Point2F(0.75f * a.x + 0.25f * b.x, 0.75f * a.y + 0.25f * b.y)); next.push_back(D2D1::Point2F(0.25f * a.x + 0.75f * b.x, 0.25f * a.y + 0.75f * b.y)); }
        next.push_back(current.back()); current.swap(next);
    }
}
RECT CalculateTrailBounds(float maxHalfWidth) {
    if (g_renderCache.smoothed.empty()) return {0, 0, 0, 0};
    float minX = g_renderCache.smoothed.front().x, minY = g_renderCache.smoothed.front().y; float maxX = minX, maxY = minY;
    for (const auto& p : g_renderCache.smoothed) { minX = std::min(minX, p.x); minY = std::min(minY, p.y); maxX = std::max(maxX, p.x); maxY = std::max(maxY, p.y); }
    const float pad = maxHalfWidth + kRenderPadding;
    RECT result = {static_cast<LONG>(floorf(minX - pad)), static_cast<LONG>(floorf(minY - pad)), static_cast<LONG>(ceilf(maxX + pad)), static_cast<LONG>(ceilf(maxY + pad))};
    if (result.right <= result.left) result.right = result.left + 1; if (result.bottom <= result.top) result.bottom = result.top + 1; return result;
}
static inline float TrailTaper(size_t index, size_t count) {
    if (count <= 1 || index + 1 >= count) return 0.0f;
    const float ratio = static_cast<float>(index) / static_cast<float>(count - 1);
    return powf(std::max(0.0f, 1.0f - ratio), g_taperPower);
}
static void DrawTrailStroke(ID2D1RenderTarget* target, ID2D1Brush* brush, float halfWidth, bool drawHead) {
    const auto& points = g_renderCache.smoothed; if (points.size() < 2 || !brush) return;
    for (size_t i = 0; i + 1 < points.size(); ++i) {
        const float width = std::max(0.1f, halfWidth * TrailTaper(i, points.size()) * 2.0f);
        target->DrawLine(points[i], points[i + 1], brush, width, g_pStrokeStyle);
    }
    if (drawHead) target->FillEllipse(D2D1::Ellipse(points.front(), halfWidth, halfWidth), brush);
}

struct ColorCandidate { uint32_t rgb; int count; };
static bool ExtractCursorColorCandidates(std::vector<ColorCandidate>& out) {
    out.clear(); CURSORINFO ci = {sizeof(ci)}; if (!GetCursorInfo(&ci) || !(ci.flags & CURSOR_SHOWING) || !ci.hCursor) return false;
    ICONINFO ii = {}; if (!GetIconInfo(ci.hCursor, &ii)) return false;
    auto cleanup = [&] { if (ii.hbmColor) DeleteObject(ii.hbmColor); if (ii.hbmMask) DeleteObject(ii.hbmMask); };
    if (ii.hbmColor) {
        BITMAP bm = {}; if (GetObject(ii.hbmColor, sizeof(bm), &bm) && bm.bmWidth > 0 && bm.bmHeight > 0 && bm.bmWidth <= 256 && bm.bmHeight <= 256) {
            const int w = bm.bmWidth, h = bm.bmHeight; BITMAPINFO bmi = {};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); bmi.bmiHeader.biWidth = w; bmi.bmiHeader.biHeight = -h; bmi.bmiHeader.biPlanes = 1; bmi.bmiHeader.biBitCount = 32; bmi.bmiHeader.biCompression = BI_RGB;
            std::vector<uint32_t> pixels(static_cast<size_t>(w) * h); HDC screen = GetScreenDC();
            if (screen && GetDIBits(screen, ii.hbmColor, 0, h, pixels.data(), &bmi, DIB_RGB_COLORS) == h) {
                std::unordered_map<uint32_t, int> histogram; histogram.reserve(64);
                for (uint32_t pixel : pixels) { if (((pixel >> 24) & 0xFF) < kCursorAlphaThreshold) continue; ++histogram[pixel & 0x00FFFFFF]; }
                out.reserve(histogram.size()); for (const auto& entry : histogram) out.push_back({entry.first, entry.second});
                std::sort(out.begin(), out.end(), [](const ColorCandidate& a, const ColorCandidate& b) { return a.count > b.count; });
                cleanup(); return !out.empty();
            }
        }
    }
    cleanup(); out.push_back({kFallbackCoreColor, 1}); return true;
}

static HDC g_bgSamplerDC = nullptr; static HBITMAP g_bgSamplerBitmap = nullptr; static HGDIOBJ g_bgSamplerOriginal = nullptr; static uint32_t* g_bgSamplerPixels = nullptr; static int g_bgSamplerSize = 0;
static void ReleaseBackgroundSampler() {
    if (g_bgSamplerDC) { if (g_bgSamplerOriginal) SelectObject(g_bgSamplerDC, g_bgSamplerOriginal); DeleteDC(g_bgSamplerDC); g_bgSamplerDC = nullptr; }
    if (g_bgSamplerBitmap) { DeleteObject(g_bgSamplerBitmap); g_bgSamplerBitmap = nullptr; }
    g_bgSamplerOriginal = nullptr; g_bgSamplerPixels = nullptr; g_bgSamplerSize = 0;
}
static float SampleBackgroundLuminance(POINT center) {
    const int size = kBackgroundSampleRadius * 2 + 1; HDC screen = GetScreenDC(); if (!screen) return 0.5f;
    if (g_bgSamplerSize != size || !g_bgSamplerDC || !g_bgSamplerBitmap || !g_bgSamplerPixels) {
        ReleaseBackgroundSampler(); BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); bmi.bmiHeader.biWidth = size; bmi.bmiHeader.biHeight = -size; bmi.bmiHeader.biPlanes = 1; bmi.bmiHeader.biBitCount = 32; bmi.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr; g_bgSamplerBitmap = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!g_bgSamplerBitmap || !bits) { ReleaseBackgroundSampler(); return 0.5f; }
        g_bgSamplerPixels = static_cast<uint32_t*>(bits); g_bgSamplerDC = CreateCompatibleDC(screen);
        if (!g_bgSamplerDC) { ReleaseBackgroundSampler(); return 0.5f; }
        g_bgSamplerOriginal = SelectObject(g_bgSamplerDC, g_bgSamplerBitmap); g_bgSamplerSize = size;
    }
    if (!BitBlt(g_bgSamplerDC, 0, 0, size, size, screen, center.x - kBackgroundSampleRadius, center.y - kBackgroundSampleRadius, SRCCOPY)) return 0.5f;
    double sum = 0.0; int count = 0;
    for (int iy = 0; iy < kBackgroundSampleGrid; ++iy) { const int y = (size - 1) * iy / (kBackgroundSampleGrid - 1); for (int ix = 0; ix < kBackgroundSampleGrid; ++ix) { const int x = (size - 1) * ix / (kBackgroundSampleGrid - 1); const uint32_t pixel = g_bgSamplerPixels[y * size + x]; sum += RgbLuminance(static_cast<uint8_t>((pixel >> 16) & 0xFF), static_cast<uint8_t>((pixel >> 8) & 0xFF), static_cast<uint8_t>(pixel & 0xFF)); ++count; } }
    return count ? static_cast<float>(sum / count) : 0.5f;
}
static uint32_t ResolveOutlineColor(uint32_t core) { if (g_outlineColorMode == 1) return g_manualOutlineRGB; return RgbLuminance(core) > 0.5f ? kFallbackOuterColor : kFallbackCoreColor; }
static void UpdateTrailColorIfNeeded(DWORD now) {
    static DWORD lastUpdate = 0; static HCURSOR lastCursor = nullptr; static bool initialized = false;
    if (g_trailColorMode == 0) {
        static uint32_t lastCore = 0xFFFFFFFFu, lastOutline = 0xFFFFFFFFu; static int lastOutlineMode = -1;
        if (lastCore != g_manualColorRGB || lastOutline != g_manualOutlineRGB || lastOutlineMode != g_outlineColorMode) { g_currentCoreRGB = g_manualColorRGB; g_currentOuterRGB = ResolveOutlineColor(g_currentCoreRGB); lastCore = g_manualColorRGB; lastOutline = g_manualOutlineRGB; lastOutlineMode = g_outlineColorMode; }
        initialized = true; return;
    }
    bool resample = !initialized; CURSORINFO ci = {sizeof(ci)};
    if (!resample && GetCursorInfo(&ci)) { if (g_autoResampleInterval > 0) resample = now - lastUpdate >= static_cast<DWORD>(g_autoResampleInterval); else if (ci.hCursor != lastCursor && now - lastUpdate >= kMinCursorChangeResampleIntervalMs) resample = true; }
    if (!resample) return;
    if (GetCursorInfo(&ci)) lastCursor = ci.hCursor; lastUpdate = now; initialized = true;
    POINT point = {}; if (!GetCursorPos(&point)) return; std::vector<ColorCandidate> candidates;
    if (!ExtractCursorColorCandidates(candidates) || candidates.empty()) return;
    const float background = SampleBackgroundLuminance(point);
    for (const auto& candidate : candidates) if (ContrastRatio(RgbLuminance(candidate.rgb), background) >= kMinColorContrast) { g_currentCoreRGB = candidate.rgb; g_currentOuterRGB = ResolveOutlineColor(candidate.rgb); return; }
    g_currentCoreRGB = kFallbackCoreColor; g_currentOuterRGB = ResolveOutlineColor(g_currentCoreRGB);
}

bool RenderTrail(HWND hwnd) {
    if (g_historyCount < 2) return false; SmoothTrail(g_smoothIterations); if (g_renderCache.smoothed.size() < 2) return false;
    const float speed = g_speedScaling ? (g_isSmearing ? g_smoothedSpeedNorm : g_frozenSpeedNorm) : 1.0f;
    const float outerHalf = g_widthMin + (g_widthMax - g_widthMin) * speed; const float coreHalf = g_coreWidthMin + (g_coreWidthMax - g_coreWidthMin) * speed;
    const float alphaScale = g_alphaMin + (g_alphaMax - g_alphaMin) * speed; const float alpha = kTrailAlpha * alphaScale * g_fadeAlpha; if (alpha < 0.02f) return false;
    const float boundsHalf = g_glowEnabled ? outerHalf * std::max(1.0f, g_glowWidthFactor) : outerHalf;
    const RECT bounds = CalculateTrailBounds(boundsHalf); const int width = bounds.right - bounds.left, height = bounds.bottom - bounds.top; HDC screen = GetScreenDC();
    if (!screen || !EnsureBackbuffer(width, height, screen) || !EnsureD2DResources()) return false;
    if (!g_dcBound) { RECT bind = {0, 0, g_cachedWidth, g_cachedHeight}; if (FAILED(g_pDCRenderTarget->BindDC(g_hdcMem, &bind))) return false; g_dcBound = true; }
    g_pDCRenderTarget->BeginDraw(); g_pDCRenderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));
    g_pDCRenderTarget->SetTransform(D2D1::Matrix3x2F::Translation(-static_cast<float>(bounds.left), -static_cast<float>(bounds.top)));
    g_pOuterBrush->SetColor(ToColorF(g_currentOuterRGB, alpha)); g_pGlowBrush->SetColor(ToColorF(g_glowRGB, g_glowAlpha * alpha)); g_pCoreBrush->SetColor(ToColorF(g_currentCoreRGB, std::min(1.0f, alpha * 1.02f)));
    if (g_glowEnabled) DrawTrailStroke(g_pDCRenderTarget, g_pGlowBrush, outerHalf * std::max(1.0f, g_glowWidthFactor), true);
    DrawTrailStroke(g_pDCRenderTarget, g_pOuterBrush, outerHalf, true);
    ID2D1Brush* coreBrush = g_pCoreBrush;
    if (g_gradientEnabled && EnsureGradientBrush()) { g_pGradientBrush->SetStartPoint(g_renderCache.smoothed.front()); g_pGradientBrush->SetEndPoint(g_renderCache.smoothed.back()); g_pGradientBrush->SetOpacity(alpha); coreBrush = g_pGradientBrush; }
    DrawTrailStroke(g_pDCRenderTarget, coreBrush, coreHalf, true);
    g_pDCRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
    const HRESULT hr = g_pDCRenderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) { ReleaseRenderResources(); return false; }
    if (FAILED(hr)) return false;
    BLENDFUNCTION blend = {}; blend.BlendOp = AC_SRC_OVER; blend.SourceConstantAlpha = 255; blend.AlphaFormat = AC_SRC_ALPHA;
    const POINT pos = {bounds.left, bounds.top}; const SIZE size = {width, height}; const POINT source = {0, 0};
    return UpdateLayeredWindow(hwnd, screen, &pos, &size, g_hdcMem, &source, 0, &blend, ULW_ALPHA) != FALSE;
}

void UpdateTrailState(const POINT& point, float velocity) {
    if (velocity > g_triggerVelocity && !g_isSmearing) { g_isSmearing = true; g_lowVelocityFrames = 0; g_fadeAlpha = 1.0f; g_smoothedSpeedNorm = 0.0f; }
    else if (velocity < g_stopVelocity && g_isSmearing) { if (++g_lowVelocityFrames > 2) { g_isSmearing = false; g_frozenSpeedNorm = g_smoothedSpeedNorm; } }
    else if (g_isSmearing) g_lowVelocityFrames = 0;
    if (g_isSmearing) {
        const float target = std::clamp((velocity - g_stopVelocity) / (g_triggerVelocity * 2.0f), 0.0f, 1.0f);
        g_smoothedSpeedNorm = g_smoothedSpeedNorm * 0.55f + target * 0.45f; int effectiveLength = g_tailLength;
        if (g_speedScaling) effectiveLength = std::max(2, static_cast<int>(g_tailLength * (0.55f + 0.45f * g_smoothedSpeedNorm)));
        HistoryPushFront(point, effectiveLength); while (g_historyCount > g_tailLength) HistoryPopBack();
    } else if (g_fadeEnabled) { g_fadeAlpha *= g_fadeDecay; if (g_fadeAlpha < kFadeCutoff || g_historyCount < 2) { HistoryClear(); g_fadeAlpha = 0.0f; } }
    else { if (g_historyCount > 0) HistoryPopBack(); if (g_historyCount > 0) HistoryPopBack(); g_fadeAlpha = 1.0f; }
}

void SmearFrame(HWND hwnd, DWORD now) {
    if (g_hotkeySuspended) return; POINT point = {}; if (!GetCursorPos(&point)) return; HWND foreground = GetForegroundWindow(); const int appRule = CheckAppRuleCached(foreground);
    if (appRule < 0) { HistoryClear(); g_isSmearing = false; g_lowVelocityFrames = 0; g_fadeAlpha = 0.0f; HideOverlay(); return; }
    if (appRule == 0 && UpdateFullscreenState(now, foreground)) return;
    const int dx = point.x - g_lastPos.x, dy = point.y - g_lastPos.y; const float velocity = sqrtf(static_cast<float>(dx * dx + dy * dy)); g_currentVelocity = velocity; g_lastPos = point;
    UpdateTrailState(point, velocity);
    if (g_isSmearing || g_historyCount >= 2) UpdateTrailColorIfNeeded(now);
    if (g_historyCount < 2) { HideOverlay(); return; } if (!RenderTrail(hwnd)) { HideOverlay(); return; } ShowOverlay();
}

LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case kSettingsChangedMessage: LoadSettings(); return 0;
    case WM_HOTKEY:
        if (wParam == kHotkeyId) { g_hotkeySuspended = !g_hotkeySuspended; if (g_hotkeySuspended) { HistoryClear(); g_isSmearing = false; g_fadeAlpha = 0.0f; HideOverlay(); } }
        return 0;
    case WM_DISPLAYCHANGE: ReleaseScreenDC(); g_lastFullscreenStrongCheck = 0; return 0;
    case WM_CLOSE: DestroyWindow(hwnd); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    default: return DefWindowProc(hwnd, message, wParam, lParam);
    }
}

static LONGLONG QpcNow() { LARGE_INTEGER value = {}; QueryPerformanceCounter(&value); return value.QuadPart; }
static bool ArmFrameTimer(HANDLE timer, LONGLONG deadline, LONGLONG frequency) {
    const LONGLONG now = QpcNow(); LONGLONG ticks = deadline - now; if (ticks < 1) ticks = 1;
    const LONGLONG due100ns = std::max<LONGLONG>(1, ticks * 10000000LL / frequency); LARGE_INTEGER due = {}; due.QuadPart = -due100ns;
    return SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE) != FALSE;
}

DWORD WINAPI OverlayThreadProc(LPVOID) {
    const HRESULT coResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED); if (FAILED(coResult)) return 0;
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    LARGE_INTEGER qpcFrequency = {}; if (!QueryPerformanceFrequency(&qpcFrequency) || qpcFrequency.QuadPart <= 0) { CoUninitialize(); return 0; }
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_pD2DFactory)) || !g_pD2DFactory) { CoUninitialize(); return 0; }
    HINSTANCE instance = GetModuleHandle(nullptr); const wchar_t className[] = L"SmearFrameOverlayClass"; WNDCLASSW wc = {};
    wc.lpfnWndProc = OverlayWndProc; wc.hInstance = instance; wc.lpszClassName = className;
    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) { g_pD2DFactory->Release(); g_pD2DFactory = nullptr; CoUninitialize(); return 0; }
    HWND hwnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, className, L"SmearOverlay", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, instance, nullptr);
    if (!hwnd) { UnregisterClassW(className, instance); g_pD2DFactory->Release(); g_pD2DFactory = nullptr; CoUninitialize(); return 0; }
    g_overlayHwnd.store(hwnd); HideOverlay(); GetCursorPos(&g_lastPos); g_fullscreenForegroundWindow = GetForegroundWindow(); g_lastFullscreenStrongCheck = 0; g_fullscreenStrongSignal = false; g_fullscreenSuppressed = false;
    bool hotkeyRegistered = false; if (g_hotkeyEnabled) hotkeyRegistered = RegisterHotKey(hwnd, kHotkeyId, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'T') != FALSE;
    HANDLE frameTimer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!frameTimer) frameTimer = CreateWaitableTimerW(nullptr, FALSE, nullptr);
    if (!frameTimer) { if (hotkeyRegistered) UnregisterHotKey(hwnd, kHotkeyId); DestroyWindow(hwnd); g_overlayHwnd.store(nullptr); UnregisterClassW(className, instance); g_pD2DFactory->Release(); g_pD2DFactory = nullptr; CoUninitialize(); return 0; }
    const LONGLONG frameTicks = std::max<LONGLONG>(1, qpcFrequency.QuadPart / kTargetFrameRate); LONGLONG nextDeadline = QpcNow(); MSG message = {}; bool running = true;
    while (running) {
        nextDeadline += frameTicks;
        if (!ArmFrameTimer(frameTimer, nextDeadline, qpcFrequency.QuadPart)) break;
        HANDLE handles[] = {frameTimer};
        const DWORD waitResult = MsgWaitForMultipleObjectsEx(1, handles, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        if (waitResult == WAIT_OBJECT_0) {
            SmearFrame(hwnd, GetTickCount());
            const LONGLONG now = QpcNow(); if (nextDeadline <= now) { const LONGLONG skipped = (now - nextDeadline) / frameTicks + 1; nextDeadline += skipped * frameTicks; }
        } else if (waitResult == WAIT_OBJECT_0 + 1) {
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) { if (message.message == WM_QUIT) { running = false; break; } TranslateMessage(&message); DispatchMessageW(&message); }
        } else break;
    }
    CancelWaitableTimer(frameTimer); CloseHandle(frameTimer); if (hotkeyRegistered) UnregisterHotKey(hwnd, kHotkeyId);
    HideOverlay(); ReleaseBackbuffer(); ReleaseBackgroundSampler(); ReleaseScreenDC(); if (g_pD2DFactory) { g_pD2DFactory->Release(); g_pD2DFactory = nullptr; }
    DestroyWindow(hwnd); g_overlayHwnd.store(nullptr); UnregisterClassW(className, instance); CoUninitialize(); return 0;
}

BOOL WhTool_ModInit() { LoadSettings(); g_threadHandle = CreateThread(nullptr, 0, OverlayThreadProc, nullptr, 0, nullptr); return g_threadHandle != nullptr; }
void WhTool_ModUninit() { HWND hwnd = g_overlayHwnd.load(); if (hwnd && IsWindow(hwnd)) PostMessageW(hwnd, WM_CLOSE, 0, 0); if (g_threadHandle) { const DWORD result = WaitForSingleObject(g_threadHandle, 3000); if (result == WAIT_TIMEOUT) TerminateThread(g_threadHandle, 0); CloseHandle(g_threadHandle); g_threadHandle = nullptr; } g_overlayHwnd.store(nullptr); }
void WhTool_ModSettingsChanged() { HWND hwnd = g_overlayHwnd.load(); if (hwnd && IsWindow(hwnd)) PostMessageW(hwnd, kSettingsChangedMessage, 0, 0); }

bool g_isToolModProcessLauncher = false; HANDLE g_toolModProcessMutex = nullptr;
void WINAPI EntryPoint_Hook() { ExitThread(0); }
BOOL Wh_ModInit() {
    DWORD sessionId = 0; if (ProcessIdToSessionId(GetCurrentProcessId(), &sessionId) && sessionId == 0) return FALSE;
    bool isExcluded = false, isToolModProcess = false, isCurrentToolModProcess = false; int argc = 0; LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc); if (!argv) return FALSE;
    for (int i = 1; i < argc; ++i) if (wcscmp(argv[i], L"-service") == 0 || wcscmp(argv[i], L"-service-start") == 0 || wcscmp(argv[i], L"-service-stop") == 0) { isExcluded = true; break; }
    for (int i = 1; i + 1 < argc; ++i) if (wcscmp(argv[i], L"-tool-mod") == 0) { isToolModProcess = true; isCurrentToolModProcess = wcscmp(argv[i + 1], WH_MOD_ID) == 0; break; }
    LocalFree(argv); if (isExcluded) return FALSE;
    if (isCurrentToolModProcess) {
        g_toolModProcessMutex = CreateMutexW(nullptr, TRUE, L"windhawk-tool-mod_" WH_MOD_ID); if (!g_toolModProcessMutex || GetLastError() == ERROR_ALREADY_EXISTS) ExitProcess(1); if (!WhTool_ModInit()) ExitProcess(1);
        IMAGE_DOS_HEADER* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(GetModuleHandle(nullptr)); IMAGE_NT_HEADERS* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(reinterpret_cast<BYTE*>(dosHeader) + dosHeader->e_lfanew); void* entryPoint = reinterpret_cast<BYTE*>(dosHeader) + ntHeaders->OptionalHeader.AddressOfEntryPoint;
        Wh_SetFunctionHook(entryPoint, reinterpret_cast<void*>(EntryPoint_Hook), nullptr); return TRUE;
    }
    if (isToolModProcess) return FALSE; g_isToolModProcessLauncher = true; return TRUE;
}
void Wh_ModAfterInit() {
    if (!g_isToolModProcessLauncher) return;
    WCHAR currentProcessPath[MAX_PATH] = {}; const DWORD pathLength = GetModuleFileNameW(nullptr, currentProcessPath, ARRAYSIZE(currentProcessPath)); if (pathLength == 0 || pathLength == ARRAYSIZE(currentProcessPath)) return;
    WCHAR commandLine[MAX_PATH + 64] = {}; swprintf_s(commandLine, L"\"%s\" -tool-mod \"%s\"", currentProcessPath, WH_MOD_ID); HMODULE kernelModule = GetModuleHandleW(L"kernelbase.dll"); if (!kernelModule) kernelModule = GetModuleHandleW(L"kernel32.dll"); if (!kernelModule) return;
    using CreateProcessInternalW_t = BOOL(WINAPI*)(HANDLE, LPCWSTR, LPWSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES, WINBOOL, DWORD, LPVOID, LPCWSTR, LPSTARTUPINFO, LPPROCESS_INFORMATION, PHANDLE);
    auto createProcessInternal = reinterpret_cast<CreateProcessInternalW_t>(GetProcAddress(kernelModule, "CreateProcessInternalW")); STARTUPINFO si = {}; si.cb = sizeof(si); si.dwFlags = STARTF_FORCEOFFFEEDBACK; PROCESS_INFORMATION pi = {}; BOOL created = FALSE;
    if (createProcessInternal) created = createProcessInternal(nullptr, currentProcessPath, commandLine, nullptr, nullptr, FALSE, NORMAL_PRIORITY_CLASS, nullptr, nullptr, &si, &pi, nullptr);
    if (!created) created = CreateProcessW(nullptr, commandLine, nullptr, nullptr, FALSE, NORMAL_PRIORITY_CLASS, nullptr, nullptr, &si, &pi);
    if (created) { CloseHandle(pi.hProcess); CloseHandle(pi.hThread); }
}
void Wh_ModSettingsChanged() { if (!g_isToolModProcessLauncher) WhTool_ModSettingsChanged(); }
void Wh_ModUninit() { if (g_isToolModProcessLauncher) return; WhTool_ModUninit(); if (g_toolModProcessMutex) { CloseHandle(g_toolModProcessMutex); g_toolModProcessMutex = nullptr; } ExitProcess(0); }
