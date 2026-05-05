<div align="center">

# 🔍 StackTrace

**Low-level Windows Stack Trace Engine**

*Walks the call stack of any running process — no DbgHelp, no debug APIs, just raw Win32*

[![PyPI version](https://img.shields.io/pypi/v/stacktrace.svg?style=for-the-badge&color=blue)](https://pypi.org/project/stacktrace/)
[![Python](https://img.shields.io/pypi/pyversions/stacktrace.svg?style=for-the-badge)](https://pypi.org/project/stacktrace/)
[![Platform](https://img.shields.io/badge/platform-Windows%20x64-blue?style=for-the-badge&logo=windows)](https://pypi.org/project/stacktrace/)
[![License](https://img.shields.io/badge/license-MIT-green?style=for-the-badge)](LICENSE)
[![Downloads](https://img.shields.io/pypi/dm/stacktrace?style=for-the-badge&color=orange)](https://pypi.org/project/stacktrace/)

</div>

---

## 🧠 What is StackTrace?

**StackTrace** is a Windows-only Python library with a C++ core that captures the full call stack of any running process at runtime — similar to WinDBG's `k` command — **without using DbgHelp.dll or any debug API**.

### How it works
1. Opens the target process with `OpenProcess`
2. Suspends the main thread and captures the full CPU context (`RIP`, `RSP`, `RBP`)
3. Scans stack memory for return addresses
4. Resolves addresses to module names and exported function names via the **PE export directory**
5. Returns structured results you can work with in Python

### Why is this useful?
- 🔬 **Malware analysis** — inspect what a suspicious process is doing
- 🛡️ **Security research** — understand execution flow without a debugger
- 🐛 **Debugging** — attach to a hung process and see exactly where it's stuck
- 📊 **Profiling** — sample stack traces for performance analysis

---

## ⚡ Installation

```bash
pip install stacktrace
```

> **Requirements:** Windows x64 · Python 3.9 – 3.14

No compilation needed — pre-built wheels are available for all supported Python versions.

---

## 🚀 Quick Start

```python
import stacktrace

pid = 1234  # replace with your target PID

# Check if the process is accessible
if not stacktrace.is_traceable(pid):
    print("Cannot trace this process — try running as Administrator")
    exit(1)

# Capture the stack
result = stacktrace.trace(pid, max_frames=64)

if not result:
    print(f"Failed: {result.error}")
    exit(1)

# Print all frames
for i, frame in enumerate(result.frames):
    if frame.function_name:
        print(f"Frame {i:>3}: {frame.module_name}!{frame.function_name}+0x{frame.offset:x}  (0x{frame.address:016x})")
    else:
        print(f"Frame {i:>3}: {frame.module_name}+0x{frame.offset:x}  (0x{frame.address:016x})")
```

### Example Output
```
Frame   0: ntdll.dll!NtWaitForAlertByThreadId+0x14         (0x00007ffa78da56a4)
Frame   1: ntdll.dll!RtlSleepConditionVariableSRW+0x1de    (0x00007ffa78cd4c1e)
Frame   2: KERNELBASE.dll!SleepConditionVariableSRW+0x38   (0x00007ffa759b2818)
Frame   3: mozglue.dll!moz_xmalloc+0x0                     (0x00007ffa19e71890)
Frame   4: xul.dll+0x17d5aea                               (0x00007ff9e6cc5aea)
...
Frame  63: ntdll.dll!LdrGetDllFullName+0x221               (0x00007ffa78c53571)

[+] Total frames captured: 64 / 64
```

---

## 📖 API Reference

### `stacktrace.trace(pid, max_frames=64, resolve_symbols=True)`
Capture the full stack trace of a running process.

| Parameter | Type | Default | Description |
|---|---|---|---|
| `pid` | `int` | — | Target process ID |
| `max_frames` | `int` | `64` | Maximum number of frames to capture |
| `resolve_symbols` | `bool` | `True` | Resolve function names via PE exports |

**Returns:** `TraceResult`

---

### `stacktrace.get_modules(pid)`
Get all DLLs loaded by a process.

```python
modules = stacktrace.get_modules(1234)
# ['ntdll.dll', 'KERNEL32.DLL', 'KERNELBASE.dll', 'firefox.exe', ...]
```

**Returns:** `list[str]`

---

### `stacktrace.is_traceable(pid)`
Check if a process can be opened for tracing.

```python
if stacktrace.is_traceable(1234):
    result = stacktrace.trace(1234)
```

**Returns:** `bool`

---

### `TraceResult`

| Attribute | Type | Description |
|---|---|---|
| `success` | `bool` | Whether the trace succeeded |
| `error` | `str` | Error message if failed |
| `frames` | `list[Frame]` | Captured stack frames |
| `pid` | `int` | Target process ID |
| `thread_id` | `int` | Thread ID that was traced |

---

### `Frame`

| Attribute | Type | Description |
|---|---|---|
| `address` | `int` | Absolute virtual address |
| `module_name` | `str` | DLL or EXE name (e.g. `ntdll.dll`) |
| `function_name` | `str` | Exported function name (empty if unknown) |
| `offset` | `int` | Byte offset from function/module base |

---

## 🛠️ Advanced Usage

### Group frames by module
```python
from collections import defaultdict
import stacktrace

result = stacktrace.trace(1234)
by_module = defaultdict(list)

for frame in result.frames:
    by_module[frame.module_name].append(frame)

for module, frames in sorted(by_module.items()):
    print(f"\n{module}  ({len(frames)} frame(s))")
    for f in frames:
        label = f.function_name if f.function_name else f"+0x{f.offset:x}"
        print(f"    {label}")
```

### Trace your own process
```python
import os
import stacktrace

pid = os.getpid()
result = stacktrace.trace(pid)
print(f"Captured {len(result.frames)} frames from self")
```

### Find which DLLs are active on the stack
```python
import stacktrace

result = stacktrace.trace(1234)
active_dlls = set(f.module_name for f in result.frames)
print("DLLs active on stack:", active_dlls)
```

---

## 🏗️ Build from Source

**Requirements:**
- Windows x64
- Python 3.9+
- Visual Studio 2019/2022 Build Tools with C++ workload
- pybind11

```powershell
git clone https://github.com/Raulisr00t/StackTrace.git
cd StackTrace/StackTrace/src/python

pip install pybind11 build
py -3.12 -m build --wheel
pip install dist/*.whl
```

---

## 📸 Real World Output

```
=============================================
  Low-Level Windows Stack Trace Tool (x64)
=============================================

[+] Attaching to PID: 376
[+] Thread suspended (SuspendCount: 0)

[+] Thread Context Captured:
    RIP: 0x00007FFA78DA56A4
    RSP: 0x00000094A57FE7C8
    RBP: 0x00000094A57FE840

[+] Enumerating 42 remote modules...
    [!] firefox.exe:     Loaded 70   exported functions
    [!] ntdll.dll:       Loaded 2516 exported functions
    [!] KERNEL32.DLL:    Loaded 1692 exported functions
    [!] KERNELBASE.dll:  Loaded 2034 exported functions
    [!] mozglue.dll:     Loaded 432  exported functions
    ...

Frame  0: 0x00007FFA78DA56A4 [ntdll.dll!NtWaitForAlertByThreadId+0x14]
Frame  1: 0x00007FFA78CD4C1E [ntdll.dll!RtlSleepConditionVariableSRW+0x1DE]
Frame  2: 0x00007FF9EE0052E5 [xul.dll+0x8B152E5]
Frame  3: 0x00007FFA759B2818 [KERNELBASE.dll!SleepConditionVariableSRW+0x38]
Frame  4: 0x00007FFA19E8FBD3 [mozglue.dll!ConditionVariableImpl+0x13]
...
Frame 63: 0x00007FFA78C53571 [ntdll.dll!LdrGetDllFullName+0x221]

[+] Total frames captured: 64 / 64
```

---

## ⚠️ Notes

- **Administrator privileges** are required to trace most system or other-user processes
- **Windows x64 only** — relies on 64-bit CPU register layout
- Symbol resolution uses the **PE export table** — resolves exported functions only, not private/debug symbols
- The target thread is briefly **suspended** during capture and immediately resumed

---

## 📄 License

MIT © [Raulisr00t](https://github.com/Raulisr00t)

