#pragma once

// Click ripples, motion accents and their state machines.

#include "platform.h"
#include "context.h"
#include "math_utils.h"
#include "types.h"

static void ClearRipples() {
    for (RippleEffect& ripple : context.ripples) {
        ripple.active = false;
    }
    context.nextRippleSlot = 0;
    context.interactionPending = false;
}

static void ClearMotionAccents() {
    for (MotionAccent& accent : context.motionAccents) accent.active = false;
    context.nextMotionAccentSlot = 0;
}

static void ResetMotionCueTracking() {
    ClearMotionAccents();
    context.lastMotionCuePoint = {};
    context.lastMotionCueQpc = 0;
    context.lastMotionCueSpeed = 0.0f;
    context.lastMotionDirectionX = 0.0f;
    context.lastMotionDirectionY = 0.0f;
    context.hasPreviousMotionCueSample = false;
    context.accelerationWindowPoint = {};
    context.accelerationWindowQpc = 0;
    context.accelerationWindowSpeed = 0.0f;
    context.hasAccelerationWindowSample = false;
    context.hasAccelerationWindowSpeed = false;
    context.lastAccelerationAccentQpc = 0;
    context.lastTurnAccentQpc = 0;
}

static void AddMotionAccent(
    const POINT& center,
    float directionX,
    float directionY,
    float intensity,
    MotionAccentKind kind,
    float previousDirectionX = 0.0f,
    float previousDirectionY = 0.0f) {
    int selectedSlot = -1;
    for (int offset = 0; offset < kMaxActiveMotionAccents; ++offset) {
        const int candidateSlot = (context.nextMotionAccentSlot + offset) % kMaxActiveMotionAccents;
        if (!context.motionAccents[candidateSlot].active) {
            selectedSlot = candidateSlot;
            break;
        }
    }
    if (selectedSlot < 0) selectedSlot = context.nextMotionAccentSlot;
    context.nextMotionAccentSlot = (selectedSlot + 1) % kMaxActiveMotionAccents;

    MotionAccent& accent = context.motionAccents[selectedSlot];
    accent.center = center;
    accent.directionX = directionX;
    accent.directionY = directionY;
    accent.previousDirectionX = previousDirectionX;
    accent.previousDirectionY = previousDirectionY;
    accent.intensity = std::clamp(intensity, 0.5f, 1.8f);
    accent.startQpc = QpcNow();
    accent.kind = kind;
    accent.active = true;
}

static int UpdateMotionAccentState(LONGLONG nowQpc) {
    const LONGLONG durationTicks = std::max<LONGLONG>(
        1, context.qpcFrequency * static_cast<LONGLONG>(context.motionAccentDurationMs) / 1000);
    int activeCount = 0;
    for (MotionAccent& accent : context.motionAccents) {
        if (!accent.active) continue;
        const LONGLONG elapsedTicks = nowQpc - accent.startQpc;
        if (elapsedTicks < 0 || elapsedTicks >= durationTicks) {
            accent.active = false;
            continue;
        }
        ++activeCount;
    }
    return activeCount;
}

static bool HasActiveMotionAccents(LONGLONG nowQpc) {
    return UpdateMotionAccentState(nowQpc) > 0;
}

