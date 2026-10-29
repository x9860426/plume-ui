// AI生成
#include "Window.h"
#include "../core/Log.h"
#include <cstring>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

// ---- 平台原生存取（自绘圆角窗口区域；仅 Windows 生效）----
// GLFW_EXPOSE_NATIVE_WIN32 让 glfw3native.h 暴露 glfwGetWin32Window，
// 用于 SetWindowRgn 把无边框窗口外角按「圆润值」裁圆（跨平台时为 no-op）。
#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef NOMINMAX
  #define NOMINMAX
  #endif
  #define GLFW_EXPOSE_NATIVE_WIN32
  #include <GLFW/glfw3native.h>
  #include <windowsx.h>   // GET_X_LPARAM / GET_Y_LPARAM（WM_NCHITTEST 取点）
#endif

namespace ui {

// ---- 自绘标题栏「原生拖动」WndProc 钩子 ----
// 拦截 WM_NCHITTEST：把屏幕坐标转客户端后交 winHitTest 裁定 HT 码。
// 返 0 = 平台层不干预（落回原 WndProc 的默认 HTCLIENT）；其余消息一律
// 经 CallWindowProc 转发给 GLFW 的原 WndProc（GLFW 用 GWLP_USERDATA 存
// 自身句柄，故本窗口实例经 SetPropW 另存，避免踩它的坑）。
#ifdef _WIN32
static LRESULT CALLBACK uiWindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    Window* self = static_cast<Window*>(GetPropW(hwnd, L"ui_window_self"));
    if (self && msg == WM_NCHITTEST) {
        POINT pt;
        pt.x = GET_X_LPARAM(lparam);
        pt.y = GET_Y_LPARAM(lparam);
        if (ScreenToClient(hwnd, &pt)) {
            int r = self->winHitTest((int)pt.x, (int)pt.y);
            if (r != 0) return r;   // 1=HTCLIENT 自绘 / 2=HTCAPTION 系统原生拖动…
        }
    }
    if (self) {
        if (WNDPROC prev = reinterpret_cast<WNDPROC>(self->origWndProc()))
            return CallWindowProcW(prev, hwnd, msg, wparam, lparam);
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}
#endif

// 按窗口分流：GLFW 回调的 window 参数定位本窗口实例，事件入各自队列
static Window* selfOf(GLFWwindow* w) {
    return static_cast<Window*>(glfwGetWindowUserPointer(w));
}

Window::Window(int w, int h, const char* title, bool useVulkan, bool customFrame)
    : customFrame_(customFrame), w_(w), h_(h) {
    // glfwInit 幂等：已初始化时直接返回（多窗口场景安全）
    if (!glfwInit()) {
        UI_LOG_ERROR("glfwInit failed");
        return;
    }
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 4);
    // 无边框自绘：去掉系统标题栏/边框（- 口 X 由引擎自己画）。
    // 该 hint 只作用于创建瞬间，运行期不可变——所以切换边框模式必须
    // 销毁重建窗口（AppWindow 快照重启链路）。
    glfwWindowHint(GLFW_DECORATED, customFrame_ ? GLFW_FALSE : GLFW_TRUE);
    if (useVulkan) {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    } else {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
    }

    // 每窗口独立 GL 上下文（share=nullptr）：应用间彻底隔离，
    // 无跨上下文对象生命周期陷阱；默认单应用行为与旧版完全一致
    glfw_ = glfwCreateWindow(w, h, title, nullptr, nullptr);
    if (!glfw_) {
        UI_LOG_ERROR("glfwCreateWindow failed");
        return;
    }
    glfwSetWindowUserPointer(glfw_, this);
    glfwSetCursorPosCallback(glfw_, onMove);
    glfwSetMouseButtonCallback(glfw_, onMouse);
    glfwSetScrollCallback(glfw_, onWheel);
    glfwSetKeyCallback(glfw_, onKey);
    glfwSetCharCallback(glfw_, onChar);
    glfwSetFramebufferSizeCallback(glfw_, onResize);
    glfwSetWindowCloseCallback(glfw_, onClose);
    glfwSetDropCallback(glfw_, onDrop);
    glfwGetFramebufferSize(glfw_, &w_, &h_);

#ifdef _WIN32
    // 自绘标题栏原生拖动：挂 WndProc 钩子（WM_NCHITTEST 分流 caption/客户区）。
    // 系统边框模式下 AppWindow 不设 hitTest_ → winHitTest 返 0 → 平台层不干预，
    // 行为与未挂钩无异（默认 HTCLIENT），故对所有窗口统一安装。
    {
        HWND hwnd = glfwGetWin32Window(glfw_);
        if (hwnd) {
            SetPropW(hwnd, L"ui_window_self", this);
            origWndProc_ = reinterpret_cast<void*>(SetWindowLongPtrW(
                hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(uiWindowProc)));
        }
    }
#endif
}

