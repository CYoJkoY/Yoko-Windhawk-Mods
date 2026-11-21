#pragma once

#include "constants.h"
#include "text_utils.h"
#include "types.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

struct StackMember {
    size_t index;
    std::wstring groupKey;
};

inline Vec2 RotatePoint(Vec2 point, Vec2 center, float radians) {
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    const float x = point.x - center.x;
    const float y = point.y - center.y;
    return Vec2{center.x + x * cosine - y * sine,
                center.y + x * sine + y * cosine};
}

inline Vec2 CardLocalPoint(Vec2 screenPoint, const RectF& bounds,
                           const CardStackPlacement& placement) {
    const Vec2 center{bounds.centerX(), bounds.centerY()};
    const Vec2 unrotated = RotatePoint(screenPoint, center,
                                       -placement.rotationRadians);
    const float scale = placement.scale > 0.0f ? placement.scale : 1.0f;
    return Vec2{center.x + (unrotated.x - center.x) / scale,
                center.y + (unrotated.y - center.y) / scale};
}

inline std::array<Vec2, 4> CardScreenQuad(const RectF& bounds,
                                         const CardStackPlacement& placement) {
    const Vec2 center{bounds.centerX(), bounds.centerY()};
    const float scale = placement.scale > 0.0f ? placement.scale : 1.0f;
    const std::array<Vec2, 4> corners = {
        Vec2{bounds.left, bounds.top}, Vec2{bounds.right, bounds.top},
        Vec2{bounds.right, bounds.bottom}, Vec2{bounds.left, bounds.bottom}};
    std::array<Vec2, 4> quad{};
    for (size_t index = 0; index < corners.size(); ++index) {
        const Vec2 scaled{center.x + (corners[index].x - center.x) * scale,
                          center.y + (corners[index].y - center.y) * scale};
        quad[index] = RotatePoint(scaled, center, placement.rotationRadians);
    }
    return quad;
}

inline RectF CardQuadBounds(const std::array<Vec2, 4>& quad) {
    RectF bounds{quad[0].x, quad[0].y, quad[0].x, quad[0].y};
    for (const Vec2& point : quad) {
        bounds.left = std::min(bounds.left, point.x);
        bounds.top = std::min(bounds.top, point.y);
        bounds.right = std::max(bounds.right, point.x);
        bounds.bottom = std::max(bounds.bottom, point.y);
    }
    return bounds;
}

inline void SubtractRect(const RectF& source, const RectF& hole,
                         std::vector<RectF>& result) {
    if (!RectsOverlap(source, hole)) {
        result.push_back(source);
        return;
    }
    const RectF overlap{std::max(source.left, hole.left),
                        std::max(source.top, hole.top),
                        std::min(source.right, hole.right),
                        std::min(source.bottom, hole.bottom)};
    const RectF pieces[] = {
        {source.left, source.top, source.right, overlap.top},
        {source.left, overlap.bottom, source.right, source.bottom},
        {source.left, overlap.top, overlap.left, overlap.bottom},
        {overlap.right, overlap.top, source.right, overlap.bottom}};
    for (const RectF& piece : pieces) {
        if (!piece.empty()) {
            result.push_back(piece);
        }
    }
}

struct CardDrawClips {
    bool clipped = false;
    std::vector<RectF> regions;
};

inline std::vector<RectF> CardOcclusionHoles(
    const RectF& viewport, const RectF& cardBounds,
    const std::vector<RectF>& holes) {
    std::vector<RectF> relevantHoles;
    for (const RectF& hole : holes) {
        if (RectsOverlap(cardBounds, hole) && RectsOverlap(viewport, hole)) {
            const RectF clippedHole{std::max(viewport.left, hole.left),
                                    std::max(viewport.top, hole.top),
                                    std::min(viewport.right, hole.right),
                                    std::min(viewport.bottom, hole.bottom)};
            relevantHoles.push_back(clippedHole);
        }
    }
    return relevantHoles;
}

inline std::vector<RectF> SubtractCardOcclusionHoles(
    const RectF& viewport, const std::vector<RectF>& holes) {
    std::vector<RectF> regions{viewport};
    for (const RectF& hole : holes) {
        std::vector<RectF> remaining;
        for (const RectF& region : regions) {
            SubtractRect(region, hole, remaining);
        }
        regions = std::move(remaining);
        if (regions.empty()) {
            break;
        }
    }
    return regions;
}

