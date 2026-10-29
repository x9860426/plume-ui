// ============================================================
// 沙盘测试窗口处理器（悬浮窗：有限沙盘；系统派发声明式接入）
//
// 规范对照（速查表"沙盘"行）：
//   只做：g_sandboxes.begin/end 申请视口 + drawNode 声明节点 + 交互转发
//   禁止：自乘 zoom/pan、自画节点、自写 LOD 阈值、裸色、自算字号
// 有限沙盘：worldW/H=400×300（pan clamp 世界边界可见；页面侧是无限的）
// ============================================================
#include "WinHandlers.h"
#include "UiContext.h"
#include "SandboxSystem.h"
#include "WindowButtons.h"
#include "WinControls.h"

namespace ui {
namespace sbx {

// ---- 沙盘内容声明（世界坐标；绘制系统全包） ----
// 世界 200×200（原点在世界左上角，有效坐标范围 0~200）；节点全部声明在
// 边界内（越界会被系统 stderr 警告 + 视觉出界）
static const SandboxNode kNodes[6] = {
    { Vec2(100, 100), "中心(跟随)", 0, 0, 28.0f, 0.5f, 0.2f },
    { Vec2(160, 100), "右(固定)",   1, 1, 28.0f, 0.5f, 0.2f },
    { Vec2( 40, 100), "左(LOD)",    2, 2, 28.0f, 0.5f, 0.2f },
    { Vec2(100, 160), "下(跟随)",   0, 0, 28.0f, 0.5f, 0.2f },
    { Vec2(100,  40), "上(固定)",   1, 1, 28.0f, 0.5f, 0.2f },
    { Vec2(150, 150), "右下(LOD)",  2, 2, 28.0f, 0.5f, 0.2f },
};

} // namespace sbx

void renderSandboxDemo(RenderDevice* dev, WindowInstance& win, float alpha) {
    float s = g_uiCtx.scale;
    float titleH = win.hasTitleBar() ? wbtn::titleBarHeight(win) : 0.0f;
    Rect view(Vec2(win.x + 8.0f * s, win.y + titleH + 8.0f * s),
              Vec2(win.w - 16.0f * s, win.h - titleH - 16.0f * s));
    // 有限沙盘申请：世界固定 200×200（系统自动边界可视化：内亮/外轻遮罩/
    // 描边，主题自适应）；辅助线/zoom% HUD 走系统参数（默认显示，按需关闭）
    SandboxHandle h = g_sandboxes.begin(dev, win,
        { view, "sb_demo", 0.2f, 5.0f, 1.0f, true, true, true, true, 200.0f, 200.0f });
    if (!h.valid) return;

    for (const auto& n : sbx::kNodes)
        g_sandboxes.drawNode(dev, h, n, alpha);

    g_sandboxes.end(dev, h, alpha);
}

bool clickSandboxDemo(WindowInstance& win, const Vec2& m) {
    return g_sandboxes.handlePress(win, m, true);   // 左键=平移（系统）
}
void hoverSandboxDemo(WindowInstance& win, const Vec2& m) { g_sandboxes.updateHover(win, m); }
void dragSandboxDemo(WindowInstance& win, const Vec2& m)  { g_sandboxes.handleDrag(win, m); }
void releaseSandboxDemo(WindowInstance&, const Vec2&)     { g_sandboxes.endDrag(); }
bool wheelSandboxDemo(WindowInstance& win, const Vec2& m, float delta) {
    return g_sandboxes.handleWheel(win, m, delta, true, false);   // 滚轮=缩放（系统）
}

} // namespace ui