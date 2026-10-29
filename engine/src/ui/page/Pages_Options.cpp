// AI生成
// ============================================================
// 其他页处理器（输入框 + 悬浮窗按钮墙）
// 原页顶控件（音量滑条 / 通知开关 / 选项 A、B 复选框 / 关于按钮）已按需求删除；
// 本页保留两部分：
//   1)「输入框」区：数字 / 身份证号两个真实可编辑输入框（TextInputSystem 托管）。
//      点击输入框 → 聚焦定位光标 + 绑定为键盘窗目标 + 弹出对应小键盘；
//      物理键盘可直接输入，小键盘按键回填同一缓冲（见 WinHandlers_Keypad）。
//   2)「悬浮窗按钮墙」：配置表单源（WALL[]），分组「真实功能 / 模板」，
//      按钮文案 = 名字-id（如「主题工作室-theme_studio」），按下 g_winMgr.openWindow(id)。
// ============================================================
#include "PageControls.h"
#include "Pages.h"
#include "../MainUI.h"
#include "../win/RegionSystem.h"
#include "../win/WindowManager.h"
#include "../win/LayerStack.h"
#include "../win/TextInputSystem.h"
#include "../win/WinHandlers.h"
#include <cstdio>

namespace ui {

namespace options {

// 本页滚动区域逻辑名
static const char* REGION_ID = "options_main";

// hover 命中索引（-1 = 无；按钮 = HIT_WALL_BASE + 索引）
static constexpr int HIT_WALL_BASE = 10;
static constexpr int HIT_ROUND     = 9;    // 应用窗口圆润值滑条

// ---- 应用窗口：边框模式切换（系统边框 ⇄ 完全自绘）----
// 自绘 = 去掉系统标题栏，引擎自己画 - 口 X 与标题（见 AppWindow 应用标题栏）；
// 切换必须重启整个应用窗口渲染（GLFW_DECORATED 只在建窗瞬间生效）——
// 这里只置 ctx.windowFrameDirty，重启由宿主快照链路完成（布局不丢）
static const char* FRAME_SECTION = u8"应用窗口";
static const char* FRAME_LABEL   = u8"完全自己画 UI（自绘 - 口 X）";
// 圆润值（应用窗口外角）：0..2 → 基准半径 UiContext::WINDOW_ROUND_BASE(16px)；
// 滑条落在开关旁，AppWindow 帧内用 Window::setRoundCorner 落地（无需重启）
static const char* FRAME_ROUND_LABEL = u8"圆润值";
// 全屏（F11）隐藏顶部 UI：开启后自绘标题栏在全屏时让位（内容铺满）
static const char* FRAME_FULL_LABEL  = u8"全屏（F11）时隐藏顶部 UI";

// ---- 输入框区（点击弹出对应小键盘；物理键盘同时可用） ----
static const char* INPUT_TITLE   = u8"输入框（点击弹出小键盘）";
static const char* LABEL_NUM     = u8"数字";
static const char* LABEL_ID      = u8"身份证号";
static const char* HINT_NUM      = u8"输入数字（0-9 . -）";
static const char* HINT_ID       = u8"输入身份证号（0-9、X）";
static const int   NUM_BYTES     = 24;   // 数字框上限（与数字键盘一致）
static const int   ID_BYTES      = 18;   // 身份证框上限（18 位）
static const char* KEYPAD_NUM_ID = "keypad_numeric";
static const char* KEYPAD_ID_ID  = "keypad_idcard";

// ---- 悬浮窗按钮墙：分组 / 文案 / 窗口 id（配置表单源，零硬编码）----
enum WallGroup { WG_REAL = 0, WG_TPL = 1 };   // 真实功能 / 模板
struct WallEntry { int group; const char* label; const char* winId; };
static const WallEntry WALL[] = {
    // 真实功能：可直接使用的功能窗
    { WG_REAL, u8"主题工作室", "theme_studio" },
    { WG_REAL, u8"调色板",     "color_picker" },
    { WG_REAL, u8"数字键盘",   "keypad_numeric" },
    { WG_REAL, u8"身份证输入器", "keypad_idcard" },
    // 模板：内置演示窗
    { WG_TPL,  u8"设置",       "settings" },
    { WG_TPL,  u8"登录",       "login" },
    { WG_TPL,  u8"关于",       "about" },
    { WG_TPL,  u8"警告",       "warning" },
    { WG_TPL,  u8"侧边设置",   "side_settings" },
    { WG_TPL,  u8"停靠-LR",    "dock_lr" },
    { WG_TPL,  u8"停靠-TB",    "dock_tb" },
    { WG_TPL,  u8"调试",       "debug" },
};
static const int WALL_COUNT = (int)(sizeof(WALL) / sizeof(WALL[0]));
static const char* WALL_TITLE_REAL = u8"真实功能（名字-id）";
static const char* WALL_TITLE_TPL  = u8"模板（名字-id）";

// 内容平移（滚动；布局矩形与屏幕矩形唯一换算点）
static Rect at(const Rect& r, float dy) {
    return Rect(Vec2(r.pos.x, r.pos.y + dy), r.size);
}

// ---- 几何单源 ----
struct Layout {
    Rect frameSection;                     // 「应用窗口」区标题
    Rect frameLabel, frameToggle;          // 边框模式：标签 / 开关
    Rect frameRoundLabel, frameRoundSlider, frameRoundValue;  // 圆润值：标签/滑条/数值
    Rect frameFullLabel, frameFullToggle;  // 全屏隐藏顶部 UI：标签 / 开关
    Rect inputTitle;                       // 输入框区标题
    Rect numLabel, numBox;                 // 数字：标签 / 输入框
    Rect idLabel,  idBox;                  // 身份证：标签 / 输入框
    Rect wallTitleReal, wallTitleTpl;      // 两个分组标题
    Rect wallBtn[WALL_COUNT];              // 按钮墙格子（与 WALL 同序）
    float contentH = 0;
};
static Layout layout(PageEnv& env) {
    Layout L;
    float s = env.scale();
    float cx = env.content.pos.x;
    // 全局规划自动适配：区域系统按当前滚动条模板为内容让位
    float cw = g_regions.getContentW(env.vwin().id, REGION_ID);
    if (cw <= 0.0f) cw = env.content.size.x;   // 首帧尚无区域 → 临时全宽
    float y = env.content.pos.y;
    float labelH = 22.0f * s;

    // ---- 应用窗口：边框模式开关 + 圆润值滑条（开关旁，自右向左排布）----
    L.frameSection = Rect(Vec2(cx, y), Vec2(cw, labelH));
    y += labelH + 8.0f * s;
    {
        float rowH = 34.0f * s;
        float togW = 56.0f * s, togH = 28.0f * s;        // 开关本体尺寸（同目录页）
        float gap  = 12.0f * s;
        float valW = 46.0f * s;                          // 「nnpx」数值宽
        float slW  = 150.0f * s;                         // 圆润值滑条槽宽
        float rlW  = 52.0f * s;                          // 「圆润值」标签宽
        float sliderH = 16.0f * s;                       // 滑条槽高（与全局标准一致）
        // 开关靠最右
        L.frameToggle = Rect(Vec2(cx + cw - togW, y + (rowH - togH) * 0.5f), Vec2(togW, togH));
        float x = cx + cw - togW - gap;
        // 数值（右对齐）：滑条右侧、开关左侧
        L.frameRoundValue = Rect(Vec2(x - valW, y), Vec2(valW, rowH)); x -= valW + gap;
        // 滑条槽（纵向居中）
        L.frameRoundSlider = Rect(Vec2(x - slW, y + (rowH - sliderH) * 0.5f), Vec2(slW, sliderH));
        x -= slW + gap;
        // 「圆润值」标签
        L.frameRoundLabel = Rect(Vec2(x - rlW, y), Vec2(rlW, rowH)); x -= rlW + gap;
        // 边框模式标签占左侧剩余
        float lw = x - cx;
        L.frameLabel = Rect(Vec2(cx, y), Vec2(lw > 24.0f * s ? lw : 24.0f * s, rowH));
        y += rowH + 18.0f * s;
    }

    // ---- 全屏（F11）隐藏顶部 UI 开关（自绘模式下全屏时顶栏让位，内容铺满）----
    {
        float rowH = 34.0f * s;
        float togW = 56.0f * s, togH = 28.0f * s;
        L.frameFullLabel  = Rect(Vec2(cx, y), Vec2(cw - togW - 12.0f * s, rowH));
        L.frameFullToggle = Rect(Vec2(cx + cw - togW, y + (rowH - togH) * 0.5f), Vec2(togW, togH));
        y += rowH + 18.0f * s;
    }

    // ---- 输入框区：标题 + 两行（标签在左、输入框在右）----
    L.inputTitle = Rect(Vec2(cx, y), Vec2(cw, labelH));
    y += labelH + 8.0f * s;

    float rowH  = 34.0f * s;
    float lblW  = 96.0f * s;
    float boxW  = cw - lblW;
    if (boxW > 300.0f * s) boxW = 300.0f * s;   // 紧凑：输入框不拉满整行
    if (boxW < 120.0f * s) boxW = 120.0f * s;

    L.numLabel = Rect(Vec2(cx, y), Vec2(lblW, rowH));
    L.numBox   = Rect(Vec2(cx + lblW, y), Vec2(boxW, rowH));
    y += rowH + 10.0f * s;
    L.idLabel  = Rect(Vec2(cx, y), Vec2(lblW, rowH));
    L.idBox    = Rect(Vec2(cx + lblW, y), Vec2(boxW, rowH));
    y += rowH + 18.0f * s;

    // ---- 悬浮窗按钮墙：两组标题 + 网格按钮（列数 3，随内容宽自适应）----
    float btnH   = 34.0f * s;
    float btnGap = 10.0f * s;
    const int cols = 3;
    float btnW = (cw - btnGap * (float)(cols - 1)) / (float)cols;

    L.wallTitleReal = Rect(Vec2(cx, y), Vec2(cw, labelH));
    y += labelH + 8.0f * s;

    int idxInGroup = 0;
    for (int i = 0; i < WALL_COUNT; i++) {
        if (i > 0 && WALL[i].group != WALL[i - 1].group) {
            int rows = (idxInGroup + cols - 1) / cols;      // 上一组收尾
            y += (float)rows * (btnH + btnGap);
            idxInGroup = 0;
            L.wallTitleTpl = Rect(Vec2(cx, y), Vec2(cw, labelH));
            y += labelH + 8.0f * s;
        }
        int c = idxInGroup % cols, r = idxInGroup / cols;
        L.wallBtn[i] = Rect(Vec2(cx + (float)c * (btnW + btnGap),
                                 y + (float)r * (btnH + btnGap)), Vec2(btnW, btnH));
        idxInGroup++;
    }
    y += (float)((idxInGroup + cols - 1) / cols) * (btnH + btnGap);

    L.contentH = y - env.content.pos.y + 16.0f * s;
    return L;
}

// ---- 页面私有状态 ----
struct State {
    int hover = -1;   // 当前 hover 命中索引（-1 无）
    bool roundDrag = false;   // 圆润值滑条拖拽中（跨帧）

