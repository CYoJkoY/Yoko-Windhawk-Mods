#pragma once

// Card creation, packing and snapping.
//
// Arranging cards never moves a real window. A card keeps its own position in
// world space, and dive-in solves for the camera state that projects the card
// back onto the window's real desktop rectangle. That keeps the user's desktop
// arrangement untouched and works for elevated windows the mod cannot move.
// The one deliberate move lives in interaction.h: activating an other-desktop
// card pulls that window onto the desktop that displays the canvas.

#include "platform.h"
#include "card_stacks.h"
#include "constants.h"
#include "context.h"
#include "layout_store.h"
#include "math_utils.h"
#include "text_utils.h"

#include "types.h"

#include "windows.h"

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
    card.zOrder = window.zOrder;
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
                         !EqualRect(&card.framePx, &window.framePx) ||
                         card.zOrder != window.zOrder;
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
    card.zOrder = window.zOrder;
    if (const LayoutEntry* saved = FindLayoutEntry(window.fingerprint);
        saved != nullptr) {
        card.pinned = saved->pinned;
        card.stack.grouped = saved->grouped;
    }
    return changed;
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
        std::stable_sort(additions.begin(), additions.end(),
                         [](const ScannedWindow& left, const ScannedWindow& right) {
                             return CardArea(left) > CardArea(right);
                         });
        const DWORD now = GetTickCount();
        for (const ScannedWindow& window : additions) {
            const LayoutEntry* saved = FindLayoutEntry(window.fingerprint);
            RectF world = saved != nullptr ? saved->world : InitialWorldFor(window);
            world.right = world.left + static_cast<float>(window.framePx.right - window.framePx.left);
            world.bottom = world.top + static_cast<float>(window.framePx.bottom - window.framePx.top);
            InsertCard(ScannedWindow{window}, world);
            if (saved != nullptr) {
                context.cards.back().pinned = saved->pinned;
                context.cards.back().stack.grouped = saved->grouped;
            }
            RememberLayoutEntry(window.fingerprint, world);
            changed = true;
        }
        if (changed) {
            MarkLayoutDirty(now);
        }
    }
    if (changed) {
        ReconcileCardStacks(context.cards, context.settings);
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
        SnapAgainstCard(moving, StackWorldRect(context.cards[index]), axes);
    }
    result.snapVertical = axes.first.snapped;
    result.snapHorizontal = axes.second.snapped;
    result.guideX = axes.first.guide;
    result.guideY = axes.second.guide;
    result.left += result.snapVertical ? axes.first.shift : 0.0f;
    result.top += result.snapHorizontal ? axes.second.shift : 0.0f;
    return result;
}
