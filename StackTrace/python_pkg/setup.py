# python_pkg/setup.py
import os
import sys
from pathlib import Path
from setuptools import setup, Extension
import pybind11

# Platform check
if sys.platform != "win32":
    raise RuntimeError("stacktrace only supports Windows")

# Source configuration
SRC_ROOT = Path(__file__).parent.parent
CORE_SOURCES = [
    str(SRC_ROOT / "src" / "core" / "stack_walker.cpp"),
    str(SRC_ROOT / "src" / "core" / "context_wrapper.cpp"),
]
BINDING_SOURCE = str(SRC_ROOT / "src" / "python" / "pybindings.cpp")

# Extension module
ext_modules = [
    Extension(
        "stacktrace._core",
        sources=[BINDING_SOURCE] + CORE_SOURCES,
        include_dirs=[
            str(SRC_ROOT / "src" / "include"),
            pybind11.get_include(),
            pybind11.get_include(user=True),
        ],
        libraries=["psapi", "ntdll", "kernel32"],
        extra_compile_args=["/EHsc", "/std:c++17", "/O2", "/DSTACKTRACE_LIB"],
        language="c++",
    ),
]

setup(
    name="stacktrace",
    version="1.0.0",
    author="Raul Mansurov",
    description="Low-level Windows Stack Trace Engine",
    packages=["stacktrace"],
    package_dir={"": "."},
    ext_modules=ext_modules,
    install_requires=["pybind11>=2.10.0"],
    python_requires=">=3.8",
    zip_safe=False,
)