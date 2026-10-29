// AI生成
#pragma once
// ============================================================
// 应用窗口（多应用架构核心，升级于 2026-10-19）
//
// 一个 AppWindow = 一个完整独立的 UI 应用实例：
//   平台窗口(Window) + 渲染设备(RenderDevice) + 输入状态(Input)
//   + 三档字体 + 内容处理器(AppContentHandler，见 AppContent.h)
//   + ★独立悬浮窗架构：LayerStack / WindowManager / RegionSystem / UiContext
//
// 内容处理器（父子应用窗口 / 绘制单独 / 名字单独）：
//   内容不再是硬编码 MainUI，而是按名分发的 AppContentHandler
//   （对齐悬浮窗 HandlerRegistry 的配置驱动思想）。
//   - 默认 "main" = 复用主绘制 MainUI（「调用主绘制偷懒」，打开即完整 UI）
//   - "test" = 子窗口测试内容（居中一行「测试win窗口」，不绘制主界面按钮）
//   - 注册新处理器 → 不同应用窗口绘制不同内容（绘制单独）
//   - 窗口标题名字由 createApp 的 title 单独指定（名字单独）
//
// ★主子级应用窗口（本轮升级）：
//   - createApp 可指定 parentId：子窗口绑定父窗口（父子语义）
//   - 父关子关：AppManager::destroyApp 级联递归销毁全部子孙窗口
//   - 子窗口大小由 createApp(w,h) 单独设置；
//     渲染位置（屏幕摆放）由宿主 setWindowPos 设置（NewAppRequest.x/y
//     显式指定，或默认 = 父窗口位置 + 级联偏移）
//   - 子窗口可禁止悬浮窗（allowSuspendWindows=false）：
//     WindowManager::handleInput / frame 检测 appSuspendWindows() 直接
//     短路 → 该应用内所有悬浮窗/悬浮窗不显示、不交互（快照迁移时若
//     栈内已有悬浮窗，仅本应用内不可见，不影响其他应用）
//   - 父子关系跨重启保持：AppSnapshot 记录 content/allowSuspendWindows/
//     parentIdx（快照数组内下标），F2 切后端 / renderRes 重启后按序重建重绑
//
// 分离语义（用户需求：每个应用窗口支持单独的悬浮窗架构）：
//   - 应用 A 的悬浮窗栈与应用 B 完全隔离，互不可见互不影响
//   - 输入路由(WindowManager)按应用独立：A 拖拽不影响 B
//   - 区域系统(RegionSystem)滚动状态按应用独立
//   - UiContext 尺寸/缩放按应用独立（视觉参数亦独立：设置面板
//     在哪个应用里调，就只影响哪个应用）
//   - 每应用独立 GL 上下文（不共享，彻底隔离零跨上下文陷阱；
//     代价：字体图集/shader 每应用一份，默认单应用无额外开销）
//
// 帧驱动（单线程逐应用）：
//   for app : apps { app->frame(); }   // 内部设置 currentApp
//   所有旧全局单例引用（AppFwd.h 宏）在栈帧内自动指向本应用。
//
// 生命周期请求（帧末由宿主循环统一处理，遍历中不改容器）：
//   shouldClose()          X 按钮/请求关闭 → 帧末销毁（级联子窗口）
//   backendSwitchRequested()  F2 → 全部应用打快照重启切后端
//   resRestartRequested()  renderRes 变更 → 本应用快照重启
//   newAppRequest()        F4/扩展按钮 → 帧末按请求参数新建应用窗口
//
// 快照迁移（AppSnapshot）：悬浮窗实例不含 GL 资源（纯绘制数据），
// 跨上下文/跨后端整体 move 安全——重启不丢用户的悬浮窗布局。
// ============================================================

#include "../core/Input.h"
#include "../platform/Window.h"
#include "../render/RenderDevice.h"
#include "../ui/win/UiContext.h"
#include "../ui/win/LayerStack.h"
#include "../ui/win/WindowManager.h"
#include "../ui/win/RegionSystem.h"
#include "../ui/win/DropdownSystem.h"
#include "../ui/win/SplitterSystem.h"
#include "../ui/win/SandboxSystem.h"
#include "AppContent.h"
#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace ui {

