#pragma once

#include "camera_ops.h"
#include "canvas_visibility.h"
#include "card_chrome.h"
#include "cards.h"
#include "context.h"
#include "desktop_pull.h"
#include "math_utils.h"
#include "minimap.h"

inline POINT CursorInCanvas() {
    POINT point = {};
    GetCursorPos(&point);
    point.x -= context.virtualScreen.left;
    point.y -= context.virtualScreen.top;
    return point;
}

inline int HitTestCard(Vec2 point) {
    return HitTestCardInOrder(context.cards, point);
}

inline CardHeaderAction HeaderActionAt(int index, Vec2 point) {
    if (index < 0 || static_cast<size_t>(index) >= context.cards.size()) {
        return CardHeaderAction::None;
    }
    const size_t cardIndex = static_cast<size_t>(index);
    const RectF header = CardHeaderRect(context.cards[cardIndex]);
    const float scale = ChromeScale();
    const int buttons = CardHeaderButtonCountForWidth(
        header, scale,
        CardHeaderButtonCount(context.cards, cardIndex,
                              context.settings.stacksEnabled));
    return HitTestCardHeader(header, point, scale, buttons);
}

inline void DismissCard(int index) {
    if (index < 0 || static_cast<size_t>(index) >= context.cards.size()) {
        return;
    }
    context.dismissedFingerprints.insert(
        context.cards[static_cast<size_t>(index)].fingerprint);
    RemoveCardAt(static_cast<size_t>(index));
    context.stackTransitions.clear();
    ReconcileCardStacks(context.cards, context.settings);
    context.hoverHeaderAction = CardHeaderAction::None;
    context.sceneDirty = true;
}

inline void ToggleCardPin(int index) {
    if (index < 0 || static_cast<size_t>(index) >= context.cards.size()) {
        return;
    }
    Card& card = context.cards[static_cast<size_t>(index)];
    card.pinned = !card.pinned;
    RememberLayoutPin(card.fingerprint, card.world, card.pinned, card.stack.grouped);
    MarkLayoutDirty(GetTickCount());
    context.sceneDirty = true;
}

// A close request goes to the real window. The card only leaves the canvas
// once the next scan reports the window gone, so a document that refuses to
// close - a save prompt, for example - keeps its card instead of silently
// disappearing and reappearing the next time the canvas is shown.
inline void CloseCardWindow(int index) {
    if (index < 0 || static_cast<size_t>(index) >= context.cards.size()) {
        return;
    }
    const HWND window = context.cards[static_cast<size_t>(index)].hwnd;
    if (window == nullptr || IsWindow(window) == FALSE) {
        // Nothing is left to close, so dismissal is the only remaining action.
        DismissCard(index);
        return;
    }
    if (PostMessageW(window, WM_CLOSE, 0, 0) == FALSE) {
        Wh_Log(L"Aleph: WM_CLOSE could not be posted: %lu", GetLastError());
        return;
    }
    context.hoverHeaderAction = CardHeaderAction::None;
    context.scanRequested = true;
    context.sceneDirty = true;
}

// Group members share one executable: a grouped card is matched by the key its
// stack already carries, an ungrouped card by its executable name, so toggling
// either way touches exactly the windows the button refers to.
inline std::vector<size_t> CollectGroupMemberIndices(
    const std::wstring& groupKey) {
    std::vector<size_t> members;
    if (groupKey.empty()) {
        return members;
    }
    for (size_t index = 0; index < context.cards.size(); ++index) {
        const Card& card = context.cards[index];
        const bool matches = IsGroupedStackCard(card)
                                 ? card.stack.groupKey == groupKey
                                 : ExecutableName(card.exePath) == groupKey;
        if (matches) {
            members.push_back(index);
        }
    }
    std::stable_sort(members.begin(), members.end(),
                     [](const size_t& left, const size_t& right) {
                         return context.cards[left].stack.slot <
                                context.cards[right].stack.slot;
                     });
    return members;
}

