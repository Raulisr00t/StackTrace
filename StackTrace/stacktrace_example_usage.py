"""
StackTrace Library - Complete Usage Guide
==========================================
A low-level Windows x64 stack trace engine.
Real x64 unwind walker via PE .pdata / RUNTIME_FUNCTION / UNWIND_INFO.
No DbgHelp. No debug symbols. No debugger.

Installation:
    pip install stacktrace

Requirements:
    - Windows x64
    - Python 3.9 - 3.14
    - Administrator privileges recommended for system processes

Usage:
    python example.py                 # auto-finds notepad
    python example.py 1234            # trace specific PID
    python example.py slack.exe       # trace by process name
"""

import stacktrace
import time
import sys
import subprocess


# ─────────────────────────────────────────────
# Utilities
# ─────────────────────────────────────────────

def find_pid_by_name(name):
    """Find all PIDs matching a process name."""
    result = subprocess.run(
        ["tasklist", "/FI", f"IMAGENAME eq {name}", "/FO", "CSV", "/NH"],
        capture_output=True, text=True
    )
    pids = []
    for line in result.stdout.strip().splitlines():
        parts = line.strip('"').split('","')
        if len(parts) >= 2:
            try:
                pids.append(int(parts[1]))
            except ValueError:
                pass
    return pids


def print_frame(i, frame):
    """Print a single stack frame."""
    if frame.function_name:
        print(f"  [{i:>3}] {frame.module_name}!{frame.function_name}+0x{frame.offset:x}"
              f"  (0x{frame.address:016x})")
    else:
        print(f"  [{i:>3}] {frame.module_name}+0x{frame.offset:x}"
              f"  (0x{frame.address:016x})")


def separator(title, pid=None):
    label = f"  {title}" + (f" (PID {pid})" if pid else "")
    print(f"\n{'='*60}")
    print(label)
    print(f"{'='*60}")


# ─────────────────────────────────────────────
# EXAMPLE 1 — One-shot stack trace (main thread)
# ─────────────────────────────────────────────

def example_trace(pid):
    separator("EXAMPLE 1 — One-shot trace, main thread", pid)

    if not stacktrace.is_traceable(pid):
        print(f"[!] PID {pid} is not traceable. Try running as Administrator.")
        return

    result = stacktrace.trace(pid, max_frames=64)

    if not result:
        print(f"[!] Trace failed: {result.error}")
        return

    print(f"[+] Captured {len(result.frames)} frames\n")
    for i, frame in enumerate(result.frames):
        print_frame(i, frame)


# ─────────────────────────────────────────────
# EXAMPLE 2 — List loaded modules
# ─────────────────────────────────────────────

def example_modules(pid):
    separator("EXAMPLE 2 — Loaded modules", pid)

    modules = stacktrace.get_modules(pid)
    print(f"[+] Found {len(modules)} loaded modules:\n")
    for mod in sorted(modules):
        print(f"  {mod}")


# ─────────────────────────────────────────────
# EXAMPLE 3 — Trace all threads simultaneously
# ─────────────────────────────────────────────

def example_all_threads(pid):
    separator("EXAMPLE 3 — All threads simultaneously", pid)

    results = stacktrace.trace_all_threads(pid, max_frames=8)
    print(f"[+] Found {len(results)} threads:\n")

    for tid, result in results.items():
        if not result.frames:
            continue
        top = result.frames[0]
        print(f"  Thread {tid:6d}: {top.module_name}!{top.function_name}+0x{top.offset:x}")


# ─────────────────────────────────────────────
# EXAMPLE 4 — Full stack per thread (non-sleeping)
# ─────────────────────────────────────────────

def example_active_threads(pid):
    separator("EXAMPLE 4 — Full stack of active threads", pid)

    SLEEP_CALLS = {"NtWaitForAlertByThreadId", "NtWaitForSingleObject",
                   "NtWaitForMultipleObjects", "NtDelayExecution",
                   "NtRemoveIoCompletion", "NtUserMsgWaitForMultipleObjectsEx"}

    results = stacktrace.trace_all_threads(pid, max_frames=32)
    shown = 0

    for tid, result in results.items():
        if not result.frames:
            continue
        top = result.frames[0].function_name or ""
        if any(s in top for s in SLEEP_CALLS):
            continue

        print(f"\n  Thread {tid}:")
        for i, frame in enumerate(result.frames):
            print_frame(i, frame)
        shown += 1

    if shown == 0:
        print("  [*] All threads are currently waiting/sleeping.")
        print("  [*] Interact with the process and run again.")


# ─────────────────────────────────────────────
# EXAMPLE 5 — Watch mode (main thread, periodic)
# ─────────────────────────────────────────────

