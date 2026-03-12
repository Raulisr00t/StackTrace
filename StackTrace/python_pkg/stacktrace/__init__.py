# python_pkg/stacktrace/__init__.py
"""
stacktrace — Low-level Windows Stack Trace Engine
"""

from stacktrace._core import (
    trace,
    get_modules,
    is_traceable,
    Frame,
    TraceResult,
)

__all__ = ["trace", "get_modules", "is_traceable", "Frame", "TraceResult"]
__version__ = "1.0.0"