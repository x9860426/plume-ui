// AI生成
#pragma once
// ============================================================
// 主界面框架（页面化重写版，对齐悬浮窗架构三大支柱）
//
//   1. 配置表驱动：PAGE_CONFIGS（页面，page/PageConfig.h）/
//      BOTTOM_BTNS（底栏，本文件下方）
//      —— 新增页面 = 配置加一条 + 处理器注册，框架零改动
//   2. 处理器注册表：PageHandlerRegistry（page/PageHandler.h）
//      —— 框架只做派发：render/click/hover/drag/release/text/blur/collect
//   3. 系统派发：RegionSystem 虚拟窗（vwin_，id 负值永不与真实窗口冲突）
//      —— 页面内容滚动/裁剪/滚动条/滚轮全部系统自动，页面零基础设施
//
// 框架自身职责（对照悬浮窗 WindowManager 的角色）：
//   tab 栏 / 侧栏 / 内容面板 chrome / 底栏 四组框架控件的绘制与命中；
//   未被框架消费的输入转发当前页处理器；文本焦点路由；
//   与悬浮窗系统的持久接缝（handleInput/feedChars/frame/rescale/注入 g_uiCtx）。
//
// 页面私有状态不再散落框架成员（原 12 个 hover 变量 + 20 个控件态
// 全部删除）—— 各页自持 State（经 pageState<T> 挂框架，按页隔离）。
// ============================================================
#include "Element.h"
#include "Theme.h"
#include "Font.h"
#include "win/WindowManager.h"
#include "win/UiContext.h"
#include "win/WindowInstance.h"
#include "page/PageConfig.h"
#include "page/PageHandler.h"
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ui {

// ============================================================
// 框架配置表（纯数据，扩展 = 加条目，零代码）
// ============================================================



// ---- 底栏按钮（对照原 saveBtn/cancelBtn/colorCustomBtn 三个手写矩形） ----
// 显示顺序 = 数组顺序（左→右）；action 为动作编码：
//   "open:窗口id" = 打开悬浮窗（配置驱动），"" = 无动作
struct BottomBtnConfig {
    const char* label;         // 按钮文字
    const char* action;        // 点击动作（action 字符串编码）
    bool primary;              // true=渐变主按钮 / false=次按钮
};
static const BottomBtnConfig BOTTOM_BTNS[] = {
    {u8"自定义颜色", "open:theme_studio", true },

    {u8"保存",       "open:settings",     true },
};
static const int BOTTOM_BTN_COUNT = (int)(sizeof(BOTTOM_BTNS) / sizeof(BOTTOM_BTNS[0]));

// ============================================================
// 框架布局（只描述框架 chrome；页面内控件矩形由各页面 layout() 单源计算，
// 框架不再持有任何页面控件矩形 —— 原 43 个手算矩形全部删除）
// ============================================================
struct MainUILayout {
    Rect tabBar;                     // tab 条区域
    std::vector<Rect> tabBtns;       // 页签按钮（数量 = PAGE_COUNT）
    Rect contentPanel;               // 内容面板底板（左侧栏已删，占满整宽）
    Rect contentIcon;                // 头部大图标区
    Rect contentDesc;                // 头部描述行
    Rect contentArea;                // 内容可用区（icon/desc 之下）→ PageEnv.content
    Rect bottomBar;                  // 底栏背景
    std::vector<Rect> bottomBtns;    // 底栏按钮（数量 = BOTTOM_BTN_COUNT）
    Rect themeLabel;                 // 左下主题名标签
};

class MainUI {
public:
    MainUI();
    ~MainUI();

    void init(Font* font, Font* smallFont, Font* bigFont, int w, int h);
    void resize(int w, int h);
    void update(Input& in);
    void render(RenderDevice* dev);
    void collectText(std::string& out) const;

    // ---- PageEnv 依赖接口（page/ 模块单源入口，见 page/PageHandler.h） ----
    // 页面私有状态：惰性创建，按页面索引隔离（切页保留，跨 resize 存活）
    template <typename T> T& pageState(int pageIdx) {
        std::shared_ptr<void>& p = pageStates_[pageIdx];
        if (!p) p = std::make_shared<T>();
        return *std::static_pointer_cast<T>(p);
    }
    int pageSel() const { return tabSel_; }             // 当前页索引
    WindowInstance& pageVirtualWin() { return vwin_; }  // RegionSystem 虚拟窗

    float layoutScale() const { return scale_; }        // 布局缩放单源
    Font* pageFont() { return font_; }
    Font* pageSmallFont() { return smallFont_; }
    Font* pageBigFont() { return bigFont_; }

private:
    // 布局（框架矩形 + 虚拟窗几何同步）
    void computeLayout(int w, int h);

    // 页面派发
    const PageHandler* currentPageHandler();   // 当前页处理器（配置表查注册表）
    PageEnv& makeEnv(RenderDevice* dev);      // 页面环境构造（envBuf_ 复用，返回引用）
    void switchPage(int idx);                  // 切页：旧页 blur + 焦点释放
    void runBottomAction(int btnIdx);          // 底栏按钮动作（action 编码解析）

    // 框架绘制（chrome；内容区由页面处理器绘制）
    void drawBackground(RenderDevice* dev);
    void drawTabBar(RenderDevice* dev);
    void drawContentPanel(RenderDevice* dev);
    void drawBottomBar(RenderDevice* dev);

    Font* font_ = nullptr;
    Font* smallFont_ = nullptr;
    Font* bigFont_ = nullptr;
    int winW_ = 1280;
    int winH_ = 720;
    float scale_ = 1.0f;
    float lastUiScale_ = 1.0f;      // 上帧全局页面缩放倍率（变化触发 resize 全链路）

    MainUILayout layout_;

    // ---- 框架交互状态（原 12 个散 hover 变量 → 3 个框架索引） ----
    int tabSel_ = 0;                // 当前页
    int hoverTab_ = -1;             // 页签 hover
    int hoverBottom_ = -1;          // 底栏按钮 hover

    // ---- 页面系统 ----
    std::unique_ptr<PageHandlerRegistry> pages_;       // 处理器注册表
    std::map<int, std::shared_ptr<void>> pageStates_;  // 页面私有状态池
    bool pageDragging_ = false;      // 页面拖拽态（页面 click 消费后置位）
    // 文本输入焦点：不经 MainUI —— 页面输入框统一走 TextInputSystem
    // （g_winMgr.textInput()，WindowManager 成员；字符流在 handleInput 内
    //  系统统一消费，切页/框架消费失焦也由系统 blurAll 提交）

    // RegionSystem 虚拟窗：id 负值永不与真实悬浮窗冲突（真实 id 从 1 起）；
    // 几何 = contentArea（computeLayout 同步），tabSel 恒 0（页面区域 tabId=0）
    WindowInstance vwin_;
    static constexpr int kPageVirtualWinId = -90000;

    // 页面环境缓冲（makeEnv 复用：每帧填充后以引用传给页面处理器，
    // 帧内同步使用无生命周期问题，避免大量临时对象构造）
    PageEnv envBuf_;

    // ---- 帧时钟（update 入口一次采样，本帧内 makeEnv 直读） ----
    // dt = 相邻两帧真实间隔（秒），动画与帧率解耦：高帧率步长小、
    // 低帧率步长大，单位时间推进量恒定
    std::chrono::steady_clock::time_point lastFrameClock_;
    bool  hasLastFrameClock_ = false;   // 首帧无前值，dt=0
    float frameDt_ = 0.0f;              // 本帧真实步长（秒，上限钳制防断点跳变）

    // 自绘应用标题栏占位高（上帧值）：变化 → 布局重排（见 update 第 1.5 步）
    float lastBarH_ = 0.0f;
};

// ---- PageEnv::state 类外模板定义（此处 MainUI 已完整，可安全调用
//      pageState<T>/pageSel；页面代码 include 本头即获得完整定义） ----
template <typename T> T& PageEnv::state() { return ui->pageState<T>(ui->pageSel()); }

} // namespace ui