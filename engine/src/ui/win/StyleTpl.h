// AI生成
#pragma once
// ============================================================
// UI 控件模板「游戏ID → 绘制模板」程序映射表（全项目唯一单源）
//
// 游戏ID 规则（用户定，硬约束）：
//   0      = 系统跟随（跟随全局；= 不点名时的默认行为）
//   1      = 系统默认（全项目统一；各控件等价于其原始默认样式）
//   2..N   = 各内置绘制变体（可点名调用）
//   - 编号一律显式常量（不依赖数组下标/声明顺序）：新模板插中间只加常量、
//     不改旧号，旧号永不漂移。
//
// 「编号不是过场」：本表即"报编号 → 模板"的程序 ID 映射。
//   用户说"按钮 0 号" = FOLLOW（跟随全局）；"按钮 1 号" = 系统默认；
//   "按钮 2 号" = 方形；"按钮 3 号" = 圆润 …… AI 依本表秒懂。
//
// 存储 vs 展示（一次换算，禁止各处自行 ±1）：
//   对外/程序接口 : 游戏ID      = 0 / 1..N            ← 本表 NAMES 下标即 ID
//   内部渲染字段  : 0 基变体值  = ID - 1（ID<=0 视作 1）
//   换算：internalOf(id, fallback) / idOf(internal)
// ============================================================
#include <cstddef>

