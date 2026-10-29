// AI生成
#pragma once
// ============================================================
// 应用窗口前向声明 + 旧全局单例转发宏（多应用架构核心）
//
// 背景：引擎升级多应用窗口后，原全局单例（g_uiCtx/g_layerStack/
// g_winMgr/g_regions）变为「每个应用窗口一份」的成员。为使
// win/ 模块全部处理器代码零改动（全仓 450+ 处引用），此处用宏
// 把旧单例名转发到「当前正在驱动/渲染的应用」的对应成员。
//
// 语义保证（单线程逐应用驱动，见 AppWindow::frame）：
//   主循环每帧 g_currentApp = 待驱动应用 → 该应用栈帧内所有
//   update/render/click/hover 调用中，宏取到的都是本应用数据。
//   应用 A 的悬浮窗栈/区域系统/输入路由与应用 B 完全分离。
//
// g_handlers 保持真全局（处理器注册表为纯函数表，无状态，
// 跨应用共享——新增窗口模块注册一次全部应用可用）。
// ============================================================

namespace ui {

class AppWindow;
struct UiContext;
class LayerStack;
class WindowManager;
class RegionSystem;
class DropdownSystem;
class SplitterSystem;
class SandboxSystem;

// 当前正在驱动/渲染的应用窗口（帧内单值；应用栈帧外调用转发宏
// 属于逻辑错误，实现侧以首应用兜底并在 Debug 下可断言）
AppWindow* currentApp();

// ---- 旧全局单例的成员访问器（实现见 AppWindow.cpp） ----
UiContext&     appCtx();
LayerStack&    appLayerStack();
WindowManager& appWinMgr();
RegionSystem&  appRegions();
DropdownSystem& appDropdowns();
SplitterSystem& appSplitters();
SandboxSystem& appSandboxes();

// 当前应用是否禁止悬浮窗显示（子应用窗口限制项：
// 从调试面板打开的扩展窗口默认禁止所有悬浮窗）——实现在 AppWindow.cpp
bool appSuspendWindows();

// 当前应用后端是否为 Vulkan（OpenGL 不支持多线程，主题工作室
// 线程模式下拉据此动态决定可选项数）——实现在 AppWindow.cpp
bool appBackendIsVulkan();

// Vulkan 后端是否可用（编译期判定 UI_VK_ON：未装 Vulkan SDK 时后端为
// stub，不可用）。F2 切后端据此判定「目标后端是否可达」——不可达则不切，
// 避免切到 stub 后端（Vulkan 窗口无 GL 上下文 → 初始化失败 → 崩溃）
bool appVulkanAvailable();

} // namespace ui

// ---- 旧单例名转发（处理器/控件/主界面代码零改动） ----
#define g_uiCtx      (::ui::appCtx())
#define g_layerStack (::ui::appLayerStack())
#define g_winMgr     (::ui::appWinMgr())
#define g_regions    (::ui::appRegions())
#define g_dropdowns  (::ui::appDropdowns())
#define g_splitters  (::ui::appSplitters())
#define g_sandboxes   (::ui::appSandboxes())