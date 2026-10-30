#pragma once

// Ornament assets, physics and drawing.

#include "platform.h"
#include "context.h"
#include "math_utils.h"
#include "types.h"
#include "forward.h"

static D2D1_POINT_2F OrnamentAnchorPoint() {
    return D2D1::Point2F(
        static_cast<float>(context.lastPos.x) + context.ornamentAnchorOffsetX * context.currentDpiScale,
        static_cast<float>(context.lastPos.y) + context.ornamentAnchorOffsetY * context.currentDpiScale);
}

static void ReleaseOrnamentBitmaps() {
    context.ornamentBitmapsSettled = false;
    for (OrnamentImage& image : context.ornamentImages) {
        if (image.bitmap) {
            image.bitmap->Release();
            image.bitmap = nullptr;
        }
        // A new target can upload again, so clear the upload latch. A decode
        // failure stays latched until the assets are rebuilt from settings.
        image.uploadFailed = false;
    }
}

static void ReleaseOrnamentAssets() {
    ReleaseOrnamentBitmaps();
    context.ornamentImages.clear();
    context.ornamentNodes.clear();
    if (context.wicFactory) {
        context.wicFactory->Release();
        context.wicFactory = nullptr;
    }
    context.ornamentAssetsReady = false;
    context.ornamentPhysicsInitialized = false;
    context.ornamentRedrawPending = false;
    context.ornamentSmoothedSpeedNorm = 0.0f;
    context.ornamentSmoothedSpeedPx = 0.0f;
}

static int FindOrnamentImageIndexById(const std::wstring& id) {
    if (id.empty()) return -1;
    for (size_t i = 0; i < context.ornamentImages.size(); ++i) {
        if (_wcsicmp(context.ornamentImages[i].id.c_str(), id.c_str()) == 0) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

static void ReleaseOrnamentPixels(std::vector<uint8_t>& pixels) {
    std::vector<uint8_t>().swap(pixels);
}

// The build step reads only image metadata. Keeping the decoded pixels for
// every image would hold up to 128 MB of CPU memory while the pendant sits
// idle, so the pixels are decoded again on demand and freed after the upload.
static bool ProbeOrnamentImage(
    const OrnamentImageSpec& spec,
    OrnamentImage& output) {
    output.id = spec.id;
    output.path = spec.path;
    if (!context.wicFactory) return false;

    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    const auto releaseInterfaces = [&] {
        if (frame) { frame->Release(); frame = nullptr; }
        if (decoder) { decoder->Release(); decoder = nullptr; }
    };

    HRESULT hr = context.wicFactory->CreateDecoderFromFilename(
        spec.path.c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,
        &decoder);
    if (FAILED(hr) || !decoder) {
        Wh_Log(L"Failed to open ornament image: %s", spec.path.c_str());
        releaseInterfaces();
        return false;
    }
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr) || !frame) {
        Wh_Log(L"Failed to read first frame of ornament image: %s", spec.path.c_str());
        releaseInterfaces();
        return false;
    }
    UINT width = 0;
    UINT height = 0;
    hr = frame->GetSize(&width, &height);
    if (FAILED(hr)
        || width == 0
        || height == 0
        || width > static_cast<UINT>(kOrnamentMaxImageDimension)
        || height > static_cast<UINT>(kOrnamentMaxImageDimension)) {
        Wh_Log(L"Unsupported ornament image dimensions: %s", spec.path.c_str());
        releaseInterfaces();
        return false;
    }
    const uint64_t imageBytes = static_cast<uint64_t>(width) * height * 4ull;
    if (imageBytes > kOrnamentMaxDecodedBytes) {
        Wh_Log(L"Ornament image memory limit reached: %s", spec.path.c_str());
        releaseInterfaces();
        return false;
    }
    output.width = width;
    output.height = height;
    releaseInterfaces();
    return true;
}