inline RectF UnoccludedThumbnailRect(const RectF& destination,
                                     const std::vector<RectF>& higherThumbnails) {
    const auto occlusion = CardOcclusionHoles(destination, destination, higherThumbnails);
    if (occlusion.empty()) {
        return destination;
    }
    const auto visible = SubtractCardOcclusionHoles(destination, occlusion);
    RectF bounds{};
    double area = 0.0;
    for (const RectF& region : visible) {
        bounds = UnionRect(bounds, region);
        area += static_cast<double>(region.width()) * static_cast<double>(region.height());
    }
    return area == static_cast<double>(bounds.width()) * static_cast<double>(bounds.height())
               ? bounds : RectF{};
}

inline CardDrawClips CardDrawRegions(
    const RectF& viewport, const std::array<Vec2, 4>& cardQuad,
    const std::vector<RectF>& holes) {
    if (viewport.empty()) {
        return CardDrawClips{true, {}};
    }
    const std::vector<RectF> relevantHoles =
        CardOcclusionHoles(viewport, CardQuadBounds(cardQuad), holes);
    if (relevantHoles.empty()) {
        return CardDrawClips{};
    }
    return CardDrawClips{true, SubtractCardOcclusionHoles(viewport, relevantHoles)};
}

inline CardStackPlacement StackPlacementForSlot(size_t slot,
                                               const std::wstring& groupKey,
                                               const CanvasSettings& settings) {
    const auto fanDepth = static_cast<size_t>(
        std::clamp(settings.stackMaxDepth, 0, kMaxStackDepth));
    const float depth = static_cast<float>(slot);
    const float degrees = std::min(depth * settings.stackStepDegrees,
                                   settings.stackMaxDegrees);
    const float radians = degrees * (std::numbers::pi_v<float> / 180.0f) *
                          static_cast<float>(settings.stackRotationDirection);
    return CardStackPlacement{groupKey, slot, true, slot <= fanDepth, radians,
                              std::pow(settings.stackScaleFactor, depth)};
}

inline bool IsStackFront(const Card& card, const std::wstring& groupKey) {
    return card.stack.grouped && card.stack.slot == 0 &&
           card.stack.groupKey == groupKey;
}

inline void ResetStackPlacement(Card& card) {
    card.stack.groupKey.clear();
    card.stack.slot = 0;
    card.stack.rotationRadians = 0.0f;
    card.stack.scale = 1.0f;
    card.stack.opacity = 1.0f;
    card.stack.drawAboveStack = false;
    card.stack.visible = true;
    card.stack.frameWorld = RectF{};
}

inline void ClearAllStackPlacements(std::vector<Card>& cards) {
    for (Card& card : cards) {
        ResetStackPlacement(card);
    }
}

inline void ClearIneligibleStackPlacements(std::vector<Card>& cards) {
    for (Card& card : cards) {
        if (!card.stack.grouped || card.applicationKey.empty()) {
            ResetStackPlacement(card);
        }
    }
}

inline std::vector<StackMember> CollectStackMembers(const std::vector<Card>& cards) {
    std::vector<StackMember> members;
    members.reserve(cards.size());
    for (size_t index = 0; index < cards.size(); ++index) {
        const Card& card = cards[index];
        if (card.stack.grouped && !card.applicationKey.empty()) {
            members.push_back({index, card.applicationKey});
        }
    }
    return members;
}

inline size_t StackFrontPosition(const std::vector<Card>& cards,
                                 const std::vector<StackMember>& members,
                                 const std::wstring& groupKey) {
    const auto previous = std::find_if(
        members.begin(), members.end(), [&cards, &groupKey](const StackMember& member) {
            return IsStackFront(cards[member.index], groupKey);
        });
    if (previous == members.end()) {
        return 0;
    }
    return static_cast<size_t>(previous - members.begin());
}

// A card that leaves a stack keeps its own window size and only takes the
// place the stack stood, so releasing a member never leaves it wearing the
// frame's proportions.
inline void RestoreOwnSizeCentredInFrame(Card& card) {
    if (!card.stack.grouped || card.stack.groupKey.empty() ||
        card.stack.frameWorld.empty() || card.world.empty()) {
        return;
    }
    const RectF& frame = card.stack.frameWorld;
    card.world = MakeRectF(frame.centerX() - card.world.width() * 0.5f,
                           frame.centerY() - card.world.height() * 0.5f,
                           card.world.width(), card.world.height());
}

// The shared rectangle the whole stack occupies. A formed stack keeps the
// frame it was given, so cycling the fan never resizes the card and every
// member shares one size and one centre; a new stack takes the front window's
// world rectangle. Keeping the frame instead of re-deriving it from the front
// window is what makes the fan read as one object: the front window can be
// resized, maximized or replaced without the card jumping in size.
inline RectF StackFrameForOrder(const std::vector<Card>& cards,
                                const std::vector<size_t>& order) {
    for (const size_t index : order) {
        if (index < cards.size() && !cards[index].stack.frameWorld.empty()) {
            return cards[index].stack.frameWorld;
        }
    }
    if (order.empty() || order.front() >= cards.size()) {
        return RectF{};
    }
    return cards[order.front()].world;
}