// The stack's own anchor is the place the group occupies; members without one
// fall back to the first member's world rectangle.
inline RectF StackAnchorForMembers(const std::vector<size_t>& members) {
    for (const size_t index : members) {
        if (index < context.cards.size() &&
            !context.cards[index].stack.anchorWorld.empty()) {
            return context.cards[index].stack.anchorWorld;
        }
    }
    if (members.empty() || members.front() >= context.cards.size()) {
        return RectF{};
    }
    return context.cards[members.front()].world;
}

// Grouping reuses the stack the canvas already knows: every member takes the
// key and the shared anchor, then the regular reconcile fans them into slots.
inline void GroupCards(const std::vector<size_t>& members,
                       const std::wstring& groupKey) {
    if (members.size() < 2 || groupKey.empty()) {
        return;
    }
    const RectF anchor = StackAnchorForMembers(members);
    for (const size_t index : members) {
        if (index >= context.cards.size()) {
            continue;
        }
        Card& card = context.cards[index];
        ResetStackPlacement(card);
        card.stack.grouped = true;
        card.stack.groupKey = groupKey;
        card.stack.anchorWorld = anchor;
        // The grouped flag is saved too: a later rescan restores the stack
        // from the layout instead of splitting the windows apart again.
        RememberLayoutPin(card.fingerprint,
                          anchor.empty() ? card.world : anchor, card.pinned,
                          true);
    }
    MarkLayoutDirty(GetTickCount());
}

// Ungrouping fans the members out in a row from where the stack stood, so each
// window becomes individually visible and draggable instead of jumping back to
// its own desktop position. The ungrouped flag is saved with the layout so a
// rescan does not restack the windows.
inline void UngroupCards(const std::vector<size_t>& members) {
    const RectF anchor = StackAnchorForMembers(members);
    const float step =
        anchor.width() + static_cast<float>(context.settings.gap);
    // Only the windows that were part of the stack move. A window of the same
    // executable that the user kept apart is a member of the group key but not
    // of the stack, so it keeps the place it was put in.
    size_t position = 0;
    for (const size_t index : members) {
        if (index >= context.cards.size()) {
            continue;
        }
        Card& card = context.cards[index];
        const bool wasStacked = IsGroupedStackCard(card);
        if (wasStacked && !anchor.empty()) {
            card.world =
                MakeRectF(anchor.left + step * static_cast<float>(position),
                          anchor.top, anchor.width(), anchor.height());
            ++position;
        }
        ResetStackPlacement(card);
        card.stack.grouped = false;
        RememberLayoutPin(card.fingerprint, card.world, card.pinned, false);
    }
    MarkLayoutDirty(GetTickCount());
}

inline void ToggleCardGroup(int index) {
    if (!context.settings.stacksEnabled || index < 0 ||
        static_cast<size_t>(index) >= context.cards.size()) {
        return;
    }
    const Card& card = context.cards[static_cast<size_t>(index)];
    const bool grouped = IsGroupedStackCard(card);
    const std::wstring groupKey =
        grouped ? card.stack.groupKey : ExecutableName(card.exePath);
    const std::vector<size_t> members = CollectGroupMemberIndices(groupKey);
    if (members.size() < 2) {
        return;
    }
    if (grouped) {
        UngroupCards(members);
    } else {
        GroupCards(members, groupKey);
    }
    // A split group must not keep the animation of a stack it no longer has.
    context.stackTransitions.clear();
    ReconcileCardStacks(context.cards, context.settings);
    context.hoverHeaderAction = CardHeaderAction::None;
    context.sceneDirty = true;
}

inline bool PointInMinimap(Vec2 point) {
    return context.settings.minimapEnabled &&
           ContainsPoint(context.minimapRect, point.x, point.y);
}

// An other-desktop card only becomes usable once its window joins the desktop
// that displays the canvas. A refused move keeps the old behaviour: the dive
// still ends in foreground activation.
inline void PullOtherDesktopWindow(const Card& card) {
    if (card.destroyed) {
        return;
    }
    const DesktopPullOutcome outcome =
        PullWindowToDesktop(context.desktopManager, card.hwnd, context.hostWindow);
    if (outcome.result != DesktopPullResult::Rejected) {
        return;
    }
    Wh_Log(L"Aleph: MoveWindowToDesktop failed: 0x%08X",
           static_cast<unsigned int>(outcome.error));
}

