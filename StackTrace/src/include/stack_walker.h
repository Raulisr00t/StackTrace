#pragma once
#include <Windows.h>
#include <functional>
#include <vector>
#include <string>
#include "types.h"

namespace stacktrace {

    using WatchCallback = std::function<bool(const TraceResult&)>;

    void watch_stack(
        DWORD pid,
        WatchCallback callback,
        int interval_ms = 2000,
        int max_frames = 64,
        bool resolve_symbols = true,
        DWORD target_tid = 0
    );

} // namespace stacktrace