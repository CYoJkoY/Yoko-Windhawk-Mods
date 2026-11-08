#pragma once

#include "platform.h"
#include "context.h"

static std::wstring TrimLower(std::wstring value) {
    const size_t first = value.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos) return {};
    const size_t last = value.find_last_not_of(L" \t\r\n");
    value = value.substr(first, last - first + 1);
    for (wchar_t& character : value) character = static_cast<wchar_t>(towlower(character));
    return value;
}

static UINT ParseResetKey() {
    const std::wstring value = yoko::GetStringSetting(L"Reset.reset_key", L"R");
    if (value.empty()) return 'R';
    const wchar_t character = value[0];
    if ((character >= L'a' && character <= L'z') ||
        (character >= L'A' && character <= L'Z') ||
        (character >= L'0' && character <= L'9')) {
        return static_cast<UINT>(towupper(character));
    }
    return 'R';
}

static void LoadProcessBlacklist() {
    context.processBlacklist.clear();
    constexpr int kMaximumEntries = 64;
    int consecutiveEmptyEntries = 0;
    for (int index = 0; index < kMaximumEntries; ++index) {
        WindhawkUtils::StringSetting setting =
            WindhawkUtils::StringSetting::make(L"Advanced.process_blacklist[%d]", index);
        const wchar_t* value = setting.get();
        if (!value || !*value) {
            if (++consecutiveEmptyEntries >= 2) break;
            continue;
        }
        consecutiveEmptyEntries = 0;
        const std::wstring entry = TrimLower(value);
        if (!entry.empty()) context.processBlacklist.push_back(entry);
    }
}

static void LoadSettings() {
    context.edgeDistance = yoko::GetClampedIntSetting(L"Behavior.edge_distance", 8, 300);
    context.dwellTime = yoko::GetClampedIntSetting(L"Behavior.dwell_time", 0, 3000);
    context.maxSpeed = yoko::GetClampedIntSetting(L"Behavior.max_speed", 50, 5000);
    context.easing = yoko::GetClampedIntSetting(L"Behavior.easing", 0, 100);
    context.constrainBounds = Wh_GetIntSetting(L"Behavior.constrain_bounds") != 0;
    context.resetKey = ParseResetKey();
    context.resetModifiers = 0;
    if (Wh_GetIntSetting(L"Reset.ctrl") != 0) context.resetModifiers |= MOD_CONTROL;
    if (Wh_GetIntSetting(L"Reset.alt") != 0) context.resetModifiers |= MOD_ALT;
    if (Wh_GetIntSetting(L"Reset.win") != 0) context.resetModifiers |= MOD_WIN;
    if (Wh_GetIntSetting(L"Reset.shift") != 0) context.resetModifiers |= MOD_SHIFT;
    if (context.resetModifiers == 0) context.resetModifiers = MOD_CONTROL | MOD_ALT | MOD_WIN;

    LoadProcessBlacklist();
}
