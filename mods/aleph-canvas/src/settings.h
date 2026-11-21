#pragma once

// Settings schema, hotkey parsing and persistence paths.

#include "platform.h"
#include "constants.h"
#include "context.h"
#include "math_utils.h"
#include "text_utils.h"
#include "types.h"

static uint32_t ReadColorSetting(const wchar_t* path, uint32_t fallback) {
    uint32_t parsed = 0;
    WindhawkUtils::StringSetting value = WindhawkUtils::StringSetting::make(path);
    return ParseHexColor(value.get(), parsed) ? parsed : fallback;
}

struct NamedHotkeyValue {
    const wchar_t* name;
    UINT value;
};

static constexpr NamedHotkeyValue kNamedHotkeyKeys[] = {
    {L"space", VK_SPACE},       {L"tab", VK_TAB},
    {L"enter", VK_RETURN},      {L"return", VK_RETURN},
    {L"escape", VK_ESCAPE},     {L"esc", VK_ESCAPE},
    {L"backspace", VK_BACK},    {L"insert", VK_INSERT},
    {L"delete", VK_DELETE},     {L"home", VK_HOME},
    {L"end", VK_END},           {L"pageup", VK_PRIOR},
    {L"pagedown", VK_NEXT},     {L"left", VK_LEFT},
    {L"right", VK_RIGHT},       {L"up", VK_UP},
    {L"down", VK_DOWN},
};

static constexpr NamedHotkeyValue kNamedHotkeyModifiers[] = {
    {L"ctrl", MOD_CONTROL}, {L"control", MOD_CONTROL},
    {L"alt", MOD_ALT},      {L"shift", MOD_SHIFT},
    {L"win", MOD_WIN},      {L"windows", MOD_WIN},
};

static bool ParseCharacterKey(wchar_t character, UINT& key) {
    const bool digit = character >= L'0' && character <= L'9';
    const bool letter = character >= L'a' && character <= L'z';
    if (!digit && !letter) {
        return false;
    }
    key = static_cast<UINT>(towupper(character));
    return true;
}

static bool ParseFunctionKey(const std::wstring& token, UINT& key) {
    if (token.size() < 2 || token[0] != L'f') {
        return false;
    }
    UINT number = 0;
    for (size_t index = 1; index < token.size(); ++index) {
        const wchar_t character = token[index];
        if (character < L'0' || character > L'9') {
            return false;
        }
        number = number * 10 + static_cast<UINT>(character - L'0');
        if (number > 24) {
            return false;
        }
    }
    if (number == 0) {
        return false;
    }
    key = VK_F1 + number - 1;
    return true;
}

static bool ParseKeyName(const std::wstring& token, UINT& key) {
    const std::wstring lower = ToLowerText(token);
    if (lower.size() == 1) {
        return ParseCharacterKey(lower[0], key);
    }
    for (const NamedHotkeyValue& entry : kNamedHotkeyKeys) {
        if (lower == entry.name) {
            key = entry.value;
            return true;
        }
    }
    return ParseFunctionKey(lower, key);
}

struct ParsedHotkey {
    UINT modifiers = 0;
    UINT key = 0;
};

static bool ParseHotkeyToken(const std::wstring& token, ParsedHotkey& parsed) {
    const std::wstring lower = ToLowerText(token);
    for (const NamedHotkeyValue& entry : kNamedHotkeyModifiers) {
        if (lower == entry.name) {
            parsed.modifiers |= entry.value;
            return true;
        }
    }
    if (parsed.key != 0) {
        return false;
    }
    return ParseKeyName(lower, parsed.key);
}

static bool ParseHotkeyText(const std::wstring& text, UINT& modifiers, UINT& key) {
    modifiers = 0;
    key = 0;
    const std::wstring trimmed = TrimText(text);
    if (trimmed.empty()) {
        return false;
    }

    size_t start = 0;
    ParsedHotkey parsed;
    while (start <= trimmed.size()) {
        const size_t plus = trimmed.find(L'+', start);
        const std::wstring token = TrimText(
            trimmed.substr(start, plus == std::wstring::npos ? std::wstring::npos
                                                            : plus - start));
        if (token.empty() || !ParseHotkeyToken(token, parsed)) {
            return false;
        }
        if (plus == std::wstring::npos) {
            break;
        }
        start = plus + 1;
    }
    if (parsed.key == 0) {
        return false;
    }
    modifiers = parsed.modifiers;
    key = parsed.key;
    return true;
}

static void LoadExcludeList() {
    context.settings.excludeList.clear();
    int consecutiveEmpty = 0;
    for (int index = 0; index < kMaxExcludeEntries; ++index) {
        WindhawkUtils::StringSetting setting = WindhawkUtils::StringSetting::make(
            L"Application.exclude_list[%d]", index);
        const wchar_t* value = setting.get();
        const bool empty = value == nullptr || *value == L'\0';
        consecutiveEmpty = empty ? consecutiveEmpty + 1 : 0;
        if (consecutiveEmpty >= 2) {
            break;
        }
        if (empty) {
            continue;
        }
        const std::wstring lower = ToLowerText(TrimText(value));
        if (!lower.empty() && lower[0] != L'#') {
            context.settings.excludeList.push_back(lower);
        }
    }
}

