// src/python/pybindings.cpp
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"
#include "stacktrace.h"

namespace py = pybind11;

PYBIND11_MODULE(_core, m) {
    m.doc() = "Windows Stack Trace Engine";  // FIX: missing semicolon

    py::class_<stacktrace::Frame>(m, "Frame")
        .def_readonly("address",       &stacktrace::Frame::address)
        .def_readonly("module_name",   &stacktrace::Frame::module_name)
        .def_readonly("function_name", &stacktrace::Frame::function_name)
        .def_readonly("offset",        &stacktrace::Frame::offset_in_module)
        .def("__repr__", [](const stacktrace::Frame& f) {
            char buf[256];
            if (!f.function_name.empty())
                snprintf(buf, sizeof(buf), "<Frame %s!%s+0x%llx>",
                         f.module_name.c_str(), f.function_name.c_str(),
                         (unsigned long long)f.offset_in_module);
            else
                snprintf(buf, sizeof(buf), "<Frame %s+0x%llx>",
                         f.module_name.c_str(),
                         (unsigned long long)f.offset_in_module);
            return std::string(buf);
        });

    py::class_<stacktrace::TraceResult>(m, "TraceResult")
        .def_readonly("success",    &stacktrace::TraceResult::success)
        .def_readonly("error",      &stacktrace::TraceResult::error_message)
        .def_readonly("frames",     &stacktrace::TraceResult::frames)
        .def_readonly("pid",        &stacktrace::TraceResult::process_id)
        .def_readonly("thread_id",  &stacktrace::TraceResult::thread_id)
        .def("__bool__", [](const stacktrace::TraceResult& r) { return r.success; })
        .def("__len__",  [](const stacktrace::TraceResult& r) { return r.frames.size(); })
        .def("__repr__", [](const stacktrace::TraceResult& r) {
            return "<TraceResult pid=" + std::to_string(r.process_id) +
                   " frames=" + std::to_string(r.frames.size()) +
                   " success=" + (r.success ? "True" : "False") + ">";
        });

    m.def("trace",
          &stacktrace::capture_stack_trace,
          py::arg("pid"),
          py::arg("max_frames")      = 64,
          py::arg("resolve_symbols") = true,
          "Capture the stack trace of a running Windows process by PID.");

    m.def("get_modules",
          &stacktrace::get_process_modules,
          py::arg("pid"),
          "Return a list of module names loaded by the process.");

    m.def("is_traceable",
          &stacktrace::is_process_traceable,
          py::arg("pid"),
          "Return True if the process can be opened for tracing.");
}