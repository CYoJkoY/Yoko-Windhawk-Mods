#pragma once

// Trail smoothing, taper lookup and ribbon geometry.

#include "platform.h"
#include "context.h"
#include "history.h"
#include "math_utils.h"
#include "types.h"
#include "forward.h"

static void EnsureTaperLut() {
    if (context.taperLutReady) return;
    for (int powerIndex = 0; powerIndex < kTaperPowerCount; ++powerIndex) {
        const float power =
            static_cast<float>(kTaperPowerSettingMin + powerIndex) / 10.0f;
        float* row = context.taperLut[powerIndex];
        for (int i = 0; i < kTaperLutSteps; ++i) {
            const float ratio =
                static_cast<float>(i) / static_cast<float>(kTaperLutSteps);
            row[i] = powf(1.0f - ratio, power);
        }
        row[kTaperLutSteps] = 0.0f;
    }
    context.taperLutReady = true;
}
inline void TrailRenderCache::PrepareTaper() {
        if (pointCount <= 1) return;
        if (taperCacheValid &&
            cachedTaperPointCount == pointCount &&
            cachedTaperPowerIndex == context.taperPowerIndex) {
            return;
        }

        EnsureTaperLut();
        const int lutPower = std::clamp(
            context.taperPowerIndex - kTaperPowerSettingMin,
            0,
            kTaperPowerCount - 1);
        const float* row = context.taperLut[lutPower];
        const float scale =
            static_cast<float>(kTaperLutSteps) /
            static_cast<float>(pointCount - 1);
        for (int i = 0; i + 1 < pointCount; ++i) {
            const float position = static_cast<float>(i) * scale;
            const int lutIndex = static_cast<int>(position);
            const float fraction = position - static_cast<float>(lutIndex);
            const float low = row[lutIndex];
            const float high = row[lutIndex + 1];
            taper[i] = low + (high - low) * fraction;
        }
        taper[pointCount - 1] = 0.0f;

        cachedTaperPointCount = pointCount;
        cachedTaperPowerIndex = context.taperPowerIndex;
        taperCacheValid = true;
}
void SmoothTrail(int iterations, int renderLength) {
    D2D1_POINT_2F* current = context.renderCache.pointsA;
    D2D1_POINT_2F* next = context.renderCache.pointsB;
    const int pointCount = std::min(context.historyCount, std::max(renderLength, 2));
    int currentCount = 0;
    const float offsetX = static_cast<float>(context.tailOffsetX) * context.currentDpiScale;
    const float offsetY = static_cast<float>(context.tailOffsetY) * context.currentDpiScale;
    for (int i = 0; i < pointCount; ++i) {
        const POINT& point = HistoryAt(i);
        current[currentCount++] = D2D1::Point2F(
            static_cast<float>(point.x) + offsetX,
            static_cast<float>(point.y) + offsetY);
    }
    const int iterationLimit = std::clamp(
        iterations, 0, kMaxSmoothIterations);
    for (int iteration = 0; iteration < iterationLimit && currentCount >= 3;
         ++iteration) {
        if (currentCount > kMaxTrailRenderPoints / 2) break;
        int nextCount = 0;
        next[nextCount++] = current[0];
        for (int i = 0; i + 1 < currentCount; ++i) {
            const D2D1_POINT_2F& a = current[i];
            const D2D1_POINT_2F& b = current[i + 1];
            next[nextCount++] = D2D1::Point2F(
                0.75f * a.x + 0.25f * b.x,
                0.75f * a.y + 0.25f * b.y);
            next[nextCount++] = D2D1::Point2F(
                0.25f * a.x + 0.75f * b.x,
                0.25f * a.y + 0.75f * b.y);
        }
        next[nextCount++] = current[currentCount - 1];
        currentCount = nextCount;
        D2D1_POINT_2F* swap = current;
        current = next;
        next = swap;
    }
    context.renderCache.activePoints = current;
    context.renderCache.pointCount = currentCount;
    ++context.renderCache.generation;
    context.renderCache.PrepareTaper();
}

