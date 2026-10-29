// AI生成
// ============================================================
// 主题工作室处理器（完整移植参考 DrawColorCustomFloatWin + 输入交互）
//
// 结构：
//   ┌────────┬─────────────────────────────┐
//   │ 侧标签  │ 内容                         │
//   │ 主题色  │ 35 主题模板列表(滚动)         │ ← tab0 子模式0
//   │        │ 12 分组卡片 55 色块(滚动)     │    子模式1 点击开颜色选择器
//   │        │ 自定义背景 4 类型编辑         │    子模式2 纯色/渐变/网格/图片
//   │ 动态背景│ 28 动态效果+速度/密度滑条     │ ← tab1
//   │ 按钮    │ AA复选框+圆润/阴影/厚度滑条   │ ← tab2
//   │ 其他UI  │ 复选框/六下拉/页面缩放滑条    │ ← tab3
//   └────────┴─────────────────────────────┘
//   └────────┴─────────────────────────────┘
//
// 数据全部配置表驱动：
//   - 55 色项表 kColorItems（成员指针，新增可编辑颜色 = 表里加一行）
//   - 12 颜色分组表 kColorGroups（详细模式卡片网格）
//   - 28 动态背景名表 kDynBgNames（枚举同序）
// 滚动区域交给 RegionSystem（系统裁剪/滚动条/滚轮）；滑条为处理器内
// 自拖拽（drag 枚举驱动，click 按下 + drag 取值 + release 复位）。
// ============================================================
#include "WinHandlers.h"
#include "UiContext.h"
#include "RegionSystem.h"
#include "LayerStack.h"

#include "WindowButtons.h"
#include "WinControls.h"
#include "DropdownSystem.h"
#include "../page/PageControls.h"   // 全局标准页面控件（滑条等）——统一走标准调用
#include "../Element.h"
#include "../Theme.h"
#include "../Background.h"
#include "../DynamicBg.h"
#include <cstdio>
#include <fstream>
#include <string>

// ---- 文件对话框（平台适配：Win32 原生；其他平台回退固定路径） ----
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#endif

namespace ui {

// ============================================================
// 平台文件辅助（导入/导出主题、选择背景图片）
// ============================================================
#ifdef _WIN32

static std::wstring utf8ToWide(const char* s) {
    // 注意：返回串不带终止 NUL（原实现带 NUL 会污染 lpstrFilter 布局，
    // 曾导致对话框过滤模式为空串、图片文件全部被隐藏无法选择）
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    std::wstring w((size_t)(n > 1 ? n - 1 : 0), L'\0');
    if (n > 1) MultiByteToWideChar(CP_UTF8, 0, s, -1, &w[0], n);
    return w;
}
static std::string wideToUtf8(const wchar_t* s) {
    int n = WideCharToMultiByte(CP_UTF8, 0, s, -1, nullptr, 0, nullptr, nullptr);
    std::string u((size_t)(n > 1 ? n - 1 : 0), '\0');
    if (n > 1) WideCharToMultiByte(CP_UTF8, 0, s, -1, &u[0], n, nullptr, nullptr);
    return u;
}

// 打开文件对话框（单过滤器；返回 false = 用户取消）
// filter 布局必须为 "描述\0模式\0\0"（c_str 隐式终止符补末尾 NUL）
static bool pickFileOpen(std::string& outPath, const char* titleUtf8,
                         const char* descUtf8, const char* patternUtf8) {
    wchar_t file[MAX_PATH] = L"";
    std::wstring filter = utf8ToWide(descUtf8);
    filter.push_back(L'\0');
    filter += utf8ToWide(patternUtf8);
    filter.push_back(L'\0');
    std::wstring title = utf8ToWide(titleUtf8);
    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetActiveWindow();   // 挂 owner 防对话框弹到主窗口后面看不到
    ofn.lpstrFilter = filter.c_str();
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title.c_str();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&ofn)) return false;
    outPath = wideToUtf8(file);
    return true;
}

// 保存文件对话框（带默认扩展名 + 默认文件名）
static bool pickFileSave(std::string& outPath, const char* titleUtf8,
                         const char* descUtf8, const char* patternUtf8,
                         const char* defExtUtf8, const char* defNameUtf8 = nullptr) {
    wchar_t file[MAX_PATH] = L"";
    if (defNameUtf8 && defNameUtf8[0]) {
        std::wstring w = utf8ToWide(defNameUtf8);
        wcsncpy(file, w.c_str(), MAX_PATH - 1);
        file[MAX_PATH - 1] = 0;
    }
    std::wstring filter = utf8ToWide(descUtf8);
    filter.push_back(L'\0');
    filter += utf8ToWide(patternUtf8);
    filter.push_back(L'\0');
    std::wstring title = utf8ToWide(titleUtf8);
    std::wstring defExt = utf8ToWide(defExtUtf8);
    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetActiveWindow();   // 挂 owner 防对话框弹到主窗口后面看不到
    ofn.lpstrFilter = filter.c_str();
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title.c_str();
    ofn.lpstrDefExt = defExt.c_str();
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetSaveFileNameW(&ofn)) return false;
    outPath = wideToUtf8(file);
    return true;
}

#else  // 非 Win32：固定路径回退（功能可用的最小实现）

static bool pickFileOpen(std::string& outPath, const char*, const char*,
                         const char*, const char* fallbackPath) {
    outPath = fallbackPath;
    return true;
}
static bool pickFileSave(std::string& outPath, const char*, const char*,
                         const char*, const char*, const char* fallbackPath) {
    outPath = fallbackPath;
    return true;
}

#endif


