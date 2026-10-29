cmake_minimum_required(VERSION 3.20)
project(plume_python LANGUAGES CXX)

set(PLUME_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(../../engine ${CMAKE_BINARY_DIR}/plume_engine)

find_package(pybind11 CONFIG REQUIRED)

pybind11_add_module(_plume src/plume_module.cpp)
target_link_libraries(_plume PRIVATE plume_engine)
target_compile_features(_plume PRIVATE cxx_std_20)

install(TARGETS _plume LIBRARY DESTINATION plume)
