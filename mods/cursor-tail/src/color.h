#pragma once

// Cursor colour sampling, background luminance and trail colour selection.

#include "platform.h"
#include "context.h"
#include "math_utils.h"
#include "types.h"
#include "render.h"
#include "forward.h"

constexpr float kAutoColorLuminanceChangeThreshold = 0.04f;
static bool ExtractCursorColorCandidates(HCURSOR cursorHandle, std::vector<ColorCandidate>& out) {
    out.clear();
    if (!cursorHandle) return false;
    ICONINFO iconInfo = {};
    if (!GetIconInfo(cursorHandle, &iconInfo)) return false;
    const auto cleanup = [&] {
        if (iconInfo.hbmColor) DeleteObject(iconInfo.hbmColor);
        if (iconInfo.hbmMask) DeleteObject(iconInfo.hbmMask);
    };
    if (iconInfo.hbmColor) {
        BITMAP bitmap = {};
        if (GetObject(iconInfo.hbmColor, sizeof(bitmap), &bitmap) && bitmap.bmWidth > 0 && bitmap.bmHeight > 0 && bitmap.bmWidth <= 256 && bitmap.bmHeight <= 256) {
            const int width = bitmap.bmWidth;
            const int height = bitmap.bmHeight;
            BITMAPINFO info = {};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = width;
            info.bmiHeader.biHeight = -height;
            info.bmiHeader.biPlanes = 1;
            info.bmiHeader.biBitCount = 32;
            info.bmiHeader.biCompression = BI_RGB;
            std::vector<uint32_t> pixels(static_cast<size_t>(width) * height);
            HDC screen = GetScreenDC();
            if (screen && GetDIBits(screen, iconInfo.hbmColor, 0, height, pixels.data(), &info, DIB_RGB_COLORS) == height) {
                std::unordered_map<uint32_t, int> histogram;
                histogram.reserve(64);
                for (uint32_t pixel : pixels) if (((pixel >> 24) & 0xFF) >= kCursorAlphaThreshold) ++histogram[pixel & 0x00FFFFFF];
                if (!histogram.empty()) {
                    out.reserve(histogram.size());
                    for (const auto& entry : histogram) {
                        out.push_back({entry.first, entry.second, 0.0f});
                    }
                    std::sort(out.begin(), out.end(), [](const ColorCandidate& a, const ColorCandidate& b) { return a.count > b.count; });
                    cleanup();
                    return true;
                }

                if (iconInfo.hbmMask) {
                    BITMAP maskBitmap = {};
                    if (GetObject(iconInfo.hbmMask, sizeof(maskBitmap), &maskBitmap) && maskBitmap.bmWidth >= width && maskBitmap.bmHeight >= height) {
                        BITMAPINFO maskInfo = {};
                        maskInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                        maskInfo.bmiHeader.biWidth = width;
                        maskInfo.bmiHeader.biHeight = -height;
                        maskInfo.bmiHeader.biPlanes = 1;
                        maskInfo.bmiHeader.biBitCount = 1;
                        maskInfo.bmiHeader.biCompression = BI_RGB;
                        maskInfo.bmiHeader.biClrUsed = 2;
                        const size_t maskStride = ((static_cast<size_t>(width) + 31u) / 32u) * 4u;
                        std::vector<uint8_t> maskBits(maskStride * static_cast<size_t>(height));
                        if (GetDIBits(screen, iconInfo.hbmMask, 0, height, maskBits.data(), &maskInfo, DIB_RGB_COLORS) == height) {
                            std::unordered_map<uint32_t, int> maskHistogram;
                            maskHistogram.reserve(64);
                            for (int y = 0; y < height; ++y) {
                                for (int x = 0; x < width; ++x) {
                                    const uint8_t rowByte = maskBits[static_cast<size_t>(y) * maskStride + static_cast<size_t>(x >> 3)];
                                    const bool transparent = (rowByte & (0x80u >> (x & 7))) != 0;
                                    if (!transparent) {
                                        const uint32_t rgb = pixels[static_cast<size_t>(y) * width + x] & 0x00FFFFFF;
                                        ++maskHistogram[rgb];
                                    }
                                }
                            }
                            out.reserve(maskHistogram.size());
                            for (const auto& entry : maskHistogram) {
                                out.push_back({entry.first, entry.second, 0.0f});
                            }
                            std::sort(out.begin(), out.end(), [](const ColorCandidate& a, const ColorCandidate& b) { return a.count > b.count; });
                            cleanup();
                            return !out.empty();
                        }
                    }
                }
            }
        }
    }
    cleanup();
    return false;
}
static void ResetAutoColorSamplingCache() {
    context.backgroundLuminanceValid = false;
    context.backgroundLuminanceCenter = {0, 0};
    context.backgroundLuminanceUpdated = 0;
    context.autoColorPickLuminanceValid = false;
}

