#pragma once
// ============================================================
// 窗口处理器注册表（配置驱动分发）
// 配置表中的 handlerName 在此注册：渲染 + 内容点击 + 内容hover。
// 新增窗口模块 = 实现处理器函数 + 注册一条，零引擎改动。
// 文本输入不经处理器：输入框统一走 TextInputSystem（WindowManager 成员）。
// ============================================================
#include "WinCommon.h"
#include "WindowInstance.h"
#include <map>
#include <string>

namespace ui {

// 渲染：绘制窗口内容（窗口背景/标题栏/按钮/滚动条由系统统一处理）
using WinRenderFunc = void(*)(RenderDevice* dev, WindowInstance& win, float alpha);
// 内容点击：返回 true = 已消费（命中列表行/按钮等）；False = 未命中（系统继续路由）
using WinClickFunc  = bool(*)(WindowInstance& win, const Vec2& m);
// 内容 hover：更新模块内 hover 态
using WinHoverFunc  = void(*)(WindowInstance& win, const Vec2& m);
// 内容拖拽：click 返回 true 后的鼠标移动（滑条/SV区等跨帧拖动，系统转发）
using WinDragFunc   = void(*)(WindowInstance& win, const Vec2& m);
// 内容释放：拖拽结束（系统转发，复位模块内拖拽标志）
using WinReleaseFunc = void(*)(WindowInstance& win, const Vec2& m);
// 滚轮：鼠标悬停窗口内容时的滚轮事件（下拉列表等模块内自绘滚动容器）；
// 返回 true = 已消费（不穿透到主界面/区域系统）
using WinWheelFunc  = bool(*)(WindowInstance& win, const Vec2& m, float delta);

struct WinHandler {
    WinRenderFunc  render = nullptr;
    WinClickFunc   click = nullptr;
    WinHoverFunc   hover = nullptr;
    WinDragFunc    drag = nullptr;     // 可选：内容拖拽
    WinReleaseFunc release = nullptr;  // 可选：拖拽释放
    WinWheelFunc   wheel = nullptr;    // 可选：滚轮（下拉列表滚动等）
};

class HandlerRegistry {
public:
    void registerHandler(const char* name, const WinHandler& h) { funcs_[name] = h; }
    const WinHandler* find(const char* name) const {
        auto it = funcs_.find(name);
        return it != funcs_.end() ? &it->second : nullptr;
    }
private:
    std::map<std::string, WinHandler> funcs_;
};

extern HandlerRegistry g_handlers;

// 注册全部窗口处理器（引擎初始化时调用一次）
void RegisterAllWindowHandlers();

} // namespace ui