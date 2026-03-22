"""
StackTrace Library - Example Usage
===================================
A low-level Windows stack trace engine.

Installation:
    pip install stacktrace

Requirements:
    - Windows x64
    - Python 3.9 - 3.14
    - Administrator privileges recommended
"""

import stacktrace
import time
import sys


def find_pid_by_name(name):
    """Find a process PID by name using tasklist."""
    import subprocess
    result = subprocess.run(
        ["tasklist", "/FI", f"IMAGENAME eq {name}", "/FO", "CSV", "/NH"],
        capture_output=True, text=True
    )
    lines = result.stdout.strip().splitlines()
    pids = []
    for line in lines:
        parts = line.strip('"').split('","')
        if len(parts) >= 2:
            try:
                pids.append(int(parts[1]))
            except ValueError:
                pass
    return pids

def example_trace(pid):
    print(f"\n{'='*50}")
    print(f"  EXAMPLE 1 — One-shot trace (PID {pid})")
    print(f"{'='*50}")

    if not stacktrace.is_traceable(pid):
        print(f"[!] PID {pid} is not traceable. Try running as Administrator.")
        return

    result = stacktrace.trace(pid, max_frames=32)

    if not result:
        print(f"[!] Trace failed: {result.error}")
        return

    print(f"[+] Captured {len(result.frames)} frames from PID {pid}\n")

    for i, frame in enumerate(result.frames):
        if frame.function_name:
            print(f"  [{i:>3}] {frame.module_name}!{frame.function_name}+0x{frame.offset:x}")
        else:
            print(f"  [{i:>3}] {frame.module_name}+0x{frame.offset:x}")

def example_modules(pid):
    print(f"\n{'='*50}")
    print(f"  EXAMPLE 2 — Loaded modules (PID {pid})")
    print(f"{'='*50}")

    modules = stacktrace.get_modules(pid)
    print(f"[+] Found {len(modules)} loaded modules:\n")
    for mod in sorted(modules):
        print(f"  {mod}")

def example_all_threads(pid):
    print(f"\n{'='*50}")
    print(f"  EXAMPLE 3 — All threads (PID {pid})")
    print(f"{'='*50}")

    results = stacktrace.trace_all_threads(pid, max_frames=8)
    print(f"[+] Found {len(results)} threads:\n")

    for tid, result in results.items():
        if not result.frames:
            continue
        top = result.frames[0]
        print(f"  Thread {tid:6d}: {top.module_name}!{top.function_name}")

def example_watch(pid, count=5):
    print(f"\n{'='*50}")
    print(f"  EXAMPLE 4 — Watch mode, {count} snapshots (PID {pid})")
    print(f"{'='*50}")
    print("  Press Ctrl+C to stop early\n")

    i = 0
    for snapshot in stacktrace.watch(pid, interval_ms=2000):
        print(f"  --- Snapshot {i+1} ({len(snapshot.frames)} frames) ---")
        for frame in snapshot.frames[:5]:  # show top 5 frames only
            print(f"    {frame.module_name}!{frame.function_name}")
        print()
        i += 1
        if i >= count:
            break

def example_watch_thread(pid, tid, count=3):
    print(f"\n{'='*50}")
    print(f"  EXAMPLE 5 — Watch thread {tid}, {count} snapshots")
    print(f"{'='*50}")
    print("  Press Ctrl+C to stop early\n")

    i = 0
    for snapshot in stacktrace.watch(pid, thread_id=tid, interval_ms=2000):
        print(f"  --- Thread {tid} snapshot {i+1} ---")
        for frame in snapshot.frames[:8]:
            print(f"    {frame.module_name}!{frame.function_name}")
        print()
        i += 1
        if i >= count:
            break

def example_detect_changes(pid, duration_seconds=20):
    print(f"\n{'='*50}")
    print(f"  EXAMPLE 6 — Detect thread changes for {duration_seconds}s (PID {pid})")
    print(f"{'='*50}")
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
                    print(f"  [CHANGED] Thread {tid}: {module}!{top}")
                    prev_tops[tid] = top

            time.sleep(1)

    except KeyboardInterrupt:
        print("\n  Stopped.")

if __name__ == "__main__":
    print("""
  StackTrace Library - Example Script
  =====================================
  This script demonstrates all features of the stacktrace library.
  Run as Administrator for best results.
    """)

    # Try to find Notepad — open it if not running
    pids = find_pid_by_name("notepad.exe")

    if not pids:
        print("[*] Notepad not found. Opening it...")
        import subprocess
        subprocess.Popen(["notepad.exe"])
        time.sleep(1)
        pids = find_pid_by_name("notepad.exe")

    if not pids:
        print("[!] Could not find or open Notepad.")
        print("    Usage: python example.py")
        print("    Or pass a PID: python example.py 1234")
        sys.exit(1)

    pid = int(sys.argv[1]) if len(sys.argv) > 1 else pids[0]
    print(f"[*] Using PID: {pid}\n")

    # Run all examples
    example_trace(pid)
    example_modules(pid)
    example_all_threads(pid)
    example_watch(pid, count=3)

    # For watch thread example, pick the first valid thread
    results = stacktrace.trace_all_threads(pid, max_frames=1)
    if results:
        first_tid = next(iter(results))
        example_watch_thread(pid, first_tid, count=2)

    example_detect_changes(pid, duration_seconds=10)

    print("\n[+] All examples complete!")
