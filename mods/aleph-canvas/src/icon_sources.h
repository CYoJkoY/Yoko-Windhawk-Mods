#pragma once

// Icon source selection for icon cards.
//
// An icon card draws its bitmap at up to kCardIconMaxPx display pixels, so the
// lookup starts at the largest source the shell offers. Asking for a 48 px
// shell icon or the 32 px window icon enlarges the bitmap two to four times and
// the interpolation shows as a soft, blurry icon on a card that has no live
// preview to distract from it.

#include "platform.h"

#include <cstddef>

// Largest first. The shell answers with the requested size or fails, so a
// missing jumbo list on an older image list simply falls through.
inline constexpr int kShellIconListSizes[] = {SHIL_JUMBO, SHIL_EXTRALARGE,
                                              SHIL_LARGE};

// Returns the first icon the callback produces for the candidate sizes, or
// nullptr when every candidate fails. Ownership of a returned icon passes to
// the caller, which must destroy it.
template <size_t Count, typename QueryIcon>
inline HICON QueryFirstAvailableIcon(const int (&sizes)[Count],
                                     QueryIcon queryIcon) {
    for (size_t index = 0; index < Count; ++index) {
        HICON icon = queryIcon(sizes[index]);
        if (icon != nullptr) {
            return icon;
        }
    }
    return nullptr;
}
