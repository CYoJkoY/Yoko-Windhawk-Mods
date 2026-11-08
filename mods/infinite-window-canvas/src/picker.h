#pragma once

#include "platform.h"
#include "context.h"
#include "windows.h"
#include "canvas.h"

static constexpr wchar_t kPickerClassName[] = L"InfiniteWindowCanvasPicker";
static constexpr int kPickerListId = 1001;
static constexpr int kPickerHintId = 1002;
static constexpr size_t kMaximumPickerRows = 40;

struct PickerSession {
    HWND window = nullptr;
    HWND list = nullptr;
    HWND previousForeground = nullptr;
    HWND chosen = nullptr;
    bool closed = false;
    std::vector<HWND> rows;
};

static PickerSession picker;

static bool IsChineseUi() {
    return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_CHINESE;
}

static const wchar_t* PickerTitle() {
    return IsChineseUi() ? L"快速跳转到窗口" : L"Quick jump to window";
}

static std::wstring PickerHint() {
    return IsChineseUi()
        ? L"双击或按 Enter 跳转，按 1-9 直接选择，按 Esc 取消。"
        : L"Double-click or press Enter to jump, 1-9 picks a row, Esc cancels.";
}

static std::wstring PickerEmptyHint() {
    return IsChineseUi()
        ? L"当前显示器上没有可管理的窗口，按 Esc 关闭。"
        : L"No managed windows on this monitor. Press Esc to close.";
}

static std::wstring WindowDisplayTitle(HWND hwnd) {
    WCHAR title[160] = {};
    if (GetWindowTextW(hwnd, title, ARRAYSIZE(title)) > 0) return title;
    WCHAR className[96] = {};
    if (GetClassNameW(hwnd, className, ARRAYSIZE(className)) > 0) return className;
    return IsChineseUi() ? L"(无标题窗口)" : L"(untitled window)";
}

static void FinishPicker(HWND chosen) {
    picker.chosen = chosen;
    picker.closed = true;
    if (picker.window) PostMessageW(picker.window, WM_CLOSE, 0, 0);
}

// Rows are addressed by their position in the picker, not by the listbox
// index, so a placeholder row can never resolve to an unrelated window.
static void ChoosePickerRow(int index) {
    if (index < 0 || static_cast<size_t>(index) >= picker.rows.size()) return;
    FinishPicker(picker.rows[static_cast<size_t>(index)]);
}

static WNDPROC pickerListBaseProc = nullptr;

static LRESULT CALLBACK PickerListProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_KEYDOWN) {
        if (wParam == VK_RETURN) {
            ChoosePickerRow(static_cast<int>(SendMessageW(hwnd, LB_GETCURSEL, 0, 0)));
            return 0;
        }
        if (wParam == VK_ESCAPE) {
            FinishPicker(nullptr);
            return 0;
        }
        if (wParam >= L'1' && wParam <= L'9') {
            ChoosePickerRow(static_cast<int>(wParam - L'1'));
            return 0;
        }
    }
    return CallWindowProcW(pickerListBaseProc, hwnd, message, wParam, lParam);
}

static LRESULT CALLBACK PickerWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_COMMAND:
            if (LOWORD(wParam) == kPickerListId && HIWORD(wParam) == LBN_DBLCLK) {
                ChoosePickerRow(static_cast<int>(SendMessageW(picker.list, LB_GETCURSEL, 0, 0)));
            }
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) FinishPicker(nullptr);
            return 0;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            picker.window = nullptr;
            picker.list = nullptr;
            // The window can also disappear without FinishPicker, for example
            // through Alt+F4. Ending the modal loop here keeps the canvas
            // thread from spinning on a window that no longer exists.
            picker.closed = true;
            return 0;
        case WM_ERASEBKGND: {
            RECT rect = {};
            GetClientRect(hwnd, &rect);
            FillRect(reinterpret_cast<HDC>(wParam), &rect,
                     reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
            return 1;
        }
        case WM_CTLCOLORSTATIC:
            SetBkColor(reinterpret_cast<HDC>(wParam), GetSysColor(COLOR_WINDOW));
            return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

static UINT PickerDpi() {
    UINT dpiX = 96;
    UINT dpiY = 96;
    if (context.monitor &&
        GetDpiForMonitor(context.monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY) == S_OK &&
        dpiX >= 96) {
        return dpiX;
    }
    return 96;
}

static void FillPickerRows(HWND list, HWND hint) {
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    picker.rows.clear();
    for (const ManagedWindow& item : context.windows) {
        if (picker.rows.size() >= kMaximumPickerRows) break;
        std::wstring title = WindowDisplayTitle(item.hwnd);
        if (title.size() > 96) title = title.substr(0, 93) + L"...";

        std::wstring row;
        if (picker.rows.size() < 9) {
            row = std::to_wstring(picker.rows.size() + 1) + L".  ";
        }
        row += title;
        SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(row.c_str()));
        picker.rows.push_back(item.hwnd);
    }
    SetWindowTextW(hint, picker.rows.empty() ? PickerEmptyHint().c_str() : PickerHint().c_str());
    if (!picker.rows.empty()) SendMessageW(list, LB_SETCURSEL, 0, 0);
}

