// ============================================================
// 图像系统实现（解码 → 抗锯齿重采样 → 纹理 → 贴图）
//
// 抗锯齿的关键在「先滤波再缩小」：把源图每个像素按覆盖面积摊到目标
// 像素上（area average），全程在预乘 alpha 空间累加，最后除权重还原。
// 这样 256px 图标缩到 32px 时边缘是连续灰度过渡（等价于 8×8 超采样），
// 而不是「最近邻抖动」或「双线性漏采样」造成的锯齿/白边。
//
// 纹理与设备的绑定：缓存键含设备指针（见 load），设备重建后旧键自然
// 不再命中；宿主另在 initDevice 时 releaseAll() 清空，双保险。
// ============================================================
#include "ImageSystem.h"
#include "../../core/Assets.h"
#include "../../core/Log.h"
#include "stb_image.h"          // 实现宏在 Background.cpp（全工程唯一编译单元）
#include <cmath>
#include <cstdio>
#include <vector>

namespace ui {

ImageSystem g_images;

namespace {

// 面积平均重采样（预乘 alpha；srcRGBA / dstRGBA 均为 8bit 非预乘）
void resampleArea(const unsigned char* src, int sw, int sh,
                  unsigned char* dst, int dw, int dh) {
    if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return;
    // 累加器（预乘 alpha 的 r/g/b 与总权重）
    std::vector<float> acc((size_t)dw * dh * 4, 0.0f);
    std::vector<float> wsum((size_t)dw * dh, 0.0f);

    const float fx = (float)dw / (float)sw;   // 源 1 像素 = 目标 fx 像素宽
    const float fy = (float)dh / (float)sh;
    for (int sy = 0; sy < sh; sy++) {
        // 源像素在目标坐标里的纵向覆盖区间
        float y0 = sy * fy, y1 = y0 + fy;
        int dy0 = (int)std::floor(y0), dy1 = (int)std::ceil(y1);
        if (dy1 > dh) dy1 = dh;
        for (int dy = dy0; dy < dy1; dy++) {
            float wy = std::fmin(y1, (float)dy + 1.0f) - std::fmax(y0, (float)dy);
            if (wy <= 0.0f) continue;
            for (int sx = 0; sx < sw; sx++) {
                const unsigned char* p = src + ((size_t)sy * sw + sx) * 4;
                float a = p[3] / 255.0f;
                if (p[3] == 0 && p[0] == 0 && p[1] == 0 && p[2] == 0) continue;
                float r = p[0] * a, g = p[1] * a, b = p[2] * a;
                float x0 = sx * fx, x1 = x0 + fx;
                int dx0 = (int)std::floor(x0), dx1 = (int)std::ceil(x1);
                if (dx1 > dw) dx1 = dw;
                for (int dx = dx0; dx < dx1; dx++) {
                    float wx = std::fmin(x1, (float)dx + 1.0f) - std::fmax(x0, (float)dx);
                    if (wx <= 0.0f) continue;
                    float w = wx * wy;
                    size_t di = (size_t)dy * dw + dx;
                    acc[di * 4 + 0] += r * w;
                    acc[di * 4 + 1] += g * w;
                    acc[di * 4 + 2] += b * w;
                    acc[di * 4 + 3] += p[3] * w;
                    wsum[di] += w;
                }
            }
        }
    }
    for (size_t i = 0; i < (size_t)dw * dh; i++) {
        float w = wsum[i];
        unsigned char* o = dst + i * 4;
        if (w <= 0.0f) { o[0] = o[1] = o[2] = o[3] = 0; continue; }
        float a = acc[i * 4 + 3] / w;              // 平均 alpha（0..255）
        float inv = (a > 0.0f) ? (255.0f / a) : 0.0f;
        // 预乘还原：颜色 = 加权平均(颜色×alpha) / 平均(alpha)
        o[3] = (unsigned char)std::lround(std::fmin(255.0f, std::fmax(0.0f, a)));
        o[0] = (unsigned char)std::lround(std::fmin(255.0f, std::fmax(0.0f, acc[i * 4 + 0] / w * inv)));
        o[1] = (unsigned char)std::lround(std::fmin(255.0f, std::fmax(0.0f, acc[i * 4 + 1] / w * inv)));
        o[2] = (unsigned char)std::lround(std::fmin(255.0f, std::fmax(0.0f, acc[i * 4 + 2] / w * inv)));
    }
}

} // namespace

const ImageTex& ImageSystem::load(RenderDevice* dev, const char* relPath, int maxPx) {
    static const ImageTex kEmpty;
    if (!dev || !relPath || !relPath[0]) return kEmpty;

    // 烘焙尺寸量化到 16 的倍数：窗口连续缩放不会一格一格重新解码
    int px = maxPx > 0 ? ((maxPx + 15) / 16) * 16 : 0;
    // 缓存键必须带设备维度：纹理句柄属于设备上下文，多应用各自持一份
    // RenderDevice 时，旧设备的 tex id 在新设备上无效 → 画出来是乱码/方块。
    // 设备重建（切后端/分辨率重启）同理 → 旧键随设备指针变化自动失效。
    char devKey[24];
    std::snprintf(devKey, sizeof(devKey), "%p", (void*)dev);
    std::string key = std::string(devKey) + "|" + relPath + "#" + std::to_string(px);
    auto it = cache_.find(key);
    if (it != cache_.end()) return it->second;

    ImageTex out;
    cache_[key] = out;   // 先占位：解码失败也不会每帧重试

    std::string path = assetPath(relPath);
    std::string bytes;
    if (!readFileUtf8(path.c_str(), bytes) || bytes.empty()) {
        UI_LOG_WARN("image: cannot read %s", path.c_str());
        return cache_[key];
    }

    int sw = 0, sh = 0, ch = 0;
    unsigned char* src = stbi_load_from_memory((const unsigned char*)bytes.data(),
                                               (int)bytes.size(), &sw, &sh, &ch, 4);
    if (!src) {
        UI_LOG_WARN("image: decode failed %s", path.c_str());
        return cache_[key];
    }

    // 目标尺寸：只缩不放（放大交给 GPU 双线性，不会失真）
    int tw = sw, th = sh;
    if (px > 0 && (sw > px || sh > px)) {
        if (sw >= sh) { tw = px; th = (int)std::lround((double)sh * px / sw); }
        else          { th = px; tw = (int)std::lround((double)sw * px / sh); }
        if (tw < 1) tw = 1;
        if (th < 1) th = 1;
    }

    std::vector<unsigned char> scaled;
    const unsigned char* rgba = src;
    if (tw != sw || th != sh) {
        scaled.resize((size_t)tw * th * 4);
        resampleArea(src, sw, sh, scaled.data(), tw, th);
        rgba = scaled.data();
    }

    out.w = tw;
    out.h = th;
    out.tex = dev->createTexture(tw, th, rgba);
    stbi_image_free(src);
    cache_[key] = out;
    return cache_[key];
}

void ImageSystem::draw(RenderDevice* dev, const ImageTex& img, const Rect& dst,
                       float alpha, const Color& tint) {
    if (!dev || !img.ok() || dst.size.x <= 0.0f || dst.size.y <= 0.0f) return;
    const Color c(tint.r, tint.g, tint.b, tint.a * alpha);
    Vertex v[4] = {
        { Vec2(dst.pos.x,              dst.pos.y),              Vec2(0, 0), c },
        { Vec2(dst.pos.x + dst.size.x, dst.pos.y),              Vec2(1, 0), c },
        { Vec2(dst.pos.x + dst.size.x, dst.pos.y + dst.size.y), Vec2(1, 1), c },
        { Vec2(dst.pos.x,              dst.pos.y + dst.size.y), Vec2(0, 1), c },
    };
    const uint32_t idx[6] = { 0, 1, 2, 0, 2, 3 };
    dev->drawTriangles(v, 4, idx, 6, img.tex, true);
}

void ImageSystem::releaseAll() {
    cache_.clear();   // 纹理随设备重建失效：只清缓存，不在此处销毁句柄
}

} // namespace ui
