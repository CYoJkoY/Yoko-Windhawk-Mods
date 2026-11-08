#pragma once

#include "platform.h"
#include "context.h"
#include "windows.h"

// Moving windows without resizing, restacking, or activating them keeps the
// foreground window, focus, and z-order exactly as the user left them.
static constexpr UINT kWindowPositionFlags =
    SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER |
    SWP_NOSENDCHANGING;

static void ApplyCanvasOffset() {
    for (const ManagedWindow& item : context.windows) {
        if (!IsWindow(item.hwnd)) continue;
        const int x = item.origin.left + context.canvasOffset.x;
        const int y = item.origin.top + context.canvasOffset.y;
        SetWindowPos(item.hwnd, nullptr, x, y, 0, 0, kWindowPositionFlags);
    }
}

// Records window positions that changed outside the canvas: a user drag, a
// snap layout, a keyboard move, or another tool. The window's virtual origin is
// rebased so the next canvas update moves it relative to its new place.
static void SyncExternalMoves() {
    for (ManagedWindow& item : context.windows) {
        RECT actual = {};
        if (!IsWindow(item.hwnd) || !GetWindowRect(item.hwnd, &actual)) continue;
        // Leave a stale origin on a window that turned fullscreen, so the next
        // reconcile pass can tell it apart from a canvas-placed window.
        if (CoversMonitor(item.hwnd)) continue;
        const int expectedX = item.origin.left + context.canvasOffset.x;
        const int expectedY = item.origin.top + context.canvasOffset.y;
        if (actual.left == expectedX && actual.top == expectedY) continue;
        actual.left -= context.canvasOffset.x;
        actual.right -= context.canvasOffset.x;
        actual.top -= context.canvasOffset.y;
        actual.bottom -= context.canvasOffset.y;
        item.origin = actual;
    }
}

static void ResetCanvas() {
    context.canvasOffset = {};
    context.edgeActive = false;
    context.panning = false;
    ApplyCanvasOffset();
}

static void ClampOffset() {
    if (!context.constrainBounds || context.windows.empty()) return;
    RECT unionRect = context.windows.front().origin;
    for (size_t index = 1; index < context.windows.size(); ++index) {
        const RECT& rect = context.windows[index].origin;
        unionRect.left = std::min(unionRect.left, rect.left);
        unionRect.top = std::min(unionRect.top, rect.top);
        unionRect.right = std::max(unionRect.right, rect.right);
        unionRect.bottom = std::max(unionRect.bottom, rect.bottom);
    }
    const LONG minX = context.monitorRect.left - unionRect.right;
    const LONG maxX = context.monitorRect.right - unionRect.left;
    const LONG minY = context.monitorRect.top - unionRect.bottom;
    const LONG maxY = context.monitorRect.bottom - unionRect.top;
    context.canvasOffset.x = std::clamp<LONG>(context.canvasOffset.x, minX, maxX);
    context.canvasOffset.y = std::clamp<LONG>(context.canvasOffset.y, minY, maxY);
}

// Centers one managed window on the active monitor in a single step. The edge
// dwell state is cleared so the canvas does not resume panning just because the
// pointer still happens to rest near an edge.
static void JumpToWindow(HWND hwnd) {
    size_t index = 0;
    if (!FindManagedWindow(hwnd, index)) return;
    const RECT& origin = context.windows[index].origin;
    const LONG centerX = context.monitorRect.left
        + (context.monitorRect.right - context.monitorRect.left) / 2;
    const LONG centerY = context.monitorRect.top
        + (context.monitorRect.bottom - context.monitorRect.top) / 2;
    context.canvasOffset.x = centerX - (origin.left + (origin.right - origin.left) / 2);
    context.canvasOffset.y = centerY - (origin.top + (origin.bottom - origin.top) / 2);
    context.edgeActive = false;
    context.panning = false;
    context.lastTick = GetTickCount();
    ClampOffset();
    ApplyCanvasOffset();
}

// Reports whether this monitor edge faces empty desktop space. A one-pixel
// probe just outside the edge lands on a neighbor monitor when the edge is
// shared, in which case moving the pointer already switches displays and the
// canvas must not pan.
static bool IsOuterEdge(Edge edge, POINT cursor) {
    POINT probe = cursor;
    switch (edge) {
        case EDGE_LEFT:
            probe.x = context.monitorRect.left - 1;
            break;
        case EDGE_TOP:
            probe.y = context.monitorRect.top - 1;
            break;
        case EDGE_RIGHT:
            probe.x = context.monitorRect.right;
            break;
        case EDGE_BOTTOM:
            probe.y = context.monitorRect.bottom;
            break;
    }
    const HMONITOR adjacent = MonitorFromPoint(probe, MONITOR_DEFAULTTONULL);
    return adjacent == nullptr || adjacent == context.monitor;
}

