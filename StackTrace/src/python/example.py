import stacktrace

pid = 13584  # replace with your target PID

# --- 1. Check if process is accessible ---
if not stacktrace.is_traceable(pid):
    print(f"Cannot trace PID {pid} (access denied or not found)")
    exit(1)

# --- 2. Get all loaded DLLs ---
modules = stacktrace.get_modules(pid)
print(f"[+] {len(modules)} modules loaded in PID {pid}:\n")
for dll in modules:
    print(f"    {dll}")

# --- 3. Capture stack trace with all frames ---
result = stacktrace.trace(pid, max_frames=128)

if not result:
    print(f"[-] Trace failed: {result.error}")
    exit(1)

print(f"\n[+] {len(result)} stack frames:\n")
for i, frame in enumerate(result.frames):
    if frame.function_name:
        print(f"  Frame {i:>3}: {frame.module_name}!{frame.function_name}+0x{frame.offset:x}"
              f"  (0x{frame.address:016x})")
    else:
        print(f"  Frame {i:>3}: {frame.module_name}+0x{frame.offset:x}"
              f"  (0x{frame.address:016x})")

# --- 4. Group frames by DLL ---
print("\n[+] Frames grouped by module:\n")
from collections import defaultdict
by_module = defaultdict(list)
for frame in result.frames:
    by_module[frame.module_name].append(frame)

for mod, frames in sorted(by_module.items()):
    print(f"  {mod}  ({len(frames)} frame(s))")
    for f in frames:
        if f.function_name:
            print(f"    +0x{f.offset:x}  [{f.function_name}]")
        else:
            print(f"    +0x{f.offset:x}")