static bool DecodeOrnamentPixels(OrnamentImage& image) {
    if (!context.wicFactory || image.path.empty()) return false;

    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;
    const auto releaseInterfaces = [&] {
        if (converter) { converter->Release(); converter = nullptr; }
        if (frame) { frame->Release(); frame = nullptr; }
        if (decoder) { decoder->Release(); decoder = nullptr; }
    };

    HRESULT hr = context.wicFactory->CreateDecoderFromFilename(
        image.path.c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,
        &decoder);
    if (FAILED(hr) || !decoder) {
        Wh_Log(L"Failed to open ornament image: %s", image.path.c_str());
        releaseInterfaces();
        return false;
    }
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr) || !frame) {
        Wh_Log(L"Failed to read first frame of ornament image: %s", image.path.c_str());
        releaseInterfaces();
        return false;
    }
    UINT width = 0;
    UINT height = 0;
    hr = frame->GetSize(&width, &height);
    if (FAILED(hr)
        || width == 0
        || height == 0
        || width > static_cast<UINT>(kOrnamentMaxImageDimension)
        || height > static_cast<UINT>(kOrnamentMaxImageDimension)) {
        Wh_Log(L"Unsupported ornament image dimensions: %s", image.path.c_str());
        releaseInterfaces();
        return false;
    }
    const uint64_t imageBytes = static_cast<uint64_t>(width) * height * 4ull;
    if (imageBytes > kOrnamentMaxDecodedBytes) {
        Wh_Log(L"Ornament image memory limit reached: %s", image.path.c_str());
        releaseInterfaces();
        return false;
    }
    hr = context.wicFactory->CreateFormatConverter(&converter);
    if (FAILED(hr) || !converter) {
        releaseInterfaces();
        return false;
    }
    hr = converter->Initialize(
        frame,
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        Wh_Log(L"Failed to convert ornament image pixels: %s", image.path.c_str());
        releaseInterfaces();
        return false;
    }
    const UINT stride = width * 4u;
    const UINT bufferSize = stride * height;
    image.pixels.resize(bufferSize);
    hr = converter->CopyPixels(nullptr, stride, bufferSize, image.pixels.data());
    if (FAILED(hr)) {
        ReleaseOrnamentPixels(image.pixels);
        Wh_Log(L"Failed to copy ornament image pixels: %s", image.path.c_str());
        releaseInterfaces();
        return false;
    }
    image.width = width;
    image.height = height;
    releaseInterfaces();
    return true;
}

static void BuildOrnamentImages() {
    context.ornamentImages.clear();
    context.ornamentImages.reserve(context.ornamentImageSpecs.size());
    if (!context.ornamentImageSpecs.empty()) {
        const HRESULT hr = CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&context.wicFactory));
        if (FAILED(hr) || !context.wicFactory) {
            Wh_Log(L"Failed to create WIC imaging factory for ornaments");
        }
    }
    for (const OrnamentImageSpec& spec : context.ornamentImageSpecs) {
        OrnamentImage image;
        if (!ProbeOrnamentImage(spec, image)) image.decodeFailed = true;
        context.ornamentImages.push_back(std::move(image));
    }
}

static void BuildOrnamentNodes() {
    context.ornamentNodes.clear();
    context.ornamentNodes.reserve(context.ornamentNodeSpecs.size());
    for (const OrnamentNodeSpec& spec : context.ornamentNodeSpecs) {
        OrnamentNode node;
        // A node without a decoded image still renders the circular fallback,
        // so the index stays -1 and the preset remains usable with no image.
        node.imageIndex = FindOrnamentImageIndexById(spec.imageId);
        node.ropeT = spec.ropeT;
        node.sizePx = spec.sizePx;
        node.angleDegrees = spec.angleDegrees;
        node.pivotX = spec.pivotX;
        node.pivotY = spec.pivotY;
        node.opacity = spec.opacity;
        context.ornamentNodes.push_back(node);
    }
}