static void ReleaseBackgroundSampler() {
    if (context.backgroundSamplerDc) {
        if (context.backgroundSamplerOriginal) SelectObject(context.backgroundSamplerDc, context.backgroundSamplerOriginal);
        DeleteDC(context.backgroundSamplerDc);
        context.backgroundSamplerDc = nullptr;
    }
    if (context.backgroundSamplerBitmap) {
        DeleteObject(context.backgroundSamplerBitmap);
        context.backgroundSamplerBitmap = nullptr;
    }
    context.backgroundSamplerOriginal = nullptr;
    context.backgroundSamplerPixels = nullptr;
    context.backgroundSamplerSize = 0;
    context.backgroundLuminanceValid = false;
}

static float SampleBackgroundLuminance(DWORD now, POINT center) {
    if (context.backgroundLuminanceValid) {
        const int64_t dx = static_cast<int64_t>(center.x) -
                           static_cast<int64_t>(context.backgroundLuminanceCenter.x);
        const int64_t dy = static_cast<int64_t>(center.y) -
                           static_cast<int64_t>(context.backgroundLuminanceCenter.y);
        const int64_t distanceThreshold = kBackgroundResampleDistancePx;
        // Short-circuit large coordinate deltas before squaring them; the final
        // squared-distance comparison only runs when both deltas are below 20.
        // A time limit is mandatory, while cursor travel is an additional gate
        // so high-speed movement cannot trigger a capture on every sample.
        const bool movedFarEnough =
            dx <= -distanceThreshold || dx >= distanceThreshold ||
            dy <= -distanceThreshold || dy >= distanceThreshold ||
            dx * dx + dy * dy >= distanceThreshold * distanceThreshold;
        if (now - context.backgroundLuminanceUpdated < kBackgroundResampleIntervalMs
            || !movedFarEnough) {
            return context.backgroundLuminance;
        }
    }

    const int size = kBackgroundSampleRadius * 2 + 1;
    HDC screen = GetScreenDC();
    if (!screen) return context.backgroundLuminanceValid ? context.backgroundLuminance : 0.5f;
    if (context.backgroundSamplerSize != size || !context.backgroundSamplerDc || !context.backgroundSamplerBitmap || !context.backgroundSamplerPixels) {
        ReleaseBackgroundSampler();
        BITMAPINFO info = {};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = size;
        info.bmiHeader.biHeight = -size;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        context.backgroundSamplerBitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!context.backgroundSamplerBitmap || !bits) { ReleaseBackgroundSampler(); return context.backgroundLuminanceValid ? context.backgroundLuminance : 0.5f; }
        context.backgroundSamplerPixels = static_cast<uint32_t*>(bits);
        context.backgroundSamplerDc = CreateCompatibleDC(screen);
        if (!context.backgroundSamplerDc) { ReleaseBackgroundSampler(); return context.backgroundLuminanceValid ? context.backgroundLuminance : 0.5f; }
        context.backgroundSamplerOriginal = SelectObject(context.backgroundSamplerDc, context.backgroundSamplerBitmap);
        context.backgroundSamplerSize = size;
    }
    if (!BitBlt(context.backgroundSamplerDc, 0, 0, size, size, screen, center.x - kBackgroundSampleRadius, center.y - kBackgroundSampleRadius, SRCCOPY)) return context.backgroundLuminanceValid ? context.backgroundLuminance : 0.5f;
    // GDI batches drawing calls, so the blit has to be flushed before the DIB
    // section bits are read directly.
    GdiFlush();
    double sum = 0.0;
    int count = 0;
    for (int yIndex = 0; yIndex < kBackgroundSampleGrid; ++yIndex) {
        const int y = (size - 1) * yIndex / (kBackgroundSampleGrid - 1);
        for (int xIndex = 0; xIndex < kBackgroundSampleGrid; ++xIndex) {
            const int x = (size - 1) * xIndex / (kBackgroundSampleGrid - 1);
            const uint32_t pixel = context.backgroundSamplerPixels[y * size + x];
            sum += RgbLuminance(static_cast<uint8_t>((pixel >> 16) & 0xFF), static_cast<uint8_t>((pixel >> 8) & 0xFF), static_cast<uint8_t>(pixel & 0xFF));
            ++count;
        }
    }
    const float luminance = count ? static_cast<float>(sum / count) : 0.5f;
    context.backgroundLuminance = luminance;
    context.backgroundLuminanceCenter = center;
    context.backgroundLuminanceUpdated = now;
    context.backgroundLuminanceValid = true;
    return luminance;
}