Window::~Window() {
#ifdef _WIN32
    if (glfw_ && origWndProc_) {
        HWND hwnd = glfwGetWin32Window(glfw_);
        if (hwnd) {
            SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(origWndProc_));
            RemovePropW(hwnd, L"ui_window_self");
        }
        origWndProc_ = nullptr;
    }
#endif
    if (glfw_) glfwDestroyWindow(glfw_);
}

void Window::terminateGLFW() { glfwTerminate(); }

bool Window::shouldClose() const {
    return glfw_ ? glfwWindowShouldClose(glfw_) != 0 : true;
}

void Window::pollAll() {
    // 全局泵：本轮全部窗口的回调触发，事件按 userPointer 分入各自
    // pending_（互不串扰）。主循环每帧只调一次，之后各窗口 drainEvents。
    glfwPollEvents();
}

void Window::drainEvents(std::vector<Event>& out) {
    if (!glfw_) return;
    // 取走本窗口队列（其余窗口的事件留在各自队列，轮到它们 drain 时取）
    if (!pending_.empty()) {
        out.insert(out.end(), pending_.begin(), pending_.end());
        pending_.clear();
    }
    // F11 全屏切换发生在 pollEvents 之外：glfwSetWindowMonitor 内部同步触发的
    // onResize 回调（事件已入 pending_，此处统一随队列取走）之外，还存在
    // w_/h_ 已更新但事件因窗口失焦等被丢弃的路径——标记补发，走
    // device->resize / setRenderScale / mainUI.resize 统一适配链路
    // （否则全屏后布局/命中测试仍用旧尺寸，交互全部错位失效）
    if (pendingResize_) {
        pendingResize_ = false;
        Event e; e.kind = EventKind::Resize; e.width = w_; e.height = h_;
        out.push_back(e);
    }
}

void* Window::nativeHandle() const { return glfw_; }

void Window::setPos(int x, int y) {
    if (glfw_) glfwSetWindowPos(glfw_, x, y);
}

void Window::setSize(int w, int h) {
    if (!glfw_ || w <= 0 || h <= 0) return;
    glfwSetWindowSize(glfw_, w, h);
    // 回调可能在 glfwSetWindowSize 内同步触发（事件已入 pending_），
    // 也可能迟至下轮 pollEvents——标记补发兜底，双路径不丢 Resize
    pendingResize_ = true;
}

void Window::toggleFullscreen() {
    if (!glfw_) return;
    GLFWmonitor* mon = glfwGetWindowMonitor(glfw_);
    if (mon) {
        // 全屏 → 窗口化：还原进入全屏前的位置与尺寸
        glfwSetWindowMonitor(glfw_, nullptr,
                             restoreX_, restoreY_, restoreW_, restoreH_, 0);
    } else {
        // 窗口化 → 全屏：主显示器当前视频模式；窗口位置尺寸留档用于还原
        glfwGetWindowPos(glfw_, &restoreX_, &restoreY_);
        glfwGetWindowSize(glfw_, &restoreW_, &restoreH_);
        GLFWmonitor* primary = glfwGetPrimaryMonitor();
        if (!primary) return;
        const GLFWvidmode* mode = glfwGetVideoMode(primary);
        if (!mode) return;
        glfwSetWindowMonitor(glfw_, primary, 0, 0,
                             mode->width, mode->height, mode->refreshRate);
    }
    // 尺寸变更的 Resize 回调发生在本函数内（可能因 sink 时序被丢）→ 标记下帧补发
    pendingResize_ = true;
}

