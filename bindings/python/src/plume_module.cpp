#include <pybind11/pybind11.h>

#include "app/AppFwd.h"

namespace py = pybind11;

PYBIND11_MODULE(_plume, m) {
    m.doc() = "Native Python bindings for the Plume UI engine";
    m.def("engine_name", []() { return "Plume UI"; });
    m.def("vulkan_available", &ui::appVulkanAvailable);
}
