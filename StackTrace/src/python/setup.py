# src/python/setup.py
import sys
import os
from setuptools import setup, Extension
import pybind11

if sys.platform != "win32":
    raise RuntimeError("stacktrace only supports Windows")

# ALL paths must be relative to this setup.py file.
# This file is at: StackTrace/src/python/setup.py
# So relative to here:
#   ../../src/core/       = core C++ sources
#   ../../src/include/    = headers
#   ../../python_pkg/     = pure Python package

ext_modules = [
    Extension(
        "stacktrace._core",
        sources=[
            "pybindings.cpp",                          # same dir as setup.py
            "../../src/core/stack_walker.cpp",
            "../../src/core/context_wrapper.cpp",
        ],
        include_dirs=[
            "../../src/include",
            pybind11.get_include(),
        ],
        libraries=["psapi", "kernel32"],
        extra_compile_args=[
            "/EHsc", "/std:c++17", "/O2",
            "/DSTACKTRACE_LIB", "/DNOMINMAX",
        ],
        extra_link_args=["/LTCG"],
        language="c++",
    ),
]

setup(
    name="stacktrace",
    version="1.0.0",
    author="Raul Mansurov",
    author_email="mansurovraul1@gmail.com",
    description="Low-level Windows Stack Trace Engine",
    packages=["stacktrace"],
    package_dir={"stacktrace": "stacktrace"},
    ext_modules=ext_modules,
    install_requires=["pybind11>=2.10.0"],
    python_requires=">=3.8",
    zip_safe=False,
)
