// src/include/stacktrace.h
#pragma once

#include "types.h"
#include <Windows.h>

#ifdef STACKTRACE_EXPORTS
    #define STACKTRACE_API __declspec(dllexport)
#else
    #define STACKTRACE_API
#endif

namespace stacktrace {

STACKTRACE_API TraceResult capture_stack_trace(
    uint32_t pid, 
    uint32_t max_frames = 64,
    bool resolve_symbols = true);

std::unordered_map<uint32_t, TraceResult> capture_all_threads(
    uint32_t pid,
    uint32_t max_frames = 64,
    bool resolve_symbols = true
);

STACKTRACE_API std::vector<std::string> get_process_modules(uint32_t pid);

STACKTRACE_API bool is_process_traceable(uint32_t pid);

}; // namespace stacktrace