// AI生成
// ============================================================
// 键盘类悬浮窗（功能性同调色板：独立窗口 + 显示区 + 按键输入）
//   1) 数字键盘   keypad_numeric：0-9 / . / -  + 退格
//   2) 身份证输入器 keypad_idcard ：0-9 / X  + 退格（上限 18 位）
//
// 布局单源：layout() 一次计算，render/hover/click 共用；
// 按键表配置驱动（KEYS[]），零硬编码；按键绘制统一走 wctl::drawBtn /
// drawBtnSecondary（跟随全局按钮模板与圆角，不自行绘制底与边框）。
//
// 目标框绑定（bindTarget）：页面输入框点击时注入 EditText*，
//   显示区改读该框缓冲、按键直接写回同一缓冲；
//   每次按键后重建焦点（帧首 blurAll 会清焦点），保证物理键盘持续可用。
//   未绑定（独立打开）时退回本窗自有缓冲，行为与旧版一致。
// ============================================================
#include "WinHandlers.h"
#include "UiContext.h"
#include "WindowButtons.h"
#include "WinControls.h"
#include "TextInputSystem.h"   // EditText（绑定目标类型）
#include "WindowManager.h"     // g_winMgr.textInput()：重建焦点
#include "../Element.h"
#include "../Theme.h"
#include <string>
#include <cstdio>