namespace studio {

// ====== 可编辑颜色项配置表（55 项，成员指针驱动，零硬编码） ======
// 与参考 CUSTOM_COLOR_COUNT=55 完全一致；新增可编辑颜色 = 表里加一行。
struct ColorItemDef {
    const char* name;                    // 显示名
    Color ThemeColors::*ptr;             // 目标颜色成员指针
};
static const ColorItemDef kColorItems[] = {
    { "背景色1",       &ThemeColors::bg1 },   { "背景色2",       &ThemeColors::bg2 },
    { "背景色3",       &ThemeColors::bg3 },   { "背景色4",       &ThemeColors::bg4 },
    { "文字色",        &ThemeColors::text },  { "标题色",        &ThemeColors::title },
    { "强调色",        &ThemeColors::accent },{ "次强调色",      &ThemeColors::accent2 },
    { "按钮背景1",     &ThemeColors::btnBg1 },{ "按钮背景2",     &ThemeColors::btnBg2 },
    { "按钮悬停1",     &ThemeColors::btnHover1 },{ "按钮悬停2",  &ThemeColors::btnHover2 },
    { "按钮按下",      &ThemeColors::btnPressed },{ "按钮文字",  &ThemeColors::btnText },
    { "按钮边框",      &ThemeColors::btnBorder },{ "按钮悬停边框", &ThemeColors::btnHoverBorder },
    { "输入框背景",    &ThemeColors::inputBg },{ "输入框边框",   &ThemeColors::inputBorder },
    { "输入框文字",    &ThemeColors::inputText },
    { "开关-关背景",   &ThemeColors::toggleOffBg },{ "开关-开背景", &ThemeColors::toggleOnBg },
    { "开关-关边框",   &ThemeColors::toggleOffBorder },{ "开关-开边框", &ThemeColors::toggleOnBorder },
    { "开关-关滑块",   &ThemeColors::toggleOffSlider },{ "开关-开滑块", &ThemeColors::toggleOnSlider },
    { "滑槽色",        &ThemeColors::sliderGroove },{ "滑块色",   &ThemeColors::sliderHandle },
    { "滑块边框",      &ThemeColors::sliderHandleBorder },
    { "复选框边框",    &ThemeColors::checkBorder },{ "复选框背景", &ThemeColors::checkBg },
    { "复选框选中背景", &ThemeColors::checkCheckedBg },{ "复选框选中边框", &ThemeColors::checkCheckedBorder },
    { "标签背景",      &ThemeColors::tabBg },{ "标签选中背景", &ThemeColors::tabSelBg },
    { "标签文字",      &ThemeColors::tabText },{ "标签选中文字", &ThemeColors::tabSelText },
    { "菜单背景",      &ThemeColors::menuBg },{ "菜单文字",     &ThemeColors::menuText },
    { "菜单选中背景",  &ThemeColors::menuSelBg },{ "菜单选中文字", &ThemeColors::menuSelText },
    { "工具栏背景",    &ThemeColors::toolbarBg },{ "工具栏悬停", &ThemeColors::toolbarHoverBg },
    { "状态栏背景",    &ThemeColors::statusBg },{ "状态栏文字",  &ThemeColors::statusText },
    { "分组边框",      &ThemeColors::groupBorder },{ "分组标题背景", &ThemeColors::groupTitleBg },
    { "分组标题",      &ThemeColors::groupTitle },
    { "卡片背景1",     &ThemeColors::cardBg1 },{ "卡片背景2",    &ThemeColors::cardBg2 },
    { "卡片悬停1",     &ThemeColors::cardHoverBg1 },{ "卡片悬停2", &ThemeColors::cardHoverBg2 },
    { "列表背景",      &ThemeColors::listBg },{ "列表边框",     &ThemeColors::listBorder },
    { "列表文字",      &ThemeColors::listText },{ "列表选中背景", &ThemeColors::listSelBg },
};
static const int kColorItemCount = (int)(sizeof(kColorItems) / sizeof(kColorItems[0]));

// ====== 12 颜色分组（详细子模式卡片网格；startIndex/count 引用上表） ======
struct ColorGroupDef {
    const char* name;
    int startIndex;
    int count;
};
static const ColorGroupDef kColorGroups[] = {
    { "背景", 0, 4 },    { "主色", 4, 4 },      { "按钮", 8, 8 },
    { "输入", 16, 3 },   { "开关", 19, 6 },     { "滑块", 25, 3 },
    { "复选", 28, 4 },   { "标签", 32, 4 },     { "菜单", 36, 4 },
    { "状态", 40, 4 },   { "分组卡片", 44, 7 }, { "列表", 51, 4 },
};
static const int kColorGroupCount = (int)(sizeof(kColorGroups) / sizeof(kColorGroups[0]));

// ====== 动态背景效果名（枚举同序，DYN_BG_COUNT=29） ======
static const char* kDynBgNames[] = {
    "无", "极地之夜", "水波涟漪", "六边蜂窝",
    "气泡上升", "星轨旋涡", "音波频谱",
    "波纹干涉", "数字雨", "能量护盾", "蜂巢矩阵",
    "涟漪荡漾", "光谱波浪", "矩阵代码", "量子场",
    "漩涡矩阵", "熔岩矩阵", "暗物质流", "时空网格",
    "光子矩阵", "电浆矩阵", "霓虹脉冲", "翡翠矩阵",
    "翠竹摇曳", "机械齿轮", "幻影棋盘", "星尘交响",
    "时空织锦", "时空蝶变"
};

// ====== 滑条拖拽目标（id 驱动，click 按下 / drag 取值 / release 复位） ======
enum DragTarget {
    DRAG_NONE = 0,
    DRAG_GRAD_R0, DRAG_GRAD_R1, DRAG_GRAD_R2, DRAG_GRAD_R3,  // 渐变四角强度
    DRAG_GRID_SPACING, DRAG_GRID_WIDTH, DRAG_GRID_ALPHA,      // 网格间距/粗细/线透明
    DRAG_IMG_ALPHA,                                           // 图片透明度
    DRAG_DYN_SPEED, DRAG_DYN_DENSITY,                         // 动态背景速度/密度
    DRAG_RADIUS, DRAG_SHADOW, DRAG_THICKNESS,                // 圆润/阴影/厚度（tab2）
    DRAG_UI_SCALE,                                            // 全局页面缩放（tab3）
};

// ====== 侧标签数量（主题色/动态背景/按钮/其他UI；新增标签 = kSideItems 加一项 + 此常量 +1） ======
static constexpr int SIDE_TAB_COUNT = 4;

// ====== 模块私有状态 ======
struct State {
    int colorCustomTab = 0;         // 侧标签 0主题色/1动态背景/2按钮/3其他UI
    int hoverColorCustomTab = -1;
    int colorEditMode = 0;          // 主题色子模式 0模板/1详细/2自定义
    int hoverColorEditMode = -1;
    int selectedColorItem = -1;     // 详细模式选中色项（写回目标）
    int hoverColorItem = -1;
    int hoverTemplateBtn = -1;      // 模板行更改按钮 hover（行号）
    bool hoverImportBtn = false, hoverExportBtn = false;
    bool hoverResetBtn = false, hoverSaveBtn = false;
    int hoverBgType = -1;           // 自定义背景类型按钮 hover
    int hoverBgSwatch = -1;         // 背景色块 hover（槽位 0-6）
    bool hoverBgLoadBtn = false;
    bool hoverBgFillMode[3] = { false, false, false };
    bool hoverBgApplyBtn = false, hoverBgResetBtn = false;
    int hoverDynBgType = -1;
    bool hoverDynApplyBtn = false, hoverDynResetBtn = false;
    bool hoverAACheck = false;      // tab2 抗锯齿复选框 hover
    bool hoverRadiusSlider = false, hoverShadowSlider = false, hoverThicknessSlider = false;
    bool hoverCheckTick = false;    // tab3 复选框显示勾 hover
    bool hoverCircGrad = false;     // tab3 复选框圆形进度条渐变 hover
    int drag = DRAG_NONE;           // 滑条拖拽目标
    float pendingUiScale = 1.0f;    // UI 缩放拖动暂存值（松手才提交，避免拖动中每帧 resize 抖动）

};

// ====== 与 RegionSystem 滚动条预留宽度一致（系统侧 4 + 2*2） ======
static constexpr float kBarSpace = 8.0f;

// ============================================================
// 布局（单源计算：render/hover/click 共用同一次几何计算）
// ============================================================
struct Layout {
    float scale = 1;
    float titleH = 0;
    // 侧栏
    float sideW = 0, sideY = 0;
    float sideItemH = 0, sideStartY = 0;
    // 内容区
    float contentX = 0, contentW = 0, contentTop = 0, contentBottom = 0;
    // tab0 子标签
    float subTabH = 0, subTabW = 0, contentAreaTop = 0;
    // ---- 子模式0 模板列表 ----
    float rowH = 0, rowGap = 0, numW = 0, nameW = 0;
    float swatch = 0, swatchGap = 0;
    float changeBtnW = 0, changeBtnH = 0;
    // ---- 子模式1 详细网格 ----
    float cardGap = 0, cardW = 0;
    float gridSwatch = 0, gridSwatchStep = 0;
    int swatchPerRow = 1;
    float rowHeights[4] = { 0, 0, 0, 0 };
    float totalGridH = 0, totalContentH = 0;
    float detailBtnW = 0, detailBtnH = 0, detailBtnGap = 0;
    // ---- 子模式2 自定义背景 ----
    float previewH = 0, ctrlH = 0;
    float typeRowY = 0, typeBtnW = 0, typeBtnGap = 0;
    Rect solidSwatch;
    Rect gradSwatch[4], gradSlider[4];
    float gradRowY = 0, gradStrengthY = 0;
    Rect gridBgSwatch, gridLineSwatch;
    Rect spacingSlider, widthSlider, gridAlphaSlider;
    float gridRowY1 = 0, gridRowY2 = 0;
    Rect loadBtn, imgAlphaSlider, fillModeBtn[3];
    float imgRowY = 0, imgPathY = 0;
    Rect bgApplyBtn, bgResetBtn;
    // ---- tab1 动态背景 ----
    float dynPreviewH = 0;
    Rect dynEffectBtn[DYN_BG_COUNT];
    float effectRowY = 0;
    Rect speedSlider, densitySlider;
    Rect dynApplyBtn, dynResetBtn;
    // ---- tab2 按钮（视觉增强实时调节） ----
    Rect aaCheck;                   // 抗锯齿复选框热区（含文字）
    Rect aaBox;                     // 复选框方盒
    Rect radiusSlider, shadowSlider, thicknessSlider;
    // ---- tab3 其他UI（复选框显示勾/圆形进度条渐变 + 开关风格下拉） ----
    Rect tickCheck;                 // 复选框"显示勾"热区（含文字）
    Rect tickBox;                   // 复选框方盒
    Rect circCheck;                 // 复选框"圆形进度条渐变"热区（含文字）
    Rect circBox;                   // 复选框方盒
    Rect dropdownBtn[7];            // 七行下拉按钮（0开关风格/1开关扩展/2滑动条绘制/3全局分辨率/4悬浮球绘制/5线程模式/6分割线风格；列表位置由 DropdownSystem 自适应算）
    Rect uiScaleSlider;             // 全局页面缩放滑条（50-200%，第 5 行）
    float btnRowY = 0;
};

static Layout layout(const WindowInstance& win) {
    Layout L;
    float s = g_uiCtx.scale;
    L.scale = s;
    L.titleH = win.hasTitleBar() ? wbtn::titleBarHeight(win) : 0.0f;

    // ---- 侧栏 ----
    L.sideW = 80.0f * s;
    L.sideY = win.y + L.titleH;
    L.sideItemH = 28.0f * s;
    L.sideStartY = L.sideY + 8.0f * s;

    // ---- 内容区 ----
    L.contentX = win.x + L.sideW + 8.0f * s;
    L.contentW = win.w - L.sideW - 16.0f * s;
    L.contentTop = L.sideY + 4.0f * s;
    L.contentBottom = win.y + win.h - 4.0f * s;

    // ---- tab0 子标签 ----
    L.subTabH = 24.0f * s;
    L.subTabW = (L.contentW - 8.0f * s) / 3.0f;
    L.contentAreaTop = L.contentTop + L.subTabH + 6.0f * s;

    // ---- 子模式0 模板列表 ----
    L.rowH = 30.0f * s;
    L.rowGap = 2.0f * s;
    L.numW = 20.0f * s;
    L.nameW = 70.0f * s;
    L.swatch = 16.0f * s;
    L.swatchGap = 4.0f * s;
    L.changeBtnW = 50.0f * s;
    L.changeBtnH = 22.0f * s;

    // ---- 子模式1 详细网格（3 列卡片 + 底部四按钮） ----
    L.cardGap = 6.0f * s;
    L.cardW = (L.contentW - 2.0f * L.cardGap) / 3.0f;
    L.gridSwatch = 18.0f * s;
    L.gridSwatchStep = 20.0f * s;      // swatch + gap
    L.swatchPerRow = (int)((L.cardW - 12.0f * s) / L.gridSwatchStep);
    if (L.swatchPerRow < 1) L.swatchPerRow = 1;
    for (int row = 0; row < 4; row++) {
        float maxCardH = 0;
        for (int col = 0; col < 3; col++) {
            int gi = row * 3 + col;
            if (gi >= kColorGroupCount) break;
            int swatchRows = (kColorGroups[gi].count + L.swatchPerRow - 1) / L.swatchPerRow;
            float cardH = 16.0f * s + swatchRows * L.gridSwatchStep + 4.0f * s;
            if (cardH > maxCardH) maxCardH = cardH;
        }
        L.rowHeights[row] = maxCardH;
    }
    L.totalGridH = 0;
    for (int row = 0; row < 4; row++) L.totalGridH += L.rowHeights[row] + L.cardGap;
    L.detailBtnH = 24.0f * s;
    L.detailBtnW = 70.0f * s;
    L.detailBtnGap = 8.0f * s;
    L.totalContentH = L.totalGridH + L.cardGap + L.detailBtnH + 4.0f * s;

    // ---- 子模式2 自定义背景 ----
    L.previewH = 70.0f * s;
    L.ctrlH = 22.0f * s;
    L.typeBtnW = 50.0f * s;
    L.typeBtnGap = 4.0f * s;
    L.typeRowY = L.contentAreaTop + L.previewH + 8.0f * s;
    float cy = L.typeRowY + L.ctrlH + 8.0f * s;
    switch (g_bgCustom.type) {
        case 0:  // 纯色：色块 28s + Hex 文字
            L.solidSwatch = Rect(Vec2(L.contentX + 40.0f * s, cy), Vec2(28.0f * s, 28.0f * s));
            cy += L.ctrlH + 8.0f * s;
            break;
        case 1: {  // 渐变：四角色块 24s + 四强度滑条
            for (int i = 0; i < 4; i++)
                L.gradSwatch[i] = Rect(Vec2(L.contentX + 40.0f * s + i * 28.0f * s, cy),
                                       Vec2(24.0f * s, 24.0f * s));
            L.gradRowY = cy;
            cy += 24.0f * s + 16.0f * s;
            L.gradStrengthY = cy;
            for (int i = 0; i < 4; i++)
                L.gradSlider[i] = Rect(Vec2(L.gradSwatch[i].pos.x, cy + 3.0f * s),
                                       Vec2(24.0f * s, 16.0f * s));
            cy += L.ctrlH + 4.0f * s;
            break;
        }
        case 2: {  // 网格：底/线色块 + 间距/粗细/透明滑条
            L.gridRowY1 = cy;
            L.gridBgSwatch = Rect(Vec2(L.contentX + 40.0f * s, cy), Vec2(24.0f * s, 24.0f * s));
            L.gridLineSwatch = Rect(Vec2(L.contentX + 40.0f * s + 24.0f * s + 12.0f * s + 40.0f * s, cy),
                                    Vec2(24.0f * s, 24.0f * s));
            cy += 24.0f * s + 8.0f * s;
            L.spacingSlider = Rect(Vec2(L.contentX + 40.0f * s, cy + 3.0f * s),
                                   Vec2(L.contentW - 80.0f * s, 16.0f * s));
            cy += L.ctrlH + 4.0f * s;
            L.gridRowY2 = cy;
            L.widthSlider = Rect(Vec2(L.contentX + 40.0f * s, cy + 3.0f * s),
                                 Vec2(L.contentW * 0.4f - 40.0f * s, 16.0f * s));
            L.gridAlphaSlider = Rect(Vec2(L.contentX + L.contentW * 0.5f + 40.0f * s, cy + 3.0f * s),
                                     Vec2(L.contentW * 0.5f - 80.0f * s, 16.0f * s));
            cy += L.ctrlH + 4.0f * s;
            break;
        }
        case 3: {  // 图片：加载按钮 + 路径 + 透明度 + 填充模式
            L.imgRowY = cy;
            L.loadBtn = Rect(Vec2(L.contentX + 40.0f * s, cy), Vec2(100.0f * s, 28.0f * s));
            L.imgPathY = cy + 28.0f * s + 4.0f * s;
            cy += 28.0f * s + 22.0f * s;
            L.imgAlphaSlider = Rect(Vec2(L.contentX + 40.0f * s, cy + 3.0f * s),
                                    Vec2(L.contentW - 80.0f * s, 16.0f * s));
            cy += L.ctrlH + 4.0f * s;
            for (int i = 0; i < 3; i++)
                L.fillModeBtn[i] = Rect(Vec2(L.contentX + 40.0f * s + i * 54.0f * s, cy),
                                        Vec2(50.0f * s, L.ctrlH));
            cy += L.ctrlH + 4.0f * s;
            break;
        }
    }
    cy += L.ctrlH + 8.0f * s;   // 公共底部按钮行
    {
        float applyW = 80.0f * s, resetW = 60.0f * s, gap = 8.0f * s;
        float totalW = applyW + gap + resetW;
        float bx = L.contentX + (L.contentW - totalW) * 0.5f;
        L.bgApplyBtn = Rect(Vec2(bx, cy), Vec2(applyW, 24.0f * s));
        L.bgResetBtn = Rect(Vec2(bx + applyW + gap, cy), Vec2(resetW, 24.0f * s));
    }

    // ---- tab1 动态背景 ----
    L.dynPreviewH = 80.0f * s;
    {
        float ctrlY = L.contentTop + L.dynPreviewH + 8.0f * s;
        float effectBtnW = (L.contentW - 40.0f * s - 8.0f * s) / 3.0f;
        int effectRows = (DYN_BG_COUNT + 2) / 3;
        for (int i = 0; i < DYN_BG_COUNT; i++) {
            int col = i % 3, row = i / 3;
            L.dynEffectBtn[i] = Rect(Vec2(L.contentX + 40.0f * s + col * (effectBtnW + 4.0f * s),
                                          ctrlY + row * (L.ctrlH + 2.0f * s)),
                                     Vec2(effectBtnW, L.ctrlH));
        }
        ctrlY += effectRows * (L.ctrlH + 2.0f * s);
        L.effectRowY = ctrlY;
        L.speedSlider = Rect(Vec2(L.contentX + 40.0f * s, ctrlY + 3.0f * s),
                             Vec2(L.contentW - 80.0f * s, 16.0f * s));
        ctrlY += L.ctrlH + 4.0f * s;
        L.densitySlider = Rect(Vec2(L.contentX + 40.0f * s, ctrlY + 3.0f * s),
                               Vec2(L.contentW - 80.0f * s, 16.0f * s));
        ctrlY += L.ctrlH + 8.0f * s;
        float applyW = 80.0f * s, resetW = 60.0f * s, gap = 8.0f * s;
        float totalW = applyW + gap + resetW;
        float bx = L.contentX + (L.contentW - totalW) * 0.5f;
        L.dynApplyBtn = Rect(Vec2(bx, ctrlY), Vec2(applyW, 24.0f * s));
        L.dynResetBtn = Rect(Vec2(bx + applyW + gap, ctrlY), Vec2(resetW, 24.0f * s));
    }

    // ---- tab2 按钮（抗锯齿复选框 + 圆润/阴影强度滑条） ----
    {
        float y = L.contentTop + 14.0f * s;
        float boxS = 18.0f * s;
        L.aaBox = Rect(Vec2(L.contentX + 8.0f * s, y), Vec2(boxS, boxS));
        L.aaCheck = Rect(Vec2(L.contentX + 8.0f * s, y - 3.0f * s),
                         Vec2(120.0f * s, boxS + 6.0f * s));
        y += boxS + 18.0f * s;
        L.btnRowY = y;
        L.radiusSlider = Rect(Vec2(L.contentX + 60.0f * s, y),
                              Vec2(L.contentW - 130.0f * s, 16.0f * s));
        y += L.ctrlH + 10.0f * s;
        L.shadowSlider = Rect(Vec2(L.contentX + 60.0f * s, y),
                              Vec2(L.contentW - 130.0f * s, 16.0f * s));
        y += L.ctrlH + 10.0f * s;
        L.thicknessSlider = Rect(Vec2(L.contentX + 60.0f * s, y),
                                 Vec2(L.contentW - 130.0f * s, 16.0f * s));
    }

    // ---- tab3 其他UI（前两行复选框"显示勾"/"圆形进度条渐变"，后三行下拉） ----
    {
        float y = L.contentTop + 14.0f * s;
        float boxS = 18.0f * s;
        L.tickBox = Rect(Vec2(L.contentX + 8.0f * s, y), Vec2(boxS, boxS));
        L.tickCheck = Rect(Vec2(L.contentX + 8.0f * s, y - 3.0f * s),
                           Vec2(140.0f * s, boxS + 6.0f * s));
        y += boxS + 18.0f * s;
        L.circBox = Rect(Vec2(L.contentX + 8.0f * s, y), Vec2(boxS, boxS));
        L.circCheck = Rect(Vec2(L.contentX + 8.0f * s, y - 3.0f * s),
                           Vec2(160.0f * s, boxS + 6.0f * s));
        y += boxS + 18.0f * s;
        for (int i = 0; i < 7; i++) {
            L.dropdownBtn[i] = Rect(Vec2(L.contentX + 134.0f * s, y),
                                    Vec2(120.0f * s, 24.0f * s));
            y += L.ctrlH + 10.0f * s;
        }
        // 第 5 行：全局页面缩放滑条（50-200%，与下拉按钮同列对齐）
        L.uiScaleSlider = Rect(Vec2(L.contentX + 134.0f * s, y + 3.0f * s),
                               Vec2(120.0f * s, 16.0f * s));
    }
    return L;
}

// ============================================================
// 绘制助手（全部主题色驱动；视觉对齐参考三态样式）
// ============================================================

// 三态分段按钮：选中(accent+白字)/hover(tabBg)/普通(bg2)
static void drawSegBtn(RenderDevice* dev, const Rect& r, const char* utf8,
                       bool selected, bool hover, float a) {
    ThemeColors& c = currentColors();
    float s = g_uiCtx.scale;
    if (selected) {
        drawRoundedRect(dev, r, Color(c.accent.r, c.accent.g, c.accent.b, 220.0f / 255.0f * a), 4.0f * s);
        wctl::drawTextCenterAt(dev, g_uiCtx.smallFont, utf8, r, Color(1, 1, 1, a));
    } else if (hover) {
        drawRoundedRect(dev, r, Color(c.tabBg.r, c.tabBg.g, c.tabBg.b, 200.0f / 255.0f * a), 4.0f * s);
        wctl::drawTextCenterAt(dev, g_uiCtx.smallFont, utf8, r,
                               Color(c.text.r, c.text.g, c.text.b, 220.0f / 255.0f * a));
    } else {
        drawRoundedRect(dev, r, Color(c.bg2.r, c.bg2.g, c.bg2.b, 150.0f / 255.0f * a), 4.0f * s);
        wctl::drawTextCenterAt(dev, g_uiCtx.smallFont, utf8, r,
                               Color(c.text.r, c.text.g, c.text.b, 160.0f / 255.0f * a));
    }
}

// 参考风格滑条：圆角槽 + accent 填充 + 数值文字叠加
static void drawBarSlider(RenderDevice* dev, const Rect& r, float t,
                          const char* valText, float a) {
    ThemeColors& c = currentColors();
    t = clampf(t, 0.0f, 1.0f);
    drawRoundedRect(dev, r, Color(c.bg2.r, c.bg2.g, c.bg2.b, 200.0f / 255.0f * a),
                    r.size.y * 0.5f);
    if (t > 0.001f)
        drawRoundedRect(dev, Rect(r.pos, Vec2(r.size.x * t, r.size.y)),
                        Color(c.accent.r, c.accent.g, c.accent.b, 220.0f / 255.0f * a),
                        r.size.y * 0.5f);
    if (valText)
        wctl::drawTextCenterAt(dev, g_uiCtx.smallFont, valText, r,
                               Color(c.text.r, c.text.g, c.text.b, a));
}

// 右侧数值框（间距/粗细/透明度等滑条的伴随数值）
static void drawValText(RenderDevice* dev, const Rect& slider, const char* valText, float a) {
    ThemeColors& c = currentColors();
    float s = g_uiCtx.scale;
    Rect vr(Vec2(slider.pos.x + slider.size.x + 4.0f * s, slider.pos.y - 3.0f * s),
            Vec2(36.0f * s, 22.0f * s));
    wctl::drawTextCenterAt(dev, g_uiCtx.smallFont, valText, vr,
                           Color(c.text.r, c.text.g, c.text.b, a));
}

// 行标签（"类型:"/"颜色:"等左对齐小字）
static void drawLabel(RenderDevice* dev, float x, float y, float h,
                      const char* utf8, float a) {
    ThemeColors& c = currentColors();
    wctl::drawTextLeftAt(dev, g_uiCtx.smallFont, utf8, Vec2(x, y), h,
                         Color(c.text.r, c.text.g, c.text.b, 200.0f / 255.0f * a));
}

// 可点击色块（hover 边框高亮 accent）
static void drawSwatch(RenderDevice* dev, const Rect& r, const Color& col,
                       bool hover, float a) {
    ThemeColors& c = currentColors();
    float s = g_uiCtx.scale;
    drawRoundedRect(dev, r, Color(col.r, col.g, col.b, a), 4.0f * s);
    Color bc = hover ? Color(c.accent.r, c.accent.g, c.accent.b, 180.0f / 255.0f * a)
                     : Color(c.inputBorder.r, c.inputBorder.g, c.inputBorder.b, 180.0f / 255.0f * a);
    drawRoundedBorder(dev, r, bc, 1.0f * s, 4.0f * s);
}

// 操作按钮：普通(bg2/hover tabBg)；primary=保存/应用类(accent/hover accent2)
static void drawActionBtn(RenderDevice* dev, const Rect& r, const char* utf8,
                          bool hover, bool primary, float a) {
    ThemeColors& c = currentColors();
    float s = g_uiCtx.scale;
    Color bg = primary ? (hover ? c.accent2 : c.accent) : (hover ? c.tabBg : c.bg2);
    drawRoundedRect(dev, r, Color(bg.r, bg.g, bg.b, 220.0f / 255.0f * a), 4.0f * s);
    drawRoundedBorder(dev, r, Color(c.accent2.r, c.accent2.g, c.accent2.b, 180.0f / 255.0f * a),
                      1.0f * s, 4.0f * s);
    if (primary)
        wctl::drawTextCenterAt(dev, g_uiCtx.smallFont, utf8, r, Color(1, 1, 1, a));
    else
        wctl::drawTextCenterAt(dev, g_uiCtx.smallFont, utf8, r,
                               Color(c.text.r, c.text.g, c.text.b, a));
}

// ============================================================
// 命中辅助（render/hover/click 共用同一几何算法）
// ============================================================

// 滚动区域实际内容宽（有滚动条时扣系统预留宽度，与 render 一致）
static float regionContentW(const Layout& L, const RegionState* r) {
    return (r && r->hasScrollbar) ? L.contentW - kBarSpace * L.scale : L.contentW;
}

// 模板行更改按钮矩形（i = 行号）
static Rect templateChangeBtn(const Layout& L, const RegionState* r, int i) {
    float cw = regionContentW(L, r);
    float scrollY = r ? r->scrollY : 0.0f;
    float rowY = L.contentAreaTop + i * (L.rowH + L.rowGap) - scrollY;
    return Rect(Vec2(L.contentX + cw - L.changeBtnW - 6.0f * L.scale,
                     rowY + (L.rowH - L.changeBtnH) * 0.5f),
                Vec2(L.changeBtnW, L.changeBtnH));
}

// 详细模式：命中色块（返回 55 项索引，-1 = 未命中）
static int hitDetailSwatch(const Layout& L, const RegionState* r, const Vec2& m) {
    if (!r || !r->view.contains(m)) return -1;
    float scrollY = r->scrollY;
    float rowY = L.contentAreaTop - scrollY;
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 3; col++) {
            int gi = row * 3 + col;
            if (gi >= kColorGroupCount) break;
            float cardX = L.contentX + col * (L.cardW + L.cardGap);
            float cardY = rowY;
            for (int si = 0; si < kColorGroups[gi].count; si++) {
                int itemIdx = kColorGroups[gi].startIndex + si;
                float wx = cardX + 6.0f * L.scale + (si % L.swatchPerRow) * L.gridSwatchStep;
                float wy = cardY + 16.0f * L.scale + (si / L.swatchPerRow) * L.gridSwatchStep;
                if (m.x >= wx && m.x <= wx + L.gridSwatch &&
                    m.y >= wy && m.y <= wy + L.gridSwatch)
                    return itemIdx;
            }
        }
        rowY += L.rowHeights[row] + L.cardGap;
    }
    return -1;
}

