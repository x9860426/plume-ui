// AI生成
#pragma once
#include "../core/Event.h"
#include <vector>
#include <functional>

struct GLFWwindow;

namespace ui {

// 平台窗口封装（多应用架构：每 AppWindow 各持一个）。
// 事件分流：GLFW 回调按窗口 userPointer 把事件 push 进各自
// pending_ 队列；pollEvents 只取走本窗口的事件（glfwPollEvents
// 为全局泵，一次调用全部窗口的回调都会入各自队列，互不串扰）。
// 每窗口独立 GL 上下文（不共享——彻底隔离，代价是字体图集/
// shader 每应用一份；默认单应用无额外开销）。
class Window {
public:
    // customFrame=true：去掉系统标题栏/边框（GLFW_DECORATED=FALSE），
    // 窗口 chrome 全部由引擎自绘（见 AppWindow 应用标题栏）。
    // 平台层唯一分支点是创建时的 GLFW hint——GL/VK 后端共用同一条
    // 窗口创建通路，双渲染天然一致。
    Window(int w, int h, const char* title, bool useVulkan, bool customFrame = false);
    ~Window();

    bool valid() const { return glfw_ != nullptr; }
    bool shouldClose() const;

    bool customFrame() const { return customFrame_; }

    // ---- 自绘标题栏「原生拖动」（跟随 Windows 系统，不再自写一套拖动）----
    // 平台层挂 WndProc 钩子拦截 WM_NCHITTEST：把客户端像素坐标交给回调裁定，
    // 回调返回 Win32 HT 码（1=HTCLIENT 交引擎自绘；2=HTCAPTION 交系统原生
    // 移动——自带 Aero 贴靠/拖到顶最大化的系统手感）。未设置回调时平台层
    // 不干预（默认 HTCLIENT）。仅无边框自绘模式有意义。
    void setHitTest(std::function<int(int, int)> fn) { hitTest_ = std::move(fn); }
    int  winHitTest(int cx, int cy) const { return hitTest_ ? hitTest_(cx, cy) : 0; }
    void* origWndProc() const { return origWndProc_; }   // 原 WndProc（钩子供转发）

    // ---- 无边框模式下的窗口动作（自绘 - 口 X 的落点）----
    // 系统边框模式下同样可用（GLFW 与 WM 通路一致，双后端无差异）
    void minimize();                       // 最小化
    void toggleMaximize();                 // 最大化 ⇄ 还原
    bool isMaximized() const;              // 当前是否最大化（自绘 □ 双态用）
    void setTitle(const char* title);      // 运行期改标题（自绘标题文字单源）

    // ---- 应用窗口圆角（自绘模式 SetWindowRgn 落地；系统边框模式无效）----
    // >0 = 圆角像素半径；<=0 = 直角（清空区域）。跨平台：非 Windows no-op。
    void setRoundCorner(int radiusPx);
    bool isFullscreen() const;             // 当前是否全屏（圆角须在全屏时让位）

    // ---- 自绘边角缩放的光标反馈（位掩码：1=左 2=右 4=上 8=下；0=默认箭头）----
    // 无边框模式无系统缩放边，缩放热区在客户区内侧由引擎命中；
    // 悬停到边/角时切标准缩放光标（GLFW 标准光标，双后端一致），提升精准手感。
    void setResizeCursor(int dir);

    // Terminates the platform library (GLFW). Call once at process exit;
    // do NOT call per-window, otherwise re-initialization churns the
    // graphics driver and can stall on rapid backend switches.
    static void terminateGLFW();

    // 全局事件泵一次（主循环每帧仅调一次）：所有窗口的回调触发，
    // 事件经 userPointer 分入各自 pending_。多应用窗口务必只泵一次
    // （N 窗口各自调 glfwPollEvents 会 N 倍空转，拖低帧率）。
    static void pollAll();

    // 只取走本窗口队列（不泵全局事件；须先 pollAll），追加到 out。
    // 含 F11 全屏切换的 Resize 补发。
    void drainEvents(std::vector<Event>& out);

    void* nativeHandle() const;   // GLFWwindow*
    GLFWwindow* handle() const { return glfw_; }

    // F11 全屏切换（全屏→主显示器桌面分辨率；还原→回进入前位置尺寸）
    void toggleFullscreen();

    // 多应用窗口级联摆放（新窗口错位显示，避免完全重叠）
    void setPos(int x, int y);

    // 逻辑（屏幕坐标）尺寸调整：glfwSetWindowSize 内部走系统 WM 通路
    // （与用户拖拽边缘同源，framebufferSize 回调驱动完整 Resize 适配
    // 链——设备 resize/渲染缩放/字体重烘焙）。测试验证入口用
    // （--winsize）：绕开外部 SetWindowPos 的 DPI 二次缩放陷阱。
    void setSize(int w, int h);

    int width()  const { return w_; }
    int height() const { return h_; }

    // ---- 文件拖放（glfwSetDropCallback 跨平台；路径 UTF-8） ----
    // 窗口拖入文件累积进队列；业务帧内轮询取走（取走即清空本批）
    int droppedFilesCount() const;                       // 队列内文件数
    // 取走全部路径（'\n' 分隔写入 buf）。
    // cap 不足（含结尾 0）不消费、返回所需字节数——调用方扩容后重取，防截断；
    // 成功返回写入字节数（含结尾 0）；队列为空返回 0
    int takeDroppedFiles(char* buf, int cap);

private:
    static void onMove(GLFWwindow*, double x, double y);
    static void onMouse(GLFWwindow*, int button, int action, int mods);
    static void onWheel(GLFWwindow*, double dx, double dy);
    static void onKey(GLFWwindow*, int key, int sc, int action, int mods);
    static void onChar(GLFWwindow*, unsigned int cp);
    static void onResize(GLFWwindow*, int w, int h);
    static void onClose(GLFWwindow*);
    static void onDrop(GLFWwindow*, int count, const char** paths);

    GLFWwindow* glfw_ = nullptr;
    bool customFrame_ = false;    // 无系统边框（自绘 chrome）模式
    std::function<int(int, int)> hitTest_;   // WM_NCHITTEST 裁定（见 setHitTest）
    void* origWndProc_ = nullptr;            // 钩子链上的原 WndProc（转发用）
    int w_ = 0, h_ = 0;
    int restoreX_ = 0, restoreY_ = 0, restoreW_ = 1280, restoreH_ = 720;  // 全屏前窗口档案
    bool pendingResize_ = false;  // pollEvents 之外的尺寸变更（F11 全屏切换）：下帧 pollEvents 补发 Resize
    std::vector<Event> pending_;  // 本窗口事件队列（GLFW 回调按 userPointer 分流）
    std::vector<std::string> dropped_;  // 拖入文件路径队列（UTF-8；takeDroppedFiles 消费）
};

} // namespace ui