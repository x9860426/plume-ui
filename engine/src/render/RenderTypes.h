#pragma once
#include "../core/Math.h"
#include <cstdint>

namespace ui {

struct Vertex {
    Vec2  pos;     // pixel coords, origin top-left, y down
    Vec2  uv;      // texture coords (0..1)
    Color color;   // vertex tint (premultiplied by element color)
};

// 引擎对外后端选择（命令行/快捷键指定）；BackendType 为设备已创建的
// 实际后端。多应用架构后由 AppWindow 使用（原 Engine.h 定义迁此）
enum class Backend : int { OpenGL = 0, Vulkan = 1 };

enum class BackendType : int { OpenGL = 0, Vulkan = 1 };

// Draw a textured (or solid-color) triangle list. UI layer produces these
// commands; backends only need to upload + draw them.
struct DrawCmd {
    uint32_t      textureId = 0;   // 0 == solid color (white texture)
    bool          textured  = false;
    int           indexOffset = 0;
    int           indexCount  = 0;
};

} // namespace ui