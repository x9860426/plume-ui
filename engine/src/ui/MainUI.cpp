// AI生成
// ============================================================
// 主界面框架实现（页面化重写版）
//
// 框架只做四件事（对照悬浮窗 WindowManager 的角色）：
//   1. 框架 chrome 绘制：tab 栏/侧栏/内容面板/底栏（全部配置表驱动）
//   2. 输入派发：框架控件消费 → 当前页处理器转发（click/hover/drag/release/text）
//   3. RegionSystem 虚拟窗接入：页面滚动的滚轮/滚动条系统处理
//   4. 悬浮窗系统接缝：handleInput / feedChars / frame / rescale / 注入 g_uiCtx
//
// 原 1207 行"43 手算矩形 + 12 hover 变量 + 10 hitTest + 240 行输入路由"
// 的页面级代码全部迁移至 page/ 三页面处理器（约 900 行，每页独立可扩展）。
// ============================================================
#include "MainUI.h"
#include "page/PageHandler.h"
#include "page/PageControls.h"
#include "Background.h"
#include "DynamicBg.h"
#include "win/LayerStack.h"
#include "win/WinControls.h"
#include "win/RegionSystem.h"
#include "win/DropdownSystem.h"
#include "win/SandboxSystem.h"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace ui {

// hover "离开清空"约定坐标（页面 contains 全 false → 整批 hover 自动复位）
static const Vec2 kMouseAway(-1e9f, -1e9f);

// 底栏快捷键提示（静态文案，预热进字体图集）
static const char* INFO_HINT =
    u8"F1重置视觉 F2切后端 F3主题 F4新窗口 F11全屏";

// 矩形列表命中（框架三组按钮共用的命中测试）
static int hitRectList(const std::vector<Rect>& list, const Vec2& m) {
    for (int i = 0; i < (int)list.size(); i++)
        if (list[i].contains(m)) return i;
    return -1;
}

MainUI::MainUI() {
    // 虚拟窗：id 负值永不与真实悬浮窗冲突（真实 id 从 1 起）；
    // tabSel 恒 0（页面区域 tabId=0，无多标签滚动分组需求）
    vwin_.id = kPageVirtualWinId;
    vwin_.tabSel = 0;
    vwin_.visible = true;
}

MainUI::~MainUI() = default;   // unique_ptr<PageHandlerRegistry> 析构（完整类型在此）

// ============================================================
// 生命周期：init / resize（悬浮窗接缝原样保留）
// ============================================================

void MainUI::init(Font* font, Font* smallFont, Font* bigFont, int w, int h) {
    font_ = font;
    smallFont_ = smallFont;
    bigFont_ = bigFont;
    winW_ = w;
    winH_ = h;
    // 跨重启悬浮窗数据同步：g_layerStack/g_uiCtx 均为全局（跨 F2 切后端/分辨率
    // 切换重启保留）。若上一轮逻辑尺寸与本轮不同，按比例 rescale 全部悬浮窗
    float prevW = (float)g_uiCtx.winW, prevH = (float)g_uiCtx.winH;
    float prevScale = prevH / 600.0f;
    if (prevScale < 0.5f) prevScale = 0.5f;
    computeLayout(w, h);
    if (prevW > 0.5f && prevH > 0.5f &&
        (std::abs(prevW - (float)w) > 0.5f || std::abs(prevH - (float)h) > 0.5f)) {
        g_layerStack.rescale(prevScale, scale_, prevW, prevH, (float)w, (float)h);
    }
    // 接缝：注入全局 UI 上下文（字体/缩放/屏幕尺寸：所有窗口模块的单源数据）
    g_uiCtx.font = font;
    g_uiCtx.smallFont = smallFont;
    g_uiCtx.bigFont = bigFont;
    g_uiCtx.scale = scale_;
    g_uiCtx.winW = w;
    g_uiCtx.winH = h;
    g_winMgr.init();   // 注册全部悬浮窗处理器（配置驱动，零硬编码）
    // 注册全部页面处理器（幂等；新增页面在 RegisterAllPageHandlers 加一行）
    if (!pages_) {
        pages_ = std::make_unique<PageHandlerRegistry>();
        RegisterAllPageHandlers(*pages_);
    }
}