static void UpdateMotionCueTracking(const POINT& point, LONGLONG nowQpc) {
    if (!context.accelerationAccentEnabled && !context.turnAccentEnabled) {
        context.lastMotionCuePoint = point;
        context.lastMotionCueQpc = nowQpc;
        context.lastMotionCueSpeed = 0.0f;
        context.lastMotionDirectionX = 0.0f;
        context.lastMotionDirectionY = 0.0f;
        context.hasPreviousMotionCueSample = false;
        context.accelerationWindowPoint = point;
        context.accelerationWindowQpc = nowQpc;
        context.accelerationWindowSpeed = 0.0f;
        context.hasAccelerationWindowSample = false;
        context.hasAccelerationWindowSpeed = false;
        return;
    }

    if (!context.hasPreviousMotionCueSample) {
        context.lastMotionCuePoint = point;
        context.lastMotionCueQpc = nowQpc;
        context.lastMotionCueSpeed = 0.0f;
        context.lastMotionDirectionX = 0.0f;
        context.lastMotionDirectionY = 0.0f;
        context.hasPreviousMotionCueSample = true;
        context.accelerationWindowPoint = point;
        context.accelerationWindowQpc = nowQpc;
        context.accelerationWindowSpeed = 0.0f;
        context.hasAccelerationWindowSample = true;
        context.hasAccelerationWindowSpeed = false;
        return;
    }

    const LONGLONG elapsedTicks = nowQpc - context.lastMotionCueQpc;
    const LONGLONG resumeGapTicks = std::max<LONGLONG>(
        1, context.qpcFrequency * static_cast<LONGLONG>(kMotionResumeGapMs) / 1000);
    if (elapsedTicks <= 0 || elapsedTicks > resumeGapTicks) {
        // Rebase after idle; do not interpret time spent idle as acceleration or a turn.
        context.lastMotionCuePoint = point;
        context.lastMotionCueQpc = nowQpc;
        context.lastMotionCueSpeed = 0.0f;
        context.lastMotionDirectionX = 0.0f;
        context.lastMotionDirectionY = 0.0f;
        context.hasPreviousMotionCueSample = true;
        context.accelerationWindowPoint = point;
        context.accelerationWindowQpc = nowQpc;
        context.accelerationWindowSpeed = 0.0f;
        context.hasAccelerationWindowSample = true;
        context.hasAccelerationWindowSpeed = false;
        return;
    }

    const float dx = static_cast<float>(point.x - context.lastMotionCuePoint.x);
    const float dy = static_cast<float>(point.y - context.lastMotionCuePoint.y);
    const float distance = sqrtf(dx * dx + dy * dy);
    const float elapsedSeconds = static_cast<float>(elapsedTicks)
        / static_cast<float>(std::max<LONGLONG>(1, context.qpcFrequency));
    const float speed = distance / std::max(elapsedSeconds, 0.001f);
    if (distance >= 2.0f) {
        const float directionX = dx / distance;
        const float directionY = dy / distance;
        const LONGLONG turnCooldown = std::max<LONGLONG>(
            1, context.qpcFrequency * static_cast<LONGLONG>(kTurnAccentCooldownMs) / 1000);

        const float previousDirectionLength = sqrtf(
            context.lastMotionDirectionX * context.lastMotionDirectionX
            + context.lastMotionDirectionY * context.lastMotionDirectionY);
        if (context.turnAccentEnabled
            && context.lastMotionCueSpeed >= context.triggerVelocity
            && speed >= context.triggerVelocity
            && previousDirectionLength > 0.5f
            && (context.lastTurnAccentQpc == 0
                || nowQpc - context.lastTurnAccentQpc >= turnCooldown)) {
            const float dot = std::clamp(
                context.lastMotionDirectionX * directionX + context.lastMotionDirectionY * directionY,
                -1.0f,
                1.0f);
            if (dot <= context.turnAccentCosThreshold) {
                AddMotionAccent(
                    point,
                    directionX,
                    directionY,
                    std::clamp((1.0f - dot) * 0.9f, 0.5f, 1.8f),
                    MotionAccentKind::Turn,
                    context.lastMotionDirectionX,
                    context.lastMotionDirectionY);
                context.lastTurnAccentQpc = nowQpc;
            }
        }

        context.lastMotionDirectionX = directionX;
        context.lastMotionDirectionY = directionY;
    }

    // Compare average velocities over longer, bounded windows instead of differentiating
    // every raw-input wake, whose report batching can make adjacent speeds oscillate.
    const LONGLONG accelerationWindowTicks = std::max<LONGLONG>(
        1, context.qpcFrequency
            * static_cast<LONGLONG>(kAccelerationMeasurementWindowMs) / 1000);
    if (context.accelerationAccentEnabled
        && context.hasAccelerationWindowSample
        && nowQpc - context.accelerationWindowQpc >= accelerationWindowTicks) {
        const LONGLONG windowElapsedTicks = nowQpc - context.accelerationWindowQpc;
        const float windowElapsedSeconds = static_cast<float>(windowElapsedTicks)
            / static_cast<float>(std::max<LONGLONG>(1, context.qpcFrequency));
        const float windowDx = static_cast<float>(point.x - context.accelerationWindowPoint.x);
        const float windowDy = static_cast<float>(point.y - context.accelerationWindowPoint.y);
        const float windowDistance = sqrtf(windowDx * windowDx + windowDy * windowDy);
        const float windowSpeed = windowDistance / std::max(windowElapsedSeconds, 0.001f);

        if (context.hasAccelerationWindowSpeed && windowDistance >= 2.0f) {
            const float acceleration =
                (windowSpeed - context.accelerationWindowSpeed)
                / std::max(windowElapsedSeconds, 0.001f);
            const LONGLONG accelerationCooldown = std::max<LONGLONG>(
                1, context.qpcFrequency
                    * static_cast<LONGLONG>(kAccelerationAccentCooldownMs) / 1000);
            if (windowSpeed >= context.triggerVelocity
                && acceleration >= static_cast<float>(context.accelerationThreshold)
                && (context.lastAccelerationAccentQpc == 0
                    || nowQpc - context.lastAccelerationAccentQpc >= accelerationCooldown)) {
                const float directionX = windowDx / windowDistance;
                const float directionY = windowDy / windowDistance;
                AddMotionAccent(
                    point,
                    directionX,
                    directionY,
                    acceleration / static_cast<float>(context.accelerationThreshold),
                    MotionAccentKind::Acceleration);
                context.lastAccelerationAccentQpc = nowQpc;
            }
        }

        context.accelerationWindowPoint = point;
        context.accelerationWindowQpc = nowQpc;
        context.accelerationWindowSpeed = windowSpeed;
        context.hasAccelerationWindowSample = true;
        context.hasAccelerationWindowSpeed = true;
    }

    context.lastMotionCuePoint = point;
    context.lastMotionCueQpc = nowQpc;
    context.lastMotionCueSpeed = speed;
}

