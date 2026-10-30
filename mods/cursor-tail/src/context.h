#pragma once

// The single ModContext instance that owns every mutable value.

#include "platform.h"
#include "constants.h"
#include "types.h"

struct ModContext {

    // Thread, sampling, and input wake state.
    std::atomic<HWND> overlayHwnd{nullptr};
    HANDLE threadHandle = nullptr;
    HANDLE stopEvent = nullptr;
    HANDLE inputWakeEvent = nullptr;
    std::atomic_bool settingsDirty{false};
    bool interactionPending = false;
    bool rawInputRegistered = false;
    bool rawInputWakePending = false;
    bool rawMouseMovementPending = false;
    bool foregroundChangePending = false;
    bool elevatedForeground = false;
    bool pollingFallback = true;
    bool currentIntegrityLevelAttempted = false;
    bool currentIntegrityLevelKnown = false;
    DWORD currentIntegrityLevelRid = SECURITY_MANDATORY_MEDIUM_RID;
    POINT fallbackCursorPosition = {};
    DWORD fallbackLastMovementTick = 0;
    bool fallbackCursorPositionValid = false;
    LONGLONG nextSampleDeadline = 0;
    LONGLONG lastRawInputWakeQpc = 0;
    HWINEVENTHOOK foregroundEventHook = nullptr;

    POINT history[kHistoryCapacity] = {};
    int historyHead = 0;
    int historyCount = 0;
    POINT lastPos = {0, 0};
    LONGLONG lastSampleQpc = 0;
    LONGLONG qpcFrequency = 0;
    bool isSmearing = false;
    int lowVelocityFrames = 0;
    float fadeAlpha = 0.0f;
    POINT lastMotionCuePoint = {};
    LONGLONG lastMotionCueQpc = 0;
    float lastMotionCueSpeed = 0.0f;
    POINT accelerationWindowPoint = {};
    LONGLONG accelerationWindowQpc = 0;
    float accelerationWindowSpeed = 0.0f;
    bool hasAccelerationWindowSample = false;
    bool hasAccelerationWindowSpeed = false;
    float lastMotionDirectionX = 0.0f;
    float lastMotionDirectionY = 0.0f;
    bool hasPreviousMotionCueSample = false;
    LONGLONG lastAccelerationAccentQpc = 0;
    LONGLONG lastTurnAccentQpc = 0;
    float smoothedSpeedNorm = 0.0f;
    float frozenSpeedNorm = 0.5f;
    bool windowVisible = false;

    ID2D1Factory* d2dFactory = nullptr;
    ID2D1DCRenderTarget* renderTarget = nullptr;
    ID2D1Layer* trailOpacityLayer = nullptr;
    ID2D1Layer* glowOpacityLayer = nullptr;
    ID2D1SolidColorBrush* outerBrush = nullptr;
    ID2D1SolidColorBrush* coreBrush = nullptr;
    ID2D1SolidColorBrush* glowBrush = nullptr;
    ID2D1SolidColorBrush* rippleBrush = nullptr;
    ID2D1LinearGradientBrush* gradientBrush = nullptr;
    ID2D1GradientStopCollection* gradientStops = nullptr;
    ID2D1StrokeStyle* strokeStyle = nullptr;
    uint32_t gradientHeadCache = 0xFFFFFFFFu;
    uint32_t gradientTailCache = 0xFFFFFFFFu;
    uint32_t outerBrushColorCache = 0xFFFFFFFFu;
    uint32_t coreBrushColorCache = 0xFFFFFFFFu;
    uint32_t glowBrushColorCache = 0xFFFFFFFFu;
    bool dcBound = false;
    HDC backBufferDc = nullptr;
    HBITMAP backBufferBitmap = nullptr;
    HGDIOBJ originalBitmap = nullptr;
    int cachedWidth = 0;
    int cachedHeight = 0;
    DWORD backbufferIdleSince = 0;
    HDC screenDc = nullptr;