void MainUI::resize(int w, int h) {
    // 先保存旧尺寸/scale，供悬浮窗跟随缩放的位置比例映射
    float oldScale = scale_;
    float oldW = (float)winW_, oldH = (float)winH_;
    winW_ = w;
    winH_ = h;
    computeLayout(w, h);
    // 上下文跟随窗口尺寸（窗口缩放区域自适应由系统统一处理）
    g_uiCtx.scale = scale_;
    g_uiCtx.winW = w;
    g_uiCtx.winH = h;
    // 接缝：悬浮窗跟随主窗口缩放（尺寸按 scale 比例、位置按比例映射）
    g_layerStack.rescale(oldScale, scale_, oldW, oldH, (float)w, (float)h);
}

// ============================================================
// 框架布局：只计算 chrome 矩形（数量全部来自配置表）+ 虚拟窗几何同步
// ============================================================

void MainUI::computeLayout(int w, int h) {
    // 基础缩放（逻辑高/600，下限 0.5）× 用户全局页面缩放倍率
    float base = (float)h / 600.0f;
    if (base < 0.5f) base = 0.5f;
    scale_ = base * g_uiCtx.uiScaleUser;

    // 自绘应用标题栏占位（系统边框模式 = 0）：框架 chrome 整体下移让位；
    // "内容从哪里开始"只有 ctx.appTitleBarH 一个源
    float top = g_uiCtx.appTitleBarH;

    float mx = 20.0f * scale_;
    float innerW = w - mx * 2.0f;
    float tabH = 42.0f * scale_;
    float bottomH = 44.0f * scale_;
    float btnH = 34.0f * scale_;
    float sideW = 110.0f * scale_;

    // ---- tab 条（数量 = PAGE_COUNT，等宽分布） ----
    layout_.tabBtns.resize(PAGE_COUNT);
    float tabGap = 8.0f * scale_;
    float tabW = (innerW - tabGap * (PAGE_COUNT - 1)) / (float)PAGE_COUNT;
    float tabBtnH = tabH - 4.0f;
    for (int i = 0; i < PAGE_COUNT; i++)
        layout_.tabBtns[i] = Rect(Vec2(mx + i * (tabW + tabGap), top + 12.0f * scale_),
                                  Vec2(tabW, tabBtnH));

    // ---- 内容区（左侧栏已删；内容面板占满整宽，框架零改动） ----
    float contentY = top + 12.0f * scale_ + tabBtnH;
    float contentH = (float)h - contentY - bottomH - 8.0f * scale_;
    if (contentH < 40.0f * scale_) contentH = 40.0f * scale_;   // 极小窗兜底

    // ---- 内容面板 + 头部 icon/desc + 页面可用区 ----
    float panelX = mx;
    float panelW = innerW;
    layout_.contentPanel = Rect(Vec2(panelX, contentY), Vec2(panelW, contentH));
    layout_.contentIcon = Rect(Vec2(panelX + 16.0f * scale_, contentY + 14.0f * scale_),
                               Vec2(36.0f * scale_, 36.0f * scale_));
    layout_.contentDesc = Rect(Vec2(panelX + 60.0f * scale_, contentY + 14.0f * scale_),
                               Vec2(panelW - 76.0f * scale_, 36.0f * scale_));
    // 页面可用区：头部之下、面板内缩 20px、底部留 16px（PageEnv.content 单源）
    float areaY = contentY + 64.0f * scale_;
    layout_.contentArea = Rect(Vec2(panelX + 20.0f * scale_, areaY),
                               Vec2(panelW - 40.0f * scale_, contentH - 64.0f * scale_ - 16.0f * scale_));

    // ---- 底栏（数量 = BOTTOM_BTN_COUNT，从右向左排列，间距 110） ----
    layout_.bottomBar = Rect(Vec2(mx, h - bottomH), Vec2(innerW, bottomH));
    layout_.bottomBtns.resize(BOTTOM_BTN_COUNT);
    float btnW = 100.0f * scale_;
    for (int i = 0; i < BOTTOM_BTN_COUNT; i++) {
        float bx = (float)w - mx - 110.0f * scale_ * (float)(BOTTOM_BTN_COUNT - i);
        layout_.bottomBtns[i] = Rect(Vec2(bx, h - bottomH + 8.0f * scale_), Vec2(btnW, btnH));
    }
    layout_.themeLabel = Rect(Vec2(mx, h - bottomH + 8.0f * scale_),
                              Vec2(140.0f * scale_, btnH));

    // ---- 虚拟窗几何同步（RegionSystem 视口 = 页面可用区） ----
    vwin_.x = layout_.contentArea.pos.x;
    vwin_.y = layout_.contentArea.pos.y;
    vwin_.w = layout_.contentArea.size.x;
    vwin_.h = layout_.contentArea.size.y;
}