inline void BeginDive(int index) {
    if (index < 0 || static_cast<size_t>(index) >= context.cards.size()) {
        return;
    }
    context.panning = false;
    context.draggingMinimap = false;
    context.draggedCard = -1;
    context.camera.zoomAnchored = false;
    context.diveCard = index;
    context.diveStart = context.camera;
    Card& card = context.cards[static_cast<size_t>(index)];
    context.diveTarget = DiveCameraForCard(card);
    context.diveStartTick = GetTickCount();
    if (card.state == WindowVisualState::OtherDesktop) {
        PullOtherDesktopWindow(card);
    }
    // Pulling a minimized cross-desktop window leaves it iconic, so the restore
    // follows the pull instead of the scan state.
    if (IsIconic(card.hwnd) != FALSE) {
        ShowWindowAsync(card.hwnd, SW_RESTORE);
    }
    context.diving = true;
}

inline void FinishDive() {
    if (context.diveCard < 0 ||
        static_cast<size_t>(context.diveCard) >= context.cards.size()) {
        context.diving = false;
        return;
    }
    Card& card = context.cards[static_cast<size_t>(context.diveCard)];
    if (!card.destroyed && IsWindow(card.hwnd)) {
        SetForegroundWindow(card.hwnd);
    }
    context.camera.targetCenterX = context.camera.centerX;
    context.camera.targetCenterY = context.camera.centerY;
    context.camera.targetZoom = context.camera.zoom;
    HideCanvasWindows();
}

inline void AdvanceDive(DWORD now) {
    if (!context.diving) {
        return;
    }
    const float duration = static_cast<float>(context.settings.animationMs);
    const float progress = duration <= 0.0f ? 1.0f
        : static_cast<float>(now - context.diveStartTick) / duration;
    const float eased = EaseOutCubic(progress);
    context.camera.centerX = context.diveStart.centerX +
        (context.diveTarget.targetCenterX - context.diveStart.centerX) * eased;
    context.camera.centerY = context.diveStart.centerY +
        (context.diveTarget.targetCenterY - context.diveStart.centerY) * eased;
    context.camera.zoom = context.diveStart.zoom +
        (context.diveTarget.targetZoom - context.diveStart.zoom) * eased;
}

inline void StartPan(POINT cursor) {
    context.camera.snapToTarget();
    context.camera.zoomAnchored = false;
    context.panning = true;
    context.panAnchorCursor = cursor;
    context.panAnchorWorld = ScreenToWorld(
        Vec2{static_cast<float>(cursor.x), static_cast<float>(cursor.y)}, context.camera);
}

inline void UpdatePan(POINT cursor) {
    const Vec2 anchor = context.panAnchorWorld;
    const float zoom = std::max(context.camera.zoom, 0.01f);
    context.camera.targetCenterX = anchor.x -
        (static_cast<float>(cursor.x) - ScreenCenterX()) / zoom;
    context.camera.targetCenterY = anchor.y -
        (static_cast<float>(cursor.y) - ScreenCenterY()) / zoom;
}

inline void BeginCardDrag(int index, POINT cursor) {
    context.hoverHeaderAction = CardHeaderAction::None;
    context.camera.snapToTarget();
    context.camera.zoomAnchored = false;
    context.draggedCard = index;
    context.dragCursor = cursor;
    const RectF screen = WorldToScreen(StackWorldRect(context.cards[static_cast<size_t>(index)]),
                                       context.camera);
    context.dragGrabOffset.x = static_cast<LONG>(static_cast<float>(cursor.x) - screen.left);
    context.dragGrabOffset.y = static_cast<LONG>(static_cast<float>(cursor.y) - screen.top);
    SetCapture(context.hostWindow);
}

