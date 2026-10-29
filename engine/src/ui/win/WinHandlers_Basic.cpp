// AI生成
// ============================================================
// 基础窗口处理器：警告 / 设置 / 侧边设置 / 停靠 / 调试
// 画法 1:1 对照参考 ui_floatwin.cpp：
//   警告窗   L998-1062   设置窗   L1064-1165
//   侧边设置 L1167-1325   停靠LR  L1327-1519
//   停靠TB   L1521-1716   调试窗  L1718-1925
//   图标绘制对照 ui_draw.cpp L650-767（六边形/立方体/浣熊/剑/小人）
// 规范：布局单源化（layout() 三处共用）+ 模块私有状态 getState<T>()
// ============================================================
#include "WinHandlers.h"
#include "UiContext.h"
#include "RegionSystem.h"
#include "LayerStack.h"
#include "WindowButtons.h"
#include "WinControls.h"
#include "../../app/AppWindow.h"   // currentApp/AppManager（win 扩展应用窗口按钮）
#include "../Element.h"
#include "../Theme.h"
#include <cstdio>
#include <cstring>
#include <vector>

namespace ui {

// ============================================================
// 共享表单原语（对照参考各窗通用画法）
// ============================================================
namespace form {

// 表单标签（11px text@180，左对齐；对照参考 labelFont 画法）
static void drawLabel(RenderDevice* dev, float x, float y, float w,
                      const char* text, float alpha) {
    ThemeColors& c = currentColors();
    wctl::drawTextScaled(dev, g_uiCtx.font, text,
                         Rect(Vec2(x, y), Vec2(w, 18.0f * g_uiCtx.scale)),
                         Color(c.text.r, c.text.g, c.text.b,
                               180.0f / 255.0f * alpha), 11.0f, 0);
}

// 表单输入框已提升为共享原语 wctl::drawInput（WinControls）：
// 支持占位提示/焦点边框/光标闪烁/UTF-8 偏移定位，模板窗调用见渲染处
} // namespace form

// ============================================================
// 警告窗口（对照参考 L998-1062：⚠图标 + 消息 + 确定/取消按钮）
// ============================================================
namespace warning {

struct Layout {
    Rect iconRect;    // ⚠ 图标区（28px Bold 橙色居中）
    Rect msgRect;     // 消息正文（13px 居中）
    Rect okBtn;       // 确定按钮（渐变主按钮 90×30）
    Rect cancelBtn;   // 取消按钮（次按钮 90×30）
};
static Layout layout(const WindowInstance& win) {
    Layout L;
    float s = g_uiCtx.scale;
    float top = win.y + (win.hasTitleBar() ? wbtn::titleBarHeight(win) : 0.0f);
    float cx = win.x + 20.0f * s, cw = win.w - 40.0f * s;
    float cy = top + 16.0f * s;
    L.iconRect = Rect(Vec2(cx, cy), Vec2(cw, 40.0f * s));
    cy += 44.0f * s;
    L.msgRect = Rect(Vec2(cx, cy), Vec2(cw, 40.0f * s));
    // 按钮行：btnY = fy + fh - bottomH(48) + 10（参考 L1041）
    float btnY = win.y + win.h - 48.0f * s + 10.0f * s;
    float btnW = 90.0f * s, gap = 16.0f * s;
    float okX = win.x + (win.w - btnW * 2.0f - gap) * 0.5f;
    L.okBtn = Rect(Vec2(okX, btnY), Vec2(btnW, 30.0f * s));
    L.cancelBtn = Rect(Vec2(okX + btnW + gap, btnY), Vec2(btnW, 30.0f * s));
    return L;
}
struct State { bool hoverOk = false; bool hoverCancel = false; };

} // namespace warning

void renderWarning(RenderDevice* dev, WindowInstance& win, float alpha) {
    ThemeColors& c = currentColors();
    warning::Layout L = warning::layout(win);
    warning::State& s = win.getState<warning::State>();

    // ⚠ 图标：28px Bold 橙色 0xFF8800 居中（参考 L1034-1035）
    wctl::drawTextScaled(dev, g_uiCtx.font, "⚠", L.iconRect,
                         Color(0xFF / 255.0f, 0x88 / 255.0f, 0x00, alpha), 28.0f, 1);
    // 消息：13px text@200 居中（参考 L1038-1039）
    wctl::drawTextScaled(dev, g_uiCtx.font, win.warningMsg.c_str(), L.msgRect,
                         Color(c.text.r, c.text.g, c.text.b,
                               200.0f / 255.0f * alpha), 13.0f, 1);
    // 确定 = 渐变主按钮；取消 = inputBg@220 次按钮（参考 L1048-1061）
    wctl::drawBtn(dev, L.okBtn, "确定", s.hoverOk, alpha, 13.0f);
    wctl::drawBtnSecondary(dev, L.cancelBtn, "取消", s.hoverCancel, alpha, 13.0f);
}

bool clickWarning(WindowInstance& win, const Vec2& m) {
    warning::Layout L = warning::layout(win);
    if (L.okBtn.contains(m) || L.cancelBtn.contains(m)) {
        g_layerStack.closeWindow(win.id);  // 两个按钮都关闭警告
        return true;
    }
    return false;
}

void hoverWarning(WindowInstance& win, const Vec2& m) {
    warning::Layout L = warning::layout(win);
    warning::State& s = win.getState<warning::State>();
    s.hoverOk = L.okBtn.contains(m);
    s.hoverCancel = L.cancelBtn.contains(m);
}

// ============================================================
// 设置窗口（settings / login / about 三变体，按配置 id 区分）
// 对照参考 L1064-1165（无标题栏：系统画底板@230+边框@60，内容全在此）
// ============================================================
namespace settings {

// 模块共享值：音量滑条静态展示值（参考 g_sliderVal 默认语义）
static constexpr int VOLUME_VAL = 60;

struct Layout {
    Rect row1Lbl, row1Input;   // settings:名称 / login:用户名
    Rect row2Lbl, row2Input;   // settings:语言 / login:密码
    Rect sliderRowLbl;         // settings:音量 标签
    Rect sliderGroove;         // 音量滑条槽（水平渐变+手柄由 drawSlider 画）
    Rect sliderVal;            // 音量数值（右对齐 accent）
    Rect remember;             // login:记住登录 勾选框（16px）
    Rect rememberLbl;          // login:记住登录 文字
    Rect aboutTitle;           // about:大标题区
    Rect aboutLine1, aboutLine2;
    Rect saveBtn, cancelBtn;   // 底部按钮（80×28）
};
static Layout layout(const WindowInstance& win) {
    Layout L;
    float s = g_uiCtx.scale;
    float cx = win.x + 20.0f * s, cw = win.w - 40.0f * s;
    float cy = win.y + 16.0f * s;               // 无标题栏：从 fy+16 起（参考 L1080）
    float rowH = 56.0f * s;                     // label(18) + 间隙(2) + input(28) + 行距(8)

    L.row1Lbl = Rect(Vec2(cx, cy), Vec2(cw, 18.0f * s));
    L.row1Input = Rect(Vec2(cx, cy + 20.0f * s), Vec2(cw, 28.0f * s));
    cy += rowH;
    L.row2Lbl = Rect(Vec2(cx, cy), Vec2(cw, 18.0f * s));
    L.row2Input = Rect(Vec2(cx, cy + 20.0f * s), Vec2(cw, 28.0f * s));
    cy += rowH;
    // 音量行（参考 L1101-1112）：标签 80 宽；槽 x+90 宽 cw-130；值右对齐 30 宽
    L.sliderRowLbl = Rect(Vec2(cx, cy), Vec2(80.0f * s, 18.0f * s));
    L.sliderGroove = Rect(Vec2(cx + 90.0f * s, cy + 6.0f * s),
                          Vec2(cw - 130.0f * s, 6.0f * s));
    L.sliderVal = Rect(Vec2(cx + cw - 30.0f * s, cy), Vec2(30.0f * s, 18.0f * s));
    // login 记住登录行（参考 L1130-1136）：勾选框 16px 在 cy+4
    L.remember = Rect(Vec2(cx, cy + 4.0f * s), Vec2(16.0f * s, 16.0f * s));
    L.rememberLbl = Rect(Vec2(cx + 16.0f * s + 8.0f * s, cy),
                         Vec2(cw - 24.0f * s, 24.0f * s));
    // about 三行（参考 L1138-1143）
    L.aboutTitle = Rect(Vec2(cx, win.y + 16.0f * s), Vec2(cw, 30.0f * s));
    L.aboutLine1 = Rect(Vec2(cx, win.y + 16.0f * s + 40.0f * s), Vec2(cw, 20.0f * s));
    L.aboutLine2 = Rect(Vec2(cx, win.y + 16.0f * s + 70.0f * s), Vec2(cw, 18.0f * s));

    // 底部按钮（参考 L1146-1149）：btnY = fy+fh-44+8；save 右贴 16；cancel 左邻 28
    float btnY = win.y + win.h - 44.0f * s + 8.0f * s;
    float saveW = 80.0f * s;
    L.saveBtn = Rect(Vec2(win.x + win.w - saveW - 16.0f * s, btnY),
                     Vec2(saveW, 28.0f * s));
    L.cancelBtn = Rect(Vec2(win.x + win.w - saveW - 80.0f * s - 28.0f * s, btnY),
                       Vec2(80.0f * s, 28.0f * s));
    return L;
}
struct State { bool hoverSave = false; bool hoverCancel = false; };

} // namespace settings

void renderSettings(RenderDevice* dev, WindowInstance& win, float alpha) {
    ThemeColors& c = currentColors();
    settings::Layout L = settings::layout(win);
    settings::State& s = win.getState<settings::State>();
    float sc = g_uiCtx.scale;

    // 配置 id 区分三变体（参考按 FW_SETTINGS/FW_LOGIN/else 分支）
    const char* id = win.config() ? win.config()->id : "settings";
    if (strcmp(id, "settings") == 0) {
        // ---- 设置：名称 / 语言 / 音量（参考 L1084-1112） ----
        form::drawLabel(dev, L.row1Lbl.pos.x, L.row1Lbl.pos.y, L.row1Lbl.size.x,
                        "名称", alpha);
        wctl::drawInput(dev, L.row1Input, "用户", nullptr, false, false, alpha);
        form::drawLabel(dev, L.row2Lbl.pos.x, L.row2Lbl.pos.y, L.row2Lbl.size.x,
                        "语言", alpha);
        wctl::drawInput(dev, L.row2Input, "中文", nullptr, false, false, alpha);
        form::drawLabel(dev, L.sliderRowLbl.pos.x, L.sliderRowLbl.pos.y,
                        L.sliderRowLbl.size.x, "音量", alpha);
        wctl::drawSlider(dev, L.sliderGroove,
                         (float)settings::VOLUME_VAL / 100.0f, false, alpha);
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", settings::VOLUME_VAL);
        wctl::drawTextScaled(dev, g_uiCtx.font, buf, L.sliderVal,
                             Color(c.accent.r, c.accent.g, c.accent.b, alpha), 11.0f, 2);
    } else if (strcmp(id, "login") == 0) {
        // ---- 登录：用户名 / 密码 / 记住登录（参考 L1113-1136） ----
        form::drawLabel(dev, L.row1Lbl.pos.x, L.row1Lbl.pos.y, L.row1Lbl.size.x,
                        "用户名", alpha);
        wctl::drawInput(dev, L.row1Input, "admin", nullptr, false, false, alpha);
        form::drawLabel(dev, L.row2Lbl.pos.x, L.row2Lbl.pos.y, L.row2Lbl.size.x,
                        "密码", alpha);
        wctl::drawInput(dev, L.row2Input, "••••••", nullptr, false, false, alpha);
        wctl::drawCheck(dev, L.remember, true, false, alpha);
        wctl::drawTextScaled(dev, g_uiCtx.font, "记住登录", L.rememberLbl,
                             Color(c.text.r, c.text.g, c.text.b, alpha), 12.0f, 0);
    } else {
        // ---- 关于：大标题 + 两行说明（参考 L1137-1143） ----
        wctl::drawTextScaled(dev, g_uiCtx.font, "UI模板集合", L.aboutTitle,
                             Color(c.accent.r, c.accent.g, c.accent.b, alpha), 18.0f, 1);
        wctl::drawTextScaled(dev, g_uiCtx.font, "9种主题 · 纯C++ Win32 API + GDI+",
                             L.aboutLine1,
                             Color(c.text.r, c.text.g, c.text.b,
                                   160.0f / 255.0f * alpha), 12.0f, 1);
        wctl::drawTextScaled(dev, g_uiCtx.font, "F2切换主题  F3切换厚度  +/-调节清晰度",
                             L.aboutLine2,
                             Color(c.text.r, c.text.g, c.text.b,
                                   140.0f / 255.0f * alpha), 11.0f, 1);
    }

    // 底部按钮（三变体共用，参考 L1146-1164）：确定=渐变主按钮 / 取消=次按钮
    wctl::drawBtn(dev, L.saveBtn, "确定", s.hoverSave, alpha, 12.0f);
    wctl::drawBtnSecondary(dev, L.cancelBtn, "取消", s.hoverCancel, alpha, 12.0f);
    (void)sc;
}

bool clickSettings(WindowInstance& win, const Vec2& m) {
    settings::Layout L = settings::layout(win);
    if (L.saveBtn.contains(m) || L.cancelBtn.contains(m)) {
        g_layerStack.closeWindow(win.id);
        return true;
    }
    return false;
}

void hoverSettings(WindowInstance& win, const Vec2& m) {
    settings::Layout L = settings::layout(win);
    settings::State& s = win.getState<settings::State>();
    s.hoverSave = L.saveBtn.contains(m);
    s.hoverCancel = L.cancelBtn.contains(m);
}

void dragSettings(WindowInstance&, const Vec2&) {}
void releaseSettings(WindowInstance&, const Vec2&) {}

// ============================================================
// 侧边设置（对照参考 L1167-1325：左侧标签栏 + 右侧内容 + 底部按钮）
// 系统画整体底板@230/外框@60；侧栏底板/标签/内容由本模块绘制
// ============================================================
namespace sideset {

static const char* ITEMS[] = {"常规", "显示", "音频", "网络", "高级"};
static constexpr int ITEM_COUNT = 5;
// 内容页静态展示值（参考视觉默认）
static constexpr int BRIGHTNESS = 65;
static constexpr int VOLUME = 50;

struct Layout {
    Rect sidePanel;          // 侧栏底板（bg3@240 左圆角10）
    Rect items[ITEM_COUNT];  // 标签项（itemH32，圆角6）
    float contentX, contentW;// 内容区起点/宽（不含具体控件）
    Rect saveBtn, cancelBtn; // 底部按钮（同设置窗）
};
static Layout layout(const WindowInstance& win) {
    Layout L;
    float s = g_uiCtx.scale;
    float sideW = 100.0f * s;
    L.sidePanel = Rect(Vec2(win.x, win.y), Vec2(sideW, win.h));
    float itemH = 32.0f * s, startY = win.y + 16.0f * s;
    for (int i = 0; i < ITEM_COUNT; i++)
        L.items[i] = Rect(Vec2(win.x + 8.0f * s, startY + i * itemH),
                          Vec2(sideW - 16.0f * s, itemH - 4.0f * s));
    L.contentX = win.x + sideW + 12.0f * s;
    L.contentW = win.w - sideW - 24.0f * s;

    float sc2 = s;
    float btnY = win.y + win.h - 44.0f * sc2 + 8.0f * sc2;
    float saveW = 80.0f * sc2;
    L.saveBtn = Rect(Vec2(win.x + win.w - saveW - 16.0f * sc2, btnY),
                     Vec2(saveW, 28.0f * sc2));
    L.cancelBtn = Rect(Vec2(win.x + win.w - saveW - 80.0f * sc2 - 28.0f * sc2, btnY),
                       Vec2(80.0f * sc2, 28.0f * sc2));
    return L;
}
struct State { int hoverItem = -1; bool hoverSave = false; bool hoverCancel = false; };

// 侧栏标签项绘制（对照参考 L1196-1214：选中=垂直渐变accent→accent2/纯accent
// +btnText；hover=tabBg@200+text@220；普通=text@160）
static void drawSideItem(RenderDevice* dev, const Rect& r, const char* text,
                         bool selected, bool hovered, float alpha) {
    ThemeColors& c = currentColors();
    float rad = 6.0f * g_uiCtx.scale * g_uiCtx.radiusScale();
    if (selected) {
        if (c.antialias) {
            drawRoundedGradientRect(dev, r,
                Color(c.accent.r, c.accent.g, c.accent.b, alpha),
                Color(c.accent2.r, c.accent2.g, c.accent2.b, alpha), rad);
        } else {
            drawRoundedRect(dev, r,
                Color(c.accent.r, c.accent.g, c.accent.b, alpha), rad);
        }
        wctl::drawTextScaled(dev, g_uiCtx.font, text, r,
                             Color(c.btnText.r, c.btnText.g, c.btnText.b, alpha), 12.0f, 1);
    } else if (hovered) {
        drawRoundedRect(dev, r, Color(c.tabBg.r, c.tabBg.g, c.tabBg.b,
                                      200.0f / 255.0f * alpha), rad);
        wctl::drawTextScaled(dev, g_uiCtx.font, text, r,
                             Color(c.text.r, c.text.g, c.text.b,
                                   220.0f / 255.0f * alpha), 12.0f, 1);
    } else {
        wctl::drawTextScaled(dev, g_uiCtx.font, text, r,
                             Color(c.text.r, c.text.g, c.text.b,
                                   160.0f / 255.0f * alpha), 12.0f, 1);
    }
}

// 内容页绘制（对照参考 L1223-1304 五页表单；cy 从 fy+16 起逐行推进）
static void drawContentPage(RenderDevice* dev, const WindowInstance& win,
                            const Layout& L, int page, float alpha) {
    ThemeColors& c = currentColors();
    float s = g_uiCtx.scale;
    float x = L.contentX, w = L.contentW;
    float cy = win.y + 16.0f * s;
    float rowH = 56.0f * s;

    if (page == 0) {
        // 常规：名称 + 语言（参考 L1223-1237）
        form::drawLabel(dev, x, cy, w, "名称", alpha);
        wctl::drawInput(dev, Rect(Vec2(x, cy + 20.0f * s), Vec2(w, 28.0f * s)),
                        "用户", nullptr, false, false, alpha);
        cy += rowH;
        form::drawLabel(dev, x, cy, w, "语言", alpha);
        wctl::drawInput(dev, Rect(Vec2(x, cy + 20.0f * s), Vec2(w, 28.0f * s)),
                        "中文", nullptr, false, false, alpha);
    } else if (page == 1) {
        // 显示：分辨率 + 亮度滑条（参考 L1238-1256）
        form::drawLabel(dev, x, cy, w, "分辨率", alpha);
        wctl::drawInput(dev, Rect(Vec2(x, cy + 20.0f * s), Vec2(w, 28.0f * s)),
                        "1920×1080", nullptr, false, false, alpha);
        cy += rowH;
        form::drawLabel(dev, x, cy, 80.0f * s, "亮度", alpha);
        wctl::drawSlider(dev, Rect(Vec2(x + 90.0f * s, cy + 6.0f * s),
                                   Vec2(w - 130.0f * s, 6.0f * s)),
                         (float)BRIGHTNESS / 100.0f, false, alpha);
    } else if (page == 2) {
        // 音频：主音量滑条 + 音效（参考 L1257-1275）
        form::drawLabel(dev, x, cy, 80.0f * s, "主音量", alpha);
        wctl::drawSlider(dev, Rect(Vec2(x + 90.0f * s, cy + 6.0f * s),
                                   Vec2(w - 130.0f * s, 6.0f * s)),
                         (float)VOLUME / 100.0f, false, alpha);
        cy += 40.0f * s;
        form::drawLabel(dev, x, cy, w, "音效", alpha);
        wctl::drawInput(dev, Rect(Vec2(x, cy + 20.0f * s), Vec2(w, 28.0f * s)),
                        "环绕声", nullptr, false, false, alpha);
    } else if (page == 3) {
        // 网络：连接 + 延迟（参考 L1276-1286；延迟值用 accent 色）
        form::drawLabel(dev, x, cy, w, "连接", alpha);
        wctl::drawInput(dev, Rect(Vec2(x, cy + 20.0f * s), Vec2(w, 28.0f * s)),
                        "WiFi已连接", nullptr, false, false, alpha);
        cy += rowH;
        form::drawLabel(dev, x, cy, w, "延迟", alpha);
        wctl::drawTextScaled(dev, g_uiCtx.font, "12ms",
                             Rect(Vec2(x, cy + 20.0f * s), Vec2(w, 28.0f * s)),
                             Color(c.accent.r, c.accent.g, c.accent.b, alpha), 12.0f, 0);
    } else {
        // 高级：调试模式勾选 + 日志级别（参考 L1287-1303）
        form::drawLabel(dev, x, cy, w, "调试模式", alpha);
        wctl::drawCheck(dev, Rect(Vec2(x, cy + 4.0f * s), Vec2(16.0f * s, 16.0f * s)),
                        true, false, alpha);
        wctl::drawTextScaled(dev, g_uiCtx.font, "启用",
                             Rect(Vec2(x + 16.0f * s + 8.0f * s, cy),
                                  Vec2(w - 24.0f * s, 24.0f * s)),
                             Color(c.text.r, c.text.g, c.text.b, alpha), 12.0f, 0);
        cy += 40.0f * s;
        form::drawLabel(dev, x, cy, w, "日志级别", alpha);
        wctl::drawInput(dev, Rect(Vec2(x, cy + 20.0f * s), Vec2(w, 28.0f * s)),
                        "详细", nullptr, false, false, alpha);
    }
}

} // namespace sideset

void renderSideSettings(RenderDevice* dev, WindowInstance& win, float alpha) {
    ThemeColors& c = currentColors();
    sideset::Layout L = sideset::layout(win);
    sideset::State& s = win.getState<sideset::State>();
    float sc = g_uiCtx.scale;

    // 侧栏底板：bg3@240 左圆角10 + accent@80 边框（参考 L1181-1185）
    float sideRadius = 10.0f * sc * g_uiCtx.radiusScale();
    drawLeftRoundedRect(dev, L.sidePanel, Color(c.bg3.r, c.bg3.g, c.bg3.b,
                                                240.0f / 255.0f * alpha), sideRadius);
    drawLeftRoundedBorder(dev, L.sidePanel,
                          Color(c.accent.r, c.accent.g, c.accent.b,
                                80.0f / 255.0f * alpha),
                          g_uiCtx.borderWidth(), sideRadius);

    // 5 个标签项（选中页 = win.tabSel，RegionSystem 语义单源）
    for (int i = 0; i < sideset::ITEM_COUNT; i++)
        sideset::drawSideItem(dev, L.items[i], sideset::ITEMS[i],
                              win.tabSel == i, s.hoverItem == i, alpha);

    // 内容页（0-4）
    sideset::drawContentPage(dev, win, L, win.tabSel, alpha);

    // 底部按钮（参考 L1306-1324）
    wctl::drawBtn(dev, L.saveBtn, "确定", s.hoverSave, alpha, 12.0f);
    wctl::drawBtnSecondary(dev, L.cancelBtn, "取消", s.hoverCancel, alpha, 12.0f);
}

bool clickSideSettings(WindowInstance& win, const Vec2& m) {
    sideset::Layout L = sideset::layout(win);
    for (int i = 0; i < sideset::ITEM_COUNT; i++) {
        if (L.items[i].contains(m)) { win.tabSel = i; return true; }
    }
    if (L.saveBtn.contains(m) || L.cancelBtn.contains(m)) {
        g_layerStack.closeWindow(win.id);
        return true;
    }
    return false;
}

void hoverSideSettings(WindowInstance& win, const Vec2& m) {
    sideset::Layout L = sideset::layout(win);
    sideset::State& s = win.getState<sideset::State>();
    s.hoverItem = -1;
    for (int i = 0; i < sideset::ITEM_COUNT; i++)
        if (L.items[i].contains(m)) { s.hoverItem = i; break; }
    s.hoverSave = L.saveBtn.contains(m);
    s.hoverCancel = L.cancelBtn.contains(m);
}

// ============================================================
// 停靠窗口（LR=按钮纵向左侧 / TB=按钮横向顶部，对照参考 L1327-1716）
// 透明窗：系统不画 chrome，侧栏按钮/内容面板/关闭按钮全部自绘
// ============================================================
namespace dock {

static const char* TITLES[] = {"显示", "通用", "通知", "账户"};
static constexpr int BTN_COUNT = 4;

struct Layout {
    bool isLR;                 // true=LR（按钮纵向左侧）/ false=TB（按钮横向顶部）
    Rect btns[BTN_COUNT];      // 图标按钮
    Rect contentPanel;         // 内容面板（bg3@245 右/底圆角14）
    float iconSize;            // 图标基准尺寸 30×scale
};
static Layout layout(const WindowInstance& win) {
    Layout L;
    float s = g_uiCtx.scale;
    L.isLR = win.hasFlag(WIN_DOCK_HORIZONTAL);   // dock_lr 配置该标志
    L.iconSize = 30.0f * s;
    float pad = 6.0f * s;

    if (L.isLR) {
        // LR（参考 L1333-1347）：sideW70/btnH56 纵向左侧
        float sideW = 70.0f * s, btnH = 56.0f * s;
        for (int i = 0; i < BTN_COUNT; i++)
            L.btns[i] = Rect(Vec2(win.x + pad, win.y + pad + i * (btnH + pad)),
                             Vec2(sideW - pad, btnH));
        L.contentPanel = Rect(Vec2(win.x + sideW, win.y),
                              Vec2(win.w - sideW, win.h));
    } else {
        // TB（参考 L1527-1533）：btnH40/btnW56 横向顶部
        float btnH = 40.0f * s, btnW = 56.0f * s;
        float topH = pad + btnH;
        for (int i = 0; i < BTN_COUNT; i++)
            L.btns[i] = Rect(Vec2(win.x + pad + i * (btnW + pad), win.y + pad),
                             Vec2(btnW, btnH));
        L.contentPanel = Rect(Vec2(win.x, win.y + topH),
                              Vec2(win.w, win.h - topH));
    }
    return L;
}
struct State { int sel = 0; int hoverBtn = -1; };

// ---- 图标绘制（对照参考 ui_draw.cpp L650-767，GDI Color(a,r,g,b) 语义） ----

// 六边形（L650-660：6 点 60°i-30° 多边形 + 1.2f 描边）
static void drawHexagonIcon(RenderDevice* dev, float cx, float cy, float r,
                            const Color& fill, const Color& pen) {
    Vec2 pts[6];
    for (int i = 0; i < 6; i++) {
        float angle = (60.0f * i - 30.0f) * 3.14159f / 180.0f;
        pts[i] = Vec2(cx + r * cosf(angle), cy + r * sinf(angle));
    }
    fillPolygon(dev, pts, 6, fill);
    drawPolygonOutline(dev, pts, 6, 1.2f, pen);
}

// 立方体（L662-696：上/左/右三多边形 + 0.8f 边线）
static void drawCubeIcon(RenderDevice* dev, float cx, float cy, float size,
                         const Color& face, const Color& top, const Color& side,
                         float alpha) {
    float w = size * 0.5f, h = size * 0.3f;
    Vec2 topPts[4] = {
        {cx, cy - h}, {cx + w, cy - h * 0.5f}, {cx, cy}, {cx - w, cy - h * 0.5f}};
    Vec2 leftPts[4] = {
        {cx - w, cy - h * 0.5f}, {cx, cy}, {cx, cy + h}, {cx - w, cy + h * 0.5f}};
    Vec2 rightPts[4] = {
        {cx + w, cy - h * 0.5f}, {cx, cy}, {cx, cy + h}, {cx + w, cy + h * 0.5f}};
    fillPolygon(dev, topPts, 4, top);
    fillPolygon(dev, leftPts, 4, side);
    fillPolygon(dev, rightPts, 4, face);
    Color edge(80.0f / 255.0f, 60.0f / 255.0f, 30.0f / 255.0f,
               180.0f / 255.0f * alpha);
    drawPolygonOutline(dev, topPts, 4, 0.8f, edge);
    drawPolygonOutline(dev, leftPts, 4, 0.8f, edge);
    drawPolygonOutline(dev, rightPts, 4, 0.8f, edge);
}

// 浣熊脸（L698-717：脸+双耳+眼罩+双眼+鼻，椭圆中心坐标由 GDI 左上角换算）
static void drawRaccoonIcon(RenderDevice* dev, float cx, float cy, float size,
                            float alpha) {
    float r = size * 0.4f;
    Color face(180.0f / 255.0f, 170.0f / 255.0f, 160.0f / 255.0f,
               200.0f / 255.0f * alpha);
    Color ear(120.0f / 255.0f, 100.0f / 255.0f, 90.0f / 255.0f,
              180.0f / 255.0f * alpha);
    Color mask(60.0f / 255.0f, 40.0f / 255.0f, 30.0f / 255.0f,
               220.0f / 255.0f * alpha);
    Color eye(20.0f / 255.0f, 20.0f / 255.0f, 20.0f / 255.0f, alpha);
    Color nose(40.0f / 255.0f, 30.0f / 255.0f, 30.0f / 255.0f, alpha);
    fillEllipse(dev, cx, cy, r, r, face);                                    // 脸
    fillEllipse(dev, cx - r * 0.55f, cy - r * 1.1f + r * 0.25f,
                r * 0.25f, r * 0.25f, ear);                                   // 左耳
    fillEllipse(dev, cx + r * 0.55f, cy - r * 0.85f, r * 0.25f, r * 0.25f, ear); // 右耳
    fillEllipse(dev, cx - r * 0.45f, cy - r * 0.1f, r * 0.25f, r * 0.2f, mask);  // 眼罩左
    fillEllipse(dev, cx + r * 0.45f, cy - r * 0.1f, r * 0.25f, r * 0.2f, mask);  // 眼罩右
    fillEllipse(dev, cx - r * 0.35f, cy - r * 0.1f, r * 0.1f, r * 0.1f, eye);   // 眼左
    fillEllipse(dev, cx + r * 0.35f, cy - r * 0.1f, r * 0.1f, r * 0.1f, eye);   // 眼右
    fillEllipse(dev, cx, cy + r * 0.14f, r * 0.12f, r * 0.09f, nose);           // 鼻
}

// 宝剑（L719-743：5点刀刃 + 护手矩形 + 柄矩形 + 柄尾椭圆）
static void drawSwordIcon(RenderDevice* dev, float cx, float cy, float size,
                          float alpha) {
    float bladeLen = size * 0.6f, bladeW = size * 0.08f;
    float guardW = size * 0.25f, handleLen = size * 0.2f;
    Color blade(160.0f / 255.0f, 160.0f / 255.0f, 170.0f / 255.0f,
                220.0f / 255.0f * alpha);
    Color guard(100.0f / 255.0f, 80.0f / 255.0f, 50.0f / 255.0f,
                200.0f / 255.0f * alpha);
    Color handle(80.0f / 255.0f, 50.0f / 255.0f, 30.0f / 255.0f,
                 180.0f / 255.0f * alpha);
    Color pommel(120.0f / 255.0f, 80.0f / 255.0f, 40.0f / 255.0f,
                 200.0f / 255.0f * alpha);
    Vec2 bladePts[5] = {
        {cx, cy - bladeLen},
        {cx + bladeW, cy - bladeLen + bladeW},
        {cx + bladeW, cy + guardW * 0.5f},
        {cx - bladeW, cy + guardW * 0.5f},
        {cx - bladeW, cy - bladeLen + bladeW}};
    fillPolygon(dev, bladePts, 5, blade);
    drawRect(dev, Rect(Vec2(cx - guardW, cy + guardW * 0.5f),
                       Vec2(guardW * 2.0f, size * 0.06f)), guard);
    drawRect(dev, Rect(Vec2(cx - size * 0.04f, cy + guardW * 0.5f + size * 0.06f),
                       Vec2(size * 0.08f, handleLen)), handle);
    fillEllipse(dev, cx, cy + guardW * 0.5f + size * 0.06f + handleLen + size * 0.06f,
                size * 0.06f, size * 0.06f, pommel);
}

// 小人（L745-767：头椭圆 + 身体贝塞尔→多边形近似）
static void drawPersonIcon(RenderDevice* dev, float cx, float cy, float size,
                           float alpha) {
    float headR = size * 0.18f;
    Color head(160.0f / 255.0f, 140.0f / 255.0f, 130.0f / 255.0f,
               200.0f / 255.0f * alpha);
    Color body(100.0f / 255.0f, 90.0f / 255.0f, 80.0f / 255.0f,
               200.0f / 255.0f * alpha);
    fillEllipse(dev, cx, cy - size * 0.35f, headR, headR, head);
    // 身体：两侧贝塞尔（肩部曲线）+ 底边直线 → 7 点多边形近似
    Vec2 bodyPts[7] = {
        {cx - size * 0.20f, cy + size * 0.15f},
        {cx - size * 0.17f, cy - size * 0.02f},
        {cx - size * 0.10f, cy - size * 0.08f},
        {cx,                 cy - size * 0.10f},
        {cx + size * 0.10f, cy - size * 0.08f},
        {cx + size * 0.17f, cy - size * 0.02f},
        {cx + size * 0.20f, cy + size * 0.15f}};
    fillPolygon(dev, bodyPts, 7, body);
}

// 按钮图标总入口（对照参考 L1371-1386：0=六边形+立方体 1=浣熊 2=剑 3=小人）
static void drawDockIcon(RenderDevice* dev, int idx, float cx, float cy,
                         float iconSize, bool selected, float alpha) {
    ThemeColors& c = currentColors();
    float s = g_uiCtx.scale;
    if (idx == 0) {
        float hexR = iconSize * 0.55f;
        const Color& hexFill = selected ? c.cardBg1 : c.cardBg2;
        drawHexagonIcon(dev, cx, cy, hexR,
                        Color(hexFill.r, hexFill.g, hexFill.b,
                              220.0f / 255.0f * alpha),
                        Color(c.accent.r, c.accent.g, c.accent.b,
                              180.0f / 255.0f * alpha));
        // 立方体三面色：top 0xffd700 / face 0xdaa520 / side 0xb8860b（参考 L1376-1378）
        Color cubeTop(0xff / 255.0f, 0xd7 / 255.0f, 0x00, alpha);
        Color cubeFace(0xda / 255.0f, 0xa5 / 255.0f, 0x20, alpha);
        Color cubeSide(0xb8 / 255.0f, 0x86 / 255.0f, 0x0b, alpha);
        drawCubeIcon(dev, cx, cy + 2.0f * s, iconSize * 0.7f,
                     cubeFace, cubeTop, cubeSide, alpha);
    } else if (idx == 1) {
        drawRaccoonIcon(dev, cx, cy, iconSize, alpha);
    } else if (idx == 2) {
        drawSwordIcon(dev, cx, cy, iconSize * 1.1f, alpha);
    } else {
        drawPersonIcon(dev, cx, cy, iconSize * 1.2f, alpha);
    }
}

// 内容页（对照参考 L1414-1510 四页；inputH28 圆角6 边框@100）
static void drawContentPage(RenderDevice* dev, const WindowInstance& win,
                            const Rect& panel, int page, float alpha) {
    ThemeColors& c = currentColors();
    float s = g_uiCtx.scale;
    float cx = panel.pos.x + 16.0f * s;
    float cw = panel.size.x - 32.0f * s;
    float cy = panel.pos.y + 16.0f * s;

    // 标题：16px Bold text@255（参考 L1400-1407）
    wctl::drawTextScaled(dev, g_uiCtx.font, TITLES[page],
                         Rect(Vec2(cx, cy), Vec2(cw, 28.0f * s)),
                         Color(c.text.r, c.text.g, c.text.b, alpha), 16.0f, 0);
    cy += 36.0f * s;

    // 输入框样式（参考 L1409-1412）：inputBg@240 / inputBorder@100 / r6
    auto inputAt = [&](float y) {
        return Rect(Vec2(cx, y), Vec2(cw, 28.0f * s));
    };
    auto drawBox = [&](const Rect& r, const char* val) {
        float rad = 6.0f * s;
        drawRoundedRect(dev, r, Color(c.inputBg.r, c.inputBg.g, c.inputBg.b,
                                      240.0f / 255.0f * alpha), rad);
        drawRoundedBorder(dev, r, Color(c.inputBorder.r, c.inputBorder.g,
                                        c.inputBorder.b,
                                        100.0f / 255.0f * alpha),
                          g_uiCtx.borderWidth(), rad);
        wctl::drawTextScaled(dev, g_uiCtx.font, val,
                             Rect(Vec2(r.pos.x + 10.0f * s, r.pos.y),
                                  Vec2(r.size.x - 20.0f * s, r.size.y)),
                             Color(c.text.r, c.text.g, c.text.b, alpha), 13.0f, 0);
    };
    auto label = [&](const char* text, float y, float w) {
        wctl::drawTextScaled(dev, g_uiCtx.font, text,
                             Rect(Vec2(cx, y), Vec2(w, 18.0f * s)),
                             Color(c.tabText.r, c.tabText.g, c.tabText.b,
                                   180.0f / 255.0f * alpha), 12.0f, 0);
    };

    if (page == 0) {
        // 显示：分辨率 + 亮度(65%) + 刷新率（参考 L1414-1436）
        label("分辨率", cy, cw); cy += 22.0f * s;
        drawBox(inputAt(cy), "1920×1080"); cy += 40.0f * s;
        label("亮度", cy, 80.0f * s);
        wctl::drawSlider(dev, Rect(Vec2(cx + 90.0f * s, cy + 6.0f * s),
                                   Vec2(cw - 90.0f * s, 6.0f * s)), 0.65f, false, alpha);
        wctl::drawTextScaled(dev, g_uiCtx.font, "65%",
                             Rect(Vec2(cx + cw - 40.0f * s, cy - 2.0f * s),
                                  Vec2(40.0f * s, 18.0f * s)),
                             Color(c.text.r, c.text.g, c.text.b,
                                   200.0f / 255.0f * alpha), 11.0f, 2);
        cy += 30.0f * s;
        label("刷新率", cy, cw); cy += 22.0f * s;
        drawBox(inputAt(cy), "60Hz");
    } else if (page == 1) {
        // 通用：语言 + 自动更新开关 + 启动页（参考 L1437-1459）
        label("语言", cy, cw); cy += 22.0f * s;
        drawBox(inputAt(cy), "简体中文"); cy += 40.0f * s;
        label("自动更新", cy, 80.0f * s);
        wctl::drawToggleOn(dev, Rect(Vec2(cx + 90.0f * s, cy - 4.0f * s),
                                     Vec2(50.0f * s, 26.0f * s)), alpha);
        cy += 30.0f * s;
        label("启动页", cy, cw); cy += 22.0f * s;
        drawBox(inputAt(cy), "首页");
    } else if (page == 2) {
        // 通知：音量(50%) + 通知开关 + 选项A勾选（参考 L1460-1488）
        label("音量", cy, 80.0f * s);
        wctl::drawSlider(dev, Rect(Vec2(cx + 90.0f * s, cy + 6.0f * s),
                                   Vec2(cw - 90.0f * s, 6.0f * s)), 0.5f, false, alpha);
        wctl::drawTextScaled(dev, g_uiCtx.font, "50%",
                             Rect(Vec2(cx + cw - 40.0f * s, cy - 2.0f * s),
                                  Vec2(40.0f * s, 18.0f * s)),
                             Color(c.text.r, c.text.g, c.text.b,
                                   200.0f / 255.0f * alpha), 11.0f, 2);
        cy += 30.0f * s;
        label("通知", cy, 80.0f * s);
        wctl::drawToggleOn(dev, Rect(Vec2(cx + 90.0f * s, cy - 4.0f * s),
                                     Vec2(50.0f * s, 26.0f * s)), alpha);
        cy += 30.0f * s;
        label("选项 A", cy, 80.0f * s);
        // 复选框统一走 wctl::drawCheck（全引擎唯一绘制，勾显隐全局单源）
        float cbS = 18.0f * s;
        Rect cb(Vec2(cx + 90.0f * s, cy), Vec2(cbS, cbS));
        wctl::drawCheck(dev, cb, true, false, alpha);
    } else {
        // 账户：用户名 + 密码 + 记住登录空勾选框（参考 L1489-1509）
        label("用户名", cy, cw); cy += 22.0f * s;
        drawBox(inputAt(cy), "admin"); cy += 40.0f * s;
        label("密码", cy, cw); cy += 22.0f * s;
        drawBox(inputAt(cy), "••••••"); cy += 40.0f * s;
        label("记住登录", cy, 80.0f * s);
        float cbS = 18.0f * s;
        Rect cb(Vec2(cx + 90.0f * s, cy), Vec2(cbS, cbS));
        wctl::drawCheck(dev, cb, false, false, alpha);
    }
}

} // namespace dock

void renderDock(RenderDevice* dev, WindowInstance& win, float alpha) {
    ThemeColors& c = currentColors();
    dock::Layout L = dock::layout(win);
    dock::State& s = win.getState<dock::State>();
    float sc = g_uiCtx.scale;
    float btnRadius = 12.0f * sc * g_uiCtx.radiusScale();
    float contentRadius = 14.0f * sc * g_uiCtx.radiusScale();

    // ---- 4 个图标按钮（对照参考 L1344-1387 / L1541-1583） ----
    for (int i = 0; i < dock::BTN_COUNT; i++) {
        const Rect& r = L.btns[i];
        bool sel = (s.sel == i), hov = (s.hoverBtn == i);
        // 阴影：基础厚度值（参考 L1353 无加值）
        {
            int blur = (int)g_uiCtx.shadowBlur();
            float a255 = g_uiCtx.shadowAlpha() * 255.0f;
            for (int k = blur; k >= 1; k--) {
                float al = a255 * (float)(blur - k + 1) / (float)blur / 2.0f / 255.0f * alpha;
                if (al <= 0.0f) continue;
                drawRoundedRect(dev, Rect(Vec2(r.pos.x + k, r.pos.y + k), r.size),
                                Color(0, 0, 0, al), btnRadius);
            }
        }
        // 底色：选中 tabSelBg / hover btnHover1 / 普通 tabBg，@245（参考 L1355-1357）
        const Color& bgc = sel ? c.tabSelBg : (hov ? c.btnHover1 : c.tabBg);
        Color bg(bgc.r, bgc.g, bgc.b, 245.0f / 255.0f * alpha);
        if (L.isLR) drawLeftRoundedRect(dev, r, bg, btnRadius);
        else        drawTopRoundedRect(dev, r, bg, btnRadius);
        // 边框：普通 groupBorder@80 BorderWidth / 选中 accent@120 1.2f（参考 L1359-1365）
        if (!sel) {
            Color bc(c.groupBorder.r, c.groupBorder.g, c.groupBorder.b,
                     80.0f / 255.0f * alpha);
            if (L.isLR) drawLeftRoundedBorder(dev, r, bc, g_uiCtx.borderWidth(), btnRadius);
            else        drawTopRoundedBorder(dev, r, bc, g_uiCtx.borderWidth(), btnRadius);
        } else {
            Color bc(c.accent.r, c.accent.g, c.accent.b, 120.0f / 255.0f * alpha);
            if (L.isLR) drawLeftRoundedBorder(dev, r, bc, 1.2f, btnRadius);
            else        drawTopRoundedBorder(dev, r, bc, 1.2f, btnRadius);
        }
        // 图标（按钮中心）
        dock::drawDockIcon(dev, i, r.pos.x + r.size.x * 0.5f,
                           r.pos.y + r.size.y * 0.5f, L.iconSize, sel, alpha);
    }

    // ---- 内容面板（参考 L1389-1395：阴影+2/+15、bg3@245、groupBorder@80） ----
    const Rect& cp = L.contentPanel;
    {
        int blur = (int)(g_uiCtx.shadowBlur() + 2.0f);
        float a255 = g_uiCtx.shadowAlpha() * 255.0f + 15.0f;
        for (int k = blur; k >= 1; k--) {
            float al = a255 * (float)(blur - k + 1) / (float)blur / 2.0f / 255.0f * alpha;
            if (al <= 0.0f) continue;
            drawRoundedRect(dev, Rect(Vec2(cp.pos.x + k, cp.pos.y + k), cp.size),
                            Color(0, 0, 0, al), contentRadius);
        }
    }
    Color cpBg(c.bg3.r, c.bg3.g, c.bg3.b, 245.0f / 255.0f * alpha);
    if (L.isLR) drawRightRoundedRect(dev, cp, cpBg, contentRadius);
    else        drawBottomRoundedRect(dev, cp, cpBg, contentRadius);
    Color cpBorder(c.groupBorder.r, c.groupBorder.g, c.groupBorder.b,
                   80.0f / 255.0f * alpha);
    if (L.isLR) drawRightRoundedBorder(dev, cp, cpBorder, g_uiCtx.borderWidth(), contentRadius);
    else        drawBottomRoundedBorder(dev, cp, cpBorder, g_uiCtx.borderWidth(), contentRadius);

    // 内容页
    dock::drawContentPage(dev, win, cp, s.sel, alpha);
    // 关闭按钮：主线统一按钮系统绘制（WIN_HAS_CLOSE，矢量X），
    // 不再模块自绘（旧自绘用"✕"字符渲染存在字形缺失乱码问题）
}

bool clickDock(WindowInstance& win, const Vec2& m) {
    dock::Layout L = dock::layout(win);
    dock::State& s = win.getState<dock::State>();
    for (int i = 0; i < dock::BTN_COUNT; i++) {
        if (L.btns[i].contains(m)) { s.sel = i; return true; }
    }
    return false;
}

void hoverDock(WindowInstance& win, const Vec2& m) {
    dock::Layout L = dock::layout(win);
    dock::State& s = win.getState<dock::State>();
    s.hoverBtn = -1;
    for (int i = 0; i < dock::BTN_COUNT; i++)
        if (L.btns[i].contains(m)) { s.hoverBtn = i; break; }
}

// ============================================================
// 调试窗口（对照参考 L1718-1925；标题栏由系统画：32高@245 标题13px"调试 #N"）
// 内容：4 标签（性能/状态/输入/日志）+ 圆角内容底板 + 键值行 + 滚动条
// ============================================================
namespace dbg {

static const char* TABS[] = {"性能", "状态", "输入", "日志"};
static constexpr int TAB_COUNT = 4;

// 全局调试日志缓冲（所有调试窗口共享展示，环形上限 256 条）
static std::vector<std::string>& logs() {
    static std::vector<std::string> buf;
    return buf;
}

// 内容区高（参考 L1805-1808：行数×22 + 12）
static float pageContentH(int page) {
    float s = g_uiCtx.scale;
    if (page == 0) return (6.0f + 4 * 22.0f + 6.0f + 60.0f + 6.0f + 18.0f + 6.0f) * s;
    if (page == 1) return 3 * 22.0f * s + 12.0f * s;
    if (page == 2) return 5 * 22.0f * s + 12.0f * s;
    return 24.0f * s;
}


struct Layout {
    Rect tabs[TAB_COUNT];       // 标签（屏幕绝对坐标）
    float contentY;             // 内容底板顶 Y（绝对）
    float contentH;             // 内容底板高
    Rect contentPanel;          // 内容底板
    Rect aaBtn;                 // 本窗 AA 开关按钮（底部右侧）
    Rect tileBtn;               // 一键平铺按钮（底部）
    Rect cascadeBtn;            // 一键层叠按钮（底部）
    Rect extendBtn;             // win 专属扩展应用窗口按钮（底部最左；非 win 平台空矩形）
};
static Layout layout(const WindowInstance& win) {
    Layout L;
    float s = g_uiCtx.scale;
    float tabY = win.y + wbtn::titleBarHeight(win);
    float tabH = 28.0f * s;
    float tabW = (win.w - 16.0f * s) / (float)TAB_COUNT;
    for (int i = 0; i < TAB_COUNT; i++)
        L.tabs[i] = Rect(Vec2(win.x + 8.0f * s + i * tabW, tabY), Vec2(tabW, tabH));
    L.contentY = tabY + tabH + 4.0f * s;
    float btnRowH = 26.0f * s;  // 底部按钮行（本窗 AA 开关）
    float bottom = win.y + win.h - 4.0f * s - btnRowH;
    L.contentH = bottom - L.contentY;
    if (L.contentH < 10.0f * s) L.contentH = 10.0f * s;
    L.contentPanel = Rect(Vec2(win.x + 12.0f * s, L.contentY),
                          Vec2(win.w - 24.0f * s, L.contentH));
    L.aaBtn = Rect(Vec2(win.x + win.w - 12.0f * s - 84.0f * s, bottom + 3.0f * s),
                   Vec2(84.0f * s, 20.0f * s));
    L.cascadeBtn = Rect(Vec2(L.aaBtn.pos.x - 6.0f * s - 52.0f * s, bottom + 3.0f * s),
                        Vec2(52.0f * s, 20.0f * s));
    L.tileBtn = Rect(Vec2(L.cascadeBtn.pos.x - 6.0f * s - 52.0f * s, bottom + 3.0f * s),
                     Vec2(52.0f * s, 20.0f * s));
#ifdef _WIN32
    // win 平台专属：扩展应用窗口按钮（打开新 AppWindow，多屏办公语义；
    // 窗宽不足时按钮越出窗体被系统裁剪——用户拉大窗口即可见全貌）
    L.extendBtn = Rect(Vec2(L.tileBtn.pos.x - 6.0f * s - 76.0f * s, bottom + 3.0f * s),
                       Vec2(76.0f * s, 20.0f * s));
#endif
    return L;
}
struct State {
    int hoverTab = -1;
    bool aaOff = false;    // 本窗禁用 AA（实例级，同步 win.aaDisabled；只影响本调试窗）
    bool hoverAA = false;  // AA 按钮 hover 态
    bool hoverTile = false;    // 平铺按钮 hover 态
    bool hoverCascade = false; // 层叠按钮 hover 态
    bool hoverExtend = false;  // win 扩展应用窗口按钮 hover 态
};

// 键值行绘制（对照参考 L1835-1837：label 11px text@180 左 + val 12px accent 左偏移）
static void drawRow(RenderDevice* dev, float x, float y, float labelW,
                    const char* label, const char* val, float alpha) {
    ThemeColors& c = currentColors();
    wctl::drawTextScaled(dev, g_uiCtx.font, label,
                         Rect(Vec2(x, y), Vec2(labelW, 18.0f * g_uiCtx.scale)),
                         Color(c.text.r, c.text.g, c.text.b,
                               180.0f / 255.0f * alpha), 11.0f, 0);
    wctl::drawTextScaled(dev, g_uiCtx.font, val,
                         Rect(Vec2(x + labelW + 10.0f * g_uiCtx.scale, y),
                              Vec2(120.0f * g_uiCtx.scale, 18.0f * g_uiCtx.scale)),
                         Color(c.accent.r, c.accent.g, c.accent.b, alpha), 12.0f, 0);
}

// 统计本项目 handler_debug 窗口实例数（参考 g_debugInstances.size()）
static int debugWindowCount() {
    int n = 0;
    for (WindowInstance* w : g_layerStack.allWindows())
        if (w->config() && strcmp(w->config()->handlerName, "handler_debug") == 0)
            n++;
    return n;
}

} // namespace dbg

void AddDebugLog(const char* utf8) {
    auto& buf = dbg::logs();
    buf.push_back(utf8);
    if (buf.size() > 256) buf.erase(buf.begin());  // 环形上限
}

void renderDebug(RenderDevice* dev, WindowInstance& win, float alpha) {
    ThemeColors& c = currentColors();
    if (win.collapsed) return;  // 折叠：只留标题栏（系统已画）
    dbg::Layout L = dbg::layout(win);
    dbg::State& s = win.getState<dbg::State>();
    float sc = g_uiCtx.scale;

    // ---- 4 标签（对照参考 L1772-1789） ----
    for (int i = 0; i < dbg::TAB_COUNT; i++) {
        const Rect& r = L.tabs[i];
        bool sel = (win.tabSel == i), hov = (s.hoverTab == i);
        if (sel) {
            drawRoundedRect(dev, r, Color(c.accent.r, c.accent.g, c.accent.b,
                                          180.0f / 255.0f * alpha), 4.0f * sc);
            wctl::drawTextScaled(dev, g_uiCtx.font, dbg::TABS[i], r,
                                 Color(c.btnText.r, c.btnText.g, c.btnText.b, alpha),
                                 11.0f, 1);
        } else if (hov) {
            drawRoundedRect(dev, r, Color(c.tabBg.r, c.tabBg.g, c.tabBg.b,
                                          200.0f / 255.0f * alpha), 4.0f * sc);
            wctl::drawTextScaled(dev, g_uiCtx.font, dbg::TABS[i], r,
                                 Color(c.text.r, c.text.g, c.text.b,
                                       220.0f / 255.0f * alpha), 11.0f, 1);
        } else {
            wctl::drawTextScaled(dev, g_uiCtx.font, dbg::TABS[i], r,
                                 Color(c.text.r, c.text.g, c.text.b,
                                       160.0f / 255.0f * alpha), 11.0f, 1);
        }
    }

    // ---- 内容底板：bg3@200 圆角6（参考 L1801-1802，不乘 RadiusScale） ----
    drawRoundedRect(dev, L.contentPanel,
                    Color(c.bg3.r, c.bg3.g, c.bg3.b, 200.0f / 255.0f * alpha),
                    6.0f * sc);

    // ---- 本窗 AA 开关（底部右侧；实例级覆盖：仅本调试窗硬边，其他调试窗/全局不受影响） ----
    {
        Color btnBg;
        if (s.aaOff) {          // 关=警示红底（一眼可见本窗画质已被强制原始化）
            btnBg = Color(220.0f / 255.0f, 80.0f / 255.0f, 80.0f / 255.0f,
                          200.0f / 255.0f * alpha);
        } else if (s.hoverAA) { // 开=普通底板，hover 提示可点击
            btnBg = Color(c.tabBg.r, c.tabBg.g, c.tabBg.b, 220.0f / 255.0f * alpha);
        } else {
            btnBg = Color(c.bg3.r, c.bg3.g, c.bg3.b, 200.0f / 255.0f * alpha);
        }
        drawRoundedRect(dev, L.aaBtn, btnBg, 4.0f * sc);
        Color txt = s.aaOff ? Color(1.0f, 1.0f, 1.0f, alpha)
                            : Color(c.accent.r, c.accent.g, c.accent.b, alpha);
        wctl::drawTextScaled(dev, g_uiCtx.font, s.aaOff ? "AA:关" : "AA:开",
                             L.aaBtn, txt, 10.0f, 1);
    }

    // ---- 一键整理：平铺（网格）/ 层叠（级联），作用于所有可见面板窗 ----
    auto drawMiniBtn = [&](const Rect& r, bool hov, const char* text) {
        Color btnBg = hov ? Color(c.tabBg.r, c.tabBg.g, c.tabBg.b,
                                  220.0f / 255.0f * alpha)
                          : Color(c.bg3.r, c.bg3.g, c.bg3.b,
                                  200.0f / 255.0f * alpha);
        drawRoundedRect(dev, r, btnBg, 4.0f * sc);
        wctl::drawTextScaled(dev, g_uiCtx.font, text, r,
                             Color(c.accent.r, c.accent.g, c.accent.b, alpha),
                             10.0f, 1);
    };
    drawMiniBtn(L.tileBtn, s.hoverTile, "平铺");
    drawMiniBtn(L.cascadeBtn, s.hoverCascade, "层叠");

#ifdef _WIN32
    // ---- win 平台专属：扩展应用窗口按钮（点击新建 AppWindow，
    // 同一 UI 引擎多应用窗口并行——多屏办公语义；新窗口打开立即
    // 绘制完整基础 UI（主界面+独立悬浮窗栈），非空白等待） ----
    {
        Color btnBg = s.hoverExtend
            ? Color(c.accent.r, c.accent.g, c.accent.b, 200.0f / 255.0f * alpha)
            : Color(c.bg3.r, c.bg3.g, c.bg3.b, 200.0f / 255.0f * alpha);
        drawRoundedRect(dev, L.extendBtn, btnBg, 4.0f * sc);
        wctl::drawTextScaled(dev, g_uiCtx.font, "扩展应用窗口", L.extendBtn,
                             Color(s.hoverExtend ? 1.0f : c.btnText.r,
                                   s.hoverExtend ? 1.0f : c.btnText.g,
                                   s.hoverExtend ? 1.0f : c.btnText.b, alpha),
                             10.0f, 1);
    }
#endif

    // ---- RegionSystem 区域申请（裁剪+滚动统一系统处理） ----
    RegionHandle h = g_regions.begin(dev, win, {
        12.0f * sc, L.contentY - win.y, win.w - 24.0f * sc, L.contentH,
        win.tabSel, "debug_content"
    });
    if (!h.valid) return;

    float lineH = 22.0f * sc;
    float rowX = h.clipX + 8.0f * sc;
    float y = h.clipY + 6.0f * sc - h.scrollY;
    char buf[64];

    if (win.tabSel == 0) {
        // 性能页：4 行键值 + 帧耗时曲线 + 统计行（参考 L1831-1852 扩展）
        snprintf(buf, sizeof(buf), "%d", g_uiCtx.fps);
        dbg::drawRow(dev, rowX, y, 60.0f * sc, "FPS", buf, alpha); y += lineH;
        // 帧时间直接显示实测值（ctx.frameMs = 本帧耗时）：与下方曲线同源，
        // 旧实现用 1000/fps 反推——fps 每秒才更新，卡顿秒内反推值与实测
        // 曲线自相矛盾（用户看到"帧数与描述不匹配"的口径根源之一）
        snprintf(buf, sizeof(buf), "%.1fms", g_uiCtx.frameMs);
        dbg::drawRow(dev, rowX, y, 60.0f * sc, "帧时间", buf, alpha); y += lineH;
        snprintf(buf, sizeof(buf), "%d×%d", g_uiCtx.winW, g_uiCtx.winH);
        dbg::drawRow(dev, rowX, y, 70.0f * sc, "窗口尺寸", buf, alpha); y += lineH;
        snprintf(buf, sizeof(buf), "%.2f", g_uiCtx.scale);
        dbg::drawRow(dev, rowX, y, 70.0f * sc, "缩放系数", buf, alpha);
        y += lineH + 6.0f * sc;

        // ---- 帧耗时曲线（环形历史 64 样本；按 head 逆序展开，最旧在左最新在右） ----
        float gx = rowX;
        float gw = h.contentW - 16.0f * sc;
        float gh = 60.0f * sc;
        float gy = y;
        drawRoundedRect(dev, Rect(Vec2(gx, gy), Vec2(gw, gh)),
                        Color(c.tabBg.r, c.tabBg.g, c.tabBg.b,
                              60.0f / 255.0f * alpha), 4.0f * sc);
        int n = g_uiCtx.frameMsCount;
        if (n > 0) {
            float vals[64];
            float vmax = 0.0f, vmin = 1e9f, vsum = 0.0f;
            for (int i = 0; i < n; i++) {
                float v = g_uiCtx.frameMsHist[(g_uiCtx.frameMsHead - n + i + 64) % 64];
                vals[i] = v;
                if (v > vmax) vmax = v;
                if (v < vmin) vmin = v;
                vsum += v;
            }
            // Y 轴自适应：峰值×1.2，下限 20ms 保 60fps 参考线可见
            float yMax = vmax * 1.2f;
            if (yMax < 20.0f) yMax = 20.0f;
            float pad = 3.0f * sc;
            float gInX = gx + pad, gInW = gw - pad * 2.0f;
            float gInB = gy + gh - pad, gInH = gh - pad * 2.0f;
            // 参考线：60fps / 30fps（超出量程跳过）
            Color refC(c.text.r, c.text.g, c.text.b, 120.0f / 255.0f * alpha);
            const char* refNames[2] = {"60fps", "30fps"};
            float refMs[2] = {16.7f, 33.3f};
            for (int k = 0; k < 2; k++) {
                if (refMs[k] > yMax) continue;
                float ry = gInB - refMs[k] / yMax * gInH;
                drawLine(dev, gInX, ry, gInX + gInW, ry, 1.0f * sc, refC);
                wctl::drawTextScaled(dev, g_uiCtx.font, refNames[k],
                                     Rect(Vec2(gInX + 4.0f * sc, ry - 13.0f * sc),
                                          Vec2(40.0f * sc, 11.0f * sc)), refC, 8.0f, 0);
            }
            // 折线：相邻样本连线（accent 实线；单样本画圆点）
            Color lineC(c.accent.r, c.accent.g, c.accent.b, alpha);
            auto pxOf = [&](int i) {
                return n > 1 ? gInX + (float)i / (float)(n - 1) * gInW
                             : gInX + gInW * 0.5f;
            };
            auto pyOf = [&](float v) { return gInB - (v / yMax) * gInH; };
            if (n == 1) {
                fillCircle(dev, pxOf(0), pyOf(vals[0]), 1.5f * sc, lineC);
            } else {
                for (int i = 0; i + 1 < n; i++)
                    drawLine(dev, pxOf(i), pyOf(vals[i]),
                             pxOf(i + 1), pyOf(vals[i + 1]), 1.5f * sc, lineC);
            }
            // 统计行：均值 / 最大 / 抖动（最大-最小）
            y += gh + 6.0f * sc;
            char sbuf[96];
            snprintf(sbuf, sizeof(sbuf), "均值 %.1fms   最大 %.1fms   抖动 %.1fms",
                     vsum / n, vmax, vmax - vmin);
            wctl::drawTextScaled(dev, g_uiCtx.font, sbuf,
                                 Rect(Vec2(rowX, y),
                                      Vec2(h.contentW - 16.0f * sc, 18.0f * sc)),
                                 Color(c.text.r, c.text.g, c.text.b,
                                       160.0f / 255.0f * alpha), 10.0f, 0);
        } else {
            y += gh + 6.0f * sc;
            wctl::drawTextScaled(dev, g_uiCtx.font, "采样中…",
                                 Rect(Vec2(rowX, y),
                                      Vec2(h.contentW - 16.0f * sc, 18.0f * sc)),
                                 Color(c.text.r, c.text.g, c.text.b,
                                       140.0f / 255.0f * alpha), 10.0f, 0);
        }
        g_regions.end(dev, h, dbg::pageContentH(0));
    } else if (win.tabSel == 1) {
        // 状态页（参考 L1853-1876；本项目参数映射：主题/厚度/抗锯齿/渐变背景）
        dbg::drawRow(dev, rowX, y, 60.0f * sc, "主题",
                     currentTheme().name.c_str(), alpha); y += lineH;

        dbg::drawRow(dev, rowX, y, 60.0f * sc, "抗锯齿",
                     c.antialias ? "开启" : "关闭", alpha); y += lineH;
        dbg::drawRow(dev, rowX, y, 70.0f * sc, "渐变背景",
                     c.useGradient ? "开启" : "关闭", alpha);
        g_regions.end(dev, h, dbg::pageContentH(1));
    } else if (win.tabSel == 2) {
        // 输入页（参考 L1877-1904；数据源 = UiContext 镜像单源）
        static const char* TAB_NAMES[] = {"概览", "目录", "其他"};
        snprintf(buf, sizeof(buf), "%d", (int)g_uiCtx.mouseX);
        dbg::drawRow(dev, rowX, y, 60.0f * sc, "鼠标X", buf, alpha); y += lineH;
        snprintf(buf, sizeof(buf), "%d", (int)g_uiCtx.mouseY);
        dbg::drawRow(dev, rowX, y, 60.0f * sc, "鼠标Y", buf, alpha); y += lineH;
        int t = g_uiCtx.mainTabSel;
        dbg::drawRow(dev, rowX, y, 60.0f * sc, "标签页",
                     t >= 0 && t < 3 ? TAB_NAMES[t] : "-", alpha); y += lineH;
        snprintf(buf, sizeof(buf), "%d个", dbg::debugWindowCount());
        dbg::drawRow(dev, rowX, y, 70.0f * sc, "调试窗口", buf, alpha);
        g_regions.end(dev, h, dbg::pageContentH(2));
    } else {
        // 日志页（参考 L1905-1907 静态"暂无日志"；本项目有日志则逐行展示）
        if (dbg::logs().empty()) {
            wctl::drawTextScaled(dev, g_uiCtx.font, "暂无日志",
                                 Rect(Vec2(rowX, y),
                                      Vec2(h.contentW - 16.0f * sc, 18.0f * sc)),
                                 Color(c.text.r, c.text.g, c.text.b,
                                       140.0f / 255.0f * alpha), 10.0f, 0);
            g_regions.end(dev, h, dbg::pageContentH(3));
        } else {
            for (const std::string& entry : dbg::logs()) {
                wctl::drawTextScaled(dev, g_uiCtx.font, entry.c_str(),
                                     Rect(Vec2(rowX, y),
                                          Vec2(h.contentW - 16.0f * sc, 18.0f * sc)),
                                     Color(c.text.r, c.text.g, c.text.b,
                                           200.0f / 255.0f * alpha), 10.0f, 0);
                y += lineH;
            }
            g_regions.end(dev, h, (float)dbg::logs().size() * lineH + 12.0f * sc);
        }
    }
}

bool clickDebug(WindowInstance& win, const Vec2& m) {
    dbg::Layout L = dbg::layout(win);
    for (int i = 0; i < dbg::TAB_COUNT; i++) {
        if (L.tabs[i].contains(m)) { win.tabSel = i; return true; }
    }
    // 本窗 AA 开关：翻转后同步实例覆盖字段（只影响本调试窗，其他调试窗不受影响）
    if (L.aaBtn.contains(m)) {
        dbg::State& s = win.getState<dbg::State>();
        s.aaOff = !s.aaOff;
        win.aaDisabled = s.aaOff;
        return true;
    }
    // 一键整理：平铺/层叠所有可见面板窗（LayerStack 统一排列）
    if (L.tileBtn.contains(m)) {
        g_layerStack.tileWindows((float)g_uiCtx.winW, (float)g_uiCtx.winH);
        return true;
    }
    if (L.cascadeBtn.contains(m)) {
        g_layerStack.cascadeWindows((float)g_uiCtx.winW, (float)g_uiCtx.winH);
        return true;
    }
#ifdef _WIN32
    // win 平台专属：扩展应用窗口（走请求标记帧末统一创建——当前正处于
    // 遍历 apps_ 的帧驱动栈帧内，直接 createApp 会 push_back 使迭代器失效）
    // 主子级语义：新建窗口绑定当前应用为父级（父关子关）、480x320、
    // "test" 内容（居中一行「测试win窗口」，不绘制主界面按钮）、
    // 禁止悬浮窗（本子窗口内所有悬浮窗/悬浮窗不显示不交互）。
    if (L.extendBtn.contains(m)) {
        NewAppRequest r;
        r.w = 480;
        r.h = 320;
        r.parentId = currentApp()->id();   // 绑定父级 = 当前驱动应用
        r.content = "test";
        r.allowSuspendWindows = false;     // 子窗口禁悬浮窗
        // x/y 留 -1：宿主默认 = 父窗口位置 + 级联偏移（渲染位置可扩展指定）
        currentApp()->requestNewApp(r);
        char buf[96];
        snprintf(buf, sizeof(buf),
                 "win扩展应用窗口: 新建子窗口（父=#%d, %dx%d, test, 禁悬浮窗, 帧末创建）",
                 r.parentId, r.w, r.h);
        AddDebugLog(buf);
        return true;
    }
#endif
    return false;  // 内容区点击交给系统（滚动条拖拽/滚轮等）
}

void hoverDebug(WindowInstance& win, const Vec2& m) {
    dbg::Layout L = dbg::layout(win);
    dbg::State& s = win.getState<dbg::State>();
    s.hoverTab = -1;
    s.hoverAA = L.aaBtn.contains(m);
    s.hoverTile = L.tileBtn.contains(m);
    s.hoverCascade = L.cascadeBtn.contains(m);
#ifdef _WIN32
    s.hoverExtend = L.extendBtn.contains(m);
#endif
    for (int i = 0; i < dbg::TAB_COUNT; i++)
        if (L.tabs[i].contains(m)) { s.hoverTab = i; break; }
}

} // namespace ui