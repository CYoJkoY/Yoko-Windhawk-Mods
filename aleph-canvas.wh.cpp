// ==WindhawkMod==
// @id              aleph-canvas
// @name            Aleph - Infinite Window Canvas
// @name:zh-CN      Aleph - 无限窗口画布
// @description     Arrange open windows across a pannable canvas with live DWM previews.
// @description:zh-CN 在可平移画布上管理已打开的窗口，并显示 DWM 实时预览。
// @version         1.4.0
// @author          CYoJkoY
// @github          https://github.com/CYoJkoY
// @license         MIT
// @include         windhawk.exe
// @include         windhawk-mod.exe
// @compilerOptions -ladvapi32 -ld2d1 -ldwmapi -ldwrite -lgdi32 -lole32 -lshell32 -lshcore -luuid -lwindowscodecs
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Aleph - Infinite Window Canvas

Aleph presents eligible application windows on a navigable canvas. Live
windows use DWM thumbnails; minimized and other-desktop windows use icon cards.
Drag cards to arrange them without moving the real desktop windows.

## Controls

- `Ctrl+Alt+F12`: toggle the canvas (configurable).
- Mouse wheel: zoom around the pointer.
- Middle mouse drag: pan the canvas.
- Hold Space and drag empty canvas: pan.
- Drag a card: arrange it; nearby edges snap into alignment.
- Double-click a card or press Enter: focus the window; an other-desktop window is pulled onto this desktop first.
- Tab / Shift+Tab: select a card; arrow keys pan; Home fits all cards; Escape hides the canvas.
- Click or drag the minimap: navigate the canvas.

## Notes

Card positions are saved in the Windhawk mod storage directory as
`layout-<user-SID>.json`.
Canvas coordinates use physical pixels relative to the virtual desktop origin.
Icon cards ask the shell for the largest available icon (256 px when the jumbo
image list exists) and fall back to smaller lists, then to the window icon.
Arranging cards never moves real application windows. Activating an
other-desktop card asks the documented `IVirtualDesktopManager` interface to
move that window onto the desktop that displays the canvas, then focuses it. A
window that refuses the move, such as one owned by an elevated process, is
still brought to the foreground.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- Behavior:
    - toggle_hotkey: Ctrl+Alt+F12
      $name: Toggle hotkey
      $name:zh-CN: 显示/隐藏快捷键
      $description: Global hotkey used to show or hide the canvas. Use modifiers such as Ctrl, Alt, Shift, or Win plus one key. Avoid AltGr combinations.
      $description:zh-CN: 用于显示或隐藏画布的全局快捷键。可组合 Ctrl、Alt、Shift 或 Win 与一个按键；避免使用 AltGr 组合。
    - open_on_start: false
      $name: Open on startup
      $name:zh-CN: 启动时打开
      $description: Open the canvas when its Windhawk tool process starts.
      $description:zh-CN: Windhawk 工具进程启动时打开画布。
    - animation_ms: 140
      $name: Dive-in animation duration
      $name:zh-CN: 潜入动画时长
      $description: Duration of the camera transition when activating a window, in milliseconds. Range 60-600.
      $description:zh-CN: 激活窗口时的摄像机过渡时长，单位毫秒。范围 60-600。
    - rescan_interval_ms: 400
      $name: Window scan interval
      $name:zh-CN: 窗口扫描间隔
      $description: How often the visible canvas checks for opened or closed windows, in milliseconds. Range 100-5000.
      $description:zh-CN: 画布显示时检查窗口打开或关闭的频率，单位毫秒。范围 100-5000。
  $name: Behavior
  $name:zh-CN: 行为

- Canvas:
    - show_grid: true
      $name: Show grid
      $name:zh-CN: 显示网格
      $description: Draw a coordinate grid behind window cards.
      $description:zh-CN: 在窗口卡片后方绘制坐标网格。
    - grid_spacing: 96
      $name: Grid spacing
      $name:zh-CN: 网格间距
      $description: World-space spacing between grid lines. Range 16-512.
      $description:zh-CN: 网格线之间的世界坐标间距。范围 16-512。
    - gap: 32
      $name: Card gap
      $name:zh-CN: 卡片间距
      $description: Minimum world-space gap used when packing new cards. Range 0-256.
      $description:zh-CN: 自动排列新卡片时使用的最小世界坐标间距。范围 0-256。
    - snap_distance: 8
      $name: Snap distance
      $name:zh-CN: 磁吸距离
      $description: Screen-pixel distance for aligning card edges and centers. Range 0-48.
      $description:zh-CN: 卡片边缘和中心轴触发对齐的屏幕像素距离。范围 0-48。
    - include_other_desktops: true
      $name: Include other virtual desktops
      $name:zh-CN: 包含其他虚拟桌面
      $description: Show eligible windows cloaked on another virtual desktop as icon cards.
      $description:zh-CN: 将其他虚拟桌面上符合条件的隐藏窗口显示为图标卡片。
    - save_layout: true
      $name: Save card positions
      $name:zh-CN: 保存卡片位置
      $description: Persist card positions between runs.
      $description:zh-CN: 在多次运行之间保存卡片位置。
    - initial_layout: packed
      $name: Initial layout
      $name:zh-CN: 初始布局
      $description: Place new cards in free space or at their real desktop positions.
      $description:zh-CN: 将新卡片放入空闲区域，或放在其实际桌面位置。
      $options:
        - packed: Packed
        - desktop: Desktop positions
      $options:zh-CN:
        - packed: 自动排列
        - desktop: 桌面位置
  $name: Canvas
  $name:zh-CN: 画布

- Appearance:
    - min_zoom_percent: 15
      $name: Minimum zoom
      $name:zh-CN: 最小缩放
      $description: Minimum canvas zoom percentage. Range 15-245.
      $description:zh-CN: 画布最小缩放百分比。范围 15-245。
    - max_zoom_percent: 250
      $name: Maximum zoom
      $name:zh-CN: 最大缩放
      $description: Maximum canvas zoom percentage. Range 20-250 and at least five points above the minimum.
      $description:zh-CN: 画布最大缩放百分比。范围 20-250，且至少比最小值高 5。
    - minimap_enabled: true
      $name: Show minimap
      $name:zh-CN: 显示小地图
      $description: Show the interactive overview map in the lower-right corner.
      $description:zh-CN: 在右下角显示可交互的概览地图。
    - minimap_size: 180
      $name: Minimap size
      $name:zh-CN: 小地图尺寸
      $description: Minimap width and height in display-scaled pixels. Range 96-420.
      $description:zh-CN: 小地图宽高，按显示缩放比例调整。范围 96-420。
    - background_color: 0B0E14
      $name: Background color
      $name:zh-CN: 背景颜色
      $description: Six-digit RGB hex color without the leading #.
      $description:zh-CN: 六位 RGB 十六进制颜色，不含开头的 #。
    - grid_color: 1B2436
      $name: Grid color
      $name:zh-CN: 网格颜色
      $description: Six-digit RGB hex color without the leading #.
      $description:zh-CN: 六位 RGB 十六进制颜色，不含开头的 #。
    - accent_color: 38BDF8
      $name: Accent color
      $name:zh-CN: 强调色
      $description: Six-digit RGB hex color without the leading #.
      $description:zh-CN: 六位 RGB 十六进制颜色，不含开头的 #。
    - text_color: E2E8F0
      $name: Text color
      $name:zh-CN: 文字颜色
      $description: Six-digit RGB hex color without the leading #.
      $description:zh-CN: 六位 RGB 十六进制颜色，不含开头的 #。
    - muted_text_color: 64748B
      $name: Muted text color
      $name:zh-CN: 次要文字颜色
      $description: Six-digit RGB hex color without the leading #.
      $description:zh-CN: 六位 RGB 十六进制颜色，不含开头的 #。
    - card_fill_color: "111827"
      $name: Card fill color
      $name:zh-CN: 卡片填充颜色
      $description: Six-digit RGB hex color without the leading #.
      $description:zh-CN: 六位 RGB 十六进制颜色，不含开头的 #。
  $name: Appearance
  $name:zh-CN: 外观

- Application:
    - exclude_list: [""]
      $name: Excluded executables
      $name:zh-CN: 排除的可执行文件
      $description: One executable name per entry, such as `program.exe`. Names are matched case-insensitively; lines beginning with # are comments.
      $description:zh-CN: 每个条目填写一个可执行文件名，例如 `program.exe`。名称不区分大小写，以 # 开头的条目为注释。
  $name: Application
  $name:zh-CN: 应用程序
*/
// ==/WindhawkModSettings==


// System includes plus the shared Windhawk and Win32 helper headers.

#include <windows.h>
#include <windowsx.h>
#include <sddl.h>
#include <windhawk_api.h>
#include <windhawk_utils.h>
#include <d2d1.h>
#include <dwrite.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shellscalingapi.h>
#include <shobjidl.h>
#include <wincodec.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// The DWM cloak values are older than the SDK headers some builds ship.
#ifndef DWMWA_CLOAKED
#define DWMWA_CLOAKED 14
#endif
#ifndef DWM_CLOAKED_APP
#define DWM_CLOAKED_APP 0x00000001
#endif
#ifndef DWM_CLOAKED_SHELL
#define DWM_CLOAKED_SHELL 0x00000002
#endif

// Repository-wide helpers. The paths stay relative to this header so the
// amalgamator can inline them in the same way as local modules.

// Header-only helpers around the Windhawk API.
//
// This file is inlined by tools/bundle.py, so it must stay self-contained and
// must never include another repository header. Every function is inline so
// the amalgamated translation unit keeps a single definition.

#include <windows.h>

#include <windhawk_api.h>
#include <windhawk_utils.h>

#include <string>

namespace yoko {

// Reads an integer setting and clamps it to the documented range.
//
// Windhawk returns 0 for a missing value and does not reject out-of-range
// input, so the clamp has to happen here. An inverted range is a programming
// error in the settings block; it is logged and collapsed to the lower bound
// instead of producing undefined behavior.
inline int GetClampedIntSetting(const wchar_t* path, int minimum, int maximum) {
    if (path == nullptr) {
        Wh_Log(L"GetClampedIntSetting called without a setting path");
        return minimum;
    }
    if (maximum < minimum) {
        Wh_Log(L"GetClampedIntSetting range is inverted for %s", path);
        return minimum;
    }
    const int value = Wh_GetIntSetting(path);
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

// Reads a string setting and falls back when the value is absent.
//
// WindhawkUtils::StringSetting owns the buffer, so the result is copied before
// the temporary goes out of scope.
inline std::wstring GetStringSetting(const wchar_t* path,
                                     const wchar_t* fallback) {
    const wchar_t* safeFallback = fallback != nullptr ? fallback : L"";
    if (path == nullptr) {
        Wh_Log(L"GetStringSetting called without a setting path");
        return safeFallback;
    }
    WindhawkUtils::StringSetting value =
        WindhawkUtils::StringSetting::make(path);
    const wchar_t* text = value.get();
    if (text == nullptr) {
        return safeFallback;
    }
    return text;
}

// Logs the current thread's last error next to a short context string.
//
// The error is captured first so the logging call itself cannot replace it.
inline void LogLastError(const wchar_t* context) {
    const DWORD error = GetLastError();
    Wh_Log(L"%s failed: %lu", context != nullptr ? context : L"Win32 call",
           error);
}

}  // namespace yoko

// Header-only Win32 ownership and validation helpers.
//
// This file is inlined by tools/bundle.py, so it must stay self-contained and
// must never include another repository header.

#include <windows.h>

namespace yoko::win32 {

inline constexpr float kMinimumDpiScale = 0.5f;
inline constexpr float kMaximumDpiScale = 4.0f;
inline constexpr UINT kDefaultDpi = 96;

// Owns a kernel handle and closes it exactly once.
class UniqueHandle {
public:
    UniqueHandle() = default;

    explicit UniqueHandle(HANDLE handle) : handle_(handle) {}

    ~UniqueHandle() { reset(); }

    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;

    UniqueHandle(UniqueHandle&& other) noexcept : handle_(other.release()) {}

    UniqueHandle& operator=(UniqueHandle&& other) noexcept {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }

    HANDLE get() const { return handle_; }

    bool valid() const {
        return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE;
    }

    HANDLE release() {
        const HANDLE handle = handle_;
        handle_ = nullptr;
        return handle;
    }

    void reset(HANDLE handle = nullptr) {
        if (handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
        }
        handle_ = handle;
    }

private:
    HANDLE handle_ = nullptr;
};

// Owns a GDI object and deletes it exactly once.
class UniqueGdiObject {
public:
    UniqueGdiObject() = default;

    explicit UniqueGdiObject(HGDIOBJ object) : object_(object) {}

    ~UniqueGdiObject() { reset(); }

    UniqueGdiObject(const UniqueGdiObject&) = delete;
    UniqueGdiObject& operator=(const UniqueGdiObject&) = delete;

    UniqueGdiObject(UniqueGdiObject&& other) noexcept
        : object_(other.release()) {}

    UniqueGdiObject& operator=(UniqueGdiObject&& other) noexcept {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }

    HGDIOBJ get() const { return object_; }

    bool valid() const { return object_ != nullptr; }

    HGDIOBJ release() {
        const HGDIOBJ object = object_;
        object_ = nullptr;
        return object;
    }

    void reset(HGDIOBJ object = nullptr) {
        if (object_ != nullptr) {
            DeleteObject(object_);
        }
        object_ = object;
    }

private:
    HGDIOBJ object_ = nullptr;
};

// A CURSORINFO that is zero-initialized with the required cbSize.
struct UniqueCursorInfo {
    CURSORINFO value{};

    UniqueCursorInfo() { value.cbSize = sizeof(value); }

    bool query() { return GetCursorInfo(&value) != FALSE; }

    bool visible() const {
        return (value.flags & CURSOR_SHOWING) != 0 && value.hCursor != nullptr;
    }
};

// A window handle is only usable while the window still exists.
inline bool IsValidWindow(HWND window) {
    return window != nullptr && IsWindow(window) != FALSE;
}

// Converts a monitor DPI to a clamped scale factor. 96 DPI maps to 1.0.
inline float DpiScaleForDpi(UINT dpi) {
    const UINT effectiveDpi = dpi == 0 ? kDefaultDpi : dpi;
    const float scale = static_cast<float>(effectiveDpi) /
                        static_cast<float>(kDefaultDpi);
    if (scale < kMinimumDpiScale) {
        return kMinimumDpiScale;
    }
    if (scale > kMaximumDpiScale) {
        return kMaximumDpiScale;
    }
    return scale;
}

}  // namespace yoko::win32

// Compile-time geometry, timing and policy values.
//
// Everything a user may want to change is a Windhawk setting; this header only
// holds canvas structure that must stay internally consistent (frame pacing,
// hit-test tolerances, cap sizes).


constexpr UINT kToggleHotkeyId = 0xA1E1;
constexpr int kMaxCards = 64;
constexpr int kMaxLayoutEntries = 256;
constexpr int kMaxExcludeEntries = 64;

constexpr int kFrameIntervalMs = 16;
constexpr int kMinRescanIntervalMs = 100;
constexpr int kMaxRescanIntervalMs = 5000;

constexpr int kMinZoomPercent = 15;
constexpr int kMaxZoomPercent = 250;
constexpr int kMinAnimationMs = 60;
constexpr int kMaxAnimationMs = 600;

constexpr float kZoomStep = 1.15f;
constexpr float kCameraSmoothingPerSecond = 20.0f;
constexpr float kCameraEpsilonPx = 0.2f;
constexpr float kZoomEpsilon = 0.001f;

constexpr float kCardHeaderPx = 28.0f;
constexpr float kCardHeaderMinPx = 10.0f;
constexpr float kCardHeaderMaxPx = 30.0f;
constexpr float kCardHeaderButtonPx = 18.0f;
constexpr float kCardHeaderButtonInsetPx = 5.0f;
constexpr float kCardHeaderButtonGapPx = 4.0f;
constexpr float kCardCornerRadiusPx = 8.0f;
constexpr float kCardTitlePaddingPx = 10.0f;
constexpr float kCardIconMaxPx = 128.0f;

constexpr float kMinimapMarginPx = 16.0f;
constexpr int kMinimapMinSize = 96;
constexpr int kMinimapMaxSize = 420;
constexpr float kMinimapPaddingPx = 8.0f;

constexpr int kMaxGridLines = 400;
constexpr float kMinGridSpacingPx = 6.0f;

constexpr DWORD kLayoutSaveDelayMs = 800;
constexpr DWORD kIconRetryDelayMs = 4000;
constexpr DWORD kThumbnailRetryDelayMs = 2000;

// Value types shared by the canvas modules.


struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct RectF {
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;

    float width() const { return right - left; }
    float height() const { return bottom - top; }
    float centerX() const { return (left + right) * 0.5f; }
    float centerY() const { return (top + bottom) * 0.5f; }
    bool empty() const { return right <= left || bottom <= top; }
};

inline RectF MakeRectF(float left, float top, float width, float height) {
    return RectF{left, top, left + width, top + height};
}

inline RectF RectFromRect(const RECT& rect) {
    return RectF{static_cast<float>(rect.left), static_cast<float>(rect.top),
                 static_cast<float>(rect.right),
                 static_cast<float>(rect.bottom)};
}

inline RectF UnionRect(const RectF& a, const RectF& b) {
    if (a.empty()) {
        return b;
    }
    if (b.empty()) {
        return a;
    }
    return RectF{std::min(a.left, b.left), std::min(a.top, b.top),
                 std::max(a.right, b.right), std::max(a.bottom, b.bottom)};
}

inline RectF GrowRect(const RectF& rect, float amount) {
    return RectF{rect.left - amount, rect.top - amount, rect.right + amount,
                 rect.bottom + amount};
}

inline bool RectsOverlap(const RectF& a, const RectF& b) {
    return a.left < b.right && b.left < a.right && a.top < b.bottom &&
           b.top < a.bottom;
}

inline bool ContainsPoint(const RectF& rect, float x, float y) {
    return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
}

struct Camera {
    float centerX = 0.0f;
    float centerY = 0.0f;
    float zoom = 0.6f;
    float targetCenterX = 0.0f;
    float targetCenterY = 0.0f;
    float targetZoom = 0.6f;
    bool zoomAnchored = false;
    Vec2 zoomAnchorWorld{};
    Vec2 zoomAnchorScreen{};

    bool settled() const {
        return std::fabs(targetCenterX - centerX) < kCameraEpsilonPx &&
               std::fabs(targetCenterY - centerY) < kCameraEpsilonPx &&
               std::fabs(targetZoom - zoom) < kZoomEpsilon;
    }

    void snapToTarget() {
        centerX = targetCenterX;
        centerY = targetCenterY;
        zoom = targetZoom;
    }
};

enum class WindowVisualState {
    Live,
    Minimized,
    OtherDesktop,
};

enum class InitialLayout {
    Packed,
    Desktop,
};

enum class CardHeaderAction : uint8_t {
    None,
    Pin,
    Close,
};

// One DWM thumbnail handle. Registration is the only ownership, so moving a
// card inside the vector never leaks or double-unregisters.
class Thumbnail {
public:
    Thumbnail() = default;
    ~Thumbnail() { reset(); }

    Thumbnail(const Thumbnail&) = delete;
    Thumbnail& operator=(const Thumbnail&) = delete;

    Thumbnail(Thumbnail&& other) noexcept : handle_(other.release()) {}

    Thumbnail& operator=(Thumbnail&& other) noexcept {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }

    HTHUMBNAIL get() const { return handle_; }
    bool valid() const { return handle_ != nullptr; }

    HTHUMBNAIL release() {
        const HTHUMBNAIL handle = handle_;
        handle_ = nullptr;
        return handle;
    }

