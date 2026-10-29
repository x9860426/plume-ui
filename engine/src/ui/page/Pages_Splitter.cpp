// AI生成
// ============================================================
// 分割器页处理器（SplitterSystem 底级验收样板）
//
// 内容结构：
//   调用行：系统分割线#N 徽标 + 分割线风格下拉（游戏ID 调用入口）
//   演示 1：上下分割面板（水平条，拖动改 y）
//   演示 2：左右分割面板（垂直条，拖动改 x）
//   按钮：打开悬浮窗测试窗（splitter_demo，悬浮窗侧同系统验收）
//
// 接入协议（对照概览页下拉模式）：
//   render：面板按分割位置布局 + g_splitters.draw 画条（最后画，
//           覆盖面板之上）；坐标经 tr(r, dy) 平移滚动量
//   click 开头：g_dropdowns.handlePress（下拉优先吞点击）→
//               g_splitters.handlePress（吞点击防穿透）
//   drag / release：handleDrag / endDrag（系统写值）
//   hover：updateHover
//
// 滚动补偿：水平条的分割 y 是屏幕绝对坐标，页面滚动（dy 变化）时
// 逐帧平移补偿（垂直条 x 不受滚动影响，零补偿）。
// ============================================================
#include "PageControls.h"
#include "Pages.h"
#include "../MainUI.h"
#include "../win/RegionSystem.h"
#include "../win/WinControls.h"
#include "../win/SplitterSystem.h"
#include "../win/DropdownSystem.h"
#include "../win/StyleTpl.h"
#include "../win/LayerStack.h"
#include "../win/WindowConfig.h"
#include <cstdio>

