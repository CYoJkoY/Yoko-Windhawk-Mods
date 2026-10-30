#pragma once

// Settings loading and the settings-changed application path.

#include "platform.h"
#include "context.h"
#include "math_utils.h"
#include "ornament_settings.h"
#include "app_rules.h"
#include "overlay.h"
#include "history.h"
#include "forward.h"
#include "types.h"

void LoadSettings() {
    context.triggerVelocity = static_cast<float>(yoko::GetClampedIntSetting(L"Behavior.trigger_velocity", 50, 20000));
    context.stopVelocity = static_cast<float>(yoko::GetClampedIntSetting(L"Behavior.stop_velocity", 50, 20000));
    if (context.stopVelocity >= context.triggerVelocity) context.stopVelocity = std::max(50.0f, context.triggerVelocity * 0.5f);
    context.tailOffsetX = yoko::GetClampedIntSetting(L"Behavior.tail_offset_x", -64, 64);
    context.tailOffsetY = yoko::GetClampedIntSetting(L"Behavior.tail_offset_y", -64, 64);
    context.tailLength = yoko::GetClampedIntSetting(L"Behavior.tail_length", 2, kMaxTailLength);
    context.speedScaling = yoko::GetClampedIntSetting(L"Behavior.speed_scaling", 0, 1);
    context.widthMin = static_cast<float>(yoko::GetClampedIntSetting(L"Appearance.width_min", 1, 40));
    context.widthMax = static_cast<float>(yoko::GetClampedIntSetting(L"Appearance.width_max", 1, 60));
    context.coreWidthMin = static_cast<float>(yoko::GetClampedIntSetting(L"Appearance.core_width_min", 1, 40));
    context.coreWidthMax = static_cast<float>(yoko::GetClampedIntSetting(L"Appearance.core_width_max", 1, 60));
    context.alphaMin = yoko::GetClampedIntSetting(L"Appearance.alpha_min", 0, 100) / 100.0f;
    context.alphaMax = yoko::GetClampedIntSetting(L"Appearance.alpha_max", 0, 100) / 100.0f;
    context.taperPowerIndex = yoko::GetClampedIntSetting(L"Appearance.taper_power", kTaperPowerSettingMin, kTaperPowerSettingMax);
    if (context.widthMax < context.widthMin) std::swap(context.widthMin, context.widthMax);
    if (context.coreWidthMax < context.coreWidthMin) std::swap(context.coreWidthMin, context.coreWidthMax);
    context.smoothIterations = yoko::GetClampedIntSetting(L"Appearance.smooth_iterations", 0, kMaxSmoothIterations);
    context.gradientEnabled = yoko::GetClampedIntSetting(L"Effects.gradient_enabled", 0, 1);
    {
        uint32_t parsed = 0;
        WindhawkUtils::StringSetting value =
            WindhawkUtils::StringSetting::make(L"Effects.gradient_tail_color");
        context.gradientTailRGB = ParseHexColor(value.get(), parsed) ? parsed : 0x00FF00FF;
    }
    context.glowEnabled = yoko::GetClampedIntSetting(L"Effects.glow_enabled", 0, 1);
    {
        uint32_t parsed = 0;
        WindhawkUtils::StringSetting value =
            WindhawkUtils::StringSetting::make(L"Effects.glow_color");
        context.glowRGB = ParseHexColor(value.get(), parsed) ? parsed : 0x00FFFFFF;
    }
    context.glowWidthFactor = yoko::GetClampedIntSetting(L"Effects.glow_width_factor", 10, 30) / 10.0f;
    context.glowAlpha = yoko::GetClampedIntSetting(L"Effects.glow_alpha", 0, 100) / 100.0f;
    context.fadeEnabled = yoko::GetClampedIntSetting(L"Behavior.fade_enabled", 0, 1);
    context.fadeDecay = yoko::GetClampedIntSetting(L"Behavior.fade_decay", 50, 99) / 100.0f;
    {
        WindhawkUtils::StringSetting value =
            WindhawkUtils::StringSetting::make(L"Color.trail_color_mode");
        context.trailColorMode = _wcsicmp(value.get(), L"auto") == 0 ? 1 : 0;
    }
    {
        uint32_t parsed = 0;
        WindhawkUtils::StringSetting value =
            WindhawkUtils::StringSetting::make(L"Color.trail_color_manual");
        context.manualColorRGB = ParseHexColor(value.get(), parsed) ? parsed : kFallbackCoreColor;
    }
    {
        WindhawkUtils::StringSetting value =
            WindhawkUtils::StringSetting::make(L"Color.outline_color_mode");
        context.outlineColorMode = _wcsicmp(value.get(), L"manual") == 0 ? 1 : 0;
    }
    {
        uint32_t parsed = 0;
        WindhawkUtils::StringSetting value =
            WindhawkUtils::StringSetting::make(L"Color.outline_color_manual");
        context.manualOutlineRGB = ParseHexColor(value.get(), parsed) ? parsed : kFallbackOuterColor;
    }
    context.autoResampleInterval = yoko::GetClampedIntSetting(L"Color.auto_resample_interval", 0, kAutoResampleIntervalMaxMs);
    context.clickFeedbackEnabled = yoko::GetClampedIntSetting(L"Interaction.click_feedback_enabled", 0, 1);
    context.wheelFeedbackEnabled = yoko::GetClampedIntSetting(L"Interaction.wheel_feedback_enabled", 0, 1);
    if (!context.clickFeedbackEnabled && !context.wheelFeedbackEnabled) ClearRipples();
    context.rippleRadius = static_cast<float>(yoko::GetClampedIntSetting(L"Interaction.ripple_radius", 10, 200));
    context.wheelArrowSize = static_cast<float>(yoko::GetClampedIntSetting(L"Interaction.wheel_arrow_size", 6, 40));
    context.wheelArrowDistance = static_cast<float>(yoko::GetClampedIntSetting(L"Interaction.wheel_arrow_distance", 8, 120));
    context.rippleDurationMs = yoko::GetClampedIntSetting(L"Interaction.ripple_duration_ms", 100, 2000);
    context.rippleStrokeWidth = static_cast<float>(yoko::GetClampedIntSetting(L"Interaction.ripple_width", 1, 10));
    context.rippleOpacity = yoko::GetClampedIntSetting(L"Interaction.ripple_opacity", 5, 100) / 100.0f;
    const auto readInteractionColor = [](const wchar_t* settingPath, uint32_t fallbackColor) {
        uint32_t parsedColor = 0;
        WindhawkUtils::StringSetting value = WindhawkUtils::StringSetting::make(settingPath);
        return ParseHexColor(value.get(), parsedColor) ? parsedColor : fallbackColor;
    };
    context.leftClickRGB = readInteractionColor(L"Interaction.left_click_color", 0x0000D7FF);
    context.rightClickRGB = readInteractionColor(L"Interaction.right_click_color", 0x00FF4C8B);
    context.middleClickRGB = readInteractionColor(L"Interaction.middle_click_color", 0x00FFD166);
    context.wheelUpRGB = readInteractionColor(L"Interaction.wheel_up_color", 0x007CFF6B);
    context.wheelDownRGB = readInteractionColor(L"Interaction.wheel_down_color", 0x00A78BFA);
    context.accelerationAccentEnabled = yoko::GetClampedIntSetting(L"Motion.acceleration_accent_enabled", 0, 1);
    context.turnAccentEnabled = yoko::GetClampedIntSetting(L"Motion.turn_accent_enabled", 0, 1);
    context.fastMoveInterpolationEnabled = yoko::GetClampedIntSetting(L"Motion.fast_move_interpolation", 0, 1);
    context.accelerationThreshold = yoko::GetClampedIntSetting(L"Motion.acceleration_threshold", 3000, 100000);
    context.motionAccentOpacity = yoko::GetClampedIntSetting(L"Motion.motion_accent_opacity", 10, 100) / 100.0f;
    context.turnAngleDegrees = yoko::GetClampedIntSetting(L"Motion.turn_angle_degrees", 45, 150);
    context.turnAccentCosThreshold = cosf(
        static_cast<float>(context.turnAngleDegrees) * kDegreesToRadians);
    context.motionAccentDurationMs = yoko::GetClampedIntSetting(L"Motion.motion_accent_duration_ms", 80, 1000);
    context.motionAccentSize = static_cast<float>(yoko::GetClampedIntSetting(L"Motion.motion_accent_size", 4, 40));
    context.maxInterpolationSteps = yoko::GetClampedIntSetting(L"Motion.max_interpolation_steps", 1, 6);
    context.motionAccentRGB = readInteractionColor(L"Motion.motion_accent_color", 0x00FFD166);
    context.ornamentEnabled = yoko::GetClampedIntSetting(L"Ornament.enabled", 0, 1);
    OrnamentPreset ornamentPreset = OrnamentPreset::Classic;
    {
        WindhawkUtils::StringSetting value =
            WindhawkUtils::StringSetting::make(L"Ornament.preset");
        ornamentPreset = ParseOrnamentPreset(value.get());
    }
    // A settings blob written before this key existed reads as 0, so fall back
    // to the preset length instead of clamping to the shortest rope.
    const int ropeLengthScalePct =
        Wh_GetIntSetting(L"Ornament.rope_length_scale");
    context.ornamentRopeLengthScale =
        std::clamp(ropeLengthScalePct > 0 ? ropeLengthScalePct : 100, 25, 400) / 100.0f;
    LoadOrnamentImageSpec();
    ApplyOrnamentPreset(ornamentPreset);
    context.ornamentAssetsDirty.store(true, std::memory_order_release);
    ResetMotionCueTracking();
    ParseAppRules();
    context.cachedForegroundWindow = nullptr;
    context.cachedAppRule = 0;
    context.autoColorInitialized = false;
    context.lastAutoColorCursor = nullptr;
    context.lastAutoColorUpdate = 0;
    if (context.trailColorMode == 0) {
        SetTrailCoreColor(context.manualColorRGB, RgbLuminance(context.manualColorRGB));
    } else {
        SetTrailCoreColor(context.currentCoreRGB, context.currentCoreLuminance);
    }
    ApplyOverlayCaptureAffinity();
}
