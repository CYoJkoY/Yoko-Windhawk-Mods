#pragma once

// System includes plus the shared Windhawk and Win32 helper headers.

#include <windows.h>
#include <windowsx.h>
#include <windhawk_api.h>
#include <windhawk_utils.h>
#include <d2d1.h>
#include <dwrite.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shellscalingapi.h>
#include <shobjidl.h>
#include <wincodec.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// The DWM cloak values are older than the SDK headers some builds ship.
#ifndef DWMWA_CLOAKED
#define DWMWA_CLOAKED 14
#endif
#ifndef DWM_CLOAKED_APP
#define DWM_CLOAKED_APP 0x00000001
#endif
#ifndef DWM_CLOAKED_SHELL
#define DWM_CLOAKED_SHELL 0x00000002
#endif

// Repository-wide helpers. The paths stay relative to this header so the
// amalgamator can inline them in the same way as local modules.
#include "../../../shared/windhawk_utils.h"
#include "../../../shared/win32_helpers.h"
