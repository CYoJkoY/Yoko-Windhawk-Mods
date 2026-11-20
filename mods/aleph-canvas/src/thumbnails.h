#pragma once

#include "cards.h"
#include "card_chrome.h"
#include "constants.h"
#include "context.h"
#include "math_utils.h"
#include "types.h"

inline void ReleaseThumbnail(Card& card) {
    card.thumbnail.reset();
    card.thumbnailVisible = false;
    card.thumbnailDestination = {};
}

inline void FailThumbnail(Card& card, DWORD now) {
    ReleaseThumbnail(card);
    card.thumbnailFailed = true;
    card.thumbnailRetryTick = now;
}

inline bool HasDueThumbnailRetry(DWORD now) {
    for (const Card& card : context.cards) {
        if (card.state == WindowVisualState::Live && !card.destroyed &&
            !IsStackBackground(card) &&
            card.thumbnailFailed &&
            now - card.thumbnailRetryTick >= kThumbnailRetryDelayMs) {
            return true;
        }
    }
    return false;
}

inline bool EnsureThumbnail(Card& card, DWORD now) {
    if (card.destroyed || card.state != WindowVisualState::Live ||
        IsWindow(card.hwnd) == FALSE) {
        ReleaseThumbnail(card);
        return false;
    }
    if (card.thumbnail.valid()) {
        return true;
    }
    if (card.thumbnailFailed && now - card.thumbnailRetryTick < kThumbnailRetryDelayMs) {
        return false;
    }
    HTHUMBNAIL handle = nullptr;
    const HRESULT result =
        DwmRegisterThumbnail(context.hostWindow, card.hwnd, &handle);
    card.thumbnail.reset(handle);
    if (FAILED(result) || !card.thumbnail.valid()) {
        FailThumbnail(card, now);
        return false;
    }
    card.thumbnailFailed = false;
    return true;
}

struct ThumbnailGeometry {
    RectF source{};
    RectF destination{};
};

inline RECT ThumbnailRect(const RectF& rect) {
    return RECT{static_cast<LONG>(std::lround(rect.left)),
                static_cast<LONG>(std::lround(rect.top)),
                static_cast<LONG>(std::lround(rect.right)),
                static_cast<LONG>(std::lround(rect.bottom))};
}

inline RectF ThumbnailViewportClip(const RectF& destination) {
    return RectF{std::max(0.0f, destination.left),
                 std::max(0.0f, destination.top),
                 std::min(static_cast<float>(VirtualScreenWidth()),
                           destination.right),
                 std::min(static_cast<float>(VirtualScreenHeight()),
                           destination.bottom)};
}

inline bool ReadThumbnailSource(const Card& card, RectF& source) {
    RECT window = {};
    RECT frame = {};
    RECT cropped = {};
    SIZE size = {};
    if (GetWindowRect(card.hwnd, &window) == FALSE ||
        !ReadFrameBounds(card.hwnd, frame) ||
        IntersectRect(&cropped, &window, &frame) == FALSE ||
        FAILED(DwmQueryThumbnailSourceSize(card.thumbnail.get(), &size))) {
        return false;
    }
    const RectF outer = RectFromRect(window);
    if (outer.empty() || size.cx <= 0 || size.cy <= 0) {
        return false;
    }
    const float scaleX = static_cast<float>(size.cx) / outer.width();
    const float scaleY = static_cast<float>(size.cy) / outer.height();
    source = RectF{static_cast<float>(cropped.left - window.left) * scaleX,
                   static_cast<float>(cropped.top - window.top) * scaleY,
                   static_cast<float>(cropped.right - window.left) * scaleX,
                   static_cast<float>(cropped.bottom - window.top) * scaleY};
    return !source.empty();
}

inline bool ClipThumbnailGeometry(ThumbnailGeometry& geometry) {
    const RectF clipped = ThumbnailViewportClip(geometry.destination);
    if (clipped.empty() || geometry.destination.empty() || geometry.source.empty()) {
        return false;
    }
    const float scaleX = geometry.source.width() / geometry.destination.width();
    const float scaleY = geometry.source.height() / geometry.destination.height();
    geometry.source = RectF{
        geometry.source.left + (clipped.left - geometry.destination.left) * scaleX,
        geometry.source.top + (clipped.top - geometry.destination.top) * scaleY,
        geometry.source.right - (geometry.destination.right - clipped.right) * scaleX,
        geometry.source.bottom - (geometry.destination.bottom - clipped.bottom) * scaleY};
    geometry.destination = clipped;
    const RECT source = ThumbnailRect(geometry.source);
    const RECT destination = ThumbnailRect(geometry.destination);
    return source.right > source.left && source.bottom > source.top &&
           destination.right > destination.left && destination.bottom > destination.top;
}