inline void ApplyStackOrder(std::vector<Card>& cards,
                            const std::vector<size_t>& order,
                            const CanvasSettings& settings) {
    const RectF frame = StackFrameForOrder(cards, order);
    for (size_t slot = 0; slot < order.size(); ++slot) {
        const size_t index = order[slot];
        if (index >= cards.size()) {
            continue;
        }
        const std::wstring groupKey = cards[index].stack.groupKey;
        cards[index].stack = StackPlacementForSlot(slot, groupKey, settings);
        cards[index].stack.frameWorld = frame;
    }
}

inline void AssignStack(std::vector<Card>& cards,
                        const std::vector<StackMember>& members,
                        const CanvasSettings& settings) {
    if (members.size() == 1) {
        Card& card = cards[members.front().index];
        RestoreOwnSizeCentredInFrame(card);
        ResetStackPlacement(card);
        return;
    }
    const std::wstring groupKey = members.front().groupKey;
    const size_t start = StackFrontPosition(cards, members, groupKey);
    std::vector<size_t> order;
    order.reserve(members.size());
    for (size_t slot = 0; slot < members.size(); ++slot) {
        const StackMember& member = members[(start + slot) % members.size()];
        cards[member.index].stack.groupKey = member.groupKey;
        cards[member.index].stack.grouped = true;
        order.push_back(member.index);
    }
    ApplyStackOrder(cards, order, settings);
}

inline void AssignStackGroups(std::vector<Card>& cards,
                              const std::vector<StackMember>& members,
                              const CanvasSettings& settings) {
    std::vector<StackMember> sorted = members;
    std::stable_sort(sorted.begin(), sorted.end(), [&cards](const StackMember& left,
                                                            const StackMember& right) {
        if (left.groupKey != right.groupKey) {
            return left.groupKey < right.groupKey;
        }
        // Within one app, members fan in desktop Z-order: the topmost /
        // focused window is first, so a newly formed stack hands slot 0
        // (0 degrees, scale 1, drawn on top) to that window. Fingerprint
        // only breaks ties, e.g. cards built without a scan rank.
        if (cards[left.index].zOrder != cards[right.index].zOrder) {
            return cards[left.index].zOrder < cards[right.index].zOrder;
        }
        return cards[left.index].fingerprint < cards[right.index].fingerprint;
    });
    for (auto group = sorted.begin(); group != sorted.end();) {
        const auto end = std::find_if(group + 1, sorted.end(), [&group](const StackMember& member) {
            return member.groupKey != group->groupKey;
        });
        AssignStack(cards, std::vector<StackMember>(group, end), settings);
        group = end;
    }
}

inline void ReconcileCardStacks(std::vector<Card>& cards, const CanvasSettings& settings) {
    if (!settings.stacksEnabled) {
        ClearAllStackPlacements(cards);
        return;
    }
    std::unordered_map<std::wstring, size_t> applicationCounts;
    for (const Card& card : cards) {
        if (!card.applicationKey.empty()) {
            ++applicationCounts[card.applicationKey];
        }
    }
    for (Card& card : cards) {
        const auto count = applicationCounts.find(card.applicationKey);
        card.hasApplicationPeers = count != applicationCounts.end() && count->second > 1;
    }
    ClearIneligibleStackPlacements(cards);
    const auto members = CollectStackMembers(cards);
    AssignStackGroups(cards, members, settings);
}

inline void CollectStackSlots(const std::vector<Card>& cards,
                              const std::wstring& groupKey,
                              std::vector<std::pair<size_t, size_t>>& slots) {
    slots.reserve(cards.size());
    for (size_t index = 0; index < cards.size(); ++index) {
        const CardStackPlacement& placement = cards[index].stack;
        if (placement.grouped && placement.groupKey == groupKey) {
            slots.emplace_back(placement.slot, index);
        }
    }
}

inline bool HasDuplicateSlot(const std::vector<std::pair<size_t, size_t>>& slots) {
    for (size_t index = 1; index < slots.size(); ++index) {
        if (slots[index].first == slots[index - 1].first) {
            return true;
        }
    }
    return false;
}