def example_watch(pid, count=5, interval_ms=2000):
    separator("EXAMPLE 5 — Watch mode (main thread)", pid)
    print(f"  Interval: {interval_ms}ms  |  Snapshots: {count}")
    print("  Press Ctrl+C to stop early\n")

    for i, snapshot in enumerate(stacktrace.watch(pid, interval_ms=interval_ms)):
        print(f"  --- Snapshot {i+1} ({len(snapshot.frames)} frames) ---")
        for frame in snapshot.frames[:6]:
            if frame.function_name:
                print(f"    {frame.module_name}!{frame.function_name}+0x{frame.offset:x}")
            else:
                print(f"    {frame.module_name}+0x{frame.offset:x}")
        print()
        if i + 1 >= count:
            break


# ─────────────────────────────────────────────
# EXAMPLE 6 — Watch specific thread
# ─────────────────────────────────────────────

def example_watch_thread(pid, tid, count=3, interval_ms=2000):
    separator(f"EXAMPLE 6 — Watch specific thread {tid}", pid)
    print(f"  Interval: {interval_ms}ms  |  Snapshots: {count}")
    print("  Press Ctrl+C to stop early\n")

    for i, snapshot in enumerate(
            stacktrace.watch(pid, thread_id=tid, interval_ms=interval_ms)):
        print(f"  --- Thread {tid} snapshot {i+1} ---")
        for frame in snapshot.frames[:8]:
            if frame.function_name:
                print(f"    {frame.module_name}!{frame.function_name}+0x{frame.offset:x}")
            else:
                print(f"    {frame.module_name}+0x{frame.offset:x}")
        print()
        if i + 1 >= count:
            break


# ─────────────────────────────────────────────
# EXAMPLE 7 — Real-time change detection (all threads)
# ─────────────────────────────────────────────

def example_detect_changes(pid, duration_seconds=20):
    separator("EXAMPLE 7 — Real-time thread change detection", pid)
    print(f"  Duration: {duration_seconds}s")
    print("  Interact with the process to see activity!\n")

    prev_tops = {}
    start = time.time()

    try:
        while time.time() - start < duration_seconds:
            results = stacktrace.trace_all_threads(pid, max_frames=4)
            for tid, result in results.items():
                if not result.frames:
                    continue
                top = result.frames[0].function_name
                if tid not in prev_tops or prev_tops[tid] != top:
                    module = result.frames[0].module_name
                    print(f"  [CHANGED] Thread {tid:6d}: {module}!{top}")
                    prev_tops[tid] = top
            time.sleep(0.5)
    except KeyboardInterrupt:
        print("\n  Stopped.")


# ─────────────────────────────────────────────
# EXAMPLE 8 — Architecture fingerprinting
# Shows what runtime/framework a process uses
# ─────────────────────────────────────────────

def example_fingerprint(pid):
    separator("EXAMPLE 8 — Runtime architecture fingerprint", pid)

    SIGNATURES = {
        "Electron/Node.js": ["uv_os_getpid", "libuv", "node.dll"],
        "V8 JavaScript":    ["v8::", "CompilationDependencies", "js_new_ucstring"],
        ".NET / CLR":       ["clr.dll", "coreclr", "mscorlib", "RHBinder"],
        "JVM":              ["jvm.dll", "hotspot"],
        "Python":           ["python3", "_PyEval", "PyObject"],
        "Chromium":         ["chrome.dll", "content.dll", "IsSandboxedProcess"],
        "Qt Framework":     ["Qt5Core", "Qt6Core", "QApplication"],
        "Direct3D / GPU":   ["d3d11", "dxgi", "NtGdiDdDDI"],
        "OpenGL":           ["opengl32", "libGL"],
        "Crypto / TLS":     ["nss3.dll", "bcrypt", "SslEncryptPacket"],
        "libc / MSVC CRT":  ["ucrtbase", "msvcrt", "msvcp"],
    }

    modules = set(stacktrace.get_modules(pid))
    results = stacktrace.trace_all_threads(pid, max_frames=16)

    all_frames = []
    for result in results.values():
        all_frames.extend(result.frames)

    frame_text = " ".join(
        (f.function_name or "") + " " + f.module_name
        for f in all_frames
    ).lower()

    module_text = " ".join(modules).lower()

    print(f"\n[+] Detected technologies:\n")
    found_any = False
    for tech, sigs in SIGNATURES.items():
        for sig in sigs:
            if sig.lower() in frame_text or sig.lower() in module_text:
                print(f"  ✓ {tech}")
                found_any = True
                break

    if not found_any:
        print("  [*] No common frameworks detected — likely a native C/C++ binary.")

    print(f"\n[+] Top active modules on stack:\n")
    mod_counts = {}
    for f in all_frames:
        mod_counts[f.module_name] = mod_counts.get(f.module_name, 0) + 1
    for mod, count in sorted(mod_counts.items(), key=lambda x: -x[1])[:10]:
        print(f"  {count:4d} frames  {mod}")