    void reset(HTHUMBNAIL handle = nullptr) {
        if (handle_ != nullptr) {
            DwmUnregisterThumbnail(handle_);
        }
        handle_ = handle;
    }

private:
    HTHUMBNAIL handle_ = nullptr;
};

struct Card {
    HWND hwnd = nullptr;
    DWORD processId = 0;
    DWORD threadId = 0;
    bool destroyed = false;
    std::wstring windowClass;
    WindowVisualState state = WindowVisualState::Live;
    RECT framePx = {};
    RectF world = {};
    std::wstring title;
    std::wstring fingerprint;
    std::wstring exePath;
    Thumbnail thumbnail;
    bool thumbnailFailed = false;
    DWORD thumbnailRetryTick = 0;
    RECT thumbnailDestination = {};
    bool thumbnailVisible = false;
    bool pinned = false;
};

struct ScannedWindow {
    HWND hwnd = nullptr;
    DWORD processId = 0;
    DWORD threadId = 0;
    std::wstring windowClass;
    WindowVisualState state = WindowVisualState::Live;
    RECT framePx = {};
    std::wstring title;
    std::wstring fingerprint;
    std::wstring exePath;
};

inline bool MatchesWindow(const Card& card, const ScannedWindow& window) {
    return !card.destroyed && card.hwnd == window.hwnd &&
           card.processId == window.processId &&
           card.threadId == window.threadId &&
           card.windowClass == window.windowClass &&
           card.exePath == window.exePath;
}

struct CanvasSettings {
    std::wstring toggleHotkey = L"Ctrl+Alt+F12";
    bool openOnStart = false;
    int animationMs = 140;
    int rescanIntervalMs = 400;
    bool showGrid = true;
    int gridSpacing = 96;
    int gap = 32;
    int snapDistance = 8;
    int minZoomPercent = 15;
    int maxZoomPercent = 250;
    bool includeOtherDesktops = true;
    bool saveLayout = true;
    bool minimapEnabled = true;
    int minimapSize = 180;
    InitialLayout initialLayout = InitialLayout::Packed;
    uint32_t backgroundColor = 0x0B0E14;
    uint32_t gridColor = 0x1B2436;
    uint32_t accentColor = 0x38BDF8;
    uint32_t textColor = 0xE2E8F0;
    uint32_t mutedTextColor = 0x64748B;
    uint32_t cardFillColor = 0x111827;
    std::vector<std::wstring> excludeList;
};

struct LayoutEntry {
    std::wstring fingerprint;
    RectF world = {};
    bool pinned = false;
};

struct LayoutSnapshot {
    std::vector<LayoutEntry> entries;
};

// The single mutable canvas state owned by the worker thread.


struct CanvasContext {
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
    // Non-owning. The worker thread creates the COM manager before the canvas
    // loop starts and releases it after the loop returns.
    IVirtualDesktopManager* desktopManager = nullptr;
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

// Camera projections and small numeric helpers.


inline float ClampFloat(float value, float minimum, float maximum) {
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

inline float ClampZoom(float zoom) {
    const float minimum =
        static_cast<float>(context.settings.minZoomPercent) / 100.0f;
    const float maximum =
        static_cast<float>(context.settings.maxZoomPercent) / 100.0f;
    return ClampFloat(zoom, minimum, std::max(minimum, maximum));
}

// Window chrome uses physical pixels but should stay proportional to the
// display that hosts the canvas, so every fixed metric is scaled by the DPI of
// the monitor under the cursor.
inline float ChromeScale() {
    return yoko::win32::DpiScaleForDpi(static_cast<UINT>(context.chromeDpi));
}

inline float CardHeaderHeight() {
    return std::clamp(kCardHeaderPx * ChromeScale() * context.camera.zoom,
                      kCardHeaderMinPx * ChromeScale(),
                      kCardHeaderMaxPx * ChromeScale());
}

inline float ScreenCenterX() {
    return static_cast<float>(context.virtualScreen.right -
                              context.virtualScreen.left) *
           0.5f;
}

inline float ScreenCenterY() {
    return static_cast<float>(context.virtualScreen.bottom -
                              context.virtualScreen.top) *
           0.5f;
}

// World units are virtual-screen pixels, so a card's stored rectangle matches
// the window's own frame rectangle until the user rearranges the canvas.
inline RectF WorldToScreen(const RectF& world, const Camera& camera) {
    return RectF{
        (world.left - camera.centerX) * camera.zoom + ScreenCenterX(),
        (world.top - camera.centerY) * camera.zoom + ScreenCenterY(),
        (world.right - camera.centerX) * camera.zoom + ScreenCenterX(),
        (world.bottom - camera.centerY) * camera.zoom + ScreenCenterY()};
}

inline RectF CardScreenBounds(const Card& card) {
    RectF rect = WorldToScreen(card.world, context.camera);
    rect.top -= CardHeaderHeight();
    return rect;
}

inline Vec2 ScreenToWorld(Vec2 point, const Camera& camera) {
    return Vec2{(point.x - ScreenCenterX()) / camera.zoom + camera.centerX,
                (point.y - ScreenCenterY()) / camera.zoom + camera.centerY};
}

// Keeps the world point under the cursor fixed while the zoom changes.
inline void ZoomAtPoint(Camera& camera, float newZoom, Vec2 cursor) {
    const float clamped = ClampZoom(newZoom);
    const Vec2 anchor = ScreenToWorld(cursor, camera);
    camera.targetZoom = clamped;
    camera.targetCenterX = anchor.x - (cursor.x - ScreenCenterX()) / clamped;
    camera.targetCenterY = anchor.y - (cursor.y - ScreenCenterY()) / clamped;
    camera.zoomAnchorWorld = anchor;
    camera.zoomAnchorScreen = cursor;
    camera.zoomAnchored = true;
}

inline void CenterOnPoint(Camera& camera, Vec2 worldPoint) {
    camera.zoomAnchored = false;
    camera.targetCenterX = worldPoint.x;
    camera.targetCenterY = worldPoint.y;
}

inline float SmoothTowards(float current, float target, float factor) {
    const float blended = current + (target - current) * factor;
    return std::fabs(target - blended) < kCameraEpsilonPx ? target : blended;
}

inline void AdvanceCamera(Camera& camera, float seconds) {
    const float factor =
        1.0f - std::exp(-kCameraSmoothingPerSecond * std::max(seconds, 0.0f));
    const float zoomDelta = camera.targetZoom - camera.zoom;
    camera.zoom = std::fabs(zoomDelta) < kZoomEpsilon
                      ? camera.targetZoom
                      : camera.zoom + zoomDelta * factor;
    if (camera.zoomAnchored) {
        camera.centerX = camera.zoomAnchorWorld.x -
            (camera.zoomAnchorScreen.x - ScreenCenterX()) / camera.zoom;
        camera.centerY = camera.zoomAnchorWorld.y -
            (camera.zoomAnchorScreen.y - ScreenCenterY()) / camera.zoom;
        return;
    }
    camera.centerX = SmoothTowards(camera.centerX, camera.targetCenterX, factor);
    camera.centerY = SmoothTowards(camera.centerY, camera.targetCenterY, factor);
}

inline float EaseOutCubic(float t) {
    const float clamped = ClampFloat(t, 0.0f, 1.0f);
    const float inverse = 1.0f - clamped;
    return 1.0f - inverse * inverse * inverse;
}

inline RectF ViewportWorld(const Camera& camera) {
    const Vec2 topLeft = ScreenToWorld(Vec2{0.0f, 0.0f}, camera);
    const Vec2 bottomRight =
        ScreenToWorld(Vec2{static_cast<float>(context.virtualScreen.right -
                                              context.virtualScreen.left),
                           static_cast<float>(context.virtualScreen.bottom -
                                              context.virtualScreen.top)},
                      camera);
    return RectF{topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};
}

inline bool ParseHexColor(const wchar_t* text, uint32_t& out) {
    if (text == nullptr) {
        return false;
    }
    const wchar_t* cursor = text;
    while (*cursor == L' ' || *cursor == L'#') {
        ++cursor;
    }
    if (wcslen(cursor) != 6) {
        return false;
    }
    uint32_t value = 0;
    for (int index = 0; index < 6; ++index) {
        const wchar_t digit = cursor[index];
        uint32_t nibble = 0;
        if (digit >= L'0' && digit <= L'9') {
            nibble = static_cast<uint32_t>(digit - L'0');
        } else if (digit >= L'a' && digit <= L'f') {
            nibble = static_cast<uint32_t>(digit - L'a' + 10);
        } else if (digit >= L'A' && digit <= L'F') {
            nibble = static_cast<uint32_t>(digit - L'A' + 10);
        } else {
            return false;
        }
        value = (value << 4) | nibble;
    }
    out = value;
    return true;
}

// Small string helpers shared by settings, scanning and rendering.


static std::wstring TrimText(const std::wstring& value) {
    const size_t first = value.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos) {
        return std::wstring();
    }
    const size_t last = value.find_last_not_of(L" \t\r\n");
    return value.substr(first, last - first + 1);
}

static std::wstring ToLowerText(const std::wstring& value) {
    std::wstring result = value;
    for (wchar_t& item : result) {
        item = static_cast<wchar_t>(towlower(item));
    }
    return result;
}

static std::wstring ExecutableName(const std::wstring& path) {
    const size_t slash = path.find_last_of(L"\\/");
    return ToLowerText(slash == std::wstring::npos ? path
                                                   : path.substr(slash + 1));
}

static std::string ToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return std::string();
    }
    const int required = WideCharToMultiByte(
        CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0,
        nullptr, nullptr);
    if (required <= 0) {
        return std::string();
    }
    std::string result(static_cast<size_t>(required), '\0');
    if (WideCharToMultiByte(CP_UTF8, 0, value.c_str(),
                            static_cast<int>(value.size()), result.data(),
                            required, nullptr, nullptr) != required) {
        return std::string();
    }
    return result;
}

static std::wstring FromUtf8(const std::string& value) {
    if (value.empty()) {
        return std::wstring();
    }
    const int required = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.c_str(),
        static_cast<int>(value.size()), nullptr, 0);
    if (required <= 0) {
        return std::wstring();
    }
    std::wstring result(static_cast<size_t>(required), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.c_str(),
                            static_cast<int>(value.size()), result.data(),
                            required) != required) {
        return std::wstring();
    }
    return result;
}

static std::wstring FormatInteger(int value) {
    wchar_t buffer[32] = {};
    swprintf_s(buffer, L"%d", value);
    return std::wstring(buffer);
}

// Settings schema, hotkey parsing and persistence paths.


static uint32_t ReadColorSetting(const wchar_t* path, uint32_t fallback) {
    uint32_t parsed = 0;
    WindhawkUtils::StringSetting value = WindhawkUtils::StringSetting::make(path);
    return ParseHexColor(value.get(), parsed) ? parsed : fallback;
}

struct NamedHotkeyValue {
    const wchar_t* name;
    UINT value;
};

static constexpr NamedHotkeyValue kNamedHotkeyKeys[] = {
    {L"space", VK_SPACE},       {L"tab", VK_TAB},
    {L"enter", VK_RETURN},      {L"return", VK_RETURN},
    {L"escape", VK_ESCAPE},     {L"esc", VK_ESCAPE},
    {L"backspace", VK_BACK},    {L"insert", VK_INSERT},
    {L"delete", VK_DELETE},     {L"home", VK_HOME},
    {L"end", VK_END},           {L"pageup", VK_PRIOR},
    {L"pagedown", VK_NEXT},     {L"left", VK_LEFT},
    {L"right", VK_RIGHT},       {L"up", VK_UP},
    {L"down", VK_DOWN},
};

static constexpr NamedHotkeyValue kNamedHotkeyModifiers[] = {
    {L"ctrl", MOD_CONTROL}, {L"control", MOD_CONTROL},
    {L"alt", MOD_ALT},      {L"shift", MOD_SHIFT},
    {L"win", MOD_WIN},      {L"windows", MOD_WIN},
};

static bool ParseCharacterKey(wchar_t character, UINT& key) {
    const bool digit = character >= L'0' && character <= L'9';
    const bool letter = character >= L'a' && character <= L'z';
    if (!digit && !letter) {
        return false;
    }
    key = static_cast<UINT>(towupper(character));
    return true;
}

static bool ParseFunctionKey(const std::wstring& token, UINT& key) {
    if (token.size() < 2 || token[0] != L'f') {
        return false;
    }
    UINT number = 0;
    for (size_t index = 1; index < token.size(); ++index) {
        const wchar_t character = token[index];
        if (character < L'0' || character > L'9') {
            return false;
        }
        number = number * 10 + static_cast<UINT>(character - L'0');
        if (number > 24) {
            return false;
        }
    }
    if (number == 0) {
        return false;
    }
    key = VK_F1 + number - 1;
    return true;
}

static bool ParseKeyName(const std::wstring& token, UINT& key) {
    const std::wstring lower = ToLowerText(token);
    if (lower.size() == 1) {
        return ParseCharacterKey(lower[0], key);
    }
    for (const NamedHotkeyValue& entry : kNamedHotkeyKeys) {
        if (lower == entry.name) {
            key = entry.value;
            return true;
        }
    }
    return ParseFunctionKey(lower, key);
}

struct ParsedHotkey {
    UINT modifiers = 0;
    UINT key = 0;
};

static bool ParseHotkeyToken(const std::wstring& token, ParsedHotkey& parsed) {
    const std::wstring lower = ToLowerText(token);
    for (const NamedHotkeyValue& entry : kNamedHotkeyModifiers) {
        if (lower == entry.name) {
            parsed.modifiers |= entry.value;
            return true;
        }
    }
    if (parsed.key != 0) {
        return false;
    }
    return ParseKeyName(lower, parsed.key);
}

static bool ParseHotkeyText(const std::wstring& text, UINT& modifiers, UINT& key) {
    modifiers = 0;
    key = 0;
    const std::wstring trimmed = TrimText(text);
    if (trimmed.empty()) {
        return false;
    }

    size_t start = 0;
    ParsedHotkey parsed;
    while (start <= trimmed.size()) {
        const size_t plus = trimmed.find(L'+', start);
        const std::wstring token = TrimText(
            trimmed.substr(start, plus == std::wstring::npos ? std::wstring::npos
                                                            : plus - start));
        if (token.empty() || !ParseHotkeyToken(token, parsed)) {
            return false;
        }
        if (plus == std::wstring::npos) {
            break;
        }
        start = plus + 1;
    }
    if (parsed.key == 0) {
        return false;
    }
    modifiers = parsed.modifiers;
    key = parsed.key;
    return true;
}

static void LoadExcludeList() {
    context.settings.excludeList.clear();
    int consecutiveEmpty = 0;
    for (int index = 0; index < kMaxExcludeEntries; ++index) {
        WindhawkUtils::StringSetting setting = WindhawkUtils::StringSetting::make(
            L"Application.exclude_list[%d]", index);
        const wchar_t* value = setting.get();
        const bool empty = value == nullptr || *value == L'\0';
        consecutiveEmpty = empty ? consecutiveEmpty + 1 : 0;
        if (consecutiveEmpty >= 2) {
            break;
        }
        if (empty) {
            continue;
        }
        const std::wstring lower = ToLowerText(TrimText(value));
        if (!lower.empty() && lower[0] != L'#') {
            context.settings.excludeList.push_back(lower);
        }
    }
}

static void LoadSettings() {
    CanvasSettings& settings = context.settings;

    WindhawkUtils::StringSetting hotkey =
        WindhawkUtils::StringSetting::make(L"Behavior.toggle_hotkey");
    const wchar_t* hotkeyText = hotkey.get();
    settings.toggleHotkey =
        hotkeyText != nullptr && *hotkeyText != L'\0'
            ? TrimText(hotkeyText)
            : L"Ctrl+Alt+F12";

    settings.openOnStart =
        yoko::GetClampedIntSetting(L"Behavior.open_on_start", 0, 1) != 0;
    settings.animationMs =
        yoko::GetClampedIntSetting(L"Behavior.animation_ms", kMinAnimationMs,
                                   kMaxAnimationMs);
    settings.rescanIntervalMs = yoko::GetClampedIntSetting(
        L"Behavior.rescan_interval_ms", kMinRescanIntervalMs,
        kMaxRescanIntervalMs);

    settings.showGrid =
        yoko::GetClampedIntSetting(L"Canvas.show_grid", 0, 1) != 0;
    settings.gridSpacing =
        yoko::GetClampedIntSetting(L"Canvas.grid_spacing", 16, 512);
    settings.gap = yoko::GetClampedIntSetting(L"Canvas.gap", 0, 256);
    settings.snapDistance =
        yoko::GetClampedIntSetting(L"Canvas.snap_distance", 0, 48);
    settings.includeOtherDesktops =
        yoko::GetClampedIntSetting(L"Canvas.include_other_desktops", 0, 1) != 0;
    settings.saveLayout =
        yoko::GetClampedIntSetting(L"Canvas.save_layout", 0, 1) != 0;

    settings.minZoomPercent = yoko::GetClampedIntSetting(
        L"Appearance.min_zoom_percent", kMinZoomPercent, kMaxZoomPercent - 5);
    settings.maxZoomPercent = yoko::GetClampedIntSetting(
        L"Appearance.max_zoom_percent", settings.minZoomPercent + 5,
        kMaxZoomPercent);

    settings.minimapEnabled =
        yoko::GetClampedIntSetting(L"Appearance.minimap_enabled", 0, 1) != 0;
    settings.minimapSize = yoko::GetClampedIntSetting(
        L"Appearance.minimap_size", kMinimapMinSize, kMinimapMaxSize);

    settings.backgroundColor =
        ReadColorSetting(L"Appearance.background_color", 0x0B0E14);
    settings.gridColor = ReadColorSetting(L"Appearance.grid_color", 0x1B2436);
    settings.accentColor = ReadColorSetting(L"Appearance.accent_color", 0x38BDF8);
    settings.textColor = ReadColorSetting(L"Appearance.text_color", 0xE2E8F0);
    settings.mutedTextColor =
        ReadColorSetting(L"Appearance.muted_text_color", 0x64748B);
    settings.cardFillColor =
        ReadColorSetting(L"Appearance.card_fill_color", 0x111827);

    WindhawkUtils::StringSetting initialLayout =
        WindhawkUtils::StringSetting::make(L"Canvas.initial_layout");
    const wchar_t* layoutText = initialLayout.get();
    settings.initialLayout =
        layoutText != nullptr && _wcsicmp(layoutText, L"desktop") == 0
            ? InitialLayout::Desktop
            : InitialLayout::Packed;

    LoadExcludeList();

    context.camera.targetZoom = ClampZoom(context.camera.targetZoom);
    context.camera.zoom = ClampZoom(context.camera.zoom);
}

static std::wstring LayoutDirectory() {
    WCHAR path[MAX_PATH] = {};
    const size_t length = Wh_GetModStoragePath(path, ARRAYSIZE(path));
    if (length == 0) {
        return std::wstring();
    }
    return std::wstring(path, length);
}

static bool CurrentUserSid(std::wstring& sid) {
    HANDLE tokenHandle = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tokenHandle)) {
        return false;
    }
    yoko::win32::UniqueHandle token(tokenHandle);
    DWORD length = 0;
    const bool queryFailed = !GetTokenInformation(token.get(), TokenUser,
                                                  nullptr, 0, &length);
    if (queryFailed && GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        return false;
    }
    std::vector<BYTE> tokenData(length);
    if (!GetTokenInformation(token.get(), TokenUser, tokenData.data(), length,
                             &length)) {
        return false;
    }
    const auto* user = reinterpret_cast<const TOKEN_USER*>(tokenData.data());
    LPWSTR sidText = nullptr;
    if (!ConvertSidToStringSidW(user->User.Sid, &sidText)) {
        return false;
    }
    sid = sidText;
    LocalFree(sidText);
    return true;
}

static std::wstring LayoutFileName() {
    std::wstring sid;
    if (!CurrentUserSid(sid)) {
        Wh_Log(L"Aleph: cannot resolve the current user SID");
        return L"layout.json";
    }
    return L"layout-" + sid + L".json";
}

static std::wstring LayoutFilePath() {
    const std::wstring directory = LayoutDirectory();
    if (directory.empty()) {
        return std::wstring();
    }
    return directory + L"\\" + LayoutFileName();
}

// Crash-safe JSON persistence for card positions.

#include <cerrno>
#include <limits>
#include <locale.h>
#include <shlobj.h>

static std::wstring EscapeJsonText(const std::wstring& value) {
    std::wstring result;
    for (const wchar_t character : value) {
        if (character == L'"' || character == L'\\') {
            result.push_back(L'\\');
            result.push_back(character);
            continue;
        }
        if (character == L'\n') {
            result += L"\\n";
            continue;
        }
        if (character < 0x20) {
            static constexpr wchar_t hexDigits[] = L"0123456789abcdef";
            result += L"\\u00";
            result.push_back(hexDigits[(character >> 4) & 0xF]);
            result.push_back(hexDigits[character & 0xF]);
            continue;
        }
        result.push_back(character);
    }
    return result;
}

static bool WriteAll(HANDLE file, const std::string& data) {
    size_t offset = 0;
    while (offset < data.size()) {
        const DWORD chunk = static_cast<DWORD>(
            std::min<size_t>(data.size() - offset, 1u << 20));
        DWORD written = 0;
        if (!WriteFile(file, data.data() + offset, chunk, &written, nullptr) ||
            written == 0) {
            return false;
        }
        offset += written;
    }
    return true;
}

