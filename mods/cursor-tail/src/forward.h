#pragma once

// Declarations for helpers that are called before their definition.

#include "platform.h"
#include "constants.h"
#include "types.h"

static void SetTrailCoreColor(uint32_t core, float luminance);
static void ReleaseOrnamentBitmaps();
static void ClearRipples();
static void ClearMotionAccents();
static void ResetMotionCueTracking();
static bool OrnamentActive();
static void ResetAutoColorSamplingCache();
static bool HasActiveRipples(LONGLONG nowQpc);
