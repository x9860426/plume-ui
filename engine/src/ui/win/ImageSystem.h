#pragma once
// ============================================================
// 图像系统（全引擎唯一图片绘制入口）
//
// 存在根因：读 PNG → 建纹理 → 画贴图四角这套流程原先在扩展代码里
// 各写一遍（二维码一处、图标一处必有第二处），且「原图 256px 直接
// 缩到 24px」会产生锯齿/白边。收编为唯一实现：
//
//   1) 解码   ：readFileUtf8 取字节 → stbi 内存解码（中文路径可用）
//   2) 抗锯齿 ：面积平均（box）重采样到目标像素，**预乘 alpha** 计算，
//               缩放前先滤波 = 消除缩小时的摩尔纹/锯齿与白边
//   3) 纹理   ：createTexture 上传并缓存（同设备同图同尺寸只解码一次）
//   4) 绘制   ：贴图四角，vColor 乘算（主题着色 + 淡入淡出）
//
// 【铁律】缓存键必须含设备维度，设备重建必须 releaseAll：
//   纹理句柄属于 RenderDevice 上下文。缓存只按「路径+尺寸」时，切换后端、
//   改渲染分辨率重启、多应用各持设备 → 旧 tex id 在新设备上无效，画出来
//   正是灰色方块/乱码。本实现内部已按设备指针分键；宿主在设备创建完成后
//   调用 releaseAll() 兜底（AppWindow::initDevice 已调用）。
//
// 使用规则：任何页面/窗口需要显示图片一律走 g_images，禁止再自写
// stbi + createTexture + drawTriangles 三件套（规范：收编唯一实现）。
// ============================================================
#include "../../core/Math.h"
#include "../../render/RenderDevice.h"
#include <string>
#include <unordered_map>

namespace ui {

// 图像句柄：纹理 + 烘焙尺寸（页面只持句柄，不持纹理 id）
struct ImageTex {
    uint32_t tex = 0;
    int w = 0;
    int h = 0;
    bool ok() const { return tex != 0 && w > 0 && h > 0; }
};

class ImageSystem {
public:
    // relPath 相对资源根（如 "assets/images/icons/notice.png"）
    // maxPx   烘焙边长上限（像素）：建议 = 显示像素 × 2；0 = 保持原尺寸。
    //         缩小时按面积平均抗锯齿，放大不模糊（不超过原图）
    const ImageTex& load(RenderDevice* dev, const char* relPath, int maxPx = 96);

    // 画到 dst（拉伸铺满）。tint 乘算：白色图标 → tint 即图标颜色（主题单源）
    void draw(RenderDevice* dev, const ImageTex& img, const Rect& dst,
              float alpha = 1.0f, const Color& tint = Color(1, 1, 1, 1));

    // 设备重建 / 内容切换整体释放纹理（缓存随之清空，下次访问重新解码）
    void releaseAll();

private:
    std::unordered_map<std::string, ImageTex> cache_;
};

extern ImageSystem g_images;

} // namespace ui