static bool ReadWholeFile(const std::wstring& path, std::string& out) {
    yoko::win32::UniqueHandle file(CreateFileW(
        path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file.valid()) {
        return false;
    }
    LARGE_INTEGER size = {};
    if (!GetFileSizeEx(file.get(), &size) || size.QuadPart <= 0 ||
        size.QuadPart > (16 << 20)) {
        return false;
    }
    out.assign(static_cast<size_t>(size.QuadPart), '\0');
    size_t offset = 0;
    while (offset < out.size()) {
        const DWORD chunk = static_cast<DWORD>(
            std::min<size_t>(out.size() - offset, 1u << 20));
        DWORD read = 0;
        if (!ReadFile(file.get(), out.data() + offset, chunk, &read, nullptr) ||
            read == 0) {
            return false;
        }
        offset += read;
    }
    return true;
}

static bool IsLayoutWorldValid(const RectF& world) {
    if (!std::isfinite(world.left) || !std::isfinite(world.top) ||
        !std::isfinite(world.right) || !std::isfinite(world.bottom)) {
        return false;
    }
    return std::isfinite(world.width()) && std::isfinite(world.height()) &&
           world.width() > 0.0f && world.height() > 0.0f;
}

static bool FormatLayoutNumbers(const RectF& world, std::string& output) {
    if (!IsLayoutWorldValid(world)) {
        return false;
    }
    _locale_t locale = _create_locale(LC_NUMERIC, "C");
    if (locale == nullptr) {
        return false;
    }
    char numbers[192] = {};
    const int written = _snprintf_l(
        numbers, sizeof(numbers), "\"x\":%.9g,\"y\":%.9g,\"w\":%.9g,\"h\":%.9g",
        locale, static_cast<double>(world.left),
        static_cast<double>(world.top), static_cast<double>(world.width()),
        static_cast<double>(world.height()));
    _free_locale(locale);
    if (written < 0 || static_cast<size_t>(written) >= sizeof(numbers)) {
        return false;
    }
    output.assign(numbers, static_cast<size_t>(written));
    return true;
}

static std::string SerializeLayout(const LayoutSnapshot& layout) {
    std::string output = "{\"version\":1,\"entries\":[";
    bool first = true;
    for (const LayoutEntry& entry : layout.entries) {
        if (entry.fingerprint.empty()) {
            continue;
        }
        std::string numbers;
        if (!FormatLayoutNumbers(entry.world, numbers)) {
            return std::string();
        }
        if (!first) {
            output += ",";
        }
        first = false;
        output += "{\"key\":\"";
        output += ToUtf8(EscapeJsonText(entry.fingerprint));
        output += "\",";
        output += numbers;
        if (entry.pinned) {
            output += ",\"pin\":1";
        }
        output += "}";
    }
    output += "]}\n";
    return output;
}

static bool IsJsonDigit(char character) {
    return character >= '0' && character <= '9';
}

static constexpr int kMaxJsonDepth = 64;

static void SkipJsonSpace(const std::string& text, size_t& index) {
    while (index < text.size() && (text[index] == ' ' || text[index] == '\t' ||
                                   text[index] == '\n' || text[index] == '\r')) {
        ++index;
    }
}

static bool ReadJsonHexQuad(const std::string& text, size_t& index,
                            uint32_t& value) {
    if (index + 4 > text.size()) {
        return false;
    }
    value = 0;
    for (int digitIndex = 0; digitIndex < 4; ++digitIndex) {
        const char character = text[index];
        uint32_t digit = 0;
        if (character >= '0' && character <= '9') {
            digit = static_cast<uint32_t>(character - '0');
        } else if (character >= 'a' && character <= 'f') {
            digit = static_cast<uint32_t>(character - 'a' + 10);
        } else if (character >= 'A' && character <= 'F') {
            digit = static_cast<uint32_t>(character - 'A' + 10);
        } else {
            return false;
        }
        value = value * 16 + digit;
        ++index;
    }
    return true;
}

static bool AppendJsonCodePoint(std::string& output, uint32_t codePoint) {
    if (codePoint <= 0x7F) {
        output.push_back(static_cast<char>(codePoint));
        return true;
    }
    if (codePoint <= 0x7FF) {
        output.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        return true;
    }
    if (codePoint >= 0xD800 && codePoint <= 0xDFFF) {
        return false;
    }
    if (codePoint <= 0xFFFF) {
        output.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
        output.push_back(
            static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        return true;
    }
    if (codePoint > 0x10FFFF) {
        return false;
    }
    output.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
    output.push_back(
        static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
    output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
    output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    return true;
}

static bool ReadJsonUnicodeEscape(const std::string& text, size_t& index,
                                  std::string& output) {
    uint32_t codePoint = 0;
    if (!ReadJsonHexQuad(text, index, codePoint)) {
        return false;
    }
    if (codePoint >= 0xD800 && codePoint <= 0xDBFF) {
        if (index + 2 > text.size() || text[index] != '\\' ||
            text[index + 1] != 'u') {
            return false;
        }
        index += 2;
        uint32_t lowSurrogate = 0;
        if (!ReadJsonHexQuad(text, index, lowSurrogate)) {
            return false;
        }
        if (lowSurrogate < 0xDC00 || lowSurrogate > 0xDFFF) {
            return false;
        }
        codePoint = 0x10000 + ((codePoint - 0xD800) << 10) +
                    (lowSurrogate - 0xDC00);
    } else if (codePoint >= 0xDC00 && codePoint <= 0xDFFF) {
        return false;
    }
    return AppendJsonCodePoint(output, codePoint);
}

static bool ReadJsonEscape(const std::string& text, size_t& index,
                           std::string& output) {
    if (index >= text.size()) {
        return false;
    }
    const char escape = text[index];
    ++index;
    switch (escape) {
        case '"':
            output.push_back('"');
            return true;
        case '\\':
            output.push_back('\\');
            return true;
        case '/':
            output.push_back('/');
            return true;
        case 'b':
            output.push_back('\b');
            return true;
        case 'f':
            output.push_back('\f');
            return true;
        case 'n':
            output.push_back('\n');
            return true;
        case 'r':
            output.push_back('\r');
            return true;
        case 't':
            output.push_back('\t');
            return true;
        case 'u':
            return ReadJsonUnicodeEscape(text, index, output);
        default:
            return false;
    }
}

static bool ReadJsonString(const std::string& text, size_t& index,
                           std::string& output) {
    if (index >= text.size() || text[index] != '"') {
        return false;
    }
    ++index;
    output.clear();
    while (index < text.size() && text[index] != '"') {
        const char character = text[index];
        const bool escaped = character == '\\';
        if (escaped) {
            ++index;
        }
        if (escaped && !ReadJsonEscape(text, index, output)) {
            return false;
        }
        if (escaped) {
            continue;
        }
        ++index;
        if (static_cast<unsigned char>(character) < 0x20) {
            return false;
        }
        output.push_back(character);
    }
    if (index >= text.size()) {
        return false;
    }
    ++index;
    return true;
}

static bool ReadJsonDigits(const std::string& text, size_t& index) {
    const size_t start = index;
    while (index < text.size() && IsJsonDigit(text[index])) {
        ++index;
    }
    return index > start;
}

static bool ReadJsonInteger(const std::string& text, size_t& index) {
    if (index < text.size() && text[index] == '-') {
        ++index;
    }
    if (index >= text.size()) {
        return false;
    }
    if (text[index] != '0') {
        return ReadJsonDigits(text, index);
    }
    ++index;
    return true;
}

static bool ReadJsonFraction(const std::string& text, size_t& index) {
    if (index < text.size() && text[index] == '.') {
        ++index;
        return ReadJsonDigits(text, index);
    }
    return true;
}

static bool ReadJsonExponent(const std::string& text, size_t& index) {
    if (index >= text.size() ||
        (text[index] != 'e' && text[index] != 'E')) {
        return true;
    }
    ++index;
    if (index < text.size() &&
        (text[index] == '+' || text[index] == '-')) {
        ++index;
    }
    return ReadJsonDigits(text, index);
}

static bool ReadJsonNumber(const std::string& text, size_t& index,
                           double& value) {
    const size_t start = index;
    if (!ReadJsonInteger(text, index) || !ReadJsonFraction(text, index) ||
        !ReadJsonExponent(text, index)) {
        return false;
    }
    _locale_t locale = _create_locale(LC_NUMERIC, "C");
    if (locale == nullptr) {
        return false;
    }
    errno = 0;
    char* end = nullptr;
    const double parsed = _strtod_l(text.c_str() + start, &end, locale);
    const bool valid = end == text.c_str() + index && errno != ERANGE &&
                       std::isfinite(parsed);
    _free_locale(locale);
    if (!valid) {
        return false;
    }
    value = parsed;
    return true;
}

static bool ConsumeJsonLiteral(const std::string& text, size_t& index,
                               const char* literal) {
    for (size_t offset = 0; literal[offset] != '\0'; ++offset) {
        if (index >= text.size() || text[index] != literal[offset]) {
            return false;
        }
        ++index;
    }
    return true;
}

static bool SkipJsonValue(const std::string& text, size_t& index, int depth);

template <typename FieldReader>
static bool ReadJsonObject(const std::string& text, size_t& index,
                           FieldReader readField) {
    SkipJsonSpace(text, index);
    if (index >= text.size() || text[index] != '{') {
        return false;
    }
    ++index;
    SkipJsonSpace(text, index);
    if (index < text.size() && text[index] == '}') {
        ++index;
        return true;
    }
    while (true) {
        std::string key;
        if (!ReadJsonString(text, index, key)) {
            return false;
        }
        SkipJsonSpace(text, index);
        if (index >= text.size() || text[index] != ':') {
            return false;
        }
        ++index;
        SkipJsonSpace(text, index);
        if (!readField(key)) {
            return false;
        }
        SkipJsonSpace(text, index);
        if (index >= text.size()) {
            return false;
        }
        const char delimiter = text[index];
        ++index;
        if (delimiter == '}') {
            return true;
        }
        if (delimiter != ',') {
            return false;
        }
        SkipJsonSpace(text, index);
    }
}

static bool SkipJsonObject(const std::string& text, size_t& index, int depth) {
    if (depth > kMaxJsonDepth) {
        return false;
    }
    return ReadJsonObject(text, index, [&](const std::string&) {
        return SkipJsonValue(text, index, depth + 1);
    });
}

static bool SkipJsonArray(const std::string& text, size_t& index, int depth) {
    if (depth > kMaxJsonDepth || index >= text.size() || text[index] != '[') {
        return false;
    }
    ++index;
    SkipJsonSpace(text, index);
    if (index < text.size() && text[index] == ']') {
        ++index;
        return true;
    }
    while (true) {
        if (!SkipJsonValue(text, index, depth + 1)) {
            return false;
        }
        SkipJsonSpace(text, index);
        if (index >= text.size()) {
            return false;
        }
        const char delimiter = text[index];
        ++index;
        if (delimiter == ']') {
            return true;
        }
        if (delimiter != ',') {
            return false;
        }
        SkipJsonSpace(text, index);
    }
}

static bool SkipJsonValue(const std::string& text, size_t& index, int depth) {
    if (depth > kMaxJsonDepth) {
        return false;
    }
    SkipJsonSpace(text, index);
    if (index >= text.size()) {
        return false;
    }
    const char character = text[index];
    if (character == '"') {
        std::string ignored;
        return ReadJsonString(text, index, ignored);
    }
    if (character == '{') {
        return SkipJsonObject(text, index, depth);
    }
    if (character == '[') {
        return SkipJsonArray(text, index, depth);
    }
    if (character == 't') {
        return ConsumeJsonLiteral(text, index, "true");
    }
    if (character == 'f') {
        return ConsumeJsonLiteral(text, index, "false");
    }
    if (character == 'n') {
        return ConsumeJsonLiteral(text, index, "null");
    }
    double ignored = 0.0;
    return ReadJsonNumber(text, index, ignored);
}

struct JsonEntryFields {
    std::wstring fingerprint;
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
    double pin = 0.0;
    bool hasFingerprint = false;
    bool hasX = false;
    bool hasY = false;
    bool hasWidth = false;
    bool hasHeight = false;
    bool hasPin = false;
};

struct JsonEntryCursor {
    const std::string& text;
    size_t& index;
    JsonEntryFields& fields;
};

static bool ReadJsonEntryNumber(JsonEntryCursor& cursor, bool& assigned,
                                double& value) {
    if (assigned || !ReadJsonNumber(cursor.text, cursor.index, value)) {
        return false;
    }
    assigned = true;
    return true;
}

static bool ReadJsonEntryField(JsonEntryCursor& cursor,
                               const std::string& key) {
    if (key == "key") {
        if (cursor.fields.hasFingerprint) {
            return false;
        }
        std::string encoded;
        if (!ReadJsonString(cursor.text, cursor.index, encoded)) {
            return false;
        }
        cursor.fields.fingerprint = FromUtf8(encoded);
        if (cursor.fields.fingerprint.empty()) {
            return false;
        }
        cursor.fields.hasFingerprint = true;
        return true;
    }
    if (key == "x") {
        return ReadJsonEntryNumber(cursor, cursor.fields.hasX,
                                   cursor.fields.x);
    }
    if (key == "y") {
        return ReadJsonEntryNumber(cursor, cursor.fields.hasY,
                                   cursor.fields.y);
    }
    if (key == "w") {
        return ReadJsonEntryNumber(cursor, cursor.fields.hasWidth,
                                   cursor.fields.width);
    }
    if (key == "h") {
        return ReadJsonEntryNumber(cursor, cursor.fields.hasHeight,
                                   cursor.fields.height);
    }
    if (key == "pin") {
        return ReadJsonEntryNumber(cursor, cursor.fields.hasPin,
                                   cursor.fields.pin);
    }
    return SkipJsonValue(cursor.text, cursor.index, 2);
}

static bool IsLayoutNumberInRange(double value) {
    constexpr double maximum = std::numeric_limits<float>::max();
    return std::isfinite(value) && value >= -maximum && value <= maximum;
}

static bool BuildLayoutEntry(const JsonEntryFields& fields, LayoutEntry& entry) {
    if (!fields.hasFingerprint || !fields.hasX || !fields.hasY ||
        !fields.hasWidth || !fields.hasHeight) {
        return false;
    }
    if (!IsLayoutNumberInRange(fields.x) ||
        !IsLayoutNumberInRange(fields.y) ||
        !IsLayoutNumberInRange(fields.width) ||
        !IsLayoutNumberInRange(fields.height)) {
        return false;
    }
    if (fields.hasPin && fields.pin != 0.0 && fields.pin != 1.0) {
        return false;
    }
    const float left = static_cast<float>(fields.x);
    const float top = static_cast<float>(fields.y);
    const float width = static_cast<float>(fields.width);
    const float height = static_cast<float>(fields.height);
    const float right = left + width;
    const float bottom = top + height;
    const RectF world{left, top, right, bottom};
    if (!IsLayoutWorldValid(world)) {
        return false;
    }
    entry.fingerprint = fields.fingerprint;
    entry.world = world;
    entry.pinned = fields.hasPin && fields.pin == 1.0;
    return true;
}

static bool ReadJsonEntryObject(const std::string& text, size_t& index,
                                LayoutEntry& entry) {
    JsonEntryFields fields;
    JsonEntryCursor cursor{text, index, fields};
    const bool parsed = ReadJsonObject(text, index, [&](const std::string& key) {
        return ReadJsonEntryField(cursor, key);
    });
    if (!parsed) {
        return false;
    }
    return BuildLayoutEntry(fields, entry);
}

static bool ReadJsonEntries(const std::string& text, size_t& index,
                            LayoutSnapshot& layout) {
    SkipJsonSpace(text, index);
    if (index >= text.size() || text[index] != '[') {
        return false;
    }
    ++index;
    SkipJsonSpace(text, index);
    if (index < text.size() && text[index] == ']') {
        ++index;
        return true;
    }
    while (true) {
        LayoutEntry entry;
        if (!ReadJsonEntryObject(text, index, entry)) {
            return false;
        }
        if (layout.entries.size() <
            static_cast<size_t>(kMaxLayoutEntries)) {
            layout.entries.push_back(std::move(entry));
        }
        SkipJsonSpace(text, index);
        if (index >= text.size()) {
            return false;
        }
        const char delimiter = text[index];
        ++index;
        if (delimiter == ']') {
            return true;
        }
        if (delimiter != ',') {
            return false;
        }
        SkipJsonSpace(text, index);
    }
}

struct JsonRootFields {
    LayoutSnapshot& layout;
    bool hasVersion = false;
    bool hasEntries = false;
    double version = 0.0;
};

struct JsonRootCursor {
    const std::string& text;
    size_t& index;
    JsonRootFields& fields;
};

static bool ReadJsonRootField(JsonRootCursor& cursor,
                              const std::string& key) {
    if (key == "version") {
        if (cursor.fields.hasVersion ||
            !ReadJsonNumber(cursor.text, cursor.index,
                            cursor.fields.version)) {
            return false;
        }
        cursor.fields.hasVersion = true;
        return true;
    }
    if (key == "entries") {
        if (cursor.fields.hasEntries ||
            !ReadJsonEntries(cursor.text, cursor.index,
                             cursor.fields.layout)) {
            return false;
        }
        cursor.fields.hasEntries = true;
        return true;
    }
    return SkipJsonValue(cursor.text, cursor.index, 1);
}

static bool ParseLayoutDocument(const std::string& text,
                                LayoutSnapshot& layout) {
    if (FromUtf8(text).empty()) {
        return false;
    }
    size_t index = 0;
    JsonRootFields fields{layout};
    JsonRootCursor cursor{text, index, fields};
    const bool parsed = ReadJsonObject(text, index, [&](const std::string& key) {
        return ReadJsonRootField(cursor, key);
    });
    if (!parsed) {
        return false;
    }
    SkipJsonSpace(text, index);
    return index == text.size() && fields.hasVersion &&
           fields.hasEntries && fields.version == 1.0;
}

static LayoutSnapshot ParseLayout(const std::string& text) {
    LayoutSnapshot layout;
    if (!ParseLayoutDocument(text, layout)) {
        return LayoutSnapshot{};
    }
    return layout;
}

static void LoadLayout() {
    context.layout.entries.clear();
    if (!context.settings.saveLayout) {
        return;
    }
    const std::wstring path = LayoutFilePath();
    if (path.empty()) {
        return;
    }
    std::string text;
    if (!ReadWholeFile(path, text)) {
        return;
    }
    context.layout = ParseLayout(text);
}

static bool EnsureLayoutDirectory(const std::wstring& directory) {
    if (directory.empty()) {
        return false;
    }
    const int result =
        SHCreateDirectoryExW(nullptr, directory.c_str(), nullptr);
    if (result != ERROR_SUCCESS && result != ERROR_ALREADY_EXISTS) {
        SetLastError(static_cast<DWORD>(result));
        return false;
    }
    const DWORD attributes = GetFileAttributesW(directory.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        SetLastError(ERROR_DIRECTORY);
        return false;
    }
    return true;
}

static bool SaveLayoutNow() {
    if (!context.settings.saveLayout) {
        return false;
    }
    const std::wstring path = LayoutFilePath();
    if (path.empty()) {
        return false;
    }
    const std::wstring directory = LayoutDirectory();
    if (directory.empty()) {
        return false;
    }
    if (!EnsureLayoutDirectory(directory)) {
        Wh_Log(L"Aleph: cannot create %s: %lu", directory.c_str(),
               GetLastError());
        return false;
    }

    const std::string data = SerializeLayout(context.layout);
    if (data.empty()) {
        return false;
    }
    const std::wstring temporaryPath = path + L".tmp";
    {
        yoko::win32::UniqueHandle file(CreateFileW(
            temporaryPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL, nullptr));
        if (!file.valid()) {
            Wh_Log(L"Aleph: cannot create %s", temporaryPath.c_str());
            return false;
        }
        if (!WriteAll(file.get(), data) || !FlushFileBuffers(file.get())) {
            Wh_Log(L"Aleph: cannot write %s", temporaryPath.c_str());
            return false;
        }
    }
    if (!MoveFileExW(temporaryPath.c_str(), path.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        Wh_Log(L"Aleph: cannot replace %s: %lu", path.c_str(), GetLastError());
        return false;
    }
    return true;
}

static void MarkLayoutDirty(DWORD now) {
    if (!context.settings.saveLayout) {
        return;
    }
    context.layoutDirty = true;
    context.layoutDirtyTick = now;
}

static void FlushLayoutIfDue(DWORD now) {
    if (!context.layoutDirty) {
        return;
    }
    if (!context.settings.saveLayout) {
        context.layoutDirty = false;
        return;
    }
    if (now - context.layoutDirtyTick < kLayoutSaveDelayMs) {
        return;
    }
    if (SaveLayoutNow()) {
        context.layoutDirty = false;
        return;
    }
    context.layoutDirtyTick = now;
}

// Cross-version top-level window enumeration and filtering.


static bool IsOwnProcessWindow(HWND window) {
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    return pid != 0 && pid == GetCurrentProcessId();
}

static std::wstring ReadWindowClass(HWND window) {
    WCHAR buffer[256] = {};
    const int length =
        GetClassNameW(window, buffer, static_cast<int>(ARRAYSIZE(buffer)));
    if (length <= 0) {
        return std::wstring();
    }
    return ToLowerText(std::wstring(buffer, static_cast<size_t>(length)));
}

static std::wstring ReadWindowText(HWND window) {
    const int length = GetWindowTextLengthW(window);
    if (length <= 0 || length > 1024) {
        return std::wstring();
    }
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    const int copied =
        GetWindowTextW(window, text.data(), static_cast<int>(text.size()));
    if (copied <= 0) {
        return std::wstring();
    }
    text.resize(static_cast<size_t>(copied));
    return text;
}

static std::wstring ReadProcessImagePath(DWORD pid) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (process == nullptr) {
        return std::wstring();
    }
    WCHAR buffer[MAX_PATH * 2] = {};
    DWORD size = static_cast<DWORD>(ARRAYSIZE(buffer));
    const BOOL success = QueryFullProcessImageNameW(process, 0, buffer, &size);
    CloseHandle(process);
    if (!success || size == 0) {
        return std::wstring();
    }
    return std::wstring(buffer, size);
}

static bool IsShellWindowClass(const std::wstring& windowClass) {
    static const wchar_t* const kShellClasses[] = {
        L"progman",
        L"workerw",
        L"shell_traywnd",
        L"shell_secondarytraywnd",
        L"traynotifywnd",
        L"windows.ui.core.corewindow",
        L"startmenuwindow",
        L"searchflyout",
        L"xamlexplorerhostislandwindow",
        L"windows.ui.composition.desktopwindowcontentbridge",
    };
    for (const wchar_t* name : kShellClasses) {
        if (windowClass == name) {
            return true;
        }
    }
    return false;
}

static bool ReadCloaked(HWND window, DWORD& cloaked) {
    cloaked = 0;
    const HRESULT result = DwmGetWindowAttribute(
        window, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
    return SUCCEEDED(result);
}

// DWMWA_EXTENDED_FRAME_BOUNDS excludes the invisible resize border, so cards
// line up with what the user sees instead of leaving a transparent gap.
static bool ReadFrameBounds(HWND window, RECT& bounds) {
    RECT extended = {};
    if (SUCCEEDED(DwmGetWindowAttribute(
            window, DWMWA_EXTENDED_FRAME_BOUNDS, &extended, sizeof(extended))) &&
        extended.right > extended.left && extended.bottom > extended.top) {
        bounds = extended;
        return true;
    }
    RECT fallback = {};
    if (!GetWindowRect(window, &fallback)) {
        return false;
    }
    bounds = fallback;
    return bounds.right > bounds.left && bounds.bottom > bounds.top;
}

static bool ReadRestoredBounds(HWND window, RECT& bounds) {
    WINDOWPLACEMENT placement = {};
    placement.length = sizeof(placement);
    if (!GetWindowPlacement(window, &placement)) {
        return false;
    }
    bounds = placement.rcNormalPosition;
    if (bounds.right <= bounds.left || bounds.bottom <= bounds.top) {
        return false;
    }
    if ((GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) != 0) {
        return true;
    }
    MONITORINFO monitor = {};
    monitor.cbSize = sizeof(monitor);
    if (!GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST),
                          &monitor)) {
        return false;
    }
    OffsetRect(&bounds, monitor.rcWork.left - monitor.rcMonitor.left,
               monitor.rcWork.top - monitor.rcMonitor.top);
    return true;
}

static bool IsOtherDesktop(HWND window, IVirtualDesktopManager* manager) {
    BOOL current = TRUE;
    return manager != nullptr &&
           SUCCEEDED(manager->IsWindowOnCurrentVirtualDesktop(window, &current)) &&
           current == FALSE;
}

static bool IsExcludedExecutable(const std::wstring& exeName) {
    for (const std::wstring& entry : context.settings.excludeList) {
        if (entry == exeName) {
            return true;
        }
    }
    return false;
}

// Returns false when the window must not appear on the canvas. A true result
// with CrossDesktop reports a window on another virtual desktop, which the
// caller may drop based on settings.
static bool ClassifyWindow(HWND window, IVirtualDesktopManager* desktopManager,
                           ScannedWindow& out) {
    if (window == nullptr || IsOwnProcessWindow(window)) {
        return false;
    }
    if (GetShellWindow() == window) {
        return false;
    }
    if (!IsWindowVisible(window)) {
        return false;
    }

    const LONG_PTR extendedStyle = GetWindowLongPtrW(window, GWL_EXSTYLE);
    const bool appWindow = (extendedStyle & WS_EX_APPWINDOW) != 0;
    if ((extendedStyle & WS_EX_TOOLWINDOW) != 0 && !appWindow) {
        return false;
    }
    if (GetWindow(window, GW_OWNER) != nullptr && !appWindow) {
        return false;
    }

    const std::wstring windowClass = ReadWindowClass(window);
    if (windowClass.empty() || IsShellWindowClass(windowClass)) {
        return false;
    }
    const std::wstring title = ReadWindowText(window);
    if (title.empty()) {
        return false;
    }

    DWORD cloaked = 0;
    if (!ReadCloaked(window, cloaked) ||
        (cloaked & ~static_cast<DWORD>(DWM_CLOAKED_SHELL)) != 0) {
        return false;
    }
    const bool crossDesktop = (cloaked & DWM_CLOAKED_SHELL) != 0;
    if (crossDesktop && !IsOtherDesktop(window, desktopManager)) {
        return false;
    }

    DWORD pid = 0;
    const DWORD threadId = GetWindowThreadProcessId(window, &pid);
    if (pid == 0 || pid == GetCurrentProcessId()) {
        return false;
    }
    const std::wstring exePath = ReadProcessImagePath(pid);
    const std::wstring exeName = ExecutableName(exePath);
    if (exeName.empty() || IsExcludedExecutable(exeName)) {
        return false;
    }

    const bool minimized = IsIconic(window) != FALSE;
    RECT frame = {};
    if (minimized) {
        if (!ReadRestoredBounds(window, frame)) {
            return false;
        }
    } else if (!ReadFrameBounds(window, frame)) {
        return false;
    }

    out.hwnd = window;
    out.processId = pid;
    out.threadId = threadId;
    out.windowClass = windowClass;
    out.state = crossDesktop ? WindowVisualState::OtherDesktop
                            : (minimized ? WindowVisualState::Minimized
                                         : WindowVisualState::Live);
    out.framePx = frame;
    out.title = title;
    out.exePath = exePath;
    out.fingerprint.clear();
    return true;
}

// Enumerates every eligible window. The virtual desktop manager is passed as
// a parameter so the callback never needs global COM state.
struct CollectPass {
    std::vector<ScannedWindow> windows;
    IVirtualDesktopManager* desktopManager = nullptr;

    static BOOL CALLBACK Thunk(HWND window, LPARAM parameter) {
        CollectPass* pass = reinterpret_cast<CollectPass*>(parameter);
        if (pass == nullptr || pass->windows.size() >=
                                  static_cast<size_t>(kMaxCards) * 4) {
            return TRUE;
        }
        ScannedWindow candidate;
        if (!ClassifyWindow(window, pass->desktopManager, candidate)) {
            return TRUE;
        }
        if (candidate.state == WindowVisualState::OtherDesktop &&
            !context.settings.includeOtherDesktops) {
            return TRUE;
        }
        pass->windows.push_back(std::move(candidate));
        return TRUE;
    }
};

static std::wstring ExistingFingerprint(const ScannedWindow& window) {
    for (const Card& card : context.cards) {
        if (MatchesWindow(card, window)) {
            return card.fingerprint;
        }
    }
    return {};
}

static bool FingerprintClaimed(const std::vector<ScannedWindow>& windows,
                               const std::wstring& fingerprint) {
    return std::any_of(windows.begin(), windows.end(),
                       [&fingerprint](const ScannedWindow& window) {
                           return window.fingerprint == fingerprint;
                       });
}

static void AssignFingerprints(std::vector<ScannedWindow>& windows) {
    for (ScannedWindow& window : windows) {
        window.fingerprint = ExistingFingerprint(window);
    }
    for (ScannedWindow& window : windows) {
        if (!window.fingerprint.empty()) {
            continue;
        }
        const std::wstring base =
            ExecutableName(window.exePath) + L"|" + window.windowClass + L"#";
        unsigned int ordinal = 0;
        while (FingerprintClaimed(windows, base + std::to_wstring(ordinal))) {
            ++ordinal;
        }
        window.fingerprint = base + std::to_wstring(ordinal);
    }
}

static std::vector<ScannedWindow> ScanWindows(IVirtualDesktopManager* manager) {
    CollectPass pass;
    pass.desktopManager = manager;
    EnumWindows(&CollectPass::Thunk, reinterpret_cast<LPARAM>(&pass));
    if (pass.windows.size() > static_cast<size_t>(kMaxCards)) {
        pass.windows.resize(static_cast<size_t>(kMaxCards));
    }
    AssignFingerprints(pass.windows);
    return std::move(pass.windows);
}

static const LayoutEntry* FindLayoutEntry(const std::wstring& fingerprint) {
    for (const LayoutEntry& entry : context.layout.entries) {
        if (entry.fingerprint == fingerprint) {
            return &entry;
        }
    }
    return nullptr;
}

static void RememberLayoutEntry(const std::wstring& fingerprint,
                                const RectF& world) {
    for (LayoutEntry& entry : context.layout.entries) {
        if (entry.fingerprint == fingerprint) {
            entry.world = world;
            return;
        }
    }
    if (context.layout.entries.size() >=
        static_cast<size_t>(kMaxLayoutEntries)) {
        return;
    }
    context.layout.entries.push_back(LayoutEntry{fingerprint, world});
}

static void RememberLayoutPin(const std::wstring& fingerprint,
                              const RectF& world, bool pinned) {
    for (LayoutEntry& entry : context.layout.entries) {
        if (entry.fingerprint == fingerprint) {
            entry.world = world;
            entry.pinned = pinned;
            return;
        }
    }
    if (context.layout.entries.size() >=
        static_cast<size_t>(kMaxLayoutEntries)) {
        return;
    }
    context.layout.entries.push_back(LayoutEntry{fingerprint, world, pinned});
}

// Card creation, packing and snapping.
//
// Arranging cards never moves a real window. A card keeps its own position in
// world space, and dive-in solves for the camera state that projects the card
// back onto the window's real desktop rectangle. That keeps the user's desktop
// arrangement untouched and works for elevated windows the mod cannot move.
// The one deliberate move lives in interaction.h: activating an other-desktop
// card pulls that window onto the desktop that displays the canvas.


inline int VirtualScreenWidth() {
    return context.virtualScreen.right - context.virtualScreen.left;
}

inline int VirtualScreenHeight() {
    return context.virtualScreen.bottom - context.virtualScreen.top;
}

// Describes a desktop rectangle in canvas world units. World units are
// virtual-screen physical pixels measured from the top-left corner of the
// virtual screen, matching the projection in math_utils.h.
inline RectF WorldFromFrame(const RECT& frame) {
    return RectF{
        static_cast<float>(frame.left - context.virtualScreen.left),
        static_cast<float>(frame.top - context.virtualScreen.top),
        static_cast<float>(frame.right - context.virtualScreen.left),
        static_cast<float>(frame.bottom - context.virtualScreen.top)};
}

inline bool UpdateChromeDpi() {
    POINT cursor = {};
    if (!GetCursorPos(&cursor)) {
        return false;
    }
    const HMONITOR monitor =
        MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    context.chromeMonitor = monitor;
    UINT dpiX = 96;
    UINT dpiY = 96;
    if (SUCCEEDED(GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY)) &&
        dpiX != 0) {
        const bool changed = context.chromeDpi != static_cast<int>(dpiX);
        context.chromeDpi = static_cast<int>(dpiX);
        return changed;
    }
    return false;
}

inline bool CardOverlapsAny(const RectF& candidate, float gap,
                            int ignoreIndex) {
    const RectF padded = GrowRect(candidate, gap);
    for (size_t index = 0; index < context.cards.size(); ++index) {
        if (static_cast<int>(index) == ignoreIndex) {
            continue;
        }
        if (RectsOverlap(padded, context.cards[index].world)) {
            return true;
        }
    }
    return false;
}

// Spiral shelf search around the world origin: the first ring tries the
// right/bottom ring, then grows outward. Step size follows the candidate so
// large windows never need a long linear scan.
inline POINT PackingOffset(int ring, int step) {
    if (ring == 0) {
        return POINT{0, 0};
    }
    const int edgeLength = ring * 2;
    const int side = step / edgeLength;
    const int offset = step % edgeLength;
    switch (side) {
        case 0: return POINT{-ring + offset, -ring};
        case 1: return POINT{ring, -ring + offset};
        case 2: return POINT{ring - offset, ring};
        default: return POINT{-ring, ring - offset};
    }
}

inline RectF TryPackingRing(int ring, Vec2 size) {
    const float gap = static_cast<float>(context.settings.gap);
    const float stepX = std::max(size.x + gap, 64.0f);
    const float stepY = std::max(size.y + gap, 64.0f);
    const int count = std::max(1, ring * 8);
    for (int step = 0; step < count; ++step) {
        const POINT offset = PackingOffset(ring, step);
        const RectF candidate = MakeRectF(static_cast<float>(offset.x) * stepX,
                                         static_cast<float>(offset.y) * stepY,
                                         size.x, size.y);
        if (!CardOverlapsAny(candidate, gap, -1)) {
            return candidate;
        }
    }
    return RectF{};
}

inline RectF FindFreeSpot(Vec2 size) {
    for (int ring = 0; ring < 48; ++ring) {
        const RectF candidate = TryPackingRing(ring, size);
        if (!candidate.empty()) {
            return candidate;
        }
    }
    float right = 0.0f;
    for (const Card& card : context.cards) {
        right = std::max(right, card.world.right);
    }
    return MakeRectF(right + static_cast<float>(context.settings.gap), 0.0f,
                     size.x, size.y);
}

inline void InsertCard(ScannedWindow&& window, RectF world) {
    Card card;
    card.hwnd = window.hwnd;
    card.processId = window.processId;
    card.threadId = window.threadId;
    card.windowClass = std::move(window.windowClass);
    card.state = window.state;
    card.framePx = window.framePx;
    card.world = world;
    card.title = std::move(window.title);
    card.fingerprint = std::move(window.fingerprint);
    card.exePath = std::move(window.exePath);
    context.cards.push_back(std::move(card));
}

inline int FindCardIndex(const ScannedWindow& window) {
    for (size_t index = 0; index < context.cards.size(); ++index) {
        if (MatchesWindow(context.cards[index], window)) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

inline RectF InitialWorldFor(const ScannedWindow& window) {
    if (context.settings.initialLayout == InitialLayout::Desktop) {
        return WorldFromFrame(window.framePx);
    }
    return FindFreeSpot(Vec2{static_cast<float>(window.framePx.right -
                                               window.framePx.left),
                             static_cast<float>(window.framePx.bottom -
                                               window.framePx.top)});
}

inline void RemoveCardAt(size_t index) {
    if (index >= context.cards.size()) {
        return;
    }
    context.cards[index].thumbnail.reset();
    context.cards.erase(context.cards.begin() +
                        static_cast<long long>(index));
    if (context.selectedCard == static_cast<int>(index)) {
        context.selectedCard = -1;
    } else if (context.selectedCard > static_cast<int>(index)) {
        --context.selectedCard;
    }
    if (context.hoverCard == static_cast<int>(index)) {
        context.hoverCard = -1;
    } else if (context.hoverCard > static_cast<int>(index)) {
        --context.hoverCard;
    }
    if (context.draggedCard == static_cast<int>(index)) {
        context.draggedCard = -1;
    } else if (context.draggedCard > static_cast<int>(index)) {
        --context.draggedCard;
    }
}

inline void ClearCards() {
    for (Card& card : context.cards) {
        card.thumbnail.reset();
    }
    context.cards.clear();
    context.hoverCard = -1;
    context.selectedCard = -1;
    context.draggedCard = -1;
}

// Applies one scan result to an existing card. The canvas position is owned by
// the canvas, so a window that moved on the desktop never drags its card
// along; only the descriptive fields follow the window.
inline bool UpdateCardFromScan(size_t index, const ScannedWindow& window) {
    Card& card = context.cards[index];
    const bool changed = card.state != window.state || card.title != window.title ||
                         !EqualRect(&card.framePx, &window.framePx);
    if (card.state != window.state) {
        card.thumbnail.reset();
    }
    const float width = static_cast<float>(window.framePx.right - window.framePx.left);
    const float height = static_cast<float>(window.framePx.bottom - window.framePx.top);
    card.world.right = card.world.left + width;
    card.world.bottom = card.world.top + height;
    card.state = window.state;
    card.framePx = window.framePx;
    card.title = window.title;
    card.exePath = window.exePath;
    if (const LayoutEntry* saved = FindLayoutEntry(window.fingerprint);
        saved != nullptr) {
        card.pinned = saved->pinned;
    }
    return changed;
}

inline float CardArea(const ScannedWindow& window) {
    return static_cast<float>(window.framePx.right - window.framePx.left) *
           static_cast<float>(window.framePx.bottom - window.framePx.top);
}

// Fewer cards is the first limit to hit, so drop the least useful candidates
// instead of truncating the Z-ordered list.
inline void SortForPacking(std::vector<ScannedWindow>& windows) {
    std::stable_sort(windows.begin(), windows.end(),
                     [](const ScannedWindow& left, const ScannedWindow& right) {
                         return CardArea(left) > CardArea(right);
                     });
}

// Merges one scan into the card vector. Existing cards keep their canvas
// position and thumbnail. New cards take a saved position when the fingerprint
// matches, otherwise they are packed largest-first so wide windows claim the
// centre ring before small dialogs fill the gaps.
inline bool MergeScan(const std::vector<ScannedWindow>& scanned) {
    std::vector<bool> seen(context.cards.size(), false);
    std::vector<ScannedWindow> additions;
    bool changed = false;
    for (const ScannedWindow& window : scanned) {
        if (context.dismissedFingerprints.count(window.fingerprint) != 0) {
            continue;
        }
        const int existing = FindCardIndex(window);
        if (existing >= 0) {
            seen[static_cast<size_t>(existing)] = true;
            changed = UpdateCardFromScan(static_cast<size_t>(existing), window) || changed;
            continue;
        }
        additions.push_back(window);
    }

    for (size_t index = context.cards.size(); index-- > 0;) {
        if (index < seen.size() && seen[static_cast<size_t>(index)]) {
            continue;
        }
        RemoveCardAt(index);
        changed = true;
    }

    if (!additions.empty()) {
        SortForPacking(additions);
        const DWORD now = GetTickCount();
        for (const ScannedWindow& window : additions) {
            const LayoutEntry* saved = FindLayoutEntry(window.fingerprint);
            RectF world = saved != nullptr ? saved->world : InitialWorldFor(window);
            world.right = world.left + static_cast<float>(window.framePx.right - window.framePx.left);
            world.bottom = world.top + static_cast<float>(window.framePx.bottom - window.framePx.top);
            InsertCard(ScannedWindow{window}, world);
            if (saved != nullptr) {
                context.cards.back().pinned = saved->pinned;
            }
            RememberLayoutEntry(window.fingerprint, world);
            changed = true;
        }
        if (changed) {
            MarkLayoutDirty(now);
        }
    }
    return changed;
}

struct SnapResult {
    float left = 0.0f;
    float top = 0.0f;
    bool snapVertical = false;
    bool snapHorizontal = false;
    float guideX = 0.0f;
    float guideY = 0.0f;
};

struct SnapAxis {
    float distance;
    float shift;
    float guide = 0.0f;
    bool snapped = false;
};

inline void ConsiderSnap(float value, float target, SnapAxis& axis) {
    const float delta = target - value;
    if (std::fabs(delta) > axis.distance ||
        std::fabs(delta) >= std::fabs(axis.shift)) {
        return;
    }
    axis.shift = delta;
    axis.guide = target;
    axis.snapped = true;
}

inline void SnapEdges(const float (&moving)[3], const float (&other)[3],
                       SnapAxis& axis) {
    for (const float value : moving) {
        for (const float target : other) {
            ConsiderSnap(value, target, axis);
        }
    }
}

inline void SnapAgainstCard(const RectF& moving, const RectF& other,
                            std::pair<SnapAxis, SnapAxis>& axes) {
    const float movingX[3] = {moving.left, moving.centerX(), moving.right};
    const float movingY[3] = {moving.top, moving.centerY(), moving.bottom};
    const float otherX[3] = {other.left, other.centerX(), other.right};
    const float otherY[3] = {other.top, other.centerY(), other.bottom};
    SnapEdges(movingX, otherX, axes.first);
    SnapEdges(movingY, otherY, axes.second);
}

// Magnetic alignment uses the moving card's left, center and right edges
// against the same three edges of every other card.
inline SnapResult ComputeSnap(const RectF& moving, int ignoreIndex) {
    SnapResult result{moving.left, moving.top};
    const float distance =
        static_cast<float>(context.settings.snapDistance) / context.camera.zoom;
    if (distance <= 0.0f) {
        return result;
    }
    std::pair<SnapAxis, SnapAxis> axes{SnapAxis{distance, distance + 1.0f},
                                      SnapAxis{distance, distance + 1.0f}};
    for (size_t index = 0; index < context.cards.size(); ++index) {
        if (static_cast<int>(index) == ignoreIndex) {
            continue;
        }
        SnapAgainstCard(moving, context.cards[index].world, axes);
    }
    result.snapVertical = axes.first.snapped;
    result.snapHorizontal = axes.second.snapped;
    result.guideX = axes.first.guide;
    result.guideY = axes.second.guide;
    result.left += result.snapVertical ? axes.first.shift : 0.0f;
    result.top += result.snapHorizontal ? axes.second.shift : 0.0f;
    return result;
}

// Direct2D, DirectWrite, WIC and shared GDI surfaces.


// Icon source selection for icon cards.
//
// An icon card draws its bitmap at up to kCardIconMaxPx display pixels, so the
// lookup starts at the largest source the shell offers. Asking for a 48 px
// shell icon or the 32 px window icon enlarges the bitmap two to four times and
// the interpolation shows as a soft, blurry icon on a card that has no live
// preview to distract from it.


#include <cstddef>

// Largest first. The shell answers with the requested size or fails, so a
// missing jumbo list on an older image list simply falls through.
inline constexpr int kShellIconListSizes[] = {SHIL_JUMBO, SHIL_EXTRALARGE,
                                              SHIL_LARGE};

// Returns the first icon the callback produces for the candidate sizes, or
// nullptr when every candidate fails. Ownership of a returned icon passes to
// the caller, which must destroy it.
template <size_t Count, typename QueryIcon>
inline HICON QueryFirstAvailableIcon(const int (&sizes)[Count],
                                     QueryIcon queryIcon) {
    for (size_t index = 0; index < Count; ++index) {
        HICON icon = queryIcon(sizes[index]);
        if (icon != nullptr) {
            return icon;
        }
    }
    return nullptr;
}

#include <commoncontrols.h>

inline D2D1_COLOR_F ToColorF(uint32_t rgb, float alpha) {
    return D2D1::ColorF(static_cast<float>((rgb >> 16) & 0xFF) / 255.0f,
                        static_cast<float>((rgb >> 8) & 0xFF) / 255.0f,
                        static_cast<float>(rgb & 0xFF) / 255.0f, alpha);
}

inline HBITMAP CreateHudDib(int width, int height) {
    if (width <= 0 || height <= 0) {
        return nullptr;
    }
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    return CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
}

inline void ReleaseIconCache() {
    for (auto& pair : context.iconCache) {
        if (pair.second != nullptr) {
            pair.second->Release();
        }
    }
    context.iconCache.clear();
    context.iconRetryTick.clear();
}

inline bool ConsumeDueIconRetries(DWORD now) {
    bool due = false;
    auto retry = context.iconRetryTick.begin();
    while (retry != context.iconRetryTick.end()) {
        if (now - retry->second < kIconRetryDelayMs) {
            ++retry;
            continue;
        }
        retry = context.iconRetryTick.erase(retry);
        due = true;
    }
    return due;
}

inline void ReleaseHudSurface() {
    ReleaseIconCache();
    if (context.hudBrush != nullptr) {
        context.hudBrush->Release();
        context.hudBrush = nullptr;
    }
    if (context.hudTarget != nullptr) {
        context.hudTarget->Release();
        context.hudTarget = nullptr;
    }
    if (context.hudDc != nullptr) {
        if (context.hudOriginalBitmap != nullptr) {
            SelectObject(context.hudDc, context.hudOriginalBitmap);
            context.hudOriginalBitmap = nullptr;
        }
        DeleteDC(context.hudDc);
        context.hudDc = nullptr;
    }
    if (context.hudBitmap != nullptr) {
        DeleteObject(context.hudBitmap);
        context.hudBitmap = nullptr;
    }
    context.hudWidth = 0;
    context.hudHeight = 0;
}

inline void ReleaseTextFormats() {
    if (context.titleFormat != nullptr) {
        context.titleFormat->Release();
        context.titleFormat = nullptr;
    }
    if (context.badgeFormat != nullptr) {
        context.badgeFormat->Release();
        context.badgeFormat = nullptr;
    }
}

inline void ReleaseTextResources() {
    ReleaseTextFormats();
    if (context.writeFactory != nullptr) {
        context.writeFactory->Release();
        context.writeFactory = nullptr;
    }
}

inline void ReleaseGraphics() {
    ReleaseHudSurface();
    ReleaseTextResources();
    if (context.wicFactory != nullptr) {
        context.wicFactory->Release();
        context.wicFactory = nullptr;
    }
    if (context.d2dFactory != nullptr) {
        context.d2dFactory->Release();
        context.d2dFactory = nullptr;
    }
}

inline bool CreateTitleFormat() {
    const float size = 13.0f * ChromeScale();
    const HRESULT result = context.writeFactory->CreateTextFormat(
        L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"",
        &context.titleFormat);
    if (FAILED(result)) {
        return false;
    }
    return SUCCEEDED(context.titleFormat->SetWordWrapping(
               DWRITE_WORD_WRAPPING_NO_WRAP)) &&
           SUCCEEDED(context.titleFormat->SetTextAlignment(
               DWRITE_TEXT_ALIGNMENT_LEADING)) &&
           SUCCEEDED(context.titleFormat->SetParagraphAlignment(
               DWRITE_PARAGRAPH_ALIGNMENT_CENTER));
}

inline bool CreateBadgeFormat() {
    const float badgeSize = 11.0f * ChromeScale();
    const HRESULT result = context.writeFactory->CreateTextFormat(
        L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, badgeSize, L"", &context.badgeFormat);
    if (FAILED(result)) {
        return false;
    }
    return SUCCEEDED(context.badgeFormat->SetWordWrapping(
               DWRITE_WORD_WRAPPING_NO_WRAP)) &&
           SUCCEEDED(context.badgeFormat->SetTextAlignment(
               DWRITE_TEXT_ALIGNMENT_TRAILING)) &&
           SUCCEEDED(context.badgeFormat->SetParagraphAlignment(
               DWRITE_PARAGRAPH_ALIGNMENT_CENTER));
}

inline bool CreateTextFormats() {
    ReleaseTextFormats();
    if (context.writeFactory == nullptr) {
        return false;
    }
    if (!CreateTitleFormat() || !CreateBadgeFormat()) {
        ReleaseTextFormats();
        return false;
    }
    return true;
}

inline bool CreateHudBackingStore(int width, int height) {
    context.hudDc = CreateCompatibleDC(nullptr);
    if (context.hudDc == nullptr) {
        return false;
    }
    context.hudBitmap = CreateHudDib(width, height);
    if (context.hudBitmap == nullptr) {
        return false;
    }
    const HGDIOBJ original = SelectObject(context.hudDc, context.hudBitmap);
    if (original == nullptr || original == HGDI_ERROR) {
        return false;
    }
    context.hudOriginalBitmap = original;
    return true;
}

inline bool CreateHudRenderTarget(int width, int height) {
    if (context.d2dFactory == nullptr) {
        return false;
    }
    const D2D1_RENDER_TARGET_PROPERTIES properties =
        D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                              D2D1_ALPHA_MODE_PREMULTIPLIED));
    const HRESULT result = context.d2dFactory->CreateDCRenderTarget(
        &properties, &context.hudTarget);
    if (FAILED(result) || context.hudTarget == nullptr) {
        return false;
    }
    const RECT bounds = {0, 0, width, height};
    if (FAILED(context.hudTarget->BindDC(context.hudDc, &bounds))) {
        return false;
    }
    return SUCCEEDED(context.hudTarget->CreateSolidColorBrush(
               D2D1::ColorF(D2D1::ColorF::White), &context.hudBrush)) &&
           context.hudBrush != nullptr;
}

inline bool CreateHudSurface(int width, int height) {
    if (width <= 0 || height <= 0) {
        return false;
    }
    if (context.hudTarget != nullptr && context.hudDc != nullptr &&
        context.hudBrush != nullptr && context.hudBitmap != nullptr &&
        context.hudWidth >= width && context.hudHeight >= height) {
        return true;
    }
    ReleaseHudSurface();
    if (!CreateHudBackingStore(width, height) ||
        !CreateHudRenderTarget(width, height)) {
        ReleaseHudSurface();
        return false;
    }
    context.hudWidth = width;
    context.hudHeight = height;
    return true;
}

inline bool EnsureGraphics() {
    if (context.d2dFactory == nullptr) {
        if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                     &context.d2dFactory))) {
            Wh_Log(L"Aleph: D2D1CreateFactory failed");
            return false;
        }
    }
    if (context.writeFactory == nullptr) {
        if (FAILED(DWriteCreateFactory(
                DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                reinterpret_cast<IUnknown**>(&context.writeFactory)))) {
            Wh_Log(L"Aleph: DWriteCreateFactory failed");
            return false;
        }
    }
    if (context.wicFactory == nullptr) {
        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                    CLSCTX_INPROC_SERVER,
                                    IID_PPV_ARGS(&context.wicFactory)))) {
            Wh_Log(L"Aleph: WIC factory creation failed");
            return false;
        }
    }
    if ((context.titleFormat == nullptr || context.badgeFormat == nullptr) &&
        !CreateTextFormats()) {
        Wh_Log(L"Aleph: text format creation failed");
        return false;
    }
    return true;
}