static int UpdateRippleState(LONGLONG nowQpc) {
    const LONGLONG durationTicks = std::max<LONGLONG>(
        1, context.qpcFrequency * static_cast<LONGLONG>(context.rippleDurationMs) / 1000);
    int activeCount = 0;
    for (RippleEffect& ripple : context.ripples) {
        if (!ripple.active) continue;
        const LONGLONG elapsedTicks = nowQpc - ripple.startQpc;
        if (elapsedTicks >= durationTicks || elapsedTicks < 0) {
            ripple.active = false;
            continue;
        }
        ++activeCount;
    }
    return activeCount;
}

static bool HasActiveRipples(LONGLONG nowQpc) {
    return UpdateRippleState(nowQpc) > 0;
}

static void AddRipple(const POINT& center, uint32_t color, RippleKind kind = RippleKind::Click) {
    int selectedSlot = -1;
    for (int offset = 0; offset < kMaxActiveRipples; ++offset) {
        const int candidateSlot = (context.nextRippleSlot + offset) % kMaxActiveRipples;
        if (!context.ripples[candidateSlot].active) {
            selectedSlot = candidateSlot;
            break;
        }
    }
    if (selectedSlot < 0) selectedSlot = context.nextRippleSlot;
    context.nextRippleSlot = (selectedSlot + 1) % kMaxActiveRipples;

    RippleEffect& ripple = context.ripples[selectedSlot];
    ripple.center = center;
    ripple.startQpc = QpcNow();
    ripple.color = color;
    ripple.kind = kind;
    ripple.active = true;
    context.interactionPending = true;
    if (context.inputWakeEvent) SetEvent(context.inputWakeEvent);
}
