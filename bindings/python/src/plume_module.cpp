#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "plume/engine.h"

namespace py = pybind11;

PYBIND11_MODULE(_plume, m) {
    m.doc() = "Native Python bindings for the Plume UI engine";

    py::class_<plume::EngineConfig>(m, "EngineConfig")
        .def(py::init<>())
        .def_readwrite("application_name", &plume::EngineConfig::application_name)
        .def_readwrite("width", &plume::EngineConfig::width)
        .def_readwrite("height", &plume::EngineConfig::height)
        .def_readwrite("enable_vsync", &plume::EngineConfig::enable_vsync);

    py::class_<plume::Engine>(m, "NativeEngine")
        .def(py::init<>())
        .def("initialize", &plume::Engine::initialize, py::arg("config") = plume::EngineConfig{})
        .def("shutdown", &plume::Engine::shutdown)
        .def("is_running", &plume::Engine::is_running)
        .def("config", &plume::Engine::config, py::return_value_policy::reference_internal)
        .def_static("version", &plume::Engine::version);
}