// 详细模式：四按钮矩形（idx 0导入/1导出/2重置/3保存）
static Rect detailBtnRect(const Layout& L, const RegionState* r, int idx) {
    float cw = regionContentW(L, r);
    float scrollY = r ? r->scrollY : 0.0f;
    float totalW = L.detailBtnW * 4 + L.detailBtnGap * 3;
    float startX = L.contentX + (cw - totalW) * 0.5f;
    float y = L.contentAreaTop + L.totalGridH + L.cardGap - scrollY;
    return Rect(Vec2(startX + idx * (L.detailBtnW + L.detailBtnGap), y),
                Vec2(L.detailBtnW, L.detailBtnH));
}

// 打开颜色选择器（绑定 55 色项 + 停靠本窗口右侧）
static void openPickerForThemeItem(WindowInstance& win, int itemIdx) {
    BindColorPickerThemeItem(g_currentTheme, itemIdx);
    g_layerStack.pendingOpen(FindWindowConfigIndex("color_picker"));
    SetColorPickerSpawnNear(win.id);
}

// 打开颜色选择器（绑定背景槽位 + 停靠本窗口右侧）
static void openPickerForBgSlot(WindowInstance& win, int slot) {
    BindColorPickerBgSlot(slot);
    g_layerStack.pendingOpen(FindWindowConfigIndex("color_picker"));
    SetColorPickerSpawnNear(win.id);
}

} // namespace studio

