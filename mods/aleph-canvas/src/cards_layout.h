#pragma once

// Shared geometry helpers for scan candidate selection and card packing.

#include "types.h"

inline float CardArea(const ScannedWindow& window) {
    return static_cast<float>(window.framePx.right - window.framePx.left) *
           static_cast<float>(window.framePx.bottom - window.framePx.top);
}
