#pragma once
#include "../render/RenderDevice.h"
#include <stb_truetype.h>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ui {

struct GlyphQuad {
    float x0, y0, x1, y1;   // vertex offsets relative to pen (y down, screen space)
    float u0, v0, u1, v1;   // atlas UV
    float advance;          // horizontal advance after this glyph
};

// Loads an external TTF/OTT file and bakes a glyph atlas. Supports dynamic
// on-demand baking: call ensureGlyphs() with any UTF-8 text and missing
// codepoints are added to the atlas automatically (re-packed + re-uploaded).
class Font {
public:
    Font();
    ~Font();
    // pc_ 含内部自引用指针（nodes→堆、pixels→mono_.data()），拷贝/移动
    // 必然悬垂——Font 一律经 unique_ptr 持有（AppWindow 现行用法）
    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;

    bool loadFromFile(const char* path, float pixelHeight);


    // Initial bake: ASCII 32..126 only. Call once after load.
    bool init(RenderDevice* dev);

    // Re-bake the atlas at a new pixel height (same baked codepoint set —
    // glyphs added via ensureGlyphs are kept). ttf data stays in memory so
    // this is a pure repack+reupload. Use when the window/logic size changes
    // so glyphs are re-rasterized at the new density instead of being
    // vertex-stretched (blurry) by drawText scale.
    bool rebake(RenderDevice* dev, float pixelHeight);

    // Ensures all codepoints in utf8Text are in the atlas. If new characters
    // are found, the atlas is re-packed and re-uploaded. Safe to call every
    // frame (no-op when nothing new).
    bool ensureGlyphs(RenderDevice* dev, const char* utf8Text);

    bool   isReady()    const { return baked_ && texId_ != 0; }
    float  pixelHeight() const { return pixelHeight_; }
    float  ascent()     const { return ascent_; }
    float  lineHeight() const { return ascent_ - descent_ + lineGap_; }

    float  scaledLineHeight() const { return (ascent_ - descent_ + lineGap_) * renderScale_; }

    uint32_t textureId() const { return texId_; }

    bool getQuad(uint32_t codepoint, GlyphQuad& out) const;
    float measureText(const char* utf8) const;
    bool hasAllGlyphs(const char* utf8Text) const;

    void setRenderScale(float s) { renderScale_ = s; }
    float renderScale() const { return renderScale_; }

private:
    bool rebuildAtlas(RenderDevice* dev);

    // 字体文件数据进程级共享（性能优化）：同一 OTF（约 16.5MB）被多档
    // 字体/多应用窗口重复加载时只读盘一次、内存只驻一份——旧实现每档
    // 字体独立拷贝 ttf_，3 档 × N 应用 = 数百 MB 冗余。
    // 引擎单线程逐应用驱动，缓存访问无需加锁。
    static std::shared_ptr<const std::vector<uint8_t>> loadFontFile(const char* path);

    std::shared_ptr<const std::vector<uint8_t>> ttf_;  // 共享字体数据（rebake 只读）
    float pixelHeight_ = 24.f;
    float ascent_ = 0.f, descent_ = 0.f, lineGap_ = 0.f;
    int   atlasW_ = 1024, atlasH_ = 1024;
    uint32_t texId_ = 0;
    // 裸指针约定：设备必须活得比 Font 久（析构时 destroyTexture 依赖
    // 设备存活且 GL 上下文 current——由宿主 AppWindow 显式析构序保证：
    // 字体先于设备释放）
    RenderDevice* dev_ = nullptr;
    bool baked_ = false;

    struct Packed { float x0,y0,x1,y1,u0,v0,u1,v1,advance; };
    std::unordered_map<uint32_t, Packed> glyphs_;
    std::unordered_set<uint32_t> bakedCps_;   // O(1) 查找（每帧 ensureGlyphs 热路径）
    float renderScale_ = 1.0f;

    // ---- 图集增量烘焙（活态 pack 上下文）----
    // 旧实现：ensureGlyphs 每遇新字符全量 repack（387+ 字形全部重新
    // 光栅化 + pack + 纹理重建，实测单帧 1633ms 卡顿）。
    // 新实现：pack 上下文跨调用存活（stbtt_pack_context 的 skyline
    // 分配器持续记忆已占矩形），增量路径只光栅化新字形（毫秒级），
    // 全量 repack 仅留给 init/rebake（换像素密度）/图集满扩容。
    // 渲染效果与全量路径逐位一致：同一分配器、同一 oversample、
    // 旧字形 UV 不变（不动已 pack 矩形）。
    stbtt_pack_context pc_ = {};          // 活态 pack 上下文（pcAlive_ 才有效）
    bool pcAlive_ = false;                // PackBegin 成功且未 PackEnd
    std::vector<uint8_t> mono_;           // 图集单声道像素（pc_.pixels 指向此处）
    std::vector<uint8_t> rgba_;           // 上传缓冲（mono→RGBA，复用不重分配）
    std::vector<stbtt_packedchar> packed_;  // 全部已 pack 字形（与 packedCps_ 同序）
    std::vector<uint32_t> packedCps_;       // packed_ 对应码点表
};

} // namespace ui