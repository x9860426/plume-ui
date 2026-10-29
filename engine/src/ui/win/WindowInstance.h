#pragma once
// ============================================================
// 窗口运行时实例（替代 FloatingWindow/DockWinInstance/DebugWinInstance）
// 规范：稳定堆对象——LayerStack 用 unique_ptr 持有，堆地址终生不变，
// 排序/扩容只移动指针。跨帧交互状态只存实例 id，不存指针。
// ============================================================
#include "WinCommon.h"
#include "WindowConfig.h"
#include <memory>
#include <string>

namespace ui {

// 悬浮球状态（每窗口一份）
struct FloatBallState {
    bool active = false;       // 悬浮球是否激活（窗口收起为球）
    bool dragging = false;
    bool moved = false;
    float x = 0, y = 0, r = 12.0f;
    int downMouseX = 0, downMouseY = 0;
    float downX = 0, downY = 0;
    std::string label;         // 球上显示的标题
    // ---- 靠边吸附（对照参考 ui_input.cpp L2060-2083 / L235-256） ----
    bool hidden = false;       // 贴边半隐藏态（球心推出屏外只露一半）
    float alpha = 1.0f;        // 球独立透明度（半隐藏 0.5，正常 1.0）
    float idleTime = 0;        // 边缘区静止累计秒（≥3 触发半隐藏）
};

// ====== 窗口运行时实例 ======
struct WindowInstance {
    int id = 0;                          // 实例ID（自动分配，跨帧交互的唯一凭证）
    int configIndex = -1;                // 配置表索引
    float x = 0, y = 0, w = 0, h = 0;    // 当前位置尺寸（像素）
    float alpha = 0.0f;                  // 当前透明度（淡入淡出动画）
    float targetAlpha = 1.0f;
    bool fadingOut = false;              // 淡出中
    bool visible = true;                 // 是否可见
    bool disposed = false;               // 已标记废弃（帧末物理删除）

    // 开关动画（配合 alpha 淡入淡出：创建 0.85 弹出至 1.0，关闭收缩回 0.85；
    // renderWindow 以窗口中心几何缩放实现，0.2 秒与淡入淡出同步）
    float animScale = 1.0f;
    float animTarget = 1.0f;

    // 拖拽/缩放（按下瞬间记录，拖动按 id 现查）
    bool dragging = false;
    int dragOffsetX = 0, dragOffsetY = 0;
    bool resizing = false;
    int resizeOffsetX = 0, resizeOffsetY = 0;

    // 运行时可切换属性
    bool topmost = false;                // 置顶（绘制时最后画）
    bool fixedSize = false;              // 固定大小（隐藏缩放手柄）
    bool collapsed = false;              // 折叠（只留标题栏）
    bool aaDisabled = false;             // 本窗禁用抗锯齿（实例级覆盖全局，服务玩家接入/三方引擎投屏保留原始画质）

    // 悬浮球
    FloatBallState ball;

    // 标题栏按钮 hover（系统统一管理）
    bool hoverClose = false;
    bool hoverPinTop = false;
    bool hoverFixSize = false;
    bool hoverFloatBall = false;
    bool hoverCollapse = false;
    bool hoverResizeGrip = false;
    bool hoverTitleBar = false;

    // 标签选择（系统按 tabId 整组隐藏/显示 Region，扩展代码不写切换逻辑）
    int tabSel = 0;

    // 动态标题覆盖（配置表 dynamicTitle=true 时创建方写入，如"调试 #3"）
    std::string titleOverride;
    // 标题单源：优先动态覆盖，回退配置表标题（渲染/悬浮球标签共用）
    const char* titleText() const {
        const WindowConfig* c = config();
        if (!c) return titleOverride.c_str();
        return titleOverride.empty() ? c->title : titleOverride.c_str();
    }

    // 模块私有状态（各窗口模块通过 getState<T>() 按需创建，零硬编码）
    std::shared_ptr<void> userData;

    // 警告窗口专用（由打开方注入）
    std::string warningTitle = "警告";
    std::string warningMsg = "";

    // 玩家级窗口专用：绑定的场景 context（setSceneContext 整组比对依据）
    int sceneContext = 0;

    // ---- 便利方法 ----
    const WindowConfig* config() const { return FindWindowConfigByIndex(configIndex); }
    bool hasFlag(WindowFlags f) const {
        const WindowConfig* c = config();
        return c && ui::hasFlag(c->flags, f);
    }
    bool isDraggable() const    { return hasFlag(WIN_DRAGGABLE); }
    bool isResizable() const    { return hasFlag(WIN_RESIZABLE) && !fixedSize; }
    bool isFloating() const     { return hasFlag(WIN_FLOATING); }
    bool isFullscreen() const   { return hasFlag(WIN_FULLSCREEN); }
    bool isTransparent() const  { return hasFlag(WIN_TRANSPARENT); }
    bool isPinned() const       { return hasFlag(WIN_PINNED) || topmost; }
    bool isModal() const        { return hasFlag(WIN_MODAL); }
    bool clickThrough() const   { return hasFlag(WIN_CLICK_THROUGH); }
    bool hasTitleBar() const    { return !hasFlag(WIN_NO_TITLE_BAR); }
    bool hasClose() const       { return hasFlag(WIN_HAS_CLOSE); }
    bool hasPinTop() const      { return hasFlag(WIN_HAS_PIN_TOP); }
    bool hasFixSize() const     { return hasFlag(WIN_HAS_FIX_SIZE); }
    bool hasFloatBall() const   { return hasFlag(WIN_HAS_FLOAT_BALL); }
    bool hasCollapse() const    { return hasFlag(WIN_HAS_COLLAPSE); }
    bool hasOverlay() const     { return config() && config()->overlayAlpha > 0.5f; }

    // 实例面板矩形（折叠时只留标题栏高度）
    Rect panelRect(float titleH) const {
        float h2 = collapsed ? titleH : this->h;
        return Rect(Vec2(x, y), Vec2(w, h2));
    }
    bool contains(const Vec2& p, float titleH) const {
        Rect r = panelRect(titleH);
        return p.x >= r.pos.x && p.x <= r.pos.x + r.size.x &&
               p.y >= r.pos.y && p.y <= r.pos.y + r.size.y;
    }

    // 模块私有状态获取（惰性创建）
    template <typename T>
    T& getState() {
        if (!userData) userData = std::make_shared<T>();
        return *std::static_pointer_cast<T>(userData);
    }
    template <typename T>
    T* findState() {
        return userData ? std::static_pointer_cast<T>(userData).get() : nullptr;
    }
};

} // namespace ui