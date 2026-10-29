// AI生成
// ============================================================
// 「UI模板」页 —— 两大「交互演示区」（真控件，不是静态演示）
//
// 用户要求：
//   · 控件必须"真可交互"（点 / 拖 / 切 全部生效），不是画一张死图；
//   · 分割线必须可交互（拖动分割线实时改两面板占比）→ 给两个演示面板；
//   · 页面分「两个演示区域」，紧凑，够演示即可；下拉=选编号的入口。
//
// 结构：
//   区域①「控件交互台」：按钮 / 滑动条 / 开关 / 悬浮球 各一个真控件；
//        每行 = 名称 + 实时控件 + 系统<控件>#ID 徽标 + 「绘制风格」下拉（游戏ID）
//        开关行额外挂一个「扩展图案」下拉（switchExtra，14 项）
//   区域②「分割线交互台」：两个面板 + 可拖动分割线（SplitterSystem）+ 分割线风格下拉
//
// 游戏ID：0=系统跟随 1=系统默认 2..=变体（StyleTpl.h 单源换算 internalOf/idOf）。
// 下拉即"报编号调用"入口：选完写回全局 g_uiCtx.<控件>Style，真控件下一帧即变样。
// 系统派发（页面零基础设施代码）：
//   下拉=DropdownSystem / 分割=SplitterSystem / 滚动·裁剪·滚动条=RegionSystem。
// ============================================================
#include "PageControls.h"
#include "Pages.h"
#include "../MainUI.h"
#include "../win/RegionSystem.h"
#include "../win/WinControls.h"
#include "../win/DropdownSystem.h"
#include "../win/SplitterSystem.h"
#include "../win/TextInputSystem.h"
#include "../win/StyleTpl.h"
#include "../Element.h"
#include <cstdio>
#include <cmath>

