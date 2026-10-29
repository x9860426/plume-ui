cmake_minimum_required(VERSION 3.20)
project(plume_python LANGUAGES CXX)

option(PLUME_BUILD_NATIVE "Build the native PyBind11 extension" OFF)

if (PLUME_BUILD_NATIVE)
    set(UIENGINE_BUILD_VULKAN OFF CACHE BOOL "" FORCE)
    add_subdirectory(../../engine ${CMAKE_BINARY_DIR}/plume_engine)

    find_package(pybind11 CONFIG REQUIRED)
    pybind11_add_module(_plume src/plume_module.cpp)
    target_link_libraries(_plume PRIVATE uiengine)
    target_include_directories(_plume PRIVATE ../../engine/src)

    install(TARGETS _plume LIBRARY DESTINATION plume)
endif()
