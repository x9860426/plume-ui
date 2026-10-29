#pragma once
// ============================================================
// 页面处理器注册表（仿 win/HandlerRegistry.h：配置驱动分发）
//
// 主界面每个 tab 页对应一个页面处理器，MainUI 框架只做三件事：
//   1. 渲染期：构造 PageEnv → 转发当前页 handler.render
//   2. 交互期：框架自身控件（tab栏/侧栏/底栏）未消费的输入转发当前页
//      handler.click / hover / drag / release
//   3. 文本输入不经处理器：输入框统一走 TextInputSystem
//      （g_winMgr.textInput()，页面 click 命中时 requestFocus）
//
// 新增页面 = 实现处理器函数 + 注册一条，MainUI 框架零改动。
//
// 依赖方向（防循环包含）：
//   MainUI.h → 本头（框架需要完整 PageEnv/注册表定义）；
//   本头不回指 MainUI.h —— MainUI 只前向声明，PageEnv 全部成员
//   （含 state<T> 模板）只在本头声明，实现分两处：非模板成员在
//   PageHandler.cpp，state<T> 在 MainUI.h 的 MainUI 类之后（类外模板
//   定义——函数体要求 MainUI 完整类型）。
//
// 处理器纪律（对齐悬浮窗 WinHandlers.h）：
//   - 布局单源化：每页一个 Layout 结构 + layout() 函数，render/click/hover 共用
//   - 页面私有状态经 PageEnv::state<T>() 存取（挂 MainUI，每应用独立）
//   - 需要滚动的内容用 g_regions.begin/end + PageEnv::vwin()（虚拟窗口：
//     系统区域滚动/裁剪/滚动条全部自动，页面代码零基础设施编写）
// ============================================================
#include "../win/WinCommon.h"
#include "../win/WindowInstance.h"
#include <map>
#include <string>

namespace ui {

class MainUI;   // 前向声明（完整定义见 MainUI.h）

// ---- 页面环境（框架构造后传给处理器；替代悬浮窗的 WindowInstance& 角色） ----
//   render 期：dev/content/dt 有效
//   交互期：  dev 为 null（click/hover/drag 内禁止绘制）
struct PageEnv {
    MainUI* ui = nullptr;       // 框架（字体/缩放/状态存取；禁止页面反向驱动框架）
    RenderDevice* dev = nullptr;// 渲染设备（render 期有效；交互期为 nullptr）
    Rect content;               // 内容面板可用区（头部 icon/desc 之下，逻辑坐标）
    float dt = 0.0f;            // 帧步长（秒，steady_clock 实测；动画按真实时间推进，与帧率无关）

    // ---- 以下成员实现见 PageHandler.cpp（本头不依赖 MainUI 完整类型） ----

    // 虚拟窗口（RegionSystem 接入点）：页面内容超长时
    //   RegionHandle h = g_regions.begin(dev, vwin(), {相对 content 原点, ...});
    //   ... 绘制（坐标可超出，系统自动裁剪）...
    //   g_regions.end(dev, h, contentH);   // 超出视口 → 系统自动生成滚动容器
    // 滚轮/滚动条/裁剪/tabId 整组隐藏全部系统自动，页面零编写。
    WindowInstance& vwin();

    // 便利取值（框架单源）
    float scale() const;
    Font* font() const;
    Font* smallFont() const;
    Font* bigFont() const;

    // 页面私有状态（惰性创建，按页面索引隔离；每应用独立一份）
    // 模板成员：只声明，定义在 MainUI.h 的 MainUI 类之后（类外模板定义）——
    // 函数体需调用 ui->pageState<T>/pageSel，要求 MainUI 完整类型，
    // 而 ui 的类型 MainUI* 不依赖模板参数（非依赖名，定义点即解析），
    // 故不能在本头内联定义（本头中 MainUI 仅有前向声明）
    template <typename T> T& state();
};

// ---- 页面处理器回调（对照悬浮窗 WinHandler，去掉 wheel/text/blur ——
//      滚轮由 RegionSystem 虚拟窗通路系统处理；文本输入由 TextInputSystem
//      统一管理，页面输入框 click 时 requestFocus 即可 ----
struct PageHandler {
    // 渲染：绘制页面内容（环境 chrome/滚动/裁剪由框架与系统处理）
    void (*render)(PageEnv& env) = nullptr;
    // 内容点击：返回 true = 已消费（进入拖拽态，后续 drag/release 转发本页）
    bool (*click)(PageEnv& env, const Vec2& m) = nullptr;
    // hover 刷新（m = Vec2(-1e9f,-1e9f) 约定为"离开清空"：悬浮窗遮挡/切页/移出面板）
    void (*hover)(PageEnv& env, const Vec2& m) = nullptr;
    // 拖拽：click 消费后的鼠标按住移动（滑条等跨帧拖动，框架转发）
    // 【可选回调】无拖拽控件的页面可不实现，框架判空跳过（对齐悬浮窗
    // WinHandler 派发纪律：八回调按需实现，空缺不调用）
    void (*drag)(PageEnv& env, const Vec2& m) = nullptr;
    // 释放：拖拽结束（框架转发，复位页面内拖拽标志）
    // 【可选回调】同 drag，判空调用；拖拽态复位不受实现与否影响
    void (*release)(PageEnv& env, const Vec2& m) = nullptr;
    // 字体图集预热：页面全部固定文案（collectText 汇总调用）
    void (*collect)(std::string& out) = nullptr;
};

// ---- 注册表（每应用一份，MainUI 持有；注册幂等可重复调用） ----
class PageHandlerRegistry {
public:
    void registerHandler(const char* name, const PageHandler& h) { funcs_[name] = h; }
    const PageHandler* find(const char* name) const {
        auto it = funcs_.find(name);
        return it != funcs_.end() ? &it->second : nullptr;
    }
private:
    std::map<std::string, PageHandler> funcs_;
};

// 注册全部内置页面处理器（MainUI::init 调用；各页面文件暴露 register 函数）
void RegisterAllPageHandlers(PageHandlerRegistry& reg);

} // namespace ui