#pragma once

// Card header chrome.
//
// Every live card reserves a fixed DIP strip above its body. The strip carries
// the title and its buttons: group joins or splits the windows of one
// executable, pin keeps a card above every unpinned card, close closes the
// card's real window.
//
// This header stays free of Direct2D and Win32 drawing calls so the native
// regressions can exercise the geometry on its own.

#include "platform.h"
#include "constants.h"
#include "math_utils.h"
#include "text_utils.h"
#include "types.h"

#include <algorithm>
#include <unordered_map>

inline RectF CardHeaderRect(const Card& card) {
    const RectF bounds = CardScreenBounds(card);
    return RectF{bounds.left, bounds.top, bounds.right,
                 bounds.top + CardHeaderHeight()};
}

// The inset shrinks with the header so a zoomed-out card still keeps its
// buttons inside the strip instead of clipping them.
inline float CardHeaderButtonInset(float headerHeight, float scale) {
    return std::min(kCardHeaderButtonInsetPx * scale, headerHeight * 0.2f);
}

inline float CardHeaderButtonSize(float headerHeight, float scale) {
    if (headerHeight <= 0.0f) {
        return 0.0f;
    }
    const float inset = CardHeaderButtonInset(headerHeight, scale);
    return std::min(kCardHeaderButtonPx * scale, headerHeight - inset * 2.0f);
}

// A window group only exists once another window of the same executable can
// join it, so the group button appears only for those cards and only while
// stacking is enabled. Tying the button count to the cards keeps every card's
// row width predictable for both drawing and hit testing.
inline int CardHeaderButtonCount(const std::vector<Card>& cards, size_t index,
                                 bool stacksEnabled) {
    if (!stacksEnabled || index >= cards.size()) {
        return 2;
    }
    const std::wstring name = ExecutableName(cards[index].exePath);
    if (name.empty()) {
        return 2;
    }
    size_t sameName = 0;
    for (const Card& card : cards) {
        if (ExecutableName(card.exePath) == name) {
            ++sameName;
        }
    }
    return sameName >= 2 ? 3 : 2;
}

inline float CardHeaderButtonRowWidth(const RectF& header, float scale,
                                      int buttons) {
    if (buttons <= 0 || header.empty()) {
        return 0.0f;
    }
    const float size = CardHeaderButtonSize(header.height(), scale);
    const float inset = CardHeaderButtonInset(header.height(), scale);
    const float gap = kCardHeaderButtonGapPx * scale;
    const float count = static_cast<float>(buttons);
    return size * count + gap * (count - 1.0f) + inset * 2.0f;
}

inline bool CardHeaderButtonsFit(const RectF& header, float scale,
                                 int buttons) {
    if (header.empty()) {
        return false;
    }
    const float size = CardHeaderButtonSize(header.height(), scale);
    if (size <= 0.0f) {
        return false;
    }
    const float needed = CardHeaderButtonRowWidth(header, scale, buttons) +
                         kCardTitlePaddingPx * scale * 2.0f;
    return header.width() >= needed;
}

inline RectF CardCloseButtonRect(const RectF& header, float scale, int buttons) {
    if (!CardHeaderButtonsFit(header, scale, buttons)) {
        return {};
    }
    const float size = CardHeaderButtonSize(header.height(), scale);
    const float inset = CardHeaderButtonInset(header.height(), scale);
    const float top = header.centerY() - size * 0.5f;
    return MakeRectF(header.right - inset - size, top, size, size);
}

inline RectF CardPinButtonRect(const RectF& header, float scale, int buttons) {
    const RectF close = CardCloseButtonRect(header, scale, buttons);
    if (close.empty()) {
        return {};
    }
    const float gap = kCardHeaderButtonGapPx * scale;
    return MakeRectF(close.left - gap - close.width(), close.top, close.width(),
                     close.height());
}

// The group button sits left of the pin; an empty rect means the card has no
// window group and therefore no group button.
inline RectF CardGroupButtonRect(const RectF& header, float scale, int buttons) {
    if (buttons < 3) {
        return {};
    }
    const RectF pin = CardPinButtonRect(header, scale, buttons);
    if (pin.empty()) {
        return {};
    }
    const float gap = kCardHeaderButtonGapPx * scale;
    return MakeRectF(pin.left - gap - pin.width(), pin.top, pin.width(),
                     pin.height());
}