RECT CalculateTrailBounds(float maxHalfWidth) {
    const int pointCount = context.renderCache.pointCount;
    if (pointCount <= 0) return {0, 0, 0, 0};
    const D2D1_POINT_2F* points = context.renderCache.activePoints;
    float minX = points[0].x;
    float minY = points[0].y;
    float maxX = minX;
    float maxY = minY;
    for (int i = 1; i < pointCount; ++i) {
        const D2D1_POINT_2F& point = points[i];
        minX = std::min(minX, point.x);
        minY = std::min(minY, point.y);
        maxX = std::max(maxX, point.x);
        maxY = std::max(maxY, point.y);
    }
    const float padding = maxHalfWidth + kRenderPadding;
    RECT result = {
        static_cast<LONG>(floorf(minX - padding)),
        static_cast<LONG>(floorf(minY - padding)),
        static_cast<LONG>(ceilf(maxX + padding)),
        static_cast<LONG>(ceilf(maxY + padding)),
    };
    if (result.right <= result.left) result.right = result.left + 1;
    if (result.bottom <= result.top) result.bottom = result.top + 1;
    return result;
}

static void DrawTrailStroke(ID2D1RenderTarget* target, ID2D1Brush* brush, float halfWidth, bool drawHead) {
    const int pointCount = context.renderCache.pointCount;
    const D2D1_POINT_2F* points = context.renderCache.activePoints;
    const float* taper = context.renderCache.taper;
    if (pointCount < 2 || !brush) return;
    for (int i = 0; i + 1 < pointCount; ++i) {
        const float width = std::max(0.1f, halfWidth * taper[i] * 2.0f);
        target->DrawLine(points[i], points[i + 1], brush, width, context.strokeStyle);
    }
    if (drawHead) target->FillEllipse(D2D1::Ellipse(points[0], halfWidth, halfWidth), brush);
}
static int EnsureTrailRibbon() {
    if (context.ribbonCache.valid &&
        context.ribbonCache.generation == context.renderCache.generation) {
        return context.ribbonCache.centerCount;
    }
    context.ribbonCache.valid = true;
    context.ribbonCache.generation = context.renderCache.generation;
    context.ribbonCache.centerCount = 0;
    const int total = context.renderCache.pointCount;
    if (total < 2) return 0;

    const D2D1_POINT_2F* points = context.renderCache.activePoints;
    const float* taper = context.renderCache.taper;
    const int last = total - 1;
    const float minSpacingSq =
        kTrailRibbonMinSpacingPx * kTrailRibbonMinSpacingPx;
    const auto taperAt = [taper, last](int index) {
        if (index <= 0) return taper[0];
        if (index >= last) return taper[last - 1];
        return (taper[index - 1] + taper[index]) * 0.5f;
    };
    const auto store = [](int index, D2D1_POINT_2F point, float width) {
        context.ribbonCache.centers[index] = point;
        context.ribbonCache.taper[index] = width;
    };

    int count = 0;
    store(count++, points[0], taper[0]);
    int step = (last + kTrailRibbonMaxCenterPoints - 2) /
               (kTrailRibbonMaxCenterPoints - 1);
    if (step < 1) step = 1;
    for (int index = step;
         index < last && count < kTrailRibbonMaxCenterPoints - 1;
         index += step) {
        const D2D1_POINT_2F& previous = context.ribbonCache.centers[count - 1];
        const float dx = points[index].x - previous.x;
        const float dy = points[index].y - previous.y;
        if (dx * dx + dy * dy < minSpacingSq) continue;
        store(count++, points[index], taperAt(index));
    }

    // The tail must stay exact so the visible taper ends at the last sample.
    // A near-duplicate tail replaces the previous center instead of adding a
    // zero-length segment whose direction would be undefined.
    const D2D1_POINT_2F& previous = context.ribbonCache.centers[count - 1];
    const float tailDx = points[last].x - previous.x;
    const float tailDy = points[last].y - previous.y;
    if (count == 1 || tailDx * tailDx + tailDy * tailDy >= minSpacingSq) {
        store(count++, points[last], taperAt(last));
    } else {
        store(count - 1, points[last], taperAt(last));
    }

    float lastDirX = 1.0f;
    float lastDirY = 0.0f;
    bool directionKnown = false;
    for (int i = 0; i < count; ++i) {
        float previousX = 0.0f;
        float previousY = 0.0f;
        float nextX = 0.0f;
        float nextY = 0.0f;
        if (i > 0) {
            previousX = context.ribbonCache.centers[i].x - context.ribbonCache.centers[i - 1].x;
            previousY = context.ribbonCache.centers[i].y - context.ribbonCache.centers[i - 1].y;
        }
        if (i + 1 < count) {
            nextX = context.ribbonCache.centers[i + 1].x - context.ribbonCache.centers[i].x;
            nextY = context.ribbonCache.centers[i + 1].y - context.ribbonCache.centers[i].y;
        }
        const float dirX = previousX + nextX;
        const float dirY = previousY + nextY;
        const float lengthSq = dirX * dirX + dirY * dirY;
        if (lengthSq > 1.0e-8f) {
            const float inverseLength = 1.0f / sqrtf(lengthSq);
            lastDirX = dirX * inverseLength;
            lastDirY = dirY * inverseLength;
            directionKnown = true;
        } else if (!directionKnown) {
            // A stationary prefix repeats one point. Look ahead once for the
            // first real segment so the whole prefix shares its normal.
            for (int next = i + 1; next < count; ++next) {
                const float forwardX =
                    context.ribbonCache.centers[next].x - context.ribbonCache.centers[i].x;
                const float forwardY =
                    context.ribbonCache.centers[next].y - context.ribbonCache.centers[i].y;
                const float forwardLengthSq = forwardX * forwardX + forwardY * forwardY;
                if (forwardLengthSq <= 1.0e-8f) continue;
                const float inverseLength = 1.0f / sqrtf(forwardLengthSq);
                lastDirX = forwardX * inverseLength;
                lastDirY = forwardY * inverseLength;
                directionKnown = true;
                break;
            }
        }
        // The averaged unit direction keeps every offset at or below the half
        // width, so the trail bounds never need to grow for a miter spike.
        context.ribbonCache.normalX[i] = -lastDirY;
        context.ribbonCache.normalY[i] = lastDirX;
        // Zero-length sides only occur at the endpoints, which the head ellipse
        // and the tail cap already round, so this marks interior reversals only.
        context.ribbonCache.roundJoin[i] =
            previousX * previousX + previousY * previousY > 1.0e-8f &&
            nextX * nextX + nextY * nextY > 1.0e-8f &&
            previousX * nextX + previousY * nextY < 0.0f;
    }

    context.ribbonCache.centerCount = count;
    return count;
}

