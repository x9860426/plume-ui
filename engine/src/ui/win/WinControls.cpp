// AI生成
// ============================================================
// 窗口内容共享控件原语实现（主题色驱动，无任何窗口类型分支）
// 画法对照参考 ui_floatwin.cpp / ui_core.cpp 1:1 移植：
//   按钮 = FillGradientRoundedRect（垂直渐变 + BorderWidth 边框）
//   滑条 = 水平渐变胶囊 + 固定 14px 圆手柄（L1102-1110）
// ============================================================
#include "WinControls.h"
#include "../Element.h"
#include "../Font.h"
#include "../Theme.h"
#include "UiContext.h"
#include "StyleTpl.h"
#include "../../core/Log.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_set>

namespace ui {
namespace wctl {

// 悬浮窗文字统一放大系数（用户观感偏小，整体 +20%；仅影响悬浮窗体系文字，
// 主界面 MainUI 有独立文字函数不受影响）
static constexpr float kWinFontScale = 1.2f;

// 颜色线性插值（渐变端点色计算）
static Color lerpColor(const Color& a, const Color& b, float t) {
    return Color(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t,
                 a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t);
}

void drawTextScaled(RenderDevice* dev, Font* f, const char* utf8, const Rect& r,
                    const Color& c, float px, int align) {
    if (!f || !f->isReady() || !utf8 || !utf8[0]) return;
    // 逻辑像素字号 px × 系统缩放 × 悬浮窗放大 = 期望实际高度；
    // 实际高度 = pixelHeight × renderScale × scale参数 → 反解 scale
    float denom = f->pixelHeight() * f->renderScale();
    float sc = denom > 0.0f ? (px * g_uiCtx.scale * kWinFontScale) / denom : 1.0f;
    float tw = f->measureText(utf8) * sc;
    float th = f->scaledLineHeight() * sc;
    float x = r.pos.x;
    if (align == 1)      x = r.pos.x + (r.size.x - tw) * 0.5f;
    else if (align == 2) x = r.pos.x + r.size.x - tw;
    float y = r.pos.y + (r.size.y - th) * 0.5f;
    drawText(dev, f, utf8, Vec2(x, y), c, sc);
}

void drawBtn(RenderDevice* dev, const Rect& r, const char* utf8,
              bool hover, float alpha, float px, float zoom, int id) {
    ThemeColors& c = currentColors();
    // 游戏ID：0=跟随全局 btnStyle；1=系统默认 2=方形 3=圆润（内部渲染值 = ID-1）
    int style = tpl::internalOf(id, g_uiCtx.btnStyle);
    // zoom：沙盘等局部缩放场景传视口倍率（默认 1）；圆角/边框与矩形同比例
    float rad = c.btnRadius * g_uiCtx.scale * g_uiCtx.radiusScale() * zoom;
    if (style == 1)       rad = 0.0f;                                 // 方形（内部 1）
    else if (style == 2)  rad = std::min(r.size.x, r.size.y) * 0.5f;  // 圆润（内部 2）
    // 垂直渐变底（参考 FillGradientRoundedRect：btnBg1→btnBg2 / hover btnHover1→btnHover2）
    const Color& c1 = hover ? c.btnHover1 : c.btnBg1;
    const Color& c2 = hover ? c.btnHover2 : c.btnBg2;
    drawRoundedGradientRect(dev, r,
                            Color(c1.r, c1.g, c1.b, alpha),
                            Color(c2.r, c2.g, c2.b, alpha), rad);
    // 边框（参考 BorderWidth()：厚度驱动线宽；沙盘 zoom 同比例）
    const Color& bc = hover ? c.btnHoverBorder : c.btnBorder;
    drawRoundedBorder(dev, r, Color(bc.r, bc.g, bc.b, alpha),
                      g_uiCtx.borderWidth() * zoom, rad);
    // 文字（px>0 按逻辑字号；否则字体默认尺寸）
    if (px > 0.0f) {
        drawTextScaled(dev, g_uiCtx.font, utf8, r,
                        Color(c.btnText.r, c.btnText.g, c.btnText.b, alpha), px, 1);
    } else {
        drawTextCenterAt(dev, g_uiCtx.font, utf8, r,
                         Color(c.btnText.r, c.btnText.g, c.btnText.b, alpha));
    }
}

void drawBtnSecondary(RenderDevice* dev, const Rect& r, const char* utf8,
                      bool hover, float alpha, float px, int id) {
    ThemeColors& c = currentColors();
    // 游戏ID：0=跟随全局 btnStyle；1=系统默认 2=方形 3=圆润（内部渲染值 = ID-1）
    int style = tpl::internalOf(id, g_uiCtx.btnStyle);
    float rad = c.btnRadius * g_uiCtx.scale * g_uiCtx.radiusScale();
    if (style == 1)       rad = 0.0f;
    else if (style == 2)  rad = std::min(r.size.x, r.size.y) * 0.5f;
    // 底：inputBg@220（参考 L1057：SolidBrush cancelBg(inputBg, 220*a)）
    drawRoundedRect(dev, r,
                    Color(c.inputBg.r, c.inputBg.g, c.inputBg.b,
                          220.0f / 255.0f * alpha), rad);
    // 边框：hover accent@180 / 普通 inputBorder@120（参考 L1059）
    Color bc = hover
        ? Color(c.accent.r, c.accent.g, c.accent.b, 180.0f / 255.0f * alpha)
        : Color(c.inputBorder.r, c.inputBorder.g, c.inputBorder.b,
                120.0f / 255.0f * alpha);
    drawRoundedBorder(dev, r, bc, g_uiCtx.borderWidth(), rad);
    // 文字：hover accent / 普通 text（参考 L1061）
    Color tc = hover
        ? Color(c.accent.r, c.accent.g, c.accent.b, alpha)
        : Color(c.text.r, c.text.g, c.text.b, alpha);
    if (px > 0.0f) drawTextScaled(dev, g_uiCtx.font, utf8, r, tc, px, 1);
    else           drawTextCenterAt(dev, g_uiCtx.font, utf8, r, tc);
}

void drawProgress(RenderDevice* dev, const Rect& r, float t, float alpha) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    // 「特别绘制」族（内部 1..10 = 游戏ID 2..11）：统一委托，默认分支一字未动
    if (g_uiCtx.progressStyle > 0) {
        drawProgressStyled(dev, r, t, alpha, g_uiCtx.progressStyle);
        return;
    }
    ThemeColors& c = currentColors();
    // 胶囊轨道：圆角 = 高度一半（任意高度自适应）
    float trackRad = r.size.y * 0.5f;
    drawRoundedRect(dev, r,
                    Color(c.sliderGroove.r, c.sliderGroove.g, c.sliderGroove.b,
                          200.0f / 255.0f * alpha), trackRad);
    // 填充：circGrad 开 = accent→accent2 水平渐变（与环形进度同开关联动），关 = 纯 accent
    float fillW = t * r.size.x;
    if (fillW > 0.0f) {
        Rect fill(r.pos, Vec2(fillW, r.size.y));
        if (g_uiCtx.circGrad) {
            drawRoundedGradientRectH(dev, fill,
                Color(c.accent.r, c.accent.g, c.accent.b, alpha),
                Color(c.accent2.r, c.accent2.g, c.accent2.b, alpha), trackRad);
        } else {
            drawRoundedRect(dev, fill,
                Color(c.accent.r, c.accent.g, c.accent.b, alpha), trackRad);
        }
    }
}

// ============================================================
// 条形进度「特别绘制」族（内部 1..3 = 游戏ID 2..4）
// ============================================================
// 由 wctl::drawProgress 在 g_uiCtx.progressStyle>0 时统一分发到此。
// 主题色单源、零裸色；各 case 只负责「轨道 + 填充」，
// 左上标签 / 右上百分比仍由 pctl::drawProgressBar 在外层绘制（本函数不碰文字）。
// 1=分段 2=霓虹 3=斜纹。
void drawProgressStyled(RenderDevice* dev, const Rect& r, float t, float alpha, int style) {
    if (!dev || r.size.x <= 0.0f || r.size.y <= 0.0f) return;
    ThemeColors& c = currentColors();
    float s = g_uiCtx.scale;
    float bw = g_uiCtx.borderWidth();
    Color ac (c.accent.r,  c.accent.g,  c.accent.b,  alpha);
    Color ac2(c.accent2.r, c.accent2.g, c.accent2.b, alpha);
    Color grv(c.sliderGroove.r, c.sliderGroove.g, c.sliderGroove.b, 200.0f / 255.0f * alpha);
    Color ib (c.inputBorder.r, c.inputBorder.g, c.inputBorder.b, alpha);
    Color bg1(c.bg1.r, c.bg1.g, c.bg1.b, alpha);
    float h = r.size.y;
    float rad = h * 0.5f;               // 胶囊圆角（随高度自适应）
    float fillW = t * r.size.x;
    Rect fill(r.pos, Vec2(fillW, h));

    switch (style) {
    case 1: { // 分段：10 段块 + 段间隔，点亮段按序 accent→accent2
        const int N = 10;
        float gap = 2.0f * s;
        float sw = (r.size.x - gap * (float)(N - 1)) / (float)N;
        if (sw <= 0.0f) { drawRoundedRect(dev, r, grv, rad); break; }
        for (int i = 0; i < N; i++) {
            Rect seg(Vec2(r.pos.x + (float)i * (sw + gap), r.pos.y), Vec2(sw, h));
            float lit = t * (float)N - (float)i;      // >0 = 该段被点亮
            if (lit <= 0.0f) {
                drawRoundedRect(dev, seg, grv, 2.0f * s);
            } else {
                float k = (float)i / (float)(N - 1);   // 0..1 → accent→accent2
                Color lc(ac.r + (ac2.r - ac.r) * k, ac.g + (ac2.g - ac.g) * k,
                         ac.b + (ac2.b - ac.b) * k, alpha);
                drawRoundedRect(dev, seg, lc, 2.0f * s);
            }
        }
        break; }

    case 2: { // 霓虹：暗底轨 + 外发光 + 渐变填充 + accent 描边
        drawRoundedRect(dev, r, Color(bg1.r, bg1.g, bg1.b, 210.0f / 255.0f * alpha), rad);
        if (fillW > 0.0f) {
            drawRoundedGradientRectH(dev, fill, ac, ac2, rad);
            Rect g1(Vec2(r.pos.x - 1.0f * s, r.pos.y - 1.0f * s),
                    Vec2(fillW + 2.0f * s, h + 2.0f * s));
            drawRoundedBorder(dev, g1, Color(ac.r, ac.g, ac.b, 70.0f / 255.0f * alpha),
                              bw + 1.0f * s, rad + 1.0f * s);
            drawRoundedBorder(dev, r, Color(ac.r, ac.g, ac.b, 180.0f / 255.0f * alpha), bw, rad);
        } else {
            drawRoundedBorder(dev, r, Color(ib.r, ib.g, ib.b, 140.0f / 255.0f * alpha), bw, rad);
        }
        break; }

    case 3: { // 斜纹：轨道 + 渐变填充 + 填充体内 45° 斜线段（数学裁剪到填充矩形）
        drawRoundedRect(dev, r, grv, rad);
        if (fillW > 0.0f) {
            drawRoundedGradientRectH(dev, fill, ac, ac2, rad);
            float step = 8.0f * s;
            Color sl(1, 1, 1, 55.0f / 255.0f * alpha);
            float right = r.pos.x + fillW;
            for (float x0 = r.pos.x - h; x0 < right; x0 += step) {
                float x1 = x0,        y1 = r.pos.y;
                float x2 = x0 + h,    y2 = r.pos.y + h;
                float lo = (r.pos.x > x1) ? r.pos.x : x1;
                float hi = (right  < x2) ? right  : x2;
                if (hi <= lo) continue;
                float t1 = (lo - x1) / (x2 - x1);
                float t2 = (hi - x1) / (x2 - x1);
                drawLine(dev, x1 + (x2 - x1) * t1, y1 + (y2 - y1) * t1,
                         x1 + (x2 - x1) * t2, y1 + (y2 - y1) * t2, 2.0f * s, sl);
            }
        }
        break; }

    default: // 兜底 = 胶囊轨 + 通用填充（等价默认）
        drawRoundedRect(dev, r, grv, rad);
        if (fillW > 0.0f) drawRoundedRect(dev, fill, ac, rad);
        break;
    }
}

// ---- 输入框文本几何/光标换算（单源；drawInput 光标绘制与
//      TextInputSystem::placeCaret 点击定位共用，公式不得另写） ----

Rect inputTextRect(const Rect& r) {
    float s = g_uiCtx.scale;
    return Rect(Vec2(r.pos.x + 10.0f * s, r.pos.y), Vec2(r.size.x - 20.0f * s, r.size.y));
}

static float inputFontScale(Font* f, float px) {
    float denom = f->pixelHeight() * f->renderScale();
    return denom > 0.0f ? (px * g_uiCtx.scale * kWinFontScale) / denom : 1.0f;
}

float fontPxForScreen(float screenPx) {
    float gs = g_uiCtx.scale;
    return gs > 0.0f ? screenPx / (gs * kWinFontScale) : screenPx;
}

float caretXFromByte(Font* f, const char* value, int caretByte, float px) {
    if (!f || !f->isReady()) return 0.0f;
    if (caretByte < 0 || !value) return f->measureText(value ? value : "") * inputFontScale(f, px);
    int len = (int)std::strlen(value);
    if (caretByte > len) caretByte = len;
    if (caretByte == len) return f->measureText(value) * inputFontScale(f, px);
    std::string prefix(value, (size_t)caretByte);
    return f->measureText(prefix.c_str()) * inputFontScale(f, px);
}

int byteFromCaretX(Font* f, const char* value, float localX, float px) {
    if (!f || !f->isReady() || !value) return 0;
    int len = (int)std::strlen(value);
    if (len == 0 || localX <= 0.0f) return 0;
    float sc = inputFontScale(f, px);
    if (localX >= f->measureText(value) * sc) return len;
    // 逐字符边界推进（边界 i 的 x = 前 i 字节宽度；取离 localX 最近边界）；
    // buf 上限 64 字节，O(n²) 前缀测量无性能问题
    int best = 0;
    float bestDist = localX;               // 边界 0 距离 = localX
    int i = 0;
    while (i < len) {
        int n = 1;
        while (i + n < len && ((unsigned char)value[i + n] & 0xC0) == 0x80) n++;
        int next = i + n;
        std::string prefix(value, (size_t)next);
        float bx = f->measureText(prefix.c_str()) * sc;
        float d = std::abs(bx - localX);
        if (d < bestDist) { bestDist = d; best = next; }
        i = next;
    }
    return best;
}

void drawInput(RenderDevice* dev, const Rect& r, const char* value,
               const char* placeholder, bool focused, bool caretOn,
               float alpha, float px, int caretByte) {
    ThemeColors& c = currentColors();
    Font* f = g_uiCtx.font;
    float s = g_uiCtx.scale;
    float rad = c.inputRadius * s * g_uiCtx.radiusScale();
    // 底：inputBg@240（对照参考 L1086）
    drawRoundedRect(dev, r,
                    Color(c.inputBg.r, c.inputBg.g, c.inputBg.b,
                          240.0f / 255.0f * alpha), rad);
    // 边框：焦点 accent@200 / 普通 inputBorder@120（焦点反馈是本原语核心增强）
    Color bc = focused
        ? Color(c.accent.r, c.accent.g, c.accent.b, 200.0f / 255.0f * alpha)
        : Color(c.inputBorder.r, c.inputBorder.g, c.inputBorder.b,
                120.0f / 255.0f * alpha);
    drawRoundedBorder(dev, r, bc, g_uiCtx.borderWidth(), rad);
    if (!f || !f->isReady()) return;
    // 文字区：内边距单源（inputTextRect）
    Rect tr = inputTextRect(r);

    bool empty = !value || !value[0];
    if (empty && placeholder && placeholder[0]) {
        // 空值 + 占位提示：灰色（不画值、不画光标）
        wctl::drawTextScaled(dev, f, placeholder, tr,
                             Color(c.text.r, c.text.g, c.text.b,
                                   120.0f / 255.0f * alpha), px, 0);
        return;
    }
    if (!empty) {
        wctl::drawTextScaled(dev, f, value, tr,
                             Color(c.inputText.r, c.inputText.g, c.inputText.b, alpha),
                             px, 0);
    }
    // 光标：焦点 + 闪烁开。caretByte = UTF-8 字节偏移（-1 = 文末）；
    // 中间偏移换算单源 caretXFromByte（UTF-8 前缀测量，中文定位准确）
    if (focused && caretOn) {
        float textW = caretXFromByte(f, value, caretByte, px);
        float cx = tr.pos.x + textW + 1.0f * s;
        // 竖线：高 = 输入框 55% 居中（视觉平衡，与字号弱耦合）
        float ch = r.size.y * 0.55f;
        drawRect(dev, Rect(Vec2(cx, r.pos.y + (r.size.y - ch) * 0.5f),
                           Vec2(1.5f * s, ch)),
                 Color(c.inputText.r, c.inputText.g, c.inputText.b, alpha));
    }
}

void drawSlider(RenderDevice* dev, const Rect& groove, float t,
                bool hover, float alpha, float zoom) {

    ThemeColors& c = currentColors();
    if (t < 0) t = 0; if (t > 1) t = 1;
    (void)hover;  // 参考无 hover 放大效果（固定手柄），参数保留供调用方语义
    // zoom：沙盘等局部缩放场景传视口倍率（默认 1 = 全局一致标准外观）；
    // 槽高/手柄/圆角/边框整体 ×zoom，与 groove 矩形同比例，外观不失调
    float rad = groove.size.y * 0.5f;
    float fillW = groove.size.x * t;
    float s = g_uiCtx.scale * zoom;

    // 「中间带值」模板（内部 7..10 = 游戏ID 8..11）：值文字整框居中，
    // 统一委托 drawSliderValueBar。放在最前：把新号段收口到单一渲染入口，
    // 下方 0..6 分支保持不变。
    if (g_uiCtx.sliderStyle >= 7) {
        char vb[16];
        snprintf(vb, sizeof(vb), "%d%%", (int)(t * 100.0f + 0.5f));
        int var = g_uiCtx.sliderStyle - 7;
        if (var < 0) var = 0;
        if (var > 3) var = 3;
        drawSliderValueBar(dev, groove, t, vb, hover, alpha, zoom, var);
        return;
    }

    if (g_uiCtx.sliderStyle == 1 || g_uiCtx.sliderStyle == 2) {
        // 霓虹细轨（对照暗黑.py：6px 细槽 圆角3 + accent 手柄；2=竖直胶囊手柄）
        float gh = 6.0f * s;
        Rect gr(Vec2(groove.pos.x, groove.pos.y + groove.size.y * 0.5f - gh * 0.5f),
                Vec2(groove.size.x, gh));
        drawRoundedRect(dev, gr,
                        Color(c.sliderGroove.r, c.sliderGroove.g, c.sliderGroove.b,
                              220.0f / 255.0f * alpha), 3.0f * s);
        if (fillW > 0.5f) {
            // 填充完整渐变拉伸（accent → accent2 铺满填充长度，与默认音量条一致）+ 两端补圆角
            Color cl(c.accent.r, c.accent.g, c.accent.b, 220.0f / 255.0f * alpha);
            Color cr(c.accent2.r, c.accent2.g, c.accent2.b, 220.0f / 255.0f * alpha);
            drawHorizontalGradientRect(dev, Rect(gr.pos, Vec2(fillW, gh)), cl, cr);
            if (fillW > 6.0f * s) {
                fillCircle(dev, gr.pos.x + 3.0f * s, gr.pos.y + gh * 0.5f, 3.0f * s, cl);
                fillCircle(dev, gr.pos.x + fillW - 3.0f * s, gr.pos.y + gh * 0.5f, 3.0f * s, cr);
            }
        }
        float hx = groove.pos.x + fillW, hy = groove.pos.y + groove.size.y * 0.5f;
        Color hc(c.accent.r, c.accent.g, c.accent.b, alpha);
        if (g_uiCtx.sliderStyle == 2) {
            // 竖柄：4×16 竖胶囊
            Rect hr(Vec2(hx - 2.0f * s, hy - 8.0f * s), Vec2(4.0f * s, 16.0f * s));
            drawRoundedRect(dev, hr, hc, 2.0f * s);
        } else {
            fillCircle(dev, hx, hy, 8.0f * s, hc);
        }
        return;
    }
    if (g_uiCtx.sliderStyle == 3) {
        // 春意描边（对照春天.py：8px 槽 圆角4 + 1px 边框 + 18px 白圆手柄 + accent 描边）
        float gh = 8.0f * s;
        Rect gr(Vec2(groove.pos.x, groove.pos.y + groove.size.y * 0.5f - gh * 0.5f),
                Vec2(groove.size.x, gh));
        drawRoundedRect(dev, gr,
                        Color(c.sliderGroove.r, c.sliderGroove.g, c.sliderGroove.b,
                              220.0f / 255.0f * alpha), 4.0f * s);
        drawRoundedBorder(dev, gr,
                          Color(c.inputBorder.r, c.inputBorder.g, c.inputBorder.b,
                                160.0f / 255.0f * alpha), 1.0f * s, 4.0f * s);
        if (fillW > 0.5f) {
            // 填充完整渐变拉伸（accent → accent2 铺满填充长度）+ 两端补圆角
            Color cl(c.accent.r, c.accent.g, c.accent.b, 220.0f / 255.0f * alpha);
            Color cr(c.accent2.r, c.accent2.g, c.accent2.b, 220.0f / 255.0f * alpha);
            drawHorizontalGradientRect(dev, Rect(gr.pos, Vec2(fillW, gh)), cl, cr);
            if (fillW > 8.0f * s) {
                fillCircle(dev, gr.pos.x + 4.0f * s, gr.pos.y + gh * 0.5f, 4.0f * s, cl);
                fillCircle(dev, gr.pos.x + fillW - 4.0f * s, gr.pos.y + gh * 0.5f, 4.0f * s, cr);
            }
        }
        Rect hr(Vec2(groove.pos.x + fillW - 9.0f * s,
                     groove.pos.y + groove.size.y * 0.5f - 9.0f * s),
                Vec2(18.0f * s, 18.0f * s));
        drawRoundedRect(dev, hr, Color(1, 1, 1, alpha), hr.size.x * 0.5f);
        drawRoundedBorder(dev, hr,
                          Color(c.accent.r, c.accent.g, c.accent.b, alpha),
                          2.0f * s, hr.size.x * 0.5f);
        return;
    }
    if (g_uiCtx.sliderStyle == 4) {
        // 像素霓虹（对照电玩.py：6px 直角槽 + 16px 方块手柄 + accent2 填充 + accent 描边）
        // 手柄圆角跟随全局圆润强度（历史遗留修复：原先写死 2px，导致圆润滑条对它无效）
        float khRad = 2.0f * s * g_uiCtx.radiusScale();
        float gh = 6.0f * s;
        Rect gr(Vec2(groove.pos.x, groove.pos.y + groove.size.y * 0.5f - gh * 0.5f),
                Vec2(groove.size.x, gh));
        drawRoundedRect(dev, gr,
                        Color(c.sliderGroove.r, c.sliderGroove.g, c.sliderGroove.b,
                              220.0f / 255.0f * alpha), 0.0f);
        if (fillW > 0.5f) {
            // 填充完整渐变拉伸（accent → accent2 铺满填充长度；直角槽无需补圆）
            Color cl(c.accent.r, c.accent.g, c.accent.b, 220.0f / 255.0f * alpha);
            Color cr(c.accent2.r, c.accent2.g, c.accent2.b, 220.0f / 255.0f * alpha);
            drawHorizontalGradientRect(dev, Rect(gr.pos, Vec2(fillW, gh)), cl, cr);
        }
        Rect hr(Vec2(groove.pos.x + fillW - 8.0f * s,
                     groove.pos.y + groove.size.y * 0.5f - 8.0f * s),
                Vec2(16.0f * s, 16.0f * s));
        drawRoundedRect(dev, hr,
                        Color(c.accent2.r, c.accent2.g, c.accent2.b, alpha), khRad);
        drawRoundedBorder(dev, hr,
                          Color(c.accent.r, c.accent.g, c.accent.b, alpha),
                          1.0f * s, khRad);
        return;
    }
    if (g_uiCtx.sliderStyle == 5) {
        // 圆润：胶囊槽（圆角=高一半）+ 渐变填充 + 大圆角手柄 + accent 描边
        float gh = 8.0f * s;
        float rr = gh * 0.5f;
        Rect gr(Vec2(groove.pos.x, groove.pos.y + groove.size.y * 0.5f - gh * 0.5f),
                Vec2(groove.size.x, gh));
        drawRoundedRect(dev, gr,
                        Color(c.sliderGroove.r, c.sliderGroove.g, c.sliderGroove.b,
                              220.0f / 255.0f * alpha), rr);
        if (fillW > 0.5f) {
            Color cl(c.accent.r, c.accent.g, c.accent.b, 220.0f / 255.0f * alpha);
            Color cr(c.accent2.r, c.accent2.g, c.accent2.b, 220.0f / 255.0f * alpha);
            drawHorizontalGradientRect(dev, Rect(gr.pos, Vec2(fillW, gh)), cl, cr);
            if (fillW > gh) {
                fillCircle(dev, gr.pos.x + rr, gr.pos.y + rr, rr, cl);
                fillCircle(dev, gr.pos.x + fillW - rr, gr.pos.y + rr, rr, cr);
            }
        }
        float hs2 = 16.0f * s;
        Rect hr(Vec2(groove.pos.x + fillW - hs2 * 0.5f,
                     groove.pos.y + groove.size.y * 0.5f - hs2 * 0.5f),
                Vec2(hs2, hs2));
        drawRoundedRect(dev, hr,
                        Color(c.sliderHandle.r, c.sliderHandle.g, c.sliderHandle.b, alpha),
                        hs2 * 0.28f);
        drawRoundedBorder(dev, hr,
                          Color(c.accent.r, c.accent.g, c.accent.b, alpha),
                          1.0f * s, hs2 * 0.28f);
        return;
    }

    // 默认（sliderStyle==0）：槽 sliderGroove@200 胶囊 + 水平渐变填充 + 固定圆手柄
    drawRoundedRect(dev, groove,
                    Color(c.sliderGroove.r, c.sliderGroove.g, c.sliderGroove.b,
                          200.0f / 255.0f * alpha), rad);
    if (fillW > 0.5f) {
        // 填充完整渐变拉伸（accent → accent2 铺满填充长度，与主界面音量条一致）
        Color cl(c.accent.r, c.accent.g, c.accent.b, alpha);
        Color cr(c.accent2.r, c.accent2.g, c.accent2.b, alpha);
        drawHorizontalGradientRect(dev, Rect(groove.pos, Vec2(fillW, groove.size.y)),
                                   cl, cr);
        if (fillW > rad * 2.0f) {
            fillCircle(dev, groove.pos.x + rad, groove.pos.y + rad, rad, cl);
            fillCircle(dev, groove.pos.x + fillW - rad, groove.pos.y + rad, rad, cr);
        }
    }
    // 手柄：固定 14px 圆 sliderHandle（参考 L1108-1110）
    float hs = 14.0f * g_uiCtx.scale;
    fillCircle(dev, groove.pos.x + fillW, groove.pos.y + groove.size.y * 0.5f,
               hs * 0.5f,
               Color(c.sliderHandle.r, c.sliderHandle.g, c.sliderHandle.b, alpha));
}

void drawSliderValue(RenderDevice* dev, const Rect& groove, float t,
                     const char* valueText, bool hover, float alpha, float zoom) {
    // 对照参考 ImGui::SliderScalar 主绘制 1:1（主题色单源）：
    //   ① 整框 RenderFrame：FrameBg 底 + 边框（FrameRounding）
    //   ② 竖直抓块 grab：宽 = GrabMinSize(12)，高 = 框高 - 2×grab_padding(2)，
    //      位置 = lerp(左可用点, 右可用点, t)（可用点 = 边 + pad + 抓块宽/2）
    //   ③ 值文字 RenderTextClipped：整框居中（不随抓块移动）
    // 与参考一致：**无"左侧填充"**，抓块是一条窄竖条（此前画成宽块 = 错误）。
    ThemeColors& c = currentColors();
    float s = g_uiCtx.scale * zoom;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    float rad = c.inputRadius * s * g_uiCtx.radiusScale();

    // ① 整框（输入框质感底 + 边框）
    const Color& fc = hover ? c.bg3 : c.inputBg;
    drawRoundedRect(dev, groove,
                    Color(fc.r, fc.g, fc.b,
                          (hover ? 200.0f : 220.0f) / 255.0f * alpha), rad);
    Color bc = hover
        ? Color(c.accent.r, c.accent.g, c.accent.b, 180.0f / 255.0f * alpha)
        : Color(c.inputBorder.r, c.inputBorder.g, c.inputBorder.b,
                120.0f / 255.0f * alpha);
    drawRoundedBorder(dev, groove, bc, g_uiCtx.borderWidth() * zoom, rad);

    // ② 竖直抓块（参考：grab_padding=2，grab_sz=GrabMinSize=12，位置 lerp 可用区间）
    const float grabPad = 2.0f * s;
    const float grabMin = 12.0f * s;
    float sliderSz = groove.size.x - grabPad * 2.0f;
    if (sliderSz >= 1.0f) {
        float gsz = grabMin < sliderSz ? grabMin : sliderSz;    // min(GrabMinSize, slider_sz)
        float usableMin = groove.pos.x + grabPad + gsz * 0.5f;
        float usableMax = groove.pos.x + groove.size.x - grabPad - gsz * 0.5f;
        float gpos = usableMin + t * (usableMax - usableMin);
        Rect grab(Vec2(gpos - gsz * 0.5f, groove.pos.y + grabPad),
                  Vec2(gsz, groove.size.y - grabPad * 2.0f));
        // SliderGrab / SliderGrabActive：普通 = sliderHandle，hover = accent；
        // GrabRounding 默认 0 → 直角竖条
        Color gc = hover ? c.accent : c.sliderHandle;
        drawRoundedRect(dev, grab, Color(gc.r, gc.g, gc.b, alpha), 0.0f);
    }

    // ③ 值文字：整框中心（参考 RenderTextClipped 居中；不随抓块移动）
    if (valueText && valueText[0])
        drawTextCenterAt(dev, g_uiCtx.smallFont, valueText, groove,
                         Color(c.text.r, c.text.g, c.text.b, alpha));
}

void drawSliderValueBar(RenderDevice* dev, const Rect& groove, float t,
                        const char* valueText, bool hover, float alpha,
                        float zoom, int variant) {
    // 「中间带值」滑条族（内部 7..10 = 游戏ID 8..11）：
    // 共同点 = 值文字整框居中；差异 = 槽/填充/手柄几何与配色。
    // 全部主题色单源、零裸色。variant = 内部风格值 - 7，依次 0..3。
    // 原 10 个精简为 4 个（用户"就保留 值-霓虹/值-大圆/值-分段/值-圆柄"）→ 重排 0..3。
    ThemeColors& c = currentColors();
    float s = g_uiCtx.scale * zoom;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    if (variant < 0) variant = 0;
    if (variant > 3) variant = 3;
    float h     = groove.size.y;
    float cy    = groove.pos.y + h * 0.5f;
    float rad   = h * 0.5f;                 // 胶囊半径（自适应任意控件高）
    float fillW = groove.size.x * t;
    float bw    = g_uiCtx.borderWidth() * zoom;

    // 填充渐变端点（accent → accent2，与既有滑条公式一致）
    Color cl(c.accent.r, c.accent.g, c.accent.b, alpha);
    Color cr(c.accent2.r, c.accent2.g, c.accent2.b, alpha);
    // 轨道色（sliderGroove@200，与默认滑条一致）
    Color tk(c.sliderGroove.r, c.sliderGroove.g, c.sliderGroove.b, 200.0f / 255.0f * alpha);

    switch (variant) {
    case 0: { // 值-圆柄：胶囊槽 + 渐变填充 + 白色圆手柄（accent 外圈）
        drawRoundedRect(dev, groove, tk, rad);
        if (fillW > 0.5f)
            drawRoundedGradientRectH(dev, Rect(groove.pos, Vec2(fillW, h)), cl, cr, rad);
        float hr = h * 0.5f;
        if (hr > 10.0f * s) hr = 10.0f * s;
        float hx = groove.pos.x + fillW;                 // t=0 → 左端
        fillCircle(dev, hx, cy, hr + 1.5f * s, cl);      // accent 外圈
        fillCircle(dev, hx, cy, hr, Color(1, 1, 1, alpha));  // 白色内芯
        break; }

    case 1: { // 值-分段：胶囊槽 + 10 段色块（填充段按 index 从 accent 过渡到 accent2）
        drawRoundedRect(dev, groove, tk, rad);
        const int N = 10;
        float gap = 3.0f * s;
        float segW = (groove.size.x - gap * (float)(N - 1)) / (float)N;
        if (segW >= 1.0f) {
            int lit = (int)(t * (float)N + 0.5f);
            for (int i = 0; i < N; i++) {
                Rect seg(Vec2(groove.pos.x + (segW + gap) * (float)i,
                              groove.pos.y + 2.0f * s),
                         Vec2(segW, h - 4.0f * s));
                if (i < lit) {
                    float u = (N > 1) ? (float)i / (float)(N - 1) : 0.0f;
                    Color col(cl.r + (cr.r - cl.r) * u, cl.g + (cr.g - cl.g) * u,
                              cl.b + (cr.b - cl.b) * u, alpha);
                    drawRoundedRect(dev, seg, col, 2.0f * s);
                } else {
                    drawRoundedRect(dev, seg,
                                    Color(c.sliderGroove.r, c.sliderGroove.g,
                                          c.sliderGroove.b, 120.0f / 255.0f * alpha), 2.0f * s);
                }
            }
        }
        break; }

    case 2: { // 值-霓虹：暗底轨道 + accent 外发光(hover 增强) + 渐变填充 + accent 描边
        float gA = (hover ? 100.0f : 60.0f) / 255.0f * alpha;
        Rect glow(Vec2(groove.pos.x - 1.0f * s, groove.pos.y - 1.0f * s),
                  Vec2(groove.size.x + 2.0f * s, h + 2.0f * s));
        drawRoundedRect(dev, glow,
                        Color(c.accent.r, c.accent.g, c.accent.b, gA), rad + s);
        drawRoundedRect(dev, groove,
                        Color(c.bg1.r, c.bg1.g, c.bg1.b, 230.0f / 255.0f * alpha), rad);
        if (fillW > 0.5f)
            drawRoundedGradientRectH(dev, Rect(groove.pos, Vec2(fillW, h)), cl, cr, rad);
        drawRoundedBorder(dev, groove,
                          Color(c.accent.r, c.accent.g, c.accent.b, 150.0f / 255.0f * alpha), bw, rad);
        break; }

    case 3: { // 值-大圆：胶囊槽 + 渐变填充 + 大号 accent 圆手柄（带描边环）
        drawRoundedRect(dev, groove, tk, rad);
        if (fillW > 0.5f)
            drawRoundedGradientRectH(dev, Rect(groove.pos, Vec2(fillW, h)), cl, cr, rad);
        float hr = h * 0.5f + 4.0f * s;                  // 手柄略高于轨道（外露）
        if (hr > 13.0f * s) hr = 13.0f * s;
        float hx = groove.pos.x + fillW;
        fillCircle(dev, hx, cy, hr, cl);
        drawCircle(dev, hx, cy, hr, bw,
                   Color(c.sliderHandleBorder.r, c.sliderHandleBorder.g,
                         c.sliderHandleBorder.b, alpha));
        break; }
    }

    // 值文字：整框居中（全族共同点 —— 「中间带值」）
    if (valueText && valueText[0])
        drawTextCenterAt(dev, g_uiCtx.smallFont, valueText, groove,
                         Color(c.text.r, c.text.g, c.text.b, alpha));
}

// ============================================================
// 下拉按钮「特别绘制」族（内部 1..2 = 游戏ID 2..3）
// ============================================================
// 由 DropdownSystem::draw 在 g_uiCtx.dropdownStyle>=1 时统一分发到此。
// 主题色单源、零裸色；每 case 自绘「底 + 边框 + 预览文字 + 右侧向下三角」。
// 1=圆角胶囊 2=空心描边。
void drawComboBtnStyled(RenderDevice* dev, const Rect& r, const char* previewText,
                        bool hover, bool open, float alpha, Font* font, int style) {
    if (!dev || r.size.x <= 0.0f || r.size.y <= 0.0f) return;
    ThemeColors& c = currentColors();
    float s = g_uiCtx.scale;
    float bw = g_uiCtx.borderWidth();
    float h = r.size.y;
    float rad = c.inputRadius * s * g_uiCtx.radiusScale();
    Color ac (c.accent.r,  c.accent.g,  c.accent.b,  alpha);
    Color ib (c.inputBorder.r, c.inputBorder.g, c.inputBorder.b, alpha);
    Color ibg(c.inputBg.r, c.inputBg.g, c.inputBg.b, alpha);
    Color tx (c.text.r, c.text.g, c.text.b, 220.0f / 255.0f * alpha);   // 预览文字色

    Font* f = font ? font : g_uiCtx.smallFont;
    float arrowW = h;
    if (arrowW > r.size.x * 0.5f) arrowW = r.size.x * 0.5f;
    float acx = r.pos.x + r.size.x - arrowW * 0.5f;   // 箭头中心
    float acy = r.pos.y + h * 0.5f;
    float ahalf = arrowW * 0.24f;
    bool hov = open || hover;                         // 高亮（hover/展开）

    // 向下三角：半宽 ahalf×k，半高按上述比例
    auto tri = [&](const Color& col, float k) {
        fillTriangle(dev, acx - ahalf * k, acy - ahalf * 0.6f * k,
                     acx + ahalf * k, acy - ahalf * 0.6f * k,
                     acx, acy + ahalf * 0.8f * k, col);
    };

    switch (style) {
    case 1: { // 圆角胶囊：半径 = 高一半 + inputBg 底 + 边
        float pr = h * 0.5f;
        drawRoundedRect(dev, r, Color(ibg.r, ibg.g, ibg.b, 215.0f / 255.0f * alpha), pr);
        Color bcol = hov ? Color(ac.r, ac.g, ac.b, 190.0f / 255.0f * alpha)
                         : Color(ib.r, ib.g, ib.b, 150.0f / 255.0f * alpha);
        drawRoundedBorder(dev, r, bcol, bw, pr);
        tri(Color(c.text.r, c.text.g, c.text.b, 200.0f / 255.0f * alpha), 1.0f);
        break; }

    case 2: { // 空心描边：2px inputBorder 边 + 无底 + text 色
        Color bcol = hov ? Color(ac.r, ac.g, ac.b, 210.0f / 255.0f * alpha)
                         : Color(ib.r, ib.g, ib.b, 200.0f / 255.0f * alpha);
        drawRoundedBorder(dev, r, bcol, bw * 1.8f, rad);
        tri(Color(c.text.r, c.text.g, c.text.b, 210.0f / 255.0f * alpha), 1.0f);
        break; }

    default: // 兜底（等价细边）
        drawRoundedRect(dev, r, Color(ibg.r, ibg.g, ibg.b, 210.0f / 255.0f * alpha), rad);
        drawRoundedBorder(dev, r, Color(ib.r, ib.g, ib.b, 140.0f / 255.0f * alpha), bw, rad);
        tri(Color(c.text.r, c.text.g, c.text.b, 190.0f / 255.0f * alpha), 1.0f);
        break;
    }

    // 预览文字（左内边 8×scale，垂直居中）
    if (previewText && previewText[0])
        drawTextLeftAt(dev, f, previewText, Vec2(r.pos.x + 8.0f * s, r.pos.y), r.size.y, tx);
}

void drawCheck(RenderDevice* dev, const Rect& r, bool checked,
               bool hover, float alpha, int id) {
    // 全引擎唯一复选框绘制（对照参考 DrawCheckBox 1:1）：所有调用点统一走此函数
    // 选中 = checkCheckedBg + 边框 + 折线勾；未选 = checkBg@200 + checkBorder@140
    // hover = 边框 accent@180 高亮；勾显隐由 g_uiCtx.checkboxShowTick 全局单源控制
    ThemeColors& c = currentColors();
    float rad = c.checkRadius * g_uiCtx.scale * g_uiCtx.radiusScale();
    if (checked) {
        drawRoundedRect(dev, r,
                        Color(c.checkCheckedBg.r, c.checkCheckedBg.g,
                              c.checkCheckedBg.b, alpha), rad);
        Color bc = hover ?
            Color(c.accent.r, c.accent.g, c.accent.b, 180.0f / 255.0f * alpha) :
            Color(c.checkCheckedBorder.r, c.checkCheckedBorder.g,
                  c.checkCheckedBorder.b, alpha);
        drawRoundedBorder(dev, r, bc, g_uiCtx.borderWidth(), rad);
        // 勾：模板由「复选框」游戏ID 单源控制（drawCheck id → tpl::check / g_uiCtx.checkStyle）
        //   内部 0 = 系统默认（原折线勾，坐标见下，默认行为不变）
        //   内部 1 = 参考帧勾（对照参考 RenderCheckMark 精确几何，三段折线自适应）
        if (g_uiCtx.checkboxShowTick) {
            Color tick(c.btnText.r, c.btnText.g, c.btnText.b, alpha);
            int style = tpl::internalOf(id, g_uiCtx.checkStyle);
            if (style == 1) {
                // 参考帧勾：thickness = max(sz/5,1)；内缩 th*0.25；third = 内缩后边长/3
                float sz = r.size.x;
                float th = sz / 5.0f;
                if (th < 1.0f) th = 1.0f;
                float s2 = sz - th * 0.5f;
                float ox = r.pos.x + th * 0.25f;
                float oy = r.pos.y + th * 0.25f;
                float third = s2 / 3.0f;
                float bx = ox + third;
                float by = oy + s2 - third * 0.5f;
                drawLine(dev, bx - third, by - third, bx, by, th, tick);
                drawLine(dev, bx, by, bx + third * 2.0f, by - third * 2.0f, th, tick);
                fillCircle(dev, bx, by, th * 0.5f, tick);   // 折点补圆
            } else {
                // 系统默认（原折线勾）：参考折线坐标（固定比例，线宽 1.5f×scale btnText）
                float x = r.pos.x, y = r.pos.y, ss = r.size.x;
                float tw = 1.5f * g_uiCtx.scale;
                drawLine(dev, x + 3.0f * g_uiCtx.scale, y + ss * 0.5f,
                         x + ss * 0.35f, y + ss * 0.7f, tw, tick);
                drawLine(dev, x + ss * 0.35f, y + ss * 0.7f,
                         x + ss * 0.8f, y + ss * 0.25f, tw, tick);
            }
        }
    } else {
        drawRoundedRect(dev, r,
                        Color(c.checkBg.r, c.checkBg.g, c.checkBg.b,
                              200.0f / 255.0f * alpha), rad);
        Color bc = hover ?
            Color(c.accent.r, c.accent.g, c.accent.b, 180.0f / 255.0f * alpha) :
            Color(c.checkBorder.r, c.checkBorder.g, c.checkBorder.b,
                  140.0f / 255.0f * alpha);
        drawRoundedBorder(dev, r, bc, g_uiCtx.borderWidth(), rad);
    }
}

void drawSwitchExtra(RenderDevice* dev, const Rect& toggle, const Vec2& c,
                     float r, bool on, float alpha) {
    // 开关扩展图案（对照 py 脚本自绘开关的滑块装饰；颜色全部主题色派生，随主题/渐变联动）
    // 1=中心微光(轮回) 2=花朵(春天) 3=雪花(冬天) 4=齿轮(时间) 5=星月(塔罗牌) 6=宝石变色(王国)
    // 7=蝙蝠标记(吸血鬼) 8=星辉(星空) 9=太阳(潮汐) 10=拨杆(工业化) 11=猫爪(猫2) 12=镜面(液体金属)
    int ex = g_uiCtx.switchExtra;
    if (ex <= 0) return;
    float s = g_uiCtx.scale;
    ThemeColors& c0 = currentColors();
    // 主题色派生：accent/accent2 主次强调 + on/off 官方滑块色作图案底圆
    // （不再用纯白底圆，浅色主题下不冲突）；高光 = 底色提亮派生，深浅主题自适应
    Color ac(c0.accent.r, c0.accent.g, c0.accent.b, alpha);
    Color ac2(c0.accent2.r, c0.accent2.g, c0.accent2.b, alpha);
    Color tx(c0.btnText.r, c0.btnText.g, c0.btnText.b, alpha);
    Color sl((on ? c0.toggleOnSlider : c0.toggleOffSlider).r,
             (on ? c0.toggleOnSlider : c0.toggleOffSlider).g,
             (on ? c0.toggleOnSlider : c0.toggleOffSlider).b, alpha);
    Color hi = lerpColor(sl, Color(1, 1, 1, alpha), 0.55f);
    Color txh = lerpColor(tx, Color(1, 1, 1, alpha), 0.6f);
    Color bgc(c0.bg2.r, c0.bg2.g, c0.bg2.b, alpha);

    if (ex == 6) {
        // 宝石变色（王国.py）：底色覆盖（on=accent / off=bg2）+ 三层宝石
        Color offc(c0.bg2.r, c0.bg2.g, c0.bg2.b, alpha);
        drawRoundedRect(dev, toggle, on ? ac : offc, toggle.size.y * 0.5f);
        Color bc(c0.text.r, c0.text.g, c0.text.b, 0.8f * alpha);
        drawRoundedBorder(dev, toggle, bc, 2.0f * s, toggle.size.y * 0.5f);
        fillCircle(dev, c.x, c.y, r, ac2);
        fillCircle(dev, c.x - r * 0.25f, c.y - r * 0.25f, r * 0.62f, tx);
        fillCircle(dev, c.x - r * 0.3f, c.y - r * 0.3f, r * 0.28f, hi);
        return;
    }
    if (ex == 10) {
        // 拨杆（工业化.py）：金属方块 + 3 条横向纹理线（全主题色）
        Rect br(Vec2(c.x - r, c.y - r), Vec2(r * 2.0f, r * 2.0f));
        drawRoundedRect(dev, br, on ? ac2 : tx, 2.0f * s);
        drawRoundedBorder(dev, br, ac, 1.0f * s, 2.0f * s);
        for (int i = 0; i < 3; i++) {
            float ly = c.y + (i - 1) * r * 0.4f;
            drawLine(dev, c.x - r * 0.4f, ly, c.x + r * 0.4f, ly, 1.0f * s, bgc);
        }
        return;
    }

    // 圆形滑块图案（先画底圆再叠装饰）
    if (ex == 1) {
        // 中心微光（轮回.py）：accent2 外圈 + 提亮亮心
        fillCircle(dev, c.x, c.y, r, ac2);
        fillCircle(dev, c.x, c.y, r * 0.55f, hi);
    } else if (ex == 2) {
        // 花朵图案（春天.py）：滑块色底 + btnText 花蕊 + accent/accent2 花瓣
        fillCircle(dev, c.x, c.y, r, sl);
        fillCircle(dev, c.x, c.y, r * 0.12f, tx);
        Color pc = on ? ac : ac2;
        for (int a = 0; a < 360; a += 72) {
            float rad = a * 3.14159f / 180.0f;
            fillCircle(dev, c.x + r * 0.42f * cosf(rad), c.y + r * 0.42f * sinf(rad),
                       r * 0.26f, pc);
        }
    } else if (ex == 3) {
        // 雪花图案（冬天.py）：滑块色底 + 6 向辐条 accent2 线
        fillCircle(dev, c.x, c.y, r, sl);
        for (int a = 0; a < 360; a += 60) {
            float rad = a * 3.14159f / 180.0f;
            drawLine(dev, c.x, c.y, c.x + r * 0.75f * cosf(rad),
                     c.y + r * 0.75f * sinf(rad), 1.2f * s, ac2);
        }
    } else if (ex == 4) {
        // 齿轮纹路（时间.py）：accent 圆 + 8 齿辐线（btnText）
        fillCircle(dev, c.x, c.y, r, on ? ac : ac2);
        for (int a = 0; a < 360; a += 45) {
            float rad = a * 3.14159f / 180.0f;
            drawLine(dev, c.x + r * 0.45f * cosf(rad), c.y + r * 0.45f * sinf(rad),
                     c.x + r * 0.85f * cosf(rad), c.y + r * 0.85f * sinf(rad),
                     1.2f * s, tx);
        }
    } else if (ex == 5) {
        // 星月图案（塔罗牌.py）：滑块色底 + on=accent 月牙 / off=accent 五角星
        fillCircle(dev, c.x, c.y, r, sl);
        if (on) {
            fillCircle(dev, c.x, c.y, r * 0.55f, ac);
            fillCircle(dev, c.x + r * 0.2f, c.y - r * 0.12f, r * 0.5f, sl);
        } else {
            float R = r * 0.65f;
            for (int i = 0; i < 10; i++) {
                float a0 = i * 36.0f * 3.14159f / 180.0f - 1.5708f;
                float a1 = (i + 1) * 36.0f * 3.14159f / 180.0f - 1.5708f;
                float r0 = (i % 2 == 0) ? R : R * 0.4f;
                float r1 = ((i + 1) % 2 == 0) ? R : R * 0.4f;
                fillTriangle(dev, c.x, c.y,
                             c.x + r0 * cosf(a0), c.y + r0 * sinf(a0),
                             c.x + r1 * cosf(a1), c.y + r1 * sinf(a1), ac);
            }
        }
    } else if (ex == 7) {
        // 蝙蝠标记（吸血鬼.py）：滑块色底 + btnText 蝙蝠线形（6 段折线）
        fillCircle(dev, c.x, c.y, r, sl);
        float k = r / 12.0f;
        drawLine(dev, c.x - 2 * k, c.y - 1 * k, c.x - 4 * k, c.y - 3 * k, 1.0f * s, tx);
        drawLine(dev, c.x - 2 * k, c.y - 1 * k, c.x, c.y - 3 * k, 1.0f * s, tx);
        drawLine(dev, c.x + 2 * k, c.y - 1 * k, c.x + 4 * k, c.y - 3 * k, 1.0f * s, tx);
        drawLine(dev, c.x + 2 * k, c.y - 1 * k, c.x, c.y - 3 * k, 1.0f * s, tx);
        drawLine(dev, c.x - 1 * k, c.y + 2 * k, c.x - 3 * k, c.y + 4 * k, 1.0f * s, tx);
        drawLine(dev, c.x + 1 * k, c.y + 2 * k, c.x + 3 * k, c.y + 4 * k, 1.0f * s, tx);
    } else if (ex == 8) {
        // 星辉（星空.py）：accent2 光环 + 滑块色亮圆 + accent 内小星
        fillCircle(dev, c.x, c.y, r, ac2);
        fillCircle(dev, c.x, c.y, r * 0.75f, sl);
        float R = r * 0.45f;
        for (int i = 0; i < 10; i++) {
            float a0 = i * 36.0f * 3.14159f / 180.0f - 1.5708f;
            float a1 = (i + 1) * 36.0f * 3.14159f / 180.0f - 1.5708f;
            float r0 = (i % 2 == 0) ? R : R * 0.4f;
            float r1 = ((i + 1) % 2 == 0) ? R : R * 0.4f;
            fillTriangle(dev, c.x, c.y,
                         c.x + r0 * cosf(a0), c.y + r0 * sinf(a0),
                         c.x + r1 * cosf(a1), c.y + r1 * sinf(a1), ac);
        }
    } else if (ex == 9) {
        // 太阳（潮汐.py）：accent 光晕 + accent2 本体 + 提亮心 + 8 向光芒线
        fillCircle(dev, c.x, c.y, r, ac);
        fillCircle(dev, c.x, c.y, r * 0.55f, ac2);
        fillCircle(dev, c.x, c.y, r * 0.25f, hi);
        for (int a = 0; a < 360; a += 45) {
            float rad = a * 3.14159f / 180.0f;
            drawLine(dev, c.x + r * 0.65f * cosf(rad), c.y + r * 0.65f * sinf(rad),
                     c.x + r * 0.95f * cosf(rad), c.y + r * 0.95f * sinf(rad),
                     1.2f * s, ac2);
        }
    } else if (ex == 11) {
        // 猫爪（猫2.py）：滑块色底 + accent 掌垫 + accent2 四趾垫
        // 部件占比放大（掌垫 0.5r/0.4r、趾垫 0.21r、趾距 0.58r），小滑块上也清晰不糊
        fillCircle(dev, c.x, c.y, r, sl);
        float px = c.x, py = c.y + r * 0.18f;
        fillEllipse(dev, px, py, r * 0.5f, r * 0.4f, ac);
        for (int i = 0; i < 4; i++) {
            float ang = (-50.0f + i * 33.33f) * 3.14159f / 180.0f;
            fillCircle(dev, px + r * 0.58f * sinf(ang),
                       py - r * 0.42f * cosf(ang), r * 0.21f, ac2);
        }
    } else if (ex == 12) {
        // 镜面（液体金属.py）：三层同心圆（提亮高光心 + tx 中层 + accent2 外层）
        fillCircle(dev, c.x, c.y, r, ac2);
        fillCircle(dev, c.x, c.y, r * 0.7f, tx);
        fillCircle(dev, c.x - r * 0.2f, c.y - r * 0.2f, r * 0.35f, txh);
    }
}

void drawToggleOn(RenderDevice* dev, const Rect& r, float alpha) {
    // 悬浮窗 dock 简化版（参考 L1446-1452）：胶囊 toggleOnBg@220 + 右侧圆滑块
    // 方形（switchStyle==1）直角+方块；半圆润（==2）小圆角+圆角方块（对照使命.py）
    ThemeColors& c = currentColors();
    float th = r.size.y;
    float ss = th - 4.0f * g_uiCtx.scale;  // 滑块边长 = 高 - 4
    float sx = r.pos.x + r.size.x - ss - 2.0f * g_uiCtx.scale;
    float sy = r.pos.y + 2.0f * g_uiCtx.scale;
    int ex = g_uiCtx.switchExtra;
    if (ex == 6) {
        // 宝石变色：底+滑块全由扩展绘制
        float fs = th - 8.0f * g_uiCtx.scale;
        drawSwitchExtra(dev, r,
                        Vec2(r.pos.x + r.size.x - fs * 0.5f - 4.0f * g_uiCtx.scale,
                             r.pos.y + th * 0.5f),
                        fs * 0.5f, true, alpha);
        return;
    }
    if (g_uiCtx.switchStyle > 2) {   // 开关「特别绘制」族（内部 3..4 = 游戏ID 4..5：机械/渐变）
        drawToggleStyled(dev, r, true, alpha, 1.0f, g_uiCtx.switchStyle);
        return;
    }
    if (g_uiCtx.switchStyle == 1) {
        // 方形滑块再缩 2px（每侧边距 3px），视觉更轻盈
        float fs = th - 6.0f * g_uiCtx.scale;
        float fx = r.pos.x + r.size.x - fs - 3.0f * g_uiCtx.scale;
        float fy = r.pos.y + 3.0f * g_uiCtx.scale;
        drawRect(dev, r,
                 Color(c.toggleOnBg.r, c.toggleOnBg.g, c.toggleOnBg.b,
                       220.0f / 255.0f * alpha));
        if (ex > 0) {
            drawSwitchExtra(dev, r, Vec2(fx + fs * 0.5f, fy + fs * 0.5f), fs * 0.5f,
                            true, alpha);
        } else {
            drawRect(dev, Rect(Vec2(fx, fy), Vec2(fs, fs)),
                     Color(c.toggleOnSlider.r, c.toggleOnSlider.g, c.toggleOnSlider.b, alpha));
        }
    } else if (g_uiCtx.switchStyle == 2) {
        // 半圆润（对照使命.py toggleSwitch：小圆角底 10/32h + 圆角方块滑块，每侧边距 4）
        float rad = th * (10.0f / 32.0f);
        float fs = th - 8.0f * g_uiCtx.scale;
        float fx = r.pos.x + r.size.x - fs - 4.0f * g_uiCtx.scale;
        float fy = r.pos.y + 4.0f * g_uiCtx.scale;
        drawRoundedRect(dev, r,
                        Color(c.toggleOnBg.r, c.toggleOnBg.g, c.toggleOnBg.b,
                              220.0f / 255.0f * alpha), rad);
        if (ex > 0) {
            drawSwitchExtra(dev, r, Vec2(fx + fs * 0.5f, fy + fs * 0.5f), fs * 0.5f,
                            true, alpha);
        } else {
            drawRoundedRect(dev, Rect(Vec2(fx, fy), Vec2(fs, fs)),
                            Color(c.toggleOnSlider.r, c.toggleOnSlider.g, c.toggleOnSlider.b, alpha),
                            fs / 3.0f);
        }
    } else {
        drawRoundedRect(dev, r,
                        Color(c.toggleOnBg.r, c.toggleOnBg.g, c.toggleOnBg.b,
                              220.0f / 255.0f * alpha), th * 0.5f);
        if (ex > 0) {
            drawSwitchExtra(dev, r, Vec2(sx + ss * 0.5f, sy + ss * 0.5f), ss * 0.5f,
                            true, alpha);
        } else {
            fillCircle(dev, sx + ss * 0.5f, sy + ss * 0.5f, ss * 0.5f,
                       Color(c.toggleOnSlider.r, c.toggleOnSlider.g, c.toggleOnSlider.b, alpha));
        }
    }
}

void drawToggleStyled(RenderDevice* dev, const Rect& r, bool on, float alpha,
                      float zoom, int style) {
    // ====== 开关「特别绘制」族（内部 3..4 = 游戏ID 4..5：机械/渐变）======
    // 全部主题色单源、零裸色；on/off 双态都支持；
    // 由 wctl::drawToggleOn / pctl::drawToggle 在 switchStyle>2 时统一分发到此。
    ThemeColors& c = currentColors();
    float s  = g_uiCtx.scale * zoom;
    float th = r.size.y;
    float bw = g_uiCtx.borderWidth() * zoom;

    // 手柄（默认：直径 = 高-4、贴两端 2；on=右 off=左）
    float kd = th - 4.0f * s; if (kd < 2.0f * s) kd = 2.0f * s;
    float kx = on ? (r.pos.x + r.size.x - kd - 2.0f * s) : (r.pos.x + 2.0f * s);
    float ky = r.pos.y + 2.0f * s;
    float kcx = kx + kd * 0.5f, kcy = ky + kd * 0.5f;

    Color onSl(c.toggleOnSlider.r, c.toggleOnSlider.g, c.toggleOnSlider.b, alpha);
    Color offSl(c.toggleOffSlider.r, c.toggleOffSlider.g, c.toggleOffSlider.b, alpha);
    Color ac(c.accent.r, c.accent.g, c.accent.b, alpha);
    Color ac2(c.accent2.r, c.accent2.g, c.accent2.b, alpha);
    Color ib(c.inputBorder.r, c.inputBorder.g, c.inputBorder.b, alpha);
    Color gb(c.groupBorder.r, c.groupBorder.g, c.groupBorder.b, alpha);
    Color bg1(c.bg1.r, c.bg1.g, c.bg1.b, alpha);
    Color bg4(c.bg4.r, c.bg4.g, c.bg4.b, alpha);
    Color knob  = on ? onSl : offSl;

    switch (style) {
    case 3: { // 机械：金属槽（3 条横向纹理）+ 斜面方柄
        drawRoundedRect(dev, r, bg4, 3.0f * s);
        for (int i = 0; i < 3; i++) {
            float ly = r.pos.y + th * (0.25f + 0.25f * (float)i);
            drawLine(dev, r.pos.x + 4.0f * s, ly, r.pos.x + r.size.x - 4.0f * s, ly, 1.0f * s, gb);
        }
        drawRoundedBorder(dev, r, on ? ac : ib, bw, 3.0f * s);
        Rect kr(Vec2(kx, ky), Vec2(kd, kd));
        drawRoundedRect(dev, kr, knob, 2.0f * s);
        drawRoundedBorder(dev, kr, ac, bw, 2.0f * s);
        break; }

    case 4: { // 渐变：整条 accent→accent2 渐变槽 + 高对比实心柄
        drawRoundedGradientRectH(dev, r, ac, ac2, th * 0.5f);
        fillCircle(dev, kcx, kcy, kd * 0.5f, on ? bg1 : Color(1, 1, 1, alpha));
        break; }

    default:
        fillCircle(dev, kcx, kcy, kd * 0.5f, knob);
        break;
    }

    // 扩展图案叠加（switchExtra>0 且 ≠6；6=宝石变色为整体替换，已在上游处理）
    int ex = g_uiCtx.switchExtra;
    if (ex > 0 && ex != 6)
        drawSwitchExtra(dev, r, Vec2(kcx, kcy), kd * 0.5f, on, alpha);
}

// ============================================================
// 悬浮球「特别绘制」族（内部 2..3 = 游戏ID 3..4）
// ============================================================
// 由 LayerStack::renderBall 在 g_uiCtx.ballStyle>1 时统一分发到此。
// 主题色单源、零裸色；各 case 自绘阴影与高光；白色标签由 renderBall 统一叠加。
// 2=玻璃 3=渐变（本体风格）。
void drawBallStyled(RenderDevice* dev, float cx, float cy, float r, float alpha, int style) {
    if (!dev || r <= 0.0f) return;
    ThemeColors& c = currentColors();
    float bw = g_uiCtx.borderWidth();
    Color ac(c.accent.r, c.accent.g, c.accent.b, alpha);
    Color ac2(c.accent2.r, c.accent2.g, c.accent2.b, alpha);
    Color bg3(c.bg3.r, c.bg3.g, c.bg3.b, alpha);
    Color ib(c.inputBorder.r, c.inputBorder.g, c.inputBorder.b, alpha);
    Color shadow(0, 0, 0, 50.0f / 255.0f * alpha);
    Rect br(Vec2(cx - r, cy - r), Vec2(r * 2.0f, r * 2.0f));

    switch (style) {
    case 2: { // 玻璃：半透明底 + 顶部高光 + 描边 + 高光点
        fillEllipse(dev, cx + 2.0f, cy + 4.0f, r, r, shadow);
        fillCircle(dev, cx, cy, r, Color(bg3.r, bg3.g, bg3.b, 130.0f / 255.0f * alpha));
        fillEllipse(dev, cx, cy - 0.28f * r, r * 0.68f, r * 0.42f,
                    Color(1, 1, 1, 45.0f / 255.0f * alpha));
        fillCircle(dev, cx - 0.36f * r, cy - 0.36f * r, r * 0.12f,
                   Color(1, 1, 1, 170.0f / 255.0f * alpha));
        drawCircle(dev, cx, cy, r, bw, Color(ib.r, ib.g, ib.b, 160.0f / 255.0f * alpha));
        break; }

    case 3: { // 渐变：横向 accent→accent2 渐变圆（圆角矩形 radius=r）+ 描边
        fillEllipse(dev, cx + 2.0f, cy + 4.0f, r, r, shadow);
        drawRoundedGradientRectH(dev, br, ac, ac2, r);
        drawCircle(dev, cx, cy, r, bw, Color(ac.r, ac.g, ac.b, 200.0f / 255.0f * alpha));
        break; }

    default:
        fillCircle(dev, cx, cy, r, ac);
        break;
    }
}

float sliderValueAt(const Rect& groove, float x) {
    if (groove.size.x <= 0) return 0;
    float t = (x - groove.pos.x) / groove.size.x;
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    return t;
}

// ====== 系统文本自适应（字体行高超出可用高 → 自动等比缩小；全引擎单源）======
// 需求：控件里的文字"超过底"（行高 > 可用高，如字体偏大撑出按钮）时，
// 系统自动把字体缩小到刚好放下，并在终端报告"哪里字体超了、系统已暴力兜底"。
// 开关：g_uiCtx.textAutoFit（功能）/ g_uiCtx.textAutoFitLog（报不报）。
// 返回"适配后的绘制缩放"：未超界 = baseDrawScale 原样；超界 = 缩小后的缩放。
// 调用方用它同时缩放 measure/行高 → 垂直居中仍准确、不偏移。
float uiTextFit(Font* f, const char* utf8, float baseDrawScale, float availH) {
    if (!f || !utf8 || !g_uiCtx.textAutoFit || availH <= 0.0f) return baseDrawScale;
    float th = f->scaledLineHeight() * baseDrawScale;   // 该文本当前行高（含绘制缩放）
    if (th <= availH) return baseDrawScale;             // 放得下 → 不缩放
    float s = availH / th;                              // 需要的缩小比例（<1）
    if (s < 0.2f) s = 0.2f;                             // 保底：不缩到看不清
    if (g_uiCtx.textAutoFitLog) {
        // 终端报告"哪里字体超了、系统自动暴力处理了"。同文本只报一次，防每帧刷屏。
        static std::unordered_set<std::string> seen;
        if (seen.size() < 256) {
            std::string key(utf8);
            if (seen.insert(key).second)
                UI_LOG_WARN("text overflow: \"%s\" needH=%.0f > boxH=%.0f -> auto-shrink x%.2f (系统暴力兜底)",
                            utf8, th, availH, s);
        }
    }
    return baseDrawScale * s;
}

void drawTextLeftAt(RenderDevice* dev, Font* f, const char* utf8,
                    const Vec2& pos, float h, const Color& col) {
    if (!f || !f->isReady() || !utf8) return;
    float sc = uiTextFit(f, utf8, kWinFontScale, h);
    float th = f->scaledLineHeight() * sc;
    Vec2 p = {pos.x, pos.y + (h - th) * 0.5f};
    drawText(dev, f, utf8, p, col, sc);
}

void drawTextCenterAt(RenderDevice* dev, Font* f, const char* utf8,
                      const Rect& r, const Color& col) {
    if (!f || !f->isReady() || !utf8) return;
    float sc = uiTextFit(f, utf8, kWinFontScale, r.size.y);
    float tw = f->measureText(utf8) * sc;
    float th = f->scaledLineHeight() * sc;
    Vec2 p = {r.pos.x + (r.size.x - tw) * 0.5f, r.pos.y + (r.size.y - th) * 0.5f};
    drawText(dev, f, utf8, p, col, sc);
}

} // namespace wctl
} // namespace ui