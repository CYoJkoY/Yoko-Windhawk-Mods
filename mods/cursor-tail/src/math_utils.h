#pragma once

// Colour conversion, luminance and colour parsing helpers.

#include "platform.h"
#include "constants.h"

static void EnsureSrgbLut() {
    if (context.srgbLutReady) return;
    for (int i = 0; i < 256; ++i) {
        const float channel = static_cast<float>(i) / 255.0f;
        context.srgbToLinear[i] = channel <= 0.03928f
            ? channel / 12.92f
            : powf((channel + 0.055f) / 1.055f, 2.4f);
    }
    context.srgbLutReady = true;
}

static inline float RgbLuminance(uint8_t r, uint8_t g, uint8_t b) {
    if (!context.srgbLutReady) EnsureSrgbLut();
    return 0.2126f * context.srgbToLinear[r]
        + 0.7152f * context.srgbToLinear[g]
        + 0.0722f * context.srgbToLinear[b];
}

static inline float RgbLuminance(uint32_t rgb) {
    return RgbLuminance(
        static_cast<uint8_t>((rgb >> 16) & 0xFF),
        static_cast<uint8_t>((rgb >> 8) & 0xFF),
        static_cast<uint8_t>(rgb & 0xFF));
}

static inline float ContrastRatio(float a, float b) {
    const float lo = std::min(a, b);
    const float hi = std::max(a, b);
    return (hi + 0.05f) / (lo + 0.05f);
}

static inline D2D1_COLOR_F ToColorF(uint32_t rgb, float alpha) {
    return D2D1::ColorF(
        ((rgb >> 16) & 0xFF) / 255.0f,
        ((rgb >> 8) & 0xFF) / 255.0f,
        (rgb & 0xFF) / 255.0f,
        alpha);
}

// Solid brushes keep the last submitted color, so a recreated brush must also
// reset its cache entry. The sentinel is outside the 24-bit color space.
static inline void ApplySolidBrushColor(
    ID2D1SolidColorBrush* brush,
    uint32_t rgb,
    uint32_t& cache) {
    if (cache == rgb) return;
    brush->SetColor(ToColorF(rgb, 1.0f));
    cache = rgb;
}

static bool ParseHexColor(const wchar_t* str, uint32_t& out) {
    if (!str) return false;
    while (*str == L' ' || *str == L'\t') ++str;
    if (*str == L'#') ++str;
    else if (str[0] == L'0' && (str[1] == L'x' || str[1] == L'X')) str += 2;
    uint32_t value = 0;
    int digits = 0;
    while (digits < 6) {
        const wchar_t c = str[digits];
        int digit = -1;
        if (c >= L'0' && c <= L'9') digit = c - L'0';
        else if (c >= L'a' && c <= L'f') digit = c - L'a' + 10;
        else if (c >= L'A' && c <= L'F') digit = c - L'A' + 10;
        if (digit < 0) break;
        value = (value << 4) | static_cast<uint32_t>(digit);
        ++digits;
    }
    if (digits != 6) return false;
    out = value;
    return true;
}

static std::wstring ToLowerW(std::wstring value) {
    for (wchar_t& c : value) c = static_cast<wchar_t>(towlower(c));
    return value;
}

static LONGLONG QpcNow() {
    LARGE_INTEGER value = {};
    QueryPerformanceCounter(&value);
    return value.QuadPart;
}