namespace ui {
namespace tpl {

// 游戏ID → 内部 0 基变体值（id<=0 时用 fallback；结果不小于 0）
inline int internalOf(int id, int fallback = 0) {
    int e = (id <= 0) ? fallback : id - 1;
    return e < 0 ? 0 : e;
}
// 内部 0 基变体值 → 游戏ID
inline int idOf(int internal) { return internal + 1; }

// ---- 按钮（wctl::drawBtn / drawBtnSecondary；g_uiCtx.btnStyle）----
namespace button {
enum : int { FOLLOW = 0, DEFAULT = 1, SQUARE = 2, ROUND = 3 };       // 游戏ID
enum : int { I_DEFAULT = 0, I_SQUARE = 1, I_ROUND = 2 };             // 内部 0 基
static const char* const NAMES[] = {
    u8"系统跟随", u8"系统默认", u8"方形", u8"圆润"
};
static const int COUNT = 4;
} // namespace button

// ---- 滑动条（wctl::drawSlider / drawSliderValue；g_uiCtx.sliderStyle）----
namespace slider {
enum : int { FOLLOW = 0, DEFAULT = 1, NEON_THIN = 2, NEON_VBAR = 3, SPRING = 4, PIXEL = 5,
             ROUND = 6, VALUE = 7,
             // 4 个「中间带值」模板（游戏ID 8..11）：值文字整框居中。
             // 由来：原 10 个精简为 4 个（用户"就保留 值-霓虹/值-大圆/值-分段/值-圆柄"）；
             // 删除属"可重排"情形 → 存活项按原相对顺序压实为 8..11。
             VB_RHANDLE = 8, VB_SEG = 9, VB_NEON = 10, VB_BIGCIRCLE = 11 };
static const char* const NAMES[] = {
    u8"系统跟随", u8"系统默认", u8"霓虹细轨", u8"竖柄霓虹", u8"春意描边", u8"像素霓虹",
    u8"圆润", u8"值文字条",
    u8"值-圆柄", u8"值-分段", u8"值-霓虹", u8"值-大圆"
};
static const int COUNT = 12;
} // namespace slider

// ---- 环形进度（pctl::drawCircularProgress；g_uiCtx.ringStyle）----
// 「圆形的单独模板」：环形进度作为独立控件模板（对照原 drawCircularProgress L573-613）
namespace ring {
enum : int { FOLLOW = 0, DEFAULT = 1, LOADING = 2 };
static const char* const NAMES[] = {
    u8"系统跟随", u8"系统默认（进度环）", u8"加载环"
};
static const int COUNT = 3;
} // namespace ring

// ---- 开关（wctl::drawToggleOn / pctl::drawToggle；g_uiCtx.switchStyle）----
namespace sw {
enum : int { FOLLOW = 0, DEFAULT = 1, SQUARE = 2, SEMI = 3,
             // 2 个「特别绘制」（游戏ID 4..5）：开关本体新绘制，主题色单源、on/off 双态。
             // 由来：原 10 个精简为 2 个（用户"保留：机械 渐变"）；
             // 删除属"可重排"情形 → 存活项按原相对顺序压实为 4..5。
             MACHINE = 4, GRADIENT = 5 };
static const char* const NAMES[] = {
    u8"系统跟随", u8"系统默认", u8"方形", u8"半圆润",
    u8"机械", u8"渐变"
};
static const int COUNT = 6;
} // namespace sw

// ---- 开关扩展图案（wctl::drawSwitchExtra；g_uiCtx.switchExtra）----
namespace swExtra {
enum : int {
    FOLLOW = 0, DEFAULT = 1, GLOW = 2, FLOWER = 3, SNOW = 4, GEAR = 5,
    STARMOON = 6, GEM = 7, BAT = 8, STARLIGHT = 9, SUN = 10,
    METAL = 11, PAW = 12, MIRROR = 13,
};
static const char* const NAMES[] = {
    u8"系统跟随", u8"系统默认（无）", u8"中心微光", u8"花朵", u8"雪花", u8"齿轮", u8"星月",
    u8"宝石变色", u8"蝙蝠标记", u8"星辉", u8"太阳光芒", u8"金属拨杆", u8"猫爪印", u8"镜面光泽"
};
static const int COUNT = 14;
} // namespace swExtra

// ---- 分割线（SplitterSystem；g_uiCtx.splitterStyle）----
namespace splitter {
enum : int { FOLLOW = 0, DEFAULT = 1, THIN = 2, DOUBLE = 3, DOTS = 4, GRAD = 5 };
static const char* const NAMES[] = {
    u8"系统跟随", u8"系统默认", u8"细线", u8"双线", u8"点状", u8"渐变"
};
static const int COUNT = 6;
} // namespace splitter

// ---- 悬浮球（LayerStack；g_uiCtx.ballStyle）----
namespace ball {
enum : int { FOLLOW = 0, DEFAULT = 1, SQUARE = 2,
             // 2 个「特别绘制」（游戏ID 3..4）：悬浮球本体新绘制。
             // 由来：10 个精简保留 玻璃/渐变；再精简去掉 4 个几何（爱心/星形/六边形/三角）。
             // 删除属"可重排" → 存活项按原相对顺序压实为 3..4。
             GLASS = 3, GRADIENT = 4 };
static const char* const NAMES[] = {
    u8"系统跟随", u8"系统默认（圆形）", u8"方形",
    u8"玻璃", u8"渐变"
};
static const int COUNT = 5;
} // namespace ball

// ---- 沙盘（SandboxSystem；形态而非绘制风格）----
// 用户要求「有线和无限都单独 ID」：有限沙盘 / 无限沙盘 各占一个游戏ID，
// 报「沙盘 1 号」= 有限、「沙盘 2 号」= 无限，AI 依本表秒懂。
// 沙盘无颜色/圆角变体（绘制系统全包），故只有两种形态：ID 1/2，0=系统跟随。
namespace sandbox {
enum : int { FOLLOW = 0, FINITE = 1, INFINITE = 2 };
static const char* const NAMES[] = {
    u8"系统跟随", u8"有限沙盘", u8"无限沙盘"
};
static const int COUNT = 3;
} // namespace sandbox

// ---- 滚动条（RegionSystem；g_uiCtx.scrollbarStyle）----
// 滚动条几何/形态全项目唯一单源：RegionSystem 只读，页面/窗口一律不自算。
// 自动适配：轨道预留宽度随当前模板实时变化，系统按此给内容让位（contentW）。
namespace scrollbar {
// 游戏ID
enum : int { FOLLOW = 0, DEFAULT = 1, CAPSULE_BTN = 2, CAPSULE = 3 };
// 内部 0 基渲染值
enum : int { I_DEFAULT = 0, I_CAPSULE_BTN = 1, I_CAPSULE = 2 };
static const char* const NAMES[] = {
    u8"系统跟随", u8"系统默认（细轨）", u8"胶囊·带上下按钮", u8"胶囊·无按钮"
};
static const int COUNT = 4;

// 单源几何（逻辑像素；使用处乘 g_uiCtx.scale）
//   trackW ：轨道宽度（粗细）
//   btnH   ：两端箭头按钮高（0 = 无按钮）
//   capsule：胶囊形（圆角 = trackW*0.5）；false = 细圆角
//   pad    ：轨道距区域边缘的内边距
struct Geom { float trackW; float btnH; bool capsule; float pad; };
inline Geom geomOf(int internal) {
    switch (internal) {
        // 胶囊粗细 12→9.6（按用户要求 -20%）
        case I_CAPSULE_BTN: return { 9.6f, 14.0f, true,  2.0f };
        case I_CAPSULE:     return { 9.6f,  0.0f, true,  2.0f };
        default:            return {  4.0f,  0.0f, false, 2.0f };  // 系统默认细轨=原样
    }
}
} // namespace scrollbar

// ============================================================
// 「其余控件」统一纳管（用户要求：其他控件全部统一进同一套「名字+游戏ID」）
//   页签 / 侧栏项 / 分段按钮（视图模式）/ 计数器（步进器）/ 输入框 / 复选框 / 条形进度
//   每个控件一个族：NAMES[] 下标即游戏ID（0=系统跟随 1=系统默认 2..=变体）。
//   目前均为单一样式（0/1）；变体可后续「中间插入」——只加常量、不改旧号。
// ============================================================
namespace tab {          // 顶部页签项（pctl::drawTabItem）
enum : int { FOLLOW = 0, DEFAULT = 1 };
static const char* const NAMES[] = { u8"系统跟随", u8"系统默认" };
static const int COUNT = 2;
} // namespace tab

namespace side {         // 侧栏项（pctl::drawSideItem）
enum : int { FOLLOW = 0, DEFAULT = 1 };
static const char* const NAMES[] = { u8"系统跟随", u8"系统默认" };
static const int COUNT = 2;
} // namespace side

namespace seg {          // 分段按钮 / 视图模式切换（pctl::drawSegItem）
enum : int { FOLLOW = 0, DEFAULT = 1 };
static const char* const NAMES[] = { u8"系统跟随", u8"系统默认" };
static const int COUNT = 2;
} // namespace seg

namespace stepper {      // 计数器 / 步进器（pctl::drawStepper）
enum : int { FOLLOW = 0, DEFAULT = 1 };
static const char* const NAMES[] = { u8"系统跟随", u8"系统默认" };
static const int COUNT = 2;
} // namespace stepper

namespace input {        // 输入框（wctl::drawInput / pctl::drawInputBox）
enum : int { FOLLOW = 0, DEFAULT = 1 };
static const char* const NAMES[] = { u8"系统跟随", u8"系统默认" };
static const int COUNT = 2;
} // namespace input

namespace check {        // 复选框（wctl::drawCheck）
enum : int { FOLLOW = 0, DEFAULT = 1, REF_TICK = 2 };   // 2 = 参考帧勾（精确三段折线勾）
static const char* const NAMES[] = { u8"系统跟随", u8"系统默认", u8"参考帧勾" };
static const int COUNT = 3;
} // namespace check

namespace dropdown {     // 下拉按钮（DropdownSystem / wctl::drawComboBtnStyled）
enum : int { FOLLOW = 0, DEFAULT = 1,
             // 2 个「特别绘制」（游戏ID 2..3）：下拉按钮本体新绘制。
             // 删除「参考帧双块」及其余变体后，存活项按原相对顺序压实。
             ROUND = 2, OUTLINE = 3 };
static const char* const NAMES[] = { u8"系统跟随", u8"系统默认", u8"圆角胶囊", u8"空心描边" };
static const int COUNT = 4;
} // namespace dropdown

namespace progress {     // 条形进度（wctl::drawProgress / pctl::drawProgressBar）
enum : int { FOLLOW = 0, DEFAULT = 1,
             // 3 个「特别绘制」（游戏ID 2..4）：条形进度本体新绘制。
             // 由来：10 个精简保留 分段/霓虹/斜纹；删除属"可重排" → 存活项按原相对顺序压实为 2..4。
             SEGMENT = 2, NEON = 3, STRIPE = 4 };
static const char* const NAMES[] = {
    u8"系统跟随", u8"系统默认",
    u8"分段", u8"霓虹", u8"斜纹"
};
static const int COUNT = 5;
} // namespace progress

// ============================================================
// 全控件名录（「叫得出名字」唯一单源）：控件名 ↔ 游戏ID 族
//   每项 = 控件名（徽标「系统<名>#N」用）+ 风格名数组（下标即游戏ID）+ 风格数 + 样例种类。
//   kind：UI模板页「其余控件」区按此绘制该控件样例（K_NONE = 样例已在别处呈现）。
//   全项目控件在此一览：报名字/编号即定位控件与其风格。
// ============================================================
enum Kind : int { K_NONE = 0, K_TAB, K_SIDE, K_SEG, K_STEPPER, K_INPUT, K_CHECK, K_PROGRESS, K_DROPDOWN };

struct ControlEntry {
    const char*        name;    // 控件名（叫得出名字）
    const char* const* styles;  // 游戏ID → 风格名（下标即游戏ID；0=系统跟随 1=系统默认 2..=变体）
    int                count;   // 风格数（含 0=系统跟随）
    int                kind;    // 样例绘制种类
};

// 返回全控件名录（静态表；n 回填条目数）
inline const ControlEntry* controlRoster(int& n) {
    static const ControlEntry t[] = {
        { u8"按钮",     button::NAMES,    button::COUNT,    K_NONE },
        { u8"滑动条",   slider::NAMES,    slider::COUNT,    K_NONE },
        { u8"开关",     sw::NAMES,        sw::COUNT,        K_NONE },
        { u8"开关图案", swExtra::NAMES,   swExtra::COUNT,   K_NONE },
        { u8"分割线",   splitter::NAMES,  splitter::COUNT,  K_NONE },
        { u8"沙盘",     sandbox::NAMES,   sandbox::COUNT,   K_NONE },
        { u8"悬浮球",   ball::NAMES,      ball::COUNT,      K_NONE },
        { u8"滚动条",   scrollbar::NAMES, scrollbar::COUNT, K_NONE },
        { u8"环形进度", ring::NAMES,      ring::COUNT,      K_NONE },
        { u8"页签",     tab::NAMES,       tab::COUNT,       K_TAB },
        { u8"侧栏项",   side::NAMES,      side::COUNT,      K_SIDE },
        { u8"分段按钮", seg::NAMES,       seg::COUNT,       K_SEG },
        { u8"计数器",   stepper::NAMES,   stepper::COUNT,   K_STEPPER },
        { u8"输入框",   input::NAMES,     input::COUNT,     K_INPUT },
        { u8"复选框",   check::NAMES,     check::COUNT,     K_CHECK },
        { u8"条形进度", progress::NAMES,  progress::COUNT,  K_PROGRESS },
        { u8"下拉按钮", dropdown::NAMES,  dropdown::COUNT,  K_DROPDOWN },
    };
    n = (int)(sizeof(t) / sizeof(t[0]));
    return t;
}

} // namespace tpl
} // namespace ui