static bool ReadEdgeVector(POINT cursor, float& x, float& y) {
    x = 0.0f;
    y = 0.0f;
    const float distance = static_cast<float>(context.edgeDistance);
    const float left = static_cast<float>(context.monitorRect.left);
    const float top = static_cast<float>(context.monitorRect.top);
    const float right = static_cast<float>(context.monitorRect.right);
    const float bottom = static_cast<float>(context.monitorRect.bottom);
    if (static_cast<float>(cursor.x) - left < distance && IsOuterEdge(EDGE_LEFT, cursor)) {
        x = (distance - (static_cast<float>(cursor.x) - left)) / distance;
    } else if (right - static_cast<float>(cursor.x) < distance && IsOuterEdge(EDGE_RIGHT, cursor)) {
        x = -(distance - (right - static_cast<float>(cursor.x))) / distance;
    }
    if (static_cast<float>(cursor.y) - top < distance && IsOuterEdge(EDGE_TOP, cursor)) {
        y = (distance - (static_cast<float>(cursor.y) - top)) / distance;
    } else if (bottom - static_cast<float>(cursor.y) < distance && IsOuterEdge(EDGE_BOTTOM, cursor)) {
        y = -(distance - (bottom - static_cast<float>(cursor.y))) / distance;
    }
    return x != 0.0f || y != 0.0f;
}

// Builds a fresh layout for the monitor under the pointer. Windows that were
// being panned on the previous monitor are first returned to their origins,
// so switching displays never strands a layout off-screen.
static void StartLayout(POINT cursor) {
    if (context.layoutReady) {
        context.canvasOffset = {};
        ApplyCanvasOffset();
    }
    GetCurrentMonitor(cursor);
    context.windows.clear();
    context.canvasOffset = {};
    context.layoutReady = true;
    context.edgeActive = false;
    context.panning = false;
    context.lastTick = 0;
    context.lastReconcile = GetTickCount();
    ReconcileWindows();
}

static void TickCanvas() {
    if (context.pickerOpen) return;
    POINT cursor = {};
    if (!GetCursorPos(&cursor)) return;
    const HMONITOR cursorMonitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);

    const DWORD now = GetTickCount();
    const DWORD elapsed = context.lastTick == 0 ? 0 : now - context.lastTick;
    context.lastTick = now;
    if (now - context.lastReconcile >= 500) {
        ReconcileWindows();
        context.lastReconcile = now;
    }

    // Clicking or dragging means the user is working with a window right now.
    // Panning pauses until the button is released, and the release tick
    // resynchronizes the origins of every window that was moved.
    const bool buttonDown =
        (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0 ||
        (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0 ||
        (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
    if (buttonDown) {
        context.edgeActive = false;
        context.panning = false;
        return;
    }
    SyncExternalMoves();

    if (context.resetRequested) {
        context.resetRequested = false;
        ResetCanvas();
        return;
    }
    if (!context.layoutReady || cursorMonitor != context.monitor) {
        // Reconcile before restoring so a window the user just dragged to the
        // neighboring monitor is dropped from the old layout, not snapped back.
        if (context.layoutReady) ReconcileWindows();
        StartLayout(cursor);
    }
    if (context.windows.empty()) {
        context.edgeActive = false;
        context.panning = false;
        return;
    }

    float edgeX = 0.0f;
    float edgeY = 0.0f;
    if (!ReadEdgeVector(cursor, edgeX, edgeY)) {
        context.edgeActive = false;
        context.panning = false;
        return;
    }
    if (!context.edgeActive) {
        context.edgeActive = true;
        context.panning = false;
        context.edgeSince = now;
        return;
    }
    if (now - context.edgeSince < static_cast<DWORD>(context.dwellTime) || elapsed == 0) return;

    const float magnitude = std::sqrt(edgeX * edgeX + edgeY * edgeY);
    if (magnitude == 0.0f) return;
    const float easingPower = 1.0f + static_cast<float>(context.easing) / 50.0f;
    // Proximity alone leaves the pan crawling right after the dwell expires.
    // A minimum share keeps the pointer responsive near the trigger boundary,
    // while the full speed still applies at the physical edge.
    const float proximity = std::min(1.0f, magnitude);
    const float strength = std::pow(0.35f + 0.65f * proximity, easingPower);
    const float seconds = std::min(0.1f, static_cast<float>(elapsed) / 1000.0f);
    context.canvasOffset.x += static_cast<int>(
        std::lround(edgeX / magnitude * context.maxSpeed * strength * seconds));
    context.canvasOffset.y += static_cast<int>(
        std::lround(edgeY / magnitude * context.maxSpeed * strength * seconds));
    context.panning = true;
    ClampOffset();
    ApplyCanvasOffset();
}