static void ResetOrnamentPhysics() {
    const int segments = std::clamp(context.ornamentRopeSegments, 1, kOrnamentMaxRopeSegments);
    context.ornamentRopePointCount = segments + 1;
    const float length = std::max(1.0f, context.ornamentRopeLengthPx * context.currentDpiScale);
    context.ornamentRopeSegmentPx = length / static_cast<float>(segments);
    D2D1_POINT_2F anchor = OrnamentAnchorPoint();
    for (int i = 0; i < context.ornamentRopePointCount; ++i) {
        const D2D1_POINT_2F point = D2D1::Point2F(
            anchor.x,
            anchor.y + context.ornamentRopeSegmentPx * static_cast<float>(i));
        context.ornamentRopePoints[i] = point;
        context.ornamentRopePrevious[i] = point;
    }
    context.ornamentLastAnchor = anchor;
    context.ornamentSmoothedSpeedNorm = 0.0f;
    context.ornamentSmoothedSpeedPx = 0.0f;
    context.ornamentPhysicsInitialized = true;
    context.ornamentRedrawPending = true;
}

static void EnsureOrnamentAssetsLoaded() {
    if (!context.ornamentEnabled) {
        if (context.ornamentAssetsReady || !context.ornamentImages.empty()) ReleaseOrnamentAssets();
        return;
    }
    const bool dirty = context.ornamentAssetsDirty.exchange(false, std::memory_order_acq_rel);
    if (!dirty && context.ornamentAssetsReady) return;
    ReleaseOrnamentAssets();
    BuildOrnamentImages();
    BuildOrnamentNodes();
    context.ornamentAssetsReady = true;
    context.ornamentPhysicsInitialized = false;
    context.ornamentRedrawPending = true;
}

static void EnsureOrnamentBitmaps() {
    if (!context.renderTarget || !context.ornamentAssetsReady) return;
    if (context.ornamentBitmapsSettled) return;
    for (OrnamentImage& image : context.ornamentImages) {
        if (image.bitmap || image.decodeFailed || image.uploadFailed) continue;
        // One image at a time, so the peak CPU memory never holds more than a
        // single decoded image. Metadata is probed once at build time, so an
        // unreadable file is latched and never reopened on later frames.
        if (image.width == 0 || image.height == 0) {
            image.decodeFailed = true;
            continue;
        }
        // Pixels are released after every upload, so an image without a device
        // bitmap is always decoded again here before it can be drawn.
        if (!DecodeOrnamentPixels(image)) {
            image.decodeFailed = true;
            ReleaseOrnamentPixels(image.pixels);
            continue;
        }
        const D2D1_BITMAP_PROPERTIES properties = D2D1::BitmapProperties(
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
            96.0f,
            96.0f);
        const D2D1_SIZE_U size = D2D1::SizeU(image.width, image.height);
        const UINT stride = image.width * 4u;
        const HRESULT hr = context.renderTarget->CreateBitmap(
            size,
            image.pixels.data(),
            stride,
            properties,
            &image.bitmap);
        if (FAILED(hr)) {
            image.bitmap = nullptr;
            image.uploadFailed = true;
            ReleaseOrnamentPixels(image.pixels);
            Wh_Log(L"Failed to upload ornament image: %s", image.path.c_str());
            continue;
        }
        // The upload copied the pixels into the device bitmap. Holding the CPU
        // copy would keep the decoded size per image resident for no benefit.
        ReleaseOrnamentPixels(image.pixels);
    }
    // Every branch above ends with a device bitmap or a latched failure, so
    // one pass settles the list. ReleaseOrnamentBitmaps clears the latch.
    context.ornamentBitmapsSettled = true;
}

static void UpdateOrnamentNodeGeometry(OrnamentNode& node) {
    float aspect = 1.0f;
    if (node.imageIndex >= 0
        && node.imageIndex < static_cast<int>(context.ornamentImages.size())) {
        const OrnamentImage& image = context.ornamentImages[static_cast<size_t>(node.imageIndex)];
        if (image.width > 0 && image.height > 0) {
            aspect = static_cast<float>(image.width) / static_cast<float>(image.height);
        }
    }
    const float size = std::max(1.0f, node.sizePx * context.currentDpiScale);
    const float width = aspect >= 1.0f ? size : size * aspect;
    const float height = aspect >= 1.0f ? size / aspect : size;
    node.localBounds = D2D1::RectF(
        -node.pivotX * width,
        -node.pivotY * height,
        (1.0f - node.pivotX) * width,
        (1.0f - node.pivotY) * height);
}

