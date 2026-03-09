#include "unwind.h"

#include <winnt.h>
#include <stdio.h>
#include <psapi.h>
#include <vector>
#include <string>
#include <algorithm>

#define MAX_FRAMES 64
#define MIN_VALID_CODE_ADDR 0x10000
#define MAX_VALID_CODE_ADDR 0x7FFFFFFFFFFF
#define RSP_SCAN_WINDOW 256


struct RemoteModule {
    DWORD64 base;
    DWORD64 end;
    char name[MAX_PATH];
    std::vector<std::pair<DWORD64, std::string>> exports;
};

BOOL ReadRemoteQWORD(HANDLE hProcess, DWORD64 address, DWORD64* value)
{
    if (!value || address < MIN_VALID_CODE_ADDR || address > MAX_VALID_CODE_ADDR)
        return FALSE;

    SIZE_T bytesRead = 0;
    BOOL result = ReadProcessMemory(hProcess, (LPCVOID)address, value, sizeof(DWORD64), &bytesRead);

    if (result && bytesRead == sizeof(DWORD64))
    {
        if (*value >= MIN_VALID_CODE_ADDR && *value <= MAX_VALID_CODE_ADDR)
            return TRUE;
    }

    return FALSE;
}

BOOL ReadRemoteMemory(HANDLE hProcess, DWORD64 address, void* buffer, SIZE_T size)
{
    if (!buffer || address < MIN_VALID_CODE_ADDR)
        return FALSE;

    SIZE_T bytesRead = 0;
    BOOL result = ReadProcessMemory(hProcess, (LPCVOID)address, buffer, size, &bytesRead);
    return (result && bytesRead == size);
}

void ParseExportTable(HANDLE hProcess, RemoteModule& mod)
{
    IMAGE_DOS_HEADER dosHeader = { 0 };
    if (!ReadRemoteMemory(hProcess, mod.base, &dosHeader, sizeof(dosHeader)))
        return;

    if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE)
        return;

    IMAGE_NT_HEADERS64 ntHeaders = { 0 };
    if (!ReadRemoteMemory(hProcess, mod.base + dosHeader.e_lfanew, &ntHeaders, sizeof(ntHeaders)))
        return;

    if (ntHeaders.Signature != IMAGE_NT_SIGNATURE)
        return;

    IMAGE_DATA_DIRECTORY exportDir = ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (exportDir.VirtualAddress == 0 || exportDir.Size == 0)
        return;

    DWORD64 exportTableAddr = mod.base + exportDir.VirtualAddress;

    IMAGE_EXPORT_DIRECTORY exportDirStruct = { 0 };
    if (!ReadRemoteMemory(hProcess, exportTableAddr, &exportDirStruct, sizeof(exportDirStruct)))
        return;

    std::vector<DWORD> addressTable(exportDirStruct.NumberOfFunctions);
    std::vector<DWORD> nameTable(exportDirStruct.NumberOfNames);
    std::vector<WORD> ordinalTable(exportDirStruct.NumberOfNames);

    if (!ReadRemoteMemory(hProcess, mod.base + exportDirStruct.AddressOfFunctions,
        addressTable.data(), addressTable.size() * sizeof(DWORD)))
        return;

    if (!ReadRemoteMemory(hProcess, mod.base + exportDirStruct.AddressOfNames,
        nameTable.data(), nameTable.size() * sizeof(DWORD)))
        return;

    if (!ReadRemoteMemory(hProcess, mod.base + exportDirStruct.AddressOfNameOrdinals,
        ordinalTable.data(), ordinalTable.size() * sizeof(WORD)))
        return;

    for (DWORD i = 0; i < exportDirStruct.NumberOfNames; i++)
    {
        DWORD64 nameAddr = mod.base + nameTable[i];
        char funcName[256] = { 0 };

        SIZE_T bytesRead = 0;
        if (ReadProcessMemory(hProcess, (LPCVOID)nameAddr, funcName, 255, &bytesRead))
        {
            funcName[255] = 0; 

            WORD ordinal = ordinalTable[i];
            if (ordinal < addressTable.size())
            {
                DWORD rva = addressTable[ordinal];
                if (rva != 0)
                {
                    DWORD64 funcAddr = mod.base + rva;
                    mod.exports.push_back({ funcAddr, std::string(funcName) });
                }
            }
        }
    }

    std::sort(mod.exports.begin(), mod.exports.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });

    printf("    [!] %s: Loaded %zu exported functions\n", mod.name, mod.exports.size());
}

const char* GetFunctionName(const RemoteModule& mod, DWORD64 address, DWORD64* offset)
{
    if (mod.exports.empty())
        return nullptr;

    size_t left = 0, right = mod.exports.size();

    while (left < right)
    {
        size_t mid = left + (right - left) / 2;
        if (mod.exports[mid].first <= address)
            left = mid + 1;
        else
            right = mid;
    }

    if (left > 0)
    {
        const auto& exp = mod.exports[left - 1];
        if (exp.first <= address && address < exp.first + 0x1000) 
        {
            if (offset) *offset = address - exp.first;
            return exp.second.c_str();
        }
    }

    return nullptr;
}