// ====== 新建应用窗口请求（帧末由宿主统一消费，遍历中不改容器铁律） ======
// 语义：业务代码（调试窗按钮/F4/未来任意入口）在帧内任意时刻填好请求
// 调 requestNewApp()；宿主主循环帧末统一 createApp——请求参数完整描述
// 「开一个什么样的窗口」，不硬编码在主循环里（扩展优先）。
struct NewAppRequest {
    bool valid = false;                // 请求有效位（requestNewApp 统一置位；帧首复位）
    int w = 1280, h = 720;             // 窗口大小（子窗口可单独指定）
    int x = -1, y = -1;                // 屏幕渲染位置（-1 = 默认级联偏移；
                                       //   有父 = 父窗口位置+40 偏移，无父 = 80+40*i）
    int parentId = -1;                 // 父应用 id（-1 = 独立窗口；父关子关）
    bool allowSuspendWindows = true;   // 是否允许悬浮窗（子窗口可禁）
    std::string content = "main";      // 内容处理器名（"main"/"test"/...注册表按名分发）
};

// ====== 应用快照：F2 切后端 / renderRes 重启时整体迁移 ======
struct AppSnapshot {
    bool valid = false;
    LayerStack layerStack;    // 悬浮窗栈整体迁移（含待打开队列/悬浮球态）
    RegionSystem regions;     // 区域滚动状态迁移
    DropdownSystem dropdowns; // 下拉展开/滚动状态迁移
    SplitterSystem splitters; // 分割条 hover/拖拽状态迁移
    SandboxSystem sandboxes;  // 沙盘 zoom/pan 状态迁移
    UiContext ctx;            // 视觉参数 + 旧尺寸（新窗口 rescale 基准）
    int winW = 1280, winH = 720;  // 物理窗口尺寸（重建沿用）
    // ---- 主子级属性迁移（重启后保持同内容/同限制/同父子关系） ----
    std::string content = "main";      // 内容处理器名（重建按名分发同内容）
    bool allowSuspendWindows = true;   // 悬浮窗许可（子窗口禁悬浮窗状态保持）
    int parentIdx = -1;                // 父应用在快照数组中的下标（宿主打快照时填；
                                       //   重建按下标顺序先父后子 → 重绑新 id）
    std::string title;                 // 窗口标题（空 = 宿主默认生成）
};

// ====== 应用窗口 ======
class AppWindow {
public:
    ~AppWindow();

    int  id() const { return id_; }
    Backend backend() const { return backend_; }
    const std::string& title() const { return title_; }
    bool shouldClose() const { return closeRequested_; }

    // ---- 主子级属性 ----
    int  parentId() const { return parentId_; }              // -1 = 无父（独立窗口）
    void setParentId(int id) { parentId_ = id; }             // 宿主重建后重绑（父 id 会变）
    bool allowSuspendWindows() const { return allowSuspendWindows_; }  // false = 禁悬浮窗

    // ---- 每应用独立的悬浮窗架构子系统（public：宿主/调试直查） ----
    UiContext      ctx;         // 尺寸/缩放/视觉参数（旧 g_uiCtx）
    LayerStack     layerStack;  // 悬浮窗栈（旧 g_layerStack）
    WindowManager  winMgr;      // 输入路由 + 帧时序（旧 g_winMgr）
    RegionSystem   regions;     // 区域系统（旧 g_regions）
    DropdownSystem dropdowns;   // 下拉选择系统（系统派发：全引擎下拉唯一实现）
    SplitterSystem splitters;   // 分割器系统（系统派发：全引擎分割条唯一实现）
    SandboxSystem sandboxes;    // 沙盘系统（系统派发：可平移可缩放无限画布）

    // ---- 帧驱动（内部：设置 currentApp → 泵事件 → update → render） ----
    void frame();

    // ---- 生命周期请求（宿主帧末处理） ----
    bool backendSwitchRequested() const { return backendSwitch_; }
    bool resRestartRequested() const    { return resRestart_; }
    // 应用窗口边框模式（自绘/系统）切换：GLFW_DECORATED 只在建窗时生效，
    // 运行期改不了 → 宿主快照重启全部应用（与 F2 同一套迁移链路）
    bool frameRestartRequested() const  { return frameRestart_; }
    // 新建窗口请求（电平语义：帧内任意时刻置位 → 帧首复位 → 帧末宿主消费；
    // 默认参数 = 独立 main 窗口，即 F4 快捷键语义）
    const NewAppRequest& newAppRequest() const { return newAppReq_; }
    void requestNewApp(NewAppRequest r = NewAppRequest()) {
        r.valid = true;
        newAppReq_ = std::move(r);
    }
    void requestClose() { closeRequested_ = true; }