static D2D1_MATRIX_3X2_F OrnamentNodeMatrix(const OrnamentNode& node) {
    return D2D1::Matrix3x2F::Scale(node.scaleX, node.scaleY)
        * D2D1::Matrix3x2F::Rotation(node.drawAngleDegrees)
        * D2D1::Matrix3x2F::Translation(node.position.x, node.position.y);
}

static D2D1_POINT_2F OrnamentRopePointAt(float t) {
    const int count = context.ornamentRopePointCount;
    if (count <= 0) return D2D1::Point2F(0.0f, 0.0f);
    if (count == 1) return context.ornamentRopePoints[0];
    const float location =
        std::clamp(t, 0.0f, 1.0f) * static_cast<float>(count - 1);
    const int index = std::min(static_cast<int>(location), count - 2);
    const float fraction = location - static_cast<float>(index);
    const D2D1_POINT_2F& a = context.ornamentRopePoints[index];
    const D2D1_POINT_2F& b = context.ornamentRopePoints[index + 1];
    return D2D1::Point2F(
        a.x + (b.x - a.x) * fraction,
        a.y + (b.y - a.y) * fraction);
}

static void UpdateOrnamentDrawStates() {
    if (context.ornamentNodes.empty()) return;
    if (context.ornamentRopePointCount < 2) return;
    const float speed = std::clamp(context.ornamentSmoothedSpeedNorm, 0.0f, 1.0f);
    const float stretchScale = 1.0f + context.ornamentStretch * speed;
    const float compensatingScale = 1.0f / sqrtf(std::max(0.01f, stretchScale));
    for (OrnamentNode& node : context.ornamentNodes) {
        const float t = std::clamp(node.ropeT, 0.0f, 1.0f);
        node.position = OrnamentRopePointAt(t);
        const D2D1_POINT_2F before = OrnamentRopePointAt(t - 0.02f);
        const D2D1_POINT_2F after = OrnamentRopePointAt(t + 0.02f);
        const float dx = after.x - before.x;
        const float dy = after.y - before.y;
        if (dx * dx + dy * dy > kOrnamentConstraintEpsilonPx) {
            node.drawAngleDegrees =
                atan2f(-dx, dy) * 180.0f / kOrnamentPi + node.angleDegrees;
        } else {
            node.drawAngleDegrees = node.angleDegrees;
        }
        node.scaleY = stretchScale;
        node.scaleX = compensatingScale;
        UpdateOrnamentNodeGeometry(node);
    }
}

static void ConstrainOrnamentDistance(
    D2D1_POINT_2F& moving,
    D2D1_POINT_2F& fixed,
    float targetDistance,
    bool moveBoth) {
    float dx = moving.x - fixed.x;
    float dy = moving.y - fixed.y;
    float distance = sqrtf(dx * dx + dy * dy);
    if (distance < kOrnamentConstraintEpsilonPx) {
        moving = D2D1::Point2F(fixed.x, fixed.y + targetDistance);
        return;
    }
    const float correction = (distance - targetDistance) / distance;
    if (moveBoth) {
        const float half = correction * 0.5f;
        moving.x -= dx * half;
        moving.y -= dy * half;
        fixed.x += dx * half;
        fixed.y += dy * half;
    } else {
        moving.x -= dx * correction;
        moving.y -= dy * correction;
    }
}

