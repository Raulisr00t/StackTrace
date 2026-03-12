// src/cli/main.cpp
#include "stacktrace.h"
#include <iostream>
#include <string>

static bool is_number(const std::string& str)
{
    return !str.empty() && str.find_first_not_of("0123456789") == std::string::npos;
}

int main(int argc, char* argv[])
{
    std::cout << "=============================================\n";
    std::cout << "  Low-Level Windows Stack Trace Tool (x64)\n";
    std::cout << "=============================================\n\n";

    if (argc != 2) {
        std::cout << "[+] USAGE: StackTrace.exe <PID>\n";
        std::cout << "    Example: StackTrace.exe 1234\n";
        return 1;
    }

    if (!is_number(argv[1])) {
        std::cerr << "[-] Error: PID must be a number\n";
        return 1;
    }

    uint32_t pid = static_cast<uint32_t>(std::stoul(argv[1]));
    
    if (!stacktrace::is_process_traceable(pid)) {
        std::cerr << "[-] Process " << pid << " cannot be traced\n";
        return 1;
    }

    std::cout << "[+] Attaching to PID: " << pid << "\n";
    
    auto result = stacktrace::capture_stack_trace(pid, 64, true);
    
    if (!result.success) {
        std::cerr << "[-] Error: " << result.error_message << "\n";
        return 1;
    }
    
    std::cout << "\n[+] Captured " << result.frames.size() << " frames:\n\n";
    
    for (size_t i = 0; i < result.frames.size(); i++) {
        const auto& f = result.frames[i];
        if (!f.function_name.empty()) {
            std::cout << "Frame " << i << ": " 
                      << f.module_name << "!" << f.function_name 
                      << "+0x" << std::hex << f.offset_in_module << std::dec
                      << " (0x" << std::hex << f.address << std::dec << ")\n";
        } else {
            std::cout << "Frame " << i << ": " 
                      << f.module_name << "+0x" << std::hex << f.offset_in_module << std::dec
                      << " (0x" << std::hex << f.address << std::dec << ")\n";
        }
    }
    
    std::cout << "\n[+] Done.\n";
    return 0;
}