    // ---- 快照迁移（重启不丢悬浮窗） ----
    AppSnapshot takeSnapshot();          // 移出（本应用随即销毁；含内容/限制/标题）
    void applySnapshot(AppSnapshot&& s); // 注入新应用（内容 init 前调用）

    // ---- 平台/资源访问（内容处理器基元） ----
    Window& window() { return *window_; }
    Input&  input()  { return input_; }
    RenderDevice* device() { return device_.get(); }
    Font* font()      { return font_.get(); }
    Font* smallFont() { return smallFont_.get(); }
    Font* bigFont()   { return bigFont_.get(); }
    // 逻辑渲染尺寸（renderRes 强制分辨率时查表，0=跟随窗口）
    void logicSize(int& lw, int& lh) const;

    // 内容处理器私有态（AppContent 处理器 init 创建 / shutdown 释放）
    const AppContentHandler* content() const { return content_; }
    void* contentData() const { return contentData_; }
    void  setContentData(void* d) { contentData_ = d; }

    void setWindowPos(int x, int y);            // 屏幕摆放位置（子窗口渲染位置）
    void windowPos(int& x, int& y) const;       // 查询屏幕位置（父级联偏移基准）

    // 按当前逻辑高 + UI 缩放重烘焙三档字体（窗口缩放后保清晰；
    // Resize 后帧内去抖触发；force=true 绕 2px 防抖——UI 缩放离散
    // 提交单次调用，微小档位差也须烘焙到位）
    void refreshFonts(bool force = false);

    // 创建应用窗口（完整初始化：窗口→设备→快照恢复→字体→内容处理器）。
    // content 内容处理器名（默认 "main" 复用主绘制）；snap 非空时从快照恢复；
    // parentId 父应用 id（-1 = 独立）；allowSuspendWindows 是否允许悬浮窗；
    // frameMode 应用窗口边框模式（0=系统边框 1=完全自绘，见 UiContext::windowFrame）。
    // 失败返回 nullptr。
    static AppWindow* createApp(int w, int h, const char* title, Backend backend,
                                AppSnapshot* snap = nullptr,
                                const char* content = "main",
                                int parentId = -1,
                                bool allowSuspendWindows = true,
                                int frameMode = 0);

private:
    AppWindow(int id, int w, int h, const char* title, Backend backend,
              int parentId, bool allowSuspendWindows, int frameMode);

    // ---- 应用级自绘标题栏（frameMode=1 时生效；单一实现，零页面改动） ----
    float appBarH() const;                     // 当前逻辑像素高度（0 = 系统边框模式）
    void  drawAppTitleBar(RenderDevice* dev);  // 底板 + - 口 X + 标题文字
    bool  handleAppChrome();                   // 命中/拖动/双击最大化（先于内容消费）
    // 自绘标题栏原生命中裁定（挂到 Window::setHitTest，经 WM_NCHITTEST 生效）：
    // 客户端像素坐标 → 1=HTCLIENT（自绘按钮/贴边缩放自行处理）/ 2=HTCAPTION
    // （交系统原生拖动：自带 Aero 贴靠、拖到顶最大化的系统手感）。0=不干预。
    int   appChromeHitTest(int cx, int cy) const;

    void initDevice();      // VK 尝试 → GL 回退
    void initFonts();       // 三档字体（fontScale = 逻辑高/720）
    int  id_ = 0;
    Backend backend_;

    std::unique_ptr<Window> window_;
    std::unique_ptr<RenderDevice> device_;
    Input input_;
    // 帧事件缓冲复用（性能优化）：drainEvents 每帧高频调用，
    // 成员 clear() 保容量零堆分配——旧实现每帧局部构造/析构 vector
    std::vector<Event> events_;
    // 字体预热文案缓冲复用（性能优化）：同 events_ 理由，
    // collectText 拼接每帧高频，clear 保容量零堆分配
    std::string allText_;
    std::unique_ptr<Font> font_, smallFont_, bigFont_;
    const AppContentHandler* content_ = nullptr;  // 内容处理器（默认 "main"）
    void* contentData_ = nullptr;                 // 处理器私有视图实例
    std::string title_;

