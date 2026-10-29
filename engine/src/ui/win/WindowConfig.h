// AI生成
#pragma once
// ============================================================
// 窗口配置表（零硬编码核心）
// 规范铁律：所有窗口由配置表声明，引擎不写死任何窗口类型。
// 新增窗口 = 追加一条 WINDOW_CONFIGS 条目 + 注册渲染处理器。
// 禁止任何 if(config->id == "xxx") 式分支。
// ============================================================
#include <cstdint>
#include <cstring>
#include <deque>

namespace ui {

// ====== 层级（渲染顺序：数字大的层画在上面） ======
// LayerStack 按此分层渲染/命中测试；配置表 layer 字段声明归属层。
// 对 Lua 暴露的编号（玩家心智模型）：
//   -1 = 玩家级（LAYER_PLAYER）
//    0 = 开发级（LAYER_DEV）
//    1 = 警告级（LAYER_GLOBAL）
enum LayerId {
    LAYER_SCENE  = 0,  // 场景底：游戏画面/主视图（最先绘制，不参与浮窗命中）
    LAYER_PLAYER = 1,  // 玩家级（Lua 插件悬浮窗）：绑定场景 context，
                        //   切场景整组隐藏、切回重现（显示/隐藏语义，非销毁，
                        //   实例/位置/状态全保留）；层内可叠放（bringToFront 有效），
                        //   但永远压不过开发级/警告级（层序硬保证）
    LAYER_DEV    = 2,  // 开发级：一般 Lua / 硬编码工具窗（当前大部分悬浮窗）
    LAYER_GLOBAL = 3,  // 警告级：全局警告，永远最上
    LAYER_COUNT  = 4,
};

// ====== 关闭策略 ======
enum ClosePolicy {
    CLOSE_BUTTON_ONLY   = 0,  // 仅关闭按钮/快捷键关闭
    CLOSE_OUTSIDE_CLICK = 1,  // 点击窗口外部也关闭（仅栈顶生效）
    CLOSE_MODAL         = 2,  // 模态：外部点击被遮罩吸收
};

// ====== 窗口属性位标志（替代 FloatWinType 枚举） ======
enum WindowFlags : uint64_t {
    WIN_NONE            = 0,
    WIN_DRAGGABLE       = 1ULL << 0,   // 可拖拽（标题栏）
    WIN_RESIZABLE       = 1ULL << 1,   // 可缩放（右下角手柄）
    WIN_FLOATING        = 1ULL << 2,   // 悬浮窗样式（圆角+阴影+内边距）
    WIN_FULLSCREEN      = 1ULL << 3,   // 全屏
    WIN_TRANSPARENT     = 1ULL << 4,   // 透明背景（不画窗口底板，内容全自绘）
    WIN_NO_TITLE_BAR    = 1ULL << 5,   // 无标题栏
    WIN_AUTO_ZORDER     = 1ULL << 6,   // 聚焦即置顶
    WIN_CLICK_THROUGH   = 1ULL << 7,   // 点击穿透
    WIN_PINNED          = 1ULL << 8,   // 固定最上层（最后绘制）
    WIN_HAS_CLOSE       = 1ULL << 9,   // 显示关闭按钮
    WIN_HAS_PIN_TOP     = 1ULL << 10,  // 显示置顶按钮
    WIN_HAS_FIX_SIZE    = 1ULL << 11,  // 显示固定大小按钮
    WIN_HAS_FLOAT_BALL  = 1ULL << 12,  // 显示悬浮球按钮
    WIN_HAS_COLLAPSE    = 1ULL << 13,  // 显示折叠按钮
    WIN_MODAL           = 1ULL << 14,  // 模态窗口（带全屏遮罩，下方窗口不可交互）
    WIN_GAME_VIEW       = 1ULL << 15,  // 游戏画面窗口
    WIN_DOCK_HORIZONTAL = 1ULL << 16,  // 水平停靠窗口
    WIN_DOCK_VERTICAL   = 1ULL << 17,  // 垂直停靠窗口
    WIN_SIDE_TABS       = 1ULL << 18,  // 侧边标签栏
    WIN_NO_SCALE_SYNC   = 1ULL << 19,  // 窗口尺寸不跟随系统缩放
    WIN_MULTI_INSTANCE  = 1ULL << 20,  // 允许多实例（默认单例：已存在则置顶复显）
};

inline bool hasFlag(uint64_t flags, WindowFlags f) { return (flags & (uint64_t)f) != 0; }
inline uint64_t operator|(WindowFlags a, WindowFlags b) { return (uint64_t)a | (uint64_t)b; }

// ====== 四角独立圆角（规范：不强制四角统一，0 = 直角） =====
struct CornerRadii {
    float topLeft = 8.0f;
    float topRight = 8.0f;
    float bottomLeft = 8.0f;
    float bottomRight = 8.0f;
};

// ====== 窗口配置表条目 ======
struct WindowConfig {
    const char* id;             // 窗口逻辑名（snake_case，全局唯一）
    const char* title;          // 标题文字（UTF-8）
    uint64_t flags;             // WindowFlags 组合
    float defaultW, defaultH;   // 默认尺寸（逻辑像素，运行时乘 scale）
    float minW, minH;           // 最小尺寸（逻辑像素）
    int closePolicy;            // 关闭策略 ClosePolicy
    int contextId;              // 上下文ID（原子分组，切场景整组销毁）
    const char* handlerName;    // 渲染处理器名（注册表键，配置驱动分发）
    float overlayAlpha;         // 遮罩透明度 0~255（0=无遮罩可穿透，>0=全屏屏障）
    CornerRadii cornerRadii;    // 四角独立圆角（逻辑像素）
    int layer = LAYER_DEV;     // 归属层级 LayerId（渲染/命中顺序）
    // 点下层窗行为（规范 §2.4）：false=仅聚焦（bringToFront+消费事件）
    //                            true=穿透响应（聚焦且本次点击直达内部控件）
    bool clickThroughToFront = false;
    float titleBarH = 36.0f;    // 标题栏高度（逻辑像素；参考标准窗36、调试窗32）
    bool dynamicTitle = false;  // 动态标题：创建时追加 " #实例id"（多实例窗如"调试 #3"）
    float panelAlpha = 235.0f;  // 底板透明度 0~255（参考：标准235/设置230/调试240）
    float borderAlpha = 100.0f; // 外框描边透明度 0~255（参考：标准100/设置60；0=不画）
    // ---- 阴影参数（参考 DrawShadow(g,.., ShadowBlur()+N, ShadowAlpha()+M) 各窗差异） ----
    float shadowBlurAdd = 2.0f;   // 阴影层数加值（警告+3 / 设置+2 / 调试+2 / 停靠0）
    float shadowAlphaAdd = 20.0f; // 阴影透明度加值 0~255（警告+30 / 设置+20 / 调试+15）
    // ---- 标题栏细节（参考：警告标题14px@240 / 调试标题13px@245按钮20px） ----
    float titleBarAlpha = 240.0f; // 标题栏底透明度 0~255（标准240 / 调试245）
    float titleFontPx = 14.0f;    // 标题字号（逻辑像素：标准14 / 调试13）
    float titleBtnSize = 22.0f;   // 标题栏按钮边长（标准22 / 调试20）
};

// ====== 窗口配置表（新增窗口只需在此追加条目） ======
// 条目尾字段（位置初始化顺序）：clickThroughToFront, titleBarH, dynamicTitle,
//   panelAlpha, borderAlpha, shadowBlurAdd, shadowAlphaAdd,
//   titleBarAlpha, titleFontPx, titleBtnSize
inline const WindowConfig WINDOW_CONFIGS[] = {
    // ---- 开发级 LAYER_DEV（一般 Lua / 硬编码工具窗，当前大部分悬浮窗） ----

    // 主题工作室（原 FW_COLOR_CUSTOM；参考 fullRadius=12、阴影+2/+20、遮罩特判0）
    { "theme_studio", "主题工作室",
      WIN_DRAGGABLE | WIN_RESIZABLE | WIN_FLOATING | WIN_HAS_CLOSE |
      WIN_HAS_PIN_TOP | WIN_HAS_FIX_SIZE | WIN_HAS_FLOAT_BALL,
      480, 460, 300, 300, CLOSE_BUTTON_ONLY, 0, "handler_theme_studio", 0,
      {12,12,12,12}, LAYER_DEV, false, 36.0f, false, 235.0f, 100.0f,
      2.0f, 20.0f, 240.0f, 14.0f, 22.0f },

    // 颜色选择器（原 FW_COLOR_PICKER；参考 minW/H=140/180 可缩很小、遮罩特判0）
    { "color_picker", "颜色选择器",
      WIN_DRAGGABLE | WIN_RESIZABLE | WIN_FLOATING | WIN_HAS_CLOSE,
      360, 470, 140, 200, CLOSE_BUTTON_ONLY, 0, "handler_color_picker", 0,
      {12,12,12,12}, LAYER_DEV, false, 36.0f, false, 235.0f, 100.0f,
      2.0f, 20.0f, 240.0f, 14.0f, 22.0f },

    // 数字键盘（与调色板同构：独立悬浮窗 + 显示区 + 按键）
    { "keypad_numeric", "数字键盘",
      WIN_DRAGGABLE | WIN_RESIZABLE | WIN_FLOATING | WIN_HAS_CLOSE,
      320, 380, 200, 260, CLOSE_BUTTON_ONLY, 0, "handler_keypad_numeric", 0,
      {12,12,12,12}, LAYER_DEV, false, 36.0f, false, 235.0f, 100.0f,
      2.0f, 20.0f, 240.0f, 14.0f, 22.0f },

    // 身份证输入器（0-9 / X / 退格；上限 18 位）
    { "keypad_idcard", "身份证输入器",
      WIN_DRAGGABLE | WIN_RESIZABLE | WIN_FLOATING | WIN_HAS_CLOSE,
      320, 320, 200, 240, CLOSE_BUTTON_ONLY, 0, "handler_keypad_idcard", 0,
      {12,12,12,12}, LAYER_DEV, false, 36.0f, false, 235.0f, 100.0f,
      2.0f, 20.0f, 240.0f, 14.0f, 22.0f },

    // 设置面板（原 FW_SETTINGS；参考无标题栏，表单+底部确定/取消，整窗可拖）
    { "settings", "设置",
      WIN_DRAGGABLE | WIN_RESIZABLE | WIN_FLOATING | WIN_NO_TITLE_BAR | WIN_MODAL,
      420, 360, 300, 250, CLOSE_BUTTON_ONLY, 0, "handler_settings", 80,
      {12,12,12,12}, LAYER_DEV, false, 36.0f, false, 230.0f, 60.0f,
      2.0f, 20.0f, 240.0f, 14.0f, 22.0f },

    // 登录窗口（复用 settings 渲染）
    { "login", "登录",
      WIN_DRAGGABLE | WIN_FLOATING | WIN_NO_TITLE_BAR | WIN_MODAL,
      420, 360, 300, 250, CLOSE_BUTTON_ONLY, 0, "handler_settings", 80,
      {12,12,12,12}, LAYER_DEV, false, 36.0f, false, 230.0f, 60.0f,
      2.0f, 20.0f, 240.0f, 14.0f, 22.0f },

    // 关于窗口（复用 settings 渲染）
    { "about", "关于",
      WIN_DRAGGABLE | WIN_FLOATING | WIN_NO_TITLE_BAR | WIN_MODAL,
      420, 360, 300, 250, CLOSE_BUTTON_ONLY, 0, "handler_settings", 80,
      {12,12,12,12}, LAYER_DEV, false, 36.0f, false, 230.0f, 60.0f,
      2.0f, 20.0f, 240.0f, 14.0f, 22.0f },

    // 警告窗口（原 FW_WARNING，全局最上层；参考阴影+3/+30、标题14px）
    { "warning", "警告",
      WIN_DRAGGABLE | WIN_FLOATING | WIN_HAS_CLOSE | WIN_MODAL,
      360, 220, 200, 150, CLOSE_BUTTON_ONLY, 0, "handler_warning", 120,
      {12,12,12,12}, LAYER_GLOBAL, false, 36.0f, false, 235.0f, 100.0f,
      3.0f, 30.0f, 240.0f, 14.0f, 22.0f },

    // 侧边设置（原 FW_SETTINGS_SIDE 480×380；参考无标题栏，CLOSE_OUTSIDE_CLICK→遮罩80）
    { "side_settings", "侧边设置",
      WIN_DRAGGABLE | WIN_FLOATING | WIN_NO_TITLE_BAR | WIN_SIDE_TABS,
      480, 380, 300, 250, CLOSE_OUTSIDE_CLICK, 0, "handler_side_settings", 80,
      {12,12,12,12}, LAYER_DEV, false, 36.0f, false, 230.0f, 60.0f,
      2.0f, 20.0f, 240.0f, 14.0f, 22.0f },

    // 水平停靠窗口（原 FW_DOCK LR；透明窗：侧栏按钮+内容面板全自绘 420×340）
    // 关闭按钮走主线统一按钮系统（矢量X），不再模块自绘
    { "dock_lr", "停靠-LR",
      WIN_DRAGGABLE | WIN_FLOATING | WIN_TRANSPARENT | WIN_NO_TITLE_BAR |
      WIN_DOCK_HORIZONTAL | WIN_HAS_CLOSE,
      420, 340, 120, 80, CLOSE_BUTTON_ONLY, 0, "handler_dock", 0,
      {14,14,14,14}, LAYER_DEV, false, 36.0f, false, 0.0f, 0.0f,
      0.0f, 0.0f, 240.0f, 14.0f, 22.0f },

    // 垂直停靠窗口（原 FW_DOCK TB）
    { "dock_tb", "停靠-TB",
      WIN_DRAGGABLE | WIN_FLOATING | WIN_TRANSPARENT | WIN_NO_TITLE_BAR |
      WIN_DOCK_VERTICAL | WIN_HAS_CLOSE,
      420, 340, 80, 120, CLOSE_BUTTON_ONLY, 0, "handler_dock", 0,
      {14,14,14,14}, LAYER_DEV, false, 36.0f, false, 0.0f, 0.0f,
      0.0f, 0.0f, 240.0f, 14.0f, 22.0f },

    // 调试窗口（原 FW_DEBUG 可多实例；参考标题栏32高@245、标题13px、按钮20px、圆角8）
    { "debug", "调试",
      WIN_DRAGGABLE | WIN_RESIZABLE | WIN_FLOATING | WIN_HAS_CLOSE |
      WIN_HAS_COLLAPSE | WIN_HAS_FIX_SIZE | WIN_HAS_FLOAT_BALL | WIN_MULTI_INSTANCE,
      300, 280, 200, 150, CLOSE_BUTTON_ONLY, 0, "handler_debug", 0,
      {8,8,8,8}, LAYER_DEV, false, 32.0f, true, 240.0f, 100.0f,
      2.0f, 15.0f, 245.0f, 13.0f, 20.0f },

    // 分割器测试窗口（SplitterSystem 系统派发验收：垂直条 + 嵌套水平条）
    { "splitter_demo", "分割器测试",
      WIN_DRAGGABLE | WIN_RESIZABLE | WIN_FLOATING | WIN_HAS_CLOSE | WIN_HAS_FIX_SIZE,
      420, 320, 280, 220, CLOSE_BUTTON_ONLY, 0, "handler_splitter", 0,
      {12,12,12,12}, LAYER_DEV, false, 36.0f, false, 235.0f, 100.0f,
      2.0f, 20.0f, 240.0f, 14.0f, 22.0f },

    // 沙盘测试窗口（SandboxSystem 系统派发验收：pan+zoom 无限画布）
    { "sandbox_demo", "沙盘测试",
      WIN_DRAGGABLE | WIN_RESIZABLE | WIN_FLOATING | WIN_HAS_CLOSE | WIN_HAS_FIX_SIZE,
      460, 360, 300, 240, CLOSE_BUTTON_ONLY, 0, "handler_sandbox", 0,
      {8,8,8,8}, LAYER_DEV, false, 32.0f, false, 235.0f, 100.0f,
      2.0f, 16.0f, 240.0f, 13.0f, 20.0f },

    // 悬浮球测试窗口（LayerStack 悬浮球系统派发验收：自带悬浮球按钮，
    // 可把窗口收成球；点击球再展开——「UI模板」页悬浮球点击即打开本窗）
    { "ball_demo", "悬浮球测试",
      WIN_DRAGGABLE | WIN_RESIZABLE | WIN_FLOATING | WIN_HAS_CLOSE |
      WIN_HAS_FLOAT_BALL | WIN_HAS_FIX_SIZE,
      380, 300, 260, 200, CLOSE_BUTTON_ONLY, 0, "handler_ball_demo", 0,
      {12,12,12,12}, LAYER_DEV, false, 36.0f, false, 235.0f, 100.0f,
      2.0f, 20.0f, 240.0f, 14.0f, 22.0f },

    };

inline const int WINDOW_CONFIG_COUNT = (int)(sizeof(WINDOW_CONFIGS) / sizeof(WINDOW_CONFIGS[0]));

// ====== 动态窗口配置注册（运行时新增窗口类型，零引擎改动） ======
// 场景：嵌入宿主按需注入窗口（如"关于宿主"窗），不动静态表。
// deque 持有：扩容不搬迁，返回的指针跨注册稳定（vector 会悬空）。
// 索引约定：静态表 0..WINDOW_CONFIG_COUNT-1，动态区紧接其后。
inline std::deque<WindowConfig> g_dynamicConfigs;

// 注册新窗口配置：id 与静态表/动态区重复返回 -1；成功返回全局索引
inline int RegisterWindowConfig(const WindowConfig& cfg) {
    if (!cfg.id || !cfg.id[0] || !cfg.handlerName || !cfg.handlerName[0]) return -1;
    for (int i = 0; i < WINDOW_CONFIG_COUNT; i++)
        if (strcmp(WINDOW_CONFIGS[i].id, cfg.id) == 0) return -1;
    for (const auto& d : g_dynamicConfigs)
        if (strcmp(d.id, cfg.id) == 0) return -1;
    g_dynamicConfigs.push_back(cfg);
    return WINDOW_CONFIG_COUNT + (int)g_dynamicConfigs.size() - 1;
}

// 按逻辑名查找配置表条目（静态表 + 动态区；返回 nullptr 表示不存在）
inline const WindowConfig* FindWindowConfig(const char* id) {
    for (int i = 0; i < WINDOW_CONFIG_COUNT; i++) {
        if (strcmp(WINDOW_CONFIGS[i].id, id) == 0) return &WINDOW_CONFIGS[i];
    }
    for (const auto& d : g_dynamicConfigs) {
        if (strcmp(d.id, id) == 0) return &d;
    }
    return nullptr;
}

// 按配置表索引查找（静态区 + 动态区统一索引空间）
inline const WindowConfig* FindWindowConfigByIndex(int idx) {
    if (idx < 0) return nullptr;
    if (idx < WINDOW_CONFIG_COUNT) return &WINDOW_CONFIGS[idx];
    int d = idx - WINDOW_CONFIG_COUNT;
    if (d < (int)g_dynamicConfigs.size()) return &g_dynamicConfigs[(size_t)d];
    return nullptr;
}

// 按配置名查配置表索引（返回 -1 表示不存在）
inline int FindWindowConfigIndex(const char* id) {
    for (int i = 0; i < WINDOW_CONFIG_COUNT; i++) {
        if (strcmp(WINDOW_CONFIGS[i].id, id) == 0) return i;
    }
    for (size_t i = 0; i < g_dynamicConfigs.size(); i++) {
        if (strcmp(g_dynamicConfigs[i].id, id) == 0)
            return WINDOW_CONFIG_COUNT + (int)i;
    }
    return -1;
}

} // namespace ui