static void StepOrnamentPhysics(
    float dtSeconds,
    float retain,
    float anchorStepX,
    float anchorStepY) {
    const int count = context.ornamentRopePointCount;
    if (count < 2) return;
    const float gravityStep = context.ornamentGravity * context.currentDpiScale * dtSeconds * dtSeconds;
    for (int i = 1; i < count; ++i) {
        D2D1_POINT_2F& point = context.ornamentRopePoints[i];
        // Verlet velocity is world-space. Damping must act on the velocity
        // relative to the moving anchor; otherwise dragging the cursor at a
        // constant speed looks like wind and tilts the rope by a speed-dependent
        // angle. Subtract the frame displacement, then add it back to position.
        const float vx = point.x - context.ornamentRopePrevious[i].x - anchorStepX;
        const float vy = point.y - context.ornamentRopePrevious[i].y - anchorStepY;
        context.ornamentRopePrevious[i] = point;
        point.x += vx * retain + anchorStepX;
        point.y += vy * retain + anchorStepY + gravityStep;
    }

    D2D1_POINT_2F anchor = OrnamentAnchorPoint();
    for (int iteration = 0; iteration < context.ornamentConstraintIterations; ++iteration) {
        context.ornamentRopePoints[0] = anchor;
        ConstrainOrnamentDistance(
            context.ornamentRopePoints[1],
            anchor,
            context.ornamentRopeSegmentPx,
            false);
        for (int i = 1; i < count - 1; ++i) {
            ConstrainOrnamentDistance(
                context.ornamentRopePoints[i + 1],
                context.ornamentRopePoints[i],
                context.ornamentRopeSegmentPx,
                true);
        }
    }
    context.ornamentRopePoints[0] = anchor;
    for (int i = 0; i < count; ++i) {
        if (!std::isfinite(context.ornamentRopePoints[i].x)
            || !std::isfinite(context.ornamentRopePoints[i].y)) {
            ResetOrnamentPhysics();
            return;
        }
    }
}

static bool OrnamentAnimating() {
    for (int i = 1; i < context.ornamentRopePointCount; ++i) {
        const float dx = context.ornamentRopePoints[i].x - context.ornamentRopePrevious[i].x;
        const float dy = context.ornamentRopePoints[i].y - context.ornamentRopePrevious[i].y;
        if (dx * dx + dy * dy > kOrnamentRestVelocityPx * kOrnamentRestVelocityPx) return true;
    }
    return false;
}

static void UpdateOrnamentPhysics(float velocity, LONGLONG nowQpc) {
    const bool visible = context.ornamentEnabled && context.ornamentAssetsReady && !context.ornamentNodes.empty();
    if (!visible) {
        context.ornamentPhysicsInitialized = false;
        context.ornamentRedrawPending = false;
        context.ornamentSmoothedSpeedPx = 0.0f;
        context.ornamentSmoothedSpeedNorm = 0.0f;
        return;
    }

    const float targetSpeedPx = std::max(0.0f, velocity);
    context.ornamentSmoothedSpeedPx += (targetSpeedPx - context.ornamentSmoothedSpeedPx)
        * kOrnamentSpeedSmoothing;
    context.ornamentSmoothedSpeedNorm = std::clamp(
        context.ornamentSmoothedSpeedPx / kOrnamentSpeedReferencePxPerSecond,
        0.0f,
        1.0f);

    if (!context.ornamentPhysicsInitialized) {
        ResetOrnamentPhysics();
        context.ornamentLastStepQpc = nowQpc;
    }

    const D2D1_POINT_2F anchor = OrnamentAnchorPoint();
    const float anchorStepX = anchor.x - context.ornamentLastAnchor.x;
    const float anchorStepY = anchor.y - context.ornamentLastAnchor.y;

    float elapsed = 0.0f;
    if (context.ornamentLastStepQpc > 0 && nowQpc > context.ornamentLastStepQpc && context.qpcFrequency > 0) {
        elapsed = static_cast<float>(nowQpc - context.ornamentLastStepQpc)
            / static_cast<float>(context.qpcFrequency);
    }
    elapsed = std::clamp(elapsed, 0.0f, kOrnamentMaxCatchUpSeconds);
    context.ornamentLastStepQpc = nowQpc;
    if (elapsed > 0.0f) {
        const float fixedStep = 1.0f / kOrnamentSimulationHz;
        int steps = std::max(1, static_cast<int>(ceilf(elapsed / fixedStep)));
        steps = std::min(steps, kOrnamentMaxSubSteps);
        const float step = elapsed / static_cast<float>(steps);
        const float stepX = anchorStepX / static_cast<float>(steps);
        const float stepY = anchorStepY / static_cast<float>(steps);
        // Damping is the same for every substep, so the exponential is
        // evaluated once per frame instead of once per substep.
        const float retain = powf(
            std::clamp(1.0f - context.ornamentDamping / 100.0f, 0.5f, 1.0f),
            step * kOrnamentSimulationHz);
        for (int i = 0; i < steps; ++i) {
            StepOrnamentPhysics(step, retain, stepX, stepY);
        }
        context.ornamentLastAnchor = anchor;
    }

    if (OrnamentAnimating()) {
        context.ornamentRedrawPending = true;
    }
}