    float triggerVelocity = 3000.0f;
    float stopVelocity = 1200.0f;
    int tailOffsetX = 6;
    int tailOffsetY = 10;
    int tailLength = 10;
    int speedScaling = 1;
    float widthMin = 4.0f;
    float widthMax = 14.0f;
    float coreWidthMin = 2.0f;
    float coreWidthMax = 9.0f;
    float alphaMin = 0.39f;
    float alphaMax = 0.77f;
    int taperPowerIndex = 10;
    int smoothIterations = 2;
    int gradientEnabled = 0;
    uint32_t gradientTailRGB = 0x00FF00FF;
    int glowEnabled = 0;
    uint32_t glowRGB = 0x00FFFFFF;
    float glowWidthFactor = 1.8f;
    float glowAlpha = 0.25f;
    int fadeEnabled = 1;
    float fadeDecay = 0.90f;
    int trailColorMode = 0;
    uint32_t manualColorRGB = 0x00FFFFFF;
    int outlineColorMode = 0;
    uint32_t manualOutlineRGB = 0x00000000;
    int autoResampleInterval = 0;
    uint32_t currentCoreRGB = kFallbackCoreColor;
    uint32_t currentOuterRGB = kFallbackOuterColor;
    float currentCoreLuminance = kFallbackCoreLuminance;
    DWORD lastAutoColorUpdate = 0;
    HCURSOR lastAutoColorCursor = nullptr;
    bool autoColorInitialized = false;
    float lastAutoColorPickLuminance = 0.5f;
    bool autoColorPickLuminanceValid = false;

    HMONITOR cursorMonitor = nullptr;
    UINT cursorDpi = 96;
    float currentDpiScale = 1.0f;

    int clickFeedbackEnabled = 0;
    int wheelFeedbackEnabled = 0;
    uint32_t leftClickRGB = 0x0000D7FF;
    uint32_t rightClickRGB = 0x00FF4C8B;
    uint32_t middleClickRGB = 0x00FFD166;
    uint32_t wheelUpRGB = 0x007CFF6B;
    uint32_t wheelDownRGB = 0x00A78BFA;
    float rippleRadius = 36.0f;
    float wheelArrowSize = 18.0f;
    float wheelArrowDistance = 32.0f;
    int rippleDurationMs = 420;
    float rippleStrokeWidth = 3.0f;
    float rippleOpacity = 0.78f;
    int accelerationAccentEnabled = 1;
    int turnAccentEnabled = 0;
    int fastMoveInterpolationEnabled = 1;
    int accelerationThreshold = 30000;
    int turnAngleDegrees = 65;
    float turnAccentCosThreshold = 0.42261826f;  // cos(65 degrees)
    int motionAccentDurationMs = 300;
    float motionAccentSize = 22.0f;
    float motionAccentOpacity = 0.90f;
    int maxInterpolationSteps = 4;
    uint32_t motionAccentRGB = 0x00FFD166;

    int ornamentEnabled = 0;
    float ornamentAnchorOffsetX = 10.0f;
    float ornamentAnchorOffsetY = 12.0f;
    float ornamentGravity = 1400.0f;
    float ornamentDamping = 8.0f;
    int ornamentConstraintIterations = 4;
    float ornamentStretch = 0.18f;
    float ornamentOpacity = 1.0f;
    float ornamentClickImpulse = kOrnamentDefaultClickImpulse;
    float ornamentWheelImpulse = kOrnamentDefaultWheelImpulse;
    int ornamentRopeSegments = 8;
    float ornamentRopeLengthPx = kOrnamentDefaultRopeLengthPx;
    float ornamentRopeLengthScale = 1.0f;
    OrnamentRopeStyle ornamentRopeStyle = OrnamentRopeStyle::Silk;
    float ornamentRopeWidthPx = 1.5f;
    float ornamentRopeOpacity = 0.8f;

