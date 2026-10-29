#include "AppContent.h"
#include "AppWindow.h"
#include "../ui/MainUI.h"
#include "../ui/Theme.h"
#include "../ui/win/WinControls.h"
#include <map>

namespace ui {

// ============================================================
// 注册表（进程全局：处理器纯函数，私有态在 app->contentData()）
// ============================================================
static std::map<std::string, AppContentHandler>& registry() {
    static std::map<std::string, AppContentHandler> m;
    return m;
}

void registerAppContent(const AppContentHandler& h) { registry()[h.name] = h; }

const AppContentHandler* findAppContent(const char* name) {
    auto it = registry().find(name ? name : "main");
    return it == registry().end() ? nullptr : &it->second;
}

// ============================================================
// 内置 "main" 处理器：复用主绘制 MainUI（「调用主绘制偷懒」）
// 视图实例 = MainUI，存 app->contentData()；处理器零新增绘制代码
// ============================================================
static void mainInit(AppWindow* app) {
    int lw = 0, lh = 0;
    app->logicSize(lw, lh);
    auto* ui = new MainUI();
    ui->init(app->font(), app->smallFont(), app->bigFont(), lw, lh);
    app->setContentData(ui);
}
static void mainShutdown(AppWindow* app) {
    delete static_cast<MainUI*>(app->contentData());
    app->setContentData(nullptr);
}
static void mainResize(AppWindow* app, int w, int h) {
    static_cast<MainUI*>(app->contentData())->resize(w, h);
}
static void mainUpdate(AppWindow* app, Input& in) {
    static_cast<MainUI*>(app->contentData())->update(in);
}
static void mainRender(AppWindow* app, RenderDevice* dev) {
    static_cast<MainUI*>(app->contentData())->render(dev);
}
static void mainCollectText(AppWindow* app, std::string& out) {
    static_cast<MainUI*>(app->contentData())->collectText(out);
}

// ============================================================
// 内置 "test" 处理器：子应用窗口测试内容
//
// 需求（主子级验证）：调试面板「扩展应用窗口」打开的子窗口
//   - 不实例化 MainUI → 不绘制父窗口的主界面按钮（将来可换办公/
//     数据双窗口传入类内容处理器，注册表按名分发即可）
//   - 仅居中绘制一行「测试win窗口」
// 纯静态内容：无视图状态（contentData 恒空），全部回调空实现
// 除 render/collectText——「无状态删除铁律」的最小形态示范。
// ============================================================
static void testInit(AppWindow* app) {
    (void)app;  // 无视图状态（纯静态文本，字体图集由 collectText 预热）
}
static void testShutdown(AppWindow* app) { (void)app; }
static void testResize(AppWindow* app, int w, int h) { (void)app; (void)w; (void)h; }
static void testUpdate(AppWindow* app, Input& in) { (void)app; (void)in; }
static void testRender(AppWindow* app, RenderDevice* dev) {
    // 逻辑尺寸全覆盖矩形 + align=1 水平居中（drawTextScaled 内部垂直居中）；
    // 字号 = 逻辑高/720 基准 × 32px（与三档字体同一 720 缩放基准，非硬编码魔数）
    int lw = 0, lh = 0;
    app->logicSize(lw, lh);
    float px = 32.0f * (lh / 720.0f);
    if (px < 12.0f) px = 12.0f;
    Rect full(0.0f, 0.0f, (float)lw, (float)lh);
    wctl::drawTextScaled(dev, app->font(), "测试win窗口", full,
                         currentColors().text, px, 1);
}
static void testCollectText(AppWindow* app, std::string& out) {
    (void)app;
    out += "测试win窗口";   // 字体图集预热（AppWindow::frame 统一 ensureGlyphs）
}

void registerBuiltinAppContents() {
    static bool done = false;
    if (done) return;
    done = true;
    registerAppContent({ "main", mainInit, mainShutdown, mainResize,
                         mainUpdate, mainRender, mainCollectText });
    registerAppContent({ "test", testInit, testShutdown, testResize,
                         testUpdate, testRender, testCollectText });
}

} // namespace ui