#include "metadata.h"

#if defined(__has_include)
#if __has_include(<windhawk_tool_mod.h>)
#include <windhawk_tool_mod.h>
#define ALEPH_OFFICIAL_TOOL_HOST 1
#endif
#endif

#ifndef ALEPH_OFFICIAL_TOOL_HOST
#define ALEPH_OFFICIAL_TOOL_HOST 0
#endif

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
// Windhawk tool mod process launcher boilerplate, patterned after cursor-tail.

#if !ALEPH_OFFICIAL_TOOL_HOST

void WINAPI EntryPoint_Hook() {
    Wh_Log(L">>");
    ExitThread(0);
}

BOOL Wh_ModInit() {
    DWORD sessionId = 0;
    if (ProcessIdToSessionId(GetCurrentProcessId(), &sessionId) && sessionId == 0) {
        return FALSE;
    }
    bool excluded = false;
    bool toolProcess = false;
    bool currentToolProcess = false;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == nullptr) {
        return FALSE;
    }
    for (int index = 1; index < argc; ++index) {
        if (wcsncmp(argv[index], L"-x-", 3) == 0) {
            excluded = true;
            break;
        }
        const WCHAR* const excludeFlags[] = {
            L"-service",
            L"-service-start",
            L"-service-stop",
            L"-run-ui",
            L"-run-ui-as-admin",
            L"-run-ui-in-safe-mode",
            L"-service-start-and-run-ui",
            L"-check-for-updates",
            L"-new-updates-found",
            L"-app-settings-changed",
            L"-exit",
            L"-restart",
            L"-restart-bg",
        };
        for (const WCHAR* const flag : excludeFlags) {
            if (wcscmp(argv[index], flag) == 0) {
                excluded = true;
                break;
            }
        }
        if (excluded) {
            break;
        }
    }
    for (int index = 1; index < argc - 1; ++index) {
        if (wcscmp(argv[index], L"-tool-mod") != 0) {
            continue;
        }
        toolProcess = true;
        currentToolProcess = wcscmp(argv[index + 1], WH_MOD_ID) == 0;
        break;
    }
    LocalFree(argv);
    if (excluded) {
        return FALSE;
    }
    if (currentToolProcess) {
        context.toolModProcessMutex =
            CreateMutexW(nullptr, TRUE, L"windhawk-tool-mod_" WH_MOD_ID);
        if (context.toolModProcessMutex == nullptr ||
            GetLastError() == ERROR_ALREADY_EXISTS) {
            if (context.toolModProcessMutex != nullptr) {
                CloseHandle(context.toolModProcessMutex);
                context.toolModProcessMutex = nullptr;
            }
            ExitProcess(1);
        }
        if (!WhTool_ModInit()) {
            ExitProcess(1);
        }
        void* entryPoint = nullptr;
        if (!yoko::win32::FindModuleEntryPoint(GetModuleHandleW(nullptr),
                                               &entryPoint) ||
            !Wh_SetFunctionHook(entryPoint,
                                reinterpret_cast<void*>(EntryPoint_Hook),
                                nullptr)) {
            WhTool_ModUninit();
            ExitProcess(1);
        }
        return TRUE;
    }
    if (toolProcess) {
        return FALSE;
    }
    context.isToolModProcessLauncher = true;
    return TRUE;
}

void Wh_ModAfterInit() {
    if (!context.isToolModProcessLauncher) {
        return;
    }
    WCHAR path[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, path, ARRAYSIZE(path));
    if (length == 0 || length >= ARRAYSIZE(path)) {
        return;
    }
    WCHAR commandLine[MAX_PATH + 96] = {};
    if (swprintf_s(commandLine, L"\"%s\" -tool-mod \"%s\"", path,
                   WH_MOD_ID) < 0) {
        return;
    }
    HMODULE kernel = GetModuleHandleW(L"kernelbase.dll");
    if (kernel == nullptr) {
        kernel = GetModuleHandleW(L"kernel32.dll");
    }
    if (kernel == nullptr) {
        return;
    }
    using CreateProcessInternalWFn = BOOL(WINAPI*)(
        HANDLE, LPCWSTR, LPWSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES,
        WINBOOL, DWORD, LPVOID, LPCWSTR, LPSTARTUPINFOW,
        LPPROCESS_INFORMATION, PHANDLE);
    const auto createProcess = reinterpret_cast<CreateProcessInternalWFn>(
        GetProcAddress(kernel, "CreateProcessInternalW"));
    if (createProcess == nullptr) {
        return;
    }
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_FORCEOFFFEEDBACK;
    PROCESS_INFORMATION process = {};
    if (!createProcess(nullptr, path, commandLine, nullptr, nullptr, FALSE,
                       NORMAL_PRIORITY_CLASS, nullptr, nullptr, &startup,
                       &process, nullptr)) {
        return;
    }
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);
}

void Wh_ModSettingsChanged() {
    if (!context.isToolModProcessLauncher) {
        WhTool_ModSettingsChanged();
    }
}

void Wh_ModUninit() {
    if (!context.isToolModProcessLauncher) {
        WhTool_ModUninit();
        if (context.toolModProcessMutex != nullptr) {
            CloseHandle(context.toolModProcessMutex);
            context.toolModProcessMutex = nullptr;
        }
        ExitProcess(0);
    }
}
#endif