    // Ornament assets and simulation state.
    std::vector<OrnamentImageSpec> ornamentImageSpecs;
    std::vector<OrnamentNodeSpec> ornamentNodeSpecs;
    std::vector<OrnamentImage> ornamentImages;
    std::vector<OrnamentNode> ornamentNodes;
    // Set once every image in ornamentImages holds a device bitmap or has
    // latched a failure, so visible frames skip the whole upload scan.
    bool ornamentBitmapsSettled = false;
    D2D1_POINT_2F ornamentRopePoints[kOrnamentMaxRopePoints] = {};
    D2D1_POINT_2F ornamentRopePrevious[kOrnamentMaxRopePoints] = {};
    D2D1_POINT_2F ornamentRopeSmoothA[kOrnamentMaxRopeDrawPoints] = {};
    D2D1_POINT_2F ornamentRopeSmoothB[kOrnamentMaxRopeDrawPoints] = {};
    int ornamentRopePointCount = 0;
    float ornamentRopeSegmentPx = 0.0f;
    std::atomic_bool ornamentAssetsDirty{false};
    bool ornamentAssetsReady = false;
    bool ornamentPhysicsInitialized = false;
    bool ornamentRedrawPending = false;
    LONGLONG ornamentLastStepQpc = 0;
    LONGLONG ornamentNextRenderQpc = 0;
    D2D1_POINT_2F ornamentLastAnchor = {};
    float ornamentSmoothedSpeedNorm = 0.0f;
    float ornamentSmoothedSpeedPx = 0.0f;
    IWICImagingFactory* wicFactory = nullptr;
    ID2D1SolidColorBrush* ornamentBrush = nullptr;

    // Interaction effects.
    RippleEffect ripples[kMaxActiveRipples] = {};
    int nextRippleSlot = 0;
    MotionAccent motionAccents[kMaxActiveMotionAccents] = {};
    int nextMotionAccentSlot = 0;

    // Application rules and fullscreen suppression.
    std::vector<AppRule> appRules;
    HWND cachedForegroundWindow = nullptr;
    int cachedAppRule = 0;

    bool fullscreenSuppressed = false;
    DWORD fullscreenFalseSince = 0;
    HWND fullscreenForegroundWindow = nullptr;
    DWORD lastFullscreenStrongCheck = 0;
    bool fullscreenStrongSignal = false;
    bool fullscreenCoversMonitor = false;

    // Cubic taper lookup table.
    // The settings expose exponent values from 0.5 through 3.0 in 0.1 steps.
    // Filling the table once removes powf from every taper rebuild; interpolating
    // between the 257 samples keeps the curve visually identical.
    float taperLut[kTaperPowerCount][kTaperLutSteps + 1] = {};
    bool taperLutReady = false;

    // Trail render cache.
    TrailRenderCache renderCache = {};

    // sRGB to linear lookup table.
    // sRGB channel values converted to linear light once. Luminance is evaluated
    // for sampled screen pixels and every cursor color candidate, so a fixed table
    // replaces a powf call on each of the three channels.
    float srgbToLinear[256] = {};
    bool srgbLutReady = false;

    // Ribbon cache and outline scratch buffer.
    TrailRibbonCache ribbonCache = {};
    D2D1_POINT_2F ribbonOutline[kTrailRibbonMaxOutlinePoints] = {};

    // Cursor color candidate cache.
    HCURSOR cachedCursorCandidateHandle = nullptr;
    std::vector<ColorCandidate> cachedCursorColorCandidates;
    bool cachedCursorCandidatesReady = false;

    // Background luminance sampling surface.
    HDC backgroundSamplerDc = nullptr;
    HBITMAP backgroundSamplerBitmap = nullptr;
    HGDIOBJ backgroundSamplerOriginal = nullptr;
    uint32_t* backgroundSamplerPixels = nullptr;
    int backgroundSamplerSize = 0;
    bool backgroundLuminanceValid = false;
    POINT backgroundLuminanceCenter = {0, 0};
    DWORD backgroundLuminanceUpdated = 0;
    float backgroundLuminance = 0.5f;

    // Tool mod launcher state.
    bool isToolModProcessLauncher = false;
    HANDLE toolModProcessMutex = nullptr;
};

// The only instance. All mutable mod state lives here.
inline ModContext context;
