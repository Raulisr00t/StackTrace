#pragma once

#include <Windows.h>
#include <stdint.h>
#include <vector>
#include <string>

namespace stacktrace {

struct Frame {
    uint64_t address;
    std::string module_name;
    std::string function_name;
    uint64_t offset_in_module;
    
    Frame() : address(0), offset_in_module(0) {}
};

struct TraceResult {
    bool success;
    std::string error_message;
    uint32_t process_id;
    uint32_t thread_id;
    std::vector<Frame> frames;
    
    TraceResult() : success(false), process_id(0), thread_id(0) {}
};

}