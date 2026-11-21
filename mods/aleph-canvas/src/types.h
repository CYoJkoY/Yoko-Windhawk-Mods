#pragma once

// Value types shared by the canvas modules.

#include "platform.h"
#include "constants.h"
#include "localization.h"

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
    Group,
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

struct CardStackPlacement {
    std::wstring groupKey;
    size_t slot = 0;
    bool grouped = true;
    bool visible = true;
    float rotationRadians = 0.0f;
    float scale = 1.0f;
    float opacity = 1.0f;
    bool drawAboveStack = false;
    // The one rectangle the whole stack occupies. When a stack of two or more
    // forms, the frame takes the front card's world rectangle and every member
    // is drawn inside it, so the fan always shares a single size and centre
    // instead of every window keeping its own proportions. Empty while the
    // card stands alone, so a lone window never gets a stale frame.
    RectF frameWorld{};
};

struct StackCardTransition {
    std::wstring fingerprint;
    float fromRotation = 0.0f;
    float fromScale = 1.0f;
    float fromOpacity = 1.0f;
    float toRotation = 0.0f;
    float toScale = 1.0f;
    bool outgoing = false;
    bool incoming = false;
};

struct StackCycleTransition {
    std::wstring groupKey;
    DWORD startTick = 0;
    DWORD durationMs = 0;
    std::vector<StackCardTransition> cards;
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
    std::wstring applicationKey;
    // Hand-made stack membership, written by Alt-dropping one card onto
    // another and saved with the layout. A custom stack may span different
    // applications, so it wins over the automatic per-executable key. Empty
    // means the card follows the automatic grouping.
    std::wstring stackId;
    bool hasStackPeers = false;
    // Scan index from EnumWindows, topmost window first (0 = topmost).
    // Copied from ScannedWindow on every merge so stack order follows
    // the desktop Z-order instead of card insertion order.
    size_t zOrder = 0;
    CardStackPlacement stack;
    Thumbnail thumbnail;
    bool thumbnailFailed = false;
    DWORD thumbnailRetryTick = 0;
    RECT thumbnailDestination = {};
    bool thumbnailVisible = false;
    bool pinned = false;
};

// World rectangle a card occupies on the canvas. Every member of a formed
// stack shares the stack frame, so the fan reads as one card no matter how
// differently the member windows are sized; the front card owns the live
// preview inside that frame. Ungrouped cards and stacks without a frame use
// their own world rectangle.
inline RectF StackWorldRect(const Card& card) {
    if (card.stack.grouped && !card.stack.groupKey.empty() &&
        !card.stack.frameWorld.empty()) {
        return card.stack.frameWorld;
    }
    return card.world;
}

inline bool IsStackBackground(const Card& card) {
    return card.stack.grouped && !card.stack.groupKey.empty() &&
           card.stack.slot != 0;
}

inline bool SameActiveStack(const Card& left, const Card& right) {
    return left.stack.grouped && right.stack.grouped &&
           !left.stack.groupKey.empty() && left.stack.groupKey == right.stack.groupKey;
}

// The key that decides which stack a card belongs to. A hand-made stack wins
// over the automatic per-executable key so a custom group never leaks into the
// application-wide one.
inline std::wstring StackGroupKeyOf(const Card& card) {
    return card.stackId.empty() ? card.applicationKey : card.stackId;
}

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
    std::wstring applicationKey;
    // Scan index from EnumWindows, topmost window first (0 = topmost).
    // Stacks order members by this rank so the focused/topmost window
    // becomes the straight front card instead of an arbitrary member.
    size_t zOrder = 0;
};

inline bool MatchesWindow(const Card& card, const ScannedWindow& window) {
    return !card.destroyed && card.hwnd == window.hwnd &&
           card.processId == window.processId &&
           card.threadId == window.threadId &&
           card.windowClass == window.windowClass &&
           card.exePath == window.exePath;
}

struct CanvasSettings {
    std::wstring toggleHotkey = L"Ctrl+Alt+Shift+F12";
    LanguageId language = LanguageId::English;
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
    bool stacksEnabled = true;
    float stackStepDegrees = 6.5f;
    float stackMaxDegrees = 24.0f;
    float stackScaleFactor = 0.975f;
    int stackMaxDepth = 5;
    int stackRotationDirection = -1;
};

struct LayoutEntry {
    std::wstring fingerprint;
    RectF world = {};
    bool pinned = false;
    bool grouped = true;
    // Manual stack the card belongs to, saved so a hand-made group survives a
    // restart. Empty for cards that follow the automatic application group.
    std::wstring stackId;
};

struct LayoutSnapshot {
    std::vector<LayoutEntry> entries;
};
