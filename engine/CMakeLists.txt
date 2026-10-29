# AI生成
cmake_minimum_required(VERSION 3.16)
project(uiengine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(UIENGINE_BUILD_VULKAN "Build Vulkan backend (requires Vulkan SDK)" ON)

# ============================================================
# 第三方依赖：离线复用 py 版 FetchContent 缓存（网络不通）
# 缓存含 glfw-src / stb-src / nlohmann_json-src 完整源码
# glad 产物已复制到 third_party/glad/（离线生成的 gl.c + glad/gl.h）
# ============================================================
set(DEPS_CACHE "${CMAKE_CURRENT_SOURCE_DIR}/third_party" CACHE PATH "Third-party deps cache")

# ---- GLFW（本地源码 add_subdirectory）----
set(GLFW_BUILD_DOCS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS   OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL       OFF CACHE BOOL "" FORCE)
add_subdirectory(${DEPS_CACHE}/glfw-src ${CMAKE_BINARY_DIR}/glfw-build EXCLUDE_FROM_ALL)

# ---- glad2（离线生成的 gl:core=3.3 加载器）----
add_library(glad_gl_core STATIC third_party/glad/src/gl.c)
target_include_directories(glad_gl_core PUBLIC third_party/glad/include)

# ---- stb / nlohmann::json（header-only，仅 include 路径）----

set(UIENGINE_SOURCES
    src/app/AppContent.cpp
    src/app/AppWindow.cpp
    src/core/Assets.cpp
    src/core/Event.cpp
    src/core/Input.cpp
    src/core/Log.cpp
    src/platform/Window.cpp
    src/render/RenderThread.cpp
    src/render/ThreadScheduler.cpp
    src/render/gl/GLDevice.cpp
    src/render/vk/VKDevice.cpp
    src/ui/Background.cpp
    src/ui/DynamicBg.cpp
    src/ui/Element.cpp
    src/ui/Font.cpp
    src/ui/MainUI.cpp
    src/ui/Theme.cpp
    src/ui/page/PageControls.cpp
    src/ui/page/PageHandler.cpp
    src/ui/page/Pages_Directory.cpp
    src/ui/page/Pages_Options.cpp
    src/ui/page/Pages_Overview.cpp
    src/ui/page/Pages_Sandbox.cpp
    src/ui/page/Pages_Splitter.cpp
    src/ui/win/DropdownSystem.cpp
    src/ui/win/HandlerRegistry.cpp
    src/ui/win/LayerStack.cpp
    src/ui/win/RegionSystem.cpp
    src/ui/win/SandboxSystem.cpp
    src/ui/win/SplitterSystem.cpp
    src/ui/win/TextInputSystem.cpp
    src/ui/win/WinControls.cpp
    src/ui/win/WinHandlers_Basic.cpp
    src/ui/win/WinHandlers_BallDemo.cpp
    src/ui/win/WinHandlers_ColorPicker.cpp
    src/ui/win/WinHandlers_Keypad.cpp
    src/ui/win/WinHandlers_Sandbox.cpp
    src/ui/win/WinHandlers_Splitter.cpp
    src/ui/win/WinHandlers_ThemeStudio.cpp
    src/ui/win/WindowButtons.cpp
    src/ui/win/WindowManager.cpp)

add_library(uiengine STATIC ${UIENGINE_SOURCES})
target_include_directories(uiengine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${DEPS_CACHE}/stb-src
    ${DEPS_CACHE}/nlohmann_json-src/single_include)
target_link_libraries(uiengine PUBLIC glfw glad_gl_core)

if(UIENGINE_BUILD_VULKAN)
    find_package(Vulkan QUIET)
    if(Vulkan_FOUND)
        target_compile_definitions(uiengine PUBLIC UIENGINE_HAS_VULKAN=1)
        target_link_libraries(uiengine PUBLIC Vulkan::Vulkan)
    else()
        message(WARNING "Vulkan SDK not found; Vulkan backend disabled (stub only)")
    endif()
endif()

if(MSVC)
    target_compile_options(uiengine PRIVATE /utf-8)
endif()

add_executable(uiengine_demo src/main.cpp)
target_link_libraries(uiengine_demo PRIVATE uiengine)
if(MSVC)
    target_compile_options(uiengine_demo PRIVATE /utf-8)
endif()
if(MINGW)
    target_link_options(uiengine_demo PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_custom_command(TARGET uiengine_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets $<TARGET_FILE_DIR:uiengine_demo>/assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/themes $<TARGET_FILE_DIR:uiengine_demo>/themes
    COMMENT "Copying assets/themes to output directory")