inline CardHeaderAction HitTestCardHeader(const RectF& header, Vec2 point,
                                          float scale, int buttons) {
    if (ContainsPoint(CardCloseButtonRect(header, scale, buttons), point.x,
                      point.y)) {
        return CardHeaderAction::Close;
    }
    if (ContainsPoint(CardPinButtonRect(header, scale, buttons), point.x,
                      point.y)) {
        return CardHeaderAction::Pin;
    }
    if (ContainsPoint(CardGroupButtonRect(header, scale, buttons), point.x,
                      point.y)) {
        return CardHeaderAction::Group;
    }
    return CardHeaderAction::None;
}

inline bool IsGroupedStackCard(const Card& card) {
    return card.stack.grouped && !card.stack.groupKey.empty();
}

// Stack members share one draw group so they stay contiguous in paint order;
// an ungrouped card is its own group and keeps its scan position.
inline std::wstring CardDrawGroupKey(const Card& card, size_t index) {
    if (IsGroupedStackCard(card)) {
        return L"S:" + card.stack.groupKey;
    }
    return L"C:" + card.fingerprint + L":" + std::to_wstring(index);
}

inline std::unordered_map<std::wstring, bool> CollectStackPins(
    const std::vector<Card>& cards) {
    std::unordered_map<std::wstring, bool> stackPinned;
    for (const Card& card : cards) {
        if (!IsGroupedStackCard(card)) {
            continue;
        }
        bool& pinned = stackPinned[card.stack.groupKey];
        pinned = pinned || card.pinned;
    }
    return stackPinned;
}

inline std::vector<size_t> AssignDrawGroups(const std::vector<Card>& cards) {
    std::unordered_map<std::wstring, size_t> groupIds;
    std::vector<size_t> drawGroups(cards.size());
    for (size_t index = 0; index < cards.size(); ++index) {
        const auto entry = groupIds.try_emplace(
            CardDrawGroupKey(cards[index], index), groupIds.size()).first;
        drawGroups[index] = entry->second;
    }
    return drawGroups;
}

inline bool CardDrawPinned(
    const Card& card, const std::unordered_map<std::wstring, bool>& stackPinned) {
    if (!IsGroupedStackCard(card)) {
        return card.pinned;
    }
    const auto entry = stackPinned.find(card.stack.groupKey);
    return entry == stackPinned.end() ? card.pinned : entry->second;
}

inline bool IsStackFrontCard(const Card& card) {
    return IsGroupedStackCard(card) &&
           (card.stack.slot == 0 || card.stack.drawAboveStack);
}

// The card lifted out of the stack paints above its own members, then the
// highest slot (deepest background card) paints first and the front slot
// (slot 0) paints last, so a stack always reads as one card with its front on
// top.
inline bool StackDrawLess(const Card& left, const Card& right) {
    if (left.stack.drawAboveStack != right.stack.drawAboveStack) {
        return !left.stack.drawAboveStack;
    }
    return left.stack.slot > right.stack.slot;
}

// Stack fronts are the highest layer, followed by pinned cards, so a pin lifts
// other cards without covering the upright card at the front of a stack.
// Hit testing walks this order backwards, reaching the visible top card first.
inline std::vector<size_t> CardDrawOrder(const std::vector<Card>& cards) {
    const std::unordered_map<std::wstring, bool> stackPinned =
        CollectStackPins(cards);
    const std::vector<size_t> drawGroups = AssignDrawGroups(cards);
    std::vector<size_t> order;
    order.reserve(cards.size());
    for (size_t index = 0; index < cards.size(); ++index) {
        order.push_back(index);
    }
    std::stable_sort(
        order.begin(), order.end(),
        [&cards, &drawGroups, &stackPinned](size_t left, size_t right) {
            const bool leftFront = IsStackFrontCard(cards[left]);
            const bool rightFront = IsStackFrontCard(cards[right]);
            if (leftFront != rightFront) {
                return !leftFront;
            }
            const bool leftPinned = CardDrawPinned(cards[left], stackPinned);
            const bool rightPinned = CardDrawPinned(cards[right], stackPinned);
            if (leftPinned != rightPinned) {
                return !leftPinned;
            }
            if (drawGroups[left] != drawGroups[right]) {
                return drawGroups[left] < drawGroups[right];
            }
            if (!IsGroupedStackCard(cards[left]) ||
                !IsGroupedStackCard(cards[right])) {
                return false;
            }
            return StackDrawLess(cards[left], cards[right]);
        });
    return order;
}

inline int HitTestCardInOrder(const std::vector<Card>& cards, Vec2 point) {
    const std::vector<size_t> order = CardDrawOrder(cards);
    for (size_t slot = order.size(); slot-- > 0;) {
        const size_t index = order[slot];
        if (ContainsPoint(CardScreenBounds(cards[index]), point.x, point.y)) {
            return static_cast<int>(index);
        }
    }
    return -1;
}