BOOL EnumerateRemoteModules(HANDLE hProcess, std::vector<RemoteModule>& modules)
{
    HMODULE hMods[1024];
    DWORD cbNeeded = 0;

    if (!EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded))
    {
        return FALSE;
    }

    DWORD moduleCount = cbNeeded / sizeof(HMODULE);
    modules.reserve(moduleCount);

    printf("[+] Enumerating %u remote modules...\n", moduleCount);

    for (DWORD i = 0; i < moduleCount; i++)
    {
        MODULEINFO modInfo = { 0 };
        if (GetModuleInformation(hProcess, hMods[i], &modInfo, sizeof(modInfo)))
        {
            RemoteModule mod = { 0 };
            mod.base = (DWORD64)modInfo.lpBaseOfDll;
            mod.end = mod.base + (DWORD64)modInfo.SizeOfImage;

            if (GetModuleFileNameExA(hProcess, hMods[i], mod.name, sizeof(mod.name)))
            {
                char* fileName = strrchr(mod.name, '\\');
                if (fileName)
                    memmove(mod.name, fileName + 1, strlen(fileName));
            }

            else
                strcpy_s(mod.name, "unknown");

            if (modInfo.SizeOfImage < 50 * 1024 * 1024) 
                ParseExportTable(hProcess, mod);
            

            modules.push_back(mod);
        }
    }

    return !modules.empty();
}

BOOL IsValidCodeAddress(const std::vector<RemoteModule>& modules, DWORD64 address)
{
    if (address < MIN_VALID_CODE_ADDR || address > MAX_VALID_CODE_ADDR)
        return FALSE;

    for (const auto& mod : modules)
    {
        if (address >= mod.base && address < mod.end)
            return TRUE;
    }

    if (modules.empty())
        return TRUE;

    return FALSE;
}

const RemoteModule* GetModuleForAddress(const std::vector<RemoteModule>& modules, DWORD64 address)
{
    for (const auto& mod : modules)
    {
        if (address >= mod.base && address < mod.end)
            return &mod;
    }
    return nullptr;
}


void WalkStackRemote(HANDLE hProcess, CONTEXT* ctx)
{
    if (!hProcess || !ctx)
    {
        printf("[-] Invalid handle or context\n");
        return;
    }

    std::vector<RemoteModule> modules;
    if (!EnumerateRemoteModules(hProcess, modules))
    {
        printf("[!] Module enumeration failed\n");
        return;
    }

    DWORD frame = 0;
    DWORD64 currentRip = ctx->Rip;
    DWORD64 currentRsp = ctx->Rsp;
    DWORD64 currentRbp = ctx->Rbp;

    printf("\n[+] Remote Stack Walk (PID: %lu)\n", GetProcessId(hProcess));
    printf("    Entry: RIP=0x%016llX RSP=0x%016llX RBP=0x%016llX\n\n",
        currentRip, currentRsp, currentRbp);

    printf("[DEBUG] Scanning %d QWORDS from RSP...\n", RSP_SCAN_WINDOW);
    DWORD candidateCount = 0;

    for (int offset = 0; offset < RSP_SCAN_WINDOW && candidateCount < 32; offset++)
    {
        DWORD64 candidateAddr = currentRsp + (offset * 8);
        DWORD64 value = 0;

        if (ReadRemoteQWORD(hProcess, candidateAddr, &value))
        {
            if (value != 0 && IsValidCodeAddress(modules, value) && value != currentRip)
            {
                candidateCount++;
                printf("    [Candidate %d] Offset %3d: 0x%016llX\n", candidateCount, offset, value);
            }
        }
    }
    printf("\n");

    while (frame < MAX_FRAMES && currentRip != 0)
    {
        printf("Frame %2u: 0x%016llX", frame, currentRip);

        const RemoteModule* mod = GetModuleForAddress(modules, currentRip);
        if (mod)
        {
            DWORD64 modOffset = currentRip - mod->base;

            DWORD64 funcOffset = 0;
            const char* funcName = GetFunctionName(*mod, currentRip, &funcOffset);

            if (funcName)
            
                printf(" [%s!%s+0x%llX]", mod->name, funcName, funcOffset);
            
            else
            
                printf(" [%s+0x%llX]", mod->name, modOffset);
            
        }

        else
            printf(" [unknown]");
        
        printf("\n");

        BOOL unwound = FALSE;
        DWORD64 nextRip = 0, nextRbp = 0, nextRsp = 0;

        if (!unwound && currentRbp >= MIN_VALID_CODE_ADDR)
        {
            DWORD64 savedRbp = 0, savedRip = 0;
            if (ReadRemoteQWORD(hProcess, currentRbp, &savedRbp) &&
                ReadRemoteQWORD(hProcess, currentRbp + 8, &savedRip))
            {
                if (savedRbp > currentRbp && IsValidCodeAddress(modules, savedRip) &&
                    savedRip != currentRip && savedRip != 0)
                {
                    nextRip = savedRip;
                    nextRbp = savedRbp;
                    nextRsp = currentRbp + 16;
                    unwound = TRUE;
                }
            }
        }

        if (!unwound && currentRsp >= MIN_VALID_CODE_ADDR)
        {
            for (int offset = 0; offset < RSP_SCAN_WINDOW; offset++)
            {
                DWORD64 candidateAddr = currentRsp + (offset * 8);
                DWORD64 candidateRip = 0;

                if (ReadRemoteQWORD(hProcess, candidateAddr, &candidateRip))
                {
                    if (IsValidCodeAddress(modules, candidateRip) &&
                        candidateRip != currentRip && candidateRip != 0)
                    {
                        nextRip = candidateRip;
                        nextRbp = 0;
                        nextRsp = candidateAddr + 8;
                        unwound = TRUE;
                        break;
                    }
                }
            }
        }

        if (!unwound || nextRip == 0)
        {
            printf("    [!] End of traceable stack\n");
            break;
        }

        currentRip = nextRip;
        currentRbp = nextRbp;
        currentRsp = nextRsp;
        frame++;
    }

    printf("\n[+] Total frames captured: %u / %d\n", frame, MAX_FRAMES);
}