inline bool StackMemberOrder(const std::vector<Card>& cards,
                             const std::wstring& groupKey,
                             std::vector<size_t>& members) {
    members.clear();
    std::vector<std::pair<size_t, size_t>> slots;
    CollectStackSlots(cards, groupKey, slots);
    std::stable_sort(slots.begin(), slots.end(), [](const auto& left, const auto& right) {
        return left.first < right.first;
    });
    if (HasDuplicateSlot(slots)) {
        return false;
    }
    members.reserve(slots.size());
    for (const auto& slot : slots) {
        members.push_back(slot.second);
    }
    return !members.empty();
}

inline std::vector<size_t> RotateStackOrder(const std::vector<size_t>& members, int shift) {
    std::vector<size_t> order;
    order.reserve(members.size());
    const int count = static_cast<int>(members.size());
    for (int slot = 0; slot < count; ++slot) {
        order.push_back(members[static_cast<size_t>((shift + slot) % count)]);
    }
    return order;
}

inline bool CycleCardStack(std::vector<Card>& cards, const CanvasSettings& settings,
                           int hitIndex, int steps) {
    if (!settings.stacksEnabled || hitIndex < 0 || steps == 0 ||
        static_cast<size_t>(hitIndex) >= cards.size()) {
        return false;
    }
    const CardStackPlacement& hitPlacement = cards[static_cast<size_t>(hitIndex)].stack;
    if (!hitPlacement.grouped || hitPlacement.groupKey.empty()) {
        return false;
    }
    std::vector<size_t> members;
    if (!StackMemberOrder(cards, hitPlacement.groupKey, members) || members.size() <= 1) {
        return false;
    }
    if (hitPlacement.slot >= members.size() ||
        members[hitPlacement.slot] != static_cast<size_t>(hitIndex)) {
        return false;
    }
    const int count = static_cast<int>(members.size());
    const int shift = (steps % count + count) % count;
    if (shift == 0) {
        return false;
    }
    ApplyStackOrder(cards, RotateStackOrder(members, shift), settings);
    return true;
}

// Alt+wheel cycles a stack instead of zooming. Wheel delta is in
// WHEEL_DELTA units (120 per detent, positive = wheel up): wheel down
// brings the next card to the front, wheel up brings the previous one
// back. A sub-detent delta maps to zero steps and cycles nothing.
inline int StackWheelSteps(int wheelDelta) {
    return -wheelDelta / WHEEL_DELTA;
}

inline bool CycleStackForWheel(std::vector<Card>& cards,
                               const CanvasSettings& settings, int hitIndex,
                               int wheelDelta) {
    if (wheelDelta == 0) {
        return false;
    }
    return CycleCardStack(cards, settings, hitIndex, StackWheelSteps(wheelDelta));
}

// Alt+wheel cycle phases, as fractions of the transition duration. The
// outgoing front card lifts above the stack and fades out first, then the
// incoming front fades in while every member eases into its new slot, so the
// stack reads as one set of cards rotating under the new front.
constexpr float kStackCycleLiftFraction = 0.35f;
constexpr float kStackCycleIncomingFraction = 0.65f;
constexpr float kStackCycleFadeSpan =
    kStackCycleIncomingFraction - kStackCycleLiftFraction;
constexpr float kStackCycleSettleSpan = 1.0f - kStackCycleIncomingFraction;

struct StackCycleRequest {
    const CanvasSettings& settings;
    int hitIndex;
    int steps;
    DWORD now;
};

inline Card* FindStackCard(std::vector<Card>& cards, const std::wstring& groupKey,
                          const std::wstring& fingerprint) {
    const auto card = std::find_if(
        cards.begin(), cards.end(), [&fingerprint, &groupKey](const Card& candidate) {
            return candidate.fingerprint == fingerprint &&
                   candidate.stack.groupKey == groupKey;
        });
    return card == cards.end() ? nullptr : &*card;
}

inline float StackCycleProgress(const StackCycleTransition& transition, DWORD now) {
    if (transition.durationMs == 0) {
        return 1.0f;
    }
    const float elapsed = static_cast<float>(now - transition.startTick);
    return std::clamp(elapsed / static_cast<float>(transition.durationMs), 0.0f, 1.0f);
}

inline float StackCycleOpacity(const StackCardTransition& pose, float progress) {
    if (pose.outgoing) {
        if (progress < kStackCycleLiftFraction) {
            return pose.fromOpacity * (1.0f - progress / kStackCycleLiftFraction);
        }
        if (progress < kStackCycleIncomingFraction) {
            return 0.0f;
        }
        return std::clamp((progress - kStackCycleIncomingFraction) /
                              kStackCycleSettleSpan,
                          0.0f, 1.0f);
    }
    if (pose.incoming) {
        if (progress < kStackCycleLiftFraction) {
            return 0.0f;
        }
        if (progress < kStackCycleIncomingFraction) {
            return std::clamp((progress - kStackCycleLiftFraction) /
                                  kStackCycleFadeSpan,
                              0.0f, 1.0f);
        }
    }
    return pose.fromOpacity +
        (1.0f - pose.fromOpacity) * progress;
}

