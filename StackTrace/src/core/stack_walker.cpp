// src/core/stack_walker.cpp
#include "stacktrace.h"
#include "types.h"
#include <tlhelp32.h>
#include <psapi.h>
#include <vector>
#include <string>
#include <algorithm>

#pragma comment(lib, "psapi.lib")

namespace stacktrace {

struct ModuleInfo {
    ULONG_PTR base;
    ULONG_PTR size;
    std::string name;
};

static DWORD get_main_thread_id(DWORD pid)
{
    THREADENTRY32 te32 = { sizeof(te32) };
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return 0;

    DWORD tid = 0;
    if (Thread32First(hSnap, &te32)) {
        do {
            if (te32.th32OwnerProcessID == pid) {
                tid = te32.th32ThreadID;
                break;
            }
        } while (Thread32Next(hSnap, &te32));
    }
    CloseHandle(hSnap);
    return tid;
}

static std::vector<ModuleInfo> enumerate_modules(HANDLE hProcess)
{
    std::vector<ModuleInfo> mods;
    HMODULE hMods[1024];
    DWORD cbNeeded = 0;

    if (!EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded))
        return mods;

    DWORD count = cbNeeded / sizeof(HMODULE);
    for (DWORD i = 0; i < count; i++) {
        MODULEINFO mi{};
        if (!GetModuleInformation(hProcess, hMods[i], &mi, sizeof(mi)))
            continue;

        char path[MAX_PATH]{};
        GetModuleFileNameExA(hProcess, hMods[i], path, sizeof(path));
        const char* name = strrchr(path, '\\');

        ModuleInfo info;
        info.base = reinterpret_cast<ULONG_PTR>(mi.lpBaseOfDll);
        info.size = mi.SizeOfImage;
        info.name = name ? name + 1 : path;
        mods.push_back(info);
    }
    return mods;
}

// Resolve address → module name + offset  (no DbgHelp, pure export scan)
static bool resolve_address(
    HANDLE hProcess,
    const std::vector<ModuleInfo>& modules,
    ULONG_PTR addr,
    std::string& out_module,
    std::string& out_func,
    ULONG_PTR& out_offset)
{
    for (const auto& m : modules) {
        if (addr < m.base || addr >= m.base + m.size)
            continue;

        out_module = m.name;
        out_offset = addr - m.base;
        out_func.clear();

        // Try to resolve symbol via export directory
        IMAGE_DOS_HEADER dos{};
        ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(m.base),
                          &dos, sizeof(dos), nullptr);
        if (dos.e_magic != IMAGE_DOS_SIGNATURE) return true;

        IMAGE_NT_HEADERS64 nt{};
        ReadProcessMemory(hProcess,
                          reinterpret_cast<LPCVOID>(m.base + dos.e_lfanew),
                          &nt, sizeof(nt), nullptr);
        if (nt.Signature != IMAGE_NT_SIGNATURE) return true;

        auto& expDir = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        if (!expDir.VirtualAddress) return true;

        IMAGE_EXPORT_DIRECTORY expData{};
        ReadProcessMemory(hProcess,
                          reinterpret_cast<LPCVOID>(m.base + expDir.VirtualAddress),
                          &expData, sizeof(expData), nullptr);

        DWORD numFuncs  = expData.NumberOfFunctions;
        DWORD numNames  = expData.NumberOfNames;
        ULONG_PTR addrOfFunctions  = m.base + expData.AddressOfFunctions;
        ULONG_PTR addrOfNames      = m.base + expData.AddressOfNames;
        ULONG_PTR addrOfOrdinals   = m.base + expData.AddressOfNameOrdinals;

        std::vector<DWORD>  funcRVAs(numFuncs);
        std::vector<DWORD>  nameRVAs(numNames);
        std::vector<WORD>   ordinals(numNames);

        ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(addrOfFunctions),
                          funcRVAs.data(), numFuncs * sizeof(DWORD), nullptr);
        ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(addrOfNames),
                          nameRVAs.data(), numNames * sizeof(DWORD), nullptr);
        ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(addrOfOrdinals),
                          ordinals.data(), numNames * sizeof(WORD), nullptr);

        ULONG_PTR bestAddr   = 0;
        
        int        bestIndex = -1;

        for (DWORD j = 0; j < numNames; j++) {
            ULONG_PTR fAddr = m.base + funcRVAs[ordinals[j]];
            
            if (fAddr <= addr && fAddr > bestAddr) {
                bestAddr  = fAddr;
                bestIndex = static_cast<int>(j);
            }
        }

        if (bestIndex >= 0) {
            char fname[256]{};
            ReadProcessMemory(hProcess,
                              reinterpret_cast<LPCVOID>(m.base + nameRVAs[bestIndex]),
                              fname, sizeof(fname) - 1, nullptr);
            out_func   = fname;
            out_offset = addr - bestAddr;
        }
        return true;
    }
    return false;
}