inline void UpdateCardDrag(POINT cursor) {
    if (context.draggedCard < 0 ||
        static_cast<size_t>(context.draggedCard) >= context.cards.size()) {
        return;
    }
    const Vec2 world = ScreenToWorld(
        Vec2{static_cast<float>(cursor.x - context.dragGrabOffset.x),
             static_cast<float>(cursor.y - context.dragGrabOffset.y)}, context.camera);
    Card& card = context.cards[static_cast<size_t>(context.draggedCard)];
    RectF moved = MakeRectF(world.x, world.y, card.world.width(), card.world.height());
    const SnapResult snap = ComputeSnap(moved, context.draggedCard);
    moved.left = snap.left;
    moved.top = snap.top;
    moved.right = moved.left + card.world.width();
    moved.bottom = moved.top + card.world.height();
    card.world = moved;
    // Dragging one member moves the whole stack: every member shares the
    // anchor, so update it together instead of leaving the stack behind.
    if (card.stack.grouped && !card.stack.groupKey.empty()) {
        for (Card& member : context.cards) {
            if (member.stack.grouped && member.stack.groupKey == card.stack.groupKey) {
                member.stack.anchorWorld = moved;
            }
        }
    }
    context.hasGuideVertical = snap.snapVertical;
    context.hasGuideHorizontal = snap.snapHorizontal;
    context.guideVertical = RectF{(snap.guideX - context.camera.centerX) * context.camera.zoom +
                                      ScreenCenterX(), 0.0f, 0.0f, 0.0f};
    context.guideHorizontal = RectF{0.0f,
        (snap.guideY - context.camera.centerY) * context.camera.zoom + ScreenCenterY(),
        0.0f, 0.0f};
    RememberLayoutEntry(card.fingerprint, card.world);
    MarkLayoutDirty(GetTickCount());
}

inline void EndPointerDrag() {
    context.panning = false;
    context.draggingMinimap = false;
    context.draggedCard = -1;
    context.hasGuideVertical = false;
    context.hasGuideHorizontal = false;
    context.hoverHeaderAction = CardHeaderAction::None;
    context.sceneDirty = true;
    if (GetCapture() == context.hostWindow) {
        ReleaseCapture();
    }
}

inline void HandleMouseMove(POINT cursor) {
    if (context.diving) {
        return;
    }
    if (UpdateChromeDpi()) {
        ReleaseTextFormats();
        context.sceneDirty = true;
    }
    context.dragCursor = cursor;
    if (context.draggingMinimap) {
        PanCameraFromMinimap(Vec2{static_cast<float>(cursor.x),
                                  static_cast<float>(cursor.y)});
        return;
    }
    if (context.draggedCard >= 0) {
        UpdateCardDrag(cursor);
        context.sceneDirty = true;
        return;
    }
    if (context.panning) {
        UpdatePan(cursor);
        context.sceneDirty = true;
        return;
    }
    const Vec2 point{static_cast<float>(cursor.x), static_cast<float>(cursor.y)};
    const bool minimapHover = PointInMinimap(point);
    const int hoverCard = minimapHover ? -1 : HitTestCard(point);
    const CardHeaderAction hoverAction =
        hoverCard >= 0 ? HeaderActionAt(hoverCard, point) : CardHeaderAction::None;
    if (minimapHover != context.minimapHover || hoverCard != context.hoverCard ||
        hoverAction != context.hoverHeaderAction) {
        context.minimapHover = minimapHover;
        context.hoverCard = hoverCard;
        context.hoverHeaderAction = hoverAction;
        context.sceneDirty = true;
    }
}

inline void HandleMouseLeave() {
    context.hoverCard = -1;
    context.hoverHeaderAction = CardHeaderAction::None;
    context.minimapHover = false;
    context.sceneDirty = true;
}

inline void HandleLeftButtonDown(POINT cursor) {
    if (context.diving) {
        return;
    }
    context.sceneDirty = true;
    const Vec2 point{static_cast<float>(cursor.x), static_cast<float>(cursor.y)};
    if (PointInMinimap(point)) {
        context.draggingMinimap = true;
        SetCapture(context.hostWindow);
        PanCameraFromMinimap(point);
        return;
    }
    const int card = HitTestCard(point);
    if (card >= 0) {
        const CardHeaderAction action = HeaderActionAt(card, point);
        if (action == CardHeaderAction::Close) {
            CloseCardWindow(card);
            return;
        }
        if (action == CardHeaderAction::Pin) {
            ToggleCardPin(card);
            return;
        }
        if (action == CardHeaderAction::Group) {
            ToggleCardGroup(card);
            return;
        }
    }
    // Shift+click on a stacked card brings the next window in the stack
    // to the front. Without it the background windows would be visible
    // in the fan but unreachable by mouse; drag and dive stay unchanged.
    if (card >= 0 && (GetKeyState(VK_SHIFT) & 0x8000) != 0) {
        const std::wstring groupKey = context.cards[static_cast<size_t>(card)].stack.groupKey;
        if (CycleCardStack(context.cards, context.settings, card, 1)) {
            context.selectedCard = card;
            for (size_t index = 0; index < context.cards.size(); ++index) {
                if (context.cards[index].stack.groupKey == groupKey &&
                    context.cards[index].stack.slot == 0) {
                    context.selectedCard = static_cast<int>(index);
                    break;
                }
            }
            return;
        }
    }
    context.selectedCard = card;
    if (card >= 0) {
        BeginCardDrag(card, cursor);
        return;
    }
    if ((GetKeyState(VK_SPACE) & 0x8000) != 0) {
        StartPan(cursor);
        SetCapture(context.hostWindow);
    }
}

