// ============================================================
// 分割器测试窗口处理器（SplitterSystem 系统派发验收样板）
//
// 内容结构（嵌套分割：一垂直 + 一水平）：
//   ┌─────────┬──────────┐
//   │         │   右上    │
//   │  左栏    ├─(水平条)─┤
//   │         │   右下    │
//   └(垂直条)─┴──────────┘
//
// 业务代码只做声明式接入（对照悬浮窗规范）：
//   render：面板按分割位置布局 + g_splitters.draw 画条（两行）
//   click 开头 handlePress（吞点击防穿透）
//   drag / release：handleDrag / endDrag（位置写值系统全包）
//   hover：updateHover
// 分割位置存模块 State（splitX/splitY 屏幕坐标，系统拖动直写）。
// ============================================================
#include "WinHandlers.h"
#include "UiContext.h"
#include "SplitterSystem.h"
#include "WindowButtons.h"
#include "WinControls.h"
#include "../Element.h"
#include "../Theme.h"
#include <cstdio>

namespace ui {
namespace spl {

// ====== 模块私有状态（跨帧分割位置；拖动由系统直写） ======
struct State {
    float splitX = 0;      // 垂直分割位置（屏幕 x）
    float splitY = 0;      // 右栏水平分割位置（屏幕 y）
    bool  inited = false;  // 首帧初始化标记（按窗口尺寸定中）
};

// ====== 几何单源（render 共用；clamp 后布局三面板与两条热区） ======
struct Layout {
    float contentX = 0, contentTop = 0, contentRight = 0, contentBottom = 0;
    Rect vBar;              // 垂直分割条热区
    Rect hBar;              // 右栏水平分割条热区
    Rect leftPane;          // 左面板
    Rect rightTopPane;      // 右上面板
    Rect rightBottomPane;   // 右下面板
};

static Layout layout(WindowInstance& win, State& st) {
    float s = g_uiCtx.scale;
    float titleH = win.hasTitleBar() ? wbtn::titleBarHeight(win) : 0.0f;
    float pad = 8.0f * s;
    Layout L;
    L.contentX = win.x + pad;
    L.contentTop = win.y + titleH + pad;
    L.contentRight = win.x + win.w - pad;
    L.contentBottom = win.y + win.h - pad;

    // 首帧初始化（左栏 45% / 右栏上下各半）
    if (!st.inited) {
        st.splitX = L.contentX + (L.contentRight - L.contentX) * 0.45f;
        st.splitY = L.contentTop + (L.contentBottom - L.contentTop) * 0.5f;
        st.inited = true;
    }
    // 窗口缩放后 clamp（面板最小尺寸保底）
    float minPaneW = 60.0f * s, minPaneH = 50.0f * s;
    if (st.splitX < L.contentX + minPaneW) st.splitX = L.contentX + minPaneW;
    if (st.splitX > L.contentRight - minPaneW) st.splitX = L.contentRight - minPaneW;
    if (st.splitY < L.contentTop + minPaneH) st.splitY = L.contentTop + minPaneH;
    if (st.splitY > L.contentBottom - minPaneH) st.splitY = L.contentBottom - minPaneH;

    // 条热区（厚 10×scale 好命中；长度端部内缩 6×scale 留呼吸空间）
    L.vBar = Rect(Vec2(st.splitX - 5.0f * s, L.contentTop + 6.0f * s),
                  Vec2(10.0f * s, L.contentBottom - L.contentTop - 12.0f * s));
    float rx = st.splitX + 10.0f * s + 8.0f * s;   // 右栏起点（垂直条右侧留 8×scale）
    L.hBar = Rect(Vec2(rx + 6.0f * s, st.splitY - 5.0f * s),
                  Vec2(L.contentRight - rx - 12.0f * s, 10.0f * s));

    // 三面板（沿分割位置切分；条热区半厚 5×scale 作为间隙）
    L.leftPane = Rect(Vec2(L.contentX, L.contentTop),
                      Vec2(st.splitX - 5.0f * s - L.contentX,
                           L.contentBottom - L.contentTop));
    L.rightTopPane = Rect(Vec2(rx, L.contentTop),
                          Vec2(L.contentRight - rx,
                               st.splitY - 5.0f * s - L.contentTop));
    L.rightBottomPane = Rect(Vec2(rx, st.splitY + 5.0f * s),
                             Vec2(L.contentRight - rx,
                                  L.contentBottom - st.splitY - 5.0f * s));
    return L;
}

// 面板绘制（半透明底 + 居中标注栏名与实时尺寸，拖动效果直观可验）
static void drawPane(RenderDevice* dev, const Rect& r, const char* name,
                     const Color& bg, float alpha) {
    float s = g_uiCtx.scale;
    ThemeColors& c = currentColors();
    drawRoundedRect(dev, r, Color(bg.r, bg.g, bg.b, bg.a * alpha), 6.0f * s);
    char label[48];
    snprintf(label, sizeof(label), "%s %dx%d", name,
             (int)(r.size.x / s + 0.5f), (int)(r.size.y / s + 0.5f));
    wctl::drawTextCenterAt(dev, g_uiCtx.smallFont, label, r,
                           Color(c.text.r, c.text.g, c.text.b, 210.0f / 255.0f * alpha));
}

} // namespace spl

// ============================================================
// 渲染：三面板 + 两条（声明式，分割位置系统拖动直写 State）
// ============================================================
void renderSplitterDemo(RenderDevice* dev, WindowInstance& win, float alpha) {
    ThemeColors& c = currentColors();
    float s = g_uiCtx.scale;
    spl::State& st = win.getState<spl::State>();
    spl::Layout L = spl::layout(win, st);

    // 三面板（不同底色区分：左 bg2 / 右上 bg3 / 右下 bg2 弱化）
    spl::drawPane(dev, L.leftPane, "左栏",
                  Color(c.bg2.r, c.bg2.g, c.bg2.b, 130.0f / 255.0f), alpha);
    spl::drawPane(dev, L.rightTopPane, "右上",
                  Color(c.bg3.r, c.bg3.g, c.bg3.b, 130.0f / 255.0f), alpha);
    spl::drawPane(dev, L.rightBottomPane, "右下",
                  Color(c.bg2.r, c.bg2.g, c.bg2.b, 90.0f / 255.0f), alpha);

    // 两条分割条（系统派发：拖动写 st.splitX / st.splitY，范围与 layout clamp 一致）
    float minPaneW = 60.0f * s, minPaneH = 50.0f * s;
    g_splitters.draw(dev, win, { L.vBar, true,
        L.contentX + minPaneW, L.contentRight - minPaneW,
        &st.splitX, nullptr, "sp_demo_v" }, alpha);
    g_splitters.draw(dev, win, { L.hBar, false,
        L.contentTop + minPaneH, L.contentBottom - minPaneH,
        &st.splitY, nullptr, "sp_demo_h" }, alpha);
}

// ============================================================
// 交互（全部转发系统，模块零拖拽代码）
// ============================================================
bool clickSplitterDemo(WindowInstance& win, const Vec2& m) {
    if (g_splitters.handlePress(win, m).consumed) return true;
    return false;
}

void hoverSplitterDemo(WindowInstance& win, const Vec2& m) {
    g_splitters.updateHover(win, m);
}

void dragSplitterDemo(WindowInstance& win, const Vec2& m) {
    g_splitters.handleDrag(win, m);
}

void releaseSplitterDemo(WindowInstance&, const Vec2&) {
    g_splitters.endDrag();
}

} // namespace ui