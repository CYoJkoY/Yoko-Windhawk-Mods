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

// Windhawk returns 0 for a missing integer key, which cannot be told apart
// from a stored zero. The raw string form is empty only when the key is
// absent, so it decides whether the declared default applies.
static int ReadIntSetting(const wchar_t* path, int defaultValue, int minimum, int maximum) {
    const int value = Wh_GetIntSetting(path);
    if (value != 0) return std::clamp(value, minimum, maximum);

    WindhawkUtils::StringSetting raw = WindhawkUtils::StringSetting::make(path);
    const wchar_t* text = raw.get();
    if (!text || !*text) return std::clamp(defaultValue, minimum, maximum);

    wchar_t* end = nullptr;
    const long parsed = wcstol(text, &end, 10);
    if (end == text) return std::clamp(defaultValue, minimum, maximum);
    return std::clamp(static_cast<int>(parsed), minimum, maximum);
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

static bool ParseKeyToken(const std::wstring& token, UINT& key) {
    if (token.size() == 1) {
        const wchar_t character = token[0];
        if ((character >= L'a' && character <= L'z') ||
            (character >= L'A' && character <= L'Z') ||
            (character >= L'0' && character <= L'9')) {
            key = static_cast<UINT>(towupper(character));
            return true;
        }
        return false;
    }
    if (token.size() >= 2 && token[0] == L'f') {
        wchar_t* end = nullptr;
        const long number = wcstol(token.c_str() + 1, &end, 10);
        if (end == token.c_str() + 1 || *end != L'\0') return false;
        if (number < 1 || number > 24) return false;
        key = VK_F1 + static_cast<UINT>(number - 1);
        return true;
    }
    return false;
}

// Parses combinations such as Ctrl+Alt+Win+Q or Ctrl+Shift+F9. The parser is
// strict on purpose: a typo must disable the hotkey instead of silently
// registering a different one, so it never steals keys the user did not ask
// for. A combination without a modifier is rejected as well.
static bool ParseHotkey(const std::wstring& text, UINT& modifiers, UINT& key) {
    modifiers = 0;
    key = 0;
    bool hasKey = false;
    size_t start = 0;
    while (start <= text.size()) {
        const size_t separator = text.find(L'+', start);
        const std::wstring token = TrimLower(text.substr(start, separator - start));
        start = separator == std::wstring::npos ? text.size() + 1 : separator + 1;
        if (token.empty()) return false;
        if (token == L"ctrl" || token == L"control") {
            modifiers |= MOD_CONTROL;
        } else if (token == L"alt") {
            modifiers |= MOD_ALT;
        } else if (token == L"win" || token == L"windows" ||
                   token == L"meta" || token == L"super") {
            modifiers |= MOD_WIN;
        } else if (token == L"shift") {
            modifiers |= MOD_SHIFT;
        } else if (!hasKey && ParseKeyToken(token, key)) {
            hasKey = true;
        } else {
            return false;
        }
    }
    return hasKey && modifiers != 0;
}

static void LoadJumpHotkey() {
    const std::wstring value = yoko::GetStringSetting(L"QuickJump.jump_hotkey", L"Ctrl+Alt+Win+Q");
    UINT modifiers = 0;
    UINT key = 0;
    context.jumpEnabled = ParseHotkey(value, modifiers, key);
    if (!context.jumpEnabled) {
        if (!value.empty()) Wh_Log(L"Ignoring invalid quick jump hotkey");
        return;
    }
    context.jumpModifiers = modifiers;
    context.jumpKey = key;
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
    context.edgeDistance = ReadIntSetting(L"Behavior.edge_distance", kDefaultEdgeDistance, 8, 400);
    context.dwellTime = ReadIntSetting(L"Behavior.dwell_time", kDefaultDwellTime, 10, 3000);
    context.maxSpeed = ReadIntSetting(L"Behavior.max_speed", kDefaultMaxSpeed, 100, 6000);
    context.easing = ReadIntSetting(L"Behavior.easing", kDefaultEasing, 1, 100);
    context.constrainBounds = Wh_GetIntSetting(L"Behavior.constrain_bounds") != 0;
    context.resetKey = ParseResetKey();
    context.resetModifiers = 0;
    if (Wh_GetIntSetting(L"Reset.ctrl") != 0) context.resetModifiers |= MOD_CONTROL;
    if (Wh_GetIntSetting(L"Reset.alt") != 0) context.resetModifiers |= MOD_ALT;
    if (Wh_GetIntSetting(L"Reset.win") != 0) context.resetModifiers |= MOD_WIN;
    if (Wh_GetIntSetting(L"Reset.shift") != 0) context.resetModifiers |= MOD_SHIFT;
    if (context.resetModifiers == 0) context.resetModifiers = MOD_CONTROL | MOD_ALT | MOD_WIN;
    LoadJumpHotkey();

    LoadProcessBlacklist();
}