inline void HandleMouseWheel(WPARAM wParam, LPARAM lParam) {
    if (context.diving || GET_WHEEL_DELTA_WPARAM(wParam) == 0) {
        return;
    }
    POINT cursor = {GET_X_LPARAM(lParam) - context.virtualScreen.left,
                    GET_Y_LPARAM(lParam) - context.virtualScreen.top};
    // Alt+wheel over a stack cycles it instead of zooming: the wheel is
    // otherwise bound to canvas zoom, so a plain wheel can never switch
    // cards. Alt+wheel that misses a cyclable stack falls through to
    // zoom, and a plain wheel is unchanged.
    if ((GetKeyState(VK_MENU) & 0x8000) != 0) {
        const int hit = HitTestCard(Vec2{static_cast<float>(cursor.x),
                                         static_cast<float>(cursor.y)});
        if (hit >= 0 && CycleStackWithAnimation(
                context.cards,
                StackCycleRequest{context.settings, hit,
                    StackWheelSteps(GET_WHEEL_DELTA_WPARAM(wParam)), GetTickCount()},
                context.stackTransitions)) {
            const std::wstring groupKey =
                context.cards[static_cast<size_t>(hit)].stack.groupKey;
            context.selectedCard = hit;
            for (size_t index = 0; index < context.cards.size(); ++index) {
                if (context.cards[index].stack.groupKey == groupKey &&
                    context.cards[index].stack.slot == 0) {
                    context.selectedCard = static_cast<int>(index);
                    break;
                }
            }
            context.sceneDirty = true;
            return;
        }
    }
    const float steps = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) /
                        static_cast<float>(WHEEL_DELTA);
    const float factor = std::pow(kZoomStep, steps);
    ZoomAtPoint(context.camera, context.camera.targetZoom * factor,
                Vec2{static_cast<float>(cursor.x), static_cast<float>(cursor.y)});
}

inline void HandleKeyDown(WPARAM key) {
    if (key == VK_ESCAPE) {
        HideCanvasWindows();
        return;
    }
    if (context.diving) {
        return;
    }
    if (key == VK_RETURN) {
        BeginDive(context.selectedCard >= 0 ? context.selectedCard : context.hoverCard);
        return;
    }
    if (key == VK_HOME) {
        FitAllCards();
        return;
    }
    if (key == VK_TAB && !context.cards.empty()) {
        const int direction = (GetKeyState(VK_SHIFT) & 0x8000) != 0 ? -1 : 1;
        const int count = static_cast<int>(context.cards.size());
        const int start = context.selectedCard < 0
                              ? (direction > 0 ? -1 : 0)
                              : context.selectedCard;
        context.selectedCard = (start + direction + count) % count;
        CenterOnCard(context.selectedCard);
        return;
    }
    if (key == VK_LEFT || key == VK_RIGHT || key == VK_UP || key == VK_DOWN) {
        context.camera.zoomAnchored = false;
        const float amount = 120.0f / std::max(context.camera.zoom, 0.01f);
        context.camera.targetCenterX += key == VK_LEFT ? -amount : key == VK_RIGHT ? amount : 0.0f;
        context.camera.targetCenterY += key == VK_UP ? -amount : key == VK_DOWN ? amount : 0.0f;
    }
}