namespace ui {
namespace directory {

static const char* TITLE = u8"UI 控件模板 · 两个交互演示区（控件点/拖/切即时生效；分割线可拖）";
static const char* REGION_ID = "directory_main";

// 页面内的下拉槽位（idx 即 ctrlField 的映射；开关形状与扩展图案分列 2/3）
enum DD { DD_BTN = 0, DD_SLIDER, DD_SW, DD_SWEX, DD_BALL, DD_SPLIT, DD_SCROLL, DD_RING, DD_COUNT };

// 区域①四行控件类型（区域④另有"环形进度"独立行，见下）
enum Ctrl { C_BTN = 0, C_SLIDER, C_SW, C_BALL, C_ROWCOUNT };

static const char* ROW_NAME[] = { u8"按钮", u8"滑动条", u8"开关", u8"悬浮球" };

// 行索引(Ctrl) → 下拉槽位(DD) 映射。
// 禁止直接用行索引当 DD 槽位：DD_SWEX 插在 DD_SW 与 DD_BALL 之间，
// 会让「悬浮球」行(行 3)错接到 DD_SWEX —— 下拉显示成"开关扩展图案"、
// 落库还写错字段（用户报"右侧下拉按钮调节风格参数不对"之根因）。
static int rowDD(int row) {
    switch (row) {
        case C_BTN:    return DD_BTN;
        case C_SLIDER: return DD_SLIDER;
        case C_SW:     return DD_SW;
        case C_BALL:   return DD_BALL;
        default:       return DD_RING;   // C_RING
    }
}

// 下拉数据源：按 DD 槽位取 名称/数量/字段（g_uiCtx 是转发宏，字段只能运行期取址）
static const char* const* ddNames(int i) {
    switch (i) {
        case DD_BTN:    return tpl::button::NAMES;
        case DD_SLIDER: return tpl::slider::NAMES;
        case DD_SW:     return tpl::sw::NAMES;
        case DD_SWEX:   return tpl::swExtra::NAMES;
        case DD_BALL:   return tpl::ball::NAMES;
        case DD_SPLIT:  return tpl::splitter::NAMES;
        case DD_SCROLL: return tpl::scrollbar::NAMES;
        case DD_RING:   return tpl::ring::NAMES;
    }
    return nullptr;
}
static int ddCount(int i) {
    switch (i) {
        case DD_BTN:    return tpl::button::COUNT;
        case DD_SLIDER: return tpl::slider::COUNT;
        case DD_SW:     return tpl::sw::COUNT;
        case DD_SWEX:   return tpl::swExtra::COUNT;
        case DD_BALL:   return tpl::ball::COUNT;
        case DD_SPLIT:  return tpl::splitter::COUNT;
        case DD_SCROLL: return tpl::scrollbar::COUNT;
        case DD_RING:   return tpl::ring::COUNT;
    }
    return 0;
}
static int* ddField(int i) {
    switch (i) {
        case DD_BTN:    return &g_uiCtx.btnStyle;
        case DD_SLIDER: return &g_uiCtx.sliderStyle;
        case DD_SW:     return &g_uiCtx.switchStyle;
        case DD_SWEX:   return &g_uiCtx.switchExtra;
        case DD_BALL:   return &g_uiCtx.ballStyle;
        case DD_SPLIT:  return &g_uiCtx.splitterStyle;
        case DD_SCROLL: return &g_uiCtx.scrollbarStyle;
        case DD_RING:   return &g_uiCtx.ringStyle;
    }
    return nullptr;
}

// ---- 区域⑤ 行尾「模板」下拉数据源：按控件样例种类(kind) 取 名称/数量/字段 ----
// 全项目控件在此一览（数据源仍是 StyleTpl.h 单源；报编号即调用）
static const char* const* ddExNames(int kind) {
    switch (kind) {
        case tpl::K_TAB:      return tpl::tab::NAMES;
        case tpl::K_SIDE:     return tpl::side::NAMES;
        case tpl::K_SEG:      return tpl::seg::NAMES;
        case tpl::K_STEPPER:  return tpl::stepper::NAMES;
        case tpl::K_INPUT:    return tpl::input::NAMES;
        case tpl::K_CHECK:    return tpl::check::NAMES;
        case tpl::K_PROGRESS: return tpl::progress::NAMES;
        case tpl::K_DROPDOWN: return tpl::dropdown::NAMES;
    }
    return nullptr;
}
static int ddExCount(int kind) {
    switch (kind) {
        case tpl::K_TAB:      return tpl::tab::COUNT;
        case tpl::K_SIDE:     return tpl::side::COUNT;
        case tpl::K_SEG:      return tpl::seg::COUNT;
        case tpl::K_STEPPER:  return tpl::stepper::COUNT;
        case tpl::K_INPUT:    return tpl::input::COUNT;
        case tpl::K_CHECK:    return tpl::check::COUNT;
        case tpl::K_PROGRESS: return tpl::progress::COUNT;
        case tpl::K_DROPDOWN: return tpl::dropdown::COUNT;
    }
    return 0;
}
static int* ddExField(int kind) {
    switch (kind) {
        case tpl::K_TAB:      return &g_uiCtx.tabStyle;
        case tpl::K_SIDE:     return &g_uiCtx.sideStyle;
        case tpl::K_SEG:      return &g_uiCtx.segStyle;
        case tpl::K_STEPPER:  return &g_uiCtx.stepperStyle;
        case tpl::K_INPUT:    return &g_uiCtx.inputStyle;
        case tpl::K_CHECK:    return &g_uiCtx.checkStyle;
        case tpl::K_PROGRESS: return &g_uiCtx.progressStyle;
        case tpl::K_DROPDOWN: return &g_uiCtx.dropdownStyle;
    }
    return nullptr;
}

// ---- 页面私有状态 ----
// 区域⑤ 行数上限（名录中 kind≠K_NONE 的条目数封顶；render/click/hover/state 统一）
static constexpr int EX_MAX = 8;
enum Hit { HIT_NONE = -1, HIT_BTN = 0, HIT_SLIDER, HIT_SW, HIT_BALL, HIT_RING,
           HIT_EX_TAB, HIT_EX_SIDE, HIT_EX_SEG, HIT_EX_STEP_M, HIT_EX_STEP_V,
           HIT_EX_STEP_P, HIT_EX_INPUT, HIT_EX_CHECK };

struct State {
    int   sel[DD_COUNT];      // 各下拉当前选中（游戏ID 索引）
    bool  dirty[DD_COUNT];    // 值变化待落库
    bool  init = false;
    // 区域①交互量（真控件活数据）
    int   btnClicks = 0;      // 按钮点击次数（点击可见反馈）
    float sliderVal = 0.6f;   // 滑动条值 0~1
    bool  swOn = true;        // 开关
    bool  ballOn = true;      // 悬浮球高亮
    bool  dragSlider = false; // 滑动条拖拽中（click 消费后框架转发 drag）
    int   hover = HIT_NONE;   // 命中项
    bool  btnPressed = false; // 按钮按下态
    // 区域②分割线（屏幕 y；系统拖动直写）
    float spPos = 0;
    bool  spInited = false;
    float spLastDy = 0;       // 上帧滚动量（分割线 y 滚动补偿）
    // 区域④ 进度模板（下载进度 = 条形 / 点击切换加载 = 环形）
    float ringAnim = 0.0f;    // 动画时钟（render 期 env.dt 推进；进度循环 + 环形旋转共用）
    bool  loading = true;     // 环形：true 加载态 / false 进度态（点击切换）
    // 区域⑤ 其余控件交互量（真控件活数据——非静态样例，全部可交互）
    EditText exInput{u8"输入框（示例）", 64, TEXT_ANY};  // 输入框（系统 TextInputSystem 托管编辑）
    int   exTab   = 0;        // 页签选中索引（页签 A/B）
    int   exSeg   = 0;        // 分段按钮选中段（视图模式 列表/网格/详情）
    float exStepVal = 42.0f;  // 计数器数值（支持小数 / 负数）
    EditText exStepEdit{u8"0", 16, TEXT_NUM};   // 计数器数值输入框（系统托管；点击数值区即可编辑）
    bool  exSide  = false;    // 侧栏项选中
    bool  exCheck = true;     // 复选框
    int   hoverPart = -1;     // 命中行内子部件（计数器 −/＋/数值、分段段号等）
    // 区域⑤ 每行「模板」下拉（索引即游戏ID；选完写回该控件族全局字段）
    int   exSel[EX_MAX]   = {0};
    bool  exDirty[EX_MAX] = {false};
    // 下拉按钮样例：真下拉（独立状态；点击展开，展示当前模板绘制）
    int   dsSel = 0;          // 样例下拉选中项索引
    bool  dsDirty = false;
};

// 计数器数值 → 输入框文本（%g 去尾零，支持负数/小数；−/＋ 步进与提交后回写共用单源）
static void exStepSync(State& st) {
    char b[32];
    snprintf(b, sizeof(b), "%g", (double)st.exStepVal);
    st.exStepEdit.buf = b;
    st.exStepEdit.caret = (int)st.exStepEdit.buf.size();
}

// ---- 几何单源（render/click/hover 共用；逻辑坐标）----
struct Layout {
    Rect area1, area2;                    // 两块演示区底板
    Rect nameR[C_ROWCOUNT];               // 行·名称
    Rect ctlR[C_ROWCOUNT];                // 行·真控件区
    Rect idR[C_ROWCOUNT];                 // 行·游戏ID 徽标
    Rect drop1R[C_ROWCOUNT];              // 行·主风格下拉
    Rect drop2R;                          // 开关行·扩展图案下拉
    Rect demo;                            // 区域②：两面板演示区
    Rect spDropR;                         // 区域②：分割线风格下拉
    Rect area3;                           // 区域③：滚动条交互台底板
    Rect scrollBox;                       // 区域③：内嵌滚动容器（真滚动条）
    Rect scDropR;                         // 区域③：滚动条风格下拉
    Rect scBadgeR;                        // 区域③：系统滚动条#N 徽标
    Rect area4;                           // 区域④：进度模板底板（区域③下面）
    Rect pvNameR[2], pvCtlR[2];           // 区域④：两行 名称 / 真控件区
    Rect pvIdR[2],   pvDropR[2];          // 区域④：两行 游戏ID 徽标 / 模板下拉
    // 区域⑤：其余控件（命名+编号）——名录中 kind≠K_NONE 的项各一行
    static constexpr int EX_MAX = 8;
    Rect area5;
    Rect exNameR[EX_MAX], exCtlR[EX_MAX], exIdR[EX_MAX];
    Rect exDropR[EX_MAX];                 // 区域⑤：行尾「模板」下拉（游戏ID 切换）
    Rect exPart[EX_MAX][4];               // 行内子部件矩形（页签×2/分段×3/计数器三键/输入框/复选框）
    int  exPartN[EX_MAX] = {0};           // 各行子部件数（render/click/hover 单源）
    int  exCount = 0;                     // 实际行数（名录中 kind≠K_NONE 的条目数）
    float contentH = 0;
};

static Rect tr(const Rect& r, float dy) { return Rect(Vec2(r.pos.x, r.pos.y + dy), r.size); }

// ---- 区域⑤：「其余控件」条目直接从全控件名录（StyleTpl.h 单源）取，页面零硬编码名字 ----
// 名录中 kind != K_NONE 的项 = 需要在区域⑤呈现样例的控件（页签/侧栏/分段/计数器/输入框/复选框/条形进度）
static int exCount() {
    int n = 0; const tpl::ControlEntry* r = tpl::controlRoster(n);
    int k = 0;
    for (int i = 0; i < n; i++) if (r[i].kind != tpl::K_NONE) k++;
    return k;
}
static const tpl::ControlEntry* exEntry(int i) {
    int n = 0; const tpl::ControlEntry* r = tpl::controlRoster(n);
    int k = 0;
    for (int j = 0; j < n; j++)
        if (r[j].kind != tpl::K_NONE) { if (k == i) return &r[j]; k++; }
    return nullptr;
}

// ---- 行内子部件矩形（逻辑坐标；render/click/hover 单源切分） ----
// 返回子部件数；out 由调用方提供 4 槽。
static int exParts(int kind, const Rect& cr, float s, Rect out[4]) {
    switch (kind) {
        case tpl::K_TAB: {          // 两个页签项
            float w = 104.0f * s, gap = 6.0f * s;
            out[0] = Rect(Vec2(cr.pos.x, cr.pos.y), Vec2(w, cr.size.y));
            out[1] = Rect(Vec2(cr.pos.x + w + gap, cr.pos.y), Vec2(w, cr.size.y));
            return 2;
        }
        case tpl::K_SIDE:           // 一个侧栏项
            out[0] = Rect(cr.pos, Vec2(150.0f * s, cr.size.y)); return 1;
        case tpl::K_SEG: {          // 视图模式：三段（列表/网格/详情）
            float w = 170.0f * s / 3.0f;
            for (int j = 0; j < 3; j++)
                out[j] = Rect(Vec2(cr.pos.x + w * (float)j, cr.pos.y), Vec2(w, cr.size.y));
            return 3;
        }
        case tpl::K_STEPPER: {      // − 值 ＋
            float bw = 34.0f * s, vw = 64.0f * s, h = cr.size.y;
            out[0] = Rect(Vec2(cr.pos.x,           cr.pos.y), Vec2(bw, h));
            out[1] = Rect(Vec2(cr.pos.x + bw,      cr.pos.y), Vec2(vw, h));
            out[2] = Rect(Vec2(cr.pos.x + bw + vw, cr.pos.y), Vec2(bw, h));
            return 3;
        }
        case tpl::K_INPUT:          // 输入框
            out[0] = Rect(cr.pos, Vec2(230.0f * s, cr.size.y)); return 1;
        case tpl::K_CHECK: {        // 复选框（点击区含右侧文字）
            float d = cr.size.y; if (d > 22.0f * s) d = 22.0f * s;
            out[0] = Rect(cr.pos, Vec2(d + 10.0f * s + 96.0f * s, cr.size.y));
            return 1;
        }
        case tpl::K_DROPDOWN:       // 下拉按钮样例（真下拉，按钮宽 220）
            out[0] = Rect(cr.pos, Vec2(220.0f * s, cr.size.y)); return 1;
        default: return 0;          // K_PROGRESS 等：纯显示，无子部件
    }
}

// 命中区域⑤ 行内子部件：返回 true 并写出行号/部件号
static bool exHit(const Layout& L, const Vec2& lm, int& rowOut, int& partOut) {
    for (int i = 0; i < L.exCount; i++)
        for (int j = 0; j < L.exPartN[i]; j++)
            if (L.exPart[i][j].contains(lm)) { rowOut = i; partOut = j; return true; }
    return false;
}

static Layout layout(PageEnv& env) {
    Layout L;
    float s  = env.scale();
    float cx = env.content.pos.x;
    // 全局规划自动适配：区域系统按当前滚动条模板为内容让位（换模板自动改可用宽）
    float cw = g_regions.getContentW(env.vwin().id, REGION_ID);
    if (cw <= 0.0f) cw = env.content.size.x;   // 首帧尚无区域 → 临时全宽

    const float pad    = 12.0f * s;
    const float header = 24.0f * s;
    const float rowH   = 46.0f * s;
    const float dropW  = 240.0f * s;      // 右侧"编号/风格"下拉区总宽
    const float idW    = 116.0f * s;
    const float nameX  = cx + 14.0f * s;
    const float nameW  = 108.0f * s;
    const float dropX  = cx + cw - dropW - 12.0f * s;
    const float idX    = dropX - idW - 14.0f * s;
    const float ctlX   = nameX + nameW + 10.0f * s;
    const float ctlW   = idX - ctlX - 14.0f * s;

    float y = env.content.pos.y + 8.0f * s;
    // 页标题占位（绘制在 render）
    y += 24.0f * s + 8.0f * s;

    // ---- 区域① 控件交互台 ----
    L.area1 = Rect(Vec2(cx, y), Vec2(cw, pad * 2 + header + rowH * C_ROWCOUNT));
    float ry = L.area1.pos.y + pad + header;
    for (int i = 0; i < C_ROWCOUNT; i++) {
        L.nameR[i] = Rect(Vec2(nameX, ry), Vec2(nameW, rowH));
        L.ctlR[i]  = Rect(Vec2(ctlX,  ry + 8.0f * s), Vec2(ctlW, rowH - 16.0f * s));
        L.idR[i]   = Rect(Vec2(idX,   ry + (rowH - 26.0f * s) * 0.5f), Vec2(idW, 26.0f * s));
        if (i == C_SW) {
            // 开关：形状 + 图案 两个窄下拉并排
            float halfW = (dropW - 8.0f * s) * 0.5f;
            L.drop1R[i] = Rect(Vec2(dropX, ry + (rowH - 30.0f * s) * 0.5f), Vec2(halfW, 30.0f * s));
            L.drop2R    = Rect(Vec2(dropX + halfW + 8.0f * s, ry + (rowH - 30.0f * s) * 0.5f),
                               Vec2(halfW, 30.0f * s));
        } else {
            L.drop1R[i] = Rect(Vec2(dropX, ry + (rowH - 30.0f * s) * 0.5f), Vec2(dropW, 30.0f * s));
        }
        ry += rowH;
    }

    // ---- 区域② 分割线交互台 ----
    y = L.area1.pos.y + L.area1.size.y + 16.0f * s;
    const float demoH = 150.0f * s;
    L.area2 = Rect(Vec2(cx, y), Vec2(cw, pad * 2 + header + demoH));
    L.demo  = Rect(Vec2(cx + pad, L.area2.pos.y + pad + header), Vec2(cw - pad * 2, demoH));
    L.spDropR = Rect(Vec2(dropX, L.area2.pos.y + pad + (header - 30.0f * s) * 0.5f),
                     Vec2(dropW, 30.0f * s));

    // ---- 区域③ 滚动条交互台（内嵌真滚动容器 + 模板切换下拉）----
    y = L.area2.pos.y + L.area2.size.y + 16.0f * s;
    const float scHead = 34.0f * s;        // 头行（标题 + 徽标 + 下拉）
    const float scH    = 170.0f * s;       // 滚动容器高（列表远超此高 → 自动生滚动条）
    L.area3 = Rect(Vec2(cx, y), Vec2(cw, pad * 2 + scHead + scH));
    L.scrollBox = Rect(Vec2(cx + pad, L.area3.pos.y + pad + scHead), Vec2(cw - pad * 2, scH));
    L.scDropR   = Rect(Vec2(dropX, L.area3.pos.y + pad + (scHead - 30.0f * s) * 0.5f),
                       Vec2(dropW, 30.0f * s));
    L.scBadgeR  = Rect(Vec2(idX,   L.area3.pos.y + pad + (scHead - 26.0f * s) * 0.5f),
                       Vec2(idW, 26.0f * s));

    // ---- 区域④ 进度模板（下载进度 = 条形 / 点击切换加载 = 环形），在区域③下面 ----
    y = L.area3.pos.y + L.area3.size.y + 16.0f * s;
    const float pvRowH = 80.0f * s;   // 行加高：给大号环形进度留出完整空间
    L.area4 = Rect(Vec2(cx, y), Vec2(cw, pad * 2 + header + pvRowH * 2.0f));
    {
        float py = L.area4.pos.y + pad + header;
        for (int i = 0; i < 2; i++) {
            L.pvNameR[i] = Rect(Vec2(nameX, py), Vec2(nameW, pvRowH));
            L.pvCtlR[i]  = Rect(Vec2(ctlX,  py + 8.0f * s), Vec2(ctlW, pvRowH - 16.0f * s));
            L.pvIdR[i]   = Rect(Vec2(idX,   py + (pvRowH - 26.0f * s) * 0.5f), Vec2(idW, 26.0f * s));
            L.pvDropR[i] = Rect(Vec2(dropX, py + (pvRowH - 30.0f * s) * 0.5f), Vec2(dropW, 30.0f * s));
            py += pvRowH;
        }
    }

    // ---- 区域⑤ 其余控件（命名+编号）：名录中 kind≠K_NONE 的项各一行 ----
    y = L.area4.pos.y + L.area4.size.y + 16.0f * s;
    const float exRowH = 56.0f * s;
    L.exCount = exCount();
    if (L.exCount > Layout::EX_MAX) L.exCount = Layout::EX_MAX;
    L.area5 = Rect(Vec2(cx, y), Vec2(cw, pad * 2 + header + exRowH * (float)L.exCount));
    {
        float ey = L.area5.pos.y + pad + header;
        for (int i = 0; i < L.exCount; i++) {
            L.exNameR[i] = Rect(Vec2(nameX, ey), Vec2(nameW, exRowH));
            L.exCtlR[i]  = Rect(Vec2(ctlX,  ey + 8.0f * s), Vec2(ctlW, exRowH - 16.0f * s));
            L.exIdR[i]   = Rect(Vec2(idX,   ey + (exRowH - 26.0f * s) * 0.5f), Vec2(idW, 26.0f * s));
            L.exDropR[i] = Rect(Vec2(dropX, ey + (exRowH - 30.0f * s) * 0.5f), Vec2(dropW, 30.0f * s));
            const tpl::ControlEntry* e = exEntry(i);
            L.exPartN[i] = e ? exParts(e->kind, L.exCtlR[i], s, L.exPart[i]) : 0;
            ey += exRowH;
        }
    }

    L.contentH = (L.area5.pos.y + L.area5.size.y) - env.content.pos.y + 16.0f * s;
    return L;
}

} // namespace directory

// 演示区底板（半透明圆角 + 淡边框）
static void drawAreaPanel(RenderDevice* dev, const Rect& r, const char* title, const Color& tint) {
    float s = g_uiCtx.scale;
    ThemeColors& c = currentColors();
    float rad = 10.0f * s;
    drawRoundedRect(dev, r, Color(c.bg2.r, c.bg2.g, c.bg2.b, 90.0f / 255.0f), rad);
    drawRoundedBorder(dev, r, Color(c.groupBorder.r, c.groupBorder.g, c.groupBorder.b, 90.0f / 255.0f),
                      g_uiCtx.borderWidth(), rad);
    pctl::drawTextLeft(dev, g_uiCtx.smallFont, title,
                       Rect(Vec2(r.pos.x + 14.0f * s, r.pos.y + 6.0f * s),
                            Vec2(r.size.x - 28.0f * s, 20.0f * s)), tint);
}

// ---- 区域⑤ 样例（真交互）：按名录 kind 绘制；状态取自页面 State（hover/选中/编辑）----
// partsL = 逻辑坐标子部件；内部统一按 dy 平移到屏幕坐标后绘制。
static void drawExtraSample(PageEnv& env, directory::State& st, int kind,
                            const Rect& crL, const Rect partsL[4], int partN,
                            float dy, Font* sf, int styleId) {
    using namespace directory;
    RenderDevice* dev = env.dev;
    ThemeColors& c = currentColors();
    float s = g_uiCtx.scale;
    int hov = st.hover, hp = st.hoverPart;
    Rect cr = tr(crL, dy);
    Rect parts[4];
    for (int j = 0; j < 4; j++) parts[j] = tr(partsL[j], dy);
    switch (kind) {
        case tpl::K_TAB:            // 页签：点击选中（ACTIVE）/ 悬停 HOVER
            for (int j = 0; j < partN && j < 2; j++) {
                int sp = (j == st.exTab) ? pctl::STATE_ACTIVE
                       : (hov == HIT_EX_TAB && hp == j ? pctl::STATE_HOVER : pctl::STATE_NORMAL);
                pctl::drawTabItem(dev, parts[j], j == 0 ? u8"页签 A" : u8"页签 B", sp, sf);
            }
            break;
        case tpl::K_SIDE: {         // 侧栏项：点击切换选中
            int sp = st.exSide ? pctl::STATE_ACTIVE
                   : (hov == HIT_EX_SIDE ? pctl::STATE_HOVER : pctl::STATE_NORMAL);
            pctl::drawSideItem(dev, parts[0], u8"侧栏项", sp, sf);
            break;
        }
        case tpl::K_SEG: {          // 视图模式：点击选中某段
            static const char* const OPT[3] = { u8"列表", u8"网格", u8"详情" };
            for (int j = 0; j < partN && j < 3; j++) {
                int sp = (j == st.exSeg) ? pctl::STATE_ACTIVE
                       : (hov == HIT_EX_SEG && hp == j ? pctl::STATE_HOVER : pctl::STATE_NORMAL);
                pctl::drawSegItem(dev, parts[j], OPT[j], sp, sf);
            }
            break;
        }
        case tpl::K_STEPPER: {      // 计数器：−/＋ 步进，或点数值区直接编辑（小数/负数）
            TextInputSystem& tis = g_winMgr.textInput();
            bool focused = tis.focused(st.exStepEdit);
            pctl::drawStepper(dev, parts[0], parts[1], parts[2], st.exStepEdit.buf.c_str(),
                              hov == HIT_EX_STEP_M, hov == HIT_EX_STEP_P, sf, sf,
                              focused, focused && tis.caretOn(),
                              focused ? st.exStepEdit.caret : -1,
                              hov == HIT_EX_STEP_V);
            break;
        }
        case tpl::K_INPUT: {        // 输入框：点击聚焦，走系统 TextInputSystem 可编辑
            TextInputSystem& tis = g_winMgr.textInput();
            bool focused = tis.focused(st.exInput);
            wctl::drawInput(dev, parts[0], st.exInput.buf.c_str(),
                            st.exInput.placeholder.c_str(), focused,
                            focused && tis.caretOn(), 1.0f, 12.0f,
                            focused ? st.exInput.caret : -1);
            break;
        }
        case tpl::K_CHECK: {        // 复选框：点击勾选；勾形由本行下拉（游戏ID）单源控制
            float d = parts[0].size.y; if (d > 22.0f * s) d = 22.0f * s;
            Rect box(Vec2(parts[0].pos.x, parts[0].pos.y + (parts[0].size.y - d) * 0.5f), Vec2(d, d));
            wctl::drawCheck(dev, box, st.exCheck, hov == HIT_EX_CHECK, 1.0f, styleId);
            pctl::drawTextLeft(dev, sf, u8"复选框（点我勾选）",
                Rect(Vec2(box.pos.x + d + 10.0f * s, parts[0].pos.y),
                     Vec2(parts[0].size.x - d - 10.0f * s, parts[0].size.y)),
                Color(c.text.r, c.text.g, c.text.b, 215.0f / 255.0f));
            break;
        }
        case tpl::K_PROGRESS: {     // 条形进度：实时循环（活数据）
            float h = 14.0f * s;
            float t = st.ringAnim * 0.3333f;   // 约 3s 一轮
            t -= (float)(int)t;                // 取小数 → 0~1
            if (t < 0.0f) t = 0.0f;
            wctl::drawProgress(dev, Rect(Vec2(cr.pos.x, cr.pos.y + (cr.size.y - h) * 0.5f),
                                         Vec2(cr.size.x, h)), t);
            break;
        }
        case tpl::K_DROPDOWN: {     // 下拉按钮样例：真下拉（按钮绘制随 g_uiCtx.dropdownStyle）
            static const char* const OPTS[] = { u8"选项一", u8"选项二", u8"选项三" };
            char did[28]; snprintf(did, sizeof(did), "dir_dd_exsmp");
            g_dropdowns.draw(env.dev, env.vwin(),
                { parts[0], OPTS, 3, st.dsSel, &st.dsSel, &st.dsDirty, did, 24.0f * s, sf,
                  env.content.pos.y, env.content.pos.y + env.content.size.y }, 1.0f);
            break;
        }
        default: break;
    }
}

// ---------------- render ----------------

static void renderDirectory(PageEnv& env) {
    using namespace directory;
    Layout L = layout(env);
    ThemeColors& c = currentColors();
    float s = env.scale();
    Font* f  = env.font();
    Font* sf = env.smallFont();
    Color name(c.text.r, c.text.g, c.text.b, 215.0f / 255.0f);
    Color cur (c.accent.r, c.accent.g, c.accent.b, 235.0f / 255.0f);
    Color dim (c.text.r, c.text.g, c.text.b, 150.0f / 255.0f);

    State& st = env.state<State>();
    if (!st.init) {
        for (int i = 0; i < DD_COUNT; i++) {
            int* fl = ddField(i);
            st.sel[i] = tpl::idOf(fl ? *fl : 0);
            st.dirty[i] = false;
        }
        // 区域⑤ 行尾下拉：按控件族初值（布局已先行 → L 可用）
        for (int i = 0; i < L.exCount; i++) {
            const tpl::ControlEntry* e = exEntry(i);
            int* fl = e ? ddExField(e->kind) : nullptr;
            st.exSel[i] = tpl::idOf(fl ? *fl : 0);
            st.exDirty[i] = false;
        }
        // 计数器数值框：初始文本 + 提交回调（Enter/失焦时解析，小数/负数皆可；
        // 非法/空输入 → 回写原值，规范化显示）
        exStepSync(st);
        st.exStepEdit.onCommit = [&st](const std::string& v) {
            float fv = 0.0f;
            if (sscanf(v.c_str(), "%f", &fv) == 1) st.exStepVal = fv;
            exStepSync(st);
        };
        st.init = true;
    }

    RegionRequest req;
    req.x = 0; req.y = 0;
    req.w = env.content.size.x; req.h = env.content.size.y;
    req.tabId = 0; req.id = REGION_ID;
    RegionHandle h = g_regions.begin(env.dev, env.vwin(), req);
    if (!h.valid) return;
    float dy = -h.scrollY;

    // 顶部页标题
    pctl::drawTextLeft(env.dev, sf, TITLE,
        tr(Rect(Vec2(env.content.pos.x + 2.0f * s, env.content.pos.y + 4.0f * s),
                Vec2(env.content.size.x, 20.0f * s)), dy), dim);

    // ================= 区域① 控件交互台 =================
    drawAreaPanel(env.dev, tr(L.area1, dy), u8"① 控件交互台（点 / 拖 / 切 即时生效）", cur);

    for (int i = 0; i < C_ROWCOUNT; i++) {
        Ctrl ct = (Ctrl)i;
        // 名称
        pctl::drawTextLeft(env.dev, (i == 0 ? f : sf), ROW_NAME[i],
            tr(L.nameR[i], dy), name);
        Rect cr = tr(L.ctlR[i], dy);
        switch (ct) {
            case C_BTN: {
                bool hov = (st.hover == HIT_BTN) || st.btnPressed;
                char txt[32]; snprintf(txt, sizeof(txt), u8"按钮（点击 %d）", st.btnClicks);
                // id=0 → 跟随全局 g_uiCtx.btnStyle（下拉选编号即改全局 → 立即变样）
                Rect br = Rect(Vec2(cr.pos.x, cr.pos.y + (cr.size.y - 32.0f * s) * 0.5f),
                               Vec2(150.0f * s, 32.0f * s));
                wctl::drawBtn(env.dev, br, txt, hov, 1.0f, 12.0f * s, 1.0f, 0);
            } break;
            case C_SLIDER: {
                float gy = cr.pos.y + cr.size.y * 0.5f - 6.0f * s;
                Rect g(Vec2(cr.pos.x, gy), Vec2(cr.size.x - 44.0f * s, 16.0f * s));
                pctl::drawSliderFull(env.dev, g, st.sliderVal,
                                     st.hover == HIT_SLIDER || st.dragSlider, 1.0f);
                char vb[16]; snprintf(vb, sizeof(vb), "%d%%", (int)(st.sliderVal * 100.0f + 0.5f));
                pctl::drawTextRight(env.dev, sf, vb,
                    Rect(Vec2(cr.pos.x + cr.size.x - 40.0f * s, cr.pos.y),
                         Vec2(40.0f * s, cr.size.y)), cur);
            } break;
            case C_SW: {
                Rect swr(Vec2(cr.pos.x, cr.pos.y + (cr.size.y - 28.0f * s) * 0.5f),
                         Vec2(56.0f * s, 28.0f * s));
                pctl::drawToggle(env.dev, swr, st.swOn);
                pctl::drawTextLeft(env.dev, sf, st.swOn ? u8"开" : u8"关",
                    Rect(Vec2(swr.pos.x + swr.size.x + 10.0f * s, cr.pos.y),
                         Vec2(40.0f * s, cr.size.y)), st.hover == HIT_SW ? cur : dim);
            } break;
            case C_BALL: {
                int v = tpl::internalOf(0, g_uiCtx.ballStyle);   // 跟随全局（内部 0=圆形 1=方形 >1=特别绘制）
                float cyb = cr.pos.y + cr.size.y * 0.5f, cxb = cr.pos.x + 18.0f * s;
                Color fill(c.accent.r, c.accent.g, c.accent.b, 230.0f / 255.0f);
                if (v == 0) fillCircle(env.dev, cxb, cyb, 14.0f * s, fill);
                else if (v == 1) drawRoundedRect(env.dev, Rect(Vec2(cxb - 14.0f * s, cyb - 14.0f * s),
                                                   Vec2(28.0f * s, 28.0f * s)), fill, 0.0f);
                else wctl::drawBallStyled(env.dev, cxb, cyb, 14.0f * s, 230.0f / 255.0f, v);
                pctl::drawTextLeft(env.dev, sf, u8"点击打开测试悬浮窗",
                    Rect(Vec2(cxb + 24.0f * s, cr.pos.y), Vec2(cr.size.x - 24.0f * s, cr.size.y)),
                    st.hover == HIT_BALL ? cur : dim);
            } break;
        }

        // 游戏ID 徽标（系统<控件>#N；N = 该控件当前全局值）
        int* fl = ddField(rowDD(i));
        int gid = tpl::idOf(fl ? *fl : 0);
        char idb[48]; snprintf(idb, sizeof(idb), u8"系统%s#%d", ROW_NAME[i], gid);
        Rect br = tr(L.idR[i], dy);
        float rad = 13.0f * s;
        drawRoundedRect(env.dev, br, Color(c.accent.r, c.accent.g, c.accent.b, 36.0f / 255.0f), rad);
        drawRoundedBorder(env.dev, br, Color(c.accent.r, c.accent.g, c.accent.b, 130.0f / 255.0f),
                          g_uiCtx.borderWidth(), rad);
        pctl::drawTextCentered(env.dev, sf, idb, br,
            Color(c.accent2.r, c.accent2.g, c.accent2.b, 245.0f / 255.0f));

        // 主风格下拉（开关=形状；其余=本控件风格）
        // 行→槽位必须走 rowDD(i)：直接用行索引当 DD 槽位会因 DD_SWEX 错位，
        // 使「悬浮球」行下拉错接成"开关扩展图案"（参数不对之根因）。
        int drow = rowDD(i);
        char did[24]; snprintf(did, sizeof(did), "dir_dd%d", drow);
        g_dropdowns.draw(env.dev, env.vwin(),
            { tr(L.drop1R[i], dy), ddNames(drow), ddCount(drow),
              st.sel[drow], &st.sel[drow], &st.dirty[drow],
              did, 24.0f * s, sf, env.content.pos.y, env.content.pos.y + env.content.size.y }, 1.0f);

        // 开关行：扩展图案下拉（switchExtra）
        if (ct == C_SW) {
            g_dropdowns.draw(env.dev, env.vwin(),
                { tr(L.drop2R, dy), tpl::swExtra::NAMES, tpl::swExtra::COUNT,
                  st.sel[DD_SWEX], &st.sel[DD_SWEX], &st.dirty[DD_SWEX],
                  "dir_dd_swx", 24.0f * s, sf,
                  env.content.pos.y, env.content.pos.y + env.content.size.y }, 1.0f);
        }
    }

    // ================= 区域② 分割线交互台 =================
    drawAreaPanel(env.dev, tr(L.area2, dy), u8"② 分割线交互台（拖动分割线改占比）", cur);

    Rect demoS = tr(L.demo, dy);
    const float paneMin = 32.0f * s;
    // 滚动补偿：分割线 y 是屏幕绝对坐标，页面滚动时同步平移
    if (st.spInited && st.spLastDy != dy) st.spPos += dy - st.spLastDy;
    st.spLastDy = dy;
    if (!st.spInited) {
        st.spPos = demoS.pos.y + demoS.size.y * 0.45f;
        st.spInited = true;
    }
    if (st.spPos < demoS.pos.y + paneMin) st.spPos = demoS.pos.y + paneMin;
    if (st.spPos > demoS.pos.y + demoS.size.y - paneMin)
        st.spPos = demoS.pos.y + demoS.size.y - paneMin;

    // 两面板（上下）+ 占比标注
    float gap = 5.0f * s;
    Rect topP   (Vec2(demoS.pos.x, demoS.pos.y), Vec2(demoS.size.x, st.spPos - gap - demoS.pos.y));
    Rect botP   (Vec2(demoS.pos.x, st.spPos + gap),
                 Vec2(demoS.size.x, demoS.pos.y + demoS.size.y - st.spPos - gap));
    {
        char lb[48];
        float tpct = topP.size.y / demoS.size.y;
        snprintf(lb, sizeof(lb), u8"上面板 %d%%", (int)(tpct * 100.0f + 0.5f));
        drawRoundedRect(env.dev, topP, Color(c.bg2.r, c.bg2.g, c.bg2.b, 130.0f / 255.0f), 6.0f * s);
        pctl::drawTextCentered(env.dev, f, lb, topP, Color(c.text.r, c.text.g, c.text.b, 210.0f / 255.0f));
        snprintf(lb, sizeof(lb), u8"下面板 %d%%", (int)((1.0f - tpct) * 100.0f + 0.5f));
        drawRoundedRect(env.dev, botP, Color(c.bg3.r, c.bg3.g, c.bg3.b, 120.0f / 255.0f), 6.0f * s);
        pctl::drawTextCentered(env.dev, f, lb, botP, Color(c.text.r, c.text.g, c.text.b, 210.0f / 255.0f));
    }
    // 可拖动分割线（系统派发：拖动写 st.spPos；风格随 g_uiCtx.splitterStyle 即改）
    Rect spBar(Vec2(demoS.pos.x + 6.0f * s, st.spPos - 5.0f * s),
               Vec2(demoS.size.x - 12.0f * s, 10.0f * s));
    g_splitters.draw(env.dev, env.vwin(),
        { spBar, false, demoS.pos.y + paneMin, demoS.pos.y + demoS.size.y - paneMin,
          &st.spPos, nullptr, "dir_sp" }, 1.0f);

    // 分割线风格下拉 + 徽标
    {
        char idb[48]; snprintf(idb, sizeof(idb), u8"系统分割线#%d", tpl::idOf(g_uiCtx.splitterStyle));
        Rect br = tr(L.idR[C_BTN], dy);      // 复用同列 x，行位在区域②头
        br.pos.y = L.area2.pos.y + 8.0f * s + dy;
        float rad = 13.0f * s;
        drawRoundedRect(env.dev, br, Color(c.accent.r, c.accent.g, c.accent.b, 36.0f / 255.0f), rad);
        drawRoundedBorder(env.dev, br, Color(c.accent.r, c.accent.g, c.accent.b, 130.0f / 255.0f),
                          g_uiCtx.borderWidth(), rad);
        pctl::drawTextCentered(env.dev, sf, idb, br,
            Color(c.accent2.r, c.accent2.g, c.accent2.b, 245.0f / 255.0f));
    }
    g_dropdowns.draw(env.dev, env.vwin(),
        { tr(L.spDropR, dy), tpl::splitter::NAMES, tpl::splitter::COUNT,
          st.sel[DD_SPLIT], &st.sel[DD_SPLIT], &st.dirty[DD_SPLIT],
          "dir_dd_sp", 24.0f * s, sf,
          env.content.pos.y, env.content.pos.y + env.content.size.y }, 1.0f);

    // ================= 区域③ 滚动条交互台 =================
    drawAreaPanel(env.dev, tr(L.area3, dy),
                  u8"③ 滚动条交互台（切换滚动条模板，系统自动让位）", cur);
    {
        // 内嵌真滚动容器：与页面同一套 RegionSystem（独立 RegionState/独立滚动位置，
        // 系统自动裁剪/滚轮/滚动条）；列表远超容器高 → 自动生成本容器滚动条=当前模板。
        Rect box = tr(L.scrollBox, dy);
        RegionRequest sr;
        sr.x = box.pos.x - env.content.pos.x;   // 相对虚拟窗原点（=内容原点）
        sr.y = box.pos.y - env.content.pos.y;
        sr.w = box.size.x; sr.h = box.size.y;
        sr.tabId = 0; sr.id = "dir_scroll_demo";
        RegionHandle sh = g_regions.begin(env.dev, env.vwin(), sr);
        if (sh.valid) {
            const int N = 24;
            const float itemH = 26.0f * s;
            for (int i = 0; i < N; i++) {
                float ry = box.pos.y - sh.scrollY + i * itemH;
                if (ry + itemH < box.pos.y || ry > box.pos.y + box.size.y) continue;  // 仅画可视行
                Rect rowR(Vec2(box.pos.x + 8.0f * s, ry + 2.0f * s),
                          Vec2(sh.contentW - 16.0f * s, itemH - 4.0f * s));
                Color rc = (i % 2) ? Color(c.bg3.r, c.bg3.g, c.bg3.b, 150.0f / 255.0f)
                                   : Color(c.bg2.r, c.bg2.g, c.bg2.b, 150.0f / 255.0f);
                drawRoundedRect(env.dev, rowR, rc, 4.0f * s);
                char t[32]; snprintf(t, sizeof(t), u8"列表项 %02d", i + 1);
                pctl::drawTextLeft(env.dev, sf, t, rowR, dim);
            }
            g_regions.end(env.dev, sh, (float)N * itemH);
        }
    }
    // 滚动条风格下拉 + 徽标（游戏ID；选编号写回全局，下一帧滚动条即变样）
    {
        char idb[48]; snprintf(idb, sizeof(idb), u8"系统滚动条#%d", tpl::idOf(g_uiCtx.scrollbarStyle));
        Rect br = tr(L.scBadgeR, dy);
        float rad = 13.0f * s;
        drawRoundedRect(env.dev, br, Color(c.accent.r, c.accent.g, c.accent.b, 36.0f / 255.0f), rad);
        drawRoundedBorder(env.dev, br, Color(c.accent.r, c.accent.g, c.accent.b, 130.0f / 255.0f),
                          g_uiCtx.borderWidth(), rad);
        pctl::drawTextCentered(env.dev, sf, idb, br,
            Color(c.accent2.r, c.accent2.g, c.accent2.b, 245.0f / 255.0f));
    }
    g_dropdowns.draw(env.dev, env.vwin(),
        { tr(L.scDropR, dy), tpl::scrollbar::NAMES, tpl::scrollbar::COUNT,
          st.sel[DD_SCROLL], &st.sel[DD_SCROLL], &st.dirty[DD_SCROLL],
          "dir_dd_sc", 24.0f * s, sf,
          env.content.pos.y, env.content.pos.y + env.content.size.y }, 1.0f);

    // ================= 区域④ 进度模板（下载进度 / 点击切换加载） =================
    // 旧版（D:\UI自研\主修改\988-22）UI模板页里就有这两项，本轮重写时丢失；
    // 现按用户要求接回，作为区域③下面的「单独模板」区：下载进度（条形）+ 点击切换加载（环形）。
    drawAreaPanel(env.dev, tr(L.area4, dy),
                  u8"④ 进度模板（下载进度 / 点击切换加载）", cur);
    st.ringAnim += env.dt;                 // 动画时钟（进度循环 + 环形旋转共用）
    for (int i = 0; i < 2; i++) {
        // 行名
        pctl::drawTextLeft(env.dev, sf, (i == 0 ? u8"下载进度" : u8"点击切换加载"),
            tr(L.pvNameR[i], dy), name);
        Rect cr = tr(L.pvCtlR[i], dy);
        if (i == 0) {
            // 下载进度（原始条形绘制，4s 循环 0→100%）：走旧版 pctl::drawProgressBar，
            // 与滑动条无关（滑动条是另一控件；条形进度只作自身演示，绝不复用滑动条绘制）。
            float t = fmodf(st.ringAnim / 4.0f, 1.0f);
            float barRight = L.pvDropR[0].pos.x + L.pvDropR[0].size.x;   // 铺满右侧徽标/下拉列，避免留白
            float barH = 14.0f * s;
            Rect bar(Vec2(cr.pos.x, cr.pos.y + (cr.size.y - barH) * 0.5f + 10.0f * s),
                     Vec2(barRight - cr.pos.x, barH));
            pctl::drawProgressBar(env.dev, bar, t, u8"下载进度", sf);
        } else {
            // 点击切换加载（环形）：点击环 → 加载态/进度态互换；模板由 tpl::ring 下拉控制。
            // 放大：环直径 = min(控件高, 60*s)；环内数字/省略号改用 small 字体 → 不再压到环轨上。
            float d = cr.size.y; if (d > 60.0f * s) d = 60.0f * s;
            Rect ringR(Vec2(cr.pos.x, cr.pos.y + (cr.size.y - d) * 0.5f), Vec2(d, d));
            pctl::drawCircularProgress(env.dev, ringR, st.loading, st.ringAnim,
                                       sf, sf, sf, nullptr);
            pctl::drawTextLeft(env.dev, sf, u8"点击切换加载",
                Rect(Vec2(cr.pos.x + d + 16.0f * s, cr.pos.y),
                     Vec2(cr.size.x - d - 16.0f * s, cr.size.y)),
                st.hover == HIT_RING ? cur : dim);
        }

        // 环形进度行：游戏ID 徽标 + 模板下拉（下载进度行不挂滑动条模板，故不显示徽标/下拉）
        if (i == 1) {
            int* fl = ddField(DD_RING);
            int gid = tpl::idOf(fl ? *fl : 0);
            char idb[48]; snprintf(idb, sizeof(idb), u8"系统%s#%d", u8"环形进度", gid);
            Rect br2 = tr(L.pvIdR[i], dy);
            float rad = 13.0f * s;
            drawRoundedRect(env.dev, br2, Color(c.accent.r, c.accent.g, c.accent.b, 36.0f / 255.0f), rad);
            drawRoundedBorder(env.dev, br2, Color(c.accent.r, c.accent.g, c.accent.b, 130.0f / 255.0f),
                              g_uiCtx.borderWidth(), rad);
            pctl::drawTextCentered(env.dev, sf, idb, br2,
                Color(c.accent2.r, c.accent2.g, c.accent2.b, 245.0f / 255.0f));

            int dd = DD_RING;
            char did[24]; snprintf(did, sizeof(did), "dir_dd_pv%d", dd);
            g_dropdowns.draw(env.dev, env.vwin(),
                { tr(L.pvDropR[i], dy), ddNames(dd), ddCount(dd),
                  st.sel[dd], &st.sel[dd], &st.dirty[dd],
                  did, 24.0f * s, sf,
                  env.content.pos.y, env.content.pos.y + env.content.size.y }, 1.0f);
        }
    }

    // ================= 区域⑤ 其余控件（统一命名 + 编号） =================
    // 行直接从全控件名录（StyleTpl.h 单源）取：名字 + 样例 + 系统<控件>#N 徽标。
    // 这些控件（页签/侧栏项/分段按钮/计数器/输入框/复选框/条形进度）此前未纳入编号体系，现全部统一。
    drawAreaPanel(env.dev, tr(L.area5, dy), u8"⑤ 其余控件（命名 + 编号）", cur);
    for (int i = 0; i < L.exCount; i++) {
        const tpl::ControlEntry* e = exEntry(i);
        if (!e) continue;
        // 行名
        pctl::drawTextLeft(env.dev, sf, e->name, tr(L.exNameR[i], dy), name);
        // 样例（按名录 kind 走既有控件原语；状态取自 st → 真交互，非静态）
        drawExtraSample(env, st, e->kind, L.exCtlR[i], L.exPart[i], L.exPartN[i], dy, sf, st.exSel[i]);
        // 游戏ID 徽标（系统<控件>#N，N = 该行下拉当前选中 → 报编号即可定位）
        char idb[48]; snprintf(idb, sizeof(idb), u8"系统%s#%d", e->name, st.exSel[i]);
        Rect br = tr(L.exIdR[i], dy);
        float rad = 13.0f * s;
        drawRoundedRect(env.dev, br, Color(c.accent.r, c.accent.g, c.accent.b, 36.0f / 255.0f), rad);
        drawRoundedBorder(env.dev, br, Color(c.accent.r, c.accent.g, c.accent.b, 130.0f / 255.0f),
                          g_uiCtx.borderWidth(), rad);
        pctl::drawTextCentered(env.dev, sf, idb, br,
            Color(c.accent2.r, c.accent2.g, c.accent2.b, 245.0f / 255.0f));
        // 行尾「模板」下拉（游戏ID 切换；选完写回该控件族全局字段，下一帧样例即变样）
        const char* const* dn = ddExNames(e->kind);
        int dc = ddExCount(e->kind);
        if (dn && dc > 0) {
            char did[24]; snprintf(did, sizeof(did), "dir_dd_ex%d", i);
            g_dropdowns.draw(env.dev, env.vwin(),
                { tr(L.exDropR[i], dy), dn, dc, st.exSel[i], &st.exSel[i], &st.exDirty[i],
                  did, 24.0f * s, sf,
                  env.content.pos.y, env.content.pos.y + env.content.size.y }, 1.0f);
        }
    }

    // 下拉落库：索引(游戏ID) → 内部渲染值（下一帧真控件即反映新风格）
    for (int i = 0; i < DD_COUNT; i++) {
        if (st.dirty[i]) {
            int* fl = ddField(i);
            if (fl) *fl = tpl::internalOf(st.sel[i], *fl);
            st.dirty[i] = false;
        }
    }
    // 区域⑤ 行尾下拉落库（写回各自控件族字段）
    for (int i = 0; i < L.exCount; i++) {
        if (st.exDirty[i]) {
            const tpl::ControlEntry* e = exEntry(i);
            int* fl = e ? ddExField(e->kind) : nullptr;
            if (fl) *fl = tpl::internalOf(st.exSel[i], *fl);
            st.exDirty[i] = false;
        }
    }

    g_regions.end(env.dev, h, L.contentH);
    g_dropdowns.renderPopups(env.dev, env.vwin(), 1.0f);
}

// ---------------- click：真控件交互（下拉/分割 系统优先） ----------------

static bool clickDirectory(PageEnv& env, const Vec2& m) {
    using namespace directory;
    Layout L = layout(env);
    State& st = env.state<State>();
    float scroll = g_regions.getScroll(env.vwin().id, REGION_ID);
    Vec2 lm(m.x, m.y + scroll);

    // 系统派发优先：下拉（吞点击）→ 分割线（吞点击进入拖拽）
    if (g_dropdowns.handlePress(env.vwin(), m).consumed) return true;
    if (g_splitters.handlePress(env.vwin(), m).consumed) return true;

    for (int i = 0; i < C_ROWCOUNT; i++) {
        if (!L.ctlR[i].contains(lm)) continue;
        switch ((Ctrl)i) {
            case C_BTN:
                st.btnClicks++; st.btnPressed = true; return true;
            case C_SLIDER:
                st.dragSlider = true;
                st.sliderVal = wctl::sliderValueAt(
                    Rect(Vec2(L.ctlR[i].pos.x, L.ctlR[i].pos.y),
                         Vec2(L.ctlR[i].size.x - 44.0f * env.scale(), L.ctlR[i].size.y)), lm.x);
                return true;
            case C_SW:
                st.swOn = !st.swOn; return true;
            case C_BALL:
                // 悬浮球真实语义 = 悬浮窗的收拢态：点击 = 打开（恢复）测试悬浮窗。
                // 系统派发：经 WindowManager 配置表驱动打开（配置加一条 + 处理器注册，
                // 框架零改动）；帧末 openPending 统一创建，与底栏 open: 动作同通路。
                g_winMgr.openWindow("ball_demo");
                return true;
        }
    }

    // 区域④：点击环形进度 → 切换 加载态 / 进度态（"点击切换加载"）
    {
        Rect cr = L.pvCtlR[1];
        float d = cr.size.y;
        Rect ringR(Vec2(cr.pos.x, cr.pos.y + (cr.size.y - d) * 0.5f), Vec2(d, d));
        if (ringR.contains(lm)) { st.loading = !st.loading; return true; }
    }

    // 区域⑤：其余控件真交互（页签/侧栏/分段/计数器/输入框/复选框）
    {
        int row = -1, part = -1;
        if (exHit(L, lm, row, part)) {
            const tpl::ControlEntry* e = exEntry(row);
            if (e) switch (e->kind) {
                case tpl::K_TAB:   st.exTab = part;          return true;
                case tpl::K_SIDE:  st.exSide = !st.exSide;   return true;
                case tpl::K_SEG:   st.exSeg = part;          return true;
                case tpl::K_STEPPER:
                    if      (part == 0) { st.exStepVal -= 1.0f;  exStepSync(st); }
                    else if (part == 2) { st.exStepVal += 1.0f;  exStepSync(st); }
                    else if (part == 1) {   // 数值区：聚焦 + 定位光标（可直接输小数/负数）
                        TextInputSystem& tis = g_winMgr.textInput();
                        tis.requestFocus(TEXT_OWNER_PAGE, env.ui->pageSel(), st.exStepEdit);
                        tis.placeCaret(st.exStepEdit, tr(L.exPart[row][1], -scroll), m.x);
                    }
                    return true;
                case tpl::K_INPUT: {   // 聚焦并定位光标（系统 TextInputSystem 托管编辑）
                    TextInputSystem& tis = g_winMgr.textInput();
                    tis.requestFocus(TEXT_OWNER_PAGE, env.ui->pageSel(), st.exInput);
                    tis.placeCaret(st.exInput, tr(L.exPart[row][0], -scroll), m.x);
                    return true;
                }
                case tpl::K_CHECK: st.exCheck = !st.exCheck; return true;
                default: break;
            }
        }
    }
    return false;
}

static void hoverDirectory(PageEnv& env, const Vec2& m) {
    using namespace directory;
    Layout L = layout(env);
    State& st = env.state<State>();
    float scroll = g_regions.getScroll(env.vwin().id, REGION_ID);
    Vec2 lm(m.x, m.y + scroll);

    g_dropdowns.updateHover(env.vwin(), m);
    g_splitters.updateHover(env.vwin(), m);

    st.hover = HIT_NONE;
    st.hoverPart = -1;
    for (int i = 0; i < C_ROWCOUNT; i++) {
        if (L.ctlR[i].contains(lm)) { st.hover = i; return; }
    }
    if (L.pvCtlR[1].contains(lm)) { st.hover = HIT_RING; return; }

    // 区域⑤：其余控件行内子部件（页签/侧栏/分段/计数器/输入框/复选框）
    {
        int row = -1, part = -1;
        if (exHit(L, lm, row, part)) {
            const tpl::ControlEntry* e = exEntry(row);
            if (e) switch (e->kind) {
                case tpl::K_TAB:     st.hover = HIT_EX_TAB;   break;
                case tpl::K_SIDE:    st.hover = HIT_EX_SIDE;  break;
                case tpl::K_SEG:     st.hover = HIT_EX_SEG;   break;
                case tpl::K_STEPPER: st.hover = (part == 0 ? HIT_EX_STEP_M
                                              : part == 1 ? HIT_EX_STEP_V
                                              : HIT_EX_STEP_P); break;
                case tpl::K_INPUT:   st.hover = HIT_EX_INPUT; break;
                case tpl::K_CHECK:   st.hover = HIT_EX_CHECK; break;
                default: break;
            }
            st.hoverPart = part;
        }
    }
}

// ---------------- drag / release：滑动条跨帧拖拽 + 分割线拖动 ----------------

static void dragDirectory(PageEnv& env, const Vec2& m) {
    using namespace directory;
    Layout L = layout(env);
    State& st = env.state<State>();
    if (st.dragSlider) {
        const Rect& cr = L.ctlR[C_SLIDER];
        Rect g(Vec2(cr.pos.x, cr.pos.y), Vec2(cr.size.x - 44.0f * env.scale(), cr.size.y));
        st.sliderVal = wctl::sliderValueAt(g, m.x);
    }
    g_splitters.handleDrag(env.vwin(), m);
}

static void releaseDirectory(PageEnv& env, const Vec2&) {
    using namespace directory;
    State& st = env.state<State>();
    st.dragSlider = false;
    st.btnPressed = false;
    g_splitters.endDrag();
}

// ---------------- collect（字体预热） ----------------

static void collectDirectory(std::string& out) {
    using namespace directory;
    out += u8"UI 控件模板系统调用绘制游戏ID系统跟随下拉按钮的扩展绘制点拖切即时生效分割线可拖改占比";
    out += u8"控件交互台分割线交互台按钮滑动条开关悬浮球上面板下面板点击打开测试悬浮窗";
    out += u8"进度模板下载进度点击切换加载环形进度";
    out += u8"其余控件命名编号页签侧栏项分段按钮视图模式计数器输入框复选框条形进度输入框（示例）";
    for (int i = 0; i < DD_COUNT; i++) {
        const char* const* names = ddNames(i);
        int n = ddCount(i);
        for (int t = 0; names && t < n; t++) out += names[t];
    }
    // 区域⑤ 行尾下拉各控件族风格名 + 下拉按钮样例项（字体预热）
    {
        int n = 0; const tpl::ControlEntry* r = tpl::controlRoster(n);
        for (int i = 0; i < n; i++) {
            if (r[i].kind == tpl::K_NONE) continue;
            for (int t = 0; t < r[i].count; t++) out += r[i].styles[t];
        }
    }
    out += u8"选项一选项二选项三";
}

// ---------------- 注册 ----------------

void RegisterDirectoryPage(PageHandlerRegistry& reg) {
    PageHandler h;
    h.render  = renderDirectory;
    h.click   = clickDirectory;
    h.hover   = hoverDirectory;
    h.drag    = dragDirectory;
    h.release = releaseDirectory;
    h.collect = collectDirectory;
    reg.registerHandler("page_directory", h);
}

} // namespace ui