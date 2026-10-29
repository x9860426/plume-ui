// AI生成
#pragma once
// ============================================================
// 主界面页面共享控件库（pctl，对照悬浮窗 wctl 的定位）
//
// 职责边界：
//   - wctl（win/WinControls）：悬浮窗内容控件（drawBtn/drawCheck/drawSlider…）
//   - pctl（本文件）：主界面框架 + 页面额外需要的控件原语——
//     tab 项/侧栏项/分段项三态、输入框/下拉/步进器、聚焦光晕、
//     环形/条形进度、三风格开关、完整滑条
//
// 纪律（对齐悬浮窗规范）：
//   - 控件只负责"画"，交互路由由 MainUI 框架统一派发到页面处理器
//   - 全部主题色 + g_uiCtx 视觉参数单源（厚度/圆角/阴影零自存）
//   - 几何单源：下拉列表矩形/列表项矩形各只有一个函数（页面绘制
//     与命中测试共用，根治原 MainUI 三处重复计算）
// ============================================================
#include "../win/WinControls.h"
#include <string>

namespace ui {

namespace pctl {

// ====== 三态常量（本库统一约定） ======
//   STATE_NORMAL = 普通 / STATE_HOVER = 悬停 / STATE_ACTIVE = 选中或聚焦
static constexpr int STATE_NORMAL = 0;
static constexpr int STATE_HOVER  = 1;
static constexpr int STATE_ACTIVE = 2;

// ====== 文本排版（原 MainUI 私有三件套迁出，全页面共用） ======
// 矩形内垂直居中；对齐方式：Centered 居中 / Left 左对齐 / Right 右对齐
void drawTextCentered(RenderDevice* dev, Font* f, const char* utf8,
                      const Rect& r, const Color& c);
void drawTextLeft(RenderDevice* dev, Font* f, const char* utf8,
                  const Rect& r, const Color& c);
void drawTextRight(RenderDevice* dev, Font* f, const char* utf8,
                   const Rect& r, const Color& c);


// ====== tab 项（顶部圆角；对照原 MainUI::drawTabBar 三分支 L203-226） ======
void drawTabItem(RenderDevice* dev, const Rect& r, const char* utf8,
                 int state, Font* f);

// ====== 侧栏项（胶囊形；对照原 drawSidePanel 三分支 L244-262） ======
void drawSideItem(RenderDevice* dev, const Rect& r, const char* utf8,
                  int state, Font* f);

// ====== 分段按钮项（胶囊形；对照原 drawSegmented 三分支 L447-460） ======
void drawSegItem(RenderDevice* dev, const Rect& r, const char* utf8,
                 int state, Font* f);

// ====== 输入框底（对照原 drawEditableInput 边框三态 L295-306） ======
// state: 0普通(inputBorder@120) / 1hover(@210) / 2聚焦(accent + 1.6倍线宽)
void drawInputBox(RenderDevice* dev, const Rect& r, int state);


// ====== 步进器（三键一体绘制；对照原 drawStepper L393-429） ======
// minus/val/plus 三个矩形 + 当前值文字 + ±号（hover 高亮 accent）
// 数值区 = 输入框样式（系统级统一）：左对齐数值 + 焦点边框高亮 + 光标。
// valFocused/valCaretOn/valCaretByte：由拥有该步进器的页面传入（走 TextInputSystem）；
//   默认全 false/-1 → 旧调用零回归（仅静态数值，无光标）。
void drawStepper(RenderDevice* dev, const Rect& minus, const Rect& val,
                 const Rect& plus, const char* valText,
                 bool hoverMinus, bool hoverPlus, Font* f, Font* bigF,
                 bool valFocused = false, bool valCaretOn = false,
                 int valCaretByte = -1, bool valHover = false);

// ====== 聚焦光晕（按钮 focus 态共用；对照原 L492-495 外扩光晕+描边环） ======
// glowA/ringA 为 0~1 透明度；radius 为按钮圆角（环自动外扩 3px）
void drawFocusGlow(RenderDevice* dev, const Rect& r,
                   float glowA, float ringA, float ringW, float radius);

// ====== 渐变弧（颜色沿扫过角度 c1→c2；对照原 static drawGradientArc L554-571） ======
void drawGradientArc(RenderDevice* dev, float cx, float cy, float radius,
                     float penW, float startAngleDeg, float sweepDeg,
                     const Color& c1, const Color& c2);

// ====== 环形进度（对照原 drawCircularProgress L573-613） ======
// loading=true 旋转加载弧（60°扫过） / false 进度弧（0→100% 循环）
// animTime 由页面 State 累加（render 期 env.dt 推进）
// hint 非空时在环下方绘制小字提示
void drawCircularProgress(RenderDevice* dev, const Rect& r, bool loading,
                          float animTime, Font* f, Font* bigF, Font* smallF,
                          const char* hint);

// ====== 条形进度（对照原 drawProgressBar L615-639） ======
// label 左上 + 百分比右上 + 圆角轨道 + accent→accent2 渐变填充
void drawProgressBar(RenderDevice* dev, const Rect& r, float t,
                     const char* label, Font* smallF);

// ====== 开关（对照原 drawToggleSwitch L683-752） ======
// 风格/扩展图案全部由 g_uiCtx.switchStyle/switchExtra 单源驱动，
// 系统圆润(0)/方形(1)/半圆润(2)；>2 = 「特别绘制」族（内部 3..4 = 游戏ID 4..5：机械/渐变），
// 委托 wctl::drawToggleStyled 统一绘制（on/off 双态）。
void drawToggle(RenderDevice* dev, const Rect& r, bool on);

// ====== 完整滑条（对照原 drawSlider L641-681；主界面音量条标准外观） ======
// sliderStyle=0：自绘槽(8px)+渐变填充+圆形手柄+边框；
// 非 0：委托 wctl::drawSlider（霓虹/竖柄/描边/像素 4 变体统一入口）
// zoom：沙盘等局部缩放场景传视口倍率（默认 1 = 全局标准外观）；
// 槽/手柄/圆角/边框整体 ×zoom 与 groove 矩形同比例，缩放后外观不失调。
void drawSliderFull(RenderDevice* dev, const Rect& groove, float t, bool hover,
                    float zoom = 1.0f);

} // namespace pctl
} // namespace ui