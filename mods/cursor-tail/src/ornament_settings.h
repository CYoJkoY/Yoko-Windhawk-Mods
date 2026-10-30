#pragma once

// Ornament preset selection and image specification parsing.

#include "platform.h"
#include "context.h"
#include "math_utils.h"
#include "types.h"
#include "forward.h"

static void TrimOrnamentText(std::wstring& text) {
    const size_t first = text.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos) {
        text.clear();
        return;
    }
    const size_t last = text.find_last_not_of(L" \t\r\n");
    text = text.substr(first, last - first + 1);
}

static bool LooksLikeOrnamentPath(const std::wstring& text) {
    return text.find(L'\\') != std::wstring::npos
        || text.find(L'/') != std::wstring::npos
        || text.find(L':') != std::wstring::npos;
}

// Preset keys are matched case-insensitively. An unknown value falls back to
// the first preset so a stale or hand-edited key still renders something.
static OrnamentPreset ParseOrnamentPreset(const wchar_t* value) {
    if (value && *value) {
        for (size_t i = 0; i < _countof(kOrnamentPresets); ++i) {
            if (_wcsicmp(kOrnamentPresets[i].key, value) == 0) {
                return static_cast<OrnamentPreset>(i);
            }
        }
    }
    return OrnamentPreset::Classic;
}

// The legacy `Ornament.images[0]` entry is read only when the new single-image
// setting is empty, so an upgrading user keeps the pendant they configured.
static void LoadOrnamentImageSpec() {
    context.ornamentImageSpecs.clear();
    std::wstring path;
    WindhawkUtils::StringSetting imageSetting =
        WindhawkUtils::StringSetting::make(L"Ornament.image");
    if (imageSetting.get()) path = imageSetting.get();
    TrimOrnamentText(path);
    if (path.empty()) {
        WindhawkUtils::StringSetting legacySetting =
            WindhawkUtils::StringSetting::make(L"Ornament.images[0]");
        if (legacySetting.get()) {
            std::wstring entry = legacySetting.get();
            TrimOrnamentText(entry);
            const size_t equals = entry.find(L'=');
            if (equals != std::wstring::npos) {
                std::wstring candidateId = entry.substr(0, equals);
                std::wstring candidatePath = entry.substr(equals + 1);
                TrimOrnamentText(candidateId);
                TrimOrnamentText(candidatePath);
                if (!candidatePath.empty() && !LooksLikeOrnamentPath(candidateId)) {
                    entry = std::move(candidatePath);
                }
            }
            TrimOrnamentText(entry);
            path = std::move(entry);
        }
    }
    if (path.empty()) return;
    OrnamentImageSpec spec;
    spec.id = L"pendant";
    spec.path = std::move(path);
    context.ornamentImageSpecs.push_back(std::move(spec));
}

// The selected preset owns the anchor, physics, reaction impulses, and node
// layout. Nodes always exist, so an empty image still renders the circular
// fallback of the draw path.
static void ApplyOrnamentPreset(OrnamentPreset preset) {
    const OrnamentPresetDefinition& definition =
        kOrnamentPresets[static_cast<size_t>(preset)];
    context.ornamentAnchorOffsetX = definition.anchorOffsetX;
    context.ornamentAnchorOffsetY = definition.anchorOffsetY;
    context.ornamentGravity = definition.gravity;
    context.ornamentDamping = definition.damping;
    context.ornamentConstraintIterations = definition.constraintIterations;
    context.ornamentStretch = definition.stretch;
    context.ornamentOpacity = definition.opacity;
    context.ornamentClickImpulse = definition.clickImpulse;
    context.ornamentWheelImpulse = definition.wheelImpulse;
    context.ornamentRopeSegments = definition.ropeSegments;
    context.ornamentRopeLengthPx = definition.ropeLengthPx * context.ornamentRopeLengthScale;
    context.ornamentRopeStyle = definition.ropeStyle;
    context.ornamentRopeWidthPx = definition.ropeWidthPx;
    context.ornamentRopeOpacity = definition.ropeOpacity;

    context.ornamentNodeSpecs.clear();
    const int nodeCount = std::clamp(definition.nodeCount, 0, kOrnamentMaxPresetNodes);
    context.ornamentNodeSpecs.reserve(static_cast<size_t>(nodeCount));
    for (int i = 0; i < nodeCount; ++i) {
        const OrnamentPresetNode& presetNode = definition.nodes[i];
        OrnamentNodeSpec spec;
        if (!context.ornamentImageSpecs.empty()) {
            spec.imageId = context.ornamentImageSpecs.front().id;
        }
        spec.ropeT = presetNode.ropeT;
        spec.sizePx = presetNode.sizePx;
        spec.opacity = presetNode.opacity;
        spec.angleDegrees = presetNode.angleDegrees;
        context.ornamentNodeSpecs.push_back(std::move(spec));
    }
}
