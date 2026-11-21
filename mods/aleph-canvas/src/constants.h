#pragma once

// Compile-time geometry, timing and policy values.
//
// Everything a user may want to change is a Windhawk setting; this header only
// holds canvas structure that must stay internally consistent (frame pacing,
// hit-test tolerances, cap sizes).

#include "platform.h"

constexpr UINT kToggleHotkeyId = 0xA1E1;
constexpr int kMaxCards = 64;
constexpr int kMaxLayoutEntries = 256;
constexpr int kMaxExcludeEntries = 64;
constexpr int kMaxStackDepth = 8;

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
constexpr float kCardOpacityEpsilon = 0.001f;

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

// A hotkey registration can fail transiently, for example right after a
// settings reload while another program still holds the combination. The
// worker retries with a capped exponential backoff so a reloaded hotkey
// becomes active without the user touching the settings again.
constexpr DWORD kHotkeyRetryInitialMs = 1000;
constexpr DWORD kHotkeyRetryMaxMs = 5000;

[[nodiscard]] inline DWORD NextHotkeyRetryDelay(DWORD previous) {
    if (previous == 0) {
        return kHotkeyRetryInitialMs;
    }
    const DWORD doubled =
        previous > kHotkeyRetryMaxMs / 2 ? kHotkeyRetryMaxMs : previous * 2;
    return std::min(doubled, kHotkeyRetryMaxMs);
}
