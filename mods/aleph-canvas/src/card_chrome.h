#pragma once

// Card header chrome.
//
// Every live card reserves a 28 DIP strip above its body. The strip carries the
// title and two buttons: pin keeps a card above every unpinned card, close
// dismisses the card until the canvas is shown again.
//
// This header stays free of Direct2D and Win32 drawing calls so the native
// regressions can exercise the geometry on its own.

#include "platform.h"
#include "constants.h"
#include "math_utils.h"
#include "types.h"

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

inline bool CardHeaderButtonsFit(const RectF& header, float scale) {
    if (header.empty()) {
        return false;
    }
    const float size = CardHeaderButtonSize(header.height(), scale);
    if (size <= 0.0f) {
        return false;
    }
    const float inset = CardHeaderButtonInset(header.height(), scale);
    const float gap = kCardHeaderButtonGapPx * scale;
    const float needed =
        size * 2.0f + gap + inset * 2.0f + kCardTitlePaddingPx * scale * 2.0f;
    return header.width() >= needed;
}

inline RectF CardCloseButtonRect(const RectF& header, float scale) {
    if (!CardHeaderButtonsFit(header, scale)) {
        return {};
    }
    const float size = CardHeaderButtonSize(header.height(), scale);
    const float inset = CardHeaderButtonInset(header.height(), scale);
    const float top = header.centerY() - size * 0.5f;
    return MakeRectF(header.right - inset - size, top, size, size);
}

inline RectF CardPinButtonRect(const RectF& header, float scale) {
    const RectF close = CardCloseButtonRect(header, scale);
    if (close.empty()) {
        return {};
    }
    const float gap = kCardHeaderButtonGapPx * scale;
    return MakeRectF(close.left - gap - close.width(), close.top, close.width(),
                     close.height());
}

inline CardHeaderAction HitTestCardHeader(const RectF& header, Vec2 point,
                                          float scale) {
    if (ContainsPoint(CardCloseButtonRect(header, scale), point.x, point.y)) {
        return CardHeaderAction::Close;
    }
    if (ContainsPoint(CardPinButtonRect(header, scale), point.x, point.y)) {
        return CardHeaderAction::Pin;
    }
    return CardHeaderAction::None;
}

// Pinned cards are drawn last and hit-tested first, so pinning a card lifts it
// above every unpinned card without reordering the scan-owned card vector.
inline std::vector<size_t> CardDrawOrder(const std::vector<Card>& cards) {
    std::vector<size_t> order;
    order.reserve(cards.size());
    for (size_t index = 0; index < cards.size(); ++index) {
        if (!cards[index].pinned) {
            order.push_back(index);
        }
    }
    for (size_t index = 0; index < cards.size(); ++index) {
        if (cards[index].pinned) {
            order.push_back(index);
        }
    }
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
