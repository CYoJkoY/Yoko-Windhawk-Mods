#pragma once

// Plain data types used by the renderer, the caches and the settings layer.

#include "platform.h"
#include "constants.h"

// The preset table below is ordered to match this enum, so a preset key
// resolves to its definition by index.
enum class OrnamentPreset : uint8_t {
    Classic,
    Bouncy,
    Floaty,
    Comet,
};

enum class OrnamentRopeStyle : uint8_t {
    Silk,
    Neon,
    Chain,
    Ribbon,
};

// Every preset owns the physics, reaction strength, rope look, and node layout
// so the user only chooses a look. ropeT places a node along the rope, where
// 0 is the pointer anchor and 1 is the rope end.
struct OrnamentPresetNode {
    float ropeT;
    float sizePx;
    float opacity;
    float angleDegrees;
};

struct OrnamentPresetDefinition {
    const wchar_t* key;
    float anchorOffsetX;
    float anchorOffsetY;
    float gravity;
    float damping;
    int constraintIterations;
    float stretch;
    float opacity;
    float clickImpulse;
    float wheelImpulse;
    int ropeSegments;
    float ropeLengthPx;
    OrnamentRopeStyle ropeStyle;
    float ropeWidthPx;
    float ropeOpacity;
    int nodeCount;
    OrnamentPresetNode nodes[kOrnamentMaxPresetNodes];
};

// Damping sets the drag lag time constant, dt * 100 / damping, so a low value
// makes a steady drag trail far behind the pointer instead of just swinging.
constexpr OrnamentPresetDefinition kOrnamentPresets[] = {
    {
        L"classic",
        10.0f,
        12.0f,
        1400.0f,
        22.0f,
        12,
        0.18f,
        1.0f,
        240.0f,
        200.0f,
        8,
        48.0f,
        OrnamentRopeStyle::Silk,
        1.5f,
        0.8f,
        1,
        {{1.0f, 32.0f, 1.0f, 0.0f}},
    },
    {
        L"bouncy",
        12.0f,
        10.0f,
        1800.0f,
        16.0f,
        14,
        0.30f,
        1.0f,
        420.0f,
        320.0f,
        10,
        58.0f,
        OrnamentRopeStyle::Chain,
        1.6f,
        0.9f,
        1,
        {{1.0f, 34.0f, 1.0f, 0.0f}},
    },
    {
        L"floaty",
        8.0f,
        8.0f,
        220.0f,
        26.0f,
        10,
        0.06f,
        0.9f,
        120.0f,
        100.0f,
        12,
        68.0f,
        OrnamentRopeStyle::Ribbon,
        1.2f,
        0.65f,
        1,
        {{1.0f, 30.0f, 0.95f, 0.0f}},
    },
    {
        L"comet",
        10.0f,
        12.0f,
        1200.0f,
        20.0f,
        12,
        0.22f,
        1.0f,
        300.0f,
        240.0f,
        10,
        64.0f,
        OrnamentRopeStyle::Neon,
        1.7f,
        0.9f,
        3,
        {
            {0.35f, 30.0f, 1.0f, 0.0f},
            {0.67f, 22.0f, 0.7f, 8.0f},
            {1.0f, 15.0f, 0.4f, 16.0f},
        },
    },
};
struct OrnamentImageSpec {
    std::wstring id;
    std::wstring path;
};

struct OrnamentNodeSpec {
    std::wstring imageId;
    float ropeT = 1.0f;
    float sizePx = kOrnamentDefaultSizePx;
    float angleDegrees = 0.0f;
    float pivotX = 0.5f;
    float pivotY = 0.18f;
    float opacity = 1.0f;
};

struct OrnamentImage {
    std::wstring id;
    std::wstring path;
    UINT width = 0;
    UINT height = 0;
    std::vector<uint8_t> pixels;
    ID2D1Bitmap* bitmap = nullptr;
    bool decodeFailed = false;
    bool uploadFailed = false;
};

struct OrnamentNode {
    int imageIndex = -1;
    float ropeT = 1.0f;
    float sizePx = kOrnamentDefaultSizePx;
    float angleDegrees = 0.0f;
    float pivotX = 0.5f;
    float pivotY = 0.18f;
    float opacity = 1.0f;
    D2D1_POINT_2F position = {};
    D2D1_RECT_F localBounds = {};
    float drawAngleDegrees = 0.0f;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
};
enum class RippleKind : uint8_t {
    Click,
    WheelUp,
    WheelDown,
    WheelLeft,
    WheelRight,
};

struct RippleEffect {
    POINT center = {};
    LONGLONG startQpc = 0;
    uint32_t color = 0x0000D7FF;
    RippleKind kind = RippleKind::Click;
    bool active = false;
};

enum class MotionAccentKind : uint8_t {
    Acceleration,
    Turn,
};

struct MotionAccent {
    POINT center = {};
    float directionX = 1.0f;
    float directionY = 0.0f;
    float previousDirectionX = 0.0f;
    float previousDirectionY = 0.0f;
    float intensity = 1.0f;
    LONGLONG startQpc = 0;
    MotionAccentKind kind = MotionAccentKind::Acceleration;
    bool active = false;
};
struct AppRule { std::wstring exe; bool enabled; };
struct TrailRenderCache {
    D2D1_POINT_2F pointsA[kMaxTrailRenderPoints] = {};
    D2D1_POINT_2F pointsB[kMaxTrailRenderPoints] = {};
    D2D1_POINT_2F* activePoints = pointsA;
    int pointCount = 0;
    int generation = 0;
    float taper[kMaxTrailRenderPoints] = {};
    int cachedTaperPointCount = -1;
    int cachedTaperPowerIndex = -1;
    bool taperCacheValid = false;

    void PrepareTaper();
};
// The ribbon submits one closed outline per layer: side one, a round tail cap,
// then side two. The head stays a separate ellipse, which keeps the original
// rounded head exactly. A single fill replaces a stroke per trail segment, and
// the winding fill mode keeps self-crossing trails filled as one shape.
// A reversing turn folds the outline instead of reaching the round tip of the
// stroke join it replaced, so those interior centers get a disk of at most the
// local half width after the fill.
//
// Center decimation, taper sampling, and normals depend only on the smoothed
// trail, so they are computed once per generation and shared by all layers.
// Only the half width scaling and the outline points vary per layer.
struct TrailRibbonCache {
    D2D1_POINT_2F centers[kTrailRibbonMaxCenterPoints] = {};
    float normalX[kTrailRibbonMaxCenterPoints] = {};
    float normalY[kTrailRibbonMaxCenterPoints] = {};
    float taper[kTrailRibbonMaxCenterPoints] = {};
    bool roundJoin[kTrailRibbonMaxCenterPoints] = {};
    int centerCount = 0;
    int generation = 0;
    bool valid = false;
};
struct ColorCandidate {
    uint32_t rgb;
    int count;
    float luminance;
};
