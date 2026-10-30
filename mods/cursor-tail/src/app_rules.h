#pragma once

// Per-application enable and disable rules.

#include "platform.h"
#include "context.h"
#include "math_utils.h"

static bool GetProcessExeName(DWORD pid, std::wstring& out) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return false;
    WCHAR path[512] = {};
    DWORD size = ARRAYSIZE(path);
    const BOOL success = QueryFullProcessImageNameW(process, 0, path, &size);
    CloseHandle(process);
    if (!success) return false;
    const wchar_t* name = wcsrchr(path, L'\\');
    out = ToLowerW(name ? name + 1 : path);
    return true;
}

static void ParseAppRuleEntry(const wchar_t* text) {
    if (!text) return;

    std::wstring ruleText(text);
    const size_t comment = ruleText.find(L'#');
    if (comment != std::wstring::npos) ruleText.resize(comment);
    const size_t equals = ruleText.find(L'=');
    if (equals == std::wstring::npos) return;

    std::wstring exe = ruleText.substr(0, equals);
    std::wstring value = ruleText.substr(equals + 1);
    const auto trim = [](std::wstring& item) {
        const size_t first = item.find_first_not_of(L" \t");
        if (first == std::wstring::npos) {
            item.clear();
            return;
        }
        const size_t last = item.find_last_not_of(L" \t");
        item = item.substr(first, last - first + 1);
    };
    trim(exe);
    trim(value);

    const bool enabled = _wcsicmp(value.c_str(), L"on") == 0
        || value == L"1"
        || _wcsicmp(value.c_str(), L"true") == 0
        || _wcsicmp(value.c_str(), L"yes") == 0;
    const bool disabled = _wcsicmp(value.c_str(), L"off") == 0
        || value == L"0"
        || _wcsicmp(value.c_str(), L"false") == 0
        || _wcsicmp(value.c_str(), L"no") == 0;
    if (!exe.empty() && (enabled || disabled)) {
        context.appRules.push_back({ToLowerW(exe), enabled});
    }
}

static void ParseAppRules() {
    context.appRules.clear();

    constexpr int kMaxAppRuleEntries = 64;
    int consecutiveEmptyEntries = 0;
    for (int index = 0; index < kMaxAppRuleEntries; ++index) {
        WindhawkUtils::StringSetting setting =
            WindhawkUtils::StringSetting::make(L"Application.app_rules[%d]", index);
        const wchar_t* value = setting.get();
        if (!value || !*value) {
            if (++consecutiveEmptyEntries >= 2) break;
            continue;
        }

        consecutiveEmptyEntries = 0;
        ParseAppRuleEntry(value);
    }
}

static int CheckAppRuleCached(HWND foreground) {
    if (foreground == context.cachedForegroundWindow) return context.cachedAppRule;
    context.cachedForegroundWindow = foreground;
    context.cachedAppRule = 0;
    if (!foreground || context.appRules.empty()) return 0;
    DWORD pid = 0;
    GetWindowThreadProcessId(foreground, &pid);
    if (!pid) return 0;
    std::wstring exe;
    if (!GetProcessExeName(pid, exe)) return 0;
    for (const auto& rule : context.appRules) {
        if (wcscmp(rule.exe.c_str(), exe.c_str()) == 0) {
            context.cachedAppRule = rule.enabled ? 1 : -1;
            break;
        }
    }
    return context.cachedAppRule;
}
