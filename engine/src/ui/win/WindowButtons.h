#pragma once
// ============================================================
// 统一按钮系统（规范：由 WindowFlags 控制，不按窗口类型分支）
// 标题栏按钮从右到左排列：关闭 → 悬浮球 → 置顶 → 固定大小 → 折叠
// 每个按钮只在 flags 包含对应标志时才检测和绘制。
// 命中矩形计算单源化：绘制 / hover / 点击 全部调用同一下列函数。
// ============================================================
#include "WinCommon.h"
#include "WindowInstance.h"

namespace ui {
namespace wbtn {

// 按钮几何常量（逻辑像素，内部乘 g_uiCtx.scale）
float btnSize(const WindowInstance& win); // 按钮边长（配置表 titleBtnSize：标准22/调试20）
float btnGap();        // 按钮间距 6
float rightMargin();   // 右边距 8
// 标题栏高（配置表 titleBarH 驱动：标准窗36、调试窗32；无配置回退36）
float titleBarHeight(const WindowInstance& win);

// ====== 按钮命中矩形（单源化：所有调用方共享同一计算） ======
Rect closeBtnRect(const WindowInstance& win);     // 关闭
Rect floatBallBtnRect(const WindowInstance& win); // 悬浮球
Rect pinTopBtnRect(const WindowInstance& win);    // 置顶
Rect fixSizeBtnRect(const WindowInstance& win);   // 固定大小
Rect collapseBtnRect(const WindowInstance& win);  // 折叠
Rect resizeGripRect(const WindowInstance& win);   // 右下角缩放手柄

// ====== hover 态更新（系统每帧调用，flags 过滤） ======
void updateHover(WindowInstance& win, const Vec2& m);

// ====== 点击处理（系统按下时调用；返回动作枚举由 WindowManager 执行） ======
enum class BtnAction { None, Close, FloatBall, PinTop, FixSize, Collapse, Resize, DragTitle };
BtnAction handlePress(WindowInstance& win, const Vec2& m);

// ====== 绘制（窗口 chrome 阶段由系统调用） ======
void drawTitleBarButtons(RenderDevice* dev, WindowInstance& win, float alpha);
void drawResizeGrip(RenderDevice* dev, WindowInstance& win, float alpha);

} // namespace wbtn
} // namespace ui