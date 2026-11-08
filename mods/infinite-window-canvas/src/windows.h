#pragma once

#include "platform.h"
#include "context.h"

static std::wstring GetWindowExeName(HWND hwnd) {
    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (!processId) return {};
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (!process) return {};
    WCHAR path[MAX_PATH] = {};
    DWORD size = ARRAYSIZE(path);
    const BOOL success = QueryFullProcessImageNameW(process, 0, path, &size);
    CloseHandle(process);
    if (!success) return {};
    const wchar_t* name = wcsrchr(path, L'\\');
    std::wstring result = name ? name + 1 : path;
    for (wchar_t& character : result) character = static_cast<wchar_t>(towlower(character));
    return result;
}

static bool IsBlacklisted(const std::wstring& exeName) {
    return std::find(context.processBlacklist.begin(), context.processBlacklist.end(), exeName)
        != context.processBlacklist.end();
}

static bool IsShellWindow(HWND hwnd) {
    WCHAR className[128] = {};
    if (!GetClassNameW(hwnd, className, ARRAYSIZE(className))) return true;
    return _wcsicmp(className, L"Progman") == 0
        || _wcsicmp(className, L"WorkerW") == 0
        || _wcsicmp(className, L"Shell_TrayWnd") == 0
        || _wcsicmp(className, L"Shell_SecondaryTrayWnd") == 0
        || _wcsicmp(className, L"XamlExplorerHostIslandWindow") == 0;
}

static bool CoversMonitor(HWND hwnd) {
    RECT rect = {};
    if (!GetWindowRect(hwnd, &rect)) return true;
    return rect.left <= context.monitorRect.left
        && rect.top <= context.monitorRect.top
        && rect.right >= context.monitorRect.right
        && rect.bottom >= context.monitorRect.bottom;
}

// Windows are grouped by the monitor that holds most of their area. Only the
// monitor under the pointer takes part in a canvas layout, so anything that
// belongs to a neighbor monitor is left alone.
static bool IsOnCurrentMonitor(HWND hwnd) {
    return MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST) == context.monitor;
}

// A window that the canvas itself panned onto a neighbor display still belongs
// to this layout; only its origin proves where the user parked it.
static bool IsOriginOnCurrentMonitor(const RECT& origin) {
    return MonitorFromRect(&origin, MONITOR_DEFAULTTONEAREST) == context.monitor;
}

static bool IsEligibleWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd) || !IsWindowVisible(hwnd)) return false;
    if (IsIconic(hwnd) || IsZoomed(hwnd)) return false;
    if (hwnd == GetDesktopWindow() || IsShellWindow(hwnd)) return false;
    if (GetWindow(hwnd, GW_OWNER) != nullptr) return false;

    const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    const LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if ((exStyle & WS_EX_TOOLWINDOW) != 0 || (exStyle & WS_EX_NOACTIVATE) != 0) return false;
    if ((style & WS_CHILD) != 0 || (style & WS_POPUP) != 0) return false;
    if (context.processBlacklist.empty()) return true;
    return !IsBlacklisted(GetWindowExeName(hwnd));
}

static bool IsManagedCandidate(HWND hwnd) {
    return IsEligibleWindow(hwnd) && IsOnCurrentMonitor(hwnd) && !CoversMonitor(hwnd);
}

// True while the window still sits exactly where the canvas last put it, with
// the size it had when the layout was captured.
static bool IsAtPlacedPosition(const ManagedWindow& item) {
    RECT actual = {};
    if (!GetWindowRect(item.hwnd, &actual)) return false;
    return actual.left == item.origin.left + context.canvasOffset.x
        && actual.top == item.origin.top + context.canvasOffset.y
        && actual.right - actual.left == item.origin.right - item.origin.left
        && actual.bottom - actual.top == item.origin.bottom - item.origin.top;
}

// A layout member is kept until it stops being an ordinary window, is moved to
// another monitor by the user, or turns into a fullscreen window. The last case
// is only reported when the window also left the position the canvas gave it,
// so a large window that merely panned over the screen edge is not dropped.
static bool IsStillManaged(const ManagedWindow& item) {
    if (!IsEligibleWindow(item.hwnd)) return false;
    if (!IsOriginOnCurrentMonitor(item.origin)) return false;
    return !CoversMonitor(item.hwnd) || IsAtPlacedPosition(item);
}

static bool FindManagedWindow(HWND hwnd, size_t& index) {
    for (size_t i = 0; i < context.windows.size(); ++i) {
        if (context.windows[i].hwnd == hwnd) {
            index = i;
            return true;
        }
    }
    return false;
}

static BOOL CALLBACK CollectWindows(HWND hwnd, LPARAM) {
    if (!IsManagedCandidate(hwnd)) return TRUE;
    RECT rect = {};
    if (!GetWindowRect(hwnd, &rect)) return TRUE;
    size_t index = 0;
    if (FindManagedWindow(hwnd, index)) return TRUE;
    rect.left -= context.canvasOffset.x;
    rect.right -= context.canvasOffset.x;
    rect.top -= context.canvasOffset.y;
    rect.bottom -= context.canvasOffset.y;
    context.windows.push_back({hwnd, rect});
    return TRUE;
}

static void GetCurrentMonitor(POINT point) {
    context.monitor = MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {sizeof(info)};
    if (!GetMonitorInfoW(context.monitor, &info)) {
        yoko::LogLastError(L"GetMonitorInfoW");
        context.monitorRect = {};
        return;
    }
    context.monitorRect = info.rcMonitor;
}

// Adds newly eligible windows and forgets windows that stopped qualifying.
// A forgotten window keeps the position it currently occupies: minimizing or
// hiding a window must not teleport it back to the layout origin.
static void ReconcileWindows() {
    EnumWindows(CollectWindows, 0);
    for (size_t index = context.windows.size(); index > 0; --index) {
        const ManagedWindow& item = context.windows[index - 1];
        if (!IsStillManaged(item)) context.windows.erase(context.windows.begin() + index - 1);
    }
}