// ============================================================
// 55 色项表共享访问（颜色选择器绑定/其他模块复用，单源零复制）
// ============================================================
Color ThemeColors::*ColorItemMember(int itemIndex) {
    if (itemIndex < 0 || itemIndex >= studio::kColorItemCount) return nullptr;
    return studio::kColorItems[itemIndex].ptr;
}

// ============================================================
// 渲染
// ============================================================
void renderThemeStudio(RenderDevice* dev, WindowInstance& win, float alpha) {
    ThemeColors& c = currentColors();
    studio::State& st = win.getState<studio::State>();
    studio::Layout L = studio::layout(win);
    float a = alpha;
    float s = L.scale;

    // ---- 侧栏底色 + 右分隔线（系统画窗口底板，这里叠加侧栏专属色） ----
    // 侧栏底形状：左下角继承窗口底板圆角（配置驱动，与底板轮廓吻合），
    // 其余三角直角
    float sideBL = win.config()
        ? win.config()->cornerRadii.bottomLeft * s * g_uiCtx.radiusScale() : 0.0f;
    drawCornerRadiiRect(dev, Rect(Vec2(win.x, L.sideY), Vec2(L.sideW, win.h - L.titleH)),
                        Color(c.bg3.r, c.bg3.g, c.bg3.b, 240.0f / 255.0f * a),
                        0, 0, sideBL, 0);
    drawLine(dev, win.x + L.sideW, L.sideY, win.x + L.sideW, win.y + win.h,
             1.0f * s, Color(c.accent.r, c.accent.g, c.accent.b, 80.0f / 255.0f * a));

    // ---- 侧标签（选中 accent→accent2 垂直渐变） ----
    static const char* kSideItems[] = { "主题色", "动态背景", "按钮", "其他UI" };
    for (int i = 0; i < studio::SIDE_TAB_COUNT; i++) {
        Rect ir(Vec2(win.x + 6.0f * s, L.sideStartY + i * L.sideItemH),
                Vec2(L.sideW - 12.0f * s, L.sideItemH - 4.0f * s));
        if (st.colorCustomTab == i) {
            drawRoundedGradientRect(dev, ir, Color(c.accent.r, c.accent.g, c.accent.b, a),
                                    Color(c.accent2.r, c.accent2.g, c.accent2.b, a), 6.0f * s);
            wctl::drawTextCenterAt(dev, g_uiCtx.smallFont, kSideItems[i], ir,
                                   Color(c.btnText.r, c.btnText.g, c.btnText.b, a));
        } else if (st.hoverColorCustomTab == i) {
            drawRoundedRect(dev, ir, Color(c.tabBg.r, c.tabBg.g, c.tabBg.b, 200.0f / 255.0f * a), 6.0f * s);
            wctl::drawTextCenterAt(dev, g_uiCtx.smallFont, kSideItems[i], ir,
                                   Color(c.text.r, c.text.g, c.text.b, 220.0f / 255.0f * a));
        } else {
            wctl::drawTextCenterAt(dev, g_uiCtx.smallFont, kSideItems[i], ir,
                                   Color(c.text.r, c.text.g, c.text.b, 160.0f / 255.0f * a));
        }
    }

    if (st.colorCustomTab == 0) {
        // ================= tab0 主题色 =================
        // ---- 子标签（模板/详细/自定义） ----
        static const char* kSubTabs[] = { "模板", "详细", "自定义" };
        for (int i = 0; i < 3; i++) {
            Rect sub(Vec2(L.contentX + i * (L.subTabW + 4.0f * s), L.contentTop),
                     Vec2(L.subTabW, L.subTabH));
            if (st.colorEditMode == i) {
                drawRoundedGradientRect(dev, sub, Color(c.accent.r, c.accent.g, c.accent.b, a),
                                        Color(c.accent2.r, c.accent2.g, c.accent2.b, a), 4.0f * s);
                wctl::drawTextCenterAt(dev, g_uiCtx.smallFont, kSubTabs[i], sub, Color(1, 1, 1, a));
            } else if (st.hoverColorEditMode == i) {
                drawRoundedRect(dev, sub, Color(c.tabBg.r, c.tabBg.g, c.tabBg.b, 200.0f / 255.0f * a), 4.0f * s);
                wctl::drawTextCenterAt(dev, g_uiCtx.smallFont, kSubTabs[i], sub,
                                       Color(c.text.r, c.text.g, c.text.b, 220.0f / 255.0f * a));
            } else {
                drawRoundedRect(dev, sub, Color(c.bg3.r, c.bg3.g, c.bg3.b, 150.0f / 255.0f * a), 4.0f * s);
                wctl::drawTextCenterAt(dev, g_uiCtx.smallFont, kSubTabs[i], sub,
                                       Color(c.text.r, c.text.g, c.text.b, 160.0f / 255.0f * a));
            }
        }

        if (st.colorEditMode == 0) {
            // ============ 子模式0：35 主题模板列表（RegionSystem 滚动） ============
            RegionHandle h = g_regions.begin(dev, win, {
                L.contentX - win.x, L.contentAreaTop - win.y,
                L.contentW, L.contentBottom - L.contentAreaTop, 0, "ts_template_list" });
            if (h.valid) {
                float rowStep = L.rowH + L.rowGap;
                float y = h.clipY - h.scrollY;
                for (int i = 0; i < THEME_COUNT; i++) {
                    if (y + L.rowH < h.clipY) { y += rowStep; continue; }   // 视口上外快跳
                    if (y > h.clipY + h.clipH) break;                       // 视口下外截断
                    bool isCur = (i == g_currentTheme);
                    Rect row(Vec2(L.contentX, y), Vec2(h.contentW, L.rowH));
                    // 行背景（当前主题高亮 + accent 描边）
                    float rowA = (isCur ? 220.0f : 150.0f) / 255.0f * a;
                    drawRoundedRect(dev, row, Color(c.bg3.r, c.bg3.g, c.bg3.b, rowA), 4.0f * s);
                    if (isCur)
                        drawRoundedBorder(dev, row, Color(c.accent.r, c.accent.g, c.accent.b, 200.0f / 255.0f * a),
                                          1.5f * s, 4.0f * s);
                    // 编号
                    char numBuf[8];
                    snprintf(numBuf, sizeof(numBuf), "%d", i);
                    wctl::drawTextCenterAt(dev, g_uiCtx.smallFont, numBuf,
                                           Rect(Vec2(L.contentX + 2.0f * s, y), Vec2(L.numW, L.rowH)),
                                           Color(c.text.r, c.text.g, c.text.b, 200.0f / 255.0f * a));
                    // 名称
                    wctl::drawTextLeftAt(dev, g_uiCtx.smallFont, g_themes[i].name.c_str(),
                                         Vec2(L.contentX + L.numW + 4.0f * s, y), L.rowH,
                                         Color(c.text.r, c.text.g, c.text.b, 220.0f / 255.0f * a));
                    // 4 色块预览（accent/accent2/bg1/text）
                    ThemeColors& tc = g_themes[i].colors;
                    const Color* refs[4] = { &tc.accent, &tc.accent2, &tc.bg1, &tc.text };
                    float sx = L.contentX + L.numW + L.nameW + 8.0f * s;
                    for (int sc = 0; sc < 4; sc++) {
                        Rect sw(Vec2(sx + sc * (L.swatch + L.swatchGap),
                                     y + (L.rowH - L.swatch) * 0.5f), Vec2(L.swatch, L.swatch));
                        drawRoundedRect(dev, sw, Color(refs[sc]->r, refs[sc]->g, refs[sc]->b, a), 3.0f * s);
                        drawRoundedBorder(dev, sw, Color(c.inputBorder.r, c.inputBorder.g, c.inputBorder.b,
                                                         100.0f / 255.0f * a), 1.0f * s, 3.0f * s);
                    }
                    // 更改/当前按钮
                    Rect btn(Vec2(L.contentX + h.contentW - L.changeBtnW - 6.0f * s,
                                  y + (L.rowH - L.changeBtnH) * 0.5f),
                             Vec2(L.changeBtnW, L.changeBtnH));
                    bool hovBtn = (st.hoverTemplateBtn == i);
                    Color btnBg = hovBtn ? c.btnHover1 : (isCur ? c.accent : c.btnBg1);
                    float btnRad = c.btnRadius * s * g_uiCtx.radiusScale();   // 跟随全局圆润（不写死）
                    drawRoundedRect(dev, btn, Color(btnBg.r, btnBg.g, btnBg.b, 220.0f / 255.0f * a), btnRad);
                    Color btnBc = hovBtn ? c.btnHoverBorder : c.btnBorder;
                    drawRoundedBorder(dev, btn, Color(btnBc.r, btnBc.g, btnBc.b, 180.0f / 255.0f * a),
                                      1.0f * s, btnRad);
                    wctl::drawTextCenterAt(dev, g_uiCtx.smallFont, isCur ? "当前" : "更改", btn,
                                           isCur ? Color(1, 1, 1, a)
                                                 : Color(c.btnText.r, c.btnText.g, c.btnText.b, a));
                    y += rowStep;
                }
                g_regions.end(dev, h, THEME_COUNT * rowStep);
            }
        } else if (st.colorEditMode == 1) {
            // ============ 子模式1：12 分组卡片 55 色块（RegionSystem 滚动） ============
            RegionHandle h = g_regions.begin(dev, win, {
                L.contentX - win.x, L.contentAreaTop - win.y,
                L.contentW, L.contentBottom - L.contentAreaTop, 0, "ts_detail_grid" });
            if (h.valid) {
                ThemeColors& tc = g_themes[g_currentTheme].colors;
                float rowY = L.contentAreaTop - h.scrollY;
                for (int row = 0; row < 4; row++) {
                    for (int col = 0; col < 3; col++) {
                        int gi = row * 3 + col;
                        if (gi >= studio::kColorGroupCount) break;
                        float cardX = L.contentX + col * (L.cardW + L.cardGap);
                        float cardY = rowY;
                        float cardH = L.rowHeights[row];
                        if (cardY + cardH < h.clipY) continue;
                        if (cardY > h.clipY + h.clipH) break;
                        // 卡片底 + 边框
                        Rect card(Vec2(cardX, cardY), Vec2(L.cardW, cardH));
                        drawRoundedRect(dev, card, Color(c.bg3.r, c.bg3.g, c.bg3.b, 180.0f / 255.0f * a), 4.0f * s);
                        drawRoundedBorder(dev, card, Color(c.groupBorder.r, c.groupBorder.g, c.groupBorder.b,
                                                           100.0f / 255.0f * a), 1.0f * s, 4.0f * s);
                        // 分组标题
                        wctl::drawTextLeftAt(dev, g_uiCtx.smallFont, studio::kColorGroups[gi].name,
                                             Vec2(cardX + 6.0f * s, cardY + 2.0f * s), 14.0f * s,
                                             Color(c.text.r, c.text.g, c.text.b, 200.0f / 255.0f * a));
                        // 色块网格（选中/hover 双层描边）
                        for (int si = 0; si < studio::kColorGroups[gi].count; si++) {
                            int itemIdx = studio::kColorGroups[gi].startIndex + si;
                            float wx = cardX + 6.0f * s + (si % L.swatchPerRow) * L.gridSwatchStep;
                            float wy = cardY + 16.0f * s + (si / L.swatchPerRow) * L.gridSwatchStep;
                            Rect sw(Vec2(wx, wy), Vec2(L.gridSwatch, L.gridSwatch));
                            const Color& col2 = tc.*studio::kColorItems[itemIdx].ptr;
                            drawRoundedRect(dev, sw, Color(col2.r, col2.g, col2.b, a), 3.0f * s);
                            drawRoundedBorder(dev, sw, Color(c.inputBorder.r, c.inputBorder.g, c.inputBorder.b,
                                                             120.0f / 255.0f * a), 1.0f * s, 3.0f * s);
                            if (st.selectedColorItem == itemIdx)
                                drawRoundedBorder(dev, sw, Color(c.accent.r, c.accent.g, c.accent.b, a),
                                                  2.0f * s, 3.0f * s);
                            else if (st.hoverColorItem == itemIdx)
                                drawRoundedBorder(dev, sw, Color(c.text.r, c.text.g, c.text.b, 180.0f / 255.0f * a),
                                                  1.0f * s, 3.0f * s);
                        }
                    }
                    rowY += L.rowHeights[row] + L.cardGap;
                }
                // 底部四按钮（导入/导出/重置/保存；滚动内容坐标）
                const RegionState* rs = g_regions.findRegion(win.id, "ts_detail_grid");
                Rect b0 = studio::detailBtnRect(L, rs, 0);
                studio::drawActionBtn(dev, b0, "导入", st.hoverImportBtn, false, a);
                studio::drawActionBtn(dev, studio::detailBtnRect(L, rs, 1), "导出", st.hoverExportBtn, false, a);
                studio::drawActionBtn(dev, studio::detailBtnRect(L, rs, 2), "重置", st.hoverResetBtn, false, a);
                studio::drawActionBtn(dev, studio::detailBtnRect(L, rs, 3), "保存", st.hoverSaveBtn, true, a);
                g_regions.end(dev, h, L.totalContentH);
            }
        } else {
            // ============ 子模式2：自定义背景（预览 + 4 类型编辑） ============
            // 预览条（窗口比例所见即所得）
            drawBgCustomPreview(dev, L.contentX, L.contentAreaTop, L.contentW, L.previewH, c);
            drawRoundedBorder(dev, Rect(Vec2(L.contentX, L.contentAreaTop), Vec2(L.contentW, L.previewH)),
                              Color(c.inputBorder.r, c.inputBorder.g, c.inputBorder.b, 120.0f / 255.0f * a),
                              1.0f * s, 4.0f * s);
            // 类型行
            studio::drawLabel(dev, L.contentX, L.typeRowY + 3.0f * s, L.ctrlH, "类型:", a);
            static const char* kTypeNames[] = { "纯色", "渐变", "网格", "图片" };
            for (int i = 0; i < 4; i++) {
                Rect br(Vec2(L.contentX + 40.0f * s + i * (L.typeBtnW + L.typeBtnGap), L.typeRowY),
                        Vec2(L.typeBtnW, L.ctrlH));
                studio::drawSegBtn(dev, br, kTypeNames[i], g_bgCustom.type == i,
                                   st.hoverBgType == i, a);
            }
            // 分类型编辑区
            if (g_bgCustom.type == 0) {
                // ---- 纯色 ----
                studio::drawLabel(dev, L.contentX, L.solidSwatch.pos.y + 3.0f * s, L.ctrlH, "颜色:", a);
                studio::drawSwatch(dev, L.solidSwatch, g_bgCustom.solidColor,
                                   st.hoverBgSwatch == 0, a);
                char hex[10];
                snprintf(hex, sizeof(hex), "#%02X%02X%02X",
                         (int)(g_bgCustom.solidColor.r * 255), (int)(g_bgCustom.solidColor.g * 255),
                         (int)(g_bgCustom.solidColor.b * 255));
                wctl::drawTextLeftAt(dev, g_uiCtx.smallFont, hex,
                                     Vec2(L.solidSwatch.pos.x + L.solidSwatch.size.x + 8.0f * s,
                                          L.solidSwatch.pos.y + 5.0f * s), L.ctrlH,
                                     Color(c.text.r, c.text.g, c.text.b, 200.0f / 255.0f * a));
            } else if (g_bgCustom.type == 1) {
                // ---- 渐变：四角色块 + 强度滑条 ----
                static const char* kCornerNames[] = { "左上", "右上", "左下", "右下" };
                studio::drawLabel(dev, L.contentX, L.gradRowY + 3.0f * s, L.ctrlH, "四角:", a);
                for (int i = 0; i < 4; i++) {
                    studio::drawSwatch(dev, L.gradSwatch[i], g_bgCustom.gradColors[i],
                                       st.hoverBgSwatch == 1 + i, a);
                    wctl::drawTextCenterAt(dev, g_uiCtx.smallFont, kCornerNames[i],
                                           Rect(Vec2(L.gradSwatch[i].pos.x,
                                                     L.gradSwatch[i].pos.y + L.gradSwatch[i].size.y + 2.0f * s),
                                                Vec2(L.gradSwatch[i].size.x, 12.0f * s)),
                                           Color(c.text.r, c.text.g, c.text.b, 180.0f / 255.0f * a));
                }
                studio::drawLabel(dev, L.contentX, L.gradStrengthY + 3.0f * s, L.ctrlH, "强度:", a);
                for (int i = 0; i < 4; i++) {
                    char val[8];
                    snprintf(val, sizeof(val), "%d", g_bgCustom.gradIntensity[i]);
                    studio::drawBarSlider(dev, L.gradSlider[i],
                                          g_bgCustom.gradIntensity[i] / 100.0f, val, a);
                }
            } else if (g_bgCustom.type == 2) {
                // ---- 网格：底/线色 + 间距/粗细/透明 ----
                studio::drawLabel(dev, L.contentX, L.gridRowY1 + 3.0f * s, L.ctrlH, "底色:", a);
                studio::drawSwatch(dev, L.gridBgSwatch, g_bgCustom.gridBgColor,
                                   st.hoverBgSwatch == 5, a);
                studio::drawLabel(dev, L.gridBgSwatch.pos.x + L.gridBgSwatch.size.x + 12.0f * s,
                                  L.gridRowY1 + 3.0f * s, L.ctrlH, "线色:", a);
                studio::drawSwatch(dev, L.gridLineSwatch, g_bgCustom.gridLineColor,
                                   st.hoverBgSwatch == 6, a);
                studio::drawLabel(dev, L.contentX,
                                  L.spacingSlider.pos.y - 3.0f * s + 3.0f * s, L.ctrlH, "间距:", a);
                {
                    char val[8];
                    snprintf(val, sizeof(val), "%d", g_bgCustom.gridSpacing);
                    studio::drawBarSlider(dev, L.spacingSlider,
                                          (g_bgCustom.gridSpacing - 2) / 48.0f, nullptr, a);
                    studio::drawValText(dev, L.spacingSlider, val, a);
                }
                studio::drawLabel(dev, L.contentX, L.gridRowY2 + 3.0f * s, L.ctrlH, "粗细:", a);
                {
                    char val[8];
                    snprintf(val, sizeof(val), "%dpx", g_bgCustom.gridLineWidth);
                    studio::drawBarSlider(dev, L.widthSlider,
                                          (g_bgCustom.gridLineWidth - 1) / 9.0f, nullptr, a);
                    studio::drawValText(dev, L.widthSlider, val, a);
                }
                studio::drawLabel(dev, L.contentX + L.contentW * 0.5f, L.gridRowY2 + 3.0f * s, L.ctrlH, "透明:", a);
                {
                    char val[8];
                    snprintf(val, sizeof(val), "%d%%", g_bgCustom.gridLineAlpha * 100 / 255);
                    studio::drawBarSlider(dev, L.gridAlphaSlider,
                                          g_bgCustom.gridLineAlpha / 255.0f, nullptr, a);
                    studio::drawValText(dev, L.gridAlphaSlider, val, a);
                }
            } else if (g_bgCustom.type == 3) {
                // ---- 图片：加载 + 路径 + 透明度 + 填充模式 ----
                {
                    ThemeColors& cc = currentColors();
                    Color bg = st.hoverBgLoadBtn ? Color(cc.accent.r, cc.accent.g, cc.accent.b, 220.0f / 255.0f * a)
                                                 : Color(cc.bg2.r, cc.bg2.g, cc.bg2.b, 220.0f / 255.0f * a);
                    drawRoundedRect(dev, L.loadBtn, bg, 4.0f * s);
                    drawRoundedBorder(dev, L.loadBtn, Color(cc.accent2.r, cc.accent2.g, cc.accent2.b, 180.0f / 255.0f * a),
                                      1.0f * s, 4.0f * s);
                    wctl::drawTextCenterAt(dev, g_uiCtx.smallFont, "加载图片", L.loadBtn,
                                           Color(cc.text.r, cc.text.g, cc.text.b, a));
                }
                if (g_bgCustom.imgLoaded && !g_bgCustom.imgPath.empty())
                    wctl::drawTextLeftAt(dev, g_uiCtx.smallFont, g_bgCustom.imgPath.c_str(),
                                         Vec2(L.contentX + 40.0f * s, L.imgPathY), 18.0f * s,
                                         Color(c.text.r, c.text.g, c.text.b, 180.0f / 255.0f * a));
                else
                    wctl::drawTextLeftAt(dev, g_uiCtx.smallFont, "未选择图片",
                                         Vec2(L.contentX + 40.0f * s, L.imgPathY), 18.0f * s,
                                         Color(c.text.r, c.text.g, c.text.b, 120.0f / 255.0f * a));
                studio::drawLabel(dev, L.contentX, L.imgAlphaSlider.pos.y, L.ctrlH, "透明度:", a);
                {
                    char val[8];
                    snprintf(val, sizeof(val), "%d%%", g_bgCustom.imgAlpha);
                    studio::drawBarSlider(dev, L.imgAlphaSlider,
                                          g_bgCustom.imgAlpha / 100.0f, nullptr, a);
                    studio::drawValText(dev, L.imgAlphaSlider, val, a);
                }
                studio::drawLabel(dev, L.contentX, L.fillModeBtn[0].pos.y + 3.0f * s, L.ctrlH, "填充:", a);
                static const char* kFillNames[] = { "拉伸", "平铺", "裁剪" };
                for (int i = 0; i < 3; i++)
                    studio::drawSegBtn(dev, L.fillModeBtn[i], kFillNames[i],
                                       g_bgCustom.imgFillMode == i, st.hoverBgFillMode[i], a);
            }
            // 底部应用/重置按钮（窗口内放得下才画）
            if (L.bgApplyBtn.pos.y + L.bgApplyBtn.size.y < L.contentBottom) {
                studio::drawActionBtn(dev, L.bgApplyBtn, "应用背景", st.hoverBgApplyBtn, true, a);
                studio::drawActionBtn(dev, L.bgResetBtn, "重置", st.hoverBgResetBtn, false, a);
            }
        }
    } else if (st.colorCustomTab == 1) {
        // ================= tab1 动态背景 =================
        float winRatio = (float)g_uiCtx.winW / (float)(g_uiCtx.winH > 0 ? g_uiCtx.winH : 1);
        drawDynBgPreview(dev, L.contentX, L.contentTop, L.contentW, L.dynPreviewH, winRatio);
        drawRoundedBorder(dev, Rect(Vec2(L.contentX, L.contentTop), Vec2(L.contentW, L.dynPreviewH)),
                          Color(c.inputBorder.r, c.inputBorder.g, c.inputBorder.b, 120.0f / 255.0f * a),
                          1.0f * s, 4.0f * s);
        // 28 效果按钮（3 列网格）
        studio::drawLabel(dev, L.contentX, L.dynEffectBtn[0].pos.y + 3.0f * s, L.ctrlH, "效果:", a);
        for (int i = 0; i < DYN_BG_COUNT; i++)
            studio::drawSegBtn(dev, L.dynEffectBtn[i], studio::kDynBgNames[i],
                               g_dynBg.type == i, st.hoverDynBgType == i, a);
        // 速度/密度滑条
        studio::drawLabel(dev, L.contentX, L.speedSlider.pos.y, L.ctrlH, "速度:", a);
        {
            char val[8];
            snprintf(val, sizeof(val), "%.1f", g_dynBg.speed);
            studio::drawBarSlider(dev, L.speedSlider, (g_dynBg.speed - 0.1f) / 3.9f, nullptr, a);
            studio::drawValText(dev, L.speedSlider, val, a);
        }
        studio::drawLabel(dev, L.contentX, L.densitySlider.pos.y, L.ctrlH, "密度:", a);
        {
            char val[8];
            snprintf(val, sizeof(val), "%d%%", g_dynBg.density);
            studio::drawBarSlider(dev, L.densitySlider, g_dynBg.density / 100.0f, nullptr, a);
            studio::drawValText(dev, L.densitySlider, val, a);
        }
        // 应用/重置
        if (L.dynApplyBtn.pos.y + L.dynApplyBtn.size.y < L.contentBottom) {
            studio::drawActionBtn(dev, L.dynApplyBtn, "应用背景", st.hoverDynApplyBtn, true, a);
            studio::drawActionBtn(dev, L.dynResetBtn, "重置", st.hoverDynResetBtn, false, a);
        }
    } else if (st.colorCustomTab == 2) {
        // ================= tab2 按钮：视觉增强实时调节 =================
        // 抗锯齿复选框：统一走 wctl::drawCheck（全引擎唯一绘制，勾显隐全局单源）
        {
            ThemeColors& tc = currentColors();
            wctl::drawCheck(dev, L.aaBox, g_uiCtx.sdfAA, st.hoverAACheck, a);
            float boxS = L.aaBox.size.x;
            wctl::drawTextLeftAt(dev, g_uiCtx.smallFont, "抗锯齿",
                                 Vec2(L.aaBox.pos.x + boxS + 8.0f * s, L.aaBox.pos.y - 3.0f * s),
                                 boxS + 6.0f * s,
                                 Color(tc.text.r, tc.text.g, tc.text.b, 220.0f / 255.0f * a));
        }
        // 圆润强度滑条（0-100%，写 g_uiCtx.radiusScaleUser，全引擎圆角即时生效）
        studio::drawLabel(dev, L.contentX, L.btnRowY, L.ctrlH, "圆润强度:", a);
        {
            int pct = (int)(g_uiCtx.radiusScaleUser * 50.0f + 0.5f);
            char val[8];
            snprintf(val, sizeof(val), "%d%%", pct);
            studio::drawBarSlider(dev, L.radiusSlider, g_uiCtx.radiusScaleUser * 0.5f, nullptr, a);
            studio::drawValText(dev, L.radiusSlider, val, a);
        }
        // 阴影强度滑条（0-100%，写 g_uiCtx.shadowStrength，悬浮窗阴影即时生效）
        studio::drawLabel(dev, L.contentX, L.shadowSlider.pos.y, L.ctrlH, "阴影强度:", a);
        {
            int pct = (int)(g_uiCtx.shadowStrength * 100.0f + 0.5f);
            char val[8];
            snprintf(val, sizeof(val), "%d%%", pct);
            studio::drawBarSlider(dev, L.shadowSlider, g_uiCtx.shadowStrength, nullptr, a);
            studio::drawValText(dev, L.shadowSlider, val, a);
        }
        // 厚度百分比滑条（0-200%，写 g_uiCtx.thicknessUser，全引擎边框线宽即时生效）
        studio::drawLabel(dev, L.contentX, L.thicknessSlider.pos.y, L.ctrlH, "厚度百分比:", a);
        {
            int pct = (int)(g_uiCtx.thicknessUser * 100.0f + 0.5f);
            char val[8];
            snprintf(val, sizeof(val), "%d%%", pct);
            studio::drawBarSlider(dev, L.thicknessSlider, g_uiCtx.thicknessUser * 0.5f, nullptr, a);
            studio::drawValText(dev, L.thicknessSlider, val, a);
        }
    } else if (st.colorCustomTab == 3) {
        // ================= tab3 其他UI：复选框显示勾 + 开关风格下拉配置 =================
        // 复选框"显示勾"：统一走 wctl::drawCheck（开=绘制对勾，关=仅色块，全引擎联动）
        {
            wctl::drawCheck(dev, L.tickBox, g_uiCtx.checkboxShowTick, st.hoverCheckTick, a);
            float boxS = L.tickBox.size.x;
            wctl::drawTextLeftAt(dev, g_uiCtx.smallFont, "显示勾",
                                 Vec2(L.tickBox.pos.x + boxS + 8.0f * s, L.tickBox.pos.y - 3.0f * s),
                                 boxS + 6.0f * s,
                                 Color(c.text.r, c.text.g, c.text.b, 220.0f / 255.0f * a));
        }
        // 复选框"圆形进度条渐变色"：开=进度弧 accent→accent2 渐变，关=纯 accent
        {
            wctl::drawCheck(dev, L.circBox, g_uiCtx.circGrad, st.hoverCircGrad, a);
            float boxS = L.circBox.size.x;
            wctl::drawTextLeftAt(dev, g_uiCtx.smallFont, "圆形进度条渐变色",
                                 Vec2(L.circBox.pos.x + boxS + 8.0f * s, L.circBox.pos.y - 3.0f * s),
                                 boxS + 6.0f * s,
                                 Color(c.text.r, c.text.g, c.text.b, 220.0f / 255.0f * a));
        }
        // 开关按钮风格 / 开关扩展绘制 / 滑动条绘制 / 全局分辨率 / 悬浮球绘制 / 线程模式：六行下拉（5/13/5/7/4/4 项）
        // 分辨率选项切换后置 renderResDirty，main 循环检测重启窗口生效；
        // 线程模式切换后置 threadModeDirty，main 帧末 pause 段内 configure 生效
        // 线程模式选项数按后端动态：OpenGL 强制单线程（1 项），Vulkan 全部（4 项）
        int threadOpts = appBackendIsVulkan() ? 4 : 1;
        static const char* kLabels[7] = { "开关按钮风格:", "开关扩展绘制:", "滑动条绘制:", "全局分辨率:", "悬浮球绘制:", "线程模式:", "分割线风格:" };
        static const char* kOpts[7][13] = {
            { "系统圆润", "方形", "半圆润", "机械", "渐变" },
            { "无", "中心微光", "花朵图案", "雪花图案", "齿轮纹路", "星月图案",
              "宝石变色", "蝙蝠标记", "星辉", "太阳光芒", "金属拨杆", "猫爪印", "镜面光泽" },
            { "默认", "霓虹细轨", "竖柄霓虹", "春意描边", "像素霓虹" },
            { "跟随窗口", "360p", "480p", "720p", "1080p", "2K", "4K" },
            { "默认", "方形", "玻璃", "渐变" },
            { "单线程", "双线程", "四线程", "全部线程" },
            { "默认胶囊", "细线", "双线", "点状", "纯色" },
        };
        const int kOptCount[7] = { 5, 13, 5, 7, 4, threadOpts, 5 };
        const int curVal[7] = { g_uiCtx.switchStyle, g_uiCtx.switchExtra,
                                g_uiCtx.sliderStyle, g_uiCtx.renderRes,
                                g_uiCtx.ballStyle, g_uiCtx.threadMode, g_uiCtx.splitterStyle };

        int* targets[7] = { &g_uiCtx.switchStyle, &g_uiCtx.switchExtra, &g_uiCtx.sliderStyle, &g_uiCtx.renderRes, &g_uiCtx.ballStyle, &g_uiCtx.threadMode, &g_uiCtx.splitterStyle };
        bool* dirts[7] = { nullptr, nullptr, nullptr, &g_uiCtx.renderResDirty, nullptr, &g_uiCtx.threadModeDirty, nullptr };
        static const char* ddIds[7] = { "ts_dd0", "ts_dd1", "ts_dd2", "ts_dd3", "ts_dd4", "ts_dd5", "ts_dd6" };
        for (int i = 0; i < 7; i++) {
            studio::drawLabel(dev, L.contentX, L.dropdownBtn[i].pos.y + 3.0f * s, L.ctrlH, kLabels[i], a);
            g_dropdowns.draw(dev, win, { L.dropdownBtn[i], kOpts[i], kOptCount[i], curVal[i], targets[i], dirts[i], ddIds[i], 0, g_uiCtx.smallFont, L.contentTop, L.contentBottom }, a);
        }
        // 全局页面缩放滑条（80-150%，100 附近吸附；松手才提交倍率，
        // 主界面/悬浮窗/字号/悬浮球等比跟随，resize 一次生效）
        {
            const Rect& r = L.uiScaleSlider;
            studio::drawLabel(dev, L.contentX, r.pos.y, L.ctrlH, "UI缩放:", a);
            // 拖动中显示暂存值，静止显示已提交值
            float cur = (st.drag == studio::DRAG_UI_SCALE) ? st.pendingUiScale
                                                            : g_uiCtx.uiScaleUser;
            char val[16];
            snprintf(val, sizeof(val), "%d%%", (int)(cur * 100.0f + 0.5f));
            // 全局标准滑条（与「UI模板」页/主界面一致），不再自绘单独条
            pctl::drawSliderFull(dev, r, (cur - 0.8f) / 0.7f, false, 1.0f);
            wctl::drawTextCenterAt(dev, g_uiCtx.smallFont, val, r,
                                   Color(c.text.r, c.text.g, c.text.b, a));
        }
        // 下拉展开浮层（系统派发：DropdownSystem 统一画，覆盖下方内容；最后绘制）
        g_dropdowns.renderPopups(dev, win, a);

    }
}