inline void ApplyStackCyclePose(Card& card, const StackCardTransition& pose,
                                float eased, float progress) {
    card.stack.rotationRadians =
        pose.fromRotation + (pose.toRotation - pose.fromRotation) * eased;
    card.stack.scale = pose.fromScale + (pose.toScale - pose.fromScale) * eased;
    card.stack.drawAboveStack = pose.outgoing && progress < kStackCycleLiftFraction;
    card.stack.opacity = StackCycleOpacity(pose, progress);
}

inline void SettleStackCyclePose(Card& card, const StackCardTransition& pose) {
    card.stack.rotationRadians = pose.toRotation;
    card.stack.scale = pose.toScale;
    card.stack.opacity = 1.0f;
    card.stack.drawAboveStack = false;
}

inline void ApplyStackCycleFrame(std::vector<Card>& cards,
                                const StackCycleTransition& transition,
                                float progress) {
    const float eased = progress * progress * (3.0f - 2.0f * progress);
    for (const StackCardTransition& pose : transition.cards) {
        Card* card = FindStackCard(cards, transition.groupKey, pose.fingerprint);
        if (card == nullptr) {
            continue;
        }
        if (progress >= 1.0f) {
            SettleStackCyclePose(*card, pose);
            continue;
        }
        ApplyStackCyclePose(*card, pose, eased, progress);
    }
}

inline bool AdvanceStackCycleTransitions(
    std::vector<Card>& cards, std::vector<StackCycleTransition>& transitions,
    DWORD now) {
    bool active = false;
    for (size_t transitionIndex = transitions.size(); transitionIndex-- > 0;) {
        const StackCycleTransition& transition = transitions[transitionIndex];
        const float progress = StackCycleProgress(transition, now);
        ApplyStackCycleFrame(cards, transition, progress);
        if (progress >= 1.0f) {
            transitions.erase(transitions.begin() +
                              static_cast<std::ptrdiff_t>(transitionIndex));
            continue;
        }
        active = true;
    }
    return active;
}

inline bool CycleStackWithAnimation(std::vector<Card>& cards,
                                    const StackCycleRequest& request,
                                    std::vector<StackCycleTransition>& transitions) {
    if (request.steps == 0 || request.hitIndex < 0 ||
        static_cast<size_t>(request.hitIndex) >= cards.size()) {
        return false;
    }
    const std::wstring groupKey =
        cards[static_cast<size_t>(request.hitIndex)].stack.groupKey;
    if (groupKey.empty()) {
        return false;
    }
    AdvanceStackCycleTransitions(cards, transitions, request.now);
    std::vector<size_t> members;
    if (!StackMemberOrder(cards, groupKey, members) || members.size() <= 1) {
        return false;
    }
    const size_t frontIndex = members.front();
    std::vector<StackCardTransition> poses;
    poses.reserve(members.size());
    for (const size_t index : members) {
        const Card& card = cards[index];
        poses.push_back(StackCardTransition{card.fingerprint,
            card.stack.rotationRadians, card.stack.scale, card.stack.opacity,
            0.0f, 1.0f, index == frontIndex, false});
    }
    if (!CycleCardStack(cards, request.settings, request.hitIndex, request.steps)) {
        return false;
    }
    for (StackCardTransition& pose : poses) {
        Card* card = FindStackCard(cards, groupKey, pose.fingerprint);
        if (card != nullptr) {
            pose.toRotation = card->stack.rotationRadians;
            pose.toScale = card->stack.scale;
            pose.incoming = card->stack.slot == 0 && !pose.outgoing;
            card->stack.opacity = pose.fromOpacity;
            card->stack.drawAboveStack = pose.outgoing;
        }
    }
    transitions.erase(
        std::remove_if(transitions.begin(), transitions.end(), [&groupKey](const auto& item) {
            return item.groupKey == groupKey;
        }), transitions.end());
    if (request.settings.animationMs <= 0) {
        for (Card& card : cards) {
            if (card.stack.groupKey == groupKey) {
                card.stack.opacity = 1.0f;
                card.stack.drawAboveStack = false;
            }
        }
        return true;
    }
    transitions.push_back(StackCycleTransition{groupKey, request.now,
        static_cast<DWORD>(request.settings.animationMs), std::move(poses)});
    AdvanceStackCycleTransitions(cards, transitions, request.now);
    return true;
}