static int BuildTrailRibbonOutline(float halfWidth) {
    const int count = EnsureTrailRibbon();
    if (count < 2) return 0;
    const D2D1_POINT_2F* centers = context.ribbonCache.centers;
    D2D1_POINT_2F* outline = context.ribbonOutline;
    int outCount = 0;
    for (int i = 0; i < count; ++i) {
        const float scaled = halfWidth * context.ribbonCache.taper[i];
        const float width =
            scaled > kTrailRibbonMinHalfWidth ? scaled : kTrailRibbonMinHalfWidth;
        outline[outCount++] = D2D1::Point2F(
            centers[i].x + context.ribbonCache.normalX[i] * width,
            centers[i].y + context.ribbonCache.normalY[i] * width);
    }
    const int tail = count - 1;
    const float tailWidth = halfWidth * context.ribbonCache.taper[tail];
    const float tailRadius =
        tailWidth > kTrailRibbonMinHalfWidth ? tailWidth : kTrailRibbonMinHalfWidth;
    const float normalX = context.ribbonCache.normalX[tail];
    const float normalY = context.ribbonCache.normalY[tail];
    // The stored normal is the left offset, so the travel direction is the
    // normal rotated to the right. The cap bulges along that direction.
    const float capX = normalY;
    const float capY = -normalX;
    for (int step = 0; step < kTrailRibbonCapSegments - 1; ++step) {
        const float cosine = kTrailRibbonCapCos[step] * tailRadius;
        const float sine = kTrailRibbonCapSin[step] * tailRadius;
        outline[outCount++] = D2D1::Point2F(
            centers[tail].x + normalX * cosine + capX * sine,
            centers[tail].y + normalY * cosine + capY * sine);
    }
    for (int i = tail; i >= 0; --i) {
        const float scaled = halfWidth * context.ribbonCache.taper[i];
        const float width =
            scaled > kTrailRibbonMinHalfWidth ? scaled : kTrailRibbonMinHalfWidth;
        outline[outCount++] = D2D1::Point2F(
            centers[i].x - context.ribbonCache.normalX[i] * width,
            centers[i].y - context.ribbonCache.normalY[i] * width);
    }
    return outCount;
}