// ============================================================
// 滑条取值（click 按下与 drag 拖动共用）
// ============================================================
static void applySliderDrag(studio::State& st, const studio::Layout& L, const Vec2& m) {
    using namespace studio;
    if (st.drag >= DRAG_GRAD_R0 && st.drag <= DRAG_GRAD_R3) {
        int i = st.drag - DRAG_GRAD_R0;
        g_bgCustom.gradIntensity[i] = (int)(wctl::sliderValueAt(L.gradSlider[i], m.x) * 100.0f + 0.5f);
    } else if (st.drag == DRAG_GRID_SPACING) {
        g_bgCustom.gridSpacing = 2 + (int)(wctl::sliderValueAt(L.spacingSlider, m.x) * 48.0f + 0.5f);
    } else if (st.drag == DRAG_GRID_WIDTH) {
        g_bgCustom.gridLineWidth = 1 + (int)(wctl::sliderValueAt(L.widthSlider, m.x) * 9.0f + 0.5f);
    } else if (st.drag == DRAG_GRID_ALPHA) {
        g_bgCustom.gridLineAlpha = (int)(wctl::sliderValueAt(L.gridAlphaSlider, m.x) * 255.0f + 0.5f);
    } else if (st.drag == DRAG_IMG_ALPHA) {
        g_bgCustom.imgAlpha = (int)(wctl::sliderValueAt(L.imgAlphaSlider, m.x) * 100.0f + 0.5f);
    } else if (st.drag == DRAG_DYN_SPEED) {
        g_dynBg.speed = 0.1f + wctl::sliderValueAt(L.speedSlider, m.x) * 3.9f;
    } else if (st.drag == DRAG_DYN_DENSITY) {
        g_dynBg.density = (int)(wctl::sliderValueAt(L.densitySlider, m.x) * 100.0f + 0.5f);
    } else if (st.drag == DRAG_RADIUS) {
        g_uiCtx.radiusScaleUser = wctl::sliderValueAt(L.radiusSlider, m.x) * 2.0f;
    } else if (st.drag == DRAG_SHADOW) {
        g_uiCtx.shadowStrength = wctl::sliderValueAt(L.shadowSlider, m.x);
    } else if (st.drag == DRAG_THICKNESS) {
        g_uiCtx.thicknessUser = wctl::sliderValueAt(L.thicknessSlider, m.x) * 2.0f;
    } else if (st.drag == DRAG_UI_SCALE) {
        // 全局页面缩放：80-150%；100 附近 ±4 吸附；拖动只写暂存值，松手才提交
        float v = 80.0f + wctl::sliderValueAt(L.uiScaleSlider, m.x) * 70.0f;
        if (v > 96.0f && v < 104.0f) v = 100.0f;
        st.pendingUiScale = v / 100.0f;
    }
}