// ============================================================
// 页面派发辅助
// ============================================================

const PageHandler* MainUI::currentPageHandler() {
    if (!pages_ || tabSel_ < 0 || tabSel_ >= PAGE_COUNT) return nullptr;
    return pages_->find(PAGE_CONFIGS[tabSel_].handlerName);   // 配置表 → 注册表
}

PageEnv& MainUI::makeEnv(RenderDevice* dev) {
    envBuf_.ui = this;
    envBuf_.dev = dev;               // 交互期传 nullptr（页面禁止绘制）
    envBuf_.content = layout_.contentArea;
    envBuf_.dt = frameDt_;          // 本帧真实步长（update 入口采样，帧内恒定）
    return envBuf_;                  // 缓冲复用，引用直达页面处理器
}

void MainUI::switchPage(int idx) {
    if (idx == tabSel_ || idx < 0 || idx >= PAGE_COUNT) return;
    // 旧页文本输入统一提交（输入框系统单点失焦）
    g_winMgr.textInput().blurAll();
    tabSel_ = idx;
    // 新页 hover 立即清空一次（避免上一帧残留；下帧正常刷新）
    const PageHandler* now = currentPageHandler();
    if (now) now->hover(makeEnv(nullptr), kMouseAway);
    g_uiCtx.mainTabSel = tabSel_;     // 镜像即时同步（调试窗）
}

void MainUI::runBottomAction(int btnIdx) {
    if (btnIdx < 0 || btnIdx >= BOTTOM_BTN_COUNT) return;
    const char* action = BOTTOM_BTNS[btnIdx].action;
    if (!action || !action[0]) return;
    // "open:窗口id" = 打开悬浮窗（新动作类型在此扩展，框架零改动）
    if (strncmp(action, "open:", 5) == 0) {
        g_winMgr.openWindow(action + 5);
    }
}


// ============================================================
// update：输入统一路由（框架控件 → 页面处理器，系统派发优先）
// ============================================================

