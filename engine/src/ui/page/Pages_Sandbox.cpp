// AI生成
// ============================================================
// 沙盘页处理器（底级页面：无限沙盘；系统派发声明式接入）
//
// 规范对照（速查表"沙盘"行）：
//   只做：g_sandboxes.begin/end 申请视口 + drawNode 声明节点 +
//         wctl 共享控件（rect/字号经系统换算）+ 交互转发系统
//   禁止：自乘 zoom/pan、自画节点、自写 LOD 阈值、裸色、自算字号
//
// 调用条（游戏ID，单行紧凑条：ID 为主，沙盘最大化）：
//   左：有限沙盘 → 系统沙盘#1 → 「调用」打开悬浮窗有限沙盘（sandbox_demo）
//   右：无限沙盘 → 系统沙盘#2 → 「调用」复位本页无限沙盘视图（原点回视口中心 / 100%）
//   ID 映射单源：StyleTpl.h 的 tpl::sandbox（0=系统跟随 1=有限 2=无限）
// ============================================================
#include "PageControls.h"
#include "Pages.h"
#include "../MainUI.h"
#include "../win/RegionSystem.h"
#include "../win/WinControls.h"
#include "../win/SandboxSystem.h"
#include "../win/StyleTpl.h"
#include "../win/LayerStack.h"
#include "../win/WindowConfig.h"
#include <cstdio>

