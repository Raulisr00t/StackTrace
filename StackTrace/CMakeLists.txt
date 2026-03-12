cmake_minimum_required(VERSION 3.15)
project(StackTrace LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Core static library  (shared by both the CLI and the Python extension)
# ---------------------------------------------------------------------------
add_library(stacktrace_core STATIC
    src/core/stack_walker.cpp
    src/core/context_wrapper.cpp
)

target_include_directories(stacktrace_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/include
)

target_compile_definitions(stacktrace_core PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

target_link_libraries(stacktrace_core PUBLIC
    psapi
    kernel32
)

# ---------------------------------------------------------------------------
# CLI executable
# ---------------------------------------------------------------------------
add_executable(StackTrace
    src/cli/main.cpp
)

target_link_libraries(StackTrace PRIVATE
    stacktrace_core
)

set_target_properties(StackTrace PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# ---------------------------------------------------------------------------
# Python extension  (only built when Python + pybind11 are found)
# ---------------------------------------------------------------------------
find_package(Python3 COMPONENTS Interpreter Development)
find_package(pybind11 CONFIG QUIET)

if(Python3_FOUND AND pybind11_FOUND)
    message(STATUS "Python ${Python3_VERSION} and pybind11 found — building Python extension")

    pybind11_add_module(_core MODULE
        src/python/pybindings.cpp
    )

    target_link_libraries(_core PRIVATE
        stacktrace_core
    )

    target_compile_definitions(_core PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )

    # Install the .pyd next to the pure-Python package so `import stacktrace` works
    install(TARGETS _core
        LIBRARY DESTINATION python_pkg/stacktrace
    )
else()
    if(NOT Python3_FOUND)
        message(STATUS "Python3 not found — skipping Python extension")
    endif()
    if(NOT pybind11_FOUND)
        message(STATUS "pybind11 not found — skipping Python extension (run: pip install pybind11)")
    endif()
endif()