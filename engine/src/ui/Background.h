#pragma once
// ============================================================
// 背景系统（静态主题背景 + 自定义背景 + 图片背景）
//
// bgType 语义（ThemeData.bgType，配置驱动）：
//   0-34   内置主题背景模板（drawBgSimple…drawBgMagic）
//   100    自定义纯色（g_bgCustom.solidColor）
//   101    自定义四角渐变（g_bgCustom.gradColors/gradIntensity）
//   102    自定义网格（g_bgCustom.grid*）
//   103    自定义图片（g_bgCustom.imgPath + 三种填充模式）
//   200    动态背景（由 g_dynBg 系统渲染，背景仅画底色）
//
// g_bgCustom 是背景系统自身的全局配置（UI 主题工作室只是编辑器），
// 主界面渲染直接读取——扩展代码零基础设施（规范：系统派发）。
// ============================================================
#include "../render/RenderDevice.h"
#include "Theme.h"
#include <string>
#include <cstdint>

namespace ui {

void drawBackground(RenderDevice* dev, int themeIndex, int w, int h);
void drawBgTemplatePreview(RenderDevice* dev, int templateIdx, float x, float y, float w, float h, ThemeColors& c);

// ====== 自定义背景配置（主题工作室「自定义」子模式编辑） ======
struct BgCustomConfig {
    int type = 0;                       // 0=纯色 1=渐变 2=网格 3=图片
    Color solidColor{0.12f, 0.12f, 0.12f, 1.0f};

    // 渐变（四角颜色 + 各角强度 0-100）
    Color gradColors[4] = {
        {0.12f, 0.12f, 0.12f, 1.0f}, {0.24f, 0.24f, 0.24f, 1.0f},
        {0.12f, 0.12f, 0.12f, 1.0f}, {0.24f, 0.24f, 0.24f, 1.0f}};
    int gradIntensity[4] = {50, 50, 50, 50};

    // 网格
    Color gridBgColor{0.12f, 0.12f, 0.12f, 1.0f};
    Color gridLineColor{0.4f, 0.4f, 0.4f, 1.0f};
    int gridSpacing = 20;               // 2-50
    int gridLineWidth = 1;              // 1-10
    int gridLineAlpha = 60;             // 0-255

    // 图片
    int imgFillMode = 0;                // 0=拉伸 1=平铺 2=裁剪
    int imgAlpha = 100;                 // 0-100
    std::string imgPath;                // UTF-8 路径
    bool imgLoaded = false;

    // 运行时纹理（系统懒加载，UI 不参与）
    uint32_t imgTex = 0;
    int imgW = 0, imgH = 0;
};

extern BgCustomConfig g_bgCustom;

// 自定义背景预览（主题工作室 70px 高预览条）
void drawBgCustomPreview(RenderDevice* dev, float x, float y, float w, float h, ThemeColors& c);

// 背景槽位 → 颜色指针（颜色选择器写回用；槽位定义与主题工作室一致）
//   0=纯色 1-4=渐变四角 5=网格底色 6=网格线色
Color* BgSlotColor(int slot);

// 图片纹理生命周期（窗口关闭/换图时释放）
void releaseBgImageTexture();

} // namespace ui