void MainUI::update(Input& in) {
    // 0. 帧时钟采样：真实帧间隔（秒）= 本帧动画步长。
    //    钳制 0~0.1s——下限防时钟异常、上限防断点/休眠恢复后巨量跳帧。
    //    动画由此与帧率彻底解耦（渲染只是"每帧看一眼数据状态"）
    auto nowClock = std::chrono::steady_clock::now();
    if (hasLastFrameClock_) {
        std::chrono::duration<float> d = nowClock - lastFrameClock_;
        frameDt_ = d.count();
        if (frameDt_ > 0.1f) frameDt_ = 0.1f;
        if (frameDt_ < 0.0f) frameDt_ = 0.0f;
    } else {
        frameDt_ = 0.0f;            // 首帧无前值（动画停一帧，无碍）
    }
    lastFrameClock_ = nowClock;
    hasLastFrameClock_ = true;

    // 0.5 动态背景推进（update 链唯一推进点，时间单源铁律 1.5）：
    //     render 链的 drawDynamicBg/主题工作室 drawDynBgPreview 均只读
    //     g_dynBgTime 不推进——旧版 render 链自建计时 + 预览复入双推进
    //     会让动画速度翻倍（updateDynamicBg 收毫秒）
    updateDynamicBg(frameDt_ * 1000.0f);

    Vec2 m = in.mouse();
    bool pressed = in.mousePressed(0);
    bool down = in.mouseDown(0);
    bool released = in.mouseReleased(0);

    // 1. UI 缩放变化 → resize 全链路（主界面重排 + 悬浮窗 rescale，一帧生效）
    if (lastUiScale_ != g_uiCtx.uiScaleUser) {
        lastUiScale_ = g_uiCtx.uiScaleUser;
        resize(winW_, winH_);
    }
    // 1.5 自绘应用标题栏占位变化（边框模式切换/窗口 resize）→ 布局重排：
    //     让位高变了但布局不重排会导致 tab 条与标题栏重叠/缝隙
    if (std::abs(lastBarH_ - g_uiCtx.appTitleBarH) > 0.5f) {
        lastBarH_ = g_uiCtx.appTitleBarH;
        resize(winW_, winH_);
    }

    // 2. 主界面选择状态镜像到全局单源（调试窗「输入」页展示）
    g_uiCtx.mainTabSel = tabSel_;

    // 3. 接缝：悬浮窗全部输入统一路由（球 > 面板 > 滚动条 > 内容）
    g_winMgr.handleInput(in);

    // 4. 悬浮窗遮挡判定：面板/悬浮球悬停或本帧点击已消费 → 主界面全部屏蔽
    bool fwVisible = g_layerStack.hitTest(m) != nullptr ||
                     g_layerStack.hitTestBall(m) != nullptr ||
                     g_winMgr.consumedThisFrame();

    const PageHandler* ph = currentPageHandler();

    // 5. RegionSystem 虚拟窗交互（页面滚动容器：滚轮/滚动条，系统统一处理）
    bool scrollbarPress = false;
    if (!fwVisible) {
        if (in.wheelDelta() != 0.0f) {
            // 下拉展开时优先吃滚轮（列表上下滚动），其次区域滚动，最后沙盘
            if (!g_dropdowns.handleWheel(vwin_, m, in.wheelDelta()) &&
                !g_regions.handleWheel(vwin_, m, in.wheelDelta()))
                g_sandboxes.handleWheel(vwin_, m, in.wheelDelta(), true, false);
        }
        if (pressed) scrollbarPress = g_regions.handlePress(vwin_, m);
        if (down) g_regions.handleDrag(vwin_, m);
        if (released) g_regions.endDrag();
        g_regions.updateHover(vwin_, m);
    } else {
        g_regions.endDrag();   // 遮挡时复位滚动条拖拽
    }

    // 6. hover 派发：框架控件命中 → 页面"离开清空"；否则转发当前页
    if (fwVisible) {
        hoverTab_ = hoverBottom_ = -1;
        if (ph) ph->hover(makeEnv(nullptr), kMouseAway);
    } else {
        hoverTab_ = hitRectList(layout_.tabBtns, m);
        hoverBottom_ = hitRectList(layout_.bottomBtns, m);
        bool onFrameCtl = (hoverTab_ >= 0 || hoverBottom_ >= 0);
        if (ph) ph->hover(makeEnv(nullptr), onFrameCtl ? kMouseAway : m);
    }

    // 7. 点击派发：tab → 侧栏 → 底栏 → 当前页
    //    （任何点击的输入框统一提交已在 g_winMgr.handleInput→onMousePressed
    //     的 textInput().blurAll() 完成——点击输入框本身时页面 click 会
    //     requestFocus 重新获焦，框架/页面无需自写失焦逻辑）
    if (pressed && !fwVisible && !scrollbarPress) {
        if (hoverTab_ >= 0) {
            switchPage(hoverTab_);
        } else if (hoverBottom_ >= 0) {
            runBottomAction(hoverBottom_);
        } else if (ph) {
            pageDragging_ = ph->click(makeEnv(nullptr), m);   // 消费才进入拖拽态
        }
    }

    // 8. 页面拖拽推进（click 消费后的滑条等跨帧拖动；release 复位）
    //    drag/release 为可选回调（对照悬浮窗 WindowManager::onMouseMove/
    //    onMouseReleased 的 if (h->drag) 判空纪律）：无拖拽控件的页面
    //    不实现，框架不转发——空函数指针调用即崩溃，此为概览/目录页
    //    点击控件直接崩溃的根因
    if (pageDragging_ && ph) {
        if (down && ph->drag) ph->drag(makeEnv(nullptr), m);
        if (released) {
            pageDragging_ = false;                 // 拖拽态无条件复位
            if (ph->release) ph->release(makeEnv(nullptr), m);
        }
    }

    // 9. 字符流：不经 MainUI —— TextInputSystem 在 g_winMgr.handleInput
    //    帧首统一消费（页面/悬浮窗输入框同通路，见 WindowManager::handleInput）

    // 10. F1：重置全部视觉增强参数（g_uiCtx 单源，全引擎即时生效；位掩码可关；
    //     文本编辑中豁免，防打断输入）
    if (in.keyPressed(290) && (g_uiCtx.hotkeyMask & UiContext::HOTKEY_F1_RESET) &&
        !g_winMgr.textInput().hasFocus()) {

        g_uiCtx.thicknessUser = 1.0f;
        g_uiCtx.radiusScaleUser = 1.0f;
        g_uiCtx.shadowStrength = 1.0f;
        g_uiCtx.sdfAA = true;
        g_uiCtx.baseAA = true;
        g_uiCtx.floatWinAA = true;
        g_uiCtx.checkboxShowTick = true;
        g_uiCtx.switchStyle = 0;
        g_uiCtx.switchExtra = 0;
        g_uiCtx.sliderStyle = 0;
        g_uiCtx.ballStyle = 0;
        g_uiCtx.btnStyle = 0;
        g_uiCtx.uiScaleUser = 1.0f;
        g_uiCtx.circGrad = true;
        if (g_uiCtx.renderRes != 0) {
            g_uiCtx.renderRes = 0;
            g_uiCtx.renderResDirty = true;
        }
    }
    // F2/F3：主题循环切换（位掩码单源；文本编辑中豁免防打断输入）
    if (in.keyPressed(292) && (g_uiCtx.hotkeyMask & UiContext::HOTKEY_F3_THEME) &&
        !g_winMgr.textInput().hasFocus()) {
        g_currentTheme = (g_currentTheme + 1) % THEME_COUNT;
    }
    // Esc 关闭栈顶：由 WindowManager::onKeys 统一处理（开窗改走「其他」页按钮墙）
}