static void SetTrailCoreColor(uint32_t core, float luminance) {
    context.currentCoreRGB = core;
    context.currentCoreLuminance = luminance;
    if (context.outlineColorMode == 1) {
        context.currentOuterRGB = context.manualOutlineRGB;
        return;
    }
    context.currentOuterRGB = luminance > 0.5f ? kFallbackOuterColor : kFallbackCoreColor;
}

static void UpdateTrailColorIfNeeded(DWORD now, const POINT& point) {
    if (context.trailColorMode == 0) {
        if (context.currentCoreRGB != context.manualColorRGB) {
            SetTrailCoreColor(context.manualColorRGB, RgbLuminance(context.manualColorRGB));
        }
        return;
    }

    yoko::win32::UniqueCursorInfo cursorInfo;
    if (!cursorInfo.query() || !cursorInfo.visible()) {
        context.lastAutoColorCursor = nullptr;
        context.autoColorInitialized = true;
        context.autoColorPickLuminanceValid = false;
        context.cachedCursorCandidateHandle = nullptr;
        context.cachedCursorColorCandidates.clear();
        context.cachedCursorCandidatesReady = false;
        SetTrailCoreColor(kFallbackCoreColor, kFallbackCoreLuminance);
        return;
    }

    const HCURSOR cursorHandle = cursorInfo.value.hCursor;
    const bool cursorChanged = !context.autoColorInitialized || cursorHandle != context.lastAutoColorCursor;
    const bool intervalElapsed = context.autoResampleInterval > 0
        && now - context.lastAutoColorUpdate >= static_cast<DWORD>(context.autoResampleInterval);
    const bool refreshCandidates = cursorChanged || intervalElapsed
        || !context.cachedCursorCandidatesReady
        || context.cachedCursorCandidateHandle != cursorHandle;

    // SampleBackgroundLuminance already throttles screen reads by both time and
    // cursor travel. Use its cached value to adapt contrast without re-reading
    // the cursor bitmap when only the content beneath the pointer changed.
    const float background = SampleBackgroundLuminance(now, point);

    if (refreshCandidates) {
        context.cachedCursorColorCandidates.clear();
        context.cachedCursorCandidatesReady = true;
        context.cachedCursorCandidateHandle = cursorHandle;
        ExtractCursorColorCandidates(cursorHandle, context.cachedCursorColorCandidates);
        // A cursor bitmap is resampled only when its handle or the configured
        // interval changes. Fold luminance into that same step so selection
        // never pays for it per frame.
        for (ColorCandidate& candidate : context.cachedCursorColorCandidates) {
            candidate.luminance = RgbLuminance(candidate.rgb);
        }
        context.lastAutoColorCursor = cursorHandle;
        context.lastAutoColorUpdate = now;
        context.autoColorInitialized = true;
    }

    if (context.cachedCursorColorCandidates.empty()) {
        SetTrailCoreColor(kFallbackCoreColor, kFallbackCoreLuminance);
        context.autoColorPickLuminanceValid = false;
        return;
    }

    const bool backgroundChanged = !context.autoColorPickLuminanceValid
        || fabsf(background - context.lastAutoColorPickLuminance) >= kAutoColorLuminanceChangeThreshold;
    if (!refreshCandidates && !backgroundChanged) return;

    // Keep the existing color when it still has acceptable contrast. Using a
    // lower hold threshold than the selection threshold prevents color flapping.
    if (!refreshCandidates
        && ContrastRatio(context.currentCoreLuminance, background)
            >= kAutoColorHysteresisContrast) {
        context.lastAutoColorPickLuminance = background;
        context.autoColorPickLuminanceValid = true;
        return;
    }

    // In Auto mode the overlay is excluded from screen capture, so the sample
    // can't pick up the trail's own pixels and the window need not be hidden.
    for (const ColorCandidate& candidate : context.cachedCursorColorCandidates) {
        if (ContrastRatio(candidate.luminance, background) >= kMinColorContrast) {
            SetTrailCoreColor(candidate.rgb, candidate.luminance);
            context.lastAutoColorPickLuminance = background;
            context.autoColorPickLuminanceValid = true;
            return;
        }
    }

    SetTrailCoreColor(kFallbackCoreColor, kFallbackCoreLuminance);
    context.lastAutoColorPickLuminance = background;
    context.autoColorPickLuminanceValid = true;
}
