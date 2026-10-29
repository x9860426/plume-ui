#pragma once
// ============================================================
// 应用窗口内容处理器（对齐悬浮窗 HandlerRegistry 的配置驱动思想）
//
// 背景：多应用窗口要求「父子应用窗口、绘制单独、应用窗口名字单独」。
//   每个 AppWindow 的「内容」不再是硬编码的 MainUI，而由内容处理器
//   按名分发——与悬浮窗「配置表 handlerName → HandlerRegistry」同一模式。
//
//   默认 "main" 处理器 = 复用主绘制 MainUI（即「调用主绘制偷懒」）：
//   从任何入口新建的扩展应用窗口，不传内容名时都直接复用主界面绘制，
//   打开即刻呈现完整基础 UI，无需为每个新窗口重写绘制代码。
//
//   扩展（绘制单独）：注册新处理器名，实现 init/resize/update/render/
//   collectText 一套回调，即可让指定应用窗口绘制完全不同内容，
//   应用窗口标题名字仍由 createApp 的 title 参数单独指定。
//
// 处理器为纯函数（无状态）：跨应用共享；每个应用窗口的处理器私有
// 状态保存在 app->contentData()（void*，init 创建 / shutdown 释放）。
// ============================================================
#include <string>

namespace ui {

struct AppWindow;
class RenderDevice;
class Input;

struct AppContentHandler {
    const char* name;                                // 处理器名（snake_case，唯一键）
    void (*init)(AppWindow* app);                    // 初始化：字体/设备就绪后创建视图
    void (*shutdown)(AppWindow* app);                // 析构：释放视图（无状态删除铁律）
    void (*resize)(AppWindow* app, int w, int h);    // 窗口/分辨率适配
    void (*update)(AppWindow* app, Input& in);       // 逻辑更新（含悬浮窗输入路由）
    void (*render)(AppWindow* app, RenderDevice* dev); // 绘制内容（含悬浮窗帧尾时序）
    void (*collectText)(AppWindow* app, std::string& out); // 字体图集预热
};

// ---- 内容处理器注册表（进程全局，跨应用共享） ----
const AppContentHandler* findAppContent(const char* name);   // 找不到返回 nullptr
void registerAppContent(const AppContentHandler& h);
void registerBuiltinAppContents();   // 内置 "main" 等（幂等）

} // namespace ui