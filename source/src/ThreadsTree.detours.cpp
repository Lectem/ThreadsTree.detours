#include <Windows.h>
#include <detours.h>
#include <fmt/format.h>
#include <shlwapi.h>
#include <thread>

#define LOG_PREFIX "(ThreadsTree.detours.dll):"
#include "Log.h"

#include <DbgHelp.h>
#include <mutex>
std::mutex dbgHelpMutex;
std::mutex logMutex;

static FILE* dumpFile = nullptr;

static void DumpStackTrace()
{
    std::lock_guard<std::mutex> lockDbgHelp{dbgHelpMutex};
    ::SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_INCLUDE_32BIT_MODULES | SYMOPT_UNDNAME);
    if (!::SymInitialize(::GetCurrentProcess(), nullptr, TRUE)) return;

    PVOID  addrs[256] = {0};
    USHORT frames     = RtlCaptureStackBackTrace(2, std::size(addrs), addrs, nullptr);

    for (USHORT i = 0; i < frames; i++)
    {
        const size_t maxNameSize = 2048;
        SYMBOL_INFO* info        = (SYMBOL_INFO*)alloca(sizeof(SYMBOL_INFO) + (maxNameSize - 1) * sizeof(TCHAR));
        info->SizeOfStruct       = sizeof(SYMBOL_INFO);
        info->MaxNameLen         = maxNameSize;

        // Attempt to get information about the symbol and add it to our output parameter.
        DWORD64 displacement = 0;
        if (::SymFromAddr(::GetCurrentProcess(), (DWORD64)addrs[i], &displacement, info))
        {
            fwrite(info->Name, sizeof(TCHAR), info->NameLen, dumpFile);
            fputs("\\n", dumpFile);
        }
        else
            fprintf(dumpFile,"0x%p\\n", addrs[i]);
    }
    fflush(dumpFile);
    ::SymCleanup(::GetCurrentProcess());

    return;
}

using CreateRemoteThreadExFuncPtr                    = decltype(&CreateRemoteThreadEx);
CreateRemoteThreadExFuncPtr RealCreateRemoteThreadEx = CreateRemoteThreadEx;

HANDLE WINAPI DetouredCreateRemoteThreadEx(_In_ HANDLE hProcess, _In_opt_ LPSECURITY_ATTRIBUTES lpThreadAttributes,
                                           _In_ SIZE_T dwStackSize, _In_ LPTHREAD_START_ROUTINE lpStartAddress,
                                           _In_opt_ LPVOID lpParameter, _In_ DWORD dwCreationFlags,
                                           _In_opt_ LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList,
                                           _Out_opt_ LPDWORD lpThreadId)
{
    const HANDLE newThreadHandle = RealCreateRemoteThreadEx(hProcess, lpThreadAttributes, dwStackSize, lpStartAddress,
                                                            lpParameter, dwCreationFlags, lpAttributeList, lpThreadId);
    if (newThreadHandle && dumpFile)
    {
        std::lock_guard<std::mutex> lock{logMutex};
        const DWORD                 currentThreadId = GetThreadId(GetCurrentThread());
        fprintf(dumpFile, "%d -> %d [label=\"", currentThreadId, GetThreadId(newThreadHandle));
        DumpStackTrace();
        fputs("\"]\n", dumpFile);
        fflush(dumpFile);
    }
    return newThreadHandle;
}

using SetThreadDescriptionFuncPtr                    = decltype(&SetThreadDescription);
SetThreadDescriptionFuncPtr RealSetThreadDescription = SetThreadDescription;

HRESULT WINAPI DetouredSetThreadDescription(_In_ HANDLE hThread, _In_ PCWSTR lpThreadDescription)
{
    const HRESULT hr = RealSetThreadDescription(hThread, lpThreadDescription);
    if (SUCCEEDED(hr) && dumpFile)
    {
        std::lock_guard<std::mutex> lock{logMutex};
        const DWORD                 currentThreadId = GetThreadId(hThread);
        fprintf(dumpFile, "%d [label=\"%ls\"]\n", currentThreadId, lpThreadDescription);
        fflush(dumpFile);
    }
    return hr;
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved)
{
    // Needed is someone tries to inject from a 64bit process
    if (DetourIsHelperProcess()) { return TRUE; }

    if (dwReason == DLL_PROCESS_ATTACH)
    {
        DetourRestoreAfterWith();

        LOG(" Starting.\n");

        LOG(" DLLs:\n");
        for (HMODULE hModule = NULL; (hModule = DetourEnumerateModules(hModule)) != NULL;)
        {
            CHAR szName[MAX_PATH] = {0};
            GetModuleFileNameA(hModule, szName, sizeof(szName) - 1);
            LOG("  {}: {}\n", (void*)hModule, szName);
        }

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());

        dumpFile = fopen("ThreadsTree.dot", "w");
        if (dumpFile)
        {
            fputs("digraph G {", dumpFile);
            LOGW(L"Patching KernelBase.dll\n");
            DetourAttach(&(PVOID&)RealCreateRemoteThreadEx, DetouredCreateRemoteThreadEx);
            LOGW(L"Patching Kernel32.dll\n");
            DetourAttach(&(PVOID&)RealSetThreadDescription, DetouredSetThreadDescription);
        }
        else
            LOGW(L"Couldn't create file ThreadsTree.dot, not hooking");
        const LONG error = DetourTransactionCommit();
        if (error == NO_ERROR)
            LOG(" Detoured for ThreadsTree.\n");
        else
            LOG(" Error detouring for ThreadsTree: {}\n", error);
    }
    else if (dwReason == DLL_PROCESS_DETACH)
    {
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());

        LOGW(L"Unpatching Kernel32.dll\n");
        DetourDetach(&(PVOID&)RealSetThreadDescription, DetouredSetThreadDescription);
        LOGW(L"Unpatching KernelBase.dll\n");
        DetourDetach(&(PVOID&)RealCreateRemoteThreadEx, DetouredCreateRemoteThreadEx);

        LONG error = DetourTransactionCommit();

        LOG(" Exiting ThreadsTree detours\n");

        fputs("}", dumpFile);
        fclose(dumpFile);
    }
    return TRUE;
}