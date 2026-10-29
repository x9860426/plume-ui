#pragma once
#include "Math.h"
#include <functional>
#include <string>

namespace ui {

enum class EventKind : int {
    None = 0,
    MouseMove,
    MouseDown,
    MouseUp,
    MouseWheel,
    KeyDown,
    KeyUp,
    Char,
    Resize,
    Close,
};

struct Event {
    EventKind kind = EventKind::None;
    Vec2  mouse;       // window coords (y down)
    int   button = 0;  // 0=left 1=right 2=middle
    int   key    = 0;  // GLFW key code
    int   scancode = 0;
    int   mods   = 0;
    unsigned int codepoint = 0;
    float wheel = 0.f;
    int   width = 0, height = 0;

    bool isMouseButton() const {
        return kind == EventKind::MouseDown || kind == EventKind::MouseUp;
    }
};

using EventCallback = std::function<void(const Event&)>;

} // namespace ui