    // 输入框（系统托管：焦点/字符流/光标全归 TextInputSystem）
    EditText numEdit{HINT_NUM, NUM_BYTES, TEXT_NUM};      // 数字：数值过滤
    EditText idEdit {HINT_ID,  ID_BYTES,  TEXT_IDCARD};   // 身份证：0-9 / X
};

} // namespace options

// ---------------- render ----------------

static void renderOptions(PageEnv& env) {
    options::Layout L = options::layout(env);
    options::State& s = env.state<options::State>();
    ThemeColors& c = currentColors();
    float sc = env.scale();
    Font* f = env.font();
    Font* sf = env.smallFont();
    Color lblCol(c.text.r, c.text.g, c.text.b, 180.0f / 255.0f);

    RegionRequest req;
    req.x = 0; req.y = 0;
    req.w = env.content.size.x; req.h = env.content.size.y;
    req.tabId = 0; req.id = options::REGION_ID;
    RegionHandle h = g_regions.begin(env.dev, env.vwin(), req);
    if (!h.valid) return;
    float dy = -h.scrollY;

    // ---- 应用窗口：边框模式开关（切换后整个应用窗口重启渲染）----
    pctl::drawTextLeft(env.dev, f, options::FRAME_SECTION,
                       options::at(L.frameSection, dy),
                       Color(c.accent.r, c.accent.g, c.accent.b, 235.0f / 255.0f));
    pctl::drawTextLeft(env.dev, sf, options::FRAME_LABEL,
                       options::at(L.frameLabel, dy), lblCol);
    pctl::drawToggle(env.dev, options::at(L.frameToggle, dy),
                     g_uiCtx.windowFrame != 0);

    // 圆润值（开关旁）：滑条 0..2 → 行程 0..1；数值以「基准×值」显示实际像素
    pctl::drawTextLeft(env.dev, sf, options::FRAME_ROUND_LABEL,
                       options::at(L.frameRoundLabel, dy), lblCol);
    pctl::drawSliderFull(env.dev, options::at(L.frameRoundSlider, dy),
                         g_uiCtx.windowRound * 0.5f,
                         s.hover == options::HIT_ROUND || s.roundDrag, 1.0f);
    {
        char vb[16];
        snprintf(vb, sizeof(vb), "%dpx",
                 (int)(UiContext::WINDOW_ROUND_BASE * g_uiCtx.windowRound + 0.5f));
        pctl::drawTextRight(env.dev, sf, vb, options::at(L.frameRoundValue, dy), lblCol);
    }

    // 全屏（F11）隐藏顶部 UI 开关：开关态由 g_uiCtx.fullscreenHideBar 单源
    pctl::drawTextLeft(env.dev, sf, options::FRAME_FULL_LABEL,
                       options::at(L.frameFullLabel, dy), lblCol);
    pctl::drawToggle(env.dev, options::at(L.frameFullToggle, dy),
                     g_uiCtx.fullscreenHideBar);

    // ---- 输入框区：标题 + 标签 + 两个可编辑输入框 ----
    pctl::drawTextLeft(env.dev, f, options::INPUT_TITLE,
                       options::at(L.inputTitle, dy),
                       Color(c.accent.r, c.accent.g, c.accent.b, 235.0f / 255.0f));
    {
        TextInputSystem& tis = g_winMgr.textInput();   // 焦点态/光标相位系统单源

        bool fn = tis.focused(s.numEdit);
        pctl::drawTextLeft(env.dev, sf, options::LABEL_NUM,
                           options::at(L.numLabel, dy), lblCol);
        wctl::drawInput(env.dev, options::at(L.numBox, dy), s.numEdit.buf.c_str(),
                        options::HINT_NUM, fn, fn && tis.caretOn(), 1.0f, 12.0f,
                        fn ? s.numEdit.caret : -1);

        bool fi = tis.focused(s.idEdit);
        pctl::drawTextLeft(env.dev, sf, options::LABEL_ID,
                           options::at(L.idLabel, dy), lblCol);
        wctl::drawInput(env.dev, options::at(L.idBox, dy), s.idEdit.buf.c_str(),
                        options::HINT_ID, fi, fi && tis.caretOn(), 1.0f, 12.0f,
                        fi ? s.idEdit.caret : -1);
    }

    // ---- 悬浮窗按钮墙：分组标题 + 按钮（文案 = 名字-id；按下开窗） ----
    pctl::drawTextLeft(env.dev, f, options::WALL_TITLE_REAL,
                       options::at(L.wallTitleReal, dy),
                       Color(c.accent.r, c.accent.g, c.accent.b, 235.0f / 255.0f));
    pctl::drawTextLeft(env.dev, f, options::WALL_TITLE_TPL,
                       options::at(L.wallTitleTpl, dy),
                       Color(c.text.r, c.text.g, c.text.b, 180.0f / 255.0f));
    for (int i = 0; i < options::WALL_COUNT; i++) {
        Rect r = options::at(L.wallBtn[i], dy);
        bool hov = (s.hover == options::HIT_WALL_BASE + i);
        float rad = c.btnRadius * sc * g_uiCtx.radiusScale();
        drawRoundedRect(env.dev, r,
            Color(c.inputBg.r, c.inputBg.g, c.inputBg.b, 220.0f / 255.0f), rad);
        Color bc = hov ?
            Color(c.accent.r, c.accent.g, c.accent.b, 200.0f / 255.0f) :
            Color(c.inputBorder.r, c.inputBorder.g, c.inputBorder.b, 120.0f / 255.0f);
        drawRoundedBorder(env.dev, r, bc, g_uiCtx.borderWidth(), rad);
        char lbl[128];
        snprintf(lbl, sizeof(lbl), "%s-%s", options::WALL[i].label, options::WALL[i].winId);
        pctl::drawTextCentered(env.dev, f, lbl, r, hov ? (Color)c.accent : (Color)c.text);
    }

    g_regions.end(env.dev, h, L.contentH);
}

// ---------------- click：输入框聚焦/绑定/开小键盘；按钮 → 打开对应窗 ----------------

static bool clickOptions(PageEnv& env, const Vec2& m) {
    options::Layout L = options::layout(env);
    options::State& s = env.state<options::State>();
    float scroll = g_regions.getScroll(env.vwin().id, options::REGION_ID);
    Vec2 lm(m.x, m.y + scroll);

    // 应用窗口：边框模式开关 → 翻转并请求整体重启（宿主快照重建全部应用窗口）
    if (L.frameToggle.contains(lm)) {
        g_uiCtx.windowFrame = g_uiCtx.windowFrame ? 0 : 1;
        g_uiCtx.windowFrameDirty = true;   // AppWindow 帧内消费 → frameRestart
        return true;
    }

    // 应用窗口：圆润值滑条 → 实时改 g_uiCtx.windowRound（AppWindow 帧内落地，无需重启）。
    // 命中区纵向外扩 ±8，槽偏细仍好点。
    {
        Rect hit = L.frameRoundSlider;
        hit.pos.y -= 8.0f * env.scale();
        hit.size.y += 16.0f * env.scale();
        if (hit.contains(lm)) {
            s.roundDrag = true;
            g_uiCtx.windowRound = wctl::sliderValueAt(L.frameRoundSlider, lm.x) * 2.0f;
            return true;
        }
    }

    // 全屏（F11）隐藏顶部 UI 开关 → 翻转（AppWindow 每帧同步 appTitleBarH 即时生效）
    if (L.frameFullToggle.contains(lm)) {
        g_uiCtx.fullscreenHideBar = !g_uiCtx.fullscreenHideBar;
        return true;
    }

    // 输入框：聚焦（物理键盘可用）+ 光标定位 + 绑定键盘窗目标 + 弹出小键盘
    // 注：帧首已 blurAll 提交旧焦点，此处的 requestFocus 即"点击输入框聚焦"语义
    if (L.numBox.contains(lm)) {
        TextInputSystem& tis = g_winMgr.textInput();
        int page = env.ui->pageSel();
        tis.requestFocus(TEXT_OWNER_PAGE, page, s.numEdit);
        tis.placeCaret(s.numEdit, options::at(L.numBox, -scroll), m.x);
        bindKeypadTarget(false, &s.numEdit, page);
        if (!g_layerStack.findWindowByConfig(options::KEYPAD_NUM_ID))
            g_winMgr.openWindow(options::KEYPAD_NUM_ID);
        return true;
    }
    if (L.idBox.contains(lm)) {
        TextInputSystem& tis = g_winMgr.textInput();
        int page = env.ui->pageSel();
        tis.requestFocus(TEXT_OWNER_PAGE, page, s.idEdit);
        tis.placeCaret(s.idEdit, options::at(L.idBox, -scroll), m.x);
        bindKeypadTarget(true, &s.idEdit, page);
        if (!g_layerStack.findWindowByConfig(options::KEYPAD_ID_ID))
            g_winMgr.openWindow(options::KEYPAD_ID_ID);
        return true;
    }

    for (int i = 0; i < options::WALL_COUNT; i++) {
        if (L.wallBtn[i].contains(lm)) {
            g_winMgr.openWindow(options::WALL[i].winId);   // 按钮 → 打开对应窗
            return true;
        }
    }
    return false;
}

// ---------------- hover ----------------

static void hoverOptions(PageEnv& env, const Vec2& m) {
    options::Layout L = options::layout(env);
    options::State& s = env.state<options::State>();
    float scroll = g_regions.getScroll(env.vwin().id, options::REGION_ID);
    Vec2 lm(m.x, m.y + scroll);

    s.hover = -1;
    {
        Rect hit = L.frameRoundSlider;
        hit.pos.y -= 8.0f * env.scale();
        hit.size.y += 16.0f * env.scale();
        if (hit.contains(lm)) s.hover = options::HIT_ROUND;
    }
    for (int i = 0; i < options::WALL_COUNT; i++)
        if (L.wallBtn[i].contains(lm)) { s.hover = options::HIT_WALL_BASE + i; break; }
}

// ---------------- drag / release：本页无拖拽（占位，保持接口一致） ----------------

static void dragOptions(PageEnv& env, const Vec2& m) {
    options::Layout L = options::layout(env);
    options::State& s = env.state<options::State>();
    if (!s.roundDrag) return;
    float scroll = g_regions.getScroll(env.vwin().id, options::REGION_ID);
    Vec2 lm(m.x, m.y + scroll);
    // 圆润值滑条跨帧拖拽：直接由鼠标 x 反推值（与全局滑条同手感）
    g_uiCtx.windowRound = wctl::sliderValueAt(L.frameRoundSlider, lm.x) * 2.0f;
}
static void releaseOptions(PageEnv& env, const Vec2&) {
    options::State& s = env.state<options::State>();
    s.roundDrag = false;
}

// ---------------- collect ----------------

static void collectOptions(std::string& out) {
    out += options::FRAME_SECTION;
    out += options::FRAME_LABEL;
    out += options::FRAME_ROUND_LABEL;
    out += options::FRAME_FULL_LABEL;
    out += options::INPUT_TITLE;
    out += options::LABEL_NUM;
    out += options::LABEL_ID;
    out += options::WALL_TITLE_REAL;
    out += options::WALL_TITLE_TPL;
    for (int i = 0; i < options::WALL_COUNT; i++) {
        out += options::WALL[i].label;
        out += "-";
        out += options::WALL[i].winId;
    }
}

// ---------------- 注册 ----------------

void RegisterOptionsPage(PageHandlerRegistry& reg) {
    PageHandler h;
    h.render  = renderOptions;
    h.click   = clickOptions;
    h.hover   = hoverOptions;
    h.drag    = dragOptions;
    h.release = releaseOptions;
    h.collect = collectOptions;
    reg.registerHandler("page_options", h);
}

} // namespace ui