inline HICON QueryWindowIcon(HWND window) {
    DWORD_PTR iconResult = 0;
    const LRESULT sent = SendMessageTimeoutW(
        window, WM_GETICON, ICON_BIG, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK, 50,
        &iconResult);
    HICON icon = sent != 0 ? reinterpret_cast<HICON>(iconResult) : nullptr;
    if (icon == nullptr) {
        icon = reinterpret_cast<HICON>(
            GetClassLongPtrW(window, GCLP_HICON));
    }
    if (icon == nullptr) {
        icon = reinterpret_cast<HICON>(
            GetClassLongPtrW(window, GCLP_HICONSM));
    }
    return icon;
}

inline HICON QueryShellIconAtSize(int imageIndex, int listSize) {
    IImageList* images = nullptr;
    const HRESULT queried =
        SHGetImageList(listSize, IID_PPV_ARGS(&images));
    if (FAILED(queried) || images == nullptr) {
        if (images != nullptr) {
            images->Release();
        }
        return nullptr;
    }
    HICON icon = nullptr;
    const HRESULT result = images->GetIcon(imageIndex, ILD_TRANSPARENT, &icon);
    images->Release();
    if (FAILED(result)) {
        if (icon != nullptr) {
            DestroyIcon(icon);
        }
        return nullptr;
    }
    return icon;
}