// ============================================================
// 点击
// ============================================================
bool clickThemeStudio(WindowInstance& win, const Vec2& m) {
    using namespace studio;
    State& st = win.getState<State>();
    Layout L = layout(win);
    float s = L.scale;

    // ---- 侧标签 ----
    for (int i = 0; i < SIDE_TAB_COUNT; i++) {
        Rect ir(Vec2(win.x + 6.0f * s, L.sideStartY + i * L.sideItemH),
                Vec2(L.sideW - 12.0f * s, L.sideItemH - 4.0f * s));
        if (ir.contains(m)) {
            st.colorCustomTab = i;

            return true;
        }
    }

    if (st.colorCustomTab == 0) {
        // ---- 子标签 ----
        for (int i = 0; i < 3; i++) {
            Rect sub(Vec2(L.contentX + i * (L.subTabW + 4.0f * s), L.contentTop),
                     Vec2(L.subTabW, L.subTabH));
            if (sub.contains(m)) { st.colorEditMode = i; return true; }
        }

        if (st.colorEditMode == 0) {
            // ---- 子模式0 模板列表：更改按钮（滚动条命中由系统先处理） ----
            const RegionState* rs = g_regions.findRegion(win.id, "ts_template_list");
            if (!rs || !rs->view.contains(m)) return false;
            for (int i = 0; i < THEME_COUNT; i++) {
                float rowY = L.contentAreaTop + i * (L.rowH + L.rowGap) - rs->scrollY;
                if (rowY + L.rowH < L.contentAreaTop) continue;
                if (rowY > L.contentBottom) break;
                Rect btn = templateChangeBtn(L, rs, i);
                if (btn.contains(m)) {
                    if (i != g_currentTheme) g_currentTheme = i;
                    return true;
                }
            }
        } else if (st.colorEditMode == 1) {
            // ---- 子模式1 详细：色块 → 颜色选择器；底部四按钮 ----
            const RegionState* rs = g_regions.findRegion(win.id, "ts_detail_grid");
            if (!rs || !rs->view.contains(m)) return false;
            int hit = hitDetailSwatch(L, rs, m);
            if (hit >= 0) {
                st.selectedColorItem = hit;
                openPickerForThemeItem(win, hit);
                return true;
            }
            for (int idx = 0; idx < 4; idx++) {
                if (!detailBtnRect(L, rs, idx).contains(m)) continue;
                if (idx == 0) {         // 导入（JSON：完整主题，名称/颜色/文本整体应用到当前主题）
#ifdef _WIN32
                    std::string path;
                    if (pickFileOpen(path, "导入主题(JSON)", "主题文件(JSON)", "*.json"))
                        ImportThemeFromFile(g_currentTheme, path);
#else
                    ImportThemeFromFile(g_currentTheme, "theme_import.json");
#endif
                } else if (idx == 1) { // 导出（JSON：可读可分享，默认文件名 = 主题名）
#ifdef _WIN32
                    std::string path;
                    std::string defName = g_themes[g_currentTheme].name + ".json";
                    if (pickFileSave(path, "导出主题(JSON)", "主题文件(JSON)", "*.json",
                                     "json", defName.c_str()))
                        ExportThemeToFile(g_currentTheme, path);
#else
                    ExportThemeToFile(g_currentTheme, "theme_export.json");
#endif
                } else if (idx == 2) { // 重置当前主题为默认
                    g_themes[g_currentTheme].colors = g_defaultThemeColors[g_currentTheme];
                } else {                // 保存当前修改为默认
                    g_defaultThemeColors[g_currentTheme] = g_themes[g_currentTheme].colors;
                }
                return true;
            }
        } else {
            // ---- 子模式2 自定义背景 ----
            // 类型按钮
            for (int i = 0; i < 4; i++) {
                Rect br(Vec2(L.contentX + 40.0f * s + i * (L.typeBtnW + L.typeBtnGap), L.typeRowY),
                        Vec2(L.typeBtnW, L.ctrlH));
                if (br.contains(m)) { g_bgCustom.type = i; return true; }
            }
            if (g_bgCustom.type == 0) {
                if (L.solidSwatch.contains(m)) {
                    openPickerForBgSlot(win, 0);
                    return true;
                }
            } else if (g_bgCustom.type == 1) {
                for (int i = 0; i < 4; i++) {
                    if (L.gradSwatch[i].contains(m)) {
                        openPickerForBgSlot(win, 1 + i);
                        return true;
                    }
                }
                for (int i = 0; i < 4; i++) {
                    if (L.gradSlider[i].contains(m)) {
                        st.drag = (DragTarget)(DRAG_GRAD_R0 + i);
                        applySliderDrag(st, L, m);
                        return true;
                    }
                }
            } else if (g_bgCustom.type == 2) {
                if (L.gridBgSwatch.contains(m)) {
                    openPickerForBgSlot(win, 5);
                    return true;
                }
                if (L.gridLineSwatch.contains(m)) {
                    openPickerForBgSlot(win, 6);
                    return true;
                }
                if (L.spacingSlider.contains(m)) {
                    st.drag = DRAG_GRID_SPACING;
                    applySliderDrag(st, L, m);
                    return true;
                }
                if (L.widthSlider.contains(m)) {
                    st.drag = DRAG_GRID_WIDTH;
                    applySliderDrag(st, L, m);
                    return true;
                }
                if (L.gridAlphaSlider.contains(m)) {
                    st.drag = DRAG_GRID_ALPHA;
                    applySliderDrag(st, L, m);
                    return true;
                }
            } else if (g_bgCustom.type == 3) {
                if (L.loadBtn.contains(m)) {
#ifdef _WIN32
                    std::string path;
                    if (pickFileOpen(path, "选择背景图片", "图片文件",
                                     "*.png;*.jpg;*.jpeg;*.bmp;*.gif")) {
                        g_bgCustom.imgPath = path;
                        g_bgCustom.imgLoaded = true;
                        releaseBgImageTexture();   // 清旧纹理（懒加载自动重载）
                    }
#else
                    g_bgCustom.imgPath = "background.png";
                    g_bgCustom.imgLoaded = true;
                    releaseBgImageTexture();
#endif
                    return true;
                }
                if (L.imgAlphaSlider.contains(m)) {
                    st.drag = DRAG_IMG_ALPHA;
                    applySliderDrag(st, L, m);
                    return true;
                }
                for (int i = 0; i < 3; i++) {
                    if (L.fillModeBtn[i].contains(m)) {
                        g_bgCustom.imgFillMode = i;
                        return true;
                    }
                }
            }
            // 底部应用/重置
            if (L.bgApplyBtn.contains(m)) {
                switch (g_bgCustom.type) {
                    case 0: g_themes[g_currentTheme].bgType = 100;
                            g_themes[g_currentTheme].colors.bg1 = g_bgCustom.solidColor; break;
                    case 1: g_themes[g_currentTheme].bgType = 101; break;
                    case 2: g_themes[g_currentTheme].bgType = 102;
                            g_themes[g_currentTheme].colors.bg1 = g_bgCustom.gridBgColor; break;
                    case 3: g_themes[g_currentTheme].bgType = 103; break;
                }
                return true;
            }
            if (L.bgResetBtn.contains(m)) {
                g_bgCustom.type = 0;
                g_bgCustom.solidColor = Color(30.0f / 255.0f, 30.0f / 255.0f, 30.0f / 255.0f, 1.0f);
                g_bgCustom.gradColors[0] = Color(30.0f / 255.0f, 30.0f / 255.0f, 30.0f / 255.0f, 1.0f);
                g_bgCustom.gradColors[1] = Color(60.0f / 255.0f, 60.0f / 255.0f, 60.0f / 255.0f, 1.0f);
                g_bgCustom.gradColors[2] = Color(30.0f / 255.0f, 30.0f / 255.0f, 30.0f / 255.0f, 1.0f);
                g_bgCustom.gradColors[3] = Color(60.0f / 255.0f, 60.0f / 255.0f, 60.0f / 255.0f, 1.0f);
                for (int i = 0; i < 4; i++) g_bgCustom.gradIntensity[i] = 50;
                g_bgCustom.gridBgColor = Color(30.0f / 255.0f, 30.0f / 255.0f, 30.0f / 255.0f, 1.0f);
                g_bgCustom.gridLineColor = Color(100.0f / 255.0f, 100.0f / 255.0f, 100.0f / 255.0f, 1.0f);
                g_bgCustom.gridSpacing = 20;
                g_bgCustom.gridLineWidth = 1;
                g_bgCustom.gridLineAlpha = 60;
                g_bgCustom.imgFillMode = 0;
                g_bgCustom.imgAlpha = 100;
                g_bgCustom.imgPath.clear();
                g_bgCustom.imgLoaded = false;
                releaseBgImageTexture();
                return true;
            }
        }
    } else if (st.colorCustomTab == 1) {
        // ---- tab1 动态背景 ----
        for (int i = 0; i < DYN_BG_COUNT; i++) {
            if (L.dynEffectBtn[i].contains(m)) {
                g_dynBg.type = i;
                g_dynBg.enabled = (i != DYN_BG_NONE);
                return true;
            }
        }
        if (L.speedSlider.contains(m)) {
            st.drag = DRAG_DYN_SPEED;
            applySliderDrag(st, L, m);
            return true;
        }
        if (L.densitySlider.contains(m)) {
            st.drag = DRAG_DYN_DENSITY;
            applySliderDrag(st, L, m);
            return true;
        }
        if (L.dynApplyBtn.contains(m)) {
            if (g_dynBg.type != DYN_BG_NONE) {
                g_dynBg.enabled = true;
                g_themes[g_currentTheme].bgType = 200;
            } else {
                g_dynBg.enabled = false;
                g_themes[g_currentTheme].bgType = 0;
            }
            return true;
        }
        if (L.dynResetBtn.contains(m)) {
            resetDynamicBg();
            g_themes[g_currentTheme].bgType = 0;
            return true;
        }
    } else if (st.colorCustomTab == 2) {
        // ---- tab2 按钮：抗锯齿复选框 + 强度滑条 ----
        if (L.aaCheck.contains(m)) {
            g_uiCtx.sdfAA = !g_uiCtx.sdfAA;
            return true;
        }
        if (L.radiusSlider.contains(m)) {
            st.drag = DRAG_RADIUS;
            applySliderDrag(st, L, m);
            return true;
        }
        if (L.shadowSlider.contains(m)) {
            st.drag = DRAG_SHADOW;
            applySliderDrag(st, L, m);
            return true;
        }
        if (L.thicknessSlider.contains(m)) {
            st.drag = DRAG_THICKNESS;
            applySliderDrag(st, L, m);
            return true;
        }
    } else if (st.colorCustomTab == 3) {
        // ---- tab3 其他UI：下拉系统派发 + 复选框翻转 + 缩放滑条 ----
        if (g_dropdowns.handlePress(win, m).consumed) return true;
        if (L.tickCheck.contains(m)) {
            g_uiCtx.checkboxShowTick = !g_uiCtx.checkboxShowTick;
            return true;
        }
        if (L.circCheck.contains(m)) {
            g_uiCtx.circGrad = !g_uiCtx.circGrad;
            return true;
        }

        // 全局页面缩放滑条：按下即拖拽取暂存值（80-150%，100 吸附，松手提交）
        if (L.uiScaleSlider.contains(m)) {
            st.drag = DRAG_UI_SCALE;
            st.pendingUiScale = g_uiCtx.uiScaleUser;
            applySliderDrag(st, L, m);
            return true;
        }
    }
    return false;
}