# ─────────────────────────────────────────────
# EXAMPLE 9 — Export to JSON
# ─────────────────────────────────────────────

def example_export_json(pid, filename="trace_output.json"):
    separator("EXAMPLE 9 — Export trace to JSON", pid)
    import json

    results = stacktrace.trace_all_threads(pid, max_frames=32)

    output = {
        "pid": pid,
        "timestamp": time.strftime("%Y-%m-%dT%H:%M:%S"),
        "thread_count": len(results),
        "threads": {}
    }

    for tid, result in results.items():
        output["threads"][str(tid)] = {
            "success": result.success,
            "error": result.error,
            "frame_count": len(result.frames),
            "frames": [
                {
                    "index": i,
                    "address": hex(f.address),
                    "module": f.module_name,
                    "function": f.function_name,
                    "offset": hex(f.offset)
                }
                for i, f in enumerate(result.frames)
            ]
        }

    with open(filename, "w") as fp:
        json.dump(output, fp, indent=2)

    print(f"[+] Exported {len(results)} threads to {filename}")
    print(f"[+] Total frames: {sum(len(r.frames) for r in results.values())}")


# ─────────────────────────────────────────────
# EXAMPLE 10 — Hunt for specific function
# ─────────────────────────────────────────────

def example_hunt(pid, target_function):
    separator(f"EXAMPLE 10 — Hunt for '{target_function}' on any thread", pid)

    results = stacktrace.trace_all_threads(pid, max_frames=64)
    found = []

    for tid, result in results.items():
        for i, frame in enumerate(result.frames):
            if target_function.lower() in (frame.function_name or "").lower():
                found.append((tid, i, frame))

    if found:
        print(f"[+] Found '{target_function}' in {len(found)} frame(s):\n")
        for tid, depth, frame in found:
            print(f"  Thread {tid:6d}  Frame [{depth:>2}]: "
                  f"{frame.module_name}!{frame.function_name}+0x{frame.offset:x}")
    else:
        print(f"  [-] '{target_function}' not found on any thread right now.")
        print(f"      Try again while the process is doing relevant work.")


# ─────────────────────────────────────────────
# MAIN
# ─────────────────────────────────────────────

if __name__ == "__main__":
    print("""
  StackTrace v2.0.0 — Example Script
  =====================================
  Real x64 unwind walker via PE .pdata / RUNTIME_FUNCTION / UNWIND_INFO
  No DbgHelp. No debug symbols. No debugger.
  Run as Administrator for best results.
    """)

    # Resolve target PID
    if len(sys.argv) >= 2:
        arg = sys.argv[1]
        if arg.isdigit():
            pid = int(arg)
        else:
            # Treat as process name
            name = arg if arg.endswith(".exe") else arg + ".exe"
            pids = find_pid_by_name(name)
            if not pids:
                print(f"[!] Process '{name}' not found.")
                sys.exit(1)
            pid = pids[0]
            print(f"[*] Found {name} → PID {pid}")
    else:
        # Default: open notepad
        pids = find_pid_by_name("notepad.exe")
        if not pids:
            print("[*] Notepad not found. Opening it...")
            subprocess.Popen(["notepad.exe"])
            time.sleep(1)
            pids = find_pid_by_name("notepad.exe")
        if not pids:
            print("[!] Could not find or open Notepad.")
            sys.exit(1)
        pid = pids[0]

    print(f"[*] Target PID: {pid}\n")

    if not stacktrace.is_traceable(pid):
        print("[!] Process is not traceable. Try running as Administrator.")
        sys.exit(1)

    # Run all examples
    example_trace(pid)
    example_modules(pid)
    example_all_threads(pid)
    example_active_threads(pid)
    example_watch(pid, count=3, interval_ms=2000)

    # Watch first available thread
    thread_results = stacktrace.trace_all_threads(pid, max_frames=1)
    if thread_results:
        first_tid = next(iter(thread_results))
        example_watch_thread(pid, first_tid, count=2, interval_ms=2000)

    example_detect_changes(pid, duration_seconds=10)
    example_fingerprint(pid)
    example_export_json(pid)
    example_hunt(pid, "NtWait")

    print("\n[+] All examples complete!")
