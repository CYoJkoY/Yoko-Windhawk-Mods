#pragma once

// System includes plus the shared Windhawk and Win32 helper headers.


#include <windows.h>
#include <windhawk_utils.h>
#include <d2d1.h>
#include <wincodec.h>
#include <math.h>
#include <cmath>
#include <shellapi.h>
#include <shellscalingapi.h>
#include <cwctype>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002u
#endif

// Repository-wide helpers. The paths stay relative to this header so
// the amalgamator can inline them in the same way as local modules.
#include "../../../shared/windhawk_utils.h"
#include "../../../shared/pattern_scan.h"
#include "../../../shared/win32_helpers.h"