// ============================================================
// render：框架 chrome → 页面内容 → 滚动条 → 悬浮窗帧尾
// ============================================================

void MainUI::render(RenderDevice* dev) {
    if (!dev) return;
    // 底级 AA（主界面层独立开关）：关 = 主界面硬边（投屏/录屏场景）
    dev->setAAEnabled(g_uiCtx.sdfAA && g_uiCtx.baseAA);
    drawBackground(dev);
    drawTabBar(dev);
    drawContentPanel(dev);
    // 当前页内容（配置表 → 注册表分发；滚动/裁剪由页面经 RegionSystem 申请）
    const PageHandler* ph = currentPageHandler();
    if (ph) ph->render(makeEnv(dev));
    // 主界面虚拟窗滚动条（系统统一绘制，与悬浮窗滚动条同款式）
    g_regions.renderScrollbars(dev, vwin_, 1.0f);
    drawBottomBar(dev);
    dev->setAAEnabled(g_uiCtx.sdfAA);   // 恢复全局（悬浮窗帧尾时序内部另有覆盖）
    // 接缝：悬浮窗帧尾时序（淡入淡出 → 待打开 → 分层渲染 → 悬浮球 → 区域收帧）；
    // dt 用本帧真实步长（update 入口采样）——淡入淡出速度与帧率解耦
    g_winMgr.frame(dev, frameDt_);
}