namespace ui {
namespace keypad {

// ---- 按键表（配置表单源）----
static const char* const NUM_KEYS[] = {
    "7", "8", "9",
    "4", "5", "6",
    "1", "2", "3",
    ".", "0", "-",
};
static const char* const ID_KEYS[] = {
    "7", "8", "9",
    "4", "5", "6",
    "1", "2", "3",
    "X", "0", u8"退格",
};
static const int KEY_COUNT = 12;
static const int EXTRA_COUNT = 1;                  // 数字键盘底部：退格（通栏）

// 输入上限（字节；均为 ASCII）
static const size_t NUM_MAX = 24;
static const size_t ID_MAX = 18;

struct State {
    std::string text;
    int hoverKey = -1;      // 主键盘 hover（0..11）
    int hoverExtra = -1;    // 底部键 hover（0=退格）
};

// ---- 目标输入框绑定（0 = 数字 / 1 = 身份证；-1 = 未绑定）----
static EditText* g_target[2] = { nullptr, nullptr };
static int       g_targetPage[2] = { 0, 0 };

void bindTarget(bool idMode, EditText* t, int ownerPageId) {
    int i = idMode ? 1 : 0;
    g_target[i] = t;
    g_targetPage[i] = ownerPageId;
}
static EditText* targetOf(bool idMode) { return g_target[idMode ? 1 : 0]; }

// 编辑串单源：已绑定 → 目标框缓冲；未绑定 → 本窗自有缓冲
static std::string& textOf(State& st, bool idMode) {
    EditText* t = targetOf(idMode);
    return t ? t->buf : st.text;
}

// 按键落盘后：光标归位行尾 + 重建焦点（保持物理键盘可用；重复聚焦无副作用）
static void afterEdit(bool idMode) {
    EditText* t = targetOf(idMode);
    if (!t) return;
    int n = (int)t->buf.size();
    if (t->caret < 0 || t->caret > n) t->caret = n;
    g_winMgr.textInput().requestFocus(TEXT_OWNER_PAGE,
                                      g_targetPage[idMode ? 1 : 0], *t);
}

struct Layout {
    Rect display;           // 显示区
    Rect keys[KEY_COUNT];   // 主键盘
    Rect extra[EXTRA_COUNT];// 底部键（仅数字键盘：退格）
    bool hasExtra = false;
};

static Layout layout(const WindowInstance& win, bool idMode) {
    Layout L;
    float s = g_uiCtx.scale;
    float pad = 12.0f * s;
    float titleH = win.hasTitleBar() ? wbtn::titleBarHeight(win) : 0.0f;
    float cx = win.x + pad;
    float cw = win.w - pad * 2;
    float cy = win.y + titleH + pad;

    // 显示区
    float dispH = 38.0f * s;
    L.display = Rect(Vec2(cx, cy), Vec2(cw, dispH));
    cy += dispH + 10.0f * s;

    // 底部退格行（数字键盘有；身份证的退格在主键盘末格）
    L.hasExtra = !idMode;
    float extraH = 30.0f * s;
    float extraGap = 8.0f * s;
    float gridBottom = win.y + win.h - pad - (L.hasExtra ? extraH + extraGap : 0.0f);

    // 主键盘 3 列 4 行（行高自适应可用高）
    float gap = 8.0f * s;
    float keyW = (cw - gap * 2) / 3.0f;
    float keyH = (gridBottom - cy - gap * 3) / 4.0f;
    if (keyH < 12.0f * s) keyH = 12.0f * s;
    for (int i = 0; i < KEY_COUNT; i++) {
        int col = i % 3, row = i / 3;
        L.keys[i] = Rect(Vec2(cx + col * (keyW + gap), cy + row * (keyH + gap)),
                         Vec2(keyW, keyH));
    }

    // 底部退格（通栏）
    if (L.hasExtra) {
        L.extra[0] = Rect(Vec2(cx, gridBottom + extraGap), Vec2(cw, extraH));
    }
    return L;
}

// ---- 输入动作（追加 / 退格；单源）----
static void backspace(std::string& s) {
    if (!s.empty()) s.pop_back();                  // 全 ASCII，逐字节即逐字符
}

// 数字键盘输入规则（与全项目数值输入框同口径）：
//   '-' 仅允许出现在最前（前面已有字符则忽略）；
//   '.' 全局至多一个，且其前面若无数字（开头或紧跟 '-'）→ 自动补 0；
static void appendNumeric(std::string& s, const char* ch) {
    if (!ch || !ch[0]) return;
    if (s.size() >= NUM_MAX) return;
    char k = ch[0];
    if (k == '-') {
        if (!s.empty()) return;                                // 负号只能开头
        s = "-";
    } else if (k == '.') {
        if (s.find('.') != std::string::npos) return;          // . 只能一个
        if (s.empty() || s == "-") s += "0";                   // 无数字自动补 0
        s += '.';
    } else {
        s += k;
    }
}

// 身份证输入（0-9 / X；上限 18 位）
static void appendId(std::string& s, const char* ch) {
    if (!ch || !ch[0]) return;
    if (s.size() >= ID_MAX) return;
    s += (ch[0] == 'x' ? "X" : ch);
}

static void drawKeypad(RenderDevice* dev, WindowInstance& win, float alpha,
                       bool idMode) {
    ThemeColors& c = currentColors();
    State& st = win.getState<State>();
    Layout L = layout(win, idMode);
    float s = g_uiCtx.scale;
    float a = alpha;
    float rad = 4.0f * s * g_uiCtx.radiusScale();
    const std::string& txt = textOf(st, idMode);   // 绑定框缓冲 / 本窗缓冲

    // 显示区（底 + 边框 + 值/占位）
    drawRoundedRect(dev, L.display,
                    Color(c.inputBg.r, c.inputBg.g, c.inputBg.b, 230.0f / 255.0f * a),
                    rad);
    drawRoundedBorder(dev, L.display,
                      Color(c.inputBorder.r, c.inputBorder.g, c.inputBorder.b,
                            170.0f / 255.0f * a),
                      1.0f * s, rad);
    if (txt.empty()) {
        wctl::drawTextCenterAt(dev, g_uiCtx.smallFont,
                               idMode ? u8"输入身份证号（0-9、X）"
                                      : u8"输入数字（0-9 . -）",
                               L.display,
                               Color(c.text.r, c.text.g, c.text.b, 120.0f / 255.0f * a));
    } else {
        wctl::drawTextCenterAt(dev, g_uiCtx.font, txt.c_str(), L.display,
                               Color(c.inputText.r, c.inputText.g, c.inputText.b, a));
    }

    // 主键盘（统一 drawBtn：跟随全局按钮模板与圆角）
    const char* const* keys = idMode ? ID_KEYS : NUM_KEYS;
    for (int i = 0; i < KEY_COUNT; i++)
        wctl::drawBtn(dev, L.keys[i], keys[i], st.hoverKey == i, a);

    // 底部退格（次按钮样式）
    if (L.hasExtra)
        wctl::drawBtnSecondary(dev, L.extra[0], u8"退格", st.hoverExtra == 0, a);
}

static bool clickKeypad(WindowInstance& win, const Vec2& m, bool idMode) {
    State& st = win.getState<State>();
    Layout L = layout(win, idMode);
    const char* const* keys = idMode ? ID_KEYS : NUM_KEYS;
    std::string& s = textOf(st, idMode);           // 绑定框缓冲 / 本窗缓冲

    for (int i = 0; i < KEY_COUNT; i++) {
        if (!L.keys[i].contains(m)) continue;
        const char* k = keys[i];
        if (k && std::string(k) == std::string(u8"退格")) {   // 身份证主键盘末格
            backspace(s);
            afterEdit(idMode);
            return true;
        }
        if (idMode) appendId(s, k);
        else        appendNumeric(s, k);
        afterEdit(idMode);
        return true;
    }
    if (L.hasExtra && L.extra[0].contains(m)) {               // 数字键盘底部退格
        backspace(s);
        afterEdit(idMode);
        return true;
    }
    return false;
}

static void hoverKeypad(WindowInstance& win, const Vec2& m, bool idMode) {
    State& st = win.getState<State>();
    Layout L = layout(win, idMode);
    st.hoverKey = -1;
    for (int i = 0; i < KEY_COUNT; i++)
        if (L.keys[i].contains(m)) { st.hoverKey = i; break; }
    st.hoverExtra = -1;
    if (L.hasExtra && L.extra[0].contains(m)) st.hoverExtra = 0;
}

} // namespace keypad

// ============================================================
// 数字键盘（handler_keypad_numeric）
// ============================================================
void renderKeypadNumeric(RenderDevice* dev, WindowInstance& win, float alpha) {
    keypad::drawKeypad(dev, win, alpha, false);
}
bool clickKeypadNumeric(WindowInstance& win, const Vec2& m) {
    return keypad::clickKeypad(win, m, false);
}
void hoverKeypadNumeric(WindowInstance& win, const Vec2& m) {
    keypad::hoverKeypad(win, m, false);
}

// ============================================================
// 身份证输入器（handler_keypad_idcard）
// ============================================================
void renderKeypadIdcard(RenderDevice* dev, WindowInstance& win, float alpha) {
    keypad::drawKeypad(dev, win, alpha, true);
}
bool clickKeypadIdcard(WindowInstance& win, const Vec2& m) {
    return keypad::clickKeypad(win, m, true);
}
void hoverKeypadIdcard(WindowInstance& win, const Vec2& m) {
    keypad::hoverKeypad(win, m, true);
}

// ============================================================
// 目标框绑定（页面输入框点击时调用）：idMode = false 数字 / true 身份证
// ============================================================
void bindKeypadTarget(bool idMode, EditText* t, int ownerPageId) {
    keypad::bindTarget(idMode, t, ownerPageId);
}

} // namespace ui