namespace ui {
namespace spg {

static const char* REGION_ID = "sandbox_page";

// ---- 调用条（单行两组并排：左=有限 / 右=无限；高度只占一行，沙盘占满其余）----
static const char* CALL_GROUP_NAME[2] = { u8"有限沙盘", u8"无限沙盘" };
static const char* BTN_CALL = u8"调用";
static const int   CALL_GROUP_COUNT = 2;
static const float CALL_BAR_H = 30.0f;      // 逻辑条高（单行）

// ---- 页面私有状态（交互结果值；非绘制参数） ----
struct State {
    float sliderT = 0.5f;       // 滑动条值（拖动写回）
    bool  draggingSlider = false;
    bool  hoverBtn = false;
    bool  hoverSlider = false;
    bool  hoverCall[CALL_GROUP_COUNT] = { false, false };   // 调用按钮 hover
    int   clickCount = 0;
};

// ---- 沙盘内容声明（世界坐标；绘制系统全包） ----
static const SandboxNode kNodes[3] = {
    { Vec2(   0,   0), "原点(跟随)", 0, 0, 28.0f, 0.5f, 0.2f },
    { Vec2( 120,   0), "右(固定)",   1, 1, 28.0f, 0.5f, 0.2f },
    { Vec2(-120,   0), "左(LOD)",    2, 2, 28.0f, 0.5f, 0.2f },
};
static const Vec2 kBtnWorld(200.0f, -80.0f);      // 按钮世界位置
static const Vec2 kSliderWorld(-260.0f, 80.0f);   // 滑动条世界位置

// ---- 控件几何单源（render/click/hover/drag 共用；系统变换） ----
static Rect btnRect(const SandboxHandle& h) {
    return g_sandboxes.worldToScreenRect(h, kBtnWorld, Vec2(80.0f, 30.0f));
}
static Rect sliderRect(const SandboxHandle& h) {
    return g_sandboxes.worldToScreenRect(h, kSliderWorld, Vec2(120.0f, 16.0f));
}

// 调用条：整条 → 左右两半（各含 名称 / 游戏ID 徽标 / 调用按钮）
static Rect callBarRect(const PageEnv& env) {
    float sc = env.scale();
    const Rect& v = env.content;
    return Rect(Vec2(v.pos.x + 8.0f * sc, v.pos.y + 4.0f * sc),
                Vec2(v.size.x - 16.0f * sc, CALL_BAR_H * sc - 6.0f * sc));
}
static float callGroupX(const PageEnv& env, int i) {
    Rect b = callBarRect(env);
    return b.pos.x + b.size.x * 0.5f * (float)i;
}
static Rect callNameRect(const PageEnv& env, int i) {
    float sc = env.scale();
    Rect b = callBarRect(env);
    return Rect(Vec2(callGroupX(env, i), b.pos.y), Vec2(76.0f * sc, b.size.y));
}
static Rect callBadgeRect(const PageEnv& env, int i) {
    float sc = env.scale();
    Rect b = callBarRect(env);
    float bh = 22.0f * sc;
    return Rect(Vec2(callGroupX(env, i) + 78.0f * sc, b.pos.y + (b.size.y - bh) * 0.5f),
                Vec2(126.0f * sc, bh));
}
static Rect callBtnRect(const PageEnv& env, int i) {
    float sc = env.scale();
    Rect b = callBarRect(env);
    float w = 58.0f * sc, bh = 22.0f * sc;
    return Rect(Vec2(callGroupX(env, i) + 212.0f * sc, b.pos.y + (b.size.y - bh) * 0.5f),
                Vec2(w, bh));
}
// 沙盘视口 = 内容区扣掉顶部单行调用条（沙盘最大化）
static Rect sandboxViewRect(const PageEnv& env) {
    float sc = env.scale();
    const Rect& v = env.content;
    float top = 4.0f * sc + CALL_BAR_H * sc + 2.0f * sc;
    return Rect(Vec2(v.pos.x, v.pos.y + top), Vec2(v.size.x, v.size.y - top));
}

// 该组的游戏ID（有限=1 / 无限=2；单源 tpl::sandbox）
static int callGameId(int i) {
    return (i == 0) ? tpl::sandbox::FINITE : tpl::sandbox::INFINITE;
}

} // namespace spg

// ---------------- render（声明式：申请 → 声明节点/控件 → 归还） ----------------

static void renderSandboxPage(PageEnv& env) {
    spg::State& st = env.state<spg::State>();
    ThemeColors& c = currentColors();
    float sc = env.scale();
    Font* sf = env.smallFont();
    Color nameCol(c.text.r, c.text.g, c.text.b, 215.0f / 255.0f);

    // ---- 调用条（单行）：有限沙盘 / 无限沙盘 各自独立游戏ID + 调用按钮 ----
    for (int i = 0; i < spg::CALL_GROUP_COUNT; i++) {
        pctl::drawTextLeft(env.dev, sf, spg::CALL_GROUP_NAME[i],
                           spg::callNameRect(env, i), nameCol);
        char idb[48];
        snprintf(idb, sizeof(idb), u8"系统沙盘#%d", spg::callGameId(i));
        Rect br = spg::callBadgeRect(env, i);
        float rad = 11.0f * sc;
        drawRoundedRect(env.dev, br, Color(c.accent.r, c.accent.g, c.accent.b, 36.0f / 255.0f), rad);
        drawRoundedBorder(env.dev, br,
                          Color(c.accent.r, c.accent.g, c.accent.b, 130.0f / 255.0f),
                          g_uiCtx.borderWidth(), rad);
        pctl::drawTextCentered(env.dev, sf, idb, br,
            Color(c.accent2.r, c.accent2.g, c.accent2.b, 245.0f / 255.0f));
        wctl::drawBtn(env.dev, spg::callBtnRect(env, i), spg::BTN_CALL, st.hoverCall[i], 1.0f);
    }

    // ---- 沙盘视口（调用条之下，占满剩余）：无限沙盘 ----
    SandboxHandle h = g_sandboxes.begin(env.dev, env.vwin(),
        { spg::sandboxViewRect(env), spg::REGION_ID, 0.05f, 10.0f, 1.0f,
          true, true, true, true, 0, 0 });   // 无限沙盘（worldW/H=0；HUD 默认显示）
    if (!h.valid) return;

    // 节点：声明即画（三模式尺寸 + LOD + 配色系统全包）
    for (const auto& n : spg::kNodes)
        g_sandboxes.drawNode(env.dev, h, n, 1.0f);

    // 按钮：共享控件库 + 系统变换矩形 + 系统字号换算（整体 ×zoom 不失调）
    char btnLabel[32];
    snprintf(btnLabel, sizeof(btnLabel), u8"点击:%d", st.clickCount);
    Rect btnR = spg::btnRect(h);
    wctl::drawBtn(env.dev, btnR, btnLabel, st.hoverBtn, 1.0f,
                  wctl::fontPxForScreen(g_sandboxes.nodeFontPx(h, 13.0f)), h.zoom);

    // 滑动条：页面标准控件（对照音量条同款）+ 系统变换矩形（整体 ×zoom 不失调）
    Rect slR = spg::sliderRect(h);
    pctl::drawSliderFull(env.dev, slR, st.sliderT,
                         st.hoverSlider || st.draggingSlider, h.zoom);

    g_sandboxes.end(env.dev, h, 1.0f);
}

// ---------------- 交互（控件命中 → 系统平移；单源矩形） ----------------

static bool clickSandboxPage(PageEnv& env, const Vec2& m) {
    spg::State& st = env.state<spg::State>();
    // 调用条优先（在沙盘视口之外，不冲突）
    for (int i = 0; i < spg::CALL_GROUP_COUNT; i++) {
        if (!spg::callBtnRect(env, i).contains(m)) continue;
        if (i == 0) {
            // 有限沙盘：帧末统一创建悬浮窗（系统派发，配置表驱动）
            g_layerStack.pendingOpen(FindWindowConfigIndex("sandbox_demo"));
        } else {
            // 无限沙盘：本页即无限沙盘 → 复位视图（原点回视口中心 / 100%）
            g_sandboxes.resetView(env.vwin(), spg::REGION_ID);
        }
        return true;
    }

    const SandboxState* sb = g_sandboxes.find(env.vwin().id, spg::REGION_ID);
    if (!sb) return g_sandboxes.handlePress(env.vwin(), m, true);
    SandboxHandle h;
    h.valid = true; h.zoom = sb->zoom; h.pan = sb->pan; h.viewRect = sb->viewRect;
    if (spg::btnRect(h).contains(m)) { st.clickCount++; return true; }
    if (spg::sliderRect(h).contains(m)) {
        st.draggingSlider = true;
        st.sliderT = wctl::sliderValueAt(spg::sliderRect(h), m.x);
        return true;
    }
    return g_sandboxes.handlePress(env.vwin(), m, true);   // 未命中控件 → 沙盘平移
}

static void hoverSandboxPage(PageEnv& env, const Vec2& m) {
    spg::State& st = env.state<spg::State>();
    for (int i = 0; i < spg::CALL_GROUP_COUNT; i++)
        st.hoverCall[i] = spg::callBtnRect(env, i).contains(m);

    g_sandboxes.updateHover(env.vwin(), m);
    const SandboxState* sb = g_sandboxes.find(env.vwin().id, spg::REGION_ID);
    if (!sb) return;
    SandboxHandle h;
    h.valid = true; h.zoom = sb->zoom; h.pan = sb->pan;
    st.hoverBtn = spg::btnRect(h).contains(m);
    st.hoverSlider = spg::sliderRect(h).contains(m);
}

static void dragSandboxPage(PageEnv& env, const Vec2& m) {
    spg::State& st = env.state<spg::State>();
    if (st.draggingSlider) {
        const SandboxState* sb = g_sandboxes.find(env.vwin().id, spg::REGION_ID);
        if (sb) {
            SandboxHandle h;
            h.valid = true; h.zoom = sb->zoom; h.pan = sb->pan;
            st.sliderT = wctl::sliderValueAt(spg::sliderRect(h), m.x);
        }
        return;
    }
    g_sandboxes.handleDrag(env.vwin(), m);
}

static void releaseSandboxPage(PageEnv& env, const Vec2&) {
    env.state<spg::State>().draggingSlider = false;
    g_sandboxes.endDrag();
}

static void collectSandboxPage(std::string& out) {
    out += u8"无限沙盘缩放平移按钮滑动条跟随缩放LOD";
    out += u8"调用有限沙盘无限沙盘系统沙盘游戏ID";
    for (int i = 0; i < spg::CALL_GROUP_COUNT; i++) out += spg::CALL_GROUP_NAME[i];
    out += spg::BTN_CALL;
    for (int i = 0; i < tpl::sandbox::COUNT; i++) out += tpl::sandbox::NAMES[i];
}

void RegisterSandboxPage(PageHandlerRegistry& reg) {
    PageHandler h;
    h.render  = renderSandboxPage;
    h.click   = clickSandboxPage;
    h.hover   = hoverSandboxPage;
    h.drag    = dragSandboxPage;
    h.release = releaseSandboxPage;
    h.collect = collectSandboxPage;
    reg.registerHandler("page_sandbox", h);
}

} // namespace ui