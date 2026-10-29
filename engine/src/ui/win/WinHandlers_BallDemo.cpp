// AI生成
// ============================================================
// 悬浮球测试窗口处理器（handler_ball_demo）
//
// 用途：「UI模板」页区域①「悬浮球」行点击 → 打开本窗，演示悬浮球的真实
//   语义——悬浮球是悬浮窗的"收拢态"，点击球即把窗口展开。
//
// 系统派发对照（LayerStack 悬浮球系统，规范 §7.2/§9.1）：
//   · 悬浮球按钮/收拢/展开/拖拽/靠边吸附/半隐藏 全部由系统统一处理
//     （配置项 WIN_HAS_FLOAT_BALL 声明即得，本处理器零基础设施代码）；
//   · 本处理器只画内容：一个跟随全局 g_uiCtx.ballStyle 的预览球 + 说明文字。
//
// 样式单源：预览球直接读 g_uiCtx.ballStyle（内部 0=圆形 1=方形），与页面、
//   窗口系统同一单源，禁止本处再定义样式表（对齐 StyleTpl.h 单源纪律）。
// ============================================================
#include "WinHandlers.h"
#include "UiContext.h"
#include "StyleTpl.h"
#include "WindowButtons.h"
#include "WinControls.h"
#include "../Element.h"
#include "../Theme.h"
#include "../Font.h"
#include <cstdio>

namespace ui {

void renderBallDemo(RenderDevice* dev, WindowInstance& win, float alpha) {
    if (!dev) return;
    float s = g_uiCtx.scale;
    float titleH = win.hasTitleBar() ? wbtn::titleBarHeight(win) : 0.0f;
    ThemeColors& c = currentColors();
    Rect content(Vec2(win.x + 10.0f * s, win.y + titleH + 10.0f * s),
                 Vec2(win.w - 20.0f * s, win.h - titleH - 20.0f * s));

    // 预览球（单源：g_uiCtx.ballStyle；内部 0=圆形 1=方形 >1=特别绘制）
    float r = 22.0f * s;
    float cx = content.pos.x + r + 4.0f * s;
    float cy = content.pos.y + r + 4.0f * s;
    Color fill(c.accent.r, c.accent.g, c.accent.b, 230.0f / 255.0f * alpha);
    if (g_uiCtx.ballStyle > 1)
        wctl::drawBallStyled(dev, cx, cy, r, 230.0f / 255.0f * alpha, g_uiCtx.ballStyle);
    else if (g_uiCtx.ballStyle == 0)
        fillCircle(dev, cx, cy, r, fill);
    else
        drawRoundedRect(dev, Rect(Vec2(cx - r, cy - r), Vec2(r * 2.0f, r * 2.0f)), fill, 4.0f * s);

    int gid = tpl::idOf(g_uiCtx.ballStyle);
    const char* nm = (gid >= 0 && gid < tpl::ball::COUNT) ? tpl::ball::NAMES[gid] : u8"?";
    char l1[64];
    snprintf(l1, sizeof(l1), u8"当前悬浮球样式：%s", nm);
    wctl::drawTextLeftAt(dev, g_uiCtx.font, l1,
                         Vec2(cx + r + 12.0f * s, cy - 12.0f * s), 24.0f * s,
                         Color(c.text.r, c.text.g, c.text.b, 235.0f / 255.0f * alpha));

    Color dim(c.text.r, c.text.g, c.text.b, 165.0f / 255.0f * alpha);
    wctl::drawTextLeftAt(dev, g_uiCtx.smallFont,
                         u8"标题栏「悬浮球」按钮可把本窗收成球；点击球可展开",
                         Vec2(content.pos.x + 4.0f * s, cy + r + 12.0f * s), 18.0f * s, dim);
    wctl::drawTextLeftAt(dev, g_uiCtx.smallFont,
                         u8"样式在「UI模板」页悬浮球行 / 主题工作室「其他UI」调节",
                         Vec2(content.pos.x + 4.0f * s, cy + r + 36.0f * s), 18.0f * s, dim);
}

bool clickBallDemo(WindowInstance&, const Vec2&) { return false; }
void hoverBallDemo(WindowInstance&, const Vec2&) {}

} // namespace ui