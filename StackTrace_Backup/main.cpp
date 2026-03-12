#include <Windows.h>
#include <stdio.h>

#include "types.h"

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "ntdll.lib")

BOOL IsNumber(const char* str)
{
    if (!str || !*str)
        return FALSE;
    while (*str)
    {
        if (!isdigit((unsigned char)*str))
            return FALSE;
        str++;
    }
    return TRUE;
}

DWORD GetMainThreadId(DWORD pid)
{
    THREADENTRY32 te32 = { 0 };
    te32.dwSize = sizeof(THREADENTRY32);

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
        return 0;

    DWORD mainTid = 0;
    if (Thread32First(hSnapshot, &te32))
    {
        do
        {
            if (te32.th32OwnerProcessID == pid)
            {
                mainTid = te32.th32ThreadID;
                break;
            }
        } while (Thread32Next(hSnapshot, &te32));
    }

    CloseHandle(hSnapshot);
    return mainTid;
}

int Run(DWORD pid)
{
    HANDLE hProcess = NULL;
    HANDLE hThread = NULL;

    printf("[+] Attaching to PID: %lu\n", pid);

    hProcess = OpenProcess(
        PROCESS_VM_READ | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION,
        FALSE, pid);
    if (!hProcess)
    {
        printf("[-] Failed to open process %lu (Error: %lu)\n", pid, GetLastError());
        printf("    Tip: Run as Administrator.\n");
        return 1;
    }

    DWORD tid = GetMainThreadId(pid);
    if (!tid)
    {
        printf("[-] Failed to get main thread ID\n");
        CloseHandle(hProcess);
        return 1;
    }

    hThread = OpenThread(
        THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION,
        FALSE, tid);

    if (!hThread)
    {
        printf("[-] Failed to open thread %lu (Error: %lu)\n", tid, GetLastError());
        CloseHandle(hProcess);
        return 1;
    }

    DWORD suspendCount = SuspendThread(hThread);
    if (suspendCount == (DWORD)-1)
    {
        printf("[-] Failed to suspend thread (Error: %lu)\n", GetLastError());
        CloseHandle(hThread);
        CloseHandle(hProcess);
        return 1;
    }
    printf("[+] Thread suspended (SuspendCount: %lu)\n", suspendCount);

    CONTEXT ctx = { 0 };
    ctx.ContextFlags = CONTEXT_FULL;

    if (!GetThreadContext(hThread, &ctx))
    {
        printf("[-] Failed to get thread context (Error: %lu)\n", GetLastError());
        ResumeThread(hThread);
        CloseHandle(hThread);
        CloseHandle(hProcess);
        return 1;
    }

    printf("\n[+] Thread Context Captured:\n");
    printf("    RIP: 0x%016llX\n", ctx.Rip);
    printf("    RSP: 0x%016llX\n", ctx.Rsp);
    printf("    RBP: 0x%016llX\n", ctx.Rbp);

    if (ctx.Rip == 0 || ctx.Rsp == 0)
    {
        printf("[-] Error: Invalid context (RIP/RSP is null)\n");
        ResumeThread(hThread);
        CloseHandle(hThread);
        CloseHandle(hProcess);
        return 1;
    }

    printf("\n[+] Starting Remote Stack Walk...\n\n");
    WalkStackRemote(hProcess, &ctx);

    ResumeThread(hThread);
    CloseHandle(hThread);
    CloseHandle(hProcess);

    return 0;
}

int main(int argc, char* argv[])
{
    printf("=============================================\n");
    printf("  Low-Level Windows Stack Trace Tool (x64)\n");
    printf("=============================================\n\n");

    if (argc != 2)
    {
        printf("[+] USAGE: StackTrace.exe <PID>\n");
        printf("    Example: StackTrace.exe 1234\n\n");
        printf("    To find a PID:\n");
        printf("    1. Open Task Manager\n");
        printf("    2. Go to Details tab\n");
        printf("    3. Find the PID column\n\n");
        printf("    Or use PowerShell: Get-Process <name> | Select Id\n");
        return 1;
    }

    if (!IsNumber(argv[1]))
    {
        printf("[-] Error: Please provide a numeric PID\n");
        printf("    Example: StackTrace.exe 1234\n");
        return 1;
    }

    DWORD pid = atoi(argv[1]);

    return Run(pid);
}