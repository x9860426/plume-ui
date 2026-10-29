// AI生成
// ============================================================
// 概览页处理器（控件展示：下拉/步进器/分段/色块）
// 画法 1:1 迁移自原 MainUI.cpp tab0 分支（L289-481 + L1007-1049），
// 结构对齐悬浮窗 handler 样板（WinHandlers_Basic.cpp settings 模块）：
//   namespace xxx { Layout + layout() 几何单源 / State 页面私有态 }
//   render/click/hover 三处共用同一 layout()
//
// "名称（可编辑）"演示输入框已恢复：早期误判其为点击崩溃源而删除，
// 实际根因是 MainUI 框架对未实现的 drag/release 空指针派发（已修复）。
// 输入框统一走 TextInputSystem（g_winMgr.textInput()），页面接入
// 三行协议见 TextInputSystem.h 文件头（TEXT_OWNER_PAGE 通路）。
//
// 滚动接入（RegionSystem 虚拟窗通路）：
//   render 用 g_regions.begin/end 包裹全部内容 → 内容超高时系统自动
//   生成滚动容器（滚轮/滚动条/裁剪/tabId 隐藏全部系统自动）；
//   click/hover 用 g_regions.getScroll 把鼠标 y 平移回内容坐标。
// ============================================================
#include "PageControls.h"
#include "Pages.h"
#include "../MainUI.h"
#include "../win/RegionSystem.h"
#include "../win/TextInputSystem.h"
#include "../win/WinControls.h"
#include "../win/DropdownSystem.h"
#include <cstdio>