// ============================================================
// hover 态
// ============================================================
void hoverThemeStudio(WindowInstance& win, const Vec2& m) {
    using namespace studio;
    State& st = win.getState<State>();
    Layout L = layout(win);
    float s = L.scale;

    // 侧标签
    st.hoverColorCustomTab = -1;
    for (int i = 0; i < SIDE_TAB_COUNT; i++) {
        Rect ir(Vec2(win.x + 6.0f * s, L.sideStartY + i * L.sideItemH),
                Vec2(L.sideW - 12.0f * s, L.sideItemH - 4.0f * s));
        if (ir.contains(m)) { st.hoverColorCustomTab = i; break; }
    }

    if (st.colorCustomTab == 0) {
        // 子标签
        st.hoverColorEditMode = -1;
        for (int i = 0; i < 3; i++) {
            Rect sub(Vec2(L.contentX + i * (L.subTabW + 4.0f * s), L.contentTop),
                     Vec2(L.subTabW, L.subTabH));
            if (sub.contains(m)) { st.hoverColorEditMode = i; break; }
        }

        if (st.colorEditMode == 0) {
            // 模板行更改按钮
            st.hoverTemplateBtn = -1;
            const RegionState* rs = g_regions.findRegion(win.id, "ts_template_list");
            if (rs && rs->view.contains(m)) {
                for (int i = 0; i < THEME_COUNT; i++) {
                    float rowY = L.contentAreaTop + i * (L.rowH + L.rowGap) - rs->scrollY;
                    if (rowY + L.rowH < L.contentAreaTop) continue;
                    if (rowY > L.contentBottom) break;
                    if (templateChangeBtn(L, rs, i).contains(m)) {
                        st.hoverTemplateBtn = i;
                        break;
                    }
                }
            }
        } else if (st.colorEditMode == 1) {
            // 色块 + 四按钮
            const RegionState* rs = g_regions.findRegion(win.id, "ts_detail_grid");
            st.hoverColorItem = hitDetailSwatch(L, rs, m);
            st.hoverImportBtn = rs && detailBtnRect(L, rs, 0).contains(m);
            st.hoverExportBtn = rs && detailBtnRect(L, rs, 1).contains(m);
            st.hoverResetBtn = rs && detailBtnRect(L, rs, 2).contains(m);
            st.hoverSaveBtn = rs && detailBtnRect(L, rs, 3).contains(m);
        } else {
            // 自定义背景全部 hover
            st.hoverBgType = -1;
            for (int i = 0; i < 4; i++) {
                Rect br(Vec2(L.contentX + 40.0f * s + i * (L.typeBtnW + L.typeBtnGap), L.typeRowY),
                        Vec2(L.typeBtnW, L.ctrlH));
                if (br.contains(m)) { st.hoverBgType = i; break; }
            }
            st.hoverBgSwatch = -1;
            if (g_bgCustom.type == 0) {
                if (L.solidSwatch.contains(m)) st.hoverBgSwatch = 0;
            } else if (g_bgCustom.type == 1) {
                for (int i = 0; i < 4; i++)
                    if (L.gradSwatch[i].contains(m)) { st.hoverBgSwatch = 1 + i; break; }
            } else if (g_bgCustom.type == 2) {
                if (L.gridBgSwatch.contains(m)) st.hoverBgSwatch = 5;
                if (L.gridLineSwatch.contains(m)) st.hoverBgSwatch = 6;
            }
            st.hoverBgLoadBtn = (g_bgCustom.type == 3) && L.loadBtn.contains(m);
            for (int i = 0; i < 3; i++)
                st.hoverBgFillMode[i] = (g_bgCustom.type == 3) && L.fillModeBtn[i].contains(m);
            st.hoverBgApplyBtn = L.bgApplyBtn.contains(m);
            st.hoverBgResetBtn = L.bgResetBtn.contains(m);
        }
    } else if (st.colorCustomTab == 1) {
        st.hoverDynBgType = -1;
        for (int i = 0; i < DYN_BG_COUNT; i++)
            if (L.dynEffectBtn[i].contains(m)) { st.hoverDynBgType = i; break; }
        st.hoverDynApplyBtn = L.dynApplyBtn.contains(m);
        st.hoverDynResetBtn = L.dynResetBtn.contains(m);
    } else if (st.colorCustomTab == 2) {
        st.hoverAACheck = L.aaCheck.contains(m);
        st.hoverRadiusSlider = L.radiusSlider.contains(m);
        st.hoverShadowSlider = L.shadowSlider.contains(m);
        st.hoverThicknessSlider = L.thicknessSlider.contains(m);
    } else if (st.colorCustomTab == 3) {
        st.hoverCheckTick = L.tickCheck.contains(m);
        st.hoverCircGrad = L.circCheck.contains(m);
        g_dropdowns.updateHover(win, m);
    }
}

// ============================================================
// 滑条拖拽（click 消费后由 WindowManager 转发）
// ============================================================
void dragThemeStudio(WindowInstance& win, const Vec2& m) {
    studio::State& st = win.getState<studio::State>();
    if (st.drag == studio::DRAG_NONE) return;
    studio::Layout L = studio::layout(win);
    applySliderDrag(st, L, m);
}

void releaseThemeStudio(WindowInstance& win, const Vec2&) {
    studio::State& st = *win.findState<studio::State>();
    if (st.drag == studio::DRAG_UI_SCALE) {
        // UI 缩放松手提交（拖动中只更新暂存值，resize 一次到位）
        g_uiCtx.uiScaleUser = st.pendingUiScale;
    }
    st.drag = studio::DRAG_NONE;
}

// ============================================================
// 滚轮（下拉列表滚动：列表被压缩显示不下时滚动查看全部选项）
// ============================================================
bool wheelThemeStudio(WindowInstance& win, const Vec2& m, float delta) {
    return g_dropdowns.handleWheel(win, m, delta);
}

} // namespace ui