namespace ui {

namespace spg {

// ---- 页面静态文案 ----
static const char* LABEL_H     = u8"上下分割（拖动中间水平条）";
static const char* LABEL_V     = u8"左右分割（拖动中间垂直条）";
static const char* PANE_TOP    = u8"上面板";
static const char* PANE_BOTTOM = u8"下面板";
static const char* PANE_LEFT   = u8"左面板";
static const char* PANE_RIGHT  = u8"右面板";
static const char* BTN_OPEN    = u8"打开悬浮窗测试窗";
static const char* CALL_NAME   = u8"分割线";

static const char* REGION_ID = "splitter_main";

// ---- 几何单源（render/click/hover 共用；逻辑坐标） ----
struct Layout {
    Rect callName;     // 调用行·名称
    Rect callBadge;    // 调用行·游戏ID 徽标（系统分割线#N）
    Rect callDrop;     // 调用行·风格下拉（游戏ID 调用入口）
    Rect demoH;        // 上下分割演示面板
    Rect demoV;        // 左右分割演示面板
    Rect openBtn;      // 打开悬浮窗按钮
    float contentH = 0;
    float paneMinH = 0;   // 上下分割子面板最小高（逻辑）
    float paneMinW = 0;   // 左右分割子面板最小宽（逻辑）
};

static Layout layout(PageEnv& env) {
    Layout L;
    float s = env.scale();
    float cx = env.content.pos.x;
    // 全局规划自动适配：区域系统按当前滚动条模板为内容让位
    float cw = g_regions.getContentW(env.vwin().id, REGION_ID);
    if (cw <= 0.0f) cw = env.content.size.x;   // 首帧尚无区域 → 临时全宽
    float y  = env.content.pos.y;
    float lblH = 20.0f * s;
    float panelH = 130.0f * s;
    float gap = 14.0f * s;

    // ---- 调用行：名称 + 游戏ID 徽标 + 风格下拉（下拉选编号 → 全局即改）----
    const float callH = 32.0f * s;
    const float dropW = 240.0f * s;
    L.callName  = Rect(Vec2(cx + 2.0f * s, y), Vec2(130.0f * s, callH));
    L.callBadge = Rect(Vec2(cx + 140.0f * s, y + (callH - 26.0f * s) * 0.5f),
                       Vec2(150.0f * s, 26.0f * s));
    L.callDrop  = Rect(Vec2(cx + cw - dropW - 12.0f * s, y + (callH - 30.0f * s) * 0.5f),
                       Vec2(dropW, 30.0f * s));
    y += callH + gap;

    y += lblH + 2.0f * s;
    L.demoH = Rect(Vec2(cx, y), Vec2(cw, panelH));
    y += panelH + gap + lblH + 2.0f * s;
    L.demoV = Rect(Vec2(cx, y), Vec2(cw, panelH));
    y += panelH + gap + 8.0f * s;
    L.openBtn = Rect(Vec2(cx, y), Vec2(180.0f * s, 40.0f * s));

    L.paneMinH = 36.0f * s;
    L.paneMinW = 56.0f * s;
    L.contentH = (L.openBtn.pos.y + L.openBtn.size.y) - env.content.pos.y + 16.0f * s;
    return L;
}

// ---- 页面私有状态（分割位置屏幕坐标；系统拖动直写） ----
struct State {
    float hPos = 0;       // 水平条分割位置（屏幕 y）
    float vPos = 0;       // 垂直条分割位置（屏幕 x；滚动不影响）
    bool  inited = false;
    float lastDy = 0;     // 上帧滚动量（水平条 y 滚动补偿）
    bool  hoverBtn = false;
    // —— 调用行：分割线风格下拉（游戏ID 调用）——
    int   ddSel = 0;      // 下拉当前选中（游戏ID 索引：0=系统跟随 1=系统默认 2..=变体）
    bool  ddDirty = false;// 值变化待落库
    bool  ddInit = false;
};

// 逻辑 → 屏幕（滚动平移；对照概览页 tr）
static Rect tr(const Rect& r, float dy) { return Rect(Vec2(r.pos.x, r.pos.y + dy), r.size); }

// 子面板绘制（半透明底 + 居中标注 + 实时占比）
static void drawSubPane(RenderDevice* dev, const Rect& r, const char* name,
                        const Color& bg, float pct, Font* f) {
    ThemeColors& c = currentColors();
    float s = g_uiCtx.scale;
    drawRoundedRect(dev, r, bg, 6.0f * s);
    char label[48];
    snprintf(label, sizeof(label), "%s %d%%", name, (int)(pct * 100.0f + 0.5f));
    pctl::drawTextCentered(dev, f, label, r,
                           Color(c.text.r, c.text.g, c.text.b, 210.0f / 255.0f));
}

} // namespace spg

// ---------------- render ----------------

static void renderSplitterPage(PageEnv& env) {
    spg::Layout L = spg::layout(env);
    spg::State& st = env.state<spg::State>();
    ThemeColors& c = currentColors();
    float sc = env.scale();
    Font* f = env.font();
    Font* sf = env.smallFont();
    Color lblCol(c.text.r, c.text.g, c.text.b, 180.0f / 255.0f);
    Color nameCol(c.text.r, c.text.g, c.text.b, 215.0f / 255.0f);
    Color curCol(c.accent.r, c.accent.g, c.accent.b, 235.0f / 255.0f);

    if (!st.ddInit) {
        st.ddSel = tpl::idOf(g_uiCtx.splitterStyle);
        st.ddDirty = false;
        st.ddInit = true;
    }

    RegionRequest req;
    req.x = 0; req.y = 0;
    req.w = env.content.size.x; req.h = env.content.size.y;
    req.tabId = 0; req.id = spg::REGION_ID;
    RegionHandle h = g_regions.begin(env.dev, env.vwin(), req);
    if (!h.valid) return;
    float dy = -h.scrollY;

    // ---- 调用行：名称 + 系统分割线#N 徽标 + 分割线风格下拉 ----
    pctl::drawTextLeft(env.dev, sf, spg::CALL_NAME, spg::tr(L.callName, dy), nameCol);
    {
        char idb[48];
        snprintf(idb, sizeof(idb), u8"系统分割线#%d", tpl::idOf(g_uiCtx.splitterStyle));
        Rect br = spg::tr(L.callBadge, dy);
        float rad = 13.0f * sc;
        drawRoundedRect(env.dev, br, Color(c.accent.r, c.accent.g, c.accent.b, 36.0f / 255.0f), rad);
        drawRoundedBorder(env.dev, br,
                          Color(c.accent.r, c.accent.g, c.accent.b, 130.0f / 255.0f),
                          g_uiCtx.borderWidth(), rad);
        pctl::drawTextCentered(env.dev, sf, idb, br,
            Color(c.accent2.r, c.accent2.g, c.accent2.b, 245.0f / 255.0f));
    }
    g_dropdowns.draw(env.dev, env.vwin(),
        { spg::tr(L.callDrop, dy), tpl::splitter::NAMES, tpl::splitter::COUNT,
          st.ddSel, &st.ddSel, &st.ddDirty,
          "spg_dd_split", 24.0f * sc, sf,
          env.content.pos.y, env.content.pos.y + env.content.size.y }, 1.0f);

    // 滚动补偿：水平条 y 为屏幕绝对坐标，页面滚动时同步平移
    if (st.inited && st.lastDy != dy) {
        st.hPos += dy - st.lastDy;
    }
    st.lastDy = dy;

    // 屏幕面板矩形
    Rect demoHS = spg::tr(L.demoH, dy);
    Rect demoVS = spg::tr(L.demoV, dy);

    // 首帧初始化（上下 40% / 左右 50%）
    if (!st.inited) {
        st.hPos = demoHS.pos.y + demoHS.size.y * 0.4f;
        st.vPos = demoVS.pos.x + demoVS.size.x * 0.5f;
        st.inited = true;
    }
    // clamp（窗口缩放后保底；与 splitter 拖动范围一致）
    if (st.hPos < demoHS.pos.y + L.paneMinH) st.hPos = demoHS.pos.y + L.paneMinH;
    if (st.hPos > demoHS.pos.y + demoHS.size.y - L.paneMinH)
        st.hPos = demoHS.pos.y + demoHS.size.y - L.paneMinH;
    if (st.vPos < demoVS.pos.x + L.paneMinW) st.vPos = demoVS.pos.x + L.paneMinW;
    if (st.vPos > demoVS.pos.x + demoVS.size.x - L.paneMinW)
        st.vPos = demoVS.pos.x + demoVS.size.x - L.paneMinW;

    // ---- 演示 1：上下分割（水平条） ----
    pctl::drawTextLeft(env.dev, sf, spg::LABEL_H,
                       spg::tr(Rect(Vec2(L.demoH.pos.x, L.demoH.pos.y - 20.0f * sc),
                               Vec2(L.demoH.size.x, 20.0f * sc)), dy), lblCol);
    {
        float gap = 5.0f * sc;   // 条热区半厚 = 子面板间隙
        Rect top(Vec2(demoHS.pos.x, demoHS.pos.y),
                 Vec2(demoHS.size.x, st.hPos - gap - demoHS.pos.y));
        Rect bottom(Vec2(demoHS.pos.x, st.hPos + gap),
                    Vec2(demoHS.size.x, demoHS.pos.y + demoHS.size.y - st.hPos - gap));
        spg::drawSubPane(env.dev, top, spg::PANE_TOP,
                         Color(c.bg2.r, c.bg2.g, c.bg2.b, 130.0f / 255.0f),
                         top.size.y / demoHS.size.y, f);
        spg::drawSubPane(env.dev, bottom, spg::PANE_BOTTOM,
                         Color(c.bg3.r, c.bg3.g, c.bg3.b, 120.0f / 255.0f),
                         bottom.size.y / demoHS.size.y, f);
    }

    // ---- 演示 2：左右分割（垂直条） ----
    pctl::drawTextLeft(env.dev, sf, spg::LABEL_V,
                       spg::tr(Rect(Vec2(L.demoV.pos.x, L.demoV.pos.y - 20.0f * sc),
                               Vec2(L.demoV.size.x, 20.0f * sc)), dy), lblCol);
    {
        float gap = 5.0f * sc;
        Rect left(Vec2(demoVS.pos.x, demoVS.pos.y),
                  Vec2(st.vPos - gap - demoVS.pos.x, demoVS.size.y));
        Rect right(Vec2(st.vPos + gap, demoVS.pos.y),
                   Vec2(demoVS.pos.x + demoVS.size.x - st.vPos - gap, demoVS.size.y));
        spg::drawSubPane(env.dev, left, spg::PANE_LEFT,
                         Color(c.bg2.r, c.bg2.g, c.bg2.b, 90.0f / 255.0f),
                         left.size.x / demoVS.size.x, f);
        spg::drawSubPane(env.dev, right, spg::PANE_RIGHT,
                         Color(c.bg3.r, c.bg3.g, c.bg3.b, 130.0f / 255.0f),
                         right.size.x / demoVS.size.x, f);
    }

    // ---- 两条分割条（最后画，覆盖面板之上；系统拖动直写 st.hPos/st.vPos） ----
    Rect hBarS(Vec2(demoHS.pos.x + 6.0f * sc, st.hPos - 5.0f * sc),
               Vec2(demoHS.size.x - 12.0f * sc, 10.0f * sc));
    Rect vBarS(Vec2(st.vPos - 5.0f * sc, demoVS.pos.y + 6.0f * sc),
               Vec2(10.0f * sc, demoVS.size.y - 12.0f * sc));
    g_splitters.draw(env.dev, env.vwin(), { hBarS, false,
        demoHS.pos.y + L.paneMinH, demoHS.pos.y + demoHS.size.y - L.paneMinH,
        &st.hPos, nullptr, "pg_sp_h" }, 1.0f);
    g_splitters.draw(env.dev, env.vwin(), { vBarS, true,
        demoVS.pos.x + L.paneMinW, demoVS.pos.x + demoVS.size.x - L.paneMinW,
        &st.vPos, nullptr, "pg_sp_v" }, 1.0f);

    // ---- 打开悬浮窗测试按钮（对照目录页主按钮绘制） ----
    {
        Rect r = spg::tr(L.openBtn, dy);
        float rad = c.btnRadius * sc * g_uiCtx.radiusScale();
        drawRoundedGradientRect(env.dev, r,
                                st.hoverBtn ? c.btnHover1 : c.btnBg1,
                                st.hoverBtn ? c.btnHover2 : c.btnBg2, rad);
        drawRoundedBorder(env.dev, r,
                          st.hoverBtn ? c.btnHoverBorder : c.btnBorder,
                          g_uiCtx.borderWidth(), rad);
        pctl::drawTextCentered(env.dev, f, spg::BTN_OPEN, r, c.btnText);
    }

    // 下拉落库：索引(游戏ID) → 内部渲染值（下一帧两条分割线即按新风格绘制）
    if (st.ddDirty) {
        g_uiCtx.splitterStyle = tpl::internalOf(st.ddSel, g_uiCtx.splitterStyle);
        st.ddDirty = false;
    }

    g_regions.end(env.dev, h, L.contentH);
    g_dropdowns.renderPopups(env.dev, env.vwin(), 1.0f);
}

// ---------------- click ----------------

static bool clickSplitterPage(PageEnv& env, const Vec2& m) {
    spg::Layout L = spg::layout(env);
    float scroll = g_regions.getScroll(env.vwin().id, spg::REGION_ID);
    Vec2 lm(m.x, m.y + scroll);

    // 系统派发优先：下拉（吞点击）→ 分割器（条命中进入拖拽）
    if (g_dropdowns.handlePress(env.vwin(), m).consumed) return true;
    if (g_splitters.handlePress(env.vwin(), m).consumed) return true;
    // 打开悬浮窗测试窗（帧末统一创建；已开则置顶复显——单例语义）
    if (L.openBtn.contains(lm)) {
        g_layerStack.pendingOpen(FindWindowConfigIndex("splitter_demo"));
        return true;
    }
    return false;
}

// ---------------- hover ----------------

static void hoverSplitterPage(PageEnv& env, const Vec2& m) {
    spg::Layout L = spg::layout(env);
    spg::State& st = env.state<spg::State>();
    float scroll = g_regions.getScroll(env.vwin().id, spg::REGION_ID);
    Vec2 lm(m.x, m.y + scroll);

    g_dropdowns.updateHover(env.vwin(), m);
    g_splitters.updateHover(env.vwin(), m);
    st.hoverBtn = L.openBtn.contains(lm);
}

// ---------------- drag / release（系统拖动直写，零自理） ----------------

static void dragSplitterPage(PageEnv& env, const Vec2& m) {
    g_splitters.handleDrag(env.vwin(), m);
}

static void releaseSplitterPage(PageEnv&, const Vec2&) {
    g_splitters.endDrag();
}

// ---------------- collect ----------------

static void collectSplitterPage(std::string& out) {
    out += u8"上下分割拖动中间水平条左右分割垂直条";
    out += u8"上面板下面板左面板右面板";
    out += u8"分割线系统分割线游戏ID调用";
    out += u8"打开悬浮窗测试窗";
    for (int i = 0; i < tpl::splitter::COUNT; i++) out += tpl::splitter::NAMES[i];
}

// ---------------- 注册 ----------------

void RegisterSplitterPage(PageHandlerRegistry& reg) {
    PageHandler h;
    h.render  = renderSplitterPage;
    h.click   = clickSplitterPage;
    h.hover   = hoverSplitterPage;
    h.drag    = dragSplitterPage;
    h.release = releaseSplitterPage;
    h.collect = collectSplitterPage;
    reg.registerHandler("page_splitter", h);
}

} // namespace ui