void MainUI::drawBackground(RenderDevice* dev) {
    ui::drawBackground(dev, g_currentTheme, winW_, winH_);
    drawDynamicBg(dev, winW_, winH_);
}

void MainUI::drawTabBar(RenderDevice* dev) {
    // 页签 = 页面配置表遍历（新增页面自动出现，框架零改动）
    for (int i = 0; i < PAGE_COUNT; i++) {
        int state = (i == tabSel_) ? pctl::STATE_ACTIVE :
                    (i == hoverTab_ ? pctl::STATE_HOVER : pctl::STATE_NORMAL);
        pctl::drawTabItem(dev, layout_.tabBtns[i], PAGE_CONFIGS[i].title, state, font_);
    }
}

void MainUI::drawContentPanel(RenderDevice* dev) {
    ThemeColors& c = currentColors();
    float bottomRadius = c.groupRadius * scale_ * g_uiCtx.radiusScale();
    Rect& cp = layout_.contentPanel;

    // 底板：底部圆角 + 阴影 + accent 淡边框
    drawBottomShadow(dev, cp, g_uiCtx.shadowBlur(), 2.0f,
                     Color(0, 0, 0, g_uiCtx.shadowAlpha()), bottomRadius);
    Color bgAlpha = Color(c.groupTitleBg.r, c.groupTitleBg.g, c.groupTitleBg.b, 220.0f / 255.0f);
    drawBottomRoundedRect(dev, cp, bgAlpha, bottomRadius);
    Color borderAlpha = Color(c.accent.r, c.accent.g, c.accent.b, 60.0f / 255.0f);
    drawBottomRoundedBorder(dev, cp, borderAlpha, g_uiCtx.borderWidth(), bottomRadius);

    // 头部 icon/desc：当前页配置（大图标 + 描述行）
    const PageConfig& pc = PAGE_CONFIGS[tabSel_];
    pctl::drawTextCentered(dev, bigFont_, pc.icon, layout_.contentIcon, c.accent);
    pctl::drawTextLeft(dev, smallFont_, pc.desc, layout_.contentDesc,
                       Color(c.text.r, c.text.g, c.text.b, 160.0f / 255.0f));
}

void MainUI::drawBottomBar(RenderDevice* dev) {
    ThemeColors& c = currentColors();

    // 主题名标签（accent 淡底 + accent 文字）
    Color themeBg = Color(c.accent.r, c.accent.g, c.accent.b, 25.0f / 255.0f);
    drawRoundedRect(dev, layout_.themeLabel, themeBg, 3.0f * g_uiCtx.radiusScale());
    pctl::drawTextLeft(dev, font_, g_themes[g_currentTheme].name.c_str(),
                       Rect(Vec2(layout_.themeLabel.pos.x + 10.0f * scale_, layout_.themeLabel.pos.y),
                            Vec2(layout_.themeLabel.size.x - 20.0f * scale_, layout_.themeLabel.size.y)),
                       c.accent);

    // 快捷键提示（主题标签与最左按钮之间）
    const Rect& tl = layout_.themeLabel;
    const Rect& firstBtn = layout_.bottomBtns[0];
    Rect infoRect = Rect(Vec2(tl.pos.x + tl.size.x + 8.0f * scale_, tl.pos.y),
                         Vec2(firstBtn.pos.x - tl.pos.x - tl.size.x - 16.0f * scale_, tl.size.y));
    pctl::drawTextLeft(dev, smallFont_, INFO_HINT, infoRect,
                       Color(c.text.r, c.text.g, c.text.b, 120.0f / 255.0f));

    // 底栏按钮：配置表遍历（primary = 渐变主按钮 / secondary = 次按钮）
    for (int i = 0; i < BOTTOM_BTN_COUNT; i++) {
        bool hover = (hoverBottom_ == i);
        if (BOTTOM_BTNS[i].primary)
            wctl::drawBtn(dev, layout_.bottomBtns[i], BOTTOM_BTNS[i].label, hover, 1.0f);
        else
            wctl::drawBtnSecondary(dev, layout_.bottomBtns[i], BOTTOM_BTNS[i].label, hover, 1.0f);
    }
}