    // 主子级属性（-1 = 无父；false = 禁悬浮窗）
    int  parentId_ = -1;
    bool allowSuspendWindows_ = true;

    // 渲染缩放（逻辑分辨率 → 实际窗口）
    float devSx_ = 1.0f, devSy_ = 1.0f;

    // 字体动态重烘焙（Resize 置 dirty + 记时戳；渲染前距末次 Resize ≥250ms
    // 才重建——拖拽边缘每帧 Resize 不触发 pack+上传，松手后一次转清晰）
    bool fontsDirty_ = false;
    std::chrono::steady_clock::time_point lastResizeT_;

    // UI 缩放烘焙对齐（initFonts 末尾置初值；frame 内检测 ctx.uiScaleUser
    // 变化 → 立即 force 重烘焙：字号密度含 uiScale，密度不跟上则 UI 缩放
    // 只能顶点拉伸旧位图 → 糊（"字体没有缩放"根因））
    float lastUiScaleApp_ = 1.0f;

    // 生命周期请求标记（帧首复位 + 帧末由宿主处理）
    bool closeRequested_ = false;
    bool backendSwitch_  = false;
    bool resRestart_     = false;
    bool frameRestart_   = false;   // 边框模式切换（全部应用快照重启）
    NewAppRequest newAppReq_;    // 新窗口请求（valid=true 才有待消费请求）

    // ---- 自绘标题栏拖动态（frameMode=1；绝对屏坐标锚定，防每帧回弹闪烁） ----
    // 自绘标题栏拖动改由 Windows 系统原生接管（Window::setHitTest →
    // WM_NCHITTEST 返 HTCAPTION），不再自写拖动状态机（原 barDragging_* /
    // lastBarClickT_ 已移除）。

    // ---- 应用窗口圆角（自绘模式 SetWindowRgn；值/宽高/最大化态 变化才重下发）----
    int lastRoundPx_ = -1, lastRoundW_ = -1, lastRoundH_ = -1;

    // ---- 自绘边框缩放（frameMode=1；无系统边框后由引擎接管 8 向缩放）----
    int   barResizeDir_ = 0;                 // 位掩码 1左 2右 4上 8下（0=未在缩放）
    float barResizeMouseX_ = 0, barResizeMouseY_ = 0;   // 按下时窗口内逻辑坐标
    int   barResizeWinX_ = 0, barResizeWinY_ = 0;        // 按下时窗口屏幕矩形
    int   barResizeWinW_ = 0, barResizeWinH_ = 0;
    float barResizeDevX_ = 1.0f, barResizeDevY_ = 1.0f;  // 按下时 物理/逻辑 比（锚定）
    int   barResizeLastX_ = 0, barResizeLastY_ = 0, barResizeLastW_ = 0, barResizeLastH_ = 0;
    bool  barResizeValid_ = false;

    // FPS 统计
    int frameCount_ = 0;
    std::chrono::steady_clock::time_point lastFpsTime_;
    // 每秒帧耗时分布（性能观测：日志输出 avg/min/max，定位波动尖峰来源）
    float secMinMs_ = 1e9f, secMaxMs_ = 0.f, secSumMs_ = 0.f;
    int   secN_ = 0;
};

// ====== 应用管理器（多应用容器 + 当前应用指针） =====
class AppManager {
public:
    // 创建并接管一个应用窗口（转发 AppWindow::createApp）
    static AppWindow* createApp(int w, int h, const char* title, Backend backend,
                                AppSnapshot* snap = nullptr,
                                const char* content = "main",
                                int parentId = -1,
                                bool allowSuspendWindows = true,
                                int frameMode = 0);

    static void destroyApp(AppWindow* app);            // 帧末安全销毁（父级联子）
    static void destroyAll();                          // 进程退出清理
    static const std::vector<std::unique_ptr<AppWindow>>& apps();
    static AppWindow* current() { return current_; }
    static void setCurrent(AppWindow* a) { current_ = a; }
    static AppWindow* first();                         // 首应用（兜底/空判）
    static AppWindow* findById(int id);                // 按 id 查找（父子绑定校验）
    static int  count();
    static int  nextAppId();                           // 应用 id 分配器（现存最大 id+1，防复用冲突）

private:
    static std::vector<std::unique_ptr<AppWindow>> apps_;
    static AppWindow* current_;
};

} // namespace ui