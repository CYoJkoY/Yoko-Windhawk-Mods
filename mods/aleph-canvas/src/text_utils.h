#pragma once

// Small string helpers shared by settings, scanning and rendering.

#include "platform.h"

static std::wstring TrimText(const std::wstring& value) {
    const size_t first = value.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos) {
        return std::wstring();
    }
    const size_t last = value.find_last_not_of(L" \t\r\n");
    return value.substr(first, last - first + 1);
}

static std::wstring ToLowerText(const std::wstring& value) {
    std::wstring result = value;
    for (wchar_t& item : result) {
        item = static_cast<wchar_t>(towlower(item));
    }
    return result;
}

static std::wstring ExecutableName(const std::wstring& path) {
    const size_t slash = path.find_last_of(L"\\/");
    return ToLowerText(slash == std::wstring::npos ? path
                                                   : path.substr(slash + 1));
}

static std::string ToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return std::string();
    }
    const int required = WideCharToMultiByte(
        CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0,
        nullptr, nullptr);
    if (required <= 0) {
        return std::string();
    }
    std::string result(static_cast<size_t>(required), '\0');
    if (WideCharToMultiByte(CP_UTF8, 0, value.c_str(),
                            static_cast<int>(value.size()), result.data(),
                            required, nullptr, nullptr) != required) {
        return std::string();
    }
    return result;
}

static std::wstring FromUtf8(const std::string& value) {
    if (value.empty()) {
        return std::wstring();
    }
    const int required = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.c_str(),
        static_cast<int>(value.size()), nullptr, 0);
    if (required <= 0) {
        return std::wstring();
    }
    std::wstring result(static_cast<size_t>(required), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.c_str(),
                            static_cast<int>(value.size()), result.data(),
                            required) != required) {
        return std::wstring();
    }
    return result;
}

static std::wstring FormatInteger(int value) {
    wchar_t buffer[32] = {};
    swprintf_s(buffer, L"%d", value);
    return std::wstring(buffer);
}

// Maps the named rotation direction option to the multiplier used by the stack
// geometry: 1 clockwise, 0 no rotation, -1 counter-clockwise. The legacy
// numeric spellings stay accepted so a stored layout or an old settings file
// keeps its direction; anything else falls back to counter-clockwise.
static int RotationDirectionFromText(const std::wstring& value) {
    const std::wstring text = ToLowerText(TrimText(value));
    if (text == L"clockwise" || text == L"1") {
        return 1;
    }
    if (text == L"none" || text == L"0") {
        return 0;
    }
    return -1;
}
