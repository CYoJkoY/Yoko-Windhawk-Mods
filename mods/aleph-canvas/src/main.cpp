#include "metadata.h"

#include "platform.h"
#include "constants.h"
#include "types.h"
#include "context.h"
#include "math_utils.h"
#include "text_utils.h"
#include "settings.h"
#include "layout_store.h"
#include "windows.h"
#include "cards.h"
#include "resources.h"
#include "minimap.h"
#include "camera_ops.h"
#include "thumbnails.h"
#include "render_host.h"
#include "render_hud.h"
#include "interaction.h"
#include "worker.h"

CanvasContext context;

void CloseWorkerEvents() {
    HANDLE* events[] = {&context.stopEvent, &context.settingsEvent,
                       &context.readyEvent};
    for (HANDLE* event : events) {
        if (*event != nullptr) {
            CloseHandle(*event);
            *event = nullptr;
        }
    }
}

BOOL WhTool_ModInit() {
    LoadSettings();
    context.stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    context.settingsEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    context.readyEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (context.stopEvent == nullptr || context.settingsEvent == nullptr ||
        context.readyEvent == nullptr) {
        CloseWorkerEvents();
        return FALSE;
    }
    context.workerThread = CreateThread(nullptr, 0, CanvasWorkerThread, nullptr,
                                        0, &context.workerThreadId);
    if (context.workerThread == nullptr) {
        CloseWorkerEvents();
        return FALSE;
    }
    const HANDLE startup[] = {context.readyEvent, context.workerThread};
    const DWORD result = WaitForMultipleObjects(2, startup, FALSE, 15000);
    if (result == WAIT_OBJECT_0 && context.startupSucceeded) {
        return TRUE;
    }
    SetEvent(context.stopEvent);
    WaitForSingleObject(context.workerThread, INFINITE);
    CloseHandle(context.workerThread);
    context.workerThread = nullptr;
    CloseWorkerEvents();
    return FALSE;
}

void WhTool_ModUninit() {
    if (context.stopEvent != nullptr) {
        SetEvent(context.stopEvent);
    }
    if (context.workerThread != nullptr) {
        WaitForSingleObject(context.workerThread, INFINITE);
        CloseHandle(context.workerThread);
        context.workerThread = nullptr;
    }
    CloseWorkerEvents();
}

void WhTool_ModSettingsChanged() {
    context.settingsDirty.store(true, std::memory_order_release);
    if (context.settingsEvent != nullptr) {
        SetEvent(context.settingsEvent);
    }
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
        if (!yoko::win32::FindModuleEntryPoint(GetModuleHandle(nullptr),
                                               &entryPoint)) {
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