static bool OrnamentVisible() {
    return context.ornamentEnabled
        && context.ornamentAssetsReady
        && !context.ornamentNodes.empty()
        && context.ornamentOpacity > 0.0f;
}

static bool OrnamentActive() {
    return OrnamentVisible()
        && (context.ornamentRedrawPending || OrnamentAnimating());
}

// A click or wheel impulse is a one-shot velocity applied to the whole chain.
static void QueueOrnamentImpulse(float impulseY, LONGLONG nowQpc) {
    if (!OrnamentVisible()) return;
    if (!context.ornamentPhysicsInitialized) {
        ResetOrnamentPhysics();
    }
    // Rebase the simulation clock so the impulse is not consumed by a catch-up
    // step covering the idle interval before the input.
    context.ornamentLastStepQpc = nowQpc;
    // The impulse is a velocity in pixels per second applied once. Verlet keeps
    // velocity in position deltas, so convert it with the fixed simulation step.
    const float stepSeconds = 1.0f / kOrnamentSimulationHz;
    const float deltaY = impulseY * stepSeconds;
    for (int i = 1; i < context.ornamentRopePointCount; ++i) {
        context.ornamentRopePrevious[i].y += deltaY;
    }
    context.ornamentRedrawPending = true;
    context.interactionPending = true;
    if (context.inputWakeEvent) SetEvent(context.inputWakeEvent);
}

static void SuppressOrnament(LONGLONG nowQpc) {
    context.ornamentPhysicsInitialized = false;
    context.ornamentLastStepQpc = nowQpc;
    context.ornamentRedrawPending = false;
    context.ornamentSmoothedSpeedPx = 0.0f;
    context.ornamentSmoothedSpeedNorm = 0.0f;
}

