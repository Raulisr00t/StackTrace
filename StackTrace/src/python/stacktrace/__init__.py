from ._core import (
    trace,
    get_modules,
    is_traceable,
    trace_all_threads,
    TraceResult,
    Frame,
)
import queue
import threading


def watch(pid, interval_ms=2000, max_frames=64, resolve_symbols=True, thread_id=0):
    q = queue.Queue(maxsize=1)
    sentinel = object()

    def _callback(result):
        q.put(result)
        return True

    def _worker():
        try:
            from ._core import watch as _watch_core
            _watch_core(
                pid=pid,
                interval_ms=interval_ms,
                max_frames=max_frames,
                resolve_symbols=resolve_symbols,
                thread_id=thread_id,
                callback=_callback,
            )
        finally:
            q.put(sentinel)

    t = threading.Thread(target=_worker, daemon=True)
    t.start()

    try:
        while True:
            item = q.get()
            if item is sentinel:
                break
            yield item
    except KeyboardInterrupt:
        pass
    finally:
        t.join(timeout=0.5)


__all__ = [
    "trace",
    "watch",
    "get_modules",
    "is_traceable",
    "trace_all_threads",
    "TraceResult",
    "Frame",
]