#include "metadata.h"

// Modular sources are amalgamated by tools/bundle.py. The include order is
// the definition order of the single generated translation unit.
#include "platform.h"
#include "constants.h"
#include "types.h"
#include "context.h"
#include "forward.h"
#include "math_utils.h"
#include "overlay.h"
#include "history.h"
#include "render.h"
#include "backbuffer.h"
#include "trail.h"
#include "interaction.h"
#include "input.h"
#include "dpi.h"
#include "fullscreen.h"
#include "app_rules.h"
#include "ornament_settings.h"
#include "settings.h"
#include "color.h"
#include "ornaments.h"
#include "overlay_render.h"
#include "frame.h"
#include "hooks/overlay_window.h"
#include "overlay_thread.h"

BOOL WhTool_ModInit() {
    LoadSettings();
    context.stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!context.stopEvent) return FALSE;
    context.inputWakeEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!context.inputWakeEvent) {
        CloseHandle(context.stopEvent);
        context.stopEvent = nullptr;
        return FALSE;
    }
    context.threadHandle = CreateThread(nullptr, 0, OverlayThreadProc, nullptr, 0, nullptr);
    if (!context.threadHandle) {
        CloseHandle(context.inputWakeEvent);
        context.inputWakeEvent = nullptr;
        CloseHandle(context.stopEvent);
        context.stopEvent = nullptr;
        return FALSE;
    }
    return TRUE;
}

void WhTool_ModUninit() {
    if (context.stopEvent) SetEvent(context.stopEvent);
    if (context.threadHandle) {
        WaitForSingleObject(context.threadHandle, INFINITE);
        CloseHandle(context.threadHandle);
        context.threadHandle = nullptr;
    }
    if (context.inputWakeEvent) {
        CloseHandle(context.inputWakeEvent);
        context.inputWakeEvent = nullptr;
    }
    if (context.stopEvent) {
        CloseHandle(context.stopEvent);
        context.stopEvent = nullptr;
    }
}

void WhTool_ModSettingsChanged() {
    context.settingsDirty.store(true, std::memory_order_release);
    HWND hwnd = context.overlayHwnd.load(std::memory_order_acquire);
    if (yoko::win32::IsValidWindow(hwnd)) PostMessageW(hwnd, kSettingsChangedMessage, 0, 0);
}
////////////////////////////////////////////////////////////////////////////////
// Windhawk tool mod implementation for mods which don't need to inject to other
// processes or hook other functions. Context:
// https://github.com/ramensoftware/windhawk/wiki/Mods-as-tools:-Running-mods-in-a-dedicated-process
//
// The mod will load and run in a dedicated windhawk.exe process.
//
// Paste the code below as part of the mod code, and use these callbacks:
// * WhTool_ModInit
// * WhTool_ModSettingsChanged
// * WhTool_ModUninit
//
// Currently, other callbacks are not supported.

void WINAPI EntryPoint_Hook() {
    Wh_Log(L">");
    ExitThread(0);
}