inline HICON QueryShellIcon(const std::wstring& exePath) {
    if (exePath.empty()) {
        return nullptr;
    }
    SHFILEINFOW info = {};
    if (SHGetFileInfoW(exePath.c_str(), 0, &info, sizeof(info),
                       SHGFI_SYSICONINDEX) == 0) {
        return nullptr;
    }
    return QueryFirstAvailableIcon(
        kShellIconListSizes,
        [&](int listSize) {
            return QueryShellIconAtSize(info.iIcon, listSize);
        });
}

inline ID2D1Bitmap* CreateHudIconBitmap(IWICBitmapSource* bitmap) {
    IWICBitmapSource* converted = nullptr;
    const HRESULT conversion = WICConvertBitmapSource(
        GUID_WICPixelFormat32bppPBGRA, bitmap, &converted);
    if (FAILED(conversion) || converted == nullptr) {
        if (converted != nullptr) {
            converted->Release();
        }
        return nullptr;
    }
    ID2D1Bitmap* result = nullptr;
    const HRESULT created = context.hudTarget->CreateBitmapFromWicBitmap(
        converted, nullptr, &result);
    converted->Release();
    if (FAILED(created)) {
        if (result != nullptr) {
            result->Release();
        }
        return nullptr;
    }
    return result;
}

inline ID2D1Bitmap* CreateIconBitmap(HICON icon) {
    if (icon == nullptr || context.wicFactory == nullptr ||
        context.hudTarget == nullptr) {
        return nullptr;
    }
    IWICBitmap* bitmap = nullptr;
    const HRESULT converted =
        context.wicFactory->CreateBitmapFromHICON(icon, &bitmap);
    if (FAILED(converted) || bitmap == nullptr) {
        if (bitmap != nullptr) {
            bitmap->Release();
        }
        return nullptr;
    }
    ID2D1Bitmap* result = CreateHudIconBitmap(bitmap);
    bitmap->Release();
    return result;
}

inline ID2D1Bitmap* LoadShellIconBitmap(const std::wstring& exePath) {
    HICON icon = QueryShellIcon(exePath);
    if (icon == nullptr) {
        return nullptr;
    }
    ID2D1Bitmap* bitmap = CreateIconBitmap(icon);
    DestroyIcon(icon);
    return bitmap;
}

inline ID2D1Bitmap* LoadIconBitmap(const std::wstring& exePath,
                                   const std::wstring& windowClass,
                                   HWND window) {
    if (context.hudTarget == nullptr || context.wicFactory == nullptr) {
        return nullptr;
    }
    const std::wstring key = exePath + L"|" + windowClass;
    const auto cached = context.iconCache.find(key);
    if (cached != context.iconCache.end() && cached->second != nullptr) {
        return cached->second;
    }
    const auto retry = context.iconRetryTick.find(key);
    if (retry != context.iconRetryTick.end() &&
        GetTickCount() - retry->second < kIconRetryDelayMs) {
        return nullptr;
    }

    // The shell icon is the largest source, so it wins whenever it exists; the
    // window icon covers executables the shell cannot resolve.
    ID2D1Bitmap* result = LoadShellIconBitmap(exePath);
    if (result == nullptr) {
        result = CreateIconBitmap(QueryWindowIcon(window));
    }
    if (result == nullptr) {
        context.iconRetryTick[key] = GetTickCount();
        return nullptr;
    }
    context.iconCache[key] = result;
    context.iconRetryTick.erase(key);
    return result;
}


inline RectF MinimapInnerRect(const RectF& minimap) {
    const float pad = kMinimapPaddingPx * ChromeScale();
    return RectF{minimap.left + pad, minimap.top + pad, minimap.right - pad,
                 minimap.bottom - pad};
}

inline RectF CanvasWorldBounds() {
    RectF bounds{};
    for (const Card& card : context.cards) {
        bounds = UnionRect(bounds, card.world);
    }
    if (bounds.empty()) {
        return RectF{-500.0f, -500.0f, 500.0f, 500.0f};
    }
    const float margin = 100.0f;
    return GrowRect(bounds, margin);
}

inline RectF CalculateMinimapRect() {
    const float scale = ChromeScale();
    const float size = static_cast<float>(context.settings.minimapSize) * scale;
    const float margin = kMinimapMarginPx * scale;
    const float width = std::min(size, static_cast<float>(VirtualScreenWidth()) - margin * 2.0f);
    const float height = std::min(size, static_cast<float>(VirtualScreenHeight()) - margin * 2.0f);
    return MakeRectF(static_cast<float>(VirtualScreenWidth()) - width - margin,
                     static_cast<float>(VirtualScreenHeight()) - height - margin,
                     width, height);
}

inline RectF MinimapWorldToScreen(const RectF& world, const RectF& minimap,
                                 const RectF& bounds) {
    const RectF inner = MinimapInnerRect(minimap);
    const float scale = std::min(inner.width() / bounds.width(),
                                 inner.height() / bounds.height());
    const float offsetX = inner.left + (inner.width() - bounds.width() * scale) * 0.5f;
    const float offsetY = inner.top + (inner.height() - bounds.height() * scale) * 0.5f;
    return RectF{offsetX + (world.left - bounds.left) * scale,
                 offsetY + (world.top - bounds.top) * scale,
                 offsetX + (world.right - bounds.left) * scale,
                 offsetY + (world.bottom - bounds.top) * scale};
}

// Clicking or dragging inside the minimap centres the camera on the world
// point under the pointer.
inline void PanCameraFromMinimap(Vec2 point) {
    const RectF bounds = CanvasWorldBounds();
    const RectF inner = MinimapInnerRect(context.minimapRect);
    if (inner.empty()) {
        return;
    }
    const float scale = std::min(inner.width() / bounds.width(),
                                 inner.height() / bounds.height());
    if (scale <= 0.0f) {
        return;
    }
    const float offsetX = inner.left + (inner.width() - bounds.width() * scale) * 0.5f;
    const float offsetY = inner.top + (inner.height() - bounds.height() * scale) * 0.5f;
    CenterOnPoint(context.camera,
                  Vec2{bounds.left + (point.x - offsetX) / scale,
                       bounds.top + (point.y - offsetY) / scale});
}


inline void FitAllCards() {
    if (context.cards.empty()) {
        CenterOnPoint(context.camera, Vec2{ScreenCenterX(), ScreenCenterY()});
        return;
    }
    RectF bounds{};
    for (const Card& card : context.cards) {
        bounds = UnionRect(bounds, card.world);
    }
    const float width = static_cast<float>(std::max(VirtualScreenWidth(), 1));
    const float height = static_cast<float>(std::max(VirtualScreenHeight(), 1));
    const float zoom = std::min((width * 0.8f) / std::max(bounds.width(), 1.0f),
                                (height * 0.8f) / std::max(bounds.height(), 1.0f));
    context.camera.targetZoom = ClampZoom(zoom);
    CenterOnPoint(context.camera, Vec2{bounds.centerX(), bounds.centerY()});
}

inline void CenterOnCard(int index) {
    if (index < 0 || static_cast<size_t>(index) >= context.cards.size()) {
        return;
    }
    const RectF& world = context.cards[static_cast<size_t>(index)].world;
    CenterOnPoint(context.camera, Vec2{world.centerX(), world.centerY()});
}

inline Camera DiveCameraForCard(const Card& card) {
    Camera target = context.camera;
    target.targetZoom = 1.0f;
    const float frameCenterX =
        static_cast<float>(card.framePx.left - context.virtualScreen.left) +
        static_cast<float>(card.framePx.right - card.framePx.left) * 0.5f;
    const float frameCenterY =
        static_cast<float>(card.framePx.top - context.virtualScreen.top) +
        static_cast<float>(card.framePx.bottom - card.framePx.top) * 0.5f;
    target.targetCenterX = card.world.centerX() - frameCenterX + ScreenCenterX();
    target.targetCenterY = card.world.centerY() - frameCenterY + ScreenCenterY();
    return target;
}


inline void ReleaseThumbnail(Card& card) {
    card.thumbnail.reset();
    card.thumbnailVisible = false;
    card.thumbnailDestination = {};
}

inline void FailThumbnail(Card& card, DWORD now) {
    ReleaseThumbnail(card);
    card.thumbnailFailed = true;
    card.thumbnailRetryTick = now;
}

inline bool HasDueThumbnailRetry(DWORD now) {
    for (const Card& card : context.cards) {
        if (card.state == WindowVisualState::Live && !card.destroyed &&
            card.thumbnailFailed &&
            now - card.thumbnailRetryTick >= kThumbnailRetryDelayMs) {
            return true;
        }
    }
    return false;
}