static std::vector<Frame> walk_stack_remote(
    HANDLE hProcess,
    CONTEXT* ctx,
    uint32_t max_frames)
{
    std::vector<Frame> frames;
    auto modules = enumerate_modules(hProcess);

    // Frame 0: current RIP
    {
        Frame f;
        f.address = ctx->Rip;
        resolve_address(hProcess, modules, ctx->Rip,
                        f.module_name, f.function_name, f.offset_in_module);
        frames.push_back(f);
    }

    const DWORD  SCAN_DEPTH  = 256;
    ULONG_PTR    rsp         = ctx->Rsp;
    ULONG64      stack_buf[SCAN_DEPTH]{};
    SIZE_T       bytesRead   = 0;

    if (!ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(rsp),
                           stack_buf, sizeof(stack_buf), &bytesRead))
        return frames;

    DWORD readable = static_cast<DWORD>(bytesRead / sizeof(ULONG64));

    for (DWORD i = 0; i < readable && frames.size() < max_frames; i++) {
        ULONG_PTR candidate = stack_buf[i];
        if (candidate < 0x10000 || candidate == 0)       
            continue;

        std::string mod, func;
        ULONG_PTR   off = 0;
        
        if (resolve_address(hProcess, modules, candidate, mod, func, off)) {
            Frame f;
            f.address         = candidate;
            f.module_name     = mod;
            f.function_name   = func;
            f.offset_in_module = off;
            frames.push_back(f);
        }
    }

    return frames;
}

TraceResult capture_stack_trace(uint32_t pid, uint32_t max_frames, bool /*resolve_symbols*/)
{
    TraceResult result;
    result.process_id = pid;

    HANDLE hProcess = OpenProcess(
        PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!hProcess) {
        result.error_message = "OpenProcess failed: " + std::to_string(GetLastError());
        return result;
    }

    DWORD tid = get_main_thread_id(pid);
    if (!tid) {
        CloseHandle(hProcess);
        result.error_message = "Failed to get main thread ID";
        return result;
    }
    result.thread_id = tid;

    HANDLE hThread = OpenThread(
        THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE, tid);
    if (!hThread) {
        CloseHandle(hProcess);
        result.error_message = "OpenThread failed: " + std::to_string(GetLastError());
        return result;
    }

    if (SuspendThread(hThread) == (DWORD)-1) {
        CloseHandle(hThread);
        CloseHandle(hProcess);
        result.error_message = "SuspendThread failed";
        return result;
    }

    CONTEXT ctx{};
    ctx.ContextFlags = CONTEXT_FULL;
    
    if (!GetThreadContext(hThread, &ctx)) {
        ResumeThread(hThread);
        CloseHandle(hThread);
        CloseHandle(hProcess);
        result.error_message = "GetThreadContext failed";
        return result;
    }

    result.frames  = walk_stack_remote(hProcess, &ctx, max_frames);
    result.success = !result.frames.empty();

    ResumeThread(hThread);
    CloseHandle(hThread);
    CloseHandle(hProcess);
    
    return result;
}

std::vector<std::string> get_process_modules(uint32_t pid)
{
    std::vector<std::string> out;
    HANDLE hProcess = OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hProcess) return out;

    HMODULE hMods[1024];
    DWORD   cbNeeded = 0;
    if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
        for (DWORD i = 0; i < cbNeeded / sizeof(HMODULE); i++) {
            char path[MAX_PATH]{};
            if (GetModuleFileNameExA(hProcess, hMods[i], path, sizeof(path))) {
                const char* name = strrchr(path, '\\');
                out.push_back(name ? name + 1 : path);
            }
        }
    }
    
    CloseHandle(hProcess);
    return out;
}

bool is_process_traceable(uint32_t pid)
{
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    
    if (!h) 
        return false;
    
    CloseHandle(h);
    return true;
}

} // namespace stacktrace
