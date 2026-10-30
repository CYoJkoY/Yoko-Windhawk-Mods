#pragma once

// Frame scheduling decisions for the overlay worker.

#include "platform.h"
#include "context.h"
#include "overlay.h"
#include "overlay_render.h"
#include "dpi.h"
#include "fullscreen.h"
#include "app_rules.h"
#include "history.h"
#include "interaction.h"
#include "math_utils.h"
#include "trail.h"
#include "ornaments.h"
#include "types.h"

// Pointer speed in pixels per second, derived from the real elapsed time between
// samples. The worker is event-driven while idle and samples at up to 125 Hz while
// effects are active, so a pixels-per-sample threshold would vary with the cadence.
static float PointerSpeedPxPerSecond(const POINT& point, LONGLONG nowQpc) {
    const LONGLONG elapsedTicks = nowQpc - context.lastSampleQpc;
    context.lastSampleQpc = nowQpc;
    const float dx = static_cast<float>(point.x - context.lastPos.x);
    const float dy = static_cast<float>(point.y - context.lastPos.y);
    context.lastPos = point;
    const float dtSeconds = (context.qpcFrequency > 0 && elapsedTicks > 0)
        ? static_cast<float>(elapsedTicks) / static_cast<float>(context.qpcFrequency)
        : 0.0f;
    return sqrtf(dx * dx + dy * dy) / std::max(dtSeconds, 0.001f);
}

bool SmearFrame(HWND hwnd, DWORD now, bool renderFrame) {
    POINT point = {};
    if (!GetCursorPos(&point)) return false;
    const LONGLONG sampleQpc = QpcNow();
    const HMONITOR cursorMonitor = MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST);
    if (UpdateCursorMonitorDpi(cursorMonitor)) {
        ResetTrailState();
        ResetMotionCueTracking();
        context.ornamentPhysicsInitialized = false;
        context.lastPos = point;
        context.lastSampleQpc = sampleQpc;
    }
    const HWND foreground = GetForegroundWindow();
    const int appRule = CheckAppRuleCached(foreground);
    // While the trail is suppressed the pointer still moves. Keep the previous
    // position and sample timestamp current so a later resume cannot interpret
    // the suppressed movement as one unusually fast cursor jump.
    const auto parkSample = [&] {
        context.lastPos = point;
        context.lastSampleQpc = sampleQpc;
    };

    if (appRule < 0) {
        ResetTrailState();
        ResetMotionCueTracking();
        ClearRipples();
        SuppressOrnament(sampleQpc);
        parkSample();
        HideOverlay();
        MaybeReleaseIdleBackbuffer(now, sampleQpc);
        return false;
    }

    const bool fullscreenActive = UpdateFullscreenState(now, foreground, cursorMonitor);
    if (fullscreenActive && appRule == 0) {
        ResetTrailState();
        ResetMotionCueTracking();
        ClearRipples();
        SuppressOrnament(sampleQpc);
        parkSample();
        HideOverlay();
        MaybeReleaseIdleBackbuffer(now, sampleQpc);
        return false;
    }

    const LONGLONG sampleElapsedTicks = sampleQpc - context.lastSampleQpc;
    const LONGLONG resumeGapTicks = std::max<LONGLONG>(
        1, context.qpcFrequency * static_cast<LONGLONG>(kMotionResumeGapMs) / 1000);
    if (context.lastSampleQpc <= 0
        || sampleElapsedTicks <= 0
        || sampleElapsedTicks > resumeGapTicks) {
        // Initialize or rebase instead of inferring speed across an invalid sample interval.
        ResetTrailState();
        ResetMotionCueTracking();
        context.lastPos = point;
        context.lastSampleQpc = sampleQpc;
    }

    const bool wasSmearing = context.isSmearing;
    const int previousHistoryCount = context.historyCount;
    const POINT previousPoint = context.lastPos;
    const float velocity = PointerSpeedPxPerSecond(point, sampleQpc);
    UpdateMotionCueTracking(point, sampleQpc);
    UpdateTrailState(previousPoint, point, velocity);
    UpdateOrnamentPhysics(velocity, sampleQpc);
    MaybeReleaseIdleBackbuffer(now, sampleQpc);
    if (context.isSmearing || context.historyCount >= 2) UpdateTrailColorIfNeeded(now, point);

    const bool stateChanged = wasSmearing != context.isSmearing
        || (previousHistoryCount >= 2) != (context.historyCount >= 2);
    const bool hasRipples = HasActiveRipples(sampleQpc);
    const bool hasMotionAccents = HasActiveMotionAccents(sampleQpc);
    const bool ornamentVisible = OrnamentVisible();
    if (context.historyCount < 2 && !hasRipples && !hasMotionAccents && !ornamentVisible) {
        HideOverlay();
        return false;
    }
    const bool ornamentRedrawDue = ornamentVisible
        && context.ornamentRedrawPending
        && sampleQpc >= context.ornamentNextRenderQpc;
    if (!renderFrame && !stateChanged && !ornamentRedrawDue) return false;
    if (!RenderOverlay(hwnd, sampleQpc)) {
        HideOverlay();
        return false;
    }
    context.ornamentRedrawPending = false;
    if (context.qpcFrequency > 0) {
        context.ornamentNextRenderQpc = sampleQpc
            + std::max<LONGLONG>(1, context.qpcFrequency / kActiveRenderFrameRate);
    }
    ShowOverlay();
    return true;
}
