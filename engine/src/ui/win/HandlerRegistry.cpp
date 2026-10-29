// AI生成
#include "HandlerRegistry.h"
#include "WinHandlers.h"

namespace ui {

// 处理器注册表唯一定义（key = 配置表 handlerName，配置驱动分发）
HandlerRegistry g_handlers;

// ============================================================
// 注册全部窗口处理器（引擎初始化时调用一次）
// 新增窗口模块 = 配置表加条目 + 这里加一行注册，零引擎改动
// ============================================================
void RegisterAllWindowHandlers() {
    // 警告（warning）
    g_handlers.registerHandler("handler_warning", {
        renderWarning, clickWarning, hoverWarning });

    // 设置（settings / login / about 复用）
    g_handlers.registerHandler("handler_settings", {
        renderSettings, clickSettings, hoverSettings, dragSettings, releaseSettings });

    // 停靠（dock_lr / dock_tb 共用）
    g_handlers.registerHandler("handler_dock", {
        renderDock, clickDock, hoverDock });

    // 调试（debug，可多实例）
    g_handlers.registerHandler("handler_debug", {
        renderDebug, clickDebug, hoverDebug });

    // 颜色选择器（color_picker；R/G/B/Alpha/Hex 精确输入走 TextInputSystem）
    g_handlers.registerHandler("handler_color_picker", {
        renderColorPicker, clickColorPicker, hoverColorPicker,
        dragColorPicker, releaseColorPicker });

    // 主题工作室（theme_studio；wheel = 下拉列表滚动）
    g_handlers.registerHandler("handler_theme_studio", {
        renderThemeStudio, clickThemeStudio, hoverThemeStudio,
        dragThemeStudio, releaseThemeStudio, wheelThemeStudio });

    // 侧边设置（side_settings）
    g_handlers.registerHandler("handler_side_settings", {
        renderSideSettings, clickSideSettings, hoverSideSettings });

    // 数字键盘（keypad_numeric；按键输入 0-9 . -）
    g_handlers.registerHandler("handler_keypad_numeric", {
        renderKeypadNumeric, clickKeypadNumeric, hoverKeypadNumeric });

    // 身份证输入器（keypad_idcard；0-9 X + 退格）
    g_handlers.registerHandler("handler_keypad_idcard", {
        renderKeypadIdcard, clickKeypadIdcard, hoverKeypadIdcard });

    // 分割器测试（splitter_demo；drag/release = 分割条拖拽写值）
    g_handlers.registerHandler("handler_splitter", {
        renderSplitterDemo, clickSplitterDemo, hoverSplitterDemo,
        dragSplitterDemo, releaseSplitterDemo });

    // 沙盘测试（sandbox_demo；滚轮=缩放，左键拖拽=平移）
    g_handlers.registerHandler("handler_sandbox", {
        renderSandboxDemo, clickSandboxDemo, hoverSandboxDemo,
        dragSandboxDemo, releaseSandboxDemo, wheelSandboxDemo });

    // 悬浮球测试（ball_demo；窗口自带悬浮球按钮，可收成球 / 点击球展开）
    g_handlers.registerHandler("handler_ball_demo", {
        renderBallDemo, clickBallDemo, hoverBallDemo });

    }

} // namespace ui