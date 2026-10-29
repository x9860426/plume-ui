// AI生成
// ============================================================
// 主界面页面共享控件库实现（画法 1:1 迁移自原 MainUI.cpp，零视觉变更）
// 对照行号均指重写前的 MainUI.cpp
// ============================================================
#include "PageControls.h"
#include "../Theme.h"
#include "../win/UiContext.h"
#include "../win/WinControls.h"
#include <cmath>
#include <cstdio>

namespace ui {

namespace pctl {

// ---------------- 文本排版（原 MainUI L169-190 迁出） ----------------

void drawTextCentered(RenderDevice* dev, Font* f, const char* utf8,
                      const Rect& r, const Color& c) {
    if (!f || !f->isReady() || !utf8) return;
    float sc = wctl::uiTextFit(f, utf8, 1.0f, r.size.y);   // 系统兜底：超界自动缩小
    float tw = f->measureText(utf8) * sc;
    float th = f->scaledLineHeight() * sc;
    Vec2 pos = {r.pos.x + (r.size.x - tw) * 0.5f, r.pos.y + (r.size.y - th) * 0.5f};
    drawText(dev, f, utf8, pos, c, sc);
}

void drawTextLeft(RenderDevice* dev, Font* f, const char* utf8,
                  const Rect& r, const Color& c) {
    if (!f || !f->isReady() || !utf8) return;
    float sc = wctl::uiTextFit(f, utf8, 1.0f, r.size.y);   // 系统兜底：超界自动缩小
    float th = f->scaledLineHeight() * sc;
    Vec2 pos = {r.pos.x, r.pos.y + (r.size.y - th) * 0.5f};
    drawText(dev, f, utf8, pos, c, sc);
}

void drawTextRight(RenderDevice* dev, Font* f, const char* utf8,
                   const Rect& r, const Color& c) {
    if (!f || !f->isReady() || !utf8) return;
    float sc = wctl::uiTextFit(f, utf8, 1.0f, r.size.y);   // 系统兜底：超界自动缩小
    float tw = f->measureText(utf8) * sc;
    float th = f->scaledLineHeight() * sc;
    Vec2 pos = {r.pos.x + r.size.x - tw, r.pos.y + (r.size.y - th) * 0.5f};
    drawText(dev, f, utf8, pos, c, sc);
}

// UTF-8 编码追加已并入 TextInputSystem（输入框系统单源实现，本库不再提供）

// ---------------- tab 项（原 drawTabBar L203-226） ----------------

void drawTabItem(RenderDevice* dev, const Rect& r, const char* utf8,
                 int state, Font* f) {
    ThemeColors& c = currentColors();
    float s = g_uiCtx.scale;
    float topRadius = 10.0f * s * g_uiCtx.radiusScale();
    float bw = g_uiCtx.borderWidth();

    if (state == STATE_ACTIVE) {
        // 选中：顶部圆角渐变底 + accent 边框 + 高亮文字
        if (c.antialias) {
            drawTopRoundedGradientRect(dev, r, c.tabSelBg, c.accent2, topRadius);
        } else {
            drawTopRoundedRect(dev, r, c.tabSelBg, topRadius);
        }
        drawTopRoundedBorder(dev, r, c.accent, bw, topRadius);
        drawTextCentered(dev, f, utf8, r, c.tabSelText);
    } else if (state == STATE_HOVER) {
        // 悬停：实底提亮 + 分组边框
        Color hoverBg = Color(c.tabBg.r, c.tabBg.g, c.tabBg.b, 240.0f / 255.0f);
        drawTopRoundedRect(dev, r, hoverBg, topRadius);
        drawTopRoundedBorder(dev, r,
            Color(c.groupBorder.r, c.groupBorder.g, c.groupBorder.b, 160.0f / 255.0f),
            bw, topRadius);
        drawTextCentered(dev, f, utf8, r, c.tabText);
    } else {
        // 普通：半透明底 + 淡边框 + 淡文字
        Color bgAlpha = Color(c.tabBg.r, c.tabBg.g, c.tabBg.b, 180.0f / 255.0f);
        drawTopRoundedRect(dev, r, bgAlpha, topRadius);
        drawTopRoundedBorder(dev, r,
            Color(c.groupBorder.r, c.groupBorder.g, c.groupBorder.b, 80.0f / 255.0f),
            bw, topRadius);
        Color textAlpha = Color(c.tabText.r, c.tabText.g, c.tabText.b, 160.0f / 255.0f);
        drawTextCentered(dev, f, utf8, r, textAlpha);
    }
}

// ---------------- 侧栏项（原 drawSidePanel L244-262） ----------------

void drawSideItem(RenderDevice* dev, const Rect& r, const char* utf8,
                  int state, Font* f) {
    ThemeColors& c = currentColors();
    // 胶囊圆角 = 高度一半 × 圆润系数
    float pillRadius = r.size.y * 0.5f * g_uiCtx.radiusScale();

    if (state == STATE_ACTIVE) {
        // 选中：accent→accent2 渐变胶囊
        if (c.antialias) {
            drawRoundedGradientRect(dev, r, c.accent, c.accent2, pillRadius);
        } else {
            drawRoundedRect(dev, r, c.accent, pillRadius);
        }
        drawTextCentered(dev, f, utf8, r, c.btnText);
    } else if (state == STATE_HOVER) {
        // 悬停：菜单选中色半透明底
        Color hoverBg = Color(c.menuSelBg.r, c.menuSelBg.g, c.menuSelBg.b, 160.0f / 255.0f);
        drawRoundedRect(dev, r, hoverBg, pillRadius);
        drawTextCentered(dev, f, utf8, r, c.text);
    } else {
        Color textAlpha = Color(c.text.r, c.text.g, c.text.b, 160.0f / 255.0f);
        drawTextCentered(dev, f, utf8, r, textAlpha);
    }
}

// ---------------- 分段按钮项（原 drawSegmented L447-460） ----------------

void drawSegItem(RenderDevice* dev, const Rect& r, const char* utf8,
                 int state, Font* f) {
    ThemeColors& c = currentColors();

    if (state == STATE_ACTIVE) {
        drawRoundedGradientRect(dev, r, c.accent, c.accent2, r.size.y * 0.5f);
        drawTextCentered(dev, f, utf8, r, c.btnText);
    } else if (state == STATE_HOVER) {
        Color hoverBg = Color(c.menuSelBg.r, c.menuSelBg.g, c.menuSelBg.b, 130.0f / 255.0f);
        drawRoundedRect(dev, r, hoverBg, r.size.y * 0.5f);
        drawTextCentered(dev, f, utf8, r, c.text);
    } else {
        Color textAlpha = Color(c.text.r, c.text.g, c.text.b, 160.0f / 255.0f);
        drawTextCentered(dev, f, utf8, r, textAlpha);
    }
}

// ---------------- 输入框底（原 L295-306 边框三态） ----------------

void drawInputBox(RenderDevice* dev, const Rect& r, int state) {
    ThemeColors& c = currentColors();
    float s = g_uiCtx.scale;
    float inputRad = c.inputRadius * s * g_uiCtx.radiusScale();
    Color bgAlpha = Color(c.inputBg.r, c.inputBg.g, c.inputBg.b, 240.0f / 255.0f);
    drawRoundedRect(dev, r, bgAlpha, inputRad);

    Color borderColor;
    float bw = g_uiCtx.borderWidth();
    if (state == STATE_ACTIVE) {
        borderColor = c.accent;
        bw *= 1.6f;                       // 聚焦边框加粗（对照原 borderWidth(true)）
    } else if (state == STATE_HOVER) {
        borderColor = Color(c.inputBorder.r, c.inputBorder.g, c.inputBorder.b, 210.0f / 255.0f);
    } else {
        borderColor = Color(c.inputBorder.r, c.inputBorder.g, c.inputBorder.b, 120.0f / 255.0f);
    }
    drawRoundedBorder(dev, r, borderColor, bw, inputRad);
}


// ---------------- 步进器（原 drawStepper L393-429） ----------------

void drawStepper(RenderDevice* dev, const Rect& minus, const Rect& val,
                 const Rect& plus, const char* valText,
                 bool hoverMinus, bool hoverPlus, Font* f, Font* bigF,
                 bool valFocused, bool valCaretOn, int valCaretByte, bool valHover) {
    ThemeColors& c = currentColors();
    float s = g_uiCtx.scale;
    float rad = 8.0f * s * g_uiCtx.radiusScale();
    float bw = g_uiCtx.borderWidth();

    // 左圆角减号 / 平边数值 / 右圆角加号 三段拼合
    Color btnBg = Color(c.inputBg.r, c.inputBg.g, c.inputBg.b, 230.0f / 255.0f);
    drawLeftRoundedRect(dev, minus, btnBg, rad);
    drawRect(dev, val, Color(c.inputBg.r, c.inputBg.g, c.inputBg.b, 248.0f / 255.0f));
    drawRightRoundedRect(dev, plus, btnBg, rad);

    // 分隔线（数值区两侧）
    Color dividerColor = Color(c.inputBorder.r, c.inputBorder.g, c.inputBorder.b, 90.0f / 255.0f);
    drawRect(dev, {Vec2(minus.pos.x + minus.size.x, minus.pos.y + 6.0f * s),
                   Vec2(1.0f, minus.size.y - 12.0f * s)}, dividerColor);
    drawRect(dev, {Vec2(plus.pos.x, plus.pos.y + 6.0f * s),
                   Vec2(1.0f, plus.size.y - 12.0f * s)}, dividerColor);

    // 整组外框
    Rect groupRect = Rect(minus.pos, Vec2(plus.pos.x + plus.size.x - minus.pos.x, minus.size.y));
    Color groupBorder = Color(c.inputBorder.r, c.inputBorder.g, c.inputBorder.b, 140.0f / 255.0f);
    drawRoundedBorder(dev, groupRect, groupBorder, bw, rad);
    // 数值区焦点/悬停高亮（输入框反馈；系统级——任何步进器数值区均可编辑）
    if (valFocused || valHover) {
        Color hb = Color(c.accent.r, c.accent.g, c.accent.b,
                         (valFocused ? 200.0f : 120.0f) / 255.0f);
        drawRoundedBorder(dev, val, hb, valFocused ? 1.6f * bw : bw, 2.0f * s);
    }

    // ±号（hover 高亮 accent）
    drawTextCentered(dev, bigF, "-", minus, hoverMinus ? c.accent : c.text);
    drawTextCentered(dev, bigF, "+", plus, hoverPlus ? c.accent : c.text);

    // 数值区：居中数值（保持原外观）+ 焦点光标（与居中文本同基线；缩放同源 uiTextFit）
    const char* vs = valText ? valText : "";
    drawTextCentered(dev, f, vs, val, c.inputText);
    if (valFocused && valCaretOn && f && f->isReady()) {
        float sc = wctl::uiTextFit(f, vs, 1.0f, val.size.y);   // 与 drawTextCentered 完全同缩放
        float tw = f->measureText(vs) * sc;
        int cb = valCaretByte; if (cb < 0 || cb > 120) cb = 120;
        char pre[128];
        snprintf(pre, sizeof(pre), "%.*s", cb, vs);            // 光标前缀（至多 120 字节）
        float cx = val.pos.x + (val.size.x - tw) * 0.5f + f->measureText(pre) * sc;
        float chh = val.size.y * 0.55f;
        drawRect(dev, Rect(Vec2(cx, val.pos.y + (val.size.y - chh) * 0.5f),
                           Vec2(1.5f * s, chh)), c.inputText);
    }
}

// ---------------- 聚焦光晕（原 L492-495） ----------------

void drawFocusGlow(RenderDevice* dev, const Rect& r,
                   float glowA, float ringA, float ringW, float radius) {
    ThemeColors& c = currentColors();
    float s = g_uiCtx.scale;
    // 外扩 4px 光晕底
    Rect glowRect = Rect(Vec2(r.pos.x - 4.0f * s, r.pos.y - 4.0f * s),
                         Vec2(r.size.x + 8.0f * s, r.size.y + 8.0f * s));
    drawRoundedRect(dev, glowRect,
                    Color(c.accent.r, c.accent.g, c.accent.b, glowA), radius + 4.0f * s);
    // 外扩 3px 描边环
    Rect ringRect = Rect(Vec2(r.pos.x - 3.0f * s, r.pos.y - 3.0f * s),
                         Vec2(r.size.x + 6.0f * s, r.size.y + 6.0f * s));
    drawRoundedBorder(dev, ringRect,
                      Color(c.accent.r, c.accent.g, c.accent.b, ringA), ringW, radius + 3.0f * s);
}

// ---------------- 渐变弧（原 static L554-571） ----------------

void drawGradientArc(RenderDevice* dev, float cx, float cy, float radius,
                     float penW, float startAngleDeg, float sweepDeg,
                     const Color& c1, const Color& c2) {
    if (!dev || sweepDeg <= 0.01f) return;
    // 分段绘制（每段约 3°）+ 段间轻微重叠（0.5°），颜色沿角度 c1→c2 插值。
    // 段更细且无缝衔接 → 渐变过渡自然，无带状断层（此前 6° 分段偏粗、且相邻段无重叠）。
    int seg = (int)(sweepDeg / 3.0f) + 1;
    if (seg < 2) seg = 2;
    float step = sweepDeg / seg;
    for (int i = 0; i < seg; i++) {
        float t = (i + 0.5f) / seg;
        Color col(c1.r + (c2.r - c1.r) * t, c1.g + (c2.g - c1.g) * t,
                  c1.b + (c2.b - c1.b) * t, c1.a + (c2.a - c1.a) * t);
        float sw = step + (i < seg - 1 ? 0.5f : 0.0f);   // 仅段间重叠，末段不越界
        drawArc(dev, cx, cy, radius, penW, startAngleDeg + step * i, sw, col, false);
    }
    float r0 = startAngleDeg * 3.14159f / 180.0f;
    float r1 = (startAngleDeg + sweepDeg) * 3.14159f / 180.0f;
    fillCircle(dev, cx + cosf(r0) * radius, cy + sinf(r0) * radius, penW * 0.5f, c1);
    fillCircle(dev, cx + cosf(r1) * radius, cy + sinf(r1) * radius, penW * 0.5f, c2);
}

// ---------------- 环形进度（原 drawCircularProgress L573-613） ----------------

void drawCircularProgress(RenderDevice* dev, const Rect& r, bool loading,
                          float animTime, Font* f, Font* bigF, Font* smallF,
                          const char* hint) {
    ThemeColors& c = currentColors();
    float s = g_uiCtx.scale;
    float cx = r.pos.x + r.size.x * 0.5f;
    float cy = r.pos.y + r.size.y * 0.5f;
    float radius = r.size.x * 0.5f - 6.0f * s;
    float penW = 9.0f * s;

    // 底轨整圈
    Color trackColor = Color(c.inputBorder.r, c.inputBorder.g, c.inputBorder.b, 60.0f / 255.0f);
    drawArc(dev, cx, cy, radius, penW, 0.0f, 360.0f, trackColor, false);

    // 模板联动（g_uiCtx.ringStyle 单源）：1=加载环模板 → 恒显加载态；
    // 0=进度环模板 → 由 loading 参数（页面点击切换）决定。使环形下拉成为真开关，而非空控件。
    bool load = loading || (g_uiCtx.ringStyle == 1);
    if (load) {
        // 加载态：60° 弧随时间旋转（1.2s/圈）
        float sweep = 60.0f;
        float startAngle = fmodf(animTime / 1.2f, 1.0f) * 360.0f;
        if (g_uiCtx.circGrad)
            drawGradientArc(dev, cx, cy, radius, penW, startAngle, sweep, c.accent, c.accent2);
        else
            drawArc(dev, cx, cy, radius, penW, startAngle, sweep, c.accent, true);
        drawTextCentered(dev, bigF, u8"…", r, c.accent);
    } else {
        // 进度态：0→100% 循环填充（3s/圈，12 点钟方向起）
        float progress = fmodf(animTime / 3.0f, 1.0f) * 100.0f;
        float sweep = progress / 100.0f * 360.0f;
        if (g_uiCtx.circGrad)
            drawGradientArc(dev, cx, cy, radius, penW, -90.0f, sweep, c.accent, c.accent2);
        else
            drawArc(dev, cx, cy, radius, penW, -90.0f, sweep, c.accent, true);
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", (int)progress);
        drawTextCentered(dev, f, buf, r, c.accent);
    }

    // 环下方小字提示（可省）
    if (hint && hint[0]) {
        drawTextCentered(dev, smallF, hint,
                         Rect(Vec2(r.pos.x - 10.0f * s, r.pos.y + r.size.y + 2.0f * s),
                              Vec2(r.size.x + 20.0f * s, 16.0f * s)),
                         Color(c.text.r, c.text.g, c.text.b, 130.0f / 255.0f));
    }
}

// ---------------- 条形进度（原 drawProgressBar L615-639） ----------------

void drawProgressBar(RenderDevice* dev, const Rect& r, float t,
                     const char* label, Font* smallF) {
    ThemeColors& c = currentColors();
    float s = g_uiCtx.scale;
    float progress = t * 100.0f;

    // 左上标签 + 右上百分比
    drawTextLeft(dev, smallF, label,
                 Rect(Vec2(r.pos.x, r.pos.y - 20.0f * s), Vec2(120.0f * s, 20.0f * s)),
                 Color(c.text.r, c.text.g, c.text.b, 180.0f / 255.0f));
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", (int)progress);
    drawTextRight(dev, smallF, buf,
                  Rect(Vec2(r.pos.x + r.size.x - 60.0f * s, r.pos.y - 20.0f * s),
                       Vec2(60.0f * s, 20.0f * s)), c.accent);

    // 轨道+填充统一走 wctl::drawProgress 共享原语（circGrad 开关联动）
    wctl::drawProgress(dev, r, t);
}

// ---------------- 三风格开关（原 drawToggleSwitch L683-752） ----------------

void drawToggle(RenderDevice* dev, const Rect& r, bool on) {
    // 风格由 g_uiCtx.switchStyle 单源驱动（0=系统圆润 1=方形 2=半圆润）
    // 3..4 = 「特别绘制」族（游戏ID 4..5：机械/渐变）：委托 wctl::drawToggleStyled 统一绘制
    if (g_uiCtx.switchStyle > 2) {
        wctl::drawToggleStyled(dev, r, on, 1.0f, 1.0f, g_uiCtx.switchStyle);
        return;
    }
    ThemeColors& c = currentColors();
    float s = g_uiCtx.scale;
    float bw = g_uiCtx.borderWidth();
    int ex = g_uiCtx.switchExtra;

    // 宝石变色(6)：底+滑块全由扩展图案绘制（含 on/off 双态底色）
    if (ex == 6) {
        float fs = r.size.y - 8.0f * s;
        float fx = on ? r.pos.x + r.size.x - fs * 0.5f - 4.0f * s
                      : r.pos.x + fs * 0.5f + 4.0f * s;
        wctl::drawSwitchExtra(dev, r, Vec2(fx, r.pos.y + r.size.y * 0.5f),
                              fs * 0.5f, on, 1.0f);
        return;
    }

    if (g_uiCtx.switchStyle == 1) {
        // 方形（像素街机）：直角底 + 方块滑块（滑块每侧再缩 2px 更轻盈）
        float fs = r.size.y - 6.0f * s;
        float fx = on ? r.pos.x + r.size.x - fs - 3.0f * s : r.pos.x + 3.0f * s;
        float fy = r.pos.y + 3.0f * s;
        drawRect(dev, r, on ? c.toggleOnBg : c.toggleOffBg);
        drawBorder(dev, r, on ? c.toggleOnBorder : c.toggleOffBorder, bw);
        if (ex > 0) {
            wctl::drawSwitchExtra(dev, r, Vec2(fx + fs * 0.5f, fy + fs * 0.5f), fs * 0.5f, on, 1.0f);
        } else {
            drawRect(dev, Rect(Vec2(fx, fy), Vec2(fs, fs)), on ? c.toggleOnSlider : c.toggleOffSlider);
        }
    } else if (g_uiCtx.switchStyle == 2) {
        // 半圆润（使命风）：小圆角底(10/32h) + 圆角方块滑块
        float rad = r.size.y * (10.0f / 32.0f);
        float fs = r.size.y - 8.0f * s;
        float fx = on ? r.pos.x + r.size.x - fs - 4.0f * s : r.pos.x + 4.0f * s;
        float fy = r.pos.y + 4.0f * s;
        drawRoundedRect(dev, r, on ? c.toggleOnBg : c.toggleOffBg, rad);
        drawRoundedBorder(dev, r, on ? c.toggleOnBorder : c.toggleOffBorder, bw, rad);
        if (ex > 0) {
            wctl::drawSwitchExtra(dev, r, Vec2(fx + fs * 0.5f, fy + fs * 0.5f), fs * 0.5f, on, 1.0f);
        } else {
            drawRoundedRect(dev, Rect(Vec2(fx, fy), Vec2(fs, fs)),
                            on ? c.toggleOnSlider : c.toggleOffSlider, fs / 3.0f);
        }
    } else {
        // 系统圆润：胶囊底 + 圆滑块
        float rad = c.toggleRadius * s * g_uiCtx.radiusScale();
        drawRoundedRect(dev, r, on ? c.toggleOnBg : c.toggleOffBg, rad);
        drawRoundedBorder(dev, r, on ? c.toggleOnBorder : c.toggleOffBorder, bw, rad);
        float sliderSize = r.size.y - 4.0f * s;
        float sliderX = on ? r.pos.x + r.size.x - sliderSize - 2.0f * s : r.pos.x + 2.0f * s;
        float sliderY = r.pos.y + 2.0f * s;
        if (ex > 0) {
            wctl::drawSwitchExtra(dev, r, Vec2(sliderX + sliderSize * 0.5f, sliderY + sliderSize * 0.5f),
                                  sliderSize * 0.5f, on, 1.0f);
        } else {
            drawRoundedRect(dev, Rect(Vec2(sliderX, sliderY), Vec2(sliderSize, sliderSize)),
                            on ? c.toggleOnSlider : c.toggleOffSlider, sliderSize * 0.5f);
        }
    }
}

// ---------------- 完整滑条（原 drawSlider L641-681） ----------------

void drawSliderFull(RenderDevice* dev, const Rect& groove, float t, bool hover,
                    float zoom) {
    ThemeColors& c = currentColors();
    // zoom：局部缩放（默认 1）；细节尺寸与 groove 同比例，外观不失调
    float s = g_uiCtx.scale * zoom;

    // 值文字条（内部 6 = 游戏ID 7）：整框 + 竖直抓块 + 居中值文字（参考帧滑块 1:1）
    if (g_uiCtx.sliderStyle == 6) {
        char vb[16];
        snprintf(vb, sizeof(vb), "%d%%", (int)(t * 100.0f + 0.5f));
        wctl::drawSliderValue(dev, groove, t, vb, hover, 1.0f, zoom);
        return;
    }

    // 非默认风格（1-5）：统一委托 wctl（霓虹细轨/竖柄霓虹/春意描边/像素霓虹/圆润）
    if (g_uiCtx.sliderStyle != 0) {
        wctl::drawSlider(dev, groove, t, hover, 1.0f, zoom);
        return;
    }

    // 默认风格：8px 圆角槽 + 渐变填充 + 圆形手柄 + 边框
    float grooveH = 8.0f * s;
    float grooveY = groove.pos.y + groove.size.y * 0.5f - grooveH * 0.5f;
    float grooveRad = grooveH * 0.5f;
    drawRoundedRect(dev, Rect(Vec2(groove.pos.x, grooveY), Vec2(groove.size.x, grooveH)),
                    Color(c.sliderGroove.r, c.sliderGroove.g, c.sliderGroove.b, 200.0f / 255.0f), grooveRad);
    float fillW = t * groove.size.x;
    if (fillW > 0) {
        drawRoundedGradientRectH(dev, Rect(Vec2(groove.pos.x, grooveY), Vec2(fillW, grooveH)),
                                 c.accent, c.accent2, grooveRad);
    }
    float handleSize = 16.0f * s;
    float handleX = groove.pos.x + fillW - handleSize * 0.5f;
    float handleY = groove.pos.y + groove.size.y * 0.5f - handleSize * 0.5f;
    Rect handleRect = Rect(Vec2(handleX, handleY), Vec2(handleSize, handleSize));
    drawRoundedRect(dev, handleRect, c.sliderHandle, handleSize * 0.5f);
    drawRoundedBorder(dev, handleRect, c.sliderHandleBorder, g_uiCtx.borderWidth(), handleSize * 0.5f);
}

} // namespace pctl
} // namespace ui