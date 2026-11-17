#pragma once

// Cross-version top-level window enumeration and filtering.

#include "platform.h"
#include "constants.h"
#include "context.h"
#include "text_utils.h"
#include "types.h"

static bool IsOwnProcessWindow(HWND window) {
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    return pid != 0 && pid == GetCurrentProcessId();
}

static std::wstring ReadWindowClass(HWND window) {
    WCHAR buffer[256] = {};
    const int length =
        GetClassNameW(window, buffer, static_cast<int>(ARRAYSIZE(buffer)));
    if (length <= 0) {
        return std::wstring();
    }
    return ToLowerText(std::wstring(buffer, static_cast<size_t>(length)));
}

static std::wstring ReadWindowText(HWND window) {
    const int length = GetWindowTextLengthW(window);
    if (length <= 0 || length > 1024) {
        return std::wstring();
    }
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    const int copied =
        GetWindowTextW(window, text.data(), static_cast<int>(text.size()));
    if (copied <= 0) {
        return std::wstring();
    }
    text.resize(static_cast<size_t>(copied));
    return text;
}

static std::wstring ReadProcessImagePath(DWORD pid) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (process == nullptr) {
        return std::wstring();
    }
    WCHAR buffer[MAX_PATH * 2] = {};
    DWORD size = static_cast<DWORD>(ARRAYSIZE(buffer));
    const BOOL success = QueryFullProcessImageNameW(process, 0, buffer, &size);
    CloseHandle(process);
    if (!success || size == 0) {
        return std::wstring();
    }
    return std::wstring(buffer, size);
}

static bool IsShellWindowClass(const std::wstring& windowClass) {
    static const wchar_t* const kShellClasses[] = {
        L"progman",
        L"workerw",
        L"shell_traywnd",
        L"shell_secondarytraywnd",
        L"traynotifywnd",
        L"windows.ui.core.corewindow",
        L"startmenuwindow",
        L"searchflyout",
        L"xamlexplorerhostislandwindow",
        L"windows.ui.composition.desktopwindowcontentbridge",
    };
    for (const wchar_t* name : kShellClasses) {
        if (windowClass == name) {
            return true;
        }
    }
    return false;
}

static bool ReadCloaked(HWND window, DWORD& cloaked) {
    cloaked = 0;
    const HRESULT result = DwmGetWindowAttribute(
        window, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
    return SUCCEEDED(result);
}

// DWMWA_EXTENDED_FRAME_BOUNDS excludes the invisible resize border, so cards
// line up with what the user sees instead of leaving a transparent gap.
static bool ReadFrameBounds(HWND window, RECT& bounds) {
    RECT extended = {};
    if (SUCCEEDED(DwmGetWindowAttribute(
            window, DWMWA_EXTENDED_FRAME_BOUNDS, &extended, sizeof(extended))) &&
        extended.right > extended.left && extended.bottom > extended.top) {
        bounds = extended;
        return true;
    }
    RECT fallback = {};
    if (!GetWindowRect(window, &fallback)) {
        return false;
    }
    bounds = fallback;
    return bounds.right > bounds.left && bounds.bottom > bounds.top;
}

static bool ReadRestoredBounds(HWND window, RECT& bounds) {
    WINDOWPLACEMENT placement = {};
    placement.length = sizeof(placement);
    if (!GetWindowPlacement(window, &placement)) {
        return false;
    }
    bounds = placement.rcNormalPosition;
    if (bounds.right <= bounds.left || bounds.bottom <= bounds.top) {
        return false;
    }
    if ((GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) != 0) {
        return true;
    }
    MONITORINFO monitor = {};
    monitor.cbSize = sizeof(monitor);
    if (!GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST),
                          &monitor)) {
        return false;
    }
    OffsetRect(&bounds, monitor.rcWork.left - monitor.rcMonitor.left,
               monitor.rcWork.top - monitor.rcMonitor.top);
    return true;
}

static bool IsOtherDesktop(HWND window, IVirtualDesktopManager* manager) {
    BOOL current = TRUE;
    return manager != nullptr &&
           SUCCEEDED(manager->IsWindowOnCurrentVirtualDesktop(window, &current)) &&
           current == FALSE;
}

static bool IsExcludedExecutable(const std::wstring& exeName) {
    for (const std::wstring& entry : context.settings.excludeList) {
        if (entry == exeName) {
            return true;
        }
    }
    return false;
}