inline bool EnsureThumbnail(Card& card, DWORD now) {
    if (card.destroyed || card.state != WindowVisualState::Live ||
        IsWindow(card.hwnd) == FALSE) {
        ReleaseThumbnail(card);
        return false;
    }
    if (card.thumbnail.valid()) {
        return true;
    }
    if (card.thumbnailFailed && now - card.thumbnailRetryTick < kThumbnailRetryDelayMs) {
        return false;
    }
    HTHUMBNAIL handle = nullptr;
    const HRESULT result =
        DwmRegisterThumbnail(context.hostWindow, card.hwnd, &handle);
    card.thumbnail.reset(handle);
    if (FAILED(result) || !card.thumbnail.valid()) {
        FailThumbnail(card, now);
        return false;
    }
    card.thumbnailFailed = false;
    return true;
}

struct ThumbnailGeometry {
    RectF source{};
    RectF destination{};
};

inline RECT ThumbnailRect(const RectF& rect) {
    return RECT{static_cast<LONG>(std::lround(rect.left)),
                static_cast<LONG>(std::lround(rect.top)),
                static_cast<LONG>(std::lround(rect.right)),
                static_cast<LONG>(std::lround(rect.bottom))};
}

inline RectF ThumbnailViewportClip(const RectF& destination) {
    return RectF{std::max(0.0f, destination.left),
                 std::max(0.0f, destination.top),
                 std::min(static_cast<float>(VirtualScreenWidth()),
                           destination.right),
                 std::min(static_cast<float>(VirtualScreenHeight()),
                           destination.bottom)};
}

inline bool ReadThumbnailSource(const Card& card, RectF& source) {
    RECT window = {};
    RECT frame = {};
    RECT cropped = {};
    SIZE size = {};
    if (GetWindowRect(card.hwnd, &window) == FALSE ||
        !ReadFrameBounds(card.hwnd, frame) ||
        IntersectRect(&cropped, &window, &frame) == FALSE ||
        FAILED(DwmQueryThumbnailSourceSize(card.thumbnail.get(), &size))) {
        return false;
    }
    const RectF outer = RectFromRect(window);
    if (outer.empty() || size.cx <= 0 || size.cy <= 0) {
        return false;
    }
    const float scaleX = static_cast<float>(size.cx) / outer.width();
    const float scaleY = static_cast<float>(size.cy) / outer.height();
    source = RectF{static_cast<float>(cropped.left - window.left) * scaleX,
                   static_cast<float>(cropped.top - window.top) * scaleY,
                   static_cast<float>(cropped.right - window.left) * scaleX,
                   static_cast<float>(cropped.bottom - window.top) * scaleY};
    return !source.empty();
}

inline bool ClipThumbnailGeometry(ThumbnailGeometry& geometry) {
    const RectF clipped = ThumbnailViewportClip(geometry.destination);
    if (clipped.empty() || geometry.destination.empty() || geometry.source.empty()) {
        return false;
    }
    const float scaleX = geometry.source.width() / geometry.destination.width();
    const float scaleY = geometry.source.height() / geometry.destination.height();
    geometry.source = RectF{
        geometry.source.left + (clipped.left - geometry.destination.left) * scaleX,
        geometry.source.top + (clipped.top - geometry.destination.top) * scaleY,
        geometry.source.right - (geometry.destination.right - clipped.right) * scaleX,
        geometry.source.bottom - (geometry.destination.bottom - clipped.bottom) * scaleY};
    geometry.destination = clipped;
    const RECT source = ThumbnailRect(geometry.source);
    const RECT destination = ThumbnailRect(geometry.destination);
    return source.right > source.left && source.bottom > source.top &&
           destination.right > destination.left && destination.bottom > destination.top;
}

inline void HideThumbnail(Card& card, DWORD now) {
    DWM_THUMBNAIL_PROPERTIES properties = {};
    properties.dwFlags = DWM_TNP_VISIBLE;
    properties.fVisible = FALSE;
    if (FAILED(DwmUpdateThumbnailProperties(card.thumbnail.get(), &properties))) {
        FailThumbnail(card, now);
        return;
    }
    card.thumbnailVisible = false;
    card.thumbnailDestination = {};
}

inline void SubmitThumbnail(Card& card, const ThumbnailGeometry& geometry,
                            DWORD now) {
    DWM_THUMBNAIL_PROPERTIES properties = {};
    properties.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_RECTSOURCE |
                         DWM_TNP_VISIBLE | DWM_TNP_OPACITY |
                         DWM_TNP_SOURCECLIENTAREAONLY;
    properties.rcDestination = ThumbnailRect(geometry.destination);
    properties.rcSource = ThumbnailRect(geometry.source);
    properties.fVisible = TRUE;
    properties.opacity = 255;
    properties.fSourceClientAreaOnly = FALSE;
    if (FAILED(DwmUpdateThumbnailProperties(card.thumbnail.get(), &properties))) {
        FailThumbnail(card, now);
        return;
    }
    card.thumbnailDestination = properties.rcDestination;
    card.thumbnailVisible = true;
}

inline void UpdateThumbnail(Card& card, DWORD now) {
    if (!EnsureThumbnail(card, now)) {
        return;
    }
    ThumbnailGeometry geometry;
    geometry.destination = WorldToScreen(card.world, context.camera);
    if (ThumbnailViewportClip(geometry.destination).empty()) {
        HideThumbnail(card, now);
        return;
    }
    if (!ReadThumbnailSource(card, geometry.source)) {
        FailThumbnail(card, now);
        return;
    }
    if (!ClipThumbnailGeometry(geometry)) {
        HideThumbnail(card, now);
        return;
    }
    SubmitThumbnail(card, geometry, now);
}

inline void UpdateThumbnails() {
    const DWORD now = GetTickCount();
    for (Card& card : context.cards) {
        UpdateThumbnail(card, now);
    }
}

inline void ReleaseAllThumbnails() {
    for (Card& card : context.cards) {
        ReleaseThumbnail(card);
    }
}


