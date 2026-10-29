// AI生成
#include "AppWindow.h"
#include "../ui/Theme.h"
#include "../ui/Element.h"
#include "../ui/page/PageControls.h"
#include "../render/gl/GLDevice.h"
#include "../render/vk/VKDevice.h"
#include "../render/ThreadScheduler.h"
#include "../core/Assets.h"
#include "../core/Log.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>
#include <cmath>

namespace ui {

// ============================================================
// AppManager 静态状态
// ============================================================
std::vector<std::unique_ptr<AppWindow>> AppManager::apps_;
AppWindow* AppManager::current_ = nullptr;

// 快捷键码（GLFW：290=F1 291=F2 292=F3 293=F4 300=F11；
// 历史语义：291=切后端 292=主题切换(MainUI 内) 293=新应用窗口）
// 位掩码位定义见 UiContext.h（UiContext::HOTKEY_*，MainUI/AppWindow 共用）
static constexpr int kKeyBackendSwitch = 291;
static constexpr int kKeyNewApp        = 293;
static constexpr int kKeyFullscreen    = 300;

// 全局渲染分辨率表（index 对应 ctx.renderRes；0=跟随窗口不查表）
static const int kResW[7] = { 0, 640, 854, 1280, 1920, 2560, 3840 };
static const int kResH[7] = { 0, 360, 480, 720, 1080, 1440, 2160 };

// 三档字号公式（单源：initFonts 初次烘焙与 refreshFonts 动态重烘焙共用，
// 两处基准不一致会造成「窗口缩放重烘焙后布局/字号错位」）：
// 名义字号 = 基准px × 逻辑高/720 × UI缩放（renderRes 强制时逻辑高查表）。
// 密度含 uiScale：UI 缩放后烘焙档位同步变，绘制侧不再顶点拉伸旧位图
static void fontPxFor(int logicH, float uiScale, int& pxNormal, int& pxBig, int& pxSmall) {
    float fontScale = logicH / 720.0f * (uiScale > 0.1f ? uiScale : 1.0f);
    if (fontScale < 0.25f) fontScale = 0.25f;
    pxNormal = (int)(28.f * fontScale + 0.5f);
    pxBig    = (int)(56.f * fontScale + 0.5f);
    pxSmall  = (int)(18.f * fontScale + 0.5f);
    if (pxNormal < 8)  pxNormal = 8;
    if (pxBig    < 16) pxBig = 16;
    if (pxSmall < 8)  pxSmall = 8;
}

// ============================================================
// 旧全局单例转发（AppFwd.h 声明的访问器）
// 语义：currentApp() 在应用栈帧内恒指向正在驱动/渲染的应用；
// 栈帧外调用属逻辑错误，以首应用兜底（不崩溃）
// ============================================================
AppWindow* currentApp() {
    AppWindow* a = AppManager::current();
    if (!a) a = AppManager::first();
    return a;
}
UiContext&     appCtx()        { return currentApp()->ctx; }
LayerStack&    appLayerStack() { return currentApp()->layerStack; }
WindowManager& appWinMgr()     { return currentApp()->winMgr; }
RegionSystem&  appRegions()    { return currentApp()->regions; }
DropdownSystem& appDropdowns() { return currentApp()->dropdowns; }
SplitterSystem& appSplitters() { return currentApp()->splitters; }
SandboxSystem& appSandboxes() { return currentApp()->sandboxes; }

bool appSuspendWindows() {
    AppWindow* a = AppManager::current();
    if (!a) a = AppManager::first();
    return a ? !a->allowSuspendWindows() : false;
}

bool appBackendIsVulkan() {
    AppWindow* a = AppManager::current();
    if (!a) a = AppManager::first();
    return a ? (a->backend() == Backend::Vulkan) : false;
}

// Vulkan 后端可用性（编译期 UI_VK_ON 单源；见 VKDevice.h）：
// 未装 Vulkan SDK 时后端为 stub，init 必失败 → 判为不可用。
bool appVulkanAvailable() {
    return UI_VK_ON != 0;
}

// ============================================================
// AppManager
// ============================================================
AppWindow* AppManager::createApp(int w, int h, const char* title, Backend backend,
                                 AppSnapshot* snap, const char* content,
                                 int parentId, bool allowSuspendWindows, int frameMode) {
    AppWindow* app = AppWindow::createApp(w, h, title, backend, snap, content,
                                          parentId, allowSuspendWindows, frameMode);
    if (app) apps_.push_back(std::unique_ptr<AppWindow>(app));
    return app;
}

void AppManager::destroyApp(AppWindow* app) {
    if (!app) return;
    // 先按指针定位（不解引用）：父级联销毁时子可能已在同帧先被销毁，
    // 宿主 toClose 列表里残留悬空指针——仅 == 比较安全，命中才继续
    bool alive = false;
    for (auto& up : apps_)
        if (up.get() == app) { alive = true; break; }
    if (!alive) return;

    int pid = app->id();
    // 父级关闭 → 级联关闭所有子窗口（递归：子窗口也可能有子）。
    // 先删子再删父（避免父销毁后子仍引用 parentId 悬空）。
    // parentId=-1（无父）不参与匹配——否则所有独立窗口会互相误判为父子。
    size_t before = apps_.size();
    for (;;) {
        AppWindow* child = nullptr;
        for (auto& up : apps_)
            if (up.get() != app && up.get()->parentId() == pid) { child = up.get(); break; }
        if (!child) break;
        destroyApp(child);   // 递归（child 可能在遍历中被 erase，故外层每轮重扫）
    }
    bool hadChildren = apps_.size() < before;
    for (auto it = apps_.begin(); it != apps_.end(); ++it) {
        if (it->get() == app) {
            if (current_ == app) current_ = nullptr;
            UI_LOG_INFO("app %d destroyed%s", app->id(), hadChildren ? " (with children)" : "");
            apps_.erase(it);   // unique_ptr 析构：应用子系统随之物理回收
            return;
        }
    }
}

AppWindow* AppManager::findById(int id) {
    for (auto& up : apps_)
        if (up.get()->id() == id) return up.get();
    return nullptr;
}

void AppManager::destroyAll() {
    current_ = nullptr;
    apps_.clear();
}

const std::vector<std::unique_ptr<AppWindow>>& AppManager::apps() { return apps_; }
AppWindow* AppManager::first() { return apps_.empty() ? nullptr : apps_.front().get(); }
int  AppManager::count() { return (int)apps_.size(); }
// 现存最大 id + 1（不能用 size()+1：中途销毁后重建会与现存 id 冲突，
// 冲突 id 会使父子绑定 findById 误命中 → 级联销毁错窗口）
int  AppManager::nextAppId() {
    int mx = 0;
    for (auto& up : apps_) mx = std::max(mx, up->id());
    return mx + 1;
}

// ============================================================
// 应用级自绘标题栏（"完全自己画 UI"）
//
// 无边框窗口（GLFW_DECORATED=0）后系统不再提供 - 口 X，全部由这里画：
//   - 位置/顺序/文案/尺寸 全在下面这张配置表里（单源，改这里即改外观）
//   - 拖动标题栏空白处 = 移动窗口；双击 = 最大化 ⇄ 还原
//   - 只走 RenderDevice 图元 + 平台窗口动作 → GL/VK 双后端零差异
//   - 内容让位：ctx.appTitleBarH 由 appBarH() 写入，MainUI 布局整体下移
// ============================================================
namespace appbar {

// ---- 布局配置（逻辑像素；"左上角位置可以自己安排和文字"的唯一入口） ----
// 按钮文案为空 = 画矢量图标；填文字 = 直接画该文字（如 "最小化"）
struct BarLayout {
    const char* minText;     // 最小化按钮文字
    const char* maxText;     // 最大化按钮文字
    const char* closeText;   // 关闭按钮文字
    float barH     = 34.0f;  // 标题栏高
    float btnW     = 30.0f;  // 按钮宽
    float btnH     = 22.0f;  // 按钮高
    float leftPad  = 10.0f;  // 左上角起始左边距（按钮组整体位置）
    float btnGap   = 4.0f;   // 按钮间距
    float textPad  = 12.0f;  // 按钮组与标题文字间距
    float textY    = 0.0f;   // 标题文字纵向偏移（0 = 居中）
    const char* titleOverride = nullptr;  // 非空 = 固定标题（不跟随窗口标题）
};
static const BarLayout kLayout = {
    "", "", "",
    34.0f, 30.0f, 22.0f, 10.0f, 4.0f, 12.0f, 0.0f, nullptr
};

// 标题栏几何（绘制/命中唯一计算点：三处调用共享同一结果，防错位）
struct Geom {
    Rect bar, minR, maxR, closeR, dragR, textR;
};
static Geom geom(float w, float s) {
    Geom g;
    const BarLayout& L = kLayout;
    float h  = L.barH * s;
    float bw = L.btnW * s, bh = L.btnH * s;
    float by = (h - bh) * 0.5f;
    float x  = L.leftPad * s;
    g.bar    = Rect(0.0f, 0.0f, w, h);
    g.minR   = Rect(x, by, bw, bh); x += bw + L.btnGap * s;
    g.maxR   = Rect(x, by, bw, bh); x += bw + L.btnGap * s;
    g.closeR = Rect(x, by, bw, bh); x += bw + L.textPad * s;
    float tw = w - x - 8.0f * s;
    g.textR  = Rect(x, L.textY * s, tw > 0.0f ? tw : 0.0f, h);
    g.dragR  = Rect(x, 0.0f, w - x, h);
    return g;
}

// ---- 矢量图标（不依赖字体字形；关闭为两条对角线的点列） ----
static void glyphMin(RenderDevice* dev, const Rect& r, const Color& c) {
    float cx = r.pos.x + r.size.x * 0.5f, cy = r.pos.y + r.size.y * 0.5f;
    float w = r.size.x * 0.34f;
    drawRect(dev, Rect(cx - w * 0.5f, cy - 1.0f, w, 2.0f), c);
}
static void glyphMax(RenderDevice* dev, const Rect& r, const Color& c) {
    float cx = r.pos.x + r.size.x * 0.5f, cy = r.pos.y + r.size.y * 0.5f;
    float s = r.size.y * 0.40f;
    drawRoundedBorder(dev, Rect(cx - s * 0.5f, cy - s * 0.5f, s, s), c, 1.4f, 1.0f);
}
static void glyphClose(RenderDevice* dev, const Rect& r, const Color& c) {
    float cx = r.pos.x + r.size.x * 0.5f, cy = r.pos.y + r.size.y * 0.5f;
    float half = r.size.y * 0.17f;
    float dot  = r.size.y * 0.045f;
    const int n = 7;
    for (int i = 0; i < n; i++) {
        float t = (float)i / (float)(n - 1) - 0.5f;
        float dx = t * half * 2.0f, dy = t * half * 2.0f;
        drawRect(dev, Rect(cx + dx - dot, cy + dy - dot, dot * 2.0f, dot * 2.0f), c);
        drawRect(dev, Rect(cx + dx - dot, cy - dy - dot, dot * 2.0f, dot * 2.0f), c);
    }
}

// 单个按钮：hover 底色 + 图标/文字
static void drawButton(RenderDevice* dev, Font* f, const Rect& r, const char* text,
                       int kind, bool hover, bool danger) {
    ThemeColors& c = currentColors();
    float rad = c.btnRadius * (r.size.y / 22.0f);
    if (rad < 1.0f) rad = 1.0f;
    if (hover)
        drawRoundedRect(dev, r, danger ? Color(c.danger.r, c.danger.g, c.danger.b, 70.0f / 255.0f)
                                       : Color(c.accent.r, c.accent.g, c.accent.b, 46.0f / 255.0f), rad);
    Color col = hover ? (danger ? (Color)c.danger : (Color)c.accent) : (Color)c.text;
    if (text && text[0]) {
        pctl::drawTextCentered(dev, f, text, r, col);
        return;
    }
    if (kind == 0)      glyphMin(dev, r, col);
    else if (kind == 1) glyphMax(dev, r, col);
    else                glyphClose(dev, r, col);
}

// ---- 贴边缩放命中（8 向；逻辑坐标）----
// 无边框窗口整块去掉系统 non-client 区，缩放边热区随之消失 → 由引擎在
// 客户区内侧留一圈「贴边带」自己接管。返回位掩码 1左 2右 4上 8下（0=无）。
// band 随 scale 缩放（下限 4px 保证小窗也好抓）；命中 - □ X 按钮时不抢，
// 保证按钮点击优先级高于顶边缩放。
static int resizeDirAt(const Vec2& m, float s, int lw, int lh, const Geom& g) {
    float band = 6.0f * s;
    if (band < 4.0f) band = 4.0f;
    const float W = (float)lw, H = (float)lh;
    int dir = 0;
    if (m.x <= band)      dir |= 1;
    if (m.x >= W - band)  dir |= 2;
    if (m.y <= band)      dir |= 4;
    if (m.y >= H - band)  dir |= 8;
    if (dir && (g.minR.contains(m) || g.maxR.contains(m) || g.closeR.contains(m))) return 0;
    return dir;
}

} // namespace appbar

// ---- AppWindow：标题栏高 / 绘制 / 命中 ----

float AppWindow::appBarH() const {
    if (!window_ || !window_->customFrame()) return 0.0f;   // 系统边框：不让位
    // 全屏隐藏开关：F11 全屏且开关开启时，自绘标题栏让位为 0（内容铺满整屏）
    if (ctx.fullscreenHideBar && window_->isFullscreen()) return 0.0f;
    int lw = 0, lh = 0;
    logicSize(lw, lh);
    float base = lh / 600.0f;                               // 与 MainUI 布局同基准
    if (base < 0.5f) base = 0.5f;
    float s = base * (ctx.uiScaleUser > 0.1f ? ctx.uiScaleUser : 1.0f);
    return appbar::kLayout.barH * s;
}

void AppWindow::drawAppTitleBar(RenderDevice* dev) {
    if (!appBarH()) return;
    int lw = 0, lh = 0;
    logicSize(lw, lh);
    float base = lh / 600.0f;
    if (base < 0.5f) base = 0.5f;
    float s = base * (ctx.uiScaleUser > 0.1f ? ctx.uiScaleUser : 1.0f);
    appbar::Geom g = appbar::geom((float)lw, s);
    ThemeColors& c = currentColors();

    // 底板 + 底边分隔线（主题色派生，零裸色）
    // 部分主题 bg1==bg2（默认回退主题）→ 标题条与背景同色看不出边界，
    // 此处以 8% 压暗兜底，保证任何主题下标题条都可见
    float dr = c.bg2.r, dg = c.bg2.g, db = c.bg2.b;
    if (std::fabs(dr - c.bg1.r) + std::fabs(dg - c.bg1.g) + std::fabs(db - c.bg1.b) < 18.0f) {
        dr *= 0.92f; dg *= 0.92f; db *= 0.92f;
    }
    drawRect(dev, g.bar, Color(dr, dg, db, 246.0f / 255.0f));
    drawRect(dev, Rect(0.0f, g.bar.size.y - 1.0f, (float)lw, 1.0f),
             Color(c.groupBorder.r, c.groupBorder.g, c.groupBorder.b, 170.0f / 255.0f));

    Vec2 m = input_.mouse();
    appbar::drawButton(dev, smallFont_.get(), g.minR, appbar::kLayout.minText, 0, g.minR.contains(m), false);
    appbar::drawButton(dev, smallFont_.get(), g.maxR, appbar::kLayout.maxText, 1, g.maxR.contains(m), false);
    appbar::drawButton(dev, smallFont_.get(), g.closeR, appbar::kLayout.closeText, 2, g.closeR.contains(m), true);

    const char* title = appbar::kLayout.titleOverride ? appbar::kLayout.titleOverride : title_.c_str();
    if (title && title[0] && g.textR.size.x > 4.0f)
        pctl::drawTextLeft(dev, font_.get(), title, g.textR, (Color)c.text);
}

// 自绘标题栏原生命中裁定（挂到 Window::setHitTest，经 WM_NCHITTEST 生效）：
// 客户端像素坐标 → HT 码。命中 - □ X 按钮 / 贴边缩放带 → HTCLIENT（交引擎
// 自绘处理，保留按钮点击与精准缩放）；栏内其余空白 → HTCAPTION（交系统
// 原生拖动，跟随 Windows）。栏不可见（系统边框 / 全屏隐藏）→ 0 不干预。
int AppWindow::appChromeHitTest(int cx, int cy) const {
    float barH = appBarH();
    if (barH <= 0.5f) return 0;
    int lw = 0, lh = 0;
    logicSize(lw, lh);
    float base = lh / 600.0f;
    if (base < 0.5f) base = 0.5f;
    float s = base * (ctx.uiScaleUser > 0.1f ? ctx.uiScaleUser : 1.0f);
    // 物理像素 → 逻辑坐标（与自绘命中/布局同一坐标系）
    float lx = devSx_ > 0.001f ? (float)cx / devSx_ : (float)cx;
    float ly = devSy_ > 0.001f ? (float)cy / devSy_ : (float)cy;
    if (ly < 0.0f || ly >= barH) return 0;                 // 不在自绘栏内：不干预
    appbar::Geom g = appbar::geom((float)lw, s);
    const Vec2 m(lx, ly);
    // - □ X 按钮：HTCLIENT → 自绘点击（与拖动分离，互不抢）
    if (g.minR.contains(m) || g.maxR.contains(m) || g.closeR.contains(m)) return 1;
    // 贴边缩放带（含顶边）：HTCLIENT → 引擎自绘 8 向缩放（保持「精准」手感）
    float band = 6.0f * s;
    if (band < 4.0f) band = 4.0f;
    if (m.x <= band || m.x >= (float)lw - band || m.y <= band) return 1;
    return 2;   // HTCAPTION → 系统原生拖动（Follow Windows）
}

bool AppWindow::handleAppChrome() {
    if (!appBarH()) return false;
    int lw = 0, lh = 0;
    logicSize(lw, lh);
    float base = lh / 600.0f;
    if (base < 0.5f) base = 0.5f;
    float s = base * (ctx.uiScaleUser > 0.1f ? ctx.uiScaleUser : 1.0f);
    appbar::Geom g = appbar::geom((float)lw, s);

    const Vec2 m = input_.mouse();
    const bool pressed  = input_.mousePressed(0);
    const bool released = input_.mouseReleased(0);

    // ---- 贴边缩放命中（自绘模式：无系统边框后由引擎接管 8 向缩放）----
    const int rz = appbar::resizeDirAt(m, s, lw, lh, g);

    // 缩放进行中：绝对屏坐标锚定（同拖动，防自激振荡）→ setPos + setSize
    if (barResizeDir_ != 0) {
        if (released) { barResizeDir_ = 0; barResizeValid_ = false; return true; }
        int wx = 0, wy = 0;
        windowPos(wx, wy);
        int ww = window_->width(), wh = window_->height();
        // 目标矩形按「按下时刻」锚定 + 绝对屏位移（窗口自身移动被抵消）
        float dx = (wx + m.x * barResizeDevX_) - (barResizeWinX_ + barResizeMouseX_ * barResizeDevX_);
        float dy = (wy + m.y * barResizeDevY_) - (barResizeWinY_ + barResizeMouseY_ * barResizeDevY_);
        int nx = barResizeWinX_, ny = barResizeWinY_, nw = barResizeWinW_, nh = barResizeWinH_;
        int adx = (int)std::lround(dx), ady = (int)std::lround(dy);
        if (barResizeDir_ & 1) { nx = barResizeWinX_ + adx; nw = barResizeWinW_ - adx; }
        if (barResizeDir_ & 2) { nw = barResizeWinW_ + adx; }
        if (barResizeDir_ & 4) { ny = barResizeWinY_ + ady; nh = barResizeWinH_ - ady; }
        if (barResizeDir_ & 8) { nh = barResizeWinH_ + ady; }
        // 最小尺寸钳制（按住的对边保持不动）
        int minW = (int)std::lround(480.0f * barResizeDevX_);
        int minH = (int)std::lround(300.0f * barResizeDevY_);
        if (nw < minW) { if (barResizeDir_ & 1) nx = barResizeWinX_ + (barResizeWinW_ - minW); nw = minW; }
        if (nh < minH) { if (barResizeDir_ & 4) ny = barResizeWinY_ + (barResizeWinH_ - minH); nh = minH; }
        // 1px 死区：无实质变化不下发（避免每帧触发 Resize 重排）
        if (!barResizeValid_ || nx != barResizeLastX_ || ny != barResizeLastY_ ||
            nw != barResizeLastW_ || nh != barResizeLastH_) {
            barResizeLastX_ = nx; barResizeLastY_ = ny;
            barResizeLastW_ = nw; barResizeLastH_ = nh; barResizeValid_ = true;
            window_->setPos(nx, ny);
            if (nw != ww || nh != wh) window_->setSize(nw, nh);
        }
        return true;
    }

    if (pressed) {
        // 贴边优先于按钮/拖动：四角四边可缩放（命中按钮时 resizeDirAt 已让位）
        if (rz != 0) {
            barResizeDir_    = rz;
            barResizeMouseX_ = m.x;  barResizeMouseY_ = m.y;
            windowPos(barResizeWinX_, barResizeWinY_);
            barResizeWinW_   = window_->width();
            barResizeWinH_   = window_->height();
            barResizeDevX_   = devSx_; barResizeDevY_ = devSy_;
            barResizeValid_  = false;
            return true;
        }
        if (g.closeR.contains(m)) { requestClose();            return true; }
        if (g.minR.contains(m))   { window_->minimize();       return true; }
        if (g.maxR.contains(m))   { window_->toggleMaximize(); return true; }
        }
    // 拖动 / 双击最大化：已改由 Windows 系统原生接管（Window::setHitTest →
    // WM_NCHITTEST 返 HTCAPTION，系统走原生移动循环，自带 Aero 贴靠与
    // 拖到顶最大化）——不再自写一套拖动状态机。
    // hover 光标反馈：贴边/贴角显示缩放光标（精准手感，双后端一致）
    window_->setResizeCursor(rz);
    return g.bar.contains(m) || rz != 0;   // 标题栏/贴边点击不落到内容层
}

// ============================================================
// AppWindow 生命周期
// ============================================================
AppWindow::AppWindow(int id, int w, int h, const char* title, Backend backend,
                     int parentId, bool allowSuspendWindows, int frameMode)
    : id_(id), backend_(backend), title_(title ? title : ""),
      parentId_(parentId), allowSuspendWindows_(allowSuspendWindows) {
    // 边框模式在建窗瞬间定格（GLFW_DECORATED 无法运行期修改）：
    // 0=系统标题栏 1=无边框自绘（- 口 X 由 AppWindow 应用标题栏画）
    ctx.windowFrame = (frameMode != 0) ? 1 : 0;
    window_ = std::make_unique<Window>(w, h, title_.c_str(),
                                       backend_ == Backend::Vulkan,
                                       ctx.windowFrame != 0);
    // 自绘标题栏「原生拖动」：把 WM_NCHITTEST 裁定挂到平台窗口——命中标题栏
    // 空白返回 HTCAPTION，交 Windows 走原生移动循环（跟随系统：Aero 贴靠、
    // 拖到顶最大化）；按钮/贴边仍返 HTCLIENT 由引擎自绘处理。
    window_->setHitTest([this](int cx, int cy) { return appChromeHitTest(cx, cy); });
}

AppWindow::~AppWindow() {
    // 有序析构（与声明序无关，显式控制）：
    // 内容视图先死（不再产生绘制）→ 字体图集（GL 纹理，析构会调
    // dev_->destroyTexture，设备必须存活且上下文 current——旧序
    // device 先死导致 Font 持悬空 dev_ 虚调用，堆布局变化后崩溃）
    // → 设备释放（GL 资源归属本应用上下文）→ 平台窗口（上下文载体最后死）
    if (content_ && content_->shutdown && contentData_) content_->shutdown(this);
    contentData_ = nullptr;
    if (backend_ == Backend::OpenGL && window_ && window_->valid())
        glfwMakeContextCurrent(window_->handle());
    font_.reset(); smallFont_.reset(); bigFont_.reset();
    if (device_) {
        device_->shutdown();
        // 释放该设备的帧包池（前提：宿主结构段 pause 内销毁——无
        // in-flight 包且渲染线程不持 GL 上下文，纯 map erase 安全；
        // 不释放则设备指针作 key 悬挂，后续同地址新设备会误用旧池）
        ThreadScheduler::get().releasePool(device_.get());
        device_.reset();
    }
    window_.reset();
    glfwMakeContextCurrent(nullptr);
}

void AppWindow::logicSize(int& lw, int& lh) const {
    lw = window_->width();
    lh = window_->height();
    if (ctx.renderRes > 0 && ctx.renderRes < 7) {
        lw = kResW[ctx.renderRes];
        lh = kResH[ctx.renderRes];
    }
}

void AppWindow::initDevice() {
    if (backend_ == Backend::Vulkan) {
        auto vk = std::make_unique<VKDevice>();
        if (vk->init(window_->nativeHandle(), window_->width(), window_->height())) {
            device_ = std::move(vk);
            return;
        }
        UI_LOG_WARN("Vulkan init failed, falling back to OpenGL");
        backend_ = Backend::OpenGL;
    }
    auto gl = std::make_unique<GLDevice>();
    if (!gl->init(window_->nativeHandle(), window_->width(), window_->height())) {
        UI_LOG_ERROR("app %d: device init failed", id_);
        return;
    }
    device_ = std::move(gl);
}

void AppWindow::initFonts() {
    // 字号等比系数：按逻辑高（renderRes 强制时查表）/720 基准。
    // 布局随 scale=h/600 展开，字号须同比例烘焙，否则文字相对控件失调。
    // 快照恢复时 ctx.renderRes 为上一轮值——与重建窗口生效语义一致。
    // （公式抽 fontPxFor 单源：窗口缩放后 refreshFonts 用同一公式重烘焙）
    int lw = 0, lh = 0;
    logicSize(lw, lh);
    int pxNormal = 0, pxBig = 0, pxSmall = 0;
    fontPxFor(lh, ctx.uiScaleUser, pxNormal, pxBig, pxSmall);

    font_ = std::make_unique<Font>();
    // assetPath：相对资源根（exe 目录）解析——CWD 非资源根时同样可加载
    if (font_->loadFromFile(assetPath("assets/fonts/SourceHanSansSC-Regular.otf").c_str(), (float)pxNormal)) {
        if (!font_->init(device_.get()))
            UI_LOG_WARN("app %d: default font init failed; text invisible", id_);
    } else {
        UI_LOG_WARN("app %d: no regular font; text invisible", id_);
    }

    bigFont_ = std::make_unique<Font>();
    if (!bigFont_->loadFromFile(assetPath("assets/fonts/SourceHanSansSC-Bold.otf").c_str(), (float)pxBig) ||
        !bigFont_->init(device_.get())) {
        UI_LOG_WARN("app %d: no bold font; big titles fall back to regular %dpx", id_, pxBig);
        bigFont_->loadFromFile(assetPath("assets/fonts/SourceHanSansSC-Regular.otf").c_str(), (float)pxBig);
        bigFont_->init(device_.get());
    }

    smallFont_ = std::make_unique<Font>();
    if (smallFont_->loadFromFile(assetPath("assets/fonts/SourceHanSansSC-Regular.otf").c_str(), (float)pxSmall)) {
        if (!smallFont_->init(device_.get()))
            UI_LOG_WARN("app %d: small font init failed; text invisible", id_);
    } else {
        UI_LOG_WARN("app %d: no small font; small text uses regular", id_);
    }
    // 烘焙密度已含当前 UI 缩放：对齐检测基准（否则快照恢复 uiScale≠1 时
    // 首帧误判变化触发一次多余重烘焙）
    lastUiScaleApp_ = ctx.uiScaleUser;
}

void AppWindow::refreshFonts(bool force) {
    // 窗口缩放 / UI 缩放变化后重烘焙三档字体（修「放大模糊」根因）：
    // 字号只按创建时逻辑高烘焙一次 → 放大后顶点拉伸旧小位图 → 糊
    // （典型：480x320 子窗只烘 12px，拉大到 720 高 = 2.25 倍拉伸）。
    // 重烘焙 = 新密度 repack + 重上传，字集保留（含动态预热的中文字符）；
    // 布局零适配：名义 px 与布局 scale_ 同比增长，measureText 自动跟随，
    // 悬浮窗 drawTextScaled 期望高度恒定（分母 pixelHeight 同步补偿）。
    // rebake 后 renderScale 复位 1：Resize 期间的顶点补偿态交还烘焙密度。
    if (!device_) return;
    int lw = 0, lh = 0;
    logicSize(lw, lh);
    int pxN = 0, pxB = 0, pxS = 0;
    fontPxFor(lh, ctx.uiScaleUser, pxN, pxB, pxS);
    // 逐档重建（差值阈值 ≥2px 防抖：1px 级窗口抖动不触发 pack+上传开销；
    // force 绕防抖——UI 缩放离散提交单次调用，微小档位差也须烘焙到位）
    struct { std::unique_ptr<Font>* f; int px; const char* tag; } tiers[3] = {
        { &font_, pxN, "normal" }, { &bigFont_, pxB, "big" }, { &smallFont_, pxS, "small" }
    };
    for (auto& t : tiers) {
        if (!t.f || !*t.f || !(*t.f)->isReady()) continue;
        int oldPx = (int)(*t.f)->pixelHeight();
        if (!force && std::abs(t.px - oldPx) < 2) continue;
        if ((*t.f)->rebake(device_.get(), (float)t.px)) {
            (*t.f)->setRenderScale(1.0f);
            UI_LOG_INFO("app %d: font %s rebaked %dpx -> %dpx (logic h=%d)",
                        id_, t.tag, oldPx, t.px, lh);
        }
    }
}


AppWindow* AppWindow::createApp(int w, int h, const char* title, Backend backend,
                                AppSnapshot* snap, const char* content,
                                int parentId, bool allowSuspendWindows, int frameMode) {
    InitThemes();   // 幂等：主题全局一次（重复调用安全）

    auto app = std::unique_ptr<AppWindow>(
        new AppWindow(AppManager::nextAppId(), w, h, title, backend,
                      parentId, allowSuspendWindows, frameMode));
    if (!app->window_ || !app->window_->valid()) {
        UI_LOG_ERROR("app: window creation failed");
        return nullptr;
    }
    AppManager::setCurrent(app.get());   // 创建期栈帧：单例宏指向本应用

    app->initDevice();
    if (!app->device_) return nullptr;

    // 内容处理器注册表（内置 "main" 等；未指定默认复用主绘制）
    registerBuiltinAppContents();
    app->content_ = findAppContent(content);
    if (!app->content_ || !app->content_->init) {
        UI_LOG_ERROR("app %d: unknown content handler '%s'", app->id_,
                     content ? content : "");
        return nullptr;
    }

    // 快照恢复（内容 init 前：ctx.winW/winH 旧值用作 rescale 基准）
    if (snap && snap->valid) app->applySnapshot(std::move(*snap));

    // 逻辑渲染尺寸 → 设备渲染目标（真·分辨率降档：离屏 1:1 光栅化 +
    // present 整体缩放 blit；devSx_/devSy_ 仅剩鼠标物理→逻辑转换用途）
    int lw = 0, lh = 0;
    app->logicSize(lw, lh);
    app->devSx_ = app->window_->width() / (float)lw;
    app->devSy_ = app->window_->height() / (float)lh;
    app->device_->setTargetSize(lw, lh);

    app->initFonts();

    // 自绘标题栏占位高：必须在内容 init 之前写入——MainUI 布局按
    // ctx.appTitleBarH 整体下移让位（系统边框模式下为 0，行为不变）
    app->ctx.appTitleBarH = app->appBarH();

    // 内容处理器初始化（"main"=创建 MainUI：内部注入 ctx/注册处理器/rescale）
    app->content_->init(app.get());

    // 布局记忆：默认关闭（rememberLayout=false）；仅首应用且无快照时恢复
    // （F2/分辨率重启路径 layerStack 已从快照恢复，跳过避免重复创建）
    if (app->id_ == 1 && app->ctx.rememberLayout && app->layerStack.allWindows().empty()) {
        int restored = app->layerStack.loadLayout("layout.json", (float)lw, (float)lh);
        if (restored > 0) UI_LOG_INFO("app %d: layout restored %d windows", app->id_, restored);
    }

    UI_LOG_INFO("app %d ready: content=%s, backend=%s, size=%dx%d (logic %dx%d)",
                app->id_, content ? content : "main",
                app->backend_ == Backend::Vulkan ? "Vulkan" : "OpenGL",
                app->window_->width(), app->window_->height(), lw, lh);
    return app.release();   // AppManager::createApp 接管（push_back）
}

void AppWindow::applySnapshot(AppSnapshot&& s) {
    if (!s.valid) return;
    ctx        = s.ctx;                     // 视觉参数 + 旧尺寸（rescale 基准）
    layerStack = std::move(s.layerStack);   // 悬浮窗栈整体迁移
    regions    = std::move(s.regions);      // 区域滚动状态迁移
    dropdowns  = std::move(s.dropdowns);    // 下拉展开/滚动状态迁移
    splitters  = std::move(s.splitters);    // 分割条 hover/拖拽状态迁移
    sandboxes  = std::move(s.sandboxes);    // 沙盘 zoom/pan 状态迁移
    s.valid = false;
}

AppSnapshot AppWindow::takeSnapshot() {
    AppSnapshot s;
    s.valid = true;
    s.layerStack = std::move(layerStack);   // 移空（本应用随即销毁）
    s.regions    = std::move(regions);
    s.dropdowns  = std::move(dropdowns);
    s.splitters  = std::move(splitters);
    s.sandboxes  = std::move(sandboxes);
    s.ctx        = ctx;
    if (window_) { s.winW = window_->width(); s.winH = window_->height(); }
    // 主子级属性迁移：内容处理器名 / 悬浮窗许可 / 标题（重启保持同语义）
    s.content             = content_ ? content_->name : "main";
    s.allowSuspendWindows = allowSuspendWindows_;
    s.title               = title_;
    // s.parentIdx 由宿主打快照时填（需容器下标，本类不知自身在容器中的位置）
    return s;
}

void AppWindow::setWindowPos(int x, int y) {
    if (window_ && window_->valid()) glfwSetWindowPos(window_->handle(), x, y);
}

void AppWindow::windowPos(int& x, int& y) const {
    x = 0; y = 0;
    if (window_ && window_->valid()) glfwGetWindowPos(window_->handle(), &x, &y);
}

// ============================================================
// 帧驱动（单线程逐应用；currentApp 在本栈帧内恒为本应用）
// ============================================================
void AppWindow::frame() {
    // contentData_ 不作前提：纯静态内容处理器（如 "test"）无私有态，
    // contentData 恒空也照常渲染——生命周期由 init 成功即有效保证
    if (closeRequested_ || !content_ || !content_->render) return;
    AppManager::setCurrent(this);
    auto frameStart = std::chrono::steady_clock::now();

    // 请求标记帧首复位（电平语义只保证「本帧内设置→本帧帧末被宿主处理」；
    // 不复位的话 requestNewApp 这类电平请求会每帧恒 true → 无限新建窗口 bug）
    newAppReq_.valid = false;
    backendSwitch_ = false;
    resRestart_ = false;
    frameRestart_ = false;

    // ---- 1. 事件泵 + 输入状态机 ----
    // 注：glfwPollEvents 全局泵已由宿主主循环统一调用（Window::pollAll），
    // 每窗口这里只取走自己的队列（drainEvents），避免 N 窗口 N 次空泵。
    // 性能优化：事件缓冲成员复用（events_ 声明见 AppWindow.h，
    // clear() 保容量，每帧零堆分配）
    events_.clear();
    window_->drainEvents(events_);
    input_.beginFrame();
    for (auto& e : events_) {
        if (e.kind == EventKind::Resize) {
            device_->resize(e.width, e.height);
            // 逻辑尺寸不变（强制分辨率时固定），渲染目标尺寸同步更新
            int nlw = 0, nlh = 0;
            logicSize(nlw, nlh);
            devSx_ = e.width / (float)nlw;
            devSy_ = e.height / (float)nlh;
            device_->setTargetSize(nlw, nlh);
            // 自绘标题栏占位高随逻辑高重算：必须在内容 resize 之前写入，
            // 否则 MainUI 按旧占位布局（自绘模式下顶栏与内容会错位）
            ctx.appTitleBarH = appBarH();
            // 字号即时顶点补偿（修「resize 文字延迟放大」）：重烘焙有 250ms
            // 去抖，期间主界面文字 = pixelHeight × renderScale —— renderScale
            // 不补偿则文字保持旧大小、布局已变大，松手重烘焙后才突然放大。
            // 现按新逻辑高即时算目标档位/当前档位比例写入 renderScale：
            // 文字大小立刻正确（位图密度暂糊），静止后 refreshFonts 换高清
            // 位图并复位 renderScale=1（大小连续无跳变）。悬浮窗文字
            // drawTextScaled 分母含 renderScale 自动约掉，不受影响。
            {
                int pxN = 0, pxB = 0, pxS = 0;
                fontPxFor(nlh, ctx.uiScaleUser, pxN, pxB, pxS);
                if (font_ && font_->isReady() && font_->pixelHeight() > 0.f)
                    font_->setRenderScale(pxN / font_->pixelHeight());
                if (bigFont_ && bigFont_->isReady() && bigFont_->pixelHeight() > 0.f)
                    bigFont_->setRenderScale(pxB / bigFont_->pixelHeight());
                if (smallFont_ && smallFont_->isReady() && smallFont_->pixelHeight() > 0.f)
                    smallFont_->setRenderScale(pxS / smallFont_->pixelHeight());
            }
            // 字体待重烘焙（跟随窗口模式逻辑高变化 → 字号密度需跟上；
            // 不即时重建：拖拽边缘 Resize 每帧触发，去抖见渲染前统一处理）
            fontsDirty_ = true;
            lastResizeT_ = frameStart;
            if (content_->resize) content_->resize(this, nlw, nlh);
        } else if (e.kind == EventKind::MouseMove ||
                   e.kind == EventKind::MouseDown ||
                   e.kind == EventKind::MouseUp ||
                   e.kind == EventKind::MouseWheel) {
            // 鼠标实际像素 → 逻辑坐标（UI 布局/命中测试均在逻辑坐标系）
            e.mouse.x /= devSx_;
            e.mouse.y /= devSy_;
        }
        input_.apply(e);
    }

    // ---- 2. 生命周期请求检测（帧末由宿主统一处理） ----
    if (window_->shouldClose() || input_.closeRequested()) {
        closeRequested_ = true;
        return;
    }
    if (ctx.renderResDirty) {
        ctx.renderResDirty = false;   // 重启窗口生效（本应用快照重启）
        resRestart_ = true;
        return;
    }
    // 快捷键位掩码单源（UiContext.hotkeyMask）：清位 = 该键失效（嵌入场景用）
    if (input_.keyPressed(kKeyBackendSwitch) && (ctx.hotkeyMask & UiContext::HOTKEY_F2_BACKEND)) {
        // 目标后端可达性判定（系统层单源 appVulkanAvailable）：
        // 本机仅 OpenGL 可用（未编译 Vulkan）时 F2 无对手可切——若强行切会
        // 建出 Vulkan 窗口（无 GL 上下文），initDevice 回退 GL 时 gladLoadGL
        // 失败 → 设备为空 → 崩溃。故目标不可达即忽略本次 F2。
        if (appVulkanAvailable()) {
            backendSwitch_ = true;    // 全局动作：宿主全部应用快照重启
        } else {
            UI_LOG_WARN("F2 backend switch ignored: Vulkan backend not built (no SDK)");
        }
        return;
    }
    if (input_.keyPressed(kKeyFullscreen) && (ctx.hotkeyMask & UiContext::HOTKEY_F11_FULLSCREEN)) {
        window_->toggleFullscreen();  // F11 全屏（Resize 事件驱动后续适配）
    }
    if (input_.keyPressed(kKeyNewApp) && (ctx.hotkeyMask & UiContext::HOTKEY_F4_NEW_APP)) {
        // F4 新应用窗口（帧末创建，遍历安全）：默认请求 = 独立 main 窗口
        requestNewApp();
    }

    // 边框模式切换（其他页开关置 dirty）：GLFW_DECORATED 只在建窗瞬间生效，
    // 运行期改不了样式 → 通知宿主快照重启全部应用（与 F2 同一条迁移链路，
    // 悬浮窗布局不丢；GL/VK 双后端共用同一建窗通路，切换后同样生效）
    if (ctx.windowFrameDirty) {
        ctx.windowFrameDirty = false;
        frameRestart_ = true;
        return;
    }

    // 顶部让位高单源即时同步（边框模式 / 全屏隐藏开关 / 缩放变化 都在此归一）：
    // MainUI 检测到 appTitleBarH 变化自行重排，保证开关一开一关立即生效
    ctx.appTitleBarH = appBarH();

    // ---- 3. 逻辑更新（内部：悬浮窗统一输入路由 g_winMgr.handleInput） ----
    // 自绘标题栏先消费（- 口 X / 拖动 / 双击最大化）：它压在内容区上方，
    // 系统 chrome 优先于内容命中（与悬浮窗标题栏同语义）
    handleAppChrome();

    // ---- 3.5 应用窗口圆角（自绘模式）：Unicode「圆润值」落地 ----
    // 用 Window::setRoundCorner（Windows = SetWindowRgn）把无边框窗口外角
    // 按 ctx.windowRound 裁圆；区域按窗口坐标，尺寸变化后失效 → 值/宽高/
    // 最大化态任一变化都重下发；最大化与全屏时清空（贴屏幕边不裁圆角）。
    if (window_->customFrame()) {
        int wpx = window_->width(), hpx = window_->height();
        int rpx = 0;
        if (!window_->isMaximized() && !window_->isFullscreen()) {
            rpx = (int)std::lround(UiContext::WINDOW_ROUND_BASE * ctx.windowRound * devSx_);
            if (rpx < 0) rpx = 0;
            if (rpx > hpx / 2) rpx = hpx / 2;
        }
        if (rpx != lastRoundPx_ || wpx != lastRoundW_ || hpx != lastRoundH_) {
            lastRoundPx_ = rpx; lastRoundW_ = wpx; lastRoundH_ = hpx;
            window_->setRoundCorner(rpx);
        }
    }
    if (content_->update) content_->update(this, input_);

    // ---- 4. 渲染（多线程：录制帧包交渲染线程回放；单线程：直通） ----
    // GL 上下文归属：多线程模式归渲染线程（replay 内 makeCurrent、
    // 用毕归还），计算线程只录制（设备录制分支零 GL/VK 调用）；
    // 单线程直通维持现状（主线程自持上下文）
    ThreadScheduler& rt = ThreadScheduler::get();
    FramePacket* pkt = nullptr;
    if (rt.multithread()) {
        pkt = rt.acquire(device_.get());     // 双缓冲全忙阻塞 = 自然反压（流水线 ≤2 帧）
        device_->setRecording(pkt);          // 进入录制模式（后续接口只写包）
    } else if (backend_ == Backend::OpenGL) {
        glfwMakeContextCurrent(window_->handle());
    }

    // 字体重烘焙（Resize 去抖 250ms：拖拽中持续刷新 lastResizeT_ 不重建、
    // 拖拽流畅旧图集暂糊，停止 250ms 后本帧一次重建转清晰；多线程录制
    // 模式下 rebake 的纹理创建/销毁走设备录制分支，主线程零 GL/VK 调用）
    // UI 缩放变化 → 立即 force 重烘焙（修「UI 缩放字体没有缩放」）：
    // 字号密度已含 uiScale（fontPxFor 单源），松手提交当帧烘焙到位——
    // 旧实现密度不含 uiScale，UI 缩放只靠绘制侧顶点拉伸旧位图 → 糊且
    // drawTextRight 漏传缩放档不跟随；离散单次提交无拖拽高频问题，
    // 不走去抖直接一次烘焙。多线程录制模式下 rebake 纹理命令进帧包。
    if (lastUiScaleApp_ != ctx.uiScaleUser) {
        lastUiScaleApp_ = ctx.uiScaleUser;
        refreshFonts(true);
        fontsDirty_ = false;   // 已按最新 lh+uiScale 烘焙，Resize 待办作废
    }
    if (fontsDirty_ &&
        std::chrono::steady_clock::now() - lastResizeT_ >= std::chrono::milliseconds(250)) {
        refreshFonts();
        fontsDirty_ = false;
    }

    device_->beginFrame();
    device_->clear(currentColors().bg1);

    // 字体图集预热（collectText 拼接当帧全部文案 → ensureGlyphs 按需烘焙）。
    // 性能优化：allText_ 成员复用（clear 保容量），每帧零堆分配——
    // 旧实现每帧局部构造 string，拼接增长多次 realloc
    allText_.clear();
    if (content_->collectText) content_->collectText(this, allText_);
    // 自绘标题文字同帧预热（否则首帧标题缺字）
    if (appBarH() > 0.0f && !title_.empty()) allText_ += title_;
    if (!allText_.empty()) {
        if (font_ && font_->isReady()) font_->ensureGlyphs(device_.get(), allText_.c_str());
        if (smallFont_ && smallFont_->isReady()) smallFont_->ensureGlyphs(device_.get(), allText_.c_str());
        if (bigFont_ && bigFont_->isReady()) bigFont_->ensureGlyphs(device_.get(), allText_.c_str());
    }

    content_->render(this, device_.get());   // 内部：主界面绘制 + g_winMgr.frame 帧尾时序

    // 自绘应用标题栏（无边框模式）：画在内容之后——主界面背景是全画布铺满，
    // 先画会被盖掉；内容已按 ctx.appTitleBarH 下移，标题栏压在最上层
    // （系统 chrome 语义：与悬浮窗标题栏一致，永远是窗口最上层）
    drawAppTitleBar(device_.get());

    device_->endFrame();
    device_->present();     // 录制模式：载荷打包移交（VK）/ Present 命令入流（GL）

    if (pkt) {
        device_->setRecording(nullptr);      // 结束录制（设备恢复直通态）
        rt.submit(device_.get(), pkt);       // 渲染线程按提交序回放（多窗口串行）
    }

    // ---- 5. FPS / 帧耗时统计（写入本应用 ctx，调试窗读取展示） ----
    frameCount_++;
    auto nowT = std::chrono::steady_clock::now();
    if (nowT - lastFpsTime_ >= std::chrono::seconds(1)) {
        ctx.fps = frameCount_;
        // 每秒分布一行日志（avg/min/max）：波动尖峰（字体图集重烘焙/
        // 窗口事件/DWM 合成抖动）靠 max 帧定位，avg 判稳态负载
        if (secN_ > 0)
            UI_LOG_INFO("perf: app %d fps=%d avg=%.2fms min=%.2fms max=%.2fms",
                        id_, ctx.fps, secSumMs_ / secN_, secMinMs_, secMaxMs_);
        secMinMs_ = 1e9f; secMaxMs_ = 0.f; secSumMs_ = 0.f; secN_ = 0;
        frameCount_ = 0;
        lastFpsTime_ = nowT;
    }
    std::chrono::duration<float, std::milli> fms = nowT - frameStart;
    ctx.frameMs = fms.count();
    // 尖峰帧（>100ms：烘焙/切换/驱动抖动）单独成行，且不计入每秒 avg
    // ——旧口径 avg 混入跨秒长帧后与 fps 自相矛盾（fps=60 但 avg=43ms）
    if (ctx.frameMs > 100.f)
        UI_LOG_WARN("perf spike: %.1fms (app %d)", ctx.frameMs, id_);
    else {
        secSumMs_ += ctx.frameMs;
        secN_++;
    }
    if (ctx.frameMs < secMinMs_) secMinMs_ = ctx.frameMs;
    if (ctx.frameMs > secMaxMs_) secMaxMs_ = ctx.frameMs;
    ctx.frameMsHist[ctx.frameMsHead] = ctx.frameMs;
    ctx.frameMsHead = (ctx.frameMsHead + 1) % 64;
    if (ctx.frameMsCount < 64) ctx.frameMsCount++;
    // 鼠标位置镜像：单源在 WindowManager::handleInput 帧首（g_uiCtx.mouseX/Y），
    // 此处不再双写（旧双写两处值恒等，属冗余）
}

} // namespace ui