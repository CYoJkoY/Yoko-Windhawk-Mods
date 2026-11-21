#pragma once

#include "camera_ops.h"
#include "canvas_visibility.h"
#include "card_chrome.h"
#include "cards.h"
#include "context.h"
#include "desktop_pull.h"
#include "math_utils.h"
#include "minimap.h"

#include <optional>

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

// Alt is the modifier that turns a plain drag into a stack gesture: it both
// pulls a card out of its fan and joins it to the card it is dropped on. Every
// handler reads it through here so the two halves always agree.
inline bool AltHeld() {
    return (GetKeyState(VK_MENU) & 0x8000) != 0;
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

inline void ToggleCardPin(int index) {
    if (index < 0 || static_cast<size_t>(index) >= context.cards.size()) {
        return;
    }
    Card& card = context.cards[static_cast<size_t>(index)];
    card.pinned = !card.pinned;
    RememberLayoutPin(card.fingerprint, StackWorldRect(card), card.pinned, card.stack.grouped);
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
    const Card& card = context.cards[static_cast<size_t>(index)];
    const HWND window = card.hwnd;
    DWORD processId = 0;
    const DWORD threadId = card.destroyed || window == nullptr
                               ? 0 : GetWindowThreadProcessId(window, &processId);
    if (threadId == 0 || threadId != card.threadId || processId != card.processId) {
        RemoveCardAt(static_cast<size_t>(index));
        context.stackTransitions.clear();
        ReconcileCardStacks(context.cards, context.settings);
    } else if (PostMessageW(window, WM_CLOSE, 0, 0) == FALSE) {
        Wh_Log(L"Aleph: WM_CLOSE could not be posted: %lu", GetLastError());
        return;
    }
    context.hoverHeaderAction = CardHeaderAction::None;
    context.scanRequested = true;
    context.sceneDirty = true;
}

inline std::vector<size_t> CollectGroupMemberIndices(
    const std::wstring& groupKey) {
    std::vector<size_t> members;
    if (groupKey.empty()) {
        return members;
    }
    for (size_t index = 0; index < context.cards.size(); ++index) {
        const Card& card = context.cards[index];
        if (StackGroupKeyOf(card) == groupKey) {
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

// The members that actually fan out of one stack, front first. A window of the
// same application that the user kept apart shares the key but is not part of
// the fan, so joining or splitting a stack never drags it in.
inline std::vector<size_t> CollectFannedMemberIndices(
    const std::wstring& groupKey) {
    std::vector<size_t> members;
    if (groupKey.empty()) {
        return members;
    }
    for (size_t index = 0; index < context.cards.size(); ++index) {
        const Card& card = context.cards[index];
        if (IsGroupedStackCard(card) && card.stack.groupKey == groupKey) {
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

// The stack's own frame is the place the group occupies; members without one
// fall back to the first member's world rectangle.
inline RectF StackFrameForMembers(const std::vector<size_t>& members) {
    for (const size_t index : members) {
        if (index < context.cards.size() &&
            !context.cards[index].stack.frameWorld.empty()) {
            return context.cards[index].stack.frameWorld;
        }
    }
    if (members.empty() || members.front() >= context.cards.size()) {
        return RectF{};
    }
    return context.cards[members.front()].world;
}

// Grouping reuses the stack the canvas already knows: every member takes the
// key and the shared frame, then the regular reconcile fans them into slots.
inline void GroupCards(const std::vector<size_t>& members,
                       const std::wstring& groupKey) {
    if (members.size() < 2 || groupKey.empty()) {
        return;
    }
    const RectF frame = StackFrameForMembers(members);
    for (const size_t index : members) {
        if (index >= context.cards.size()) {
            continue;
        }
        Card& card = context.cards[index];
        ResetStackPlacement(card);
        card.stack.grouped = true;
        card.stack.groupKey = groupKey;
        card.stack.frameWorld = frame;
        // The grouped flag is saved too: a later rescan restores the stack
        // from the layout instead of splitting the windows apart again.
        RememberLayoutPin(card.fingerprint, StackWorldRect(card), card.pinned, true);
    }
    MarkLayoutDirty(GetTickCount());
}

// Ungrouping fans the members out in a row from where the stack stood, so each
// window becomes individually visible and draggable instead of jumping back to
// its own desktop position. Every released member takes back its own window
// size, so a card never keeps the proportions of the frame it shared. The
// ungrouped flag is saved with the layout so a rescan does not restack the
// windows.
inline void UngroupCards(const std::vector<size_t>& members) {
    std::optional<float> nextLeft;
    for (const size_t index : members) {
        if (index >= context.cards.size()) {
            continue;
        }
        Card& card = context.cards[index];
        if (IsGroupedStackCard(card)) {
            RestoreOwnSizeCentredInFrame(card);
            const float width = card.world.width();
            card.world.left = nextLeft.value_or(card.world.left);
            card.world.right = card.world.left + width;
            nextLeft = card.world.right + static_cast<float>(context.settings.gap);
        }
        ResetStackPlacement(card);
        card.stack.grouped = false;
        card.stackId.clear();
        // One write keeps the released geometry, the ungrouped flag and the
        // cleared hand-made id in step instead of saving them twice.
        RememberLayoutCard(card);
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
    const std::wstring groupKey = StackGroupKeyOf(card);
    if (grouped) {
        // Only the windows that really fan out are released: a same-app window
        // the user kept apart already stands on its own.
        const std::vector<size_t> fanned =
            CollectFannedMemberIndices(groupKey);
        if (fanned.size() < 2) {
            return;
        }
        UngroupCards(fanned);
    } else {
        std::vector<size_t> candidates = CollectGroupMemberIndices(groupKey);
        if (candidates.size() < 2) {
            return;
        }
        // The clicked window owns the shared frame: it is the card the user
        // acted on and the one at the front, so regrouping must not shrink the
        // stack to whichever member happens to sort first.
        const auto clicked = std::find(candidates.begin(), candidates.end(),
                                       static_cast<size_t>(index));
        if (clicked != candidates.end()) {
            std::rotate(candidates.begin(), clicked, clicked + 1);
        }
        GroupCards(candidates, groupKey);
    }
    // A split group must not keep the animation of a stack it no longer has.
    context.stackTransitions.clear();
    ReconcileCardStacks(context.cards, context.settings);
    context.hoverHeaderAction = CardHeaderAction::None;
    context.sceneDirty = true;
}

// A hand-made stack is keyed by the fingerprints of its members: they survive
// in the layout file, so the id of a group never changes between restarts and
// the same windows come back together. The fingerprints are sorted so stacking
// B onto A yields the same id as stacking A onto B: the id names the set of
// windows, not the order they were dropped in.
inline std::wstring NewStackIdFor(const std::vector<size_t>& members) {
    std::vector<std::wstring> fingerprints;
    fingerprints.reserve(members.size());
    for (const size_t index : members) {
        if (index < context.cards.size()) {
            fingerprints.push_back(context.cards[index].fingerprint);
        }
    }
    if (fingerprints.empty()) {
        return std::wstring();
    }
    std::sort(fingerprints.begin(), fingerprints.end());
    std::wstring stackId = L"stack:";
    for (size_t slot = 0; slot < fingerprints.size(); ++slot) {
        if (slot > 0) {
            stackId.push_back(L'|');
        }
        stackId += fingerprints[slot];
    }
    return stackId;
}

// Alt+dropping one card onto another joins them into a stack of their own,
// which may span different applications. Dropping onto a member of an existing
// fan brings that fan's windows along instead of starting a second one, and the
// dropped card becomes the new front so it reads as the card just placed.
inline void StackCardOntoCard(size_t draggedIndex, size_t targetIndex) {
    if (draggedIndex >= context.cards.size() ||
        targetIndex >= context.cards.size() || draggedIndex == targetIndex) {
        return;
    }
    if (SameActiveStack(context.cards[draggedIndex],
                        context.cards[targetIndex])) {
        return;
    }
    // A window the user kept apart shares the key but not the fan, so only the
    // fanned members are carried along.
    std::vector<size_t> members = CollectFannedMemberIndices(
        context.cards[targetIndex].stack.groupKey);
    if (members.size() < 2) {
        members.assign(1, targetIndex);
    }
    if (std::find(members.begin(), members.end(), draggedIndex) ==
        members.end()) {
        members.push_back(draggedIndex);
    }
    // Reuse the id of a stack that already is hand-made, so joining it keeps
    // the group (and its saved layout) intact.
    const std::wstring targetStackId = context.cards[targetIndex].stackId;
    const std::wstring stackId =
        targetStackId.empty() ? NewStackIdFor(members) : targetStackId;
    if (stackId.empty()) {
        return;
    }
    // Read the frame before the reset below: dropping onto a member of a formed
    // fan must keep that fan's shared frame, or the whole stack would snap to
    // the one member's own (now stale) window rectangle. A lone target lends its
    // own rectangle so the stack grows out of the card that was aimed at.
    const RectF frame = StackWorldRect(context.cards[targetIndex]);
    for (const size_t index : members) {
        Card& card = context.cards[index];
        ResetStackPlacement(card);
        card.stackId = stackId;
        card.stack.grouped = true;
        card.stack.groupKey = stackId;
    }
    std::vector<size_t> order;
    order.reserve(members.size());
    order.push_back(draggedIndex);
    for (const size_t index : members) {
        if (index != draggedIndex) {
            order.push_back(index);
        }
    }
    for (Card& card : context.cards) {
        if (card.stack.groupKey == stackId) {
            card.stack.frameWorld = frame;
        }
    }
    context.stackTransitions.clear();
    ReconcileCardStacks(context.cards, context.settings);
    ApplyStackOrder(context.cards, order, context.settings);
    for (const size_t index : members) {
        RememberLayoutCard(context.cards[index]);
    }
    context.selectedCard = static_cast<int>(draggedIndex);
    context.dropTargetCard = -1;
    context.sceneDirty = true;
}

// Alt+dragging a member out of a stack releases just that window: it takes
// back its own size, keeps the place the stack stood, and is marked ungrouped
// so the automatic per-application stack does not pull it straight back in.
inline void DetachCardFromStack(size_t index) {
    if (index >= context.cards.size() ||
        !IsGroupedStackCard(context.cards[index])) {
        return;
    }
    const std::wstring leftKey = context.cards[index].stack.groupKey;
    RestoreOwnSizeCentredInFrame(context.cards[index]);
    Card& card = context.cards[index];
    card.stackId.clear();
    ResetStackPlacement(card);
    card.stack.grouped = false;
    context.stackTransitions.clear();
    ReconcileCardStacks(context.cards, context.settings);
    RememberLayoutCard(card);
    // The rest of the fan kept its place, so their saved geometry has to follow
    // the slots the release handed out.
    for (Card& member : context.cards) {
        if (member.stack.grouped && !member.stack.groupKey.empty() &&
            member.stack.groupKey == leftKey) {
            RememberLayoutCard(member);
        }
    }
    context.detachFromKey = leftKey;
    context.dragDetached = true;
    context.sceneDirty = true;
}

// The card a drop lands on. Only a solid overlap counts, so brushing a card
// never merges two windows by accident; the largest overlap wins when the
// dragged card covers several.
inline int DropTargetCard(size_t draggedIndex) {
    constexpr float kMergeOverlapFraction = 0.35f;
    if (draggedIndex >= context.cards.size()) {
        return -1;
    }
    const RectF source = StackWorldRect(context.cards[draggedIndex]);
    int best = -1;
    float bestArea = 0.0f;
    float bestSmallerArea = 0.0f;
    for (size_t index = 0; index < context.cards.size(); ++index) {
        const Card& card = context.cards[index];
        if (index == draggedIndex || card.state != WindowVisualState::Live ||
            SameActiveStack(context.cards[draggedIndex], card) ||
            (!context.detachFromKey.empty() &&
             card.stack.groupKey == context.detachFromKey)) {
            continue;
        }
        const RectF bounds = StackWorldRect(card);
        const float left = std::max(source.left, bounds.left);
        const float top = std::max(source.top, bounds.top);
        const float right = std::min(source.right, bounds.right);
        const float bottom = std::min(source.bottom, bounds.bottom);
        if (right <= left || bottom <= top) {
            continue;
        }
        const float area = (right - left) * (bottom - top);
        if (area <= bestArea) {
            continue;
        }
        bestArea = area;
        bestSmallerArea = std::min(source.width() * source.height(),
                                   bounds.width() * bounds.height());
        best = static_cast<int>(index);
    }
    if (best < 0 || bestArea < bestSmallerArea * kMergeOverlapFraction) {
        return -1;
    }
    return best;
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

// The grab offset keeps the card under the point the user pressed, so a card
// swapped for one of another size mid-drag never jumps.
inline void SetDragGrabOffset(int index, POINT cursor) {
    if (index < 0 || static_cast<size_t>(index) >= context.cards.size()) {
        return;
    }
    const RectF screen = WorldToScreen(
        StackWorldRect(context.cards[static_cast<size_t>(index)]),
        context.camera);
    context.dragGrabOffset.x =
        static_cast<LONG>(static_cast<float>(cursor.x) - screen.left);
    context.dragGrabOffset.y =
        static_cast<LONG>(static_cast<float>(cursor.y) - screen.top);
}

inline void BeginCardDrag(int index, POINT cursor) {
    context.hoverHeaderAction = CardHeaderAction::None;
    context.camera.snapToTarget();
    context.camera.zoomAnchored = false;
    context.draggedCard = index;
    context.dragDetached = false;
    context.detachFromKey.clear();
    context.dropTargetCard = -1;
    context.dragCursor = cursor;
    SetDragGrabOffset(index, cursor);
    SetCapture(context.hostWindow);
}

inline void UpdateCardDrag(POINT cursor, bool combine) {
    if (context.draggedCard < 0 ||
        static_cast<size_t>(context.draggedCard) >= context.cards.size()) {
        return;
    }
    context.dragCursor = cursor;
    // Alt+dragging a member pulls it out of the fan first, so the rest of the
    // gesture moves one window at its own size instead of the whole stack.
    if (!context.dragDetached &&
        combine &&
        IsGroupedStackCard(context.cards[static_cast<size_t>(context.draggedCard)])) {
        DetachCardFromStack(static_cast<size_t>(context.draggedCard));
        SetDragGrabOffset(context.draggedCard, cursor);
    }
    const Vec2 world = ScreenToWorld(
        Vec2{static_cast<float>(cursor.x - context.dragGrabOffset.x),
             static_cast<float>(cursor.y - context.dragGrabOffset.y)}, context.camera);
    Card& card = context.cards[static_cast<size_t>(context.draggedCard)];
    // The frame is what the user grabs, so the drag follows its size instead
    // of a single member's window size.
    const RectF current = StackWorldRect(card);
    RectF moved = MakeRectF(world.x, world.y, current.width(), current.height());
    const SnapResult snap = ComputeSnap(moved, context.draggedCard);
    moved.left = snap.left;
    moved.top = snap.top;
    moved.right = moved.left + current.width();
    moved.bottom = moved.top + current.height();
    if (IsGroupedStackCard(card)) {
        // Dragging a stack moves its one frame, so every member keeps the
        // shared size and centre while the whole stack follows the cursor.
        const RectF frame = moved;
        for (Card& member : context.cards) {
            if (SameActiveStack(card, member)) {
                member.stack.frameWorld = frame;
            }
        }
    } else {
        card.world = moved;
        RememberLayoutEntry(card.fingerprint, card.world);
        MarkLayoutDirty(GetTickCount());
    }
    // Alt+dragging also shows which card the window would join, so the merge
    // is a deliberate aim instead of a surprise on release.
    context.dropTargetCard =
        combine ? DropTargetCard(static_cast<size_t>(context.draggedCard)) : -1;
    context.hasGuideVertical = snap.snapVertical;
    context.hasGuideHorizontal = snap.snapHorizontal;
    context.guideVertical = RectF{(snap.guideX - context.camera.centerX) * context.camera.zoom +
                                      ScreenCenterX(), 0.0f, 0.0f, 0.0f};
    context.guideHorizontal = RectF{0.0f,
        (snap.guideY - context.camera.centerY) * context.camera.zoom + ScreenCenterY(),
        0.0f, 0.0f};
}

inline void UpdateCardDrag(POINT cursor) {
    UpdateCardDrag(cursor, AltHeld());
}

inline void RememberDraggedStack() {
    if (context.draggedCard < 0 ||
        static_cast<size_t>(context.draggedCard) >= context.cards.size()) {
        return;
    }
    const Card& dragged = context.cards[static_cast<size_t>(context.draggedCard)];
    if (!IsGroupedStackCard(dragged)) {
        return;
    }
    for (const Card& member : context.cards) {
        if (SameActiveStack(dragged, member)) {
            RememberLayoutPin(member.fingerprint, StackWorldRect(member),
                              member.pinned, member.stack.grouped);
        }
    }
    MarkLayoutDirty(GetTickCount());
}

inline void EndPointerDrag(bool combine) {
    // Alt+drop joins the dragged window to the card it lands on. A plain drop
    // leaves both cards alone: only the Z-order decides what covers what.
    if (context.draggedCard >= 0 && combine) {
        const int target =
            DropTargetCard(static_cast<size_t>(context.draggedCard));
        if (target >= 0) {
            StackCardOntoCard(static_cast<size_t>(context.draggedCard),
                              static_cast<size_t>(target));
        }
    }
    RememberDraggedStack();
    context.panning = false;
    context.draggingMinimap = false;
    context.draggedCard = -1;
    context.dragDetached = false;
    context.detachFromKey.clear();
    context.dropTargetCard = -1;
    context.hasGuideVertical = false;
    context.hasGuideHorizontal = false;
    context.hoverHeaderAction = CardHeaderAction::None;
    context.sceneDirty = true;
    if (GetCapture() == context.hostWindow) {
        ReleaseCapture();
    }
}

inline void EndPointerDrag() {
    EndPointerDrag(AltHeld());
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
    if (AltHeld()) {
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