// ---- 无边框模式窗口动作（自绘 - 口 X 的统一落点）----

void Window::minimize() {
    if (glfw_) glfwIconifyWindow(glfw_);
}

bool Window::isMaximized() const {
    return glfw_ ? glfwGetWindowAttrib(glfw_, GLFW_MAXIMIZED) != 0 : false;
}

void Window::toggleMaximize() {
    if (!glfw_) return;
    if (isMaximized()) glfwRestoreWindow(glfw_);
    else               glfwMaximizeWindow(glfw_);
    // 最大化/还原改变窗口尺寸：WM 通路可能迟至下轮 pollEvents 才回调，
    // 标记补发兜底（与 setSize 同协议）
    pendingResize_ = true;
}

void Window::setTitle(const char* title) {
    if (glfw_) glfwSetWindowTitle(glfw_, title ? title : "");
}

bool Window::isFullscreen() const {
    return glfw_ ? glfwGetWindowMonitor(glfw_) != nullptr : false;
}

// 应用窗口圆角（自绘模式）：无边框窗口的外角按 radiusPx 裁圆。
// radiusPx<=0 = 直角（清空窗口区域，恢复系统默认矩形）。
// 区域按窗口坐标 → 尺寸变化后须重下发（调用方在 resize 后重发）。
// 成功后区域所有权交系统（勿自行 DeleteObject）；非 Windows 平台 no-op。
void Window::setRoundCorner(int radiusPx) {
#ifdef _WIN32
    if (!glfw_) return;
    HWND hwnd = glfwGetWin32Window(glfw_);
    if (!hwnd) return;
    if (radiusPx <= 0) {
        SetWindowRgn(hwnd, nullptr, TRUE);   // 直角
        return;
    }
    int ww = 0, wh = 0;
    glfwGetWindowSize(glfw_, &ww, &wh);
    if (ww <= 0 || wh <= 0) return;
    HRGN rgn = CreateRoundRectRgn(0, 0, ww + 1, wh + 1, radiusPx * 2, radiusPx * 2);
    if (!rgn) return;
    SetWindowRgn(hwnd, rgn, TRUE);
#else
    (void)radiusPx;
#endif
}

// 自绘边角缩放光标：命中方向 → 标准缩放光标（缓存复用，进程退出随 GLFW 释放）。
void Window::setResizeCursor(int dir) {
    if (!glfw_) return;
    static GLFWcursor* cH = nullptr;
    static GLFWcursor* cV = nullptr;
    if (dir == 0) { glfwSetCursor(glfw_, nullptr); return; }   // 默认箭头
    const bool L = (dir & 1) != 0, R = (dir & 2) != 0;
    const bool T = (dir & 4) != 0, B = (dir & 8) != 0;
    GLFWcursor* want = nullptr;
    if ((L || R) && (T || B)) {
#if GLFW_VERSION_MAJOR > 3 || (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 4)
        static GLFWcursor* cNWSE = nullptr;
        static GLFWcursor* cNESW = nullptr;
        if ((R && B) || (L && T)) {   // ↘ / ↖
            if (!cNWSE) cNWSE = glfwCreateStandardCursor(GLFW_RESIZE_NWSE_CURSOR);
            want = cNWSE;
        } else {                      // ↙ / ↗
            if (!cNESW) cNESW = glfwCreateStandardCursor(GLFW_RESIZE_NESW_CURSOR);
            want = cNESW;
        }
#else
        if (L || R) { if (!cH) cH = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR); want = cH; }
        else        { if (!cV) cV = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR); want = cV; }
#endif
    } else if (L || R) {
        if (!cH) cH = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
        want = cH;
    } else {
        if (!cV) cV = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);
        want = cV;
    }
    glfwSetCursor(glfw_, want);
}

// ---- callbacks（事件按 userPointer 分入本窗口队列） ----