namespace ui {

namespace overview {

// ---- 页面静态文案（配置驱动，改文案不动布局代码） ----

static const char* LABEL_NAME  = u8"名称";
static const char* HINT_INPUT  = u8"点击输入…";
static const char* LABEL_THEME  = u8"主题方案";
static const char* LABEL_COUNT  = u8"数量";
static const char* LABEL_VIEW   = u8"视图模式";
static const char* LABEL_COLOR  = u8"颜色";

static const char* DD_ITEMS[]   = { u8"海洋蓝", u8"翡翠绿", u8"落日橙", u8"星空紫" };
static constexpr int DD_COUNT   = 4;
static const char* SEG_LABELS[] = { u8"列表", u8"网格", u8"卡片" };
static constexpr int SEG_COUNT  = 3;
static constexpr int SEG_LABEL_Y_OFF = 20;   // 标签行高（逻辑 px）

// 本页滚动区域逻辑名（RegionSystem 内按 winId+id 续期，切页自动失活）
static const char* REGION_ID = "overview_main";

// ---- 几何单源：render/click/hover 共用一次计算 ----
struct Layout {
    Rect input;                  // 名称输入框（行 0，可编辑）
    Rect dd;                     // 下拉主框
    Rect stepMinus, stepVal, stepPlus;   // 步进器三键
    Rect seg[SEG_COUNT];         // 分段按钮
    Rect swatch;                 // 色块
    float contentH = 0;          // 全部内容总高（提交给滚动系统）
    float itemH = 0;             // 下拉列表项高（列表几何单源参数）
};
static Layout layout(PageEnv& env) {
    Layout L;
    float s = env.scale();
    float cx = env.content.pos.x;
    // 全局规划自动适配：区域系统按当前滚动条模板为内容让位（换模板自动改可用宽）
    float cw = g_regions.getContentW(env.vwin().id, REGION_ID);
    if (cw <= 0.0f) cw = env.content.size.x;   // 首帧尚无区域 → 临时全宽
    float y  = env.content.pos.y;
    float rowH  = 66.0f * s;      // 每行高（标签 20 + 控件 34 + 行距）
    float ctrlH = 34.0f * s;      // 控件高
    float lblH  = 20.0f * s;      // 标签行高

    // 行 0：名称输入框（半宽；编辑交互统一 TextInputSystem，页面零自理）
    L.input = Rect(Vec2(cx, y + lblH + 2.0f * s), Vec2(cw * 0.5f, ctrlH));
    y += rowH;
    // 行 1：主题下拉
    L.dd = Rect(Vec2(cx, y + lblH + 2.0f * s), Vec2(cw, ctrlH));
    y += rowH;
    // 行 2：数量步进器（- [值] +，固定偏移 120/162/226，对照原 L125-127）
    float sy = y + lblH - 2.0f * s;
    L.stepMinus = Rect(Vec2(cx + 120.0f * s, sy), Vec2(42.0f * s, ctrlH + 4.0f * s));
    L.stepVal   = Rect(Vec2(cx + 162.0f * s, sy), Vec2(64.0f * s, ctrlH + 4.0f * s));
    L.stepPlus  = Rect(Vec2(cx + 226.0f * s, sy), Vec2(42.0f * s, ctrlH + 4.0f * s));
    y += rowH;
    // 行 3：视图模式分段（三等分，段间 4px）
    float segW = cw / (float)SEG_COUNT;
    for (int i = 0; i < SEG_COUNT; i++)
        L.seg[i] = Rect(Vec2(cx + i * segW, y + lblH + 2.0f * s),
                        Vec2(segW - 4.0f * s, ctrlH));
    y += rowH;
    // 行 4：颜色色块（60×40）
    L.swatch = Rect(Vec2(cx, y + lblH + 2.0f * s), Vec2(60.0f * s, 40.0f * s));

    L.itemH = 32.0f * s;          // 下拉列表项高（原 L372）
    // 内容总高 = 色块底 + 底部留白（超出视口 → 系统自动滚动）
    L.contentH = (L.swatch.pos.y + L.swatch.size.y) - env.content.pos.y + 16.0f * s;
    return L;
}

// ---- 页面私有状态（挂 MainUI，按页隔离；切页保留） ----
struct State {
    EditText editName{HINT_INPUT, 64, TEXT_ANY};   // 名称（可编辑，系统托管）
    int  ddSel = 0;               // 下拉选中项（系统派发：DropdownSystem 读写）
    int  stepperVal = 5;          // 步进器数值
    int  segSel = 0;              // 分段选中
    int  colorR = 74, colorG = 144, colorB = 217;   // 色块 RGB
    // hover 态（hover 处理器整批重算，无需手动清空）
    bool hoverMinus = false, hoverPlus = false;
    bool hoverSwatch = false;
    int  hoverSeg = -1;
};

// 内容坐标平移工具：布局矩形 → 当前滚动偏移下的屏幕矩形
static Rect tr(const Rect& r, float dy) { return Rect(Vec2(r.pos.x, r.pos.y + dy), r.size); }

} // namespace overview

// ---------------- render：绘制全部内容（滚动区域内） ----------------

static void renderOverview(PageEnv& env) {
    overview::Layout L = overview::layout(env);
    overview::State& s = env.state<overview::State>();
    ThemeColors& c = currentColors();
    float sc = env.scale();
    Font* f = env.font();
    Font* sf = env.smallFont();
    Color lblCol(c.text.r, c.text.g, c.text.b, 180.0f / 255.0f);

    // 申请滚动区域（整个内容面板；contentH 超视口 → 系统自动滚动容器）
    RegionRequest req;
    req.x = 0; req.y = 0;
    req.w = env.content.size.x; req.h = env.content.size.y;
    req.tabId = 0; req.id = overview::REGION_ID;
    RegionHandle h = g_regions.begin(env.dev, env.vwin(), req);
    if (!h.valid) return;
    float dy = -h.scrollY;   // 滚动平移量（内容往上滚 = 往上画）

    // ---- 行 0：名称输入框（点击聚焦定位光标 → 输入/方向键编辑，
    //      Enter/点击他处提交，Esc 放弃） ----
    pctl::drawTextLeft(env.dev, sf, overview::LABEL_NAME,
                       overview::tr(Rect(Vec2(L.input.pos.x, L.input.pos.y - 20.0f * sc),
                               Vec2(120.0f * sc, 20.0f * sc)), dy), lblCol);
    {
        TextInputSystem& tis = g_winMgr.textInput();   // 焦点态/光标相位系统单源
        bool focused = tis.focused(s.editName);
        wctl::drawInput(env.dev, overview::tr(L.input, dy), s.editName.buf.c_str(),
                        overview::HINT_INPUT, focused, focused && tis.caretOn(), 1.0f,
                        12.0f, focused ? s.editName.caret : -1);
    }

    // ---- 行 1：主题下拉（系统派发：DropdownSystem 统一画+交互） ----
    pctl::drawTextLeft(env.dev, sf, overview::LABEL_THEME,
                       overview::tr(Rect(Vec2(L.dd.pos.x, L.dd.pos.y - 20.0f * sc),
                               Vec2(L.dd.size.x, 20.0f * sc)), dy), lblCol);
    Rect ddR = overview::tr(L.dd, dy);
    g_dropdowns.draw(env.dev, env.vwin(), { ddR, overview::DD_ITEMS, overview::DD_COUNT, s.ddSel, &s.ddSel, nullptr, "ov_dd", L.itemH, f }, 1.0f);

    // ---- 行 2：数量步进器 ----
    Rect mn = overview::tr(L.stepMinus, dy), vl = overview::tr(L.stepVal, dy), pl = overview::tr(L.stepPlus, dy);
    pctl::drawTextLeft(env.dev, sf, overview::LABEL_COUNT,
                       overview::tr(Rect(Vec2(L.stepMinus.pos.x, L.stepMinus.pos.y - 20.0f * sc),
                               Vec2(120.0f * sc, 20.0f * sc)), dy), lblCol);
    char valBuf[16];
    snprintf(valBuf, sizeof(valBuf), "%d", s.stepperVal);
    pctl::drawStepper(env.dev, mn, vl, pl, valBuf,
                      s.hoverMinus, s.hoverPlus, f, env.bigFont());

    // ---- 行 3：视图模式分段 ----
    pctl::drawTextLeft(env.dev, sf, overview::LABEL_VIEW,
                       overview::tr(Rect(Vec2(L.seg[0].pos.x, L.seg[0].pos.y - 20.0f * sc),
                               Vec2(120.0f * sc, 20.0f * sc)), dy), lblCol);
    for (int i = 0; i < overview::SEG_COUNT; i++) {
        int state = (i == s.segSel) ? pctl::STATE_ACTIVE :
                    (i == s.hoverSeg ? pctl::STATE_HOVER : pctl::STATE_NORMAL);
        pctl::drawSegItem(env.dev, overview::tr(L.seg[i], dy), overview::SEG_LABELS[i], state, f);
    }

    // ---- 行 4：颜色色块 ----
    pctl::drawTextLeft(env.dev, sf, overview::LABEL_COLOR,
                       overview::tr(Rect(Vec2(L.swatch.pos.x, L.swatch.pos.y - 20.0f * sc),
                               Vec2(120.0f * sc, 20.0f * sc)), dy), lblCol);
    {
        Rect r = overview::tr(L.swatch, dy);
        float rad = 8.0f * sc * g_uiCtx.radiusScale();
        drawShadow(env.dev, r, g_uiCtx.shadowBlur(), 2.0f,
                   Color(0, 0, 0, g_uiCtx.shadowAlpha()), rad);
        drawRoundedRect(env.dev, r, Color::rgb(s.colorR, s.colorG, s.colorB), rad);
        Color borderCol = s.hoverSwatch ? (Color)c.accent :
            Color(c.inputBorder.r, c.inputBorder.g, c.inputBorder.b, 140.0f / 255.0f);
        drawRoundedBorder(env.dev, r, borderCol,
                          g_uiCtx.borderWidth() * (s.hoverSwatch ? 1.6f : 1.0f), rad);
        // 色块右侧 #RRGGBB 文本
        char hexBuf[8];
        snprintf(hexBuf, sizeof(hexBuf), "#%02X%02X%02X", s.colorR, s.colorG, s.colorB);
        pctl::drawTextLeft(env.dev, sf, hexBuf,
                           Rect(Vec2(r.pos.x + r.size.x + 12.0f * sc, r.pos.y + r.size.y * 0.5f - 8.0f * sc),
                                Vec2(100.0f * sc, 16.0f * sc)),
                           Color(c.text.r, c.text.g, c.text.b, 160.0f / 255.0f));
    }

    // 提交内容总高 → 系统恢复裁剪 + 判定是否生成滚动容器
    g_regions.end(env.dev, h, L.contentH);
    // 下拉展开浮层（系统派发：内容画完后统一画，覆盖下方）
    g_dropdowns.renderPopups(env.dev, env.vwin(), 1.0f);
}

// ---------------- click：内容点击（返回 true = 消费并进入拖拽态） ----------------

static bool clickOverview(PageEnv& env, const Vec2& m) {
    overview::Layout L = overview::layout(env);
    overview::State& s = env.state<overview::State>();
    // 鼠标 y 平移回"未滚动"内容坐标（与布局矩形同坐标系）
    float scroll = g_regions.getScroll(env.vwin().id, overview::REGION_ID);
    Vec2 lm(m.x, m.y + scroll);

    // 下拉系统派发（选项命中/按钮展开/外部关闭，吞点击防穿透）
    if (g_dropdowns.handlePress(env.vwin(), m).consumed) return true;
    // 名称输入框：点击聚焦 + 光标定位到最近字符边界（系统已统一提交旧焦点）
    // 框用当前滚动平移后的屏幕矩形（与 render 的 tr(L.input, dy) 同式，dy = -scroll）
    if (L.input.contains(lm)) {
        auto& tis = g_winMgr.textInput();
        tis.requestFocus(TEXT_OWNER_PAGE, env.ui->pageSel(), s.editName);
        tis.placeCaret(s.editName, overview::tr(L.input, -scroll), m.x);
        return true;
    }

    // 步进器：- 减（下限 0）/ + 加
    if (L.stepMinus.contains(lm)) {
        if (s.stepperVal > 0) s.stepperVal--;
        return true;
    }
    if (L.stepPlus.contains(lm)) {
        s.stepperVal++;
        return true;
    }
    // 分段按钮
    for (int i = 0; i < overview::SEG_COUNT; i++) {
        if (L.seg[i].contains(lm)) { s.segSel = i; return true; }
    }
    // 色块：恢复默认色
    if (L.swatch.contains(lm)) {
        s.colorR = 74; s.colorG = 144; s.colorB = 217;
        return true;
    }

    return false;
}

// ---------------- hover：整批重算（m=(-1e9,-1e9) 时 contains 全 false = 自动清空） ----------------

static void hoverOverview(PageEnv& env, const Vec2& m) {
    overview::Layout L = overview::layout(env);
    overview::State& s = env.state<overview::State>();
    float scroll = g_regions.getScroll(env.vwin().id, overview::REGION_ID);
    Vec2 lm(m.x, m.y + scroll);


    g_dropdowns.updateHover(env.vwin(), m);
    s.hoverMinus  = L.stepMinus.contains(lm);
    s.hoverPlus   = L.stepPlus.contains(lm);
    s.hoverSwatch = L.swatch.contains(lm);
    s.hoverSeg = -1;
    for (int i = 0; i < overview::SEG_COUNT; i++)
        if (L.seg[i].contains(lm)) s.hoverSeg = i;
}

// ---------------- collect：字体图集预热（本页固定文案） ----------------

static void collectOverview(std::string& out) {
    out += u8"名称点击输入主题方案海洋蓝翡翠绿落日橙星空紫";
    out += u8"数量列表网格卡片颜色#%";
}

// ---------------- 注册（handlerName 与 PageConfig.h 配置一致） ----------------

void RegisterOverviewPage(PageHandlerRegistry& reg) {
    PageHandler h;
    h.render  = renderOverview;
    h.click   = clickOverview;
    h.hover   = hoverOverview;
    h.collect = collectOverview;
    reg.registerHandler("page_overview", h);
}

} // namespace ui