inline COLORREF ColorRef(uint32_t color) {
    return RGB((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
}

inline void DrawHostScene(HDC dc) {
    RECT client = {0, 0, context.virtualScreen.right - context.virtualScreen.left,
                   context.virtualScreen.bottom - context.virtualScreen.top};
    HBRUSH background = CreateSolidBrush(ColorRef(context.settings.backgroundColor));
    FillRect(dc, &client, background);
    DeleteObject(background);
    if (!context.settings.showGrid) {
        return;
    }
    const float spacing = static_cast<float>(context.settings.gridSpacing) *
                          context.camera.zoom;
    if (spacing < kMinGridSpacingPx) {
        return;
    }
    HPEN pen = CreatePen(PS_SOLID, 1, ColorRef(context.settings.gridColor));
    HGDIOBJ oldPen = SelectObject(dc, pen);
    const RectF view = ViewportWorld(context.camera);
    const float startX = std::floor(view.left / context.settings.gridSpacing) *
                         context.settings.gridSpacing;
    const float startY = std::floor(view.top / context.settings.gridSpacing) *
                         context.settings.gridSpacing;
    const int lineCountX = std::min(kMaxGridLines, static_cast<int>(
        std::ceil(static_cast<float>(context.virtualScreen.right - context.virtualScreen.left) / spacing) + 2.0f));
    const int lineCountY = std::min(kMaxGridLines, static_cast<int>(
        std::ceil(static_cast<float>(context.virtualScreen.bottom - context.virtualScreen.top) / spacing) + 2.0f));
    for (int index = 0; index < lineCountX; ++index) {
        const float worldX = startX + static_cast<float>(index) * context.settings.gridSpacing;
        const float screenX = (worldX - context.camera.centerX) * context.camera.zoom +
                              static_cast<float>(client.right) * 0.5f;
        MoveToEx(dc, static_cast<int>(screenX), 0, nullptr);
        LineTo(dc, static_cast<int>(screenX), client.bottom);
    }
    for (int index = 0; index < lineCountY; ++index) {
        const float worldY = startY + static_cast<float>(index) * context.settings.gridSpacing;
        const float screenY = (worldY - context.camera.centerY) * context.camera.zoom +
                              static_cast<float>(client.bottom) * 0.5f;
        MoveToEx(dc, 0, static_cast<int>(screenY), nullptr);
        LineTo(dc, client.right, static_cast<int>(screenY));
    }
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}


// Card header chrome.
//
// Every live card reserves a 28 DIP strip above its body. The strip carries the
// title and two buttons: pin keeps a card above every unpinned card, close
// dismisses the card until the canvas is shown again.
//
// This header stays free of Direct2D and Win32 drawing calls so the native
// regressions can exercise the geometry on its own.


inline RectF CardHeaderRect(const Card& card) {
    const RectF bounds = CardScreenBounds(card);
    return RectF{bounds.left, bounds.top, bounds.right,
                 bounds.top + CardHeaderHeight()};
}

// The inset shrinks with the header so a zoomed-out card still keeps its
// buttons inside the strip instead of clipping them.
inline float CardHeaderButtonInset(float headerHeight, float scale) {
    return std::min(kCardHeaderButtonInsetPx * scale, headerHeight * 0.2f);
}

inline float CardHeaderButtonSize(float headerHeight, float scale) {
    if (headerHeight <= 0.0f) {
        return 0.0f;
    }
    const float inset = CardHeaderButtonInset(headerHeight, scale);
    return std::min(kCardHeaderButtonPx * scale, headerHeight - inset * 2.0f);
}

inline bool CardHeaderButtonsFit(const RectF& header, float scale) {
    if (header.empty()) {
        return false;
    }
    const float size = CardHeaderButtonSize(header.height(), scale);
    if (size <= 0.0f) {
        return false;
    }
    const float inset = CardHeaderButtonInset(header.height(), scale);
    const float gap = kCardHeaderButtonGapPx * scale;
    const float needed =
        size * 2.0f + gap + inset * 2.0f + kCardTitlePaddingPx * scale * 2.0f;
    return header.width() >= needed;
}

inline RectF CardCloseButtonRect(const RectF& header, float scale) {
    if (!CardHeaderButtonsFit(header, scale)) {
        return {};
    }
    const float size = CardHeaderButtonSize(header.height(), scale);
    const float inset = CardHeaderButtonInset(header.height(), scale);
    const float top = header.centerY() - size * 0.5f;
    return MakeRectF(header.right - inset - size, top, size, size);
}

inline RectF CardPinButtonRect(const RectF& header, float scale) {
    const RectF close = CardCloseButtonRect(header, scale);
    if (close.empty()) {
        return {};
    }
    const float gap = kCardHeaderButtonGapPx * scale;
    return MakeRectF(close.left - gap - close.width(), close.top, close.width(),
                     close.height());
}

inline CardHeaderAction HitTestCardHeader(const RectF& header, Vec2 point,
                                          float scale) {
    if (ContainsPoint(CardCloseButtonRect(header, scale), point.x, point.y)) {
        return CardHeaderAction::Close;
    }
    if (ContainsPoint(CardPinButtonRect(header, scale), point.x, point.y)) {
        return CardHeaderAction::Pin;
    }
    return CardHeaderAction::None;
}

// Pinned cards are drawn last and hit-tested first, so pinning a card lifts it
// above every unpinned card without reordering the scan-owned card vector.
inline std::vector<size_t> CardDrawOrder(const std::vector<Card>& cards) {
    std::vector<size_t> order;
    order.reserve(cards.size());
    for (size_t index = 0; index < cards.size(); ++index) {
        if (!cards[index].pinned) {
            order.push_back(index);
        }
    }
    for (size_t index = 0; index < cards.size(); ++index) {
        if (cards[index].pinned) {
            order.push_back(index);
        }
    }
    return order;
}

inline int HitTestCardInOrder(const std::vector<Card>& cards, Vec2 point) {
    const std::vector<size_t> order = CardDrawOrder(cards);
    for (size_t slot = order.size(); slot-- > 0;) {
        const size_t index = order[slot];
        if (ContainsPoint(CardScreenBounds(cards[index]), point.x, point.y)) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

inline constexpr wchar_t kMinimizedBadge[] = L"MINIMIZED";
inline constexpr wchar_t kOtherDesktopBadge[] = L"OTHER DESKTOP";
inline constexpr wchar_t kUnavailableBadge[] = L"PREVIEW UNAVAILABLE";
inline constexpr wchar_t kEmptyCanvasText[] = L"No eligible windows";

inline void SetHudColor(uint32_t rgb, float alpha = 1.0f) {
    context.hudBrush->SetColor(ToColorF(rgb, alpha));
}

inline D2D1_ROUNDED_RECT RoundedRect(const RectF& rect, float radius) {
    const float fitted = std::max(
        0.0f, std::min(radius, std::min(rect.width(), rect.height()) * 0.5f));
    return D2D1::RoundedRect(D2D1::RectF(rect.left, rect.top, rect.right, rect.bottom),
                             fitted, fitted);
}

inline void DrawCardTitle(const Card& card, const RectF& rect, float scale) {
    const float padding = kCardTitlePaddingPx * scale;
    if (context.titleFormat == nullptr || rect.width() <= padding * 2.0f ||
        rect.empty()) {
        return;
    }
    const D2D1_RECT_F titleRect = D2D1::RectF(
        rect.left + padding, rect.top, rect.right - padding, rect.bottom);
    SetHudColor(context.settings.textColor);
    context.hudTarget->DrawTextW(card.title.c_str(),
                                 static_cast<UINT32>(card.title.size()),
                                 context.titleFormat, titleRect,
                                 context.hudBrush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

inline void DrawHeaderButtonBackdrop(const RectF& rect, bool highlighted,
                                     float scale) {
    if (rect.empty() || !highlighted) {
        return;
    }
    SetHudColor(context.settings.accentColor, 0.22f);
    context.hudTarget->FillRoundedRectangle(RoundedRect(rect, 4.0f * scale),
                                            context.hudBrush);
}

inline void DrawCloseGlyph(const RectF& rect, float scale) {
    if (rect.empty()) {
        return;
    }
    const float inset = rect.width() * 0.3f;
    SetHudColor(context.settings.textColor, 0.9f);
    context.hudTarget->DrawLine(
        D2D1::Point2F(rect.left + inset, rect.top + inset),
        D2D1::Point2F(rect.right - inset, rect.bottom - inset),
        context.hudBrush, 1.2f * scale);
    context.hudTarget->DrawLine(
        D2D1::Point2F(rect.right - inset, rect.top + inset),
        D2D1::Point2F(rect.left + inset, rect.bottom - inset),
        context.hudBrush, 1.2f * scale);
}

inline void DrawPinGlyph(const RectF& rect, bool pinned, float scale) {
    if (rect.empty()) {
        return;
    }
    const float centerX = rect.centerX();
    const float head = rect.top + rect.height() * 0.24f;
    const float shoulder = rect.top + rect.height() * 0.58f;
    const float needle = rect.top + rect.height() * 0.78f;
    const float halfWidth = rect.width() * 0.22f;
    const float stroke = (pinned ? 1.6f : 1.1f) * scale;
    SetHudColor(pinned ? context.settings.accentColor : context.settings.textColor,
                pinned ? 1.0f : 0.85f);
    context.hudTarget->DrawLine(D2D1::Point2F(centerX, head),
                                D2D1::Point2F(centerX - halfWidth, shoulder),
                                context.hudBrush, stroke);
    context.hudTarget->DrawLine(D2D1::Point2F(centerX, head),
                                D2D1::Point2F(centerX + halfWidth, shoulder),
                                context.hudBrush, stroke);
    context.hudTarget->DrawLine(
        D2D1::Point2F(centerX - halfWidth, shoulder),
        D2D1::Point2F(centerX + halfWidth, shoulder), context.hudBrush, stroke);
    context.hudTarget->DrawLine(D2D1::Point2F(centerX, shoulder),
                                D2D1::Point2F(centerX, needle),
                                context.hudBrush, stroke);
}

inline void DrawCardHeader(const Card& card, int index, const RectF& header,
                           float scale) {
    if (header.empty()) {
        return;
    }
    SetHudColor(context.settings.backgroundColor, 0.9f);
    context.hudTarget->FillRectangle(
        D2D1::RectF(header.left, header.top, header.right, header.bottom),
        context.hudBrush);

    const RectF pin = CardPinButtonRect(header, scale);
    const RectF close = CardCloseButtonRect(header, scale);
    RectF title = header;
    if (!pin.empty()) {
        title.right = pin.left;
    }
    DrawCardTitle(card, title, scale);

    const bool hovered = index == context.hoverCard;
    DrawHeaderButtonBackdrop(pin, hovered &&
                                      context.hoverHeaderAction ==
                                          CardHeaderAction::Pin,
                             scale);
    DrawHeaderButtonBackdrop(close, hovered &&
                                        context.hoverHeaderAction ==
                                            CardHeaderAction::Close,
                             scale);
    DrawPinGlyph(pin, card.pinned, scale);
    DrawCloseGlyph(close, scale);
}

inline RectF CardBadgeBounds(const RectF& body, float scale) {
    if (context.badgeFormat == nullptr) {
        return {};
    }
    const float padding = kCardTitlePaddingPx * scale;
    const float height = context.badgeFormat->GetFontSize() * 2.0f;
    if (body.width() <= padding * 2.0f ||
        body.height() <= height + padding * 2.0f) {
        return {};
    }
    return RectF{body.left + padding, body.bottom - padding - height,
                 body.right - padding, body.bottom - padding};
}

inline void DrawCardIcon(const Card& card, const RectF& area) {
    const float size = std::min(
        kCardIconMaxPx * ChromeScale(), std::min(area.width(), area.height()));
    if (area.empty() || size <= 0.0f) {
        return;
    }
    ID2D1Bitmap* icon =
        LoadIconBitmap(card.exePath, card.windowClass, card.hwnd);
    if (icon == nullptr) {
        return;
    }
    const float half = size * 0.5f;
    context.hudTarget->DrawBitmap(
        icon, D2D1::RectF(area.centerX() - half, area.centerY() - half,
                          area.centerX() + half, area.centerY() + half),
        0.9f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
}

inline void DrawCardBadge(const Card& card, const RectF& rect) {
    if (rect.empty() || context.badgeFormat == nullptr) {
        return;
    }
    const wchar_t* badge = card.state == WindowVisualState::Minimized
                               ? kMinimizedBadge
                               : kUnavailableBadge;
    if (card.state == WindowVisualState::OtherDesktop) {
        badge = kOtherDesktopBadge;
    }
    SetHudColor(context.settings.mutedTextColor);
    context.hudTarget->DrawTextW(
        badge, static_cast<UINT32>(wcslen(badge)), context.badgeFormat,
        D2D1::RectF(rect.left, rect.top, rect.right, rect.bottom),
        context.hudBrush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

inline void DrawCardBody(const Card& card, const RectF& body, float scale) {
    if (body.empty() || (card.state == WindowVisualState::Live &&
                         card.thumbnail.valid() && card.thumbnailVisible)) {
        return;
    }
    SetHudColor(context.settings.cardFillColor);
    context.hudTarget->FillRoundedRectangle(
        RoundedRect(body, kCardCornerRadiusPx * scale), context.hudBrush);
    const RectF badge = CardBadgeBounds(body, scale);
    const float padding = kCardTitlePaddingPx * scale;
    RectF iconArea = GrowRect(body, -padding);
    if (!badge.empty()) {
        iconArea.bottom = badge.top - padding;
    }
    DrawCardIcon(card, iconArea);
    DrawCardBadge(card, badge);
}

inline void DrawCard(const Card& card, int index, float scale) {
    const RectF rect = CardScreenBounds(card);
    if (rect.empty() || rect.right < 0.0f || rect.bottom < 0.0f ||
        rect.left > static_cast<float>(VirtualScreenWidth()) ||
        rect.top > static_cast<float>(VirtualScreenHeight())) {
        return;
    }
    const float radius = kCardCornerRadiusPx * scale;
    const bool emphasized = index == context.hoverCard || index == context.selectedCard;
    const RectF body = WorldToScreen(card.world, context.camera);
    const RectF header = CardHeaderRect(card);
    DrawCardBody(card, body, scale);
    DrawCardHeader(card, index, header, scale);
    SetHudColor(emphasized ? context.settings.accentColor : context.settings.gridColor);
    context.hudTarget->DrawRoundedRectangle(
        RoundedRect(rect, radius), context.hudBrush,
        emphasized ? 2.0f * scale : 1.0f * scale);
}

inline void DrawMinimapContents(const RectF& map, float scale) {
    const RectF bounds = CanvasWorldBounds();
    const RectF inner = MinimapInnerRect(map);
    if (bounds.empty() || inner.empty()) {
        return;
    }
    context.hudTarget->PushAxisAlignedClip(
        D2D1::RectF(inner.left, inner.top, inner.right, inner.bottom),
        D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    for (const Card& card : context.cards) {
        const RectF projected = MinimapWorldToScreen(card.world, map, bounds);
        SetHudColor(card.state == WindowVisualState::Live
                        ? context.settings.accentColor
                        : context.settings.mutedTextColor);
        context.hudTarget->FillRectangle(
            D2D1::RectF(projected.left, projected.top, projected.right, projected.bottom),
            context.hudBrush);
    }
    const RectF view = MinimapWorldToScreen(ViewportWorld(context.camera), map, bounds);
    SetHudColor(context.settings.textColor, 0.85f);
    context.hudTarget->DrawRectangle(
        D2D1::RectF(view.left, view.top, view.right, view.bottom), context.hudBrush,
        1.0f * scale);
    context.hudTarget->PopAxisAlignedClip();
}

inline void DrawMinimap(float scale) {
    context.minimapRect = {};
    if (!context.settings.minimapEnabled) {
        return;
    }
    const RectF map = CalculateMinimapRect();
    if (map.empty()) {
        return;
    }
    context.minimapRect = map;
    SetHudColor(context.settings.backgroundColor, 0.9f);
    context.hudTarget->FillRoundedRectangle(RoundedRect(map, 6.0f * scale),
                                            context.hudBrush);
    SetHudColor(context.settings.gridColor);
    context.hudTarget->DrawRoundedRectangle(RoundedRect(map, 6.0f * scale),
                                            context.hudBrush, 1.0f * scale);
    DrawMinimapContents(map, scale);
}

inline void DrawEmptyCanvas(float scale) {
    if (!context.cards.empty()) {
        return;
    }
    const D2D1_RECT_F emptyRect = D2D1::RectF(
        24.0f * scale, 24.0f * scale,
        static_cast<float>(VirtualScreenWidth()) - 24.0f * scale,
        static_cast<float>(VirtualScreenHeight()) - 24.0f * scale);
    SetHudColor(context.settings.mutedTextColor);
    context.hudTarget->DrawTextW(
        kEmptyCanvasText, ARRAYSIZE(kEmptyCanvasText) - 1, context.titleFormat,
        emptyRect, context.hudBrush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

inline void DrawAlignmentGuides(float scale) {
    if (context.hasGuideVertical) {
        SetHudColor(context.settings.accentColor, 0.8f);
        context.hudTarget->DrawLine(D2D1::Point2F(context.guideVertical.left, 0.0f),
                                    D2D1::Point2F(context.guideVertical.left,
                                                  static_cast<float>(VirtualScreenHeight())),
                                    context.hudBrush, 1.0f * scale);
    }
    if (context.hasGuideHorizontal) {
        SetHudColor(context.settings.accentColor, 0.8f);
        context.hudTarget->DrawLine(D2D1::Point2F(0.0f, context.guideHorizontal.top),
                                    D2D1::Point2F(static_cast<float>(VirtualScreenWidth()),
                                                  context.guideHorizontal.top),
                                    context.hudBrush, 1.0f * scale);
    }
}

inline bool PresentHudFrame() {
    POINT destination = {context.virtualScreen.left, context.virtualScreen.top};
    POINT origin = {0, 0};
    SIZE size = {VirtualScreenWidth(), VirtualScreenHeight()};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    return UpdateLayeredWindow(context.hudWindow, nullptr, &destination, &size,
                               context.hudDc, &origin, 0, &blend, ULW_ALPHA) != FALSE;
}

inline bool SubmitHudFrame() {
    const int width = VirtualScreenWidth();
    const int height = VirtualScreenHeight();
    if (!EnsureGraphics() || !CreateHudSurface(width, height)) {
        return false;
    }
    const RECT bounds = {0, 0, width, height};
    if (FAILED(context.hudTarget->BindDC(context.hudDc, &bounds))) {
        ReleaseHudSurface();
        return false;
    }
    context.hudTarget->BeginDraw();
    context.hudTarget->Clear(D2D1::ColorF(0, 0.0f));
    const float scale = ChromeScale();
    for (const size_t index : CardDrawOrder(context.cards)) {
        DrawCard(context.cards[index], static_cast<int>(index), scale);
    }
    DrawEmptyCanvas(scale);
    DrawMinimap(scale);
    DrawAlignmentGuides(scale);
    if (FAILED(context.hudTarget->EndDraw())) {
        ReleaseHudSurface();
        return false;
    }
    return PresentHudFrame();
}



inline RECT ReadVirtualScreen() {
    const int left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    return RECT{left, top, left + width, top + height};
}

inline void PlaceCanvasWindows() {
    context.virtualScreen = ReadVirtualScreen();
    const HWND windows[2] = {context.hostWindow, context.hudWindow};
    for (const HWND window : windows) {
        if (window == nullptr) {
            continue;
        }
        SetWindowPos(window, HWND_TOPMOST, context.virtualScreen.left,
                     context.virtualScreen.top, VirtualScreenWidth(),
                     VirtualScreenHeight(), SWP_NOACTIVATE);
    }
    ReleaseHudSurface();
    context.sceneDirty = true;
    context.scanRequested = true;
}

inline void CALLBACK OnWindowDestroyed(HWINEVENTHOOK, DWORD, HWND window,
                                       LONG objectId, LONG childId, DWORD,
                                       DWORD) {
    if (objectId != OBJID_WINDOW || childId != CHILDID_SELF) {
        return;
    }
    for (Card& card : context.cards) {
        if (card.hwnd == window) {
            card.destroyed = true;
            card.thumbnail.reset();
            context.scanRequested = true;
            context.sceneDirty = true;
        }
    }
}

inline void InstallDestroyHook() {
    if (context.destroyHook != nullptr) {
        return;
    }
    context.destroyHook = SetWinEventHook(
        EVENT_OBJECT_DESTROY, EVENT_OBJECT_DESTROY, nullptr, OnWindowDestroyed,
        0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    if (context.destroyHook == nullptr) {
        Wh_Log(L"Aleph: SetWinEventHook failed: %lu", GetLastError());
    }
}

inline void UninstallDestroyHook() {
    if (context.destroyHook == nullptr) {
        return;
    }
    UnhookWinEvent(context.destroyHook);
    context.destroyHook = nullptr;
}

inline void HideCanvasWindows() {
    context.visible = false;
    UninstallDestroyHook();
    if (GetCapture() == context.hostWindow) {
        ReleaseCapture();
    }
    if (context.hudWindow != nullptr) {
        ShowWindow(context.hudWindow, SW_HIDE);
    }
    if (context.hostWindow != nullptr) {
        ShowWindow(context.hostWindow, SW_HIDE);
    }
    ReleaseAllThumbnails();
    context.dismissedFingerprints.clear();
    context.panning = false;
    context.draggingMinimap = false;
    context.draggedCard = -1;
    context.diving = false;
    context.hasGuideVertical = false;
    context.hasGuideHorizontal = false;
    context.hoverCard = -1;
    context.minimapHover = false;
    context.camera.snapToTarget();
    context.sceneDirty = true;
}

inline void ShowCanvasWindows() {
    if (context.visible) {
        return;
    }
    if (UpdateChromeDpi()) {
        ReleaseTextFormats();
    }
    PlaceCanvasWindows();
    if (!context.layoutLoaded) {
        LoadLayout();
        context.layoutLoaded = true;
    }
    context.visible = true;
    context.scanRequested = true;
    context.lastFrameTick = GetTickCount();
    SetForegroundWindow(context.hostWindow);
    ShowWindow(context.hostWindow, SW_SHOWNOACTIVATE);
    ShowWindow(context.hudWindow, SW_SHOWNOACTIVATE);
    SetForegroundWindow(context.hostWindow);
    SetFocus(context.hostWindow);
    context.sceneDirty = true;
    InstallDestroyHook();
}

inline void ToggleCanvas() {
    if (context.visible) {
        HideCanvasWindows();
        return;
    }
    ShowCanvasWindows();
}

// Moving an other-desktop window onto the desktop that shows the canvas.
//
// `IVirtualDesktopManager::MoveWindowToDesktop` needs the GUID of the target
// desktop, and the documented way to obtain one is `GetWindowDesktopId`. The
// canvas host window is the only window the mod owns that the user is looking
// at when activating a card, so its desktop identifier is the pull target.


enum class DesktopPullResult {
    // No usable manager, handle or reference; the window stays where it is.
    Unavailable,
    // The window already lives on the target desktop.
    AlreadyHere,
    // The move was accepted.
    Moved,
    // The move was requested and refused.
    Rejected,
};

struct DesktopPullOutcome {
    DesktopPullResult result = DesktopPullResult::Unavailable;
    HRESULT error = S_OK;
};

inline DesktopPullOutcome PullWindowToDesktop(IVirtualDesktopManager* manager,
                                              HWND window, HWND reference) {
    DesktopPullOutcome outcome;
    if (manager == nullptr || window == nullptr || reference == nullptr) {
        return outcome;
    }
    GUID target = {};
    if (FAILED(manager->GetWindowDesktopId(reference, &target))) {
        return outcome;
    }
    GUID current = {};
    if (SUCCEEDED(manager->GetWindowDesktopId(window, &current)) &&
        IsEqualGUID(current, target) != FALSE) {
        outcome.result = DesktopPullResult::AlreadyHere;
        return outcome;
    }
    // An unreadable source desktop is not proof that no move is needed, so only
    // a confirmed match skips the request.
    outcome.error = manager->MoveWindowToDesktop(window, target);
    outcome.result = SUCCEEDED(outcome.error) ? DesktopPullResult::Moved
                                              : DesktopPullResult::Rejected;
    return outcome;
}

inline POINT CursorInCanvas() {
    POINT point = {};
    GetCursorPos(&point);
    point.x -= context.virtualScreen.left;
    point.y -= context.virtualScreen.top;
    return point;
}

inline int HitTestCard(Vec2 point) {
    return HitTestCardInOrder(context.cards, point);
}

inline CardHeaderAction HeaderActionAt(int index, Vec2 point) {
    if (index < 0 || static_cast<size_t>(index) >= context.cards.size()) {
        return CardHeaderAction::None;
    }
    return HitTestCardHeader(CardHeaderRect(context.cards[static_cast<size_t>(index)]),
                             point, ChromeScale());
}

inline void DismissCard(int index) {
    if (index < 0 || static_cast<size_t>(index) >= context.cards.size()) {
        return;
    }
    context.dismissedFingerprints.insert(
        context.cards[static_cast<size_t>(index)].fingerprint);
    RemoveCardAt(static_cast<size_t>(index));
    context.hoverHeaderAction = CardHeaderAction::None;
    context.sceneDirty = true;
}

inline void ToggleCardPin(int index) {
    if (index < 0 || static_cast<size_t>(index) >= context.cards.size()) {
        return;
    }
    Card& card = context.cards[static_cast<size_t>(index)];
    card.pinned = !card.pinned;
    RememberLayoutPin(card.fingerprint, card.world, card.pinned);
    MarkLayoutDirty(GetTickCount());
    context.sceneDirty = true;
}

inline bool PointInMinimap(Vec2 point) {
    return context.settings.minimapEnabled &&
           ContainsPoint(context.minimapRect, point.x, point.y);
}

// An other-desktop card only becomes usable once its window joins the desktop
// that displays the canvas. A refused move keeps the old behaviour: the dive
// still ends in foreground activation.
inline void PullOtherDesktopWindow(const Card& card) {
    if (card.destroyed) {
        return;
    }
    const DesktopPullOutcome outcome =
        PullWindowToDesktop(context.desktopManager, card.hwnd, context.hostWindow);
    if (outcome.result != DesktopPullResult::Rejected) {
        return;
    }
    Wh_Log(L"Aleph: MoveWindowToDesktop failed: 0x%08X",
           static_cast<unsigned int>(outcome.error));
}

inline void BeginDive(int index) {
    if (index < 0 || static_cast<size_t>(index) >= context.cards.size()) {
        return;
    }
    context.panning = false;
    context.draggingMinimap = false;
    context.draggedCard = -1;
    context.camera.zoomAnchored = false;
    context.diveCard = index;
    context.diveStart = context.camera;
    Card& card = context.cards[static_cast<size_t>(index)];
    context.diveTarget = DiveCameraForCard(card);
    context.diveStartTick = GetTickCount();
    if (card.state == WindowVisualState::OtherDesktop) {
        PullOtherDesktopWindow(card);
    }
    // Pulling a minimized cross-desktop window leaves it iconic, so the restore
    // follows the pull instead of the scan state.
    if (IsIconic(card.hwnd) != FALSE) {
        ShowWindowAsync(card.hwnd, SW_RESTORE);
    }
    context.diving = true;
}

inline void FinishDive() {
    if (context.diveCard < 0 ||
        static_cast<size_t>(context.diveCard) >= context.cards.size()) {
        context.diving = false;
        return;
    }
    Card& card = context.cards[static_cast<size_t>(context.diveCard)];
    if (!card.destroyed && IsWindow(card.hwnd)) {
        SetForegroundWindow(card.hwnd);
    }
    context.camera.targetCenterX = context.camera.centerX;
    context.camera.targetCenterY = context.camera.centerY;
    context.camera.targetZoom = context.camera.zoom;
    HideCanvasWindows();
}

inline void AdvanceDive(DWORD now) {
    if (!context.diving) {
        return;
    }
    const float duration = static_cast<float>(context.settings.animationMs);
    const float progress = duration <= 0.0f ? 1.0f
        : static_cast<float>(now - context.diveStartTick) / duration;
    const float eased = EaseOutCubic(progress);
    context.camera.centerX = context.diveStart.centerX +
        (context.diveTarget.targetCenterX - context.diveStart.centerX) * eased;
    context.camera.centerY = context.diveStart.centerY +
        (context.diveTarget.targetCenterY - context.diveStart.centerY) * eased;
    context.camera.zoom = context.diveStart.zoom +
        (context.diveTarget.targetZoom - context.diveStart.zoom) * eased;
}

inline void StartPan(POINT cursor) {
    context.camera.snapToTarget();
    context.camera.zoomAnchored = false;
    context.panning = true;
    context.panAnchorCursor = cursor;
    context.panAnchorWorld = ScreenToWorld(
        Vec2{static_cast<float>(cursor.x), static_cast<float>(cursor.y)}, context.camera);
}

inline void UpdatePan(POINT cursor) {
    const Vec2 anchor = context.panAnchorWorld;
    const float zoom = std::max(context.camera.zoom, 0.01f);
    context.camera.targetCenterX = anchor.x -
        (static_cast<float>(cursor.x) - ScreenCenterX()) / zoom;
    context.camera.targetCenterY = anchor.y -
        (static_cast<float>(cursor.y) - ScreenCenterY()) / zoom;
}

inline void BeginCardDrag(int index, POINT cursor) {
    context.hoverHeaderAction = CardHeaderAction::None;
    context.camera.snapToTarget();
    context.camera.zoomAnchored = false;
    context.draggedCard = index;
    context.dragCursor = cursor;
    const RectF screen = WorldToScreen(context.cards[static_cast<size_t>(index)].world,
                                       context.camera);
    context.dragGrabOffset.x = static_cast<LONG>(static_cast<float>(cursor.x) - screen.left);
    context.dragGrabOffset.y = static_cast<LONG>(static_cast<float>(cursor.y) - screen.top);
    SetCapture(context.hostWindow);
}

inline void UpdateCardDrag(POINT cursor) {
    if (context.draggedCard < 0 ||
        static_cast<size_t>(context.draggedCard) >= context.cards.size()) {
        return;
    }
    const Vec2 world = ScreenToWorld(
        Vec2{static_cast<float>(cursor.x - context.dragGrabOffset.x),
             static_cast<float>(cursor.y - context.dragGrabOffset.y)}, context.camera);
    Card& card = context.cards[static_cast<size_t>(context.draggedCard)];
    RectF moved = MakeRectF(world.x, world.y, card.world.width(), card.world.height());
    const SnapResult snap = ComputeSnap(moved, context.draggedCard);
    moved.left = snap.left;
    moved.top = snap.top;
    moved.right = moved.left + card.world.width();
    moved.bottom = moved.top + card.world.height();
    card.world = moved;
    context.hasGuideVertical = snap.snapVertical;
    context.hasGuideHorizontal = snap.snapHorizontal;
    context.guideVertical = RectF{(snap.guideX - context.camera.centerX) * context.camera.zoom +
                                      ScreenCenterX(), 0.0f, 0.0f, 0.0f};
    context.guideHorizontal = RectF{0.0f,
        (snap.guideY - context.camera.centerY) * context.camera.zoom + ScreenCenterY(),
        0.0f, 0.0f};
    RememberLayoutEntry(card.fingerprint, card.world);
    MarkLayoutDirty(GetTickCount());
}

inline void EndPointerDrag() {
    context.panning = false;
    context.draggingMinimap = false;
    context.draggedCard = -1;
    context.hasGuideVertical = false;
    context.hasGuideHorizontal = false;
    context.hoverHeaderAction = CardHeaderAction::None;
    context.sceneDirty = true;
    if (GetCapture() == context.hostWindow) {
        ReleaseCapture();
    }
}

inline void HandleMouseMove(POINT cursor) {
    if (context.diving) {
        return;
    }
    if (UpdateChromeDpi()) {
        ReleaseTextFormats();
        context.sceneDirty = true;
    }
    context.dragCursor = cursor;
    if (context.draggingMinimap) {
        PanCameraFromMinimap(Vec2{static_cast<float>(cursor.x),
                                  static_cast<float>(cursor.y)});
        return;
    }
    if (context.draggedCard >= 0) {
        UpdateCardDrag(cursor);
        context.sceneDirty = true;
        return;
    }
    if (context.panning) {
        UpdatePan(cursor);
        context.sceneDirty = true;
        return;
    }
    const Vec2 point{static_cast<float>(cursor.x), static_cast<float>(cursor.y)};
    const bool minimapHover = PointInMinimap(point);
    const int hoverCard = minimapHover ? -1 : HitTestCard(point);
    const CardHeaderAction hoverAction =
        hoverCard >= 0 ? HeaderActionAt(hoverCard, point) : CardHeaderAction::None;
    if (minimapHover != context.minimapHover || hoverCard != context.hoverCard ||
        hoverAction != context.hoverHeaderAction) {
        context.minimapHover = minimapHover;
        context.hoverCard = hoverCard;
        context.hoverHeaderAction = hoverAction;
        context.sceneDirty = true;
    }
}

inline void HandleMouseLeave() {
    context.hoverCard = -1;
    context.hoverHeaderAction = CardHeaderAction::None;
    context.minimapHover = false;
    context.sceneDirty = true;
}

inline void HandleLeftButtonDown(POINT cursor) {
    if (context.diving) {
        return;
    }
    context.sceneDirty = true;
    const Vec2 point{static_cast<float>(cursor.x), static_cast<float>(cursor.y)};
    if (PointInMinimap(point)) {
        context.draggingMinimap = true;
        SetCapture(context.hostWindow);
        PanCameraFromMinimap(point);
        return;
    }
    const int card = HitTestCard(point);
    if (card >= 0) {
        const CardHeaderAction action = HeaderActionAt(card, point);
        if (action == CardHeaderAction::Close) {
            DismissCard(card);
            return;
        }
        if (action == CardHeaderAction::Pin) {
            ToggleCardPin(card);
            return;
        }
    }
    context.selectedCard = card;
    if (card >= 0) {
        BeginCardDrag(card, cursor);
        return;
    }
    if ((GetKeyState(VK_SPACE) & 0x8000) != 0) {
        StartPan(cursor);
        SetCapture(context.hostWindow);
    }
}

inline void HandleMouseWheel(WPARAM wParam, LPARAM lParam) {
    if (context.diving || GET_WHEEL_DELTA_WPARAM(wParam) == 0) {
        return;
    }
    POINT cursor = {GET_X_LPARAM(lParam) - context.virtualScreen.left,
                    GET_Y_LPARAM(lParam) - context.virtualScreen.top};
    const float steps = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) /
                        static_cast<float>(WHEEL_DELTA);
    const float factor = std::pow(kZoomStep, steps);
    ZoomAtPoint(context.camera, context.camera.targetZoom * factor,
                Vec2{static_cast<float>(cursor.x), static_cast<float>(cursor.y)});
}

inline void HandleKeyDown(WPARAM key) {
    if (key == VK_ESCAPE) {
        HideCanvasWindows();
        return;
    }
    if (context.diving) {
        return;
    }
    if (key == VK_RETURN) {
        BeginDive(context.selectedCard >= 0 ? context.selectedCard : context.hoverCard);
        return;
    }
    if (key == VK_HOME) {
        FitAllCards();
        return;
    }
    if (key == VK_TAB && !context.cards.empty()) {
        const int direction = (GetKeyState(VK_SHIFT) & 0x8000) != 0 ? -1 : 1;
        const int count = static_cast<int>(context.cards.size());
        const int start = context.selectedCard < 0
                              ? (direction > 0 ? -1 : 0)
                              : context.selectedCard;
        context.selectedCard = (start + direction + count) % count;
        CenterOnCard(context.selectedCard);
        return;
    }
    if (key == VK_LEFT || key == VK_RIGHT || key == VK_UP || key == VK_DOWN) {
        context.camera.zoomAnchored = false;
        const float amount = 120.0f / std::max(context.camera.zoom, 0.01f);
        context.camera.targetCenterX += key == VK_LEFT ? -amount : key == VK_RIGHT ? amount : 0.0f;
        context.camera.targetCenterY += key == VK_UP ? -amount : key == VK_DOWN ? amount : 0.0f;
    }
}


static constexpr wchar_t kHostClassName[] = L"AlephCanvasHostWindow";
static constexpr wchar_t kHudClassName[] = L"AlephCanvasHudWindow";

inline void SyncHudZOrder() {
    if (!context.visible || context.hudWindow == nullptr) {
        return;
    }
    SetWindowPos(context.hudWindow, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

inline void RefreshCanvasGeometry() {
    if (UpdateChromeDpi()) {
        ReleaseTextFormats();
    }
    PlaceCanvasWindows();
}

inline LRESULT CALLBACK CanvasWindowProc(HWND window, UINT message,
                                          WPARAM wParam, LPARAM lParam) {
    if (window == context.hudWindow) {
        if (message == WM_NCHITTEST) {
            return HTTRANSPARENT;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }
    switch (message) {
        case WM_PAINT: {
            PAINTSTRUCT paint = {};
            HDC dc = BeginPaint(window, &paint);
            DrawHostScene(dc);
            EndPaint(window, &paint);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_MOUSEMOVE: {
            TRACKMOUSEEVENT tracking = {sizeof(tracking), TME_LEAVE, window, 0};
            TrackMouseEvent(&tracking);
            const POINT cursor = CursorInCanvas();
            HandleMouseMove(cursor);
            return 0;
        }
        case WM_MOUSELEAVE:
            HandleMouseLeave();
            return 0;
        case WM_LBUTTONDOWN:
            HandleLeftButtonDown(CursorInCanvas());
            return 0;
        case WM_LBUTTONDBLCLK: {
            const Vec2 point{static_cast<float>(CursorInCanvas().x),
                             static_cast<float>(CursorInCanvas().y)};
            const int index = HitTestCard(point);
            if (HeaderActionAt(index, point) != CardHeaderAction::None) {
                return 0;
            }
            BeginDive(index);
            return 0;
        }
        case WM_LBUTTONUP:
            EndPointerDrag();
            return 0;
        case WM_MBUTTONDOWN:
            if (context.diving) return 0;
            StartPan(CursorInCanvas());
            SetCapture(window);
            return 0;
        case WM_MBUTTONUP:
        case WM_CAPTURECHANGED:
            EndPointerDrag();
            return 0;
        case WM_MOUSEWHEEL:
            HandleMouseWheel(wParam, lParam);
            return 0;
        case WM_KEYDOWN:
            HandleKeyDown(wParam);
            return 0;
        case WM_DISPLAYCHANGE:
        case WM_DPICHANGED:
            RefreshCanvasGeometry();
            return 0;
        case WM_WINDOWPOSCHANGED:
            SyncHudZOrder();
            return DefWindowProcW(window, message, wParam, lParam);
        case WM_CLOSE:
            HideCanvasWindows();
            return 0;
        default:
            return DefWindowProcW(window, message, wParam, lParam);
    }
}

inline bool RegisterCanvasWindows(HINSTANCE instance) {
    WNDCLASSEXW hostClass = {};
    hostClass.cbSize = sizeof(hostClass);
    hostClass.lpfnWndProc = CanvasWindowProc;
    hostClass.hInstance = instance;
    hostClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    hostClass.lpszClassName = kHostClassName;
    hostClass.style = CS_DBLCLKS;
    if (RegisterClassExW(&hostClass) == 0) {
        return false;
    }
    WNDCLASSEXW hudClass = hostClass;
    hudClass.lpszClassName = kHudClassName;
    if (RegisterClassExW(&hudClass) == 0) {
        UnregisterClassW(kHostClassName, instance);
        return false;
    }
    context.virtualScreen = ReadVirtualScreen();
    context.hostWindow = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW, kHostClassName, L"Aleph Canvas",
        WS_POPUP, context.virtualScreen.left, context.virtualScreen.top,
        VirtualScreenWidth(), VirtualScreenHeight(), nullptr, nullptr, instance,
        nullptr);
    context.hudWindow = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW |
            WS_EX_NOACTIVATE,
        kHudClassName, L"Aleph Canvas HUD", WS_POPUP,
        context.virtualScreen.left, context.virtualScreen.top,
        VirtualScreenWidth(), VirtualScreenHeight(), nullptr, nullptr, instance,
        nullptr);
    if (context.hostWindow == nullptr || context.hudWindow == nullptr) {
        if (context.hudWindow != nullptr) DestroyWindow(context.hudWindow);
        if (context.hostWindow != nullptr) DestroyWindow(context.hostWindow);
        context.hudWindow = nullptr;
        context.hostWindow = nullptr;
        UnregisterClassW(kHudClassName, instance);
        UnregisterClassW(kHostClassName, instance);
        return false;
    }
    return true;
}

inline bool RegisterCanvasHotkey() {
    UINT modifiers = 0;
    UINT key = 0;
    if (!ParseHotkeyText(context.settings.toggleHotkey, modifiers, key)) {
        Wh_Log(L"Aleph: invalid toggle hotkey");
        return false;
    }
    if (context.hotkeyRegistered && context.hotkeyModifiers == modifiers &&
        context.hotkeyKey == key) {
        return true;
    }
    const UINT nextId = context.hotkeyId == kToggleHotkeyId
                            ? kToggleHotkeyId + 1 : kToggleHotkeyId;
    if (!RegisterHotKey(nullptr, nextId, modifiers | MOD_NOREPEAT, key)) {
        Wh_Log(L"Aleph: RegisterHotKey failed: %lu", GetLastError());
        return false;
    }
    if (context.hotkeyRegistered) {
        UnregisterHotKey(nullptr, context.hotkeyId);
    }
    context.hotkeyId = nextId;
    context.hotkeyModifiers = modifiers;
    context.hotkeyKey = key;
    context.hotkeyRegistered = true;
    return true;
}

inline void ScanAndMerge(IVirtualDesktopManager* desktopManager, DWORD now) {
    if (!context.visible || context.diving ||
        (!context.scanRequested && now - context.lastRescanTick <
             static_cast<DWORD>(context.settings.rescanIntervalMs))) {
        return;
    }
    const std::vector<ScannedWindow> scanned = ScanWindows(desktopManager);
    context.sceneDirty = MergeScan(scanned) || context.sceneDirty;
    if (HasDueThumbnailRetry(now) || ConsumeDueIconRetries(now)) {
        context.sceneDirty = true;
    }
    if (!context.cameraInitialized) {
        FitAllCards();
        context.camera.snapToTarget();
        context.cameraInitialized = true;
    }
    context.lastRescanTick = now;
    context.scanRequested = false;
}

inline void RenderCanvasFrame() {
    if (!context.visible) {
        return;
    }
    UpdateThumbnails();
    InvalidateRect(context.hostWindow, nullptr, FALSE);
    UpdateWindow(context.hostWindow);
    SubmitHudFrame();
    context.sceneDirty = false;
}

inline void UpdateCameraAndDive(DWORD now) {
    const DWORD elapsed = now - context.lastFrameTick;
    context.lastFrameTick = now;
    const float seconds = static_cast<float>(elapsed) / 1000.0f;
    AdvanceDive(now);
    if (!context.diving) {
        AdvanceCamera(context.camera, seconds);
    }
    context.cameraMotion = !context.camera.settled();
}

inline void ProcessCanvasMessage(const MSG& message) {
    if (message.message == WM_HOTKEY && message.wParam == context.hotkeyId) {
        ToggleCanvas();
        return;
    }
    TranslateMessage(&message);
    DispatchMessageW(&message);
}

inline void CleanupCanvas(HINSTANCE instance) {
    UnregisterHotKey(nullptr, context.hotkeyId);
    context.hotkeyRegistered = false;
    HideCanvasWindows();
    ReleaseAllThumbnails();
    ClearCards();
    if (context.hudWindow != nullptr) DestroyWindow(context.hudWindow);
    if (context.hostWindow != nullptr) DestroyWindow(context.hostWindow);
    context.hudWindow = nullptr;
    context.hostWindow = nullptr;
    ReleaseGraphics();
    UnregisterClassW(kHudClassName, instance);
    UnregisterClassW(kHostClassName, instance);
}

inline bool DrainCanvasMessages() {
    MSG message = {};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) {
            return false;
        }
        ProcessCanvasMessage(message);
    }
    return true;
}

inline void ApplyPendingSettings() {
    if (!context.settingsDirty.exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    LoadSettings();
    RegisterCanvasHotkey();
    context.camera.targetZoom = ClampZoom(context.camera.targetZoom);
    context.camera.zoomAnchored = false;
    context.scanRequested = true;
    context.sceneDirty = true;
}

inline DWORD CanvasWaitTimeout(DWORD now) {
    DWORD timeout = INFINITE;
    if (context.visible) {
        const DWORD interval = static_cast<DWORD>(context.settings.rescanIntervalMs);
        const DWORD elapsed = now - context.lastRescanTick;
        timeout = elapsed >= interval ? 0 : interval - elapsed;
    }
    if (context.layoutDirty) {
        const DWORD elapsed = now - context.layoutDirtyTick;
        const DWORD remaining = elapsed >= kLayoutSaveDelayMs
                                    ? 0 : kLayoutSaveDelayMs - elapsed;
        timeout = std::min(timeout, remaining);
    }
    return timeout;
}

inline void AdvanceCanvasFrame(IVirtualDesktopManager* manager, DWORD now) {
    ApplyPendingSettings();
    ScanAndMerge(manager, now);
    UpdateCameraAndDive(now);
    if (context.visible && (context.sceneDirty || context.cameraMotion ||
                            context.diving)) {
        RenderCanvasFrame();
    }
    if (context.diving && now - context.diveStartTick >=
                              static_cast<DWORD>(context.settings.animationMs)) {
        FinishDive();
    }
    FlushLayoutIfDue(now);
}

inline DWORD PacedCanvasTimeout(DWORD now) {
    if (!context.visible || (!context.cameraMotion && !context.diving)) {
        return CanvasWaitTimeout(now);
    }
    const DWORD started = GetTickCount();
    const HRESULT result = DwmFlush();
    return FAILED(result) || GetTickCount() == started ? kFrameIntervalMs : 0;
}

inline void RunCanvasLoop(IVirtualDesktopManager* manager) {
    const HANDLE events[2] = {context.stopEvent, context.settingsEvent};
    while (WaitForSingleObject(context.stopEvent, 0) != WAIT_OBJECT_0) {
        if (!DrainCanvasMessages()) {
            return;
        }
        const DWORD now = GetTickCount();
        AdvanceCanvasFrame(manager, now);
        const DWORD wait = MsgWaitForMultipleObjectsEx(
            2, events, PacedCanvasTimeout(now), QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        if (wait == WAIT_OBJECT_0 || wait == WAIT_FAILED) {
            return;
        }
    }
}

inline DWORD WINAPI CanvasWorkerThread(LPVOID) {
    const DPI_AWARENESS_CONTEXT previousDpi =
        SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(comResult)) {
        if (previousDpi != nullptr) SetThreadDpiAwarenessContext(previousDpi);
        return 1;
    }
    HINSTANCE instance = GetModuleHandleW(nullptr);
    if (!RegisterCanvasWindows(instance)) {
        CoUninitialize();
        if (previousDpi != nullptr) SetThreadDpiAwarenessContext(previousDpi);
        return 1;
    }
    if (!RegisterCanvasHotkey()) {
        Wh_Log(L"Aleph: the toggle hotkey is unavailable");
    }
    IVirtualDesktopManager* desktopManager = nullptr;
    CoCreateInstance(CLSID_VirtualDesktopManager, nullptr, CLSCTX_INPROC_SERVER,
                    IID_PPV_ARGS(&desktopManager));
    context.desktopManager = desktopManager;
    if (context.settings.openOnStart) {
        ToggleCanvas();
    }
    context.startupSucceeded = true;
    SetEvent(context.readyEvent);
    RunCanvasLoop(desktopManager);
    context.desktopManager = nullptr;
    if (desktopManager != nullptr) {
        desktopManager->Release();
    }
    if (context.layoutDirty) {
        SaveLayoutNow();
    }
    CleanupCanvas(instance);
    CoUninitialize();
    if (previousDpi != nullptr) SetThreadDpiAwarenessContext(previousDpi);
    return 0;
}

CanvasContext context;

void CloseWorkerEvents() {
    HANDLE* events[] = {&context.stopEvent, &context.settingsEvent,
                       &context.readyEvent};
    for (HANDLE* event : events) {
        if (*event != nullptr) {
            CloseHandle(*event);
            *event = nullptr;
        }
    }
}

BOOL WhTool_ModInit() {
    LoadSettings();
    context.stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    context.settingsEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    context.readyEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (context.stopEvent == nullptr || context.settingsEvent == nullptr ||
        context.readyEvent == nullptr) {
        CloseWorkerEvents();
        return FALSE;
    }
    context.workerThread = CreateThread(nullptr, 0, CanvasWorkerThread, nullptr,
                                        0, &context.workerThreadId);
    if (context.workerThread == nullptr) {
        CloseWorkerEvents();
        return FALSE;
    }
    const HANDLE startup[] = {context.readyEvent, context.workerThread};
    const DWORD result = WaitForMultipleObjects(2, startup, FALSE, 15000);
    if (result == WAIT_OBJECT_0 && context.startupSucceeded) {
        return TRUE;
    }
    SetEvent(context.stopEvent);
    WaitForSingleObject(context.workerThread, INFINITE);
    CloseHandle(context.workerThread);
    context.workerThread = nullptr;
    CloseWorkerEvents();
    return FALSE;
}

void WhTool_ModUninit() {
    if (context.stopEvent != nullptr) {
        SetEvent(context.stopEvent);
    }
    if (context.workerThread != nullptr) {
        WaitForSingleObject(context.workerThread, INFINITE);
        CloseHandle(context.workerThread);
        context.workerThread = nullptr;
    }
    CloseWorkerEvents();
}

void WhTool_ModSettingsChanged() {
    context.settingsDirty.store(true, std::memory_order_release);
    if (context.settingsEvent != nullptr) {
        SetEvent(context.settingsEvent);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Windhawk tool mod implementation for mods which don't need to inject to other
// processes or hook other functions. Context:
// https://github.com/ramensoftware/windhawk/wiki/Mods-as-tools:-Running-mods-in-a-dedicated-process
//
// The mod will load and run in a dedicated windhawk.exe process.
//
// Paste the code below as part of the mod code, and use these callbacks:
// * WhTool_ModInit
// * WhTool_ModSettingsChanged
// * WhTool_ModUninit
//
// Currently, other callbacks are not supported.

bool g_isToolModProcessLauncher;
HANDLE g_toolModProcessMutex;

void WINAPI EntryPoint_Hook() {
    Wh_Log(L">");
    ExitThread(0);
}

BOOL Wh_ModInit() {
    DWORD sessionId;
    if (ProcessIdToSessionId(GetCurrentProcessId(), &sessionId) &&
        sessionId == 0) {
        return FALSE;
    }

    bool isExcluded = false;
    bool isToolModProcess = false;
    bool isCurrentToolModProcess = false;
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLine(), &argc);
    if (!argv) {
        Wh_Log(L"CommandLineToArgvW failed");
        return FALSE;
    }

    for (int i = 1; i < argc; i++) {
        if (wcscmp(argv[i], L"-service") == 0 ||
            wcscmp(argv[i], L"-service-start") == 0 ||
            wcscmp(argv[i], L"-service-stop") == 0) {
            isExcluded = true;
            break;
        }
    }

    for (int i = 1; i < argc - 1; i++) {
        if (wcscmp(argv[i], L"-tool-mod") == 0) {
            isToolModProcess = true;
            if (wcscmp(argv[i + 1], WH_MOD_ID) == 0) {
                isCurrentToolModProcess = true;
            }
            break;
        }
    }

    LocalFree(argv);

    if (isExcluded) {
        return FALSE;
    }

    if (isCurrentToolModProcess) {
        g_toolModProcessMutex =
            CreateMutex(nullptr, TRUE, L"windhawk-tool-mod_" WH_MOD_ID);
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

        IMAGE_DOS_HEADER* dosHeader =
            (IMAGE_DOS_HEADER*)GetModuleHandle(nullptr);
        IMAGE_NT_HEADERS* ntHeaders =
            (IMAGE_NT_HEADERS*)((BYTE*)dosHeader + dosHeader->e_lfanew);

        DWORD entryPointRVA = ntHeaders->OptionalHeader.AddressOfEntryPoint;
        void* entryPoint = (BYTE*)dosHeader + entryPointRVA;

        Wh_SetFunctionHook(entryPoint, (void*)EntryPoint_Hook, nullptr);
        return TRUE;
    }

    if (isToolModProcess) {
        return FALSE;
    }

    g_isToolModProcessLauncher = true;
    return TRUE;
}

void Wh_ModAfterInit() {
    if (!g_isToolModProcessLauncher) {
        return;
    }

    WCHAR currentProcessPath[MAX_PATH];
    switch (GetModuleFileName(nullptr, currentProcessPath,
                              ARRAYSIZE(currentProcessPath))) {
        case 0:
        case ARRAYSIZE(currentProcessPath):
            Wh_Log(L"GetModuleFileName failed");
            return;
    }

    WCHAR
    commandLine[MAX_PATH + 2 +
                (sizeof(L" -tool-mod \"" WH_MOD_ID "\"") / sizeof(WCHAR)) - 1];
    swprintf_s(commandLine, L"\"%s\" -tool-mod \"%s\"", currentProcessPath,
               WH_MOD_ID);

    HMODULE kernelModule = GetModuleHandle(L"kernelbase.dll");
    if (!kernelModule) {
        kernelModule = GetModuleHandle(L"kernel32.dll");
        if (!kernelModule) {
            Wh_Log(L"No kernelbase.dll/kernel32.dll");
            return;
        }
    }

    using CreateProcessInternalW_t = BOOL(WINAPI*)(
        HANDLE hUserToken, LPCWSTR lpApplicationName, LPWSTR lpCommandLine,
        LPSECURITY_ATTRIBUTES lpProcessAttributes,
        LPSECURITY_ATTRIBUTES lpThreadAttributes, WINBOOL bInheritHandles,
        DWORD dwCreationFlags, LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory,
        LPSTARTUPINFOW lpStartupInfo,
        LPPROCESS_INFORMATION lpProcessInformation,
        PHANDLE hRestrictedUserToken);
    CreateProcessInternalW_t pCreateProcessInternalW =
        (CreateProcessInternalW_t)GetProcAddress(kernelModule,
                                                 "CreateProcessInternalW");
    if (!pCreateProcessInternalW) {
        Wh_Log(L"No CreateProcessInternalW");
        return;
    }

    STARTUPINFO si{
        .cb = sizeof(STARTUPINFO),
        .dwFlags = STARTF_FORCEOFFFEEDBACK,
    };
    PROCESS_INFORMATION pi;
    if (!pCreateProcessInternalW(nullptr, currentProcessPath, commandLine,
                                 nullptr, nullptr, FALSE, NORMAL_PRIORITY_CLASS,
                                 nullptr, nullptr, &si, &pi, nullptr)) {
        Wh_Log(L"CreateProcess failed");
        return;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

void Wh_ModSettingsChanged() {
    if (g_isToolModProcessLauncher) {
        return;
    }

    WhTool_ModSettingsChanged();
}

void Wh_ModUninit() {
    if (g_isToolModProcessLauncher) {
        return;
    }

    WhTool_ModUninit();
    ExitProcess(0);
}
