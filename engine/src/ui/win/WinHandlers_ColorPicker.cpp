// AI生成
// ============================================================
// 颜色选择器处理器（HSV 拾色 + Alpha 滑条 + 精确输入 + 预设色）
// 布局单源化：layout() 一次计算，render/hover/click 共用。
// 拖拽交互（SV 区/Hue 条/Alpha 滑条）由 WindowManager 转发 drag/release。
//
// 精确输入：R/G/B/Alpha/Hex 值框统一走 TextInputSystem（全引擎输入框
// 唯一认知）：click 命中值框 requestFocus（预填当前值），Enter/点击他处
// 提交（onCommit 解析写 HSV/Alpha/写回目标），Esc 放弃恢复原值，
// 编辑中快捷键系统自动豁免。Hex 支持 #RGB / #RRGGBB / #AARRGGBB
// （# 可省略）；Alpha 滑条与数值输入并存。
//
// 写回目标三种绑定模式（主题工作室注入，帧末 Poll 消费）：
//   0 = 独立模式（不写回）
//   1 = 主题色项（55 色项表：g_themes[theme].colors.*member）
//   2 = 自定义背景槽位（g_bgCustom：纯色/渐变四角/网格底/网格线）
// ============================================================
#include "WinHandlers.h"
#include "UiContext.h"
#include "RegionSystem.h"
#include "LayerStack.h"
#include "WindowButtons.h"
#include "WinControls.h"
#include "TextInputSystem.h"
#include "WindowManager.h"   // g_winMgr.textInput()：统一文本输入系统访问
#include "../Element.h"
#include "../Theme.h"
#include "../Background.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

namespace ui {

namespace picker {

// ====== 模块私有状态（每窗口实例独立） ======
struct State {
    float h = 210.0f, s = 0.7f, v = 0.8f;  // HSV
    int alpha = 255;
    int drag = 0;               // 拖拽目标：0=无 1=SV区 2=Hue条 3=Alpha
    bool hoverSV = false, hoverHue = false, hoverAlpha = false;
    int hoverPreset = -1;

    // ---- 当前色 | 之前色 双色块 ----
    // 绑定模式（主题工作室注入，bindMode!=0）：绑定时快照目标原色 → hasPrev=true，
    //   「之前色」显示该快照，按下还原；独立模式（bindMode==0，用户调用打开）：
    //   hasPrev 不参与 → 「之前色」实时等同当前色（跟随，显示一样）。
    int  prevR = 0, prevG = 0, prevB = 0, prevA = 255;
    bool hasPrev = false;
    bool hoverPrev = false;

    // ---- 明亮 / 饱和度 步进按钮 hover（0=明亮- 1=明亮+ 2=饱和- 3=饱和+） ----
    bool hoverStep[4] = {false, false, false, false};

    // ---- 写回目标（绑定模式见文件头注释；bindMode=0 时全部写回跳过） ----
    int bindMode = 0;                       // 0=独立 1=主题色项 2=背景槽位
    int targetTheme = -1;                   // bindMode=1：目标主题索引
    int targetItemIndex = -1;               // bindMode=1：55 色项表索引（调试展示用）
    Color ThemeColors::*targetField = nullptr;  // bindMode=1：目标颜色成员指针
    int targetBgSlot = -1;                  // bindMode=2：背景槽位 0-6

