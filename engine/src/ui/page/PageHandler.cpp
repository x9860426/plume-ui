// ============================================================
// 页面处理器注册表实现：PageEnv 便利成员（需 MainUI 完整定义，
// 实现集中于此打破头文件循环）+ 内置页面汇总注册
// ============================================================
#include "PageHandler.h"
#include "Pages.h"
#include "../MainUI.h"

namespace ui {

// ---- PageEnv 便利成员（转发 MainUI 单源接口） ----


WindowInstance& PageEnv::vwin()         { return ui->pageVirtualWin(); }
float PageEnv::scale() const            { return ui->layoutScale(); }
Font* PageEnv::font() const             { return ui->pageFont(); }
Font* PageEnv::smallFont() const        { return ui->pageSmallFont(); }
Font* PageEnv::bigFont() const          { return ui->pageBigFont(); }

// 页面处理器汇总注册（对齐悬浮窗 RegisterAllWindowHandlers 模式：
// 各页面文件自己暴露 registerXxxPage，此处只做汇总，新增页面加一行）
void RegisterAllPageHandlers(PageHandlerRegistry& reg) {
    RegisterOverviewPage(reg);
    RegisterDirectoryPage(reg);
    RegisterSplitterPage(reg);
    RegisterSandboxPage(reg);
    RegisterOptionsPage(reg);
}

} // namespace ui