static RECT CalculateOrnamentBounds() {
    RECT bounds = {};
    bool hasBounds = false;
    const float padding = 2.0f * context.currentDpiScale;
    const float ropeOpacity =
        std::clamp(context.ornamentRopeOpacity * context.ornamentOpacity, 0.0f, 1.0f);
    if (context.ornamentRopePointCount >= 2 && ropeOpacity > kOrnamentMinDrawOpacity) {
        const float ropePadding =
            padding + context.ornamentRopeWidthPx * context.currentDpiScale * 4.0f;
        float minX = std::numeric_limits<float>::max();
        float minY = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest();
        float maxY = std::numeric_limits<float>::lowest();
        for (int i = 0; i < context.ornamentRopePointCount; ++i) {
            minX = std::min(minX, context.ornamentRopePoints[i].x);
            minY = std::min(minY, context.ornamentRopePoints[i].y);
            maxX = std::max(maxX, context.ornamentRopePoints[i].x);
            maxY = std::max(maxY, context.ornamentRopePoints[i].y);
        }
        bounds.left = static_cast<LONG>(floorf(minX - ropePadding));
        bounds.top = static_cast<LONG>(floorf(minY - ropePadding));
        bounds.right = static_cast<LONG>(ceilf(maxX + ropePadding));
        bounds.bottom = static_cast<LONG>(ceilf(maxY + ropePadding));
        hasBounds = true;
    }
    for (const OrnamentNode& node : context.ornamentNodes) {
        if (node.opacity * context.ornamentOpacity <= kOrnamentMinDrawOpacity) continue;
        const D2D1_MATRIX_3X2_F matrix = OrnamentNodeMatrix(node);
        const D2D1_POINT_2F corners[4] = {
            D2D1::Point2F(node.localBounds.left, node.localBounds.top),
            D2D1::Point2F(node.localBounds.right, node.localBounds.top),
            D2D1::Point2F(node.localBounds.right, node.localBounds.bottom),
            D2D1::Point2F(node.localBounds.left, node.localBounds.bottom),
        };
        float minX = std::numeric_limits<float>::max();
        float minY = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest();
        float maxY = std::numeric_limits<float>::lowest();
        for (const D2D1_POINT_2F& corner : corners) {
            const D2D1_POINT_2F point = D2D1::Point2F(
                corner.x * matrix._11 + corner.y * matrix._21 + matrix._31,
                corner.x * matrix._12 + corner.y * matrix._22 + matrix._32);
            minX = std::min(minX, point.x);
            minY = std::min(minY, point.y);
            maxX = std::max(maxX, point.x);
            maxY = std::max(maxY, point.y);
        }
        const RECT candidate = {
            static_cast<LONG>(floorf(minX - padding)),
            static_cast<LONG>(floorf(minY - padding)),
            static_cast<LONG>(ceilf(maxX + padding)),
            static_cast<LONG>(ceilf(maxY + padding)),
        };
        if (!hasBounds) {
            bounds = candidate;
            hasBounds = true;
        } else {
            bounds.left = std::min(bounds.left, candidate.left);
            bounds.top = std::min(bounds.top, candidate.top);
            bounds.right = std::max(bounds.right, candidate.right);
            bounds.bottom = std::max(bounds.bottom, candidate.bottom);
        }
    }
    if (hasBounds) {
        if (bounds.right <= bounds.left) bounds.right = bounds.left + 1;
        if (bounds.bottom <= bounds.top) bounds.bottom = bounds.top + 1;
    }
    return bounds;
}

static int SmoothOrnamentRope(
    const D2D1_POINT_2F* input,
    int count,
    D2D1_POINT_2F* output,
    int capacity) {
    if (count < 2 || capacity < 3) return 0;
    int written = 0;
    output[written++] = input[0];
    for (int i = 0; i + 1 < count && written + 2 < capacity; ++i) {
        const D2D1_POINT_2F& a = input[i];
        const D2D1_POINT_2F& b = input[i + 1];
        output[written++] = D2D1::Point2F(
            a.x * 0.75f + b.x * 0.25f,
            a.y * 0.75f + b.y * 0.25f);
        output[written++] = D2D1::Point2F(
            a.x * 0.25f + b.x * 0.75f,
            a.y * 0.25f + b.y * 0.75f);
    }
    output[written++] = input[count - 1];
    return written;
}

static void StrokeOrnamentRope(
    const D2D1_POINT_2F* points,
    int count,
    float width,
    float alpha,
    uint32_t rgb,
    float taper) {
    if (!context.ornamentBrush || count < 2) return;
    for (int i = 0; i + 1 < count; ++i) {
        const float t =
            (static_cast<float>(i) + 0.5f) / static_cast<float>(count - 1);
        const float segmentAlpha =
            std::clamp(alpha * (0.5f + 0.5f * t), 0.0f, 1.0f);
        context.ornamentBrush->SetColor(ToColorF(rgb, segmentAlpha));
        context.renderTarget->DrawLine(
            points[i],
            points[i + 1],
            context.ornamentBrush,
            width * (1.0f + taper * t),
            context.strokeStyle);
    }
}

