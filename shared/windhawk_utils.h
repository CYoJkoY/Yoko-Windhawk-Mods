#pragma once

// Header-only helpers around the Windhawk API.
//
// This file is inlined by tools/bundle.py, so it must stay self-contained and
// must never include another repository header. Every function is inline so
// the amalgamated translation unit keeps a single definition.

#include <windows.h>

#include <windhawk_api.h>
#include <windhawk_utils.h>

#include <string>

namespace yoko {

// Reads an integer setting and clamps it to the documented range.
//
// Windhawk returns 0 for a missing value and does not reject out-of-range
// input, so the clamp has to happen here. An inverted range is a programming
// error in the settings block; it is logged and collapsed to the lower bound
// instead of producing undefined behavior.
inline int GetClampedIntSetting(const wchar_t* path, int minimum, int maximum) {
    if (path == nullptr) {
        Wh_Log(L"GetClampedIntSetting called without a setting path");
        return minimum;
    }
    if (maximum < minimum) {
        Wh_Log(L"GetClampedIntSetting range is inverted for %s", path);
        return minimum;
    }
    const int value = Wh_GetIntSetting(path);
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

// Reads a string setting and falls back when the value is absent.
//
// WindhawkUtils::StringSetting owns the buffer, so the result is copied before
// the temporary goes out of scope.
inline std::wstring GetStringSetting(const wchar_t* path,
                                     const wchar_t* fallback) {
    const wchar_t* safeFallback = fallback != nullptr ? fallback : L"";
    if (path == nullptr) {
        Wh_Log(L"GetStringSetting called without a setting path");
        return safeFallback;
    }
    WindhawkUtils::StringSetting value =
        WindhawkUtils::StringSetting::make(path);
    const wchar_t* text = value.get();
    if (text == nullptr) {
        return safeFallback;
    }
    return text;
}

// Logs the current thread's last error next to a short context string.
//
// The error is captured first so the logging call itself cannot replace it.
inline void LogLastError(const wchar_t* context) {
    const DWORD error = GetLastError();
    Wh_Log(L"%s failed: %lu", context != nullptr ? context : L"Win32 call",
           error);
}

}  // namespace yoko