// ============================================================
// collectText：字体图集预热（框架文案从配置表遍历，页面文案各页 collect）
// ============================================================

void MainUI::collectText(std::string& out) const {
    // 框架文案：页签标题/图标/描述（配置表遍历，加页面零维护）
    for (int i = 0; i < PAGE_COUNT; i++) {
        out += PAGE_CONFIGS[i].title;
        out += PAGE_CONFIGS[i].icon;
        out += PAGE_CONFIGS[i].desc;
    }
    // 底栏文案（配置表遍历）
    for (int i = 0; i < BOTTOM_BTN_COUNT; i++) out += BOTTOM_BTNS[i].label;
    out += INFO_HINT;
    // 各页面固定文案（处理器 collect 汇总）
    if (pages_) {
        for (int i = 0; i < PAGE_COUNT; i++) {
            const PageHandler* h = pages_->find(PAGE_CONFIGS[i].handlerName);
            if (h && h->collect) h->collect(out);
        }
    }
    // 主题名
    for (int i = 0; i < THEME_COUNT; i++) {
        out += g_themes[i].name;
    }
    // ---- 悬浮窗系统固定文案预热（原样保留，悬浮窗文字不回归） ----
    out += u8"主题工作室颜色选择器设置登录关于警告侧边设置停靠调试";
    out += u8"确定重置主题应用主题透明度日志统计主题:圆角:抗锯齿";
    out += u8"游戏1游戏2主界面点击窗口外部关闭";
    out += u8"名称语言音量中文用户用户名密码记住登录admin";
    out += u8"UI模板集合9种主题 · 纯C++ Win32 API + GDI+F2切换主题  F3切换厚度  +/-调节清晰度";
    out += u8"常规显示音频网络高级分辨率刷新率亮度主音量音效环绕声连接WiFi已连接延迟调试模式启用日志级别详细";
    out += u8"简体中文1920×1080自动更新启动页首页通知选项 A性能状态输入日志账户通用其他概览目录消息帮助";
    out += u8"FPS帧时间窗口尺寸缩放系数渐变背景开启关闭标签页侧栏调试窗口暂无日志鼠标X鼠标Y";
    out += u8"0123456789%×msHz#N·✕⚠▼▲•+-…";
    out += u8"AA:开关";
    out += u8"平铺层叠扩展应用窗口";  // 调试窗底部按钮
    out += u8"UI缩放:";              // 主题工作室 tab3 全局页面缩放滑条标签
    out += u8"悬浮球绘制:跟随系统圆润()";  // 主题工作室 tab3 悬浮球绘制下拉文案
    out += u8"悬浮球测试圆形方形点击打开测试悬浮窗标题栏按钮可把本窗收成球点击球展开样式";
    // 「其他」页悬浮窗按钮墙：模板组窗标题 + 真实功能组 + 按钮标签
    out += u8"页签侧栏项分段按钮计数器输入框复选框条形进度模板真实功能主题工作室调色板设置登录关于警告侧边设置停靠-LR停靠-TB调试打开";
}

} // namespace ui