// Shows the picker on the monitor under the pointer and blocks the canvas
// thread until the user chooses a window, cancels, or unloads the mod. The
// picker itself uses WS_EX_TOOLWINDOW, so it never enters the managed set.
static void OpenWindowPicker() {
    if (context.pickerOpen) return;

    POINT cursor = {};
    if (!GetCursorPos(&cursor)) return;
    const HMONITOR cursorMonitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    if (!context.layoutReady || cursorMonitor != context.monitor) {
        StartLayout(cursor);
    } else {
        ReconcileWindows();
        context.lastReconcile = GetTickCount();
    }
    context.edgeActive = false;
    context.panning = false;

    WNDCLASSW windowClass = {};
    windowClass.lpfnWndProc = PickerWndProc;
    windowClass.hInstance = GetModuleHandle(nullptr);
    windowClass.lpszClassName = kPickerClassName;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    if (!RegisterClassW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        yoko::LogLastError(L"RegisterClassW picker");
        return;
    }

    const float scale = static_cast<float>(PickerDpi()) / 96.0f;
    const int width = static_cast<int>(620 * scale);
    const int height = static_cast<int>(440 * scale);
    const int margin = static_cast<int>(16 * scale);
    const int hintHeight = static_cast<int>(46 * scale);
    const int listHeight = height - margin * 2 - hintHeight;
    const int x = context.monitorRect.left
        + ((context.monitorRect.right - context.monitorRect.left) - width) / 2;
    const int y = context.monitorRect.top
        + ((context.monitorRect.bottom - context.monitorRect.top) - height) / 2;

    picker.previousForeground = GetForegroundWindow();
    picker.chosen = nullptr;
    picker.closed = false;
    picker.rows.clear();
    picker.window = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST, kPickerClassName, PickerTitle(),
        WS_POPUP | WS_CAPTION | WS_SYSMENU, x, y, width, height,
        nullptr, nullptr, windowClass.hInstance, nullptr);
    if (!picker.window) {
        yoko::LogLastError(L"CreateWindowExW picker");
        return;
    }

    picker.list = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"LISTBOX", nullptr,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
        margin, margin, width - margin * 2, listHeight,
        picker.window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPickerListId)),
        windowClass.hInstance, nullptr);
    HWND hint = CreateWindowExW(
        0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
        margin, margin + listHeight + static_cast<int>(6 * scale),
        width - margin * 2, hintHeight,
        picker.window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPickerHintId)),
        windowClass.hInstance, nullptr);
    if (!picker.list || !hint) {
        yoko::LogLastError(L"CreateWindowExW picker children");
        DestroyWindow(picker.window);
        picker.window = nullptr;
        picker.list = nullptr;
        return;
    }

    HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    SendMessageW(picker.list, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    SendMessageW(hint, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

    pickerListBaseProc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(picker.list, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(PickerListProc)));
    if (!pickerListBaseProc) {
        yoko::LogLastError(L"SetWindowLongPtrW picker list");
        DestroyWindow(picker.window);
        picker.window = nullptr;
        picker.list = nullptr;
        return;
    }

    FillPickerRows(picker.list, hint);

    context.pickerOpen = true;
    ShowWindow(picker.window, SW_SHOW);
    SetForegroundWindow(picker.window);
    SetFocus(picker.list);

    MSG message = {};
    while (!picker.closed) {
        const DWORD result = MsgWaitForMultipleObjectsEx(
            1, &context.stopEvent, 100, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        if (result == WAIT_OBJECT_0) break;
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                picker.closed = true;
                break;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    if (picker.window) DestroyWindow(picker.window);
    picker.window = nullptr;
    picker.list = nullptr;
    pickerListBaseProc = nullptr;
    context.pickerOpen = false;
    // The modal loop swallowed timer messages for an arbitrary amount of time;
    // restart the frame clock so the first tick after closing cannot jump.
    context.lastTick = GetTickCount();

    if (WaitForSingleObject(context.stopEvent, 0) == WAIT_OBJECT_0) return;

    if (picker.chosen && IsWindow(picker.chosen)) {
        JumpToWindow(picker.chosen);
        // Raising and focusing the chosen window is the whole point of the
        // picker; automatic panning still never changes z-order or focus.
        SetWindowPos(picker.chosen, HWND_TOP, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
        SetForegroundWindow(picker.chosen);
    } else if (picker.previousForeground && IsWindow(picker.previousForeground)) {
        SetForegroundWindow(picker.previousForeground);
    }
}
