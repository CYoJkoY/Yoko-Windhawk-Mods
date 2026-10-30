#pragma once

// Compile-time tuning values shared by the trail, ripple and ornament code.

#include "platform.h"

constexpr UINT kSettingsChangedMessage = WM_APP + 1;
constexpr int kTargetFrameRate = 125;
constexpr int kActiveRenderFrameRate = 60;
constexpr int kFadeRenderFrameRate = 30;
constexpr DWORD kInputWakeThrottleMs = 8;
constexpr DWORD kFullscreenStrongCheckIntervalMs = 100;
constexpr DWORD kFullscreenExitConfirmMs = 500;
constexpr LONG kFullscreenTolerancePx = 2;
constexpr int kMaxTailLength = 64;
constexpr int kHistoryCapacity = kMaxTailLength;
constexpr int kMaxSmoothIterations = 4;
// Chaikin doubles the point count per pass, including both duplicated endpoints.
constexpr int kMaxTrailRenderPoints = kMaxTailLength << kMaxSmoothIterations;
constexpr int kTaperPowerSettingMin = 5;
constexpr int kTaperPowerSettingMax = 30;
constexpr int kTaperPowerCount = kTaperPowerSettingMax - kTaperPowerSettingMin + 1;
constexpr int kTaperLutSteps = 256;
// One filled ribbon replaces a stroke per trail segment. The center point
// budget bounds tessellation work for long trails, and the spacing rejects
// duplicate samples whose direction would be undefined.
constexpr int kTrailRibbonMaxCenterPoints = 192;
constexpr int kTrailRibbonCapSegments = 6;
constexpr int kTrailRibbonMaxOutlinePoints =
    kTrailRibbonMaxCenterPoints * 2 + kTrailRibbonCapSegments * 2;
constexpr float kTrailRibbonMinSpacingPx = 0.35f;
constexpr float kTrailRibbonMinHalfWidth = 0.05f;
// Unit-circle samples for the intermediate tail cap vertices. The cap spans
// half a circle in kTrailRibbonCapSegments equal steps; the two endpoints are
// already present as the last left and right side vertices.
constexpr float kTrailRibbonCapCos[kTrailRibbonCapSegments - 1] = {
    0.86602540f, 0.5f, 0.0f, -0.5f, -0.86602540f};
constexpr float kTrailRibbonCapSin[kTrailRibbonCapSegments - 1] = {
    0.5f, 0.86602540f, 1.0f, 0.86602540f, 0.5f};
static_assert(
    kTrailRibbonCapSegments == 6,
    "The cap sample tables encode a six-segment half circle");
constexpr int kRenderPadding = 2;
constexpr float kFadeCutoff = 0.05f;
constexpr int kAutoResampleIntervalMaxMs = 10000;
constexpr int kBackgroundSampleRadius = 24;
constexpr int kBackgroundSampleGrid = 5;
constexpr DWORD kBackgroundResampleIntervalMs = 50;
constexpr LONG kBackgroundResampleDistancePx = 20;
constexpr float kMinColorContrast = 3.0f;
constexpr float kAutoColorHysteresisContrast = 2.5f;
constexpr DWORD kFallbackActivePollIntervalMs = 10;
constexpr DWORD kFallbackIdlePollIntervalMs = 100;
constexpr DWORD kFallbackIdleDelayMs = 1000;
constexpr uint8_t kCursorAlphaThreshold = 96;
constexpr uint32_t kFallbackCoreColor = 0x00FFFFFF;
constexpr uint32_t kFallbackOuterColor = 0x00000000;
constexpr float kFallbackCoreLuminance = 1.0f;
static_assert(kFallbackCoreColor == 0x00FFFFFF, "The cached fallback luminance assumes a white core");
constexpr DWORD kBackbufferIdleReleaseDelayMs = 5000;
constexpr int kBackbufferGrowthGranularity = 64;
constexpr int kMaxActiveRipples = 12;
constexpr float kRippleInitialRadius = 4.0f;
constexpr int kMaxRippleClusterRadiusPx = 512;
constexpr int kMaxActiveMotionAccents = 8;
constexpr float kFastMoveInterpolationSpacingPx = 28.0f;
constexpr DWORD kMotionResumeGapMs = 120;
constexpr DWORD kAccelerationMeasurementWindowMs = 40;
constexpr DWORD kAccelerationAccentCooldownMs = 180;
constexpr DWORD kTurnAccentCooldownMs = 220;
constexpr float kDegreesToRadians = 3.14159265358979323846f / 180.0f;
constexpr int kOrnamentMaxPresetNodes = 3;
constexpr int kOrnamentMaxRopeSegments = 12;
constexpr int kOrnamentMaxRopePoints = kOrnamentMaxRopeSegments + 1;
// Two Chaikin passes keep at most 4N-6 points, so this fits the longest rope.
constexpr int kOrnamentMaxRopeDrawPoints = kOrnamentMaxRopePoints * 4;
constexpr int kOrnamentMaxImageDimension = 4096;
constexpr uint64_t kOrnamentMaxDecodedBytes = 128ull * 1024ull * 1024ull;
constexpr float kOrnamentDefaultRopeLengthPx = 48.0f;
constexpr float kOrnamentDefaultSizePx = 32.0f;
constexpr float kOrnamentConstraintEpsilonPx = 0.0001f;
constexpr float kOrnamentSpeedReferencePxPerSecond = 2400.0f;
constexpr float kOrnamentSpeedSmoothing = 0.35f;
constexpr float kOrnamentRestVelocityPx = 0.25f;
constexpr float kOrnamentSimulationHz = 125.0f;
constexpr int kOrnamentMaxSubSteps = 8;
constexpr float kOrnamentMaxCatchUpSeconds = 0.25f;
constexpr float kOrnamentMinDrawOpacity = 0.004f;
constexpr float kOrnamentPi = 3.14159265f;
constexpr float kOrnamentDefaultClickImpulse = 240.0f;
constexpr float kOrnamentDefaultWheelImpulse = 200.0f;