static void DrawOrnamentRope() {
    if (!context.ornamentBrush || context.ornamentRopePointCount < 2) return;
    const float alpha =
        std::clamp(context.ornamentRopeOpacity * context.ornamentOpacity, 0.0f, 1.0f);
    if (alpha <= kOrnamentMinDrawOpacity) return;

    const D2D1_POINT_2F* points = context.ornamentRopePoints;
    int count = context.ornamentRopePointCount;
    const int first = SmoothOrnamentRope(
        points,
        count,
        context.ornamentRopeSmoothA,
        kOrnamentMaxRopeDrawPoints);
    if (first >= 2) {
        const int second = SmoothOrnamentRope(
            context.ornamentRopeSmoothA,
            first,
            context.ornamentRopeSmoothB,
            kOrnamentMaxRopeDrawPoints);
        if (second >= 2) {
            points = context.ornamentRopeSmoothB;
            count = second;
        }
    }

    const float width = std::max(1.0f, context.ornamentRopeWidthPx * context.currentDpiScale);
    switch (context.ornamentRopeStyle) {
        case OrnamentRopeStyle::Silk:
            StrokeOrnamentRope(
                points, count, width, alpha, context.currentCoreRGB, -0.4f);
            break;
        case OrnamentRopeStyle::Neon:
            StrokeOrnamentRope(
                points, count, width * 3.6f, alpha * 0.16f, context.currentCoreRGB, 0.0f);
            StrokeOrnamentRope(
                points, count, width * 2.0f, alpha * 0.28f, context.currentCoreRGB, 0.0f);
            StrokeOrnamentRope(
                points, count, width, alpha, context.currentCoreRGB, 0.0f);
            break;
        case OrnamentRopeStyle::Chain:
            StrokeOrnamentRope(
                points, count, width * 0.55f, alpha * 0.6f, context.currentCoreRGB, 0.0f);
            for (int i = 1; i < context.ornamentRopePointCount; ++i) {
                const float t = static_cast<float>(i)
                    / static_cast<float>(context.ornamentRopePointCount - 1);
                context.ornamentBrush->SetColor(
                    ToColorF(context.currentCoreRGB, alpha * (0.45f + 0.55f * t)));
                const float radius = width * 1.6f;
                context.renderTarget->FillEllipse(
                    D2D1::Ellipse(context.ornamentRopePoints[i], radius, radius),
                    context.ornamentBrush);
            }
            break;
        case OrnamentRopeStyle::Ribbon:
            StrokeOrnamentRope(
                points, count, width * 3.2f, alpha * 0.3f, context.currentOuterRGB, 0.0f);
            StrokeOrnamentRope(
                points, count, width * 1.1f, alpha, context.currentCoreRGB, 0.0f);
            break;
    }
}

static void DrawOrnament(const RECT& renderBounds) {
    if (!context.ornamentAssetsReady || context.ornamentNodes.empty()) return;
    const D2D1_MATRIX_3X2_F baseTransform = D2D1::Matrix3x2F::Translation(
        -static_cast<float>(renderBounds.left),
        -static_cast<float>(renderBounds.top));
    context.renderTarget->SetTransform(baseTransform);
    DrawOrnamentRope();
    for (const OrnamentNode& node : context.ornamentNodes) {
        const float alpha = std::clamp(node.opacity * context.ornamentOpacity, 0.0f, 1.0f);
        if (alpha <= kOrnamentMinDrawOpacity) continue;
        context.renderTarget->SetTransform(OrnamentNodeMatrix(node) * baseTransform);
        const OrnamentImage* image = node.imageIndex >= 0
            && node.imageIndex < static_cast<int>(context.ornamentImages.size())
            ? &context.ornamentImages[static_cast<size_t>(node.imageIndex)]
            : nullptr;
        if (image && image->bitmap) {
            context.renderTarget->DrawBitmap(
                image->bitmap,
                &node.localBounds,
                alpha,
                D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                nullptr);
        } else if (context.ornamentBrush) {
            const float width = node.localBounds.right - node.localBounds.left;
            const float height = node.localBounds.bottom - node.localBounds.top;
            if (width <= 0.0f || height <= 0.0f) continue;
            context.ornamentBrush->SetColor(ToColorF(context.currentCoreRGB, alpha * 0.72f));
            context.renderTarget->FillEllipse(
                D2D1::Ellipse(
                    D2D1::Point2F(
                        (node.localBounds.left + node.localBounds.right) * 0.5f,
                        (node.localBounds.top + node.localBounds.bottom) * 0.5f),
                    width * 0.5f,
                    height * 0.5f),
                context.ornamentBrush);
        }
    }
    context.renderTarget->SetTransform(baseTransform);
}
