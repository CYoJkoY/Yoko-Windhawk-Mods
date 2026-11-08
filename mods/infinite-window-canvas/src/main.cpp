#include "metadata.h"

#include "platform.h"
#include "context.h"
#include "settings.h"
#include "windows.h"
#include "canvas.h"
#include "control.h"

BOOL WhTool_ModInit() {
    LoadSettings();
    context.stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!context.stopEvent) return FALSE;
    context.threadHandle = CreateThread(nullptr, 0, CanvasThreadProc, nullptr, 0, nullptr);
    if (!context.threadHandle) {
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
    if (context.stopEvent) {
        CloseHandle(context.stopEvent);
        context.stopEvent = nullptr;
    }
}

void WhTool_ModSettingsChanged() {
    context.settingsDirty.store(true, std::memory_order_release);
}

void WINAPI EntryPoint_Hook() {
    ExitThread(0);
}

BOOL Wh_ModInit() {
    DWORD sessionId = 0;
    if (ProcessIdToSessionId(GetCurrentProcessId(), &sessionId) && sessionId == 0) return FALSE;

    bool isExcluded = false;
    bool isToolModProcess = false;
    bool isCurrentToolModProcess = false;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return FALSE;
    for (int index = 1; index < argc; ++index) {
        if (wcscmp(argv[index], L"-service") == 0 ||
            wcscmp(argv[index], L"-service-start") == 0 ||
            wcscmp(argv[index], L"-service-stop") == 0) {
            isExcluded = true;
            break;
        }
    }
    for (int index = 1; index < argc - 1; ++index) {
        if (wcscmp(argv[index], L"-tool-mod") == 0) {
            isToolModProcess = true;
            isCurrentToolModProcess = wcscmp(argv[index + 1], WH_MOD_ID) == 0;
            break;
        }
    }
    LocalFree(argv);
    if (isExcluded) return FALSE;
    if (isCurrentToolModProcess) {
        HANDLE mutex = CreateMutexW(nullptr, TRUE, L"windhawk-tool-mod_" WH_MOD_ID);
        if (!mutex || GetLastError() == ERROR_ALREADY_EXISTS) {
            if (mutex) CloseHandle(mutex);
            ExitProcess(1);
        }
        context.toolMutex = mutex;
        if (!WhTool_ModInit()) ExitProcess(1);
        void* entryPoint = nullptr;
        if (!yoko::win32::FindModuleEntryPoint(GetModuleHandle(nullptr), &entryPoint) ||
            !Wh_SetFunctionHook(entryPoint, reinterpret_cast<void*>(EntryPoint_Hook), nullptr)) {
            WhTool_ModUninit();
            ExitProcess(1);
        }
        return TRUE;
    }
    if (isToolModProcess) return FALSE;
    context.isLauncher = true;
    return TRUE;
}

void Wh_ModAfterInit() {
    if (!context.isLauncher) return;

    WCHAR path[MAX_PATH] = {};
    if (!GetModuleFileNameW(nullptr, path, ARRAYSIZE(path))) {
        Wh_Log(L"GetModuleFileName failed");
        return;
    }

    WCHAR commandLine[MAX_PATH + 128] = {};
    swprintf_s(commandLine, L"\"%s\" -tool-mod \"%s\"", path, WH_MOD_ID);

    HMODULE kernelModule = GetModuleHandleW(L"kernelbase.dll");
    if (!kernelModule) kernelModule = GetModuleHandleW(L"kernel32.dll");
    if (!kernelModule) {
        Wh_Log(L"No kernelbase.dll/kernel32.dll");
        return;
    }

    // CreateProcess is not usable from the injected launcher context, so the
    // tool-mod process is started through the internal entry point instead.
    using CreateProcessInternalW_t = BOOL(WINAPI*)(
        HANDLE hUserToken, LPCWSTR lpApplicationName, LPWSTR lpCommandLine,
        LPSECURITY_ATTRIBUTES lpProcessAttributes,
        LPSECURITY_ATTRIBUTES lpThreadAttributes, WINBOOL bInheritHandles,
        DWORD dwCreationFlags, LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory,
        LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation,
        PHANDLE hRestrictedUserToken);
    const auto createProcessInternal = reinterpret_cast<CreateProcessInternalW_t>(
        GetProcAddress(kernelModule, "CreateProcessInternalW"));
    if (!createProcessInternal) {
        Wh_Log(L"No CreateProcessInternalW");
        return;
    }

    STARTUPINFOW startup = {sizeof(startup)};
    startup.dwFlags = STARTF_FORCEOFFFEEDBACK;
    PROCESS_INFORMATION process = {};
    if (!createProcessInternal(nullptr, path, commandLine, nullptr, nullptr, FALSE,
                               NORMAL_PRIORITY_CLASS, nullptr, nullptr, &startup,
                               &process, nullptr)) {
        yoko::LogLastError(L"CreateProcessInternalW");
        return;
    }
    yoko::win32::UniqueHandle processHandle(process.hProcess);
    yoko::win32::UniqueHandle threadHandle(process.hThread);
}

void Wh_ModSettingsChanged() {
    if (!context.isLauncher) WhTool_ModSettingsChanged();
}

void Wh_ModUninit() {
    if (context.isLauncher) return;
    WhTool_ModUninit();
    if (context.toolMutex) {
        CloseHandle(context.toolMutex);
        context.toolMutex = nullptr;
    }
    ExitProcess(0);
}