static bool FillTrailRibbon(
    ID2D1RenderTarget* target,
    ID2D1Brush* brush,
    float halfWidth) {
    if (!target || !brush) return false;
    const int outlineCount = BuildTrailRibbonOutline(halfWidth);
    if (outlineCount < 3) return false;
    ID2D1PathGeometry* geometry = nullptr;
    if (FAILED(context.d2dFactory->CreatePathGeometry(&geometry)) || !geometry) {
        return false;
    }
    ID2D1GeometrySink* sink = nullptr;
    if (FAILED(geometry->Open(&sink)) || !sink) {
        geometry->Release();
        return false;
    }
    sink->SetFillMode(D2D1_FILL_MODE_WINDING);
    sink->BeginFigure(context.ribbonOutline[0], D2D1_FIGURE_BEGIN_FILLED);
    for (int i = 1; i < outlineCount; ++i) sink->AddLine(context.ribbonOutline[i]);
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    const HRESULT hr = sink->Close();
    sink->Release();
    if (FAILED(hr)) {
        geometry->Release();
        return false;
    }
    target->FillGeometry(geometry, brush);
    geometry->Release();
    const int centerCount = context.ribbonCache.centerCount;
    for (int i = 1; i + 1 < centerCount; ++i) {
        if (!context.ribbonCache.roundJoin[i]) continue;
        float radius = halfWidth * context.ribbonCache.taper[i];
        if (radius < kTrailRibbonMinHalfWidth) radius = kTrailRibbonMinHalfWidth;
        if (radius > halfWidth) radius = halfWidth;
        if (radius <= 0.0f) continue;
        target->FillEllipse(
            D2D1::Ellipse(context.ribbonCache.centers[i], radius, radius), brush);
    }
    return true;
}

static void DrawTrailLayer(
    ID2D1RenderTarget* target,
    ID2D1Brush* brush,
    float halfWidth) {
    if (FillTrailRibbon(target, brush, halfWidth)) {
        const D2D1_POINT_2F& head = context.ribbonCache.centers[0];
        target->FillEllipse(
            D2D1::Ellipse(head, halfWidth, halfWidth), brush);
        return;
    }
    DrawTrailStroke(target, brush, halfWidth, true);
}

static int GetEffectiveTailLength(float speedNorm) {
    if (!context.speedScaling) return context.tailLength;
    return std::max(2, static_cast<int>(context.tailLength * (0.55f + 0.45f * std::clamp(speedNorm, 0.0f, 1.0f))));
}

void UpdateTrailState(const POINT& previousPoint, const POINT& point, float velocity) {
    const bool startedSmearing = velocity > context.triggerVelocity && !context.isSmearing;
    if (startedSmearing) {
        context.isSmearing = true;
        context.lowVelocityFrames = 0;
        context.fadeAlpha = 1.0f;
        context.smoothedSpeedNorm = 0.0f;
    } else if (velocity < context.stopVelocity && context.isSmearing) {
        if (++context.lowVelocityFrames > 2) {
            context.isSmearing = false;
            context.frozenSpeedNorm = context.smoothedSpeedNorm;
        }
    } else if (context.isSmearing) {
        context.lowVelocityFrames = 0;
    }
    if (context.isSmearing) {
        if (startedSmearing) {
            // A fading tail belongs to the previous motion segment. Do not connect
            // it to a new segment after a discontinuous cursor repositioning.
            HistoryClear();
        }
        const float target = std::clamp((velocity - context.stopVelocity) / (context.triggerVelocity * 2.0f), 0.0f, 1.0f);
        context.smoothedSpeedNorm = context.smoothedSpeedNorm * 0.55f + target * 0.45f;
        const float dx = static_cast<float>(point.x - previousPoint.x);
        const float dy = static_cast<float>(point.y - previousPoint.y);
        const float distance = sqrtf(dx * dx + dy * dy);
        int interpolationSteps = 1;
        // Start from the current point only. A discontinuous cursor repositioning
        // must not be turned into a streak when the velocity gate first opens.
        if (!startedSmearing
            && context.fastMoveInterpolationEnabled
            && distance > kFastMoveInterpolationSpacingPx) {
            const int requiredSteps = static_cast<int>(ceilf(distance / kFastMoveInterpolationSpacingPx));
            interpolationSteps = std::clamp(requiredSteps, 1, context.maxInterpolationSteps + 1);
        }
        // History is stored newest-first, so push the interpolated path toward
        // the current endpoint in ascending order. Work is capped at six points.
        for (int step = 1; step <= interpolationSteps; ++step) {
            const float progress = static_cast<float>(step) / interpolationSteps;
            const POINT sample = {
                static_cast<LONG>(std::lround(previousPoint.x + dx * progress)),
                static_cast<LONG>(std::lround(previousPoint.y + dy * progress)),
            };
            HistoryPushFront(sample);
            while (context.historyCount > context.tailLength) HistoryPopBack();
        }
    } else if (context.fadeEnabled) {
        context.fadeAlpha *= context.fadeDecay;
        if (context.fadeAlpha < kFadeCutoff || context.historyCount < 2) {
            HistoryClear();
            context.fadeAlpha = 0.0f;
        }
    } else {
        if (context.historyCount > 0) HistoryPopBack();
        context.fadeAlpha = 1.0f;
    }
}