static void LoadSettings() {
    CanvasSettings& settings = context.settings;

    WindhawkUtils::StringSetting language =
        WindhawkUtils::StringSetting::make(L"Behavior.language");
    settings.language = ResolveLanguageSetting(language.get());

    WindhawkUtils::StringSetting hotkey =
        WindhawkUtils::StringSetting::make(L"Behavior.toggle_hotkey");
    const wchar_t* hotkeyText = hotkey.get();
    if (hotkeyText != nullptr) {
        settings.toggleHotkey = TrimText(hotkeyText);
    }

    settings.openOnStart =
        yoko::GetClampedIntSetting(L"Behavior.open_on_start", 0, 1) != 0;
    settings.animationMs =
        yoko::GetClampedIntSetting(L"Behavior.animation_ms", kMinAnimationMs,
                                   kMaxAnimationMs);
    settings.rescanIntervalMs = yoko::GetClampedIntSetting(
        L"Behavior.rescan_interval_ms", kMinRescanIntervalMs,
        kMaxRescanIntervalMs);

    settings.showGrid =
        yoko::GetClampedIntSetting(L"Canvas.show_grid", 0, 1) != 0;
    settings.gridSpacing =
        yoko::GetClampedIntSetting(L"Canvas.grid_spacing", 16, 512);
    settings.gap = yoko::GetClampedIntSetting(L"Canvas.gap", 0, 256);
    settings.snapDistance =
        yoko::GetClampedIntSetting(L"Canvas.snap_distance", 0, 48);
    settings.includeOtherDesktops =
        yoko::GetClampedIntSetting(L"Canvas.include_other_desktops", 0, 1) != 0;
    settings.saveLayout =
        yoko::GetClampedIntSetting(L"Canvas.save_layout", 0, 1) != 0;

    settings.minZoomPercent = yoko::GetClampedIntSetting(
        L"Appearance.min_zoom_percent", kMinZoomPercent, kMaxZoomPercent - 5);
    settings.maxZoomPercent = yoko::GetClampedIntSetting(
        L"Appearance.max_zoom_percent", settings.minZoomPercent + 5,
        kMaxZoomPercent);

    settings.minimapEnabled =
        yoko::GetClampedIntSetting(L"Appearance.minimap_enabled", 0, 1) != 0;
    settings.minimapSize = yoko::GetClampedIntSetting(
        L"Appearance.minimap_size", kMinimapMinSize, kMinimapMaxSize);

    settings.backgroundColor =
        ReadColorSetting(L"Appearance.background_color", 0x0B0E14);
    settings.gridColor = ReadColorSetting(L"Appearance.grid_color", 0x1B2436);
    settings.accentColor = ReadColorSetting(L"Appearance.accent_color", 0x38BDF8);
    settings.textColor = ReadColorSetting(L"Appearance.text_color", 0xE2E8F0);
    settings.mutedTextColor =
        ReadColorSetting(L"Appearance.muted_text_color", 0x64748B);
    settings.cardFillColor =
        ReadColorSetting(L"Appearance.card_fill_color", 0x111827);

    settings.stacksEnabled =
        yoko::GetClampedIntSetting(L"Canvas.card_stacks_enabled", 0, 1) != 0;
    settings.stackStepDegrees =
        static_cast<float>(yoko::GetClampedIntSetting(
            L"Canvas.card_stack_step_degrees", 0, 900)) / 10.0f;
    settings.stackMaxDegrees =
        static_cast<float>(yoko::GetClampedIntSetting(
            L"Canvas.card_stack_max_degrees", 0, 900)) / 10.0f;
    settings.stackScaleFactor =
        static_cast<float>(yoko::GetClampedIntSetting(
            L"Canvas.card_stack_scale_percent", 800, 1000)) / 1000.0f;
    settings.stackMaxDepth =
        yoko::GetClampedIntSetting(L"Canvas.card_stack_max_depth", 0, kMaxStackDepth);
    settings.stackRotationDirection = RotationDirectionFromText(
        yoko::GetStringSetting(L"Canvas.card_stack_rotation_direction",
                               L"counter_clockwise"));

    WindhawkUtils::StringSetting initialLayout =
        WindhawkUtils::StringSetting::make(L"Canvas.initial_layout");
    const wchar_t* layoutText = initialLayout.get();
    settings.initialLayout =
        layoutText != nullptr && _wcsicmp(layoutText, L"desktop") == 0
            ? InitialLayout::Desktop
            : InitialLayout::Packed;

    LoadExcludeList();

    context.camera.targetZoom = ClampZoom(context.camera.targetZoom);
    context.camera.zoom = ClampZoom(context.camera.zoom);
}

static std::wstring LayoutDirectory() {
    WCHAR path[MAX_PATH] = {};
    const size_t length = Wh_GetModStoragePath(path, ARRAYSIZE(path));
    if (length == 0) {
        return std::wstring();
    }
    return std::wstring(path, length);
}

static bool CurrentUserSid(std::wstring& sid) {
    HANDLE tokenHandle = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tokenHandle)) {
        return false;
    }
    yoko::win32::UniqueHandle token(tokenHandle);
    DWORD length = 0;
    const bool queryFailed = !GetTokenInformation(token.get(), TokenUser,
                                                  nullptr, 0, &length);
    if (queryFailed && GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        return false;
    }
    std::vector<BYTE> tokenData(length);
    if (!GetTokenInformation(token.get(), TokenUser, tokenData.data(), length,
                             &length)) {
        return false;
    }
    const auto* user = reinterpret_cast<const TOKEN_USER*>(tokenData.data());
    LPWSTR sidText = nullptr;
    if (!ConvertSidToStringSidW(user->User.Sid, &sidText)) {
        return false;
    }
    sid = sidText;
    LocalFree(sidText);
    return true;
}

static std::wstring LayoutFileName() {
    std::wstring sid;
    if (!CurrentUserSid(sid)) {
        Wh_Log(L"Aleph: cannot resolve the current user SID");
        return L"layout.json";
    }
    return L"layout-" + sid + L".json";
}

static std::wstring LayoutFilePath() {
    const std::wstring directory = LayoutDirectory();
    if (directory.empty()) {
        return std::wstring();
    }
    return directory + L"\\" + LayoutFileName();
}
