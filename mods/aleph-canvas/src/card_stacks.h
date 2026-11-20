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
    card.stack.visible = true;
    card.stack.anchorWorld = RectF{};
}

inline void ClearAllStackPlacements(std::vector<Card>& cards) {
    for (Card& card : cards) {
        ResetStackPlacement(card);
    }
}

inline void ClearIneligibleStackPlacements(std::vector<Card>& cards) {
    for (Card& card : cards) {
        if (!card.stack.grouped || card.exePath.empty()) {
            ResetStackPlacement(card);
        }
    }
}

inline std::vector<StackMember> CollectStackMembers(const std::vector<Card>& cards) {
    std::vector<StackMember> members;
    members.reserve(cards.size());
    for (size_t index = 0; index < cards.size(); ++index) {
        const Card& card = cards[index];
        if (card.stack.grouped && !card.exePath.empty()) {
            members.push_back({index, ExecutableName(card.exePath)});
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

inline void ApplyStackOrder(std::vector<Card>& cards,
                            const std::vector<size_t>& order,
                            const CanvasSettings& settings) {
    for (size_t slot = 0; slot < order.size(); ++slot) {
        const size_t index = order[slot];
        if (index >= cards.size()) {
            continue;
        }
        const std::wstring groupKey = cards[index].stack.groupKey;
        const RectF anchor = cards[index].stack.anchorWorld;
        cards[index].stack = StackPlacementForSlot(slot, groupKey, settings);
        cards[index].stack.anchorWorld = anchor;
    }
}

inline void AssignStack(std::vector<Card>& cards,
                        const std::vector<StackMember>& members,
                        const CanvasSettings& settings) {
    if (members.empty()) {
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
    // Keep the stack at one place: reuse the anchor captured when the
    // stack was formed, and only capture the front card's world rectangle
    // the first time. Recapturing on every rescan or cycle would move the
    // stack to the new front card's unrelated window position.
    RectF anchor{};
    for (const size_t index : order) {
        if (!cards[index].stack.anchorWorld.empty()) {
            anchor = cards[index].stack.anchorWorld;
            break;
        }
    }
    if (anchor.empty() && !order.empty()) {
        anchor = cards[order.front()].world;
    }
    for (const size_t index : order) {
        cards[index].stack.anchorWorld = anchor;
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