inline void HideThumbnail(Card& card, DWORD now) {
    DWM_THUMBNAIL_PROPERTIES properties = {};
    properties.dwFlags = DWM_TNP_VISIBLE;
    properties.fVisible = FALSE;
    if (FAILED(DwmUpdateThumbnailProperties(card.thumbnail.get(), &properties))) {
        FailThumbnail(card, now);
        return;
    }
    card.thumbnailVisible = false;
    card.thumbnailDestination = {};
}

inline void SubmitThumbnail(Card& card, const ThumbnailGeometry& geometry,
                            DWORD now) {
    DWM_THUMBNAIL_PROPERTIES properties = {};
    properties.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_RECTSOURCE |
                         DWM_TNP_VISIBLE | DWM_TNP_OPACITY |
                         DWM_TNP_SOURCECLIENTAREAONLY;
    properties.rcDestination = ThumbnailRect(geometry.destination);
    properties.rcSource = ThumbnailRect(geometry.source);
    properties.fVisible = TRUE;
    properties.opacity = 255;
    properties.fSourceClientAreaOnly = FALSE;
    if (FAILED(DwmUpdateThumbnailProperties(card.thumbnail.get(), &properties))) {
        FailThumbnail(card, now);
        return;
    }
    card.thumbnailDestination = properties.rcDestination;
    card.thumbnailVisible = true;
}

inline bool CanPrepareThumbnail(Card& card, DWORD now) {
    const bool stackAnimating = std::any_of(
        context.stackTransitions.begin(), context.stackTransitions.end(),
        [&card](const StackCycleTransition& transition) {
            return transition.groupKey == card.stack.groupKey;
        });
    if (stackAnimating) {
        if (card.thumbnail.valid() && card.thumbnailVisible) {
            HideThumbnail(card, now);
        }
        return false;
    }
    // DWM thumbnails cannot be rotated or scaled, and a stack shows only
    // its front window live. Background members render as icon cards, so
    // never register or show a thumbnail for them.
    if (IsStackBackground(card)) {
        if (card.thumbnail.valid()) {
            HideThumbnail(card, now);
        }
        return false;
    }
    return EnsureThumbnail(card, now);
}

inline bool PrepareThumbnail(Card& card, DWORD now,
                             ThumbnailGeometry& geometry) {
    if (!CanPrepareThumbnail(card, now)) {
        return false;
    }
    geometry.destination = WorldToScreen(StackWorldRect(card), context.camera);
    if (ThumbnailViewportClip(geometry.destination).empty()) {
        HideThumbnail(card, now);
        return false;
    }
    if (!ReadThumbnailSource(card, geometry.source)) {
        FailThumbnail(card, now);
        return false;
    }
    if (!ClipThumbnailGeometry(geometry)) {
        HideThumbnail(card, now);
        return false;
    }
    return true;
}

inline void UpdateThumbnails() {
    const DWORD now = GetTickCount();
    std::vector<ThumbnailGeometry> geometries(context.cards.size());
    std::vector<RectF> destinations(context.cards.size());
    for (size_t index = 0; index < context.cards.size(); ++index) {
        if (PrepareThumbnail(context.cards[index], now, geometries[index])) {
            destinations[index] =
                RectFromRect(ThumbnailRect(geometries[index].destination));
        }
    }
    const std::vector<bool> selected = SelectUnoccludedThumbnails(
        CardDrawOrder(context.cards), destinations);
    for (size_t index = 0; index < context.cards.size(); ++index) {
        Card& card = context.cards[index];
        if (selected[index]) {
            SubmitThumbnail(card, geometries[index], now);
        } else if (card.thumbnail.valid() && card.thumbnailVisible) {
            HideThumbnail(card, now);
        }
    }
}

inline void ReleaseAllThumbnails() {
    for (Card& card : context.cards) {
        ReleaseThumbnail(card);
    }
}
