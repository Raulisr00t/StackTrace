# Advanced Windows App Stack Tracer ( Without Any Debug API/Lib)

## Simulates `k` command of WinDBG

## Output of example app running in windows
```powershell
.\StackTrace.exe 376
=============================================
  Low-Level Windows Stack Trace Tool (x64)
=============================================

[+] Attaching to PID: 376
[+] Thread suspended (SuspendCount: 0)

[+] Thread Context Captured:
    RIP: 0x00007FFA78DA56A4
    RSP: 0x00000094A57FE7C8
    RBP: 0x00000094A57FE840

[+] Starting Remote Stack Walk...

[+] Enumerating 42 remote modules...
    [!] firefox.exe: Loaded 70 exported functions
    [!] ntdll.dll: Loaded 2516 exported functions
    [!] KERNEL32.DLL: Loaded 1692 exported functions
    [!] KERNELBASE.dll: Loaded 2034 exported functions
    [!] SHLWAPI.dll: Loaded 380 exported functions
    [!] ucrtbase.dll: Loaded 2484 exported functions
    [!] Windows.Storage.dll: Loaded 631 exported functions
    [!] combase.dll: Loaded 466 exported functions
    [!] RPCRT4.dll: Loaded 556 exported functions
    [!] msvcp_win.dll: Loaded 1524 exported functions
    [!] advapi32.dll: Loaded 851 exported functions
    [!] msvcrt.dll: Loaded 1330 exported functions
    [!] sechost.dll: Loaded 243 exported functions
    [!] mozglue.dll: Loaded 432 exported functions
    [!] CRYPT32.dll: Loaded 295 exported functions
    [!] MSVCP140.dll: Loaded 1515 exported functions
    [!] VCRUNTIME140.dll: Loaded 71 exported functions
    [!] VCRUNTIME140_1.dll: Loaded 3 exported functions
    [!] bcrypt.dll: Loaded 60 exported functions
    [!] CRYPTBASE.dll: Loaded 11 exported functions
    [!] bcryptPrimitives.dll: Loaded 11 exported functions
    [!] nss3.dll: Loaded 1350 exported functions
    [!] WS2_32.dll: Loaded 196 exported functions
    [!] WSOCK32.dll: Loaded 75 exported functions
    [!] gkcodecs.dll: Loaded 153 exported functions
    [!] lgpllibs.dll: Loaded 16 exported functions
    [!] shcore.dll: Loaded 97 exported functions
    [!] WINTRUST.dll: Loaded 165 exported functions
    [!] cfgmgr32.dll: Loaded 283 exported functions
    [!] ktmw32.dll: Loaded 44 exported functions
    [!] PROPSYS.dll: Loaded 224 exported functions
    [!] VERSION.dll: Loaded 17 exported functions
    [!] ncrypt.dll: Loaded 160 exported functions
    [!] MSASN1.dll: Loaded 152 exported functions
    [!] kernel.appcore.dll: Loaded 266 exported functions
    [!] ntmarta.dll: Loaded 50 exported functions
    [!] OLEAUT32.dll: Loaded 425 exported functions
    [!] winmm.dll: Loaded 180 exported functions
    [!] mozavcodec.dll: Loaded 66 exported functions
    [!] mozavutil.dll: Loaded 317 exported functions

[+] Remote Stack Walk (PID: 376)
    Entry: RIP=0x00007FFA78DA56A4 RSP=0x00000094A57FE7C8 RBP=0x00000094A57FE840

[DEBUG] Scanning 256 QWORDS from RSP...
    [Candidate 1] Offset   0: 0x00007FFA78CD4C1E
    [Candidate 2] Offset  17: 0x00007FF9EE0052E5
    [Candidate 3] Offset  20: 0x00007FFA759B2818
    [Candidate 4] Offset  28: 0x00007FFA19E8FBD3
    [Candidate 5] Offset  34: 0x00007FF9E6CC5AEA
    [Candidate 6] Offset  48: 0x00007FF9E7E22401
    [Candidate 7] Offset  54: 0x00007FF9ECFA1A68
    [Candidate 8] Offset  56: 0x00007FFA19E71890
    [Candidate 9] Offset  58: 0x00007FF9E55E0200
    [Candidate 10] Offset  60: 0x00007FFA19F058EC
    [Candidate 11] Offset  72: 0x00007FFA19E71ADB
    [Candidate 12] Offset 102: 0x00007FFA19CF45CC
    [Candidate 13] Offset 114: 0x00007FFA19E7ADF0
    [Candidate 14] Offset 118: 0x00007FF9E6D12F3A
    [Candidate 15] Offset 124: 0x00007FF9E7E22712
    [Candidate 16] Offset 125: 0x00007FF9ECFA1A68
    [Candidate 17] Offset 136: 0x00007FFA19E75025
    [Candidate 18] Offset 144: 0x00007FF9EC9C1A70
    [Candidate 19] Offset 148: 0x00007FF9ECFA1A68
    [Candidate 20] Offset 152: 0x00007FF9E6CC1B99
    [Candidate 21] Offset 162: 0x00007FF9E55E22A5
    [Candidate 22] Offset 164: 0x00007FF9EC9C1A70
    [Candidate 23] Offset 174: 0x00007FF9E5531DBF
    [Candidate 24] Offset 176: 0x00007FF9E55E1F5F
    [Candidate 25] Offset 178: 0x00007FF9EC9C1A70
    [Candidate 26] Offset 184: 0x00007FF9E55E0B32
    [Candidate 27] Offset 187: 0x00007FF9EE15A500
    [Candidate 28] Offset 195: 0x00007FFA19F058EC
    [Candidate 29] Offset 198: 0x00007FF9E55E2327
    [Candidate 30] Offset 210: 0x00007FF9E55E2237
    [Candidate 31] Offset 212: 0x00007FFA19F058EC
    [Candidate 32] Offset 215: 0x00007FF9EE1CA3AB

Frame  0: 0x00007FFA78DA56A4 [ntdll.dll!NtWaitForAlertByThreadId+0x14]
Frame  1: 0x00007FFA78CD4C1E [ntdll.dll!RtlSleepConditionVariableSRW+0x1DE]
Frame  2: 0x00007FF9EE0052E5 [xul.dll+0x8B152E5]
Frame  3: 0x00007FFA759B2818 [KERNELBASE.dll!SleepConditionVariableSRW+0x38]
Frame  4: 0x00007FFA19E8FBD3 [mozglue.dll!?wait@ConditionVariableImpl@detail@mozilla@@QEAAXAEAVMutexImpl@23@@Z+0x13]
Frame  5: 0x00007FF9E6CC5AEA [xul.dll+0x17D5AEA]
Frame  6: 0x00007FF9E7E22401 [xul.dll+0x2932401]
Frame  7: 0x00007FF9ECFA1A68 [xul.dll+0x7AB1A68]
Frame  8: 0x00007FFA19E71890 [mozglue.dll!moz_xmalloc+0x0]
Frame  9: 0x00007FF9E55E0200 [xul.dll+0xF0200]
Frame 10: 0x00007FFA19F058EC [mozglue.dll!?sChildProcessType@startup@mozilla@@3W4GeckoProcessType@@A+0x0]
Frame 11: 0x00007FFA19E71ADB [mozglue.dll!moz_xmalloc+0x24B]
Frame 12: 0x00007FFA19CF45CC [nss3.dll!PR_GetThreadPrivate+0x1C]
Frame 13: 0x00007FFA19E7ADF0 [mozglue.dll!?writeLock@RWLockImpl@detail@mozilla@@IEAAXXZ+0x0]
Frame 14: 0x00007FF9E6D12F3A [xul.dll+0x1822F3A]
Frame 15: 0x00007FF9E7E22712 [xul.dll+0x2932712]
Frame 16: 0x00007FF9ECFA1A68 [xul.dll+0x7AB1A68]
Frame 17: 0x00007FFA19E75025 [mozglue.dll!free+0x1C5]
Frame 18: 0x00007FF9EC9C1A70 [xul.dll+0x74D1A70]
Frame 19: 0x00007FF9ECFA1A68 [xul.dll+0x7AB1A68]
Frame 20: 0x00007FF9E6CC1B99 [xul.dll+0x17D1B99]
Frame 21: 0x00007FF9E55E22A5 [xul.dll+0xF22A5]
Frame 22: 0x00007FF9EC9C1A70 [xul.dll+0x74D1A70]
Frame 23: 0x00007FF9E5531DBF [xul.dll+0x41DBF]
Frame 24: 0x00007FF9E55E1F5F [xul.dll+0xF1F5F]
Frame 25: 0x00007FF9EC9C1A70 [xul.dll+0x74D1A70]
Frame 26: 0x00007FF9E55E0B32 [xul.dll+0xF0B32]
Frame 27: 0x00007FF9EE15A500 [xul.dll+0x8C6A500]
Frame 28: 0x00007FFA19F058EC [mozglue.dll!?sChildProcessType@startup@mozilla@@3W4GeckoProcessType@@A+0x0]
Frame 29: 0x00007FF9E55E2327 [xul.dll+0xF2327]
Frame 30: 0x00007FF9E55E2237 [xul.dll+0xF2237]
Frame 31: 0x00007FFA19F058EC [mozglue.dll!?sChildProcessType@startup@mozilla@@3W4GeckoProcessType@@A+0x0]
Frame 32: 0x00007FF9EE1CA3AB [xul.dll+0x8CDA3AB]
Frame 33: 0x00007FF9E5531DBF [xul.dll+0x41DBF]
Frame 34: 0x00007FFA19F058EC [mozglue.dll!?sChildProcessType@startup@mozilla@@3W4GeckoProcessType@@A+0x0]
Frame 35: 0x00007FF7ADD1ADA0 [firefox.exe+0x4ADA0]
Frame 36: 0x00007FF9E552A4AB [xul.dll+0x3A4AB]
Frame 37: 0x00007FFA19F058EC [mozglue.dll!?sChildProcessType@startup@mozilla@@3W4GeckoProcessType@@A+0x0]
Frame 38: 0x00007FF7ADCDA1F0 [firefox.exe!TargetNtCreateFile+0x860]
Frame 39: 0x00007FF7ADD39E50 [firefox.exe!g_nt+0x210]
Frame 40: 0x00007FF7ADCDA100 [firefox.exe!TargetNtCreateFile+0x770]
Frame 41: 0x00007FF7ADD39D78 [firefox.exe!g_nt+0x138]
Frame 42: 0x00007FF9EC8FDED0 [xul.dll+0x740DED0]
Frame 43: 0x00007FFA78D31000 [ntdll.dll!LdrGetDllHandleByMapping+0x1C0]
Frame 44: 0x00007FFA19E49FB3 [mozglue.dll!?WindowsDpiInitialization@mozilla@@YA?AW4WindowsDpiInitializationResult@1@XZ+0x23]
Frame 45: 0x00007FF7ADCD98E5 [firefox.exe+0x98E5]
Frame 46: 0x00007FFA75A14301 [KERNELBASE.dll!SystemTimeToFileTime+0xC81]
Frame 47: 0x00007FF7ADCD6EE8 [firefox.exe!TargetNtQueryAttributesFile+0x398]
Frame 48: 0x00007FFA19F058EC [mozglue.dll!?sChildProcessType@startup@mozilla@@3W4GeckoProcessType@@A+0x0]
Frame 49: 0x00007FF7ADCD436F [firefox.exe+0x436F]
Frame 50: 0x00007FFA78CB91D0 [ntdll.dll!RtlFreeHeap+0x620]
Frame 51: 0x00007FFA78C4BCAE [ntdll.dll+0xBCAE]
Frame 52: 0x00007FFA78C914C4 [ntdll.dll+0x514C4]
Frame 53: 0x00007FFA78C5A3EE [ntdll.dll!RtlDeactivateActivationContextUnsafeFast+0x14E]
Frame 54: 0x00007FFA78E124E0 [ntdll.dll+0x1D24E0]
Frame 55: 0x00007FFA78DDB110 [ntdll.dll+0x19B110]
Frame 56: 0x00007FFA78CFC38A [ntdll.dll+0xBC38A]
Frame 57: 0x00007FF7ADCD0078 [firefox.exe+0x78]
Frame 58: 0x00007FFA78C40000 [ntdll.dll+0x0]
Frame 59: 0x00007FFA78C5D0D1 [ntdll.dll!RtlAllocateHeap+0xF01]
Frame 60: 0x00007FFA78C538E9 [ntdll.dll!LdrGetDllFullName+0x599]
Frame 61: 0x00007FFA78E0E268 [ntdll.dll+0x1CE268]
Frame 62: 0x00007FFA78D82450 [ntdll.dll!RtlValidateProcessHeaps+0x960]
Frame 63: 0x00007FFA78C53571 [ntdll.dll!LdrGetDllFullName+0x221]

[+] Total frames captured: 64 / 64
```
