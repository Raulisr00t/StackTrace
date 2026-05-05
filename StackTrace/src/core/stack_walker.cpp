// src/core/stack_walker.cpp
// Real x64 stack walker using PE .pdata / RUNTIME_FUNCTION / UNWIND_INFO
// No DbgHelp. No RSP scan heuristic. Exact frames via unwind chain.
#include "stacktrace.h"
#include "stack_walker.h"
#include "types.h"
#include <tlhelp32.h>
#include <psapi.h>
#include <vector>
#include <string>
#include <algorithm>
#include <unordered_map>

#pragma comment(lib, "psapi.lib")

namespace stacktrace {

    struct RUNTIME_FUNCTION_ENTRY {
        DWORD BeginAddress;
        DWORD EndAddress;
        DWORD UnwindInfoAddress;
    };

    struct UNWIND_INFO_HDR {
        BYTE VersionFlags;
        BYTE SizeOfProlog;
        BYTE CountOfCodes;
        BYTE FrameRegisterAndOffset;
    };

    union UNWIND_CODE_SLOT {
        struct { BYTE CodeOffset; BYTE UnwindOpAndInfo; };
        USHORT FrameOffset;
    };

#define UNW_FLAG_CHAININFO 0x04

    struct ModuleInfo {
        ULONG_PTR base;
        ULONG_PTR size;
        std::string name;
        std::vector<RUNTIME_FUNCTION_ENTRY> pdata;
        bool pdata_loaded = false;
    };

    static bool rpm(HANDLE hProc, ULONG_PTR addr, void* buf, SIZE_T sz)
    {
        SIZE_T read = 0;
        return ReadProcessMemory(hProc,
            reinterpret_cast<LPCVOID>(addr), buf, sz, &read) && read == sz;
    }

    static DWORD get_main_thread_id(DWORD pid)
    {
        THREADENTRY32 te32 = { sizeof(te32) };
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (hSnap == INVALID_HANDLE_VALUE) return 0;
        DWORD tid = 0;
        if (Thread32First(hSnap, &te32)) {
            do {
                if (te32.th32OwnerProcessID == pid) { tid = te32.th32ThreadID; break; }
            } while (Thread32Next(hSnap, &te32));
        }
        
        CloseHandle(hSnap);
        
        return tid;
    }

