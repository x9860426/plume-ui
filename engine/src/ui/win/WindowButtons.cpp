// ============================================================
// 统一按钮系统实现：绘制 + hover + 点击 全部走同一几何计算
// 画法 1:1 对照参考 window_buttons.cpp：
//   关闭钮 = 圆角4底(hover红0xFF6B6B/普通tabText@120) + 白"✕"字符
//   悬浮球 = 白色实心圆 r=6*scale
//   置顶/固定大小 = 白色图形（圆头图钉 / 锁）
//   折叠钮 = 白"▼/▲"字符
// 按钮尺寸由配置表 titleBtnSize 驱动（标准22 / 调试20）。
// ============================================================
#include "WindowButtons.h"
#include "UiContext.h"
#include "WinControls.h"
#include "../Element.h"
#include "../Theme.h"

namespace ui {
namespace wbtn {

// 按钮边长：配置表 titleBtnSize（标准窗22 / 调试窗20，对齐参考）
float btnSize(const WindowInstance& win) {
    const WindowConfig* cfg = win.config();
    return (cfg ? cfg->titleBtnSize : 22.0f) * g_uiCtx.scale;
}
float btnGap()         { return 6.0f * g_uiCtx.scale; }
float rightMargin()    { return 8.0f * g_uiCtx.scale; }
// 标题栏高：配置表 titleBarH 驱动（标准窗36 / 调试窗32，对齐参考），回退36
float titleBarHeight(const WindowInstance& win) {
    const WindowConfig* cfg = win.config();
    return (cfg ? cfg->titleBarH : 36.0f) * g_uiCtx.scale;
}

// 从右到左的按钮序列：close 最右，然后 ball、pin、fix、collapse
enum class BtnKind { Close, Ball, Pin, Fix, Collapse };

// 从右端起逐格扣除「右侧实际存在的按钮」占位，到达目标按钮返回其矩形。
// 修复：旧版传入目标按钮自己的标志位并自扣一格，导致所有按钮
// 都落在同一位置重叠渲染（4 按钮窗全部叠在「最右-1格」处）。
static Rect layoutBtn(const WindowInstance& win, BtnKind kind) {
    float bs = btnSize(win), gap = btnGap(), rm = rightMargin();
    float y = win.y + (titleBarHeight(win) - bs) * 0.5f;
    struct Item { BtnKind k; bool has; };
    Item items[] = {
        {BtnKind::Close,    win.hasClose()},
        {BtnKind::Ball,     win.hasFloatBall()},
        {BtnKind::Pin,      win.hasPinTop()},
        {BtnKind::Fix,      win.hasFixSize()},
        {BtnKind::Collapse, win.hasCollapse()},
    };
    float x = win.x + win.w - rm - bs;
    for (const auto& it : items) {
        if (it.k == kind) return Rect(Vec2(x, y), Vec2(bs, bs));
        if (it.has) x -= bs + gap;  // 右侧按钮存在，左移一格
    }
    return Rect(Vec2(x, y), Vec2(bs, bs));  // 兜底（kind 必在序列中）
}

Rect closeBtnRect(const WindowInstance& win)     { return layoutBtn(win, BtnKind::Close); }
Rect floatBallBtnRect(const WindowInstance& win) { return layoutBtn(win, BtnKind::Ball); }
Rect pinTopBtnRect(const WindowInstance& win)    { return layoutBtn(win, BtnKind::Pin); }
Rect fixSizeBtnRect(const WindowInstance& win)   { return layoutBtn(win, BtnKind::Fix); }
Rect collapseBtnRect(const WindowInstance& win)  { return layoutBtn(win, BtnKind::Collapse); }

Rect resizeGripRect(const WindowInstance& win) {
    float gs = 14.0f * g_uiCtx.scale;
    return Rect(Vec2(win.x + win.w - gs - 4.0f * g_uiCtx.scale,
                     win.y + (win.collapsed ? titleBarHeight(win) : win.h) - gs - 4.0f * g_uiCtx.scale),
                Vec2(gs, gs));
}

void updateHover(WindowInstance& win, const Vec2& m) {
    win.hoverClose      = win.hasClose()      && closeBtnRect(win).contains(m);
    win.hoverFloatBall  = win.hasFloatBall()  && floatBallBtnRect(win).contains(m);
    win.hoverPinTop     = win.hasPinTop()     && pinTopBtnRect(win).contains(m);
    win.hoverFixSize    = win.hasFixSize()    && fixSizeBtnRect(win).contains(m);
    win.hoverCollapse   = win.hasCollapse()   && collapseBtnRect(win).contains(m);
    // 锁定（fixedSize）：缩放手柄不参与 hover（锁 = 位置+尺寸全锁）
    win.hoverResizeGrip = win.isResizable()   && !win.fixedSize &&
                          resizeGripRect(win).contains(m);
    win.hoverTitleBar   = win.hasTitleBar()   &&
        m.x >= win.x && m.x <= win.x + win.w &&
        m.y >= win.y && m.y <= win.y + titleBarHeight(win);
}

BtnAction handlePress(WindowInstance& win, const Vec2& m) {
    if (win.hasClose()      && closeBtnRect(win).contains(m))      return BtnAction::Close;
    if (win.hasFloatBall()  && floatBallBtnRect(win).contains(m))  return BtnAction::FloatBall;
    if (win.hasPinTop()     && pinTopBtnRect(win).contains(m))     return BtnAction::PinTop;
    if (win.hasFixSize()    && fixSizeBtnRect(win).contains(m))    return BtnAction::FixSize;
    if (win.hasCollapse()   && collapseBtnRect(win).contains(m))   return BtnAction::Collapse;
    // 锁定（fixedSize）：缩放手柄不响应（锁 = 位置+尺寸全锁）
    if (win.isResizable()   && !win.fixedSize &&
        resizeGripRect(win).contains(m))                            return BtnAction::Resize;
    if (win.hoverTitleBar)  return BtnAction::DragTitle;
    return BtnAction::None;
}

// ---- 绘制辅助（对照参考 window_buttons.cpp 1:1 移植） ----
// 按钮底：圆角4；非 hover 统一 tabText@120；hover/active 由调用方传入强调色
static void drawBtnBg(RenderDevice* dev, const Rect& r, const Color& bg) {
    drawRoundedRect(dev, r, bg, 4.0f * g_uiCtx.scale);
}

// 非强调态底色：tabText 半透明（参考 ToColorAlpha(c.tabText, 120*alpha)）
static Color idleBg(const ThemeColors& c, float alpha) {
    return Color(c.tabText.r, c.tabText.g, c.tabText.b, 120.0f / 255.0f * alpha);
}

void drawTitleBarButtons(RenderDevice* dev, WindowInstance& win, float alpha) {
    ThemeColors& c = currentColors();
    Color white(1, 1, 1, alpha);

    // 悬浮球按钮：hover=accent@220 底 + 白色实心圆 r=6（参考 DrawFloatBallButton）
    if (win.hasFloatBall()) {
        Rect r = floatBallBtnRect(win);
        drawBtnBg(dev, r, win.hoverFloatBall
            ? Color(c.accent.r, c.accent.g, c.accent.b, 220.0f / 255.0f * alpha)
            : idleBg(c, alpha));
        fillCircle(dev, r.pos.x + r.size.x * 0.5f, r.pos.y + r.size.y * 0.5f,
                   6.0f * g_uiCtx.scale, white);
    }
    // 置顶按钮：active=accent@220 / hover=accent@180 / idle；白图钉=圆头+三角尖
    if (win.hasPinTop()) {
        Rect r = pinTopBtnRect(win);
        Color bg = win.topmost ? Color(c.accent.r, c.accent.g, c.accent.b, 220.0f / 255.0f * alpha)
                : win.hoverPinTop ? Color(c.accent.r, c.accent.g, c.accent.b, 180.0f / 255.0f * alpha)
                : idleBg(c, alpha);
        drawBtnBg(dev, r, bg);
        float tcx = r.pos.x + r.size.x * 0.5f, tcy = r.pos.y + r.size.y * 0.5f;
        float s = g_uiCtx.scale;
        // 图钉：圆头 + 细针尖（矢量，SDF 软边）
        fillCircle(dev, tcx, tcy - 3.0f * s, 3.0f * s, white);
        fillTriangle(dev,
                     tcx - 1.6f * s, tcy - 1.2f * s,
                     tcx + 1.6f * s, tcy - 1.2f * s,
                     tcx, tcy + 6.0f * s, white);
    }
    // 固定大小按钮：active=accent@220/hover=accent@180；白锁=矩形锁体+上方半圆锁环
    if (win.hasFixSize()) {
        Rect r = fixSizeBtnRect(win);
        Color bg = win.fixedSize ? Color(c.accent.r, c.accent.g, c.accent.b, 220.0f / 255.0f * alpha)
                : win.hoverFixSize ? Color(c.accent.r, c.accent.g, c.accent.b, 180.0f / 255.0f * alpha)
                : idleBg(c, alpha);
        drawBtnBg(dev, r, bg);
        float fcx = r.pos.x + r.size.x * 0.5f, fcy = r.pos.y + r.size.y * 0.5f;
        float s = g_uiCtx.scale;
        // 锁：上方半圆锁环 + 圆角锁体（矢量，SDF 软边）
        drawArcEllipse(dev, fcx, fcy - 1.0f * s, 2.6f * s, 2.6f * s,
                       1.5f * s, 180.0f, 180.0f, white);
        drawRoundedRect(dev, Rect(Vec2(fcx - 4.0f * s, fcy - 1.0f * s),
                                  Vec2(8.0f * s, 6.0f * s)), white, 1.5f * s);
    }
    // 折叠按钮：hover=accent@200 底 + 白色矢量三角（▼ 展开 / ▲ 收起，SDF 软边）
    if (win.hasCollapse()) {
        Rect r = collapseBtnRect(win);
        drawBtnBg(dev, r, win.hoverCollapse
            ? Color(c.accent.r, c.accent.g, c.accent.b, 200.0f / 255.0f * alpha)
            : idleBg(c, alpha));
        float ccx = r.pos.x + r.size.x * 0.5f, ccy = r.pos.y + r.size.y * 0.5f;
        float s = g_uiCtx.scale;
        if (win.collapsed)
            fillTriangle(dev, ccx - 4.5f * s, ccy + 2.5f * s,
                         ccx + 4.5f * s, ccy + 2.5f * s,
                         ccx, ccy - 2.5f * s, white);   // ▲ 已折叠，点击展开
        else
            fillTriangle(dev, ccx - 4.5f * s, ccy - 2.5f * s,
                         ccx + 4.5f * s, ccy - 2.5f * s,
                         ccx, ccy + 2.5f * s, white);   // ▼ 展开中，点击收起
    }
    // 关闭按钮（最右）：hover=红0xFF6B6B 底 + 白色大写 X
    // （"✕"字形部分字体缺失渲染为乱码方块，改矢量画线，天然抗锯齿）
    if (win.hasClose()) {
        Rect r = closeBtnRect(win);
        Color bg = win.hoverClose
            ? Color(0xFF / 255.0f, 0x6B / 255.0f, 0x6B / 255.0f, alpha)  // 参考红 0xFF6B6B
            : idleBg(c, alpha);
        drawBtnBg(dev, r, bg);
        float inset = r.size.x * 0.30f;
        float cx = r.pos.x + r.size.x * 0.5f, cy = r.pos.y + r.size.y * 0.5f;
        float lw = r.size.x * 0.10f;
        drawLine(dev, cx - inset, cy - inset, cx + inset, cy + inset, lw, white);
        drawLine(dev, cx - inset, cy + inset, cx + inset, cy - inset, lw, white);
    }
}

void drawResizeGrip(RenderDevice* dev, WindowInstance& win, float alpha) {
    if (!win.isResizable() || win.fixedSize) return;  // 锁定：不画缩放手柄
    ThemeColors& c = currentColors();
    Rect r = resizeGripRect(win);
    Color col = win.hoverResizeGrip ? Color(c.accent.r, c.accent.g, c.accent.b, 220.0f / 255.0f * alpha)
                                    : Color(c.text.r, c.text.g, c.text.b, 100.0f / 255.0f * alpha);
    float gs = r.size.x;
    for (int i = 0; i < 3; i++) {
        float off = (i + 1) * gs * 0.22f;
        drawLine(dev, r.pos.x + gs - off, r.pos.y + gs, r.pos.x + gs, r.pos.y + gs - off, 1.5f * g_uiCtx.scale, col);
    }
}

} // namespace wbtn
} // namespace ui