BOOL Wh_ModInit() {
    DWORD sessionId;
    if (ProcessIdToSessionId(GetCurrentProcessId(), &sessionId) &&
        sessionId == 0) {
        return FALSE;
    }

    bool isExcluded = false;
    bool isToolModProcess = false;
    bool isCurrentToolModProcess = false;
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLine(), &argc);
    if (!argv) {
        Wh_Log(L"CommandLineToArgvW failed");
        return FALSE;
    }

    for (int i = 1; i < argc; i++) {
        if (wcscmp(argv[i], L"-service") == 0 ||
            wcscmp(argv[i], L"-service-start") == 0 ||
            wcscmp(argv[i], L"-service-stop") == 0) {
            isExcluded = true;
            break;
        }
    }

    for (int i = 1; i < argc - 1; i++) {
        if (wcscmp(argv[i], L"-tool-mod") == 0) {
            isToolModProcess = true;
            if (wcscmp(argv[i + 1], WH_MOD_ID) == 0) {
                isCurrentToolModProcess = true;
            }
            break;
        }
    }

    LocalFree(argv);

    if (isExcluded) {
        return FALSE;
    }

    if (isCurrentToolModProcess) {
        context.toolModProcessMutex =
            CreateMutex(nullptr, TRUE, L"windhawk-tool-mod_" WH_MOD_ID);
        if (!context.toolModProcessMutex) {
            Wh_Log(L"CreateMutex failed");
            ExitProcess(1);
        }

        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            Wh_Log(L"Tool mod already running (%s)", WH_MOD_ID);
            CloseHandle(context.toolModProcessMutex);
            context.toolModProcessMutex = nullptr;
            ExitProcess(1);
        }

        if (!WhTool_ModInit()) {
            ExitProcess(1);
        }

        void* entryPoint = nullptr;
        if (!yoko::win32::FindModuleEntryPoint(GetModuleHandle(nullptr), &entryPoint)) {
            Wh_Log(L"Unable to resolve the process entry point");
            WhTool_ModUninit();
            ExitProcess(1);
        }

        if (!Wh_SetFunctionHook(entryPoint, (void*)EntryPoint_Hook, nullptr)) {
            Wh_Log(L"Wh_SetFunctionHook failed for the process entry point");
            WhTool_ModUninit();
            ExitProcess(1);
        }
        return TRUE;
    }

    if (isToolModProcess) {
        return FALSE;
    }

    context.isToolModProcessLauncher = true;
    return TRUE;
}

void Wh_ModAfterInit() {
    if (!context.isToolModProcessLauncher) {
        return;
    }

    WCHAR currentProcessPath[MAX_PATH];
    switch (GetModuleFileName(nullptr, currentProcessPath,
                              ARRAYSIZE(currentProcessPath))) {
        case 0:
        case ARRAYSIZE(currentProcessPath):
            Wh_Log(L"GetModuleFileName failed");
            return;
    }

    WCHAR
    commandLine[MAX_PATH + 2 +
                (sizeof(L" -tool-mod \"" WH_MOD_ID "\"") / sizeof(WCHAR)) - 1];
    swprintf_s(commandLine, L"\"%s\" -tool-mod \"%s\"", currentProcessPath,
               WH_MOD_ID);

    HMODULE kernelModule = GetModuleHandle(L"kernelbase.dll");
    if (!kernelModule) {
        kernelModule = GetModuleHandle(L"kernel32.dll");
        if (!kernelModule) {
            Wh_Log(L"No kernelbase.dll/kernel32.dll");
            return;
        }
    }

    using CreateProcessInternalW_t = BOOL(WINAPI*)(
        HANDLE hUserToken, LPCWSTR lpApplicationName, LPWSTR lpCommandLine,
        LPSECURITY_ATTRIBUTES lpProcessAttributes,
        LPSECURITY_ATTRIBUTES lpThreadAttributes, WINBOOL bInheritHandles,
        DWORD dwCreationFlags, LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory,
        LPSTARTUPINFOW lpStartupInfo,
        LPPROCESS_INFORMATION lpProcessInformation,
        PHANDLE hRestrictedUserToken);
    CreateProcessInternalW_t pCreateProcessInternalW =
        (CreateProcessInternalW_t)GetProcAddress(kernelModule,
                                                 "CreateProcessInternalW");
    if (!pCreateProcessInternalW) {
        Wh_Log(L"No CreateProcessInternalW");
        return;
    }

    STARTUPINFO si = {};
    si.cb = sizeof(STARTUPINFO);
    si.dwFlags = STARTF_FORCEOFFFEEDBACK;
    PROCESS_INFORMATION pi = {};
    if (!pCreateProcessInternalW(nullptr, currentProcessPath, commandLine,
                                 nullptr, nullptr, FALSE, NORMAL_PRIORITY_CLASS,
                                 nullptr, nullptr, &si, &pi, nullptr)) {
        Wh_Log(L"CreateProcess failed");
        return;
    }

    yoko::win32::UniqueHandle processHandle(pi.hProcess);
    yoko::win32::UniqueHandle threadHandle(pi.hThread);
}

void Wh_ModSettingsChanged() {
    if (context.isToolModProcessLauncher) {
        return;
    }

    WhTool_ModSettingsChanged();
}

void Wh_ModUninit() {
    if (context.isToolModProcessLauncher) {
        return;
    }

    WhTool_ModUninit();
    ExitProcess(0);
}
