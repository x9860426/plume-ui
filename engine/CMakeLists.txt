cmake_minimum_required(VERSION 3.20)
project(PlumeEngine VERSION 0.1.0 LANGUAGES CXX)

add_library(plume_engine
    src/engine.cpp
)

target_include_directories(plume_engine PUBLIC include)
target_compile_features(plume_engine PUBLIC cxx_std_20)

if (MSVC)
    target_compile_options(plume_engine PRIVATE /W4 /permissive-)
else()
    target_compile_options(plume_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

option(PLUME_BUILD_TESTS "Build Plume Engine tests" ON)

if (PLUME_BUILD_TESTS)
    enable_testing()
    add_executable(plume_smoke_test tests/smoke_test.cpp)
    target_link_libraries(plume_smoke_test PRIVATE plume_engine)
    add_test(NAME plume_smoke_test COMMAND plume_smoke_test)
endif()