// Returns false when the window must not appear on the canvas. A true result
// with CrossDesktop reports a window on another virtual desktop, which the
// caller may drop based on settings.
static bool ClassifyWindow(HWND window, IVirtualDesktopManager* desktopManager,
                           ScannedWindow& out) {
    if (window == nullptr || IsOwnProcessWindow(window)) {
        return false;
    }
    if (GetShellWindow() == window) {
        return false;
    }
    if (!IsWindowVisible(window)) {
        return false;
    }

    const LONG_PTR extendedStyle = GetWindowLongPtrW(window, GWL_EXSTYLE);
    const bool appWindow = (extendedStyle & WS_EX_APPWINDOW) != 0;
    if ((extendedStyle & WS_EX_TOOLWINDOW) != 0 && !appWindow) {
        return false;
    }
    if (GetWindow(window, GW_OWNER) != nullptr && !appWindow) {
        return false;
    }

    const std::wstring windowClass = ReadWindowClass(window);
    if (windowClass.empty() || IsShellWindowClass(windowClass)) {
        return false;
    }
    const std::wstring title = ReadWindowText(window);
    if (title.empty()) {
        return false;
    }

    DWORD cloaked = 0;
    if (!ReadCloaked(window, cloaked) ||
        (cloaked & ~static_cast<DWORD>(DWM_CLOAKED_SHELL)) != 0) {
        return false;
    }
    const bool crossDesktop = (cloaked & DWM_CLOAKED_SHELL) != 0;
    if (crossDesktop && !IsOtherDesktop(window, desktopManager)) {
        return false;
    }

    DWORD pid = 0;
    const DWORD threadId = GetWindowThreadProcessId(window, &pid);
    if (pid == 0 || pid == GetCurrentProcessId()) {
        return false;
    }
    const std::wstring exePath = ReadProcessImagePath(pid);
    const std::wstring exeName = ExecutableName(exePath);
    if (exeName.empty() || IsExcludedExecutable(exeName)) {
        return false;
    }

    const bool minimized = IsIconic(window) != FALSE;
    RECT frame = {};
    if (minimized) {
        if (!ReadRestoredBounds(window, frame)) {
            return false;
        }
    } else if (!ReadFrameBounds(window, frame)) {
        return false;
    }

    out.hwnd = window;
    out.processId = pid;
    out.threadId = threadId;
    out.windowClass = windowClass;
    out.state = crossDesktop ? WindowVisualState::OtherDesktop
                            : (minimized ? WindowVisualState::Minimized
                                         : WindowVisualState::Live);
    out.framePx = frame;
    out.title = title;
    out.exePath = exePath;
    out.fingerprint.clear();
    return true;
}

// Enumerates every eligible window. The virtual desktop manager is passed as
// a parameter so the callback never needs global COM state.
struct CollectPass {
    std::vector<ScannedWindow> windows;
    IVirtualDesktopManager* desktopManager = nullptr;

    static BOOL CALLBACK Thunk(HWND window, LPARAM parameter) {
        CollectPass* pass = reinterpret_cast<CollectPass*>(parameter);
        if (pass == nullptr || pass->windows.size() >=
                                  static_cast<size_t>(kMaxCards) * 4) {
            return TRUE;
        }
        ScannedWindow candidate;
        if (!ClassifyWindow(window, pass->desktopManager, candidate)) {
            return TRUE;
        }
        if (candidate.state == WindowVisualState::OtherDesktop &&
            !context.settings.includeOtherDesktops) {
            return TRUE;
        }
        pass->windows.push_back(std::move(candidate));
        return TRUE;
    }
};

static std::wstring ExistingFingerprint(const ScannedWindow& window) {
    for (const Card& card : context.cards) {
        if (MatchesWindow(card, window)) {
            return card.fingerprint;
        }
    }
    return {};
}

static bool FingerprintClaimed(const std::vector<ScannedWindow>& windows,
                               const std::wstring& fingerprint) {
    return std::any_of(windows.begin(), windows.end(),
                       [&fingerprint](const ScannedWindow& window) {
                           return window.fingerprint == fingerprint;
                       });
}

static void AssignFingerprints(std::vector<ScannedWindow>& windows) {
    for (ScannedWindow& window : windows) {
        window.fingerprint = ExistingFingerprint(window);
    }
    for (ScannedWindow& window : windows) {
        if (!window.fingerprint.empty()) {
            continue;
        }
        const std::wstring base =
            ExecutableName(window.exePath) + L"|" + window.windowClass + L"#";
        unsigned int ordinal = 0;
        while (FingerprintClaimed(windows, base + std::to_wstring(ordinal))) {
            ++ordinal;
        }
        window.fingerprint = base + std::to_wstring(ordinal);
    }
}

static std::vector<ScannedWindow> ScanWindows(IVirtualDesktopManager* manager) {
    CollectPass pass;
    pass.desktopManager = manager;
    EnumWindows(&CollectPass::Thunk, reinterpret_cast<LPARAM>(&pass));
    if (pass.windows.size() > static_cast<size_t>(kMaxCards)) {
        pass.windows.resize(static_cast<size_t>(kMaxCards));
    }
    AssignFingerprints(pass.windows);
    return std::move(pass.windows);
}

static const LayoutEntry* FindLayoutEntry(const std::wstring& fingerprint) {
    for (const LayoutEntry& entry : context.layout.entries) {
        if (entry.fingerprint == fingerprint) {
            return &entry;
        }
    }
    return nullptr;
}

static void RememberLayoutEntry(const std::wstring& fingerprint,
                                const RectF& world) {
    for (LayoutEntry& entry : context.layout.entries) {
        if (entry.fingerprint == fingerprint) {
            entry.world = world;
            return;
        }
    }
    if (context.layout.entries.size() >=
        static_cast<size_t>(kMaxLayoutEntries)) {
        return;
    }
    context.layout.entries.push_back(LayoutEntry{fingerprint, world});
}