void Window::onMove(GLFWwindow* w, double x, double y) {
    auto* s = selfOf(w);
    if (!s) return;
    Event e; e.kind = EventKind::MouseMove; e.mouse = Vec2(float(x), float(y));
    s->pending_.push_back(e);
}
void Window::onMouse(GLFWwindow* w, int button, int action, int mods) {
    auto* s = selfOf(w);
    if (!s) return;
    double x, y; glfwGetCursorPos(w, &x, &y);
    Event e;
    e.kind = action == GLFW_PRESS ? EventKind::MouseDown : EventKind::MouseUp;
    e.button = button; e.mods = mods; e.mouse = Vec2(float(x), float(y));
    s->pending_.push_back(e);
}
void Window::onWheel(GLFWwindow* w, double /*dx*/, double dy) {
    auto* s = selfOf(w);
    if (!s) return;
    Event e; e.kind = EventKind::MouseWheel; e.wheel = float(dy);
    s->pending_.push_back(e);
}
void Window::onKey(GLFWwindow* w, int key, int sc, int action, int mods) {
    auto* s = selfOf(w);
    if (!s) return;
    Event e;
    // GLFW action 三态：PRESS=按下 REPEAT=按住自动重复 RELEASE=松开。
    // REPEAT 必须按 KeyDown 处理（保持 keyHeld_ 置位）：旧实现
    // `action == PRESS ? KeyDown : KeyUp` 把 REPEAT 误判成 KeyUp，
    // 按住键 500ms 后 keyHeld_ 被错误清零 → keyDown() 状态错乱
    e.kind = (action == GLFW_RELEASE) ? EventKind::KeyUp : EventKind::KeyDown;
    e.key = key; e.scancode = sc; e.mods = mods;
    s->pending_.push_back(e);
    // GLFW char 回调只发可打印字符：回车/退格由 key 回调补充注入字符流
    // （PRESS + REPEAT 均注入，支持按住退格连续删除；输入框统一从 chars() 取）
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        unsigned int cp = 0;
        if (key == 257 /*GLFW_KEY_ENTER*/ || key == 335 /*GLFW_KEY_KP_ENTER*/) cp = 13;
        else if (key == 259 /*GLFW_KEY_BACKSPACE*/) cp = 8;
        if (cp) {
            Event ce; ce.kind = EventKind::Char; ce.codepoint = cp;
            s->pending_.push_back(ce);
        }
    }
}
void Window::onChar(GLFWwindow* w, unsigned int cp) {
    auto* s = selfOf(w);
    if (!s) return;
    Event e; e.kind = EventKind::Char; e.codepoint = cp;
    s->pending_.push_back(e);
}
void Window::onResize(GLFWwindow* w, int wdt, int hgt) {
    auto* s = selfOf(w);
    if (!s) return;
    s->w_ = wdt; s->h_ = hgt;
    Event e; e.kind = EventKind::Resize; e.width = wdt; e.height = hgt;
    s->pending_.push_back(e);
}
void Window::onClose(GLFWwindow* w) {
    auto* s = selfOf(w);
    if (!s) return;
    Event e; e.kind = EventKind::Close;
    s->pending_.push_back(e);
}

// ---------------- 文件拖放 ----------------

void Window::onDrop(GLFWwindow* w, int count, const char** paths) {
    // GLFW 保证 paths 各项以 UTF-8 编码且回调线程 = 主线程；
    // 按 userPointer 分流入本窗口队列（与事件分流同模式）
    if (Window* s = selfOf(w)) {
        for (int i = 0; i < count; i++) {
            if (paths[i] && paths[i][0]) s->dropped_.emplace_back(paths[i]);
        }
    }
}

int Window::droppedFilesCount() const {
    return (int)dropped_.size();
}

int Window::takeDroppedFiles(char* buf, int cap) {
    if (dropped_.empty()) return 0;
    // 所需字节数 = Σ(路径+'\n') - 1 + 结尾 0
    size_t need = 1;
    for (const std::string& s : dropped_) need += s.size() + 1;
    // cap 不足：不消费、返回所需——调用方按返回值扩容后重取（防截断协议）
    if (!buf || cap < (int)need) return (int)need;
    size_t pos = 0;
    for (size_t i = 0; i < dropped_.size(); i++) {
        if (i > 0) buf[pos++] = '\n';   // 路径间 '\n' 分隔（路径本身不含 '\n'）
        std::memcpy(buf + pos, dropped_[i].c_str(), dropped_[i].size());
        pos += dropped_[i].size();
    }
    buf[pos] = '\0';
    dropped_.clear();
    return (int)(pos + 1);
}

} // namespace ui