    static std::vector<ModuleInfo> enumerate_modules(HANDLE hProcess)
    {
        std::vector<ModuleInfo> mods;
        HMODULE hMods[1024]; DWORD cbNeeded = 0;
        
        if (!EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) return mods;
        
        DWORD count = cbNeeded / sizeof(HMODULE);
        
        for (DWORD i = 0; i < count; i++) {
            MODULEINFO mi{};
            if (!GetModuleInformation(hProcess, hMods[i], &mi, sizeof(mi))) continue;
        
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

    static bool resolve_address(
        HANDLE hProcess,
        const std::vector<ModuleInfo>& modules,

        ULONG_PTR addr,
        std::string& out_module,
        std::string& out_func,
        ULONG_PTR& out_offset)
    {
        for (const auto& m : modules) {
            if (addr < m.base || addr >= m.base + m.size) continue;
            out_module = m.name;
            out_offset = addr - m.base;
            out_func.clear();

            IMAGE_DOS_HEADER dos{};
            if (!rpm(hProcess, m.base, &dos, sizeof(dos))) return true;
            if (dos.e_magic != IMAGE_DOS_SIGNATURE) return true;

            IMAGE_NT_HEADERS64 nt{};
            if (!rpm(hProcess, m.base + dos.e_lfanew, &nt, sizeof(nt))) return true;
            if (nt.Signature != IMAGE_NT_SIGNATURE) return true;

            auto& expDir = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
            if (!expDir.VirtualAddress) return true;

            IMAGE_EXPORT_DIRECTORY expData{};
            if (!rpm(hProcess, m.base + expDir.VirtualAddress, &expData, sizeof(expData)))
                return true;

            DWORD numFuncs = expData.NumberOfFunctions;
            DWORD numNames = expData.NumberOfNames;
            
            ULONG_PTR addrFuncs = m.base + expData.AddressOfFunctions;
            ULONG_PTR addrNames = m.base + expData.AddressOfNames;
            ULONG_PTR addrOrdinals = m.base + expData.AddressOfNameOrdinals;

            std::vector<DWORD> funcRVAs(numFuncs);
            std::vector<DWORD> nameRVAs(numNames);
            std::vector<WORD>  ordinals(numNames);

            ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(addrFuncs),
                funcRVAs.data(), numFuncs * sizeof(DWORD), nullptr);
            ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(addrNames),
                nameRVAs.data(), numNames * sizeof(DWORD), nullptr);
            ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(addrOrdinals),
                ordinals.data(), numNames * sizeof(WORD), nullptr);

            ULONG_PTR bestAddr = 0; int bestIdx = -1;
            
            for (DWORD j = 0; j < numNames; j++) {
                if (ordinals[j] >= numFuncs) continue;
                ULONG_PTR fAddr = m.base + funcRVAs[ordinals[j]];
                if (fAddr <= addr && fAddr > bestAddr) { bestAddr = fAddr; bestIdx = (int)j; }
            }
            
            if (bestIdx >= 0) {
                char fname[256]{};
                ReadProcessMemory(hProcess,
                    reinterpret_cast<LPCVOID>(m.base + nameRVAs[bestIdx]),
                    fname, sizeof(fname) - 1, nullptr);
                out_func = fname;
                out_offset = addr - bestAddr;
            }
            return true;
        }
        return false;
    }

    static bool load_pdata(HANDLE hProcess, ModuleInfo& mod)
    {
        if (mod.pdata_loaded) return !mod.pdata.empty();
        mod.pdata_loaded = true;

        IMAGE_DOS_HEADER dos{};
        if (!rpm(hProcess, mod.base, &dos, sizeof(dos))) return false;
        if (dos.e_magic != IMAGE_DOS_SIGNATURE) return false;

        IMAGE_NT_HEADERS64 nt{};
        if (!rpm(hProcess, mod.base + dos.e_lfanew, &nt, sizeof(nt))) return false;
        if (nt.Signature != IMAGE_NT_SIGNATURE) return false;

        auto& dir = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
        if (!dir.VirtualAddress || !dir.Size) return false;

        DWORD count = dir.Size / sizeof(RUNTIME_FUNCTION_ENTRY);
        mod.pdata.resize(count);

        SIZE_T read = 0;
        ReadProcessMemory(hProcess,
            reinterpret_cast<LPCVOID>(mod.base + dir.VirtualAddress),
            mod.pdata.data(), count * sizeof(RUNTIME_FUNCTION_ENTRY), &read);

        if (read < sizeof(RUNTIME_FUNCTION_ENTRY)) { mod.pdata.clear(); return false; }
        mod.pdata.resize(read / sizeof(RUNTIME_FUNCTION_ENTRY));
        return true;
    }

    static const RUNTIME_FUNCTION_ENTRY* find_runtime_function(
        const ModuleInfo& mod, ULONG_PTR addr)
    {
        if (mod.pdata.empty()) return nullptr;
        DWORD rva = static_cast<DWORD>(addr - mod.base);
        DWORD lo = 0, hi = static_cast<DWORD>(mod.pdata.size());
        
        while (lo < hi) {
            DWORD mid = (lo + hi) / 2;
            const auto& rf = mod.pdata[mid];
        
            if (rva < rf.BeginAddress) hi = mid;
            else if (rva >= rf.EndAddress)   lo = mid + 1;
            else                             return &mod.pdata[mid];
        }
        return nullptr;
    }

    static ModuleInfo* find_module(std::vector<ModuleInfo>& modules, ULONG_PTR addr)
    {
        for (auto& m : modules)
            if (addr >= m.base && addr < m.base + m.size) return &m;
        return nullptr;
    }

    static bool unwind_one_frame(
        HANDLE hProcess,
        std::vector<ModuleInfo>& modules,
        ULONG_PTR  cur_rip,
        ULONG_PTR  cur_rsp,
        ULONG_PTR  cur_rbp,
        ULONG_PTR& next_rip,
        ULONG_PTR& next_rsp,
        ULONG_PTR& next_rbp)
    {
        ModuleInfo* pMod = find_module(modules, cur_rip);
        if (!pMod) {
            // Unknown module — leaf unwind attempt
            ULONG_PTR ret = 0;
            if (!rpm(hProcess, cur_rsp, &ret, sizeof(ret))) return false;
            if (!ret || ret < 0x10000) return false;
            next_rip = ret; next_rsp = cur_rsp + 8; next_rbp = cur_rbp;
            return true;
        }

        load_pdata(hProcess, *pMod);
        const RUNTIME_FUNCTION_ENTRY* rf = find_runtime_function(*pMod, cur_rip);

        if (!rf) {
            ULONG_PTR ret = 0;
            if (!rpm(hProcess, cur_rsp, &ret, sizeof(ret))) return false;
            if (!ret || ret < 0x10000) return false;

            next_rip = ret; next_rsp = cur_rsp + 8; next_rbp = cur_rbp;
            return true;
        }

        ULONG_PTR rsp = cur_rsp;
        ULONG_PTR rbp = cur_rbp;
        ULONG_PTR ui_addr = pMod->base + rf->UnwindInfoAddress;

        for (int chain = 0; chain < 32; chain++) {
            UNWIND_INFO_HDR hdr{};
            if (!rpm(hProcess, ui_addr, &hdr, sizeof(hdr))) return false;

            BYTE flags = (hdr.VersionFlags >> 3) & 0x1F;
            BYTE n_codes = hdr.CountOfCodes;
            BYTE freg = hdr.FrameRegisterAndOffset & 0x0F;
            BYTE foff16 = (hdr.FrameRegisterAndOffset >> 4) & 0x0F;

            ULONG_PTR codes_addr = ui_addr + sizeof(UNWIND_INFO_HDR);
            std::vector<UNWIND_CODE_SLOT> codes(n_codes);
            if (n_codes > 0) {
                SIZE_T r = 0;
                ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(codes_addr),
                    codes.data(), n_codes * sizeof(UNWIND_CODE_SLOT), &r);
            }

            ULONG_PTR frame_ptr = 0;
            if (freg == 5) // RBP
                frame_ptr = rbp - (ULONG_PTR)foff16 * 16;

            for (BYTE ci = 0; ci < n_codes; ) {
                BYTE op = codes[ci].UnwindOpAndInfo & 0x0F;
                BYTE info = (codes[ci].UnwindOpAndInfo >> 4) & 0x0F;

                switch (op) {
                    
                case 0: 
                {
                    ULONG64 saved = 0;
                    rpm(hProcess, rsp, &saved, sizeof(saved));
                    if (info == 5) rbp = saved; // restore RBP
                    rsp += 8;
                }
                ci++; break;
                    
                case 1: 
                    if (info == 0) {
                        if (ci + 1 < n_codes) { rsp += (ULONG_PTR)codes[ci + 1].FrameOffset * 8; ci += 2; }
                        else ci++;
                    }
                    else {
                        if (ci + 2 < n_codes) {
                            ULONG32 sz = 0; memcpy(&sz, &codes[ci + 1], sizeof(sz));
                            rsp += sz; ci += 3;
                        }
                        else ci++;
                    }
                    break;
                    
                case 2: 
                    rsp += (ULONG_PTR)info * 8 + 8;
                    ci++; break;
                    
                case 3: 
                    if (frame_ptr) rsp = frame_ptr;
                    ci++; break;
                    
                case 4: case 8: // UWOP_SAVE_NONVOL / UWOP_SAVE_XMM128
                    ci += 2; break;
                    
                case 5: case 9: // UWOP_SAVE_NONVOL_FAR / UWOP_SAVE_XMM128_FAR
                    ci += 3; break;
                
                case 10: // UWOP_PUSH_MACHFRAME
                    if (info == 1) rsp += 8;
                    {
                        ULONG_PTR ret = 0;
                        rpm(hProcess, rsp, &ret, sizeof(ret));
                        next_rip = ret;
                        next_rsp = rsp + 8 * 5;
                        next_rbp = rbp;
                        return next_rip > 0x10000;
                    }
                default:
                    ci++; break;
                }
            }

            if (flags & UNW_FLAG_CHAININFO) {
                DWORD slots = (n_codes + 1) & ~1;
                ULONG_PTR chain_rf_addr = codes_addr +
                    (ULONG_PTR)slots * sizeof(UNWIND_CODE_SLOT);
                RUNTIME_FUNCTION_ENTRY chain_rf{};
                if (!rpm(hProcess, chain_rf_addr, &chain_rf, sizeof(chain_rf))) break;
                ui_addr = pMod->base + chain_rf.UnwindInfoAddress;
                continue;
            }
            break;
        }

        ULONG_PTR ret_addr = 0;
        if (!rpm(hProcess, rsp, &ret_addr, sizeof(ret_addr))) return false;
        if (!ret_addr || ret_addr < 0x10000) return false;

        next_rip = ret_addr;
        next_rsp = rsp + 8;
        next_rbp = rbp;
        return true;
    }

    static std::vector<Frame> walk_stack_remote(
        HANDLE hProcess, CONTEXT* ctx, uint32_t max_frames)
    {
        std::vector<Frame> frames;
        auto modules = enumerate_modules(hProcess);
        for (auto& m : modules) load_pdata(hProcess, m);

        ULONG_PTR rip = ctx->Rip;
        ULONG_PTR rsp = ctx->Rsp;
        ULONG_PTR rbp = ctx->Rbp;

        {
            Frame f; f.address = rip;
            resolve_address(hProcess, modules, rip,
                f.module_name, f.function_name, f.offset_in_module);
            frames.push_back(f);
        }

        for (uint32_t depth = 1; depth < max_frames; depth++) {
            ULONG_PTR next_rip = 0, next_rsp = 0, next_rbp = 0;
            if (!unwind_one_frame(hProcess, modules, rip, rsp, rbp,
                next_rip, next_rsp, next_rbp)) break;
            
            if (next_rip < 0x10000) break;
            if (next_rsp <= rsp)    break;
            if (next_rip == rip && next_rsp == rsp) break;

            rip = next_rip; rsp = next_rsp; rbp = next_rbp;

            Frame f; f.address = rip;
            resolve_address(hProcess, modules, rip,
                f.module_name, f.function_name, f.offset_in_module);
            frames.push_back(f);
        }
        return frames;
    }

    TraceResult capture_stack_trace(uint32_t pid, uint32_t max_frames, bool)
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
            CloseHandle(hThread); CloseHandle(hProcess);
            result.error_message = "SuspendThread failed";
            return result;
        }

        CONTEXT ctx{}; ctx.ContextFlags = CONTEXT_FULL;
        if (!GetThreadContext(hThread, &ctx)) {
            ResumeThread(hThread); CloseHandle(hThread); CloseHandle(hProcess);
            result.error_message = "GetThreadContext failed";
            return result;
        }

        result.frames = walk_stack_remote(hProcess, &ctx, max_frames);
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
        
        if (!hProcess) 
            return out;
        
        HMODULE hMods[1024]; DWORD cbNeeded = 0;
        
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

    void watch_stack(
        DWORD pid, WatchCallback callback,
        
        int interval_ms, int max_frames, bool resolve_symbols, DWORD target_tid)
    {
        while (true) {
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
            if (!hProcess) break;
            
            DWORD exitCode = 0;
            BOOL alive = GetExitCodeProcess(hProcess, &exitCode);
            
            CloseHandle(hProcess);
            if (!alive || exitCode != STILL_ACTIVE) break;

            DWORD tid = target_tid != 0 ? target_tid : get_main_thread_id(pid);
            
            HANDLE hProcess2 = OpenProcess(
                PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
            if (!hProcess2) break;

            HANDLE hThread = OpenThread(
                THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE, tid);
            if (!hThread) { CloseHandle(hProcess2); Sleep(interval_ms); continue; }

            if (SuspendThread(hThread) == (DWORD)-1) {
                CloseHandle(hThread); CloseHandle(hProcess2);
                Sleep(interval_ms); continue;
            }

            CONTEXT ctx{}; ctx.ContextFlags = CONTEXT_FULL;
            TraceResult result;
            result.process_id = pid;
            result.thread_id = tid;

            if (GetThreadContext(hThread, &ctx)) {
                result.frames = walk_stack_remote(hProcess2, &ctx, max_frames);
                result.success = !result.frames.empty();
            }

            ResumeThread(hThread);
            CloseHandle(hThread);
            CloseHandle(hProcess2);

            if (!callback(result)) break;
            Sleep(interval_ms);
        }
    }

    std::unordered_map<uint32_t, TraceResult> capture_all_threads(
        uint32_t pid, uint32_t max_frames, bool resolve_symbols)
    {
        std::unordered_map<uint32_t, TraceResult> results;

        HANDLE hProcess = OpenProcess(
            PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
        if (!hProcess) return results;

        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (hSnap == INVALID_HANDLE_VALUE) { CloseHandle(hProcess); return results; }

        auto modules = enumerate_modules(hProcess);
        for (auto& m : modules) load_pdata(hProcess, m);

        THREADENTRY32 te32 = { sizeof(te32) };
        if (!Thread32First(hSnap, &te32)) 
            CloseHandle(hSnap); CloseHandle(hProcess); return results;

        do {
            if (te32.th32OwnerProcessID != pid) continue;
            DWORD tid = te32.th32ThreadID;
            TraceResult result;
            result.process_id = pid;
            result.thread_id = tid;

            HANDLE hThread = OpenThread(
                THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE, tid);
            if (!hThread) {
                result.error_message = "OpenThread failed";
                results[tid] = result; continue;
            }

            if (SuspendThread(hThread) == (DWORD)-1) {
                CloseHandle(hThread);
                result.error_message = "SuspendThread failed";
                results[tid] = result; continue;
            }

            CONTEXT ctx{}; 
            ctx.ContextFlags = CONTEXT_FULL;

            if (GetThreadContext(hThread, &ctx)) {
                result.frames = walk_stack_remote(hProcess, &ctx, max_frames);
                result.success = !result.frames.empty();
            }
            else 
                result.error_message = "GetThreadContext failed";

            ResumeThread(hThread);
            CloseHandle(hThread);
            results[tid] = result;

        } while (Thread32Next(hSnap, &te32));

        CloseHandle(hSnap);
        CloseHandle(hProcess);

        return results;
    }

} // namespace stacktrace
