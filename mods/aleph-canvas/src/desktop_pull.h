#pragma once

// Moving an other-desktop window onto the desktop that shows the canvas.
//
// `IVirtualDesktopManager::MoveWindowToDesktop` needs the GUID of the target
// desktop, and the documented way to obtain one is `GetWindowDesktopId`. The
// canvas host window is the only window the mod owns that the user is looking
// at when activating a card, so its desktop identifier is the pull target.

#include "platform.h"

enum class DesktopPullResult {
    // No usable manager, handle or reference; the window stays where it is.
    Unavailable,
    // The window already lives on the target desktop.
    AlreadyHere,
    // The move was accepted.
    Moved,
    // The move was requested and refused.
    Rejected,
};

struct DesktopPullOutcome {
    DesktopPullResult result = DesktopPullResult::Unavailable;
    HRESULT error = S_OK;
};

inline DesktopPullOutcome PullWindowToDesktop(IVirtualDesktopManager* manager,
                                              HWND window, HWND reference) {
    DesktopPullOutcome outcome;
    if (manager == nullptr || window == nullptr || reference == nullptr) {
        return outcome;
    }
    GUID target = {};
    if (FAILED(manager->GetWindowDesktopId(reference, &target))) {
        return outcome;
    }
    GUID current = {};
    if (SUCCEEDED(manager->GetWindowDesktopId(window, &current)) &&
        IsEqualGUID(current, target) != FALSE) {
        outcome.result = DesktopPullResult::AlreadyHere;
        return outcome;
    }
    // An unreadable source desktop is not proof that no move is needed, so only
    // a confirmed match skips the request.
    outcome.error = manager->MoveWindowToDesktop(window, target);
    outcome.result = SUCCEEDED(outcome.error) ? DesktopPullResult::Moved
                                              : DesktopPullResult::Rejected;
    return outcome;
}
