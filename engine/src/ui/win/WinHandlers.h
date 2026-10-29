// AI生成
#pragma once
// ============================================================
// 窗口处理器声明（配置表 handlerName 对应的全部实现）
//
// 处理器规范（规范 §一/§九）：
//   - 只画窗口内容区（底板/标题栏/按钮/滚动条/裁剪由系统统一处理）
//   - 布局单源化：每个模块一个 layout() 函数，render/hover/click
//     三处共用同一次几何计算（根治旧架构"三处重复"问题）
//   - 模块私有状态用 win.getState<T>() 存取（零引擎改动）
//   - 需要滚动的内容用 RegionSystem 申请区域（系统自动滚动容器）
//   - 打开新窗口用 g_layerStack.pendingOpen（帧末统一创建）
// ============================================================
#include "WinCommon.h"
#include "WindowInstance.h"
#include "TextInputSystem.h"   // EditText（键盘窗绑定目标类型）

namespace ui {

// ---- 警告窗口（handler_warning） ----
void renderWarning(RenderDevice* dev, WindowInstance& win, float alpha);
bool  clickWarning(WindowInstance& win, const Vec2& m);
void  hoverWarning(WindowInstance& win, const Vec2& m);

// ---- 设置窗口（handler_settings，login/about 复用） ----
void renderSettings(RenderDevice* dev, WindowInstance& win, float alpha);
bool  clickSettings(WindowInstance& win, const Vec2& m);
void  hoverSettings(WindowInstance& win, const Vec2& m);
void  dragSettings(WindowInstance& win, const Vec2& m);
void  releaseSettings(WindowInstance& win, const Vec2& m);

// ---- 停靠窗口（handler_dock，dock_lr/dock_tb 共用） ----
void renderDock(RenderDevice* dev, WindowInstance& win, float alpha);
bool  clickDock(WindowInstance& win, const Vec2& m);
void  hoverDock(WindowInstance& win, const Vec2& m);

// ---- 调试窗口（handler_debug，可多实例） ----
void renderDebug(RenderDevice* dev, WindowInstance& win, float alpha);
bool  clickDebug(WindowInstance& win, const Vec2& m);
void  hoverDebug(WindowInstance& win, const Vec2& m);
// 调试日志收集（业务方随时调用，所有调试窗口共享展示）
void AddDebugLog(const char* utf8);

// ---- 颜色选择器（handler_color_picker） ----
void renderColorPicker(RenderDevice* dev, WindowInstance& win, float alpha);
bool  clickColorPicker(WindowInstance& win, const Vec2& m);
void  hoverColorPicker(WindowInstance& win, const Vec2& m);
void  dragColorPicker(WindowInstance& win, const Vec2& m);
void  releaseColorPicker(WindowInstance& win, const Vec2& m);
// R/G/B/Alpha/Hex 精确输入：统一走 TextInputSystem（g_winMgr.textInput()），
// click 命中值框时 requestFocus，提交解析在 EditText::onCommit

// ---- 数字键盘（handler_keypad_numeric；0-9 . - + 退格/清空） ----
void renderKeypadNumeric(RenderDevice* dev, WindowInstance& win, float alpha);
bool  clickKeypadNumeric(WindowInstance& win, const Vec2& m);
void  hoverKeypadNumeric(WindowInstance& win, const Vec2& m);

// ---- 身份证输入器（handler_keypad_idcard；0-9 X + 退格，上限 18 位） ----
void renderKeypadIdcard(RenderDevice* dev, WindowInstance& win, float alpha);
bool  clickKeypadIdcard(WindowInstance& win, const Vec2& m);
void  hoverKeypadIdcard(WindowInstance& win, const Vec2& m);

// 键盘窗目标输入框绑定（页面输入框点击时注入）：
// idMode = false 数字键盘 / true 身份证输入器；ownerPageId = 页面索引。
// 绑定后键盘显示区读该框缓冲、按键直接写回同一缓冲，并在按键后重建焦点，
// 保证物理键盘可继续输入；未绑定时键盘窗退回自有缓冲（独立打开行为不变）。
void bindKeypadTarget(bool idMode, EditText* t, int ownerPageId);

// ---- 主题工作室（handler_theme_studio） ----
void renderThemeStudio(RenderDevice* dev, WindowInstance& win, float alpha);
bool  clickThemeStudio(WindowInstance& win, const Vec2& m);
void  hoverThemeStudio(WindowInstance& win, const Vec2& m);
void  dragThemeStudio(WindowInstance& win, const Vec2& m);
void  releaseThemeStudio(WindowInstance& win, const Vec2& m);
bool  wheelThemeStudio(WindowInstance& win, const Vec2& m, float delta);


// ---- 侧边设置（handler_side_settings） ----
void renderSideSettings(RenderDevice* dev, WindowInstance& win, float alpha);
bool  clickSideSettings(WindowInstance& win, const Vec2& m);
void  hoverSideSettings(WindowInstance& win, const Vec2& m);

// ---- 分割器测试（handler_splitter，SplitterSystem 系统派发样板） ----
void renderSplitterDemo(RenderDevice* dev, WindowInstance& win, float alpha);
bool  clickSplitterDemo(WindowInstance& win, const Vec2& m);
void  hoverSplitterDemo(WindowInstance& win, const Vec2& m);
void  dragSplitterDemo(WindowInstance& win, const Vec2& m);
void  releaseSplitterDemo(WindowInstance& win, const Vec2& m);

// ---- 沙盘测试（handler_sandbox，SandboxSystem 系统派发样板） ----
void renderSandboxDemo(RenderDevice* dev, WindowInstance& win, float alpha);
bool  clickSandboxDemo(WindowInstance& win, const Vec2& m);
void  hoverSandboxDemo(WindowInstance& win, const Vec2& m);
void  dragSandboxDemo(WindowInstance& win, const Vec2& m);
void  releaseSandboxDemo(WindowInstance& win, const Vec2& m);
bool  wheelSandboxDemo(WindowInstance& win, const Vec2& m, float delta);

// ---- 颜色选择器目标注入（主题工作室 → 颜色选择器，模块间参数传递） ----
// 三种绑定方式（互斥，后写覆盖）：主题工作室点击色块时调用其一，
// 随后 pendingOpen 选择器；帧末 openPending 创建后由 PollColorPickerBinding
// 完成绑定（一次性同步 HSV + 停靠定位）。
//   成员指针版：兼容任意字段直绑
void BindColorPickerTarget(int themeIndex, Color ThemeColors::*field);
//   55 色项表索引版：主题色「详细」子模式的色块（itemIndex → 成员指针查表）
void BindColorPickerThemeItem(int themeIndex, int itemIndex);
//   自定义背景颜色槽位版（0=纯色 1-4=渐变四角 5=网格底色 6=网格线色）
void BindColorPickerBgSlot(int slot);
// 打开位置停靠：参考窗口右侧（越屏翻左、再越屏居中），仅新建/淡入期生效
void SetColorPickerSpawnNear(int refWinId);
void PollColorPickerBinding();
// 55 色项表共享访问：itemIndex → ThemeColors 成员指针（越界返回 nullptr）
// 表定义在主题工作室实现内，颜色选择器/其他模块经此复用（单源零复制）
Color ThemeColors::*ColorItemMember(int itemIndex);

// ---- 悬浮球测试（handler_ball_demo，LayerStack 悬浮球系统派发样板） ----
void renderBallDemo(RenderDevice* dev, WindowInstance& win, float alpha);
bool  clickBallDemo(WindowInstance& win, const Vec2& m);
void  hoverBallDemo(WindowInstance& win, const Vec2& m);

} // namespace ui