    // ---- 精确输入（R/G/B/Alpha/Hex 值框；交互全归 TextInputSystem） ----
    EditText edits[5];   // 0=R 1=G 2=B 3=Alpha 4=Hex（beginEdit 配置）
};

// ====== 布局（单源计算） ======
struct Layout {
    float pad = 0, cx = 0, cw = 0, top = 0;
    Rect sv;            // SV 拾色区
    Rect hue;           // Hue 竖条
    Rect preview;       // 当前色预览块
    Rect prevSwatch;    // 之前色色块
    Rect curLbl;        // "当前色"标签
    Rect prevLbl;       // "之前色"标签
    Rect brightLbl;     // "明亮"标签
    Rect brightMinus;   // 明亮 - 按钮
    Rect brightPlus;    // 明亮 + 按钮
    Rect satLbl;        // "饱和度"标签
    Rect satMinus;      // 饱和度 - 按钮
    Rect satPlus;       // 饱和度 + 按钮
    Rect rgbLbl[3];     // R/G/B 标签
    Rect rgbVal[3];     // R/G/B 数值框
    Rect alphaLbl;      // 透明度标签
    Rect alphaSlider;   // Alpha 滑条槽
    Rect alphaVal;      // Alpha 数值框
    Rect hexLbl;        // Hex 标签
    Rect hexVal;        // Hex 值框
    Rect presets[25];   // 预设色块（5列x2行 上限）
    int presetCols = 0, presetCount = 0;
};
static Layout layout(const WindowInstance& win) {
    Layout L;
    float s = g_uiCtx.scale;
    float titleH = win.hasTitleBar() ? wbtn::titleBarHeight(win) : 0.0f;
    L.pad = 14.0f * s;
    L.cx = win.x + L.pad;
    L.cw = win.w - L.pad * 2;
    float cy = win.y + titleH + L.pad;

    // SV 区 + Hue 条
    float hueW = 22.0f * s, gap = 8.0f * s, svH = 150.0f * s;
    L.sv = Rect(Vec2(L.cx, cy), Vec2(L.cw - hueW - gap, svH));
    L.hue = Rect(Vec2(L.cx + L.cw - hueW, cy), Vec2(hueW, svH));
    cy += svH + L.pad;

    // 当前色 | 之前色 双色块 + RGB 行
    float rowH = 26.0f * s;
    float sw = 34.0f * s, swGap = 6.0f * s, lblH = 12.0f * s;
    L.preview = Rect(Vec2(L.cx, cy), Vec2(sw, rowH));                         // 当前色
    L.prevSwatch = Rect(Vec2(L.cx + sw + swGap, cy), Vec2(sw, rowH));         // 之前色
    L.curLbl = Rect(Vec2(L.cx, cy + rowH + 1.0f * s), Vec2(sw, lblH));
    L.prevLbl = Rect(Vec2(L.cx + sw + swGap, cy + rowH + 1.0f * s), Vec2(sw, lblH));
    float lblW = 14.0f * s, valW = 46.0f * s, rgap = 8.0f * s;
    float rx = L.cx + sw + swGap + sw + 12.0f * s;
    for (int i = 0; i < 3; i++) {
        L.rgbLbl[i] = Rect(Vec2(rx, cy), Vec2(lblW, rowH));
        L.rgbVal[i] = Rect(Vec2(rx + lblW + 4.0f * s, cy), Vec2(valW, rowH));
        rx += lblW + 4.0f * s + valW + rgap;
    }
    cy += rowH + lblH + L.pad;      // 让出"当前色/之前色"标签高度

    // Alpha 行
    float aRowH = 22.0f * s;
    L.alphaLbl = Rect(Vec2(L.cx, cy), Vec2(42.0f * s, aRowH));
    L.alphaSlider = Rect(Vec2(L.cx + 46.0f * s, cy + aRowH * 0.35f),
                         Vec2(L.cw - 46.0f * s - 50.0f * s - 8.0f * s, 6.0f * s));
    L.alphaVal = Rect(Vec2(L.cx + L.cw - 50.0f * s, cy), Vec2(50.0f * s, aRowH));
    cy += aRowH + L.pad;

    // Hex 行
    float hRowH = 26.0f * s;
    L.hexLbl = Rect(Vec2(L.cx, cy), Vec2(42.0f * s, hRowH));
    L.hexVal = Rect(Vec2(L.cx + 46.0f * s, cy), Vec2(L.cw - 46.0f * s, hRowH));
    cy += hRowH + L.pad;

    // 明亮 / 饱和度 步进行（左组：明亮 [-] [+]；右组：饱和度 [-] [+]）
    float stpRowH = 22.0f * s, stpBtnW = 22.0f * s, stpGap = 4.0f * s, stpLblW = 36.0f * s;
    L.brightLbl = Rect(Vec2(L.cx, cy), Vec2(stpLblW, stpRowH));
    L.brightMinus = Rect(Vec2(L.cx + stpLblW + stpGap, cy), Vec2(stpBtnW, stpRowH));
    L.brightPlus = Rect(Vec2(L.cx + stpLblW + stpGap + stpBtnW + stpGap, cy), Vec2(stpBtnW, stpRowH));
    float rx2 = L.cx + L.cw * 0.5f;
    L.satLbl = Rect(Vec2(rx2, cy), Vec2(stpLblW, stpRowH));
    L.satMinus = Rect(Vec2(rx2 + stpLblW + stpGap, cy), Vec2(stpBtnW, stpRowH));
    L.satPlus = Rect(Vec2(rx2 + stpLblW + stpGap + stpBtnW + stpGap, cy), Vec2(stpBtnW, stpRowH));
    cy += stpRowH + L.pad;

    // 预设色网格
    float ps = 20.0f * s, pg = 4.0f * s;
    L.presetCols = (int)(L.cw / (ps + pg));
    if (L.presetCols < 1) L.presetCols = 1;
    L.presetCount = 25;  // 预设色数量（与数组一致）
    for (int i = 0; i < L.presetCount; i++) {
        int col = i % L.presetCols, row = i / L.presetCols;
        L.presets[i] = Rect(Vec2(L.cx + col * (ps + pg), cy + row * (ps + pg)), Vec2(ps, ps));
    }
    return L;
}

// ====== 预设色表（经典 25 色） ======
static const uint32_t kPresets[] = {
    0xFF0000, 0xFF4500, 0xFF8C00, 0xFFD700, 0xFFFF00,
    0x9ACD32, 0x00FF00, 0x00FA9A, 0x00CED1, 0x1E90FF,
    0x0000FF, 0x4B0082, 0x8A2BE2, 0xFF1493, 0xFF69B4,
    0x000000, 0x404040, 0x808080, 0xC0C0C0, 0xFFFFFF,
    0xF5F5DC, 0xDAA520, 0xCD853F, 0x8B4513, 0xA0522D,
};

// ====== 通用：HSV 状态 → RGB int ======
static void currentRGB(const State& st, int& r, int& g, int& b) {
    HSVtoRGB(st.h, st.s, st.v, r, g, b);
}

// ====== 取值写回目标（按绑定模式分发；独立模式直接返回） ======
static void writeBack(State& st) {
    int r, g, b;
    currentRGB(st, r, g, b);
    if (st.bindMode == 1) {                 // 主题色项
        if (st.targetTheme < 0 || st.targetTheme >= THEME_COUNT || !st.targetField) return;
        Color& dst = g_themes[st.targetTheme].colors.*st.targetField;
        dst.r = r / 255.0f;
        dst.g = g / 255.0f;
        dst.b = b / 255.0f;
        dst.a = st.alpha / 255.0f;          // 透明度一并写回
    } else if (st.bindMode == 2) {          // 自定义背景槽位
        Color* dst = BgSlotColor(st.targetBgSlot);
        if (!dst) return;
        dst->r = r / 255.0f;
        dst->g = g / 255.0f;
        dst->b = b / 255.0f;
        dst->a = st.alpha / 255.0f;         // 透明度一并写回
    }
}

// ====== 从目标色同步到 HSV（绑定时一次性；按模式取源色） ======
static void syncFromTarget(State& st) {
    const Color* src = nullptr;
    if (st.bindMode == 1) {
        if (st.targetTheme < 0 || st.targetTheme >= THEME_COUNT || !st.targetField) return;
        src = &(g_themes[st.targetTheme].colors.*st.targetField);
    } else if (st.bindMode == 2) {
        src = BgSlotColor(st.targetBgSlot);
    }
    if (!src) return;
    // 透明度一并同步（此前只同步 RGB，打开绑定色时 alpha 未带进来）
    st.alpha = (int)(clampf(src->a * 255.0f + 0.5f, 0.0f, 255.0f));
    float r = src->r, g = src->g, b = src->b;
    float mx = r > g ? r : g; if (b > mx) mx = b;
    float mn = r < g ? r : g; if (b < mn) mn = b;
    float d = mx - mn;
    st.v = mx;
    st.s = mx > 0 ? d / mx : 0;
    if (d <= 0) { st.h = 0; return; }
    if (mx == r)      st.h = 60.0f * (g - b) / d + (g < b ? 360.0f : 0);
    else if (mx == g) st.h = 60.0f * (b - r) / d + 120.0f;
    else              st.h = 60.0f * (r - g) / d + 240.0f;
}

// ====== RGB → HSV 写入状态（预设色点击/输入框提交共用唯一实现） ======
static void setRGB(State& st, int r, int g, int b) {
    float rf = clampf(r / 255.0f, 0.0f, 1.0f);
    float gf = clampf(g / 255.0f, 0.0f, 1.0f);
    float bf = clampf(b / 255.0f, 0.0f, 1.0f);
    float mx = rf > gf ? rf : gf; if (bf > mx) mx = bf;
    float mn = rf < gf ? rf : gf; if (bf < mn) mn = bf;
    float d = mx - mn;
    st.v = mx;
    st.s = mx > 0 ? d / mx : 0;
    if (d <= 0) st.h = 0;
    else if (mx == rf) st.h = 60.0f * (gf - bf) / d + (gf < bf ? 360.0f : 0);
    else if (mx == gf) st.h = 60.0f * (bf - rf) / d + 120.0f;
    else st.h = 60.0f * (rf - gf) / d + 240.0f;
    writeBack(st);
}

// ====== Hex 解析：#RGB / #RRGGBB / #AARRGGBB（# 可省略） ======
static int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
// 成功返回 true 并写 rgb；a=-1 表示串中无 alpha（8 位时为前两位）
static bool parseHex(const char* s, int& r, int& g, int& b, int& a) {
    r = g = b = 0; a = -1;
    if (!s) return false;
    if (s[0] == '#') s++;
    int n = (int)strlen(s);
    if (n != 3 && n != 6 && n != 8) return false;
    for (int i = 0; i < n; i++)
        if (hexVal(s[i]) < 0) return false;
    if (n == 3) {
        r = hexVal(s[0]) * 17; g = hexVal(s[1]) * 17; b = hexVal(s[2]) * 17;
    } else if (n == 6) {
        r = hexVal(s[0]) * 16 + hexVal(s[1]);
        g = hexVal(s[2]) * 16 + hexVal(s[3]);
        b = hexVal(s[4]) * 16 + hexVal(s[5]);
    } else {
        a = hexVal(s[0]) * 16 + hexVal(s[1]);
        r = hexVal(s[2]) * 16 + hexVal(s[3]);
        g = hexVal(s[4]) * 16 + hexVal(s[5]);
        b = hexVal(s[6]) * 16 + hexVal(s[7]);
    }
    return true;
}

// ====== 精确输入：提交解析（EditText::onCommit 目标，field 0-4） ======
static void applyInput(State& st, int field, const std::string& v) {
    if (field <= 2) {                                   // R/G/B
        int n = (int)clampf((float)atoi(v.c_str()), 0.0f, 255.0f);
        int r, g, b; currentRGB(st, r, g, b);
        if (field == 0) r = n; else if (field == 1) g = n; else b = n;
        setRGB(st, r, g, b);
    } else if (field == 3) {                            // Alpha
        st.alpha = (int)clampf((float)atoi(v.c_str()), 0.0f, 255.0f);
    } else {                                            // Hex（非法串静默放弃）
        int r, g, b, a;
        if (parseHex(v.c_str(), r, g, b, a)) {
            setRGB(st, r, g, b);
            if (a >= 0) st.alpha = a;
        }
    }
}

// ====== 精确输入：进入编辑（配置 EditText + 申请焦点；预填当前值） ======
// State 经 getState 堆分配稳定（铁律 1.6），onCommit 捕其指针在窗口
// 存活期内有效；窗口销毁时系统 validateOwners 先静默清焦点，不触发回调。
// box = 该输入框屏幕矩形（鼠标点击定位光标用；nullptr = 不定位，光标落行尾）
static void beginEdit(WindowInstance& win, State& st, int field,
                      const Rect* box = nullptr, float mouseX = 0.0f) {
    EditText& e = st.edits[field];
    char buf[10];
    if (field <= 2) {
        int r, g, b; currentRGB(st, r, g, b);
        snprintf(buf, sizeof(buf), "%d", field == 0 ? r : field == 1 ? g : b);
    } else if (field == 3) {
        snprintf(buf, sizeof(buf), "%d", st.alpha);
    } else {
        int r, g, b; currentRGB(st, r, g, b);
        snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
    }
    e.filter = (field == 4) ? TEXT_HEX : TEXT_INT;
    e.maxBytes = (field == 4) ? 9 : 3;
    e.buf = buf;
    e.onCommit = [&st, field](const std::string& v) { applyInput(st, field, v); };
    g_winMgr.textInput().requestFocus(TEXT_OWNER_WINDOW, win.id, e);
    if (box) g_winMgr.textInput().placeCaret(e, *box, mouseX);   // 鼠标点击定位
}

// ====== SV 区绘制（纯色底 + 白→黑渐变覆盖） ======
static void drawSVArea(RenderDevice* dev, const Rect& r, float hue, float a) {
    int pr, pg, pb;
    HSVtoRGB(hue, 1.0f, 1.0f, pr, pg, pb);
    drawRect(dev, r, Color(pr / 255.f, pg / 255.f, pb / 255.f, a));
    int w = (int)r.size.x, h = (int)r.size.y;
    for (int col = 0; col < w; col++) {
        float sRatio = 1.0f - (float)col / w;   // 左满饱和 → 右白
        int alpha = (int)(255 * sRatio * a);
        drawRect(dev, Rect(r.pos.x + col, r.pos.y, 1, h), Color(1, 1, 1, alpha / 255.f));
    }
    for (int row = 0; row < h; row++) {
        float vRatio = (float)row / h;           // 上亮 → 下黑
        int alpha = (int)(255 * vRatio * a);
        drawRect(dev, Rect(r.pos.x, r.pos.y + row, w, 1), Color(0, 0, 0, alpha / 255.f));
    }
}

// ====== Hue 竖条绘制（彩虹渐变） ======
static void drawHueBar(RenderDevice* dev, const Rect& r, float a) {
    int h = (int)r.size.y;
    for (int row = 0; row < h; row++) {
        float hueRatio = 1.0f - (float)row / h;  // 顶红 → 底紫
        int hr, hg, hb;
        HSVtoRGB(hueRatio * 360.0f, 1.0f, 1.0f, hr, hg, hb);
        drawRect(dev, Rect(r.pos.x, r.pos.y + row, r.size.x, 1),
                 Color(hr / 255.f, hg / 255.f, hb / 255.f, a));
    }
}

// ====== 透明棋盘格（预览透明度背景） ======
static void drawCheckerboard(RenderDevice* dev, const Rect& r, float a, float radius) {
    // 对齐网格 + 整格内缩：格子边长固定、网格锚在 r.pos；圆角处按「整格」收缩，
    // 不再逐像素行扫描 → 左右格宽不再逐行变化，观感规整。
    float cs = 6.0f * g_uiCtx.scale;
    if (cs < 3.0f) cs = 3.0f;
    float R = radius;
    if (R > r.size.y * 0.5f) R = r.size.y * 0.5f;
    if (R > r.size.x * 0.5f) R = r.size.x * 0.5f;
    float top = r.pos.y, bot = r.pos.y + r.size.y;
    int rows = (int)std::ceil(r.size.y / cs);
    int cols = (int)std::ceil(r.size.x / cs);
    for (int j = 0; j < rows; ++j) {
        float y0 = top + j * cs;
        float y1 = y0 + cs; if (y1 > bot) y1 = bot;
        if (y1 - y0 <= 0.5f) continue;
        float inset = 0.0f;
        if (R > 0.0f) {                     // 取本行带内缩最大（最靠近边缘）的值，保证不越界
            float dy = -1.0f;
            if (y0 < top + R) dy = (top + R) - y0;
            else if (y1 > bot - R) dy = y1 - (bot - R);
            if (dy > R) dy = R;
            if (dy > 0.0f) inset = R - std::sqrt(R * R - dy * dy);
        }
        float xl = r.pos.x + inset;
        float xr = r.pos.x + r.size.x - inset;
        if (xr - xl <= 0.5f) continue;
        for (int i = 0; i < cols; ++i) {
            float x0 = r.pos.x + i * cs;
            float x1 = x0 + cs; if (x1 > r.pos.x + r.size.x) x1 = r.pos.x + r.size.x;
            float cx0 = x0 < xl ? xl : x0;
            float cx1 = x1 > xr ? xr : x1;
            if (cx1 - cx0 <= 0.5f) continue;
            bool light = ((i + j) & 1) == 0;
            Color cb = light ? Color(1.0f, 1.0f, 1.0f, a)
                             : Color(226.0f / 255.0f, 226.0f / 255.0f,
                                     226.0f / 255.0f, a);
            drawRect(dev, Rect(cx0, y0, cx1 - cx0, y1 - y0), cb);
        }
    }
}

} // namespace picker

// ============================================================
// 渲染
// ============================================================
void renderColorPicker(RenderDevice* dev, WindowInstance& win, float alpha) {
    ThemeColors& c = currentColors();
    picker::State& st = win.getState<picker::State>();
    picker::Layout L = picker::layout(win);
    float a = alpha;
    TextInputSystem& tis = g_winMgr.textInput();   // 焦点态/光标相位系统单源

    // SV 拾色区
    picker::drawSVArea(dev, L.sv, st.h, a);
    drawRoundedBorder(dev, L.sv, Color(c.inputBorder.r, c.inputBorder.g, c.inputBorder.b,
                                       180.0f / 255.0f * a), 1.2f * g_uiCtx.scale, 2.0f * g_uiCtx.scale);
    // SV 选择圈
    float selR = 7.0f * g_uiCtx.scale;
    float selX = L.sv.pos.x + st.s * L.sv.size.x;
    float selY = L.sv.pos.y + (1.0f - st.v) * L.sv.size.y;
    drawCircle(dev, selX, selY, selR, 2.0f * g_uiCtx.scale, Color(1, 1, 1, a));
    drawCircle(dev, selX, selY, selR, 1.0f * g_uiCtx.scale, Color(0, 0, 0, 180.0f / 255.0f * a));

    // Hue 条 + 指示三角
    picker::drawHueBar(dev, L.hue, a);
    drawRoundedBorder(dev, L.hue, Color(c.inputBorder.r, c.inputBorder.g, c.inputBorder.b,
                                        180.0f / 255.0f * a), 1.2f * g_uiCtx.scale, 2.0f * g_uiCtx.scale);
    float hueY = L.hue.pos.y + (1.0f - st.h / 360.0f) * L.hue.size.y;
    Vec2 triL[] = {Vec2(L.hue.pos.x - 5.0f * g_uiCtx.scale, hueY - 4.0f * g_uiCtx.scale),
                   Vec2(L.hue.pos.x - 5.0f * g_uiCtx.scale, hueY + 4.0f * g_uiCtx.scale),
                   Vec2(L.hue.pos.x, hueY)};
    fillPolygon(dev, triL, 3, Color(1, 1, 1, a));
    Vec2 triR[] = {Vec2(L.hue.pos.x + L.hue.size.x + 5.0f * g_uiCtx.scale, hueY - 4.0f * g_uiCtx.scale),
                   Vec2(L.hue.pos.x + L.hue.size.x + 5.0f * g_uiCtx.scale, hueY + 4.0f * g_uiCtx.scale),
                   Vec2(L.hue.pos.x + L.hue.size.x, hueY)};
    fillPolygon(dev, triR, 3, Color(1, 1, 1, a));

    // 当前色 | 之前色 双色块（棋盘格 + 颜色）
    int r, g, b;
    picker::currentRGB(st, r, g, b);
    // 当前色
    picker::drawCheckerboard(dev, L.preview, a, 3.0f * g_uiCtx.scale);
    drawRoundedRect(dev, L.preview, Color(r / 255.f, g / 255.f, b / 255.f,
                                          st.alpha / 255.0f * a), 3.0f * g_uiCtx.scale);
    drawRoundedBorder(dev, L.preview, Color(c.inputBorder.r, c.inputBorder.g, c.inputBorder.b,
                                            180.0f / 255.0f * a), 1.0f * g_uiCtx.scale, 3.0f * g_uiCtx.scale);
    // 之前色：绑定模式=绑定那刻的原色快照（可还原）；独立模式=实时等同当前色（跟随，显示一样）
    {
        int pr = r, pg = g, pb = b, pa = st.alpha;
        if (st.bindMode != 0 && st.hasPrev) { pr = st.prevR; pg = st.prevG; pb = st.prevB; pa = st.prevA; }
        picker::drawCheckerboard(dev, L.prevSwatch, a, 3.0f * g_uiCtx.scale);
        drawRoundedRect(dev, L.prevSwatch, Color(pr / 255.f, pg / 255.f, pb / 255.f,
                                                 pa / 255.0f * a), 3.0f * g_uiCtx.scale);
        drawRoundedBorder(dev, L.prevSwatch,
                          st.hoverPrev ? Color(c.accent.r, c.accent.g, c.accent.b, a)
                                       : Color(c.inputBorder.r, c.inputBorder.g, c.inputBorder.b,
                                               180.0f / 255.0f * a),
                          (st.hoverPrev ? 1.5f : 1.0f) * g_uiCtx.scale, 3.0f * g_uiCtx.scale);
        wctl::drawTextCenterAt(dev, g_uiCtx.smallFont, u8"当前色", L.curLbl,
                               Color(c.text.r, c.text.g, c.text.b, 180.0f / 255.0f * a));
        wctl::drawTextCenterAt(dev, g_uiCtx.smallFont, u8"之前色", L.prevLbl,
                               st.hoverPrev ? Color(c.accent.r, c.accent.g, c.accent.b, a)
                                            : Color(c.text.r, c.text.g, c.text.b, 180.0f / 255.0f * a));
    }

    // R/G/B 数值（标签 + 可编辑输入框：点击聚焦定位光标，输入数字，
    // 方向键/Home/End 移动光标，Enter/失焦提交）
    const char* rgbNames[] = {"R", "G", "B"};
    int rgbVals[] = {r, g, b};
    for (int i = 0; i < 3; i++) {
        wctl::drawTextLeftAt(dev, g_uiCtx.smallFont, rgbNames[i], L.rgbLbl[i].pos,
                             L.rgbLbl[i].size.y, Color(c.text.r, c.text.g, c.text.b, 200.0f / 255.0f * a));
        bool focused = tis.focused(st.edits[i]);
        char buf[8];
        const char* shown = st.edits[i].buf.c_str();
        if (!focused) { snprintf(buf, sizeof(buf), "%d", rgbVals[i]); shown = buf; }
        wctl::drawInput(dev, L.rgbVal[i], shown, nullptr,
                        focused, focused && tis.caretOn(), a, 12.0f,
                        focused ? st.edits[i].caret : -1);
    }

    // Alpha 滑条（保留）+ 可编辑数值框
    wctl::drawTextLeftAt(dev, g_uiCtx.smallFont, "透明度", L.alphaLbl.pos,
                         L.alphaLbl.size.y, Color(c.text.r, c.text.g, c.text.b, 200.0f / 255.0f * a));
    wctl::drawSlider(dev, L.alphaSlider, st.alpha / 255.0f, st.hoverAlpha || st.drag == 3, a);
    {
        bool focused = tis.focused(st.edits[3]);
        char aBuf[8];
        const char* shown = st.edits[3].buf.c_str();
        if (!focused) { snprintf(aBuf, sizeof(aBuf), "%d", st.alpha); shown = aBuf; }
        wctl::drawInput(dev, L.alphaVal, shown, nullptr,
                        focused, focused && tis.caretOn(), a, 12.0f,
                        focused ? st.edits[3].caret : -1);
    }

    // Hex 输入框（#RGB / #RRGGBB / #AARRGGBB，# 可省略）
    wctl::drawTextLeftAt(dev, g_uiCtx.smallFont, "Hex", L.hexLbl.pos,
                         L.hexLbl.size.y, Color(c.text.r, c.text.g, c.text.b, 200.0f / 255.0f * a));
    {
        bool focused = tis.focused(st.edits[4]);
        char hexBuf[10];
        const char* shown = st.edits[4].buf.c_str();
        if (!focused) { snprintf(hexBuf, sizeof(hexBuf), "#%02X%02X%02X", r, g, b); shown = hexBuf; }
        wctl::drawInput(dev, L.hexVal, shown, nullptr,
                        focused, focused && tis.caretOn(), a, 12.0f,
                        focused ? st.edits[4].caret : -1);
    }

    // 明亮 / 饱和度 步进按钮（左组：明亮 [-] [+]；右组：饱和度 [-] [+]）
    {
        auto stepBtn = [&](const Rect& b, const char* txt, bool hov) {
            float br = 4.0f * g_uiCtx.scale * g_uiCtx.radiusScale();
            drawRoundedRect(dev, b, Color(c.btnBg2.r, c.btnBg2.g, c.btnBg2.b,
                                          220.0f / 255.0f * a), br);
            drawRoundedBorder(dev, b,
                              hov ? Color(c.accent.r, c.accent.g, c.accent.b, a)
                                  : Color(c.inputBorder.r, c.inputBorder.g, c.inputBorder.b,
                                          180.0f / 255.0f * a),
                              1.0f * g_uiCtx.scale, br);
            wctl::drawTextCenterAt(dev, g_uiCtx.smallFont, txt, b,
                                   hov ? Color(c.accent.r, c.accent.g, c.accent.b, a)
                                       : Color(c.text.r, c.text.g, c.text.b, 220.0f / 255.0f * a));
        };
        auto stepLbl = [&](const Rect& lb, const char* name) {
            wctl::drawTextLeftAt(dev, g_uiCtx.smallFont, name, lb.pos, lb.size.y,
                                 Color(c.text.r, c.text.g, c.text.b, 200.0f / 255.0f * a));
        };
        stepLbl(L.brightLbl, u8"明亮");
        stepBtn(L.brightMinus, "-", st.hoverStep[0]);
        stepBtn(L.brightPlus, "+", st.hoverStep[1]);
        stepLbl(L.satLbl, u8"饱和度");
        stepBtn(L.satMinus, "-", st.hoverStep[2]);
        stepBtn(L.satPlus, "+", st.hoverStep[3]);
    }

    // 预设色网格
    for (int i = 0; i < L.presetCount; i++) {
        Color pc = Color::rgb((picker::kPresets[i] >> 16) & 0xFF,
                              (picker::kPresets[i] >> 8) & 0xFF,
                              picker::kPresets[i] & 0xFF);
        pc.a = a;
        drawRoundedRect(dev, L.presets[i], pc, 3.0f * g_uiCtx.scale);
        if (st.hoverPreset == i)
            drawRoundedBorder(dev, L.presets[i], Color(c.accent.r, c.accent.g, c.accent.b, a),
                              1.5f * g_uiCtx.scale, 3.0f * g_uiCtx.scale);
    }
}

// ============================================================
// 点击：SV/Hue/Alpha 按下即取值，预设色点击应用
// ============================================================
bool clickColorPicker(WindowInstance& win, const Vec2& m) {
    picker::State& st = win.getState<picker::State>();
    picker::Layout L = picker::layout(win);

    // 精确输入框：点击进入编辑（系统路由前已统一提交旧焦点；
    // 点击 x 同时定位光标到最近字符边界）
    for (int i = 0; i < 3; i++) {
        if (L.rgbVal[i].contains(m)) {
            picker::beginEdit(win, st, i, &L.rgbVal[i], m.x);
            return true;
        }
    }
    if (L.alphaVal.contains(m)) {
        picker::beginEdit(win, st, 3, &L.alphaVal, m.x);
        return true;
    }
    if (L.hexVal.contains(m)) {
        picker::beginEdit(win, st, 4, &L.hexVal, m.x);
        return true;
    }

    // 之前色：按下还原（绑定模式 → 还原到绑定那刻的原色；独立模式 hasPrev 不参与 → 无还原）
    if (L.prevSwatch.contains(m)) {
        if (st.bindMode != 0 && st.hasPrev) {
            picker::setRGB(st, st.prevR, st.prevG, st.prevB);
            st.alpha = st.prevA;
        }
        return true;
    }

    // 明亮 / 饱和度 步进按钮（每步 5%，越界钳制；改动立即写回目标）
    {
        const float kStep = 0.05f;
        if (L.brightMinus.contains(m)) {
            st.v = clampf(st.v - kStep, 0.0f, 1.0f); picker::writeBack(st); return true;
        }
        if (L.brightPlus.contains(m)) {
            st.v = clampf(st.v + kStep, 0.0f, 1.0f); picker::writeBack(st); return true;
        }
        if (L.satMinus.contains(m)) {
            st.s = clampf(st.s - kStep, 0.0f, 1.0f); picker::writeBack(st); return true;
        }
        if (L.satPlus.contains(m)) {
            st.s = clampf(st.s + kStep, 0.0f, 1.0f); picker::writeBack(st); return true;
        }
    }

    if (L.sv.contains(m)) {
        st.drag = 1;
        st.s = wctl::sliderValueAt(Rect(L.sv.pos, Vec2(L.sv.size.x, 0)), m.x);
        st.v = 1.0f - clampf((m.y - L.sv.pos.y) / L.sv.size.y, 0.0f, 1.0f);
        picker::writeBack(st);
        return true;
    }
    if (L.hue.contains(m)) {
        st.drag = 2;
        st.h = (1.0f - clampf((m.y - L.hue.pos.y) / L.hue.size.y, 0.0f, 1.0f)) * 360.0f;
        picker::writeBack(st);
        return true;
    }
    // Alpha 滑条（宽容命中区）
    Rect aHit(L.alphaSlider.pos.x - 8, L.alphaSlider.pos.y - 10,
              L.alphaSlider.size.x + 16, L.alphaSlider.size.y + 20);
    if (aHit.contains(m)) {
        st.drag = 3;
        st.alpha = (int)(wctl::sliderValueAt(L.alphaSlider, m.x) * 255.0f + 0.5f);
        return true;
    }
    // 预设色
    for (int i = 0; i < L.presetCount; i++) {
        if (L.presets[i].contains(m)) {
            picker::setRGB(st, (int)((picker::kPresets[i] >> 16) & 0xFF),
                                (int)((picker::kPresets[i] >> 8) & 0xFF),
                                (int)(picker::kPresets[i] & 0xFF));
            return true;
        }
    }
    return false;
}

// ============================================================
// hover 态
// ============================================================
void hoverColorPicker(WindowInstance& win, const Vec2& m) {
    picker::State& st = win.getState<picker::State>();
    picker::Layout L = picker::layout(win);
    st.hoverSV = L.sv.contains(m);
    st.hoverHue = L.hue.contains(m);
    Rect aHit(L.alphaSlider.pos.x - 8, L.alphaSlider.pos.y - 10,
              L.alphaSlider.size.x + 16, L.alphaSlider.size.y + 20);
    st.hoverAlpha = aHit.contains(m);
    st.hoverPreset = -1;
    for (int i = 0; i < L.presetCount; i++)
        if (L.presets[i].contains(m)) { st.hoverPreset = i; break; }
    st.hoverPrev = L.prevSwatch.contains(m);
    st.hoverStep[0] = L.brightMinus.contains(m);
    st.hoverStep[1] = L.brightPlus.contains(m);
    st.hoverStep[2] = L.satMinus.contains(m);
    st.hoverStep[3] = L.satPlus.contains(m);
}

// ============================================================
// 拖拽（WindowManager 转发）与释放
// ============================================================
void dragColorPicker(WindowInstance& win, const Vec2& m) {
    picker::State& st = win.getState<picker::State>();
    picker::Layout L = picker::layout(win);
    switch (st.drag) {
    case 1:
        st.s = wctl::sliderValueAt(Rect(L.sv.pos, Vec2(L.sv.size.x, 0)), m.x);
        st.v = 1.0f - clampf((m.y - L.sv.pos.y) / L.sv.size.y, 0.0f, 1.0f);
        picker::writeBack(st);
        break;
    case 2:
        st.h = (1.0f - clampf((m.y - L.hue.pos.y) / L.hue.size.y, 0.0f, 1.0f)) * 360.0f;
        picker::writeBack(st);
        break;
    case 3:
        st.alpha = (int)(wctl::sliderValueAt(L.alphaSlider, m.x) * 255.0f + 0.5f);
        break;
    default:
        break;
    }
}

void releaseColorPicker(WindowInstance& win, const Vec2&) {
    win.findState<picker::State>()->drag = 0;
}

// ============================================================

// 主题工作室 → 颜色选择器的目标注入（模块间参数传递）
//
// 流程：Bind*（记录待绑定参数）→ pendingOpen（帧末创建窗口）→
//       PollColorPickerBinding（每帧消费：写入 State + 同步 HSV + 停靠定位）
// 窗口尚未创建时参数保留，下一帧继续尝试（天然适配异步创建）。
// ============================================================
namespace picker {
// 待绑定参数（Bind* 写入，Poll 消费后清空）
static int  s_waitMode = 0;          // 0=无 1=主题色项 2=背景槽位
static int  s_waitTheme = -1;
static int  s_waitItem = -1;
static Color ThemeColors::*s_waitField = nullptr;
static int  s_waitSlot = -1;
static int  s_waitSpawnNear = -1;    // 停靠参考窗口 id
} // namespace picker

void BindColorPickerTarget(int themeIndex, Color ThemeColors::*field) {
    picker::s_waitMode = 1;
    picker::s_waitTheme = themeIndex;
    picker::s_waitItem = -1;
    picker::s_waitField = field;
    picker::s_waitSlot = -1;
}

void BindColorPickerThemeItem(int themeIndex, int itemIndex) {
    picker::s_waitMode = 1;
    picker::s_waitTheme = themeIndex;
    picker::s_waitItem = itemIndex;
    picker::s_waitField = ColorItemMember(itemIndex);   // 55 色项表查成员指针
}

void BindColorPickerBgSlot(int slot) {
    picker::s_waitMode = 2;
    picker::s_waitTheme = -1;
    picker::s_waitItem = -1;
    picker::s_waitField = nullptr;
    picker::s_waitSlot = slot;
}

void SetColorPickerSpawnNear(int refWinId) {
    picker::s_waitSpawnNear = refWinId;
}

void PollColorPickerBinding() {
    bool hasBind = picker::s_waitMode != 0;
    bool hasSpawn = picker::s_waitSpawnNear >= 0;
    if (!hasBind && !hasSpawn) return;
    WindowInstance* cp = g_layerStack.findWindowByConfig("color_picker");
    if (!cp) return;  // 窗口还没创建：参数保留，下帧再试

    // 1. 绑定参数 → State（每次绑定都重新同步一次 HSV，与参考一致）
    if (hasBind) {
        picker::State& st = cp->getState<picker::State>();
        st.bindMode = picker::s_waitMode;
        st.targetTheme = picker::s_waitTheme;
        st.targetItemIndex = picker::s_waitItem;
        st.targetField = picker::s_waitField;
        st.targetBgSlot = picker::s_waitSlot;
        picker::syncFromTarget(st);
        // 快照绑定那刻的目标原色 → 「之前色」色块（绑定模式）；按下可还原
        {
            int pr, pg, pb;
            picker::currentRGB(st, pr, pg, pb);
            st.prevR = pr; st.prevG = pg; st.prevB = pb; st.prevA = st.alpha;
            st.hasPrev = true;
        }
        picker::s_waitMode = 0;
        picker::s_waitTheme = -1;
        picker::s_waitItem = -1;
        picker::s_waitField = nullptr;
        picker::s_waitSlot = -1;
    }

    // 2. 停靠定位：参考窗口右侧（越屏翻左、再越屏水平居中；y 顶部对齐 + 底部 clamp）
    //    仅新建/淡入期生效——窗口已打开时用户点击其他色块不移动窗口（与参考一致）
    if (hasSpawn && cp->alpha < 0.5f) {
        WindowInstance* ref = g_layerStack.findWindow(picker::s_waitSpawnNear);
        if (ref) {
            float gap = 8.0f * g_uiCtx.scale;
            cp->x = ref->x + ref->w + gap;
            if (cp->x + cp->w > (float)g_uiCtx.winW) {
                cp->x = ref->x - cp->w - gap;
                if (cp->x < 0) cp->x = ((float)g_uiCtx.winW - cp->w) * 0.5f;
            }
            cp->y = ref->y;
            if (cp->y + cp->h > (float)g_uiCtx.winH)
                cp->y = (float)g_uiCtx.winH - cp->h;
            if (cp->y < 0) cp->y = 0;
        }
        picker::s_waitSpawnNear = -1;
    } else if (hasSpawn) {
        picker::s_waitSpawnNear = -1;  // 窗口已打开：只消费停靠请求，不移动
    }
}

} // namespace ui