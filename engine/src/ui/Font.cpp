#include "Font.h"
#include "../core/Log.h"
#include "../core/Assets.h"
#include "../render/ThreadScheduler.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ui {

static uint32_t decodeUtf8(const char*& s, const char* end) {
    if (s >= end) return 0;
    uint8_t c = uint8_t(*s++);
    if (c < 0x80) return c;
    if ((c & 0xE0) == 0xC0) {
        uint32_t r = c & 0x1F; if (s < end) r = (r << 6) | (*s++ & 0x3F); return r;
    }
    if ((c & 0xF0) == 0xE0) {
        uint32_t r = c & 0x0F;
        if (s < end) r = (r << 6) | (*s++ & 0x3F);
        if (s < end) r = (r << 6) | (*s++ & 0x3F);
        return r;
    }
    if ((c & 0xF8) == 0xF0) {
        uint32_t r = c & 0x07;
        if (s < end) r = (r << 6) | (*s++ & 0x3F);
        if (s < end) r = (r << 6) | (*s++ & 0x3F);
        if (s < end) r = (r << 6) | (*s++ & 0x3F);
        return r;
    }
    return 0x3F;
}

Font::Font() = default;
Font::~Font() {
    // 活态 pack 上下文先收尾（释放 rect-pack nodes；mono_ 归成员自析构）
    if (pcAlive_) { stbtt_PackEnd(&pc_); pcAlive_ = false; }
    if (dev_ && texId_) dev_->destroyTexture(texId_);
}

// 进程级字体文件缓存（性能优化）：path → 共享字体数据。
// 同一 OTF（约 16.5MB）被多档字号/多应用窗口重复加载时只读盘一次、
// 内存只驻一份——旧实现每档字体独立拷贝 ttf_，3 档 × N 应用 = 数百
// MB 冗余。weak_ptr 语义：所有引用者析构后内存自动释放，缓存项随
// 之悬挂（下次访问时惰性清除），不阻止字体卸载。
// 引擎单线程逐应用驱动，缓存访问无需加锁。
std::shared_ptr<const std::vector<uint8_t>> Font::loadFontFile(const char* path) {
    if (!path) return nullptr;
    static std::unordered_map<std::string, std::weak_ptr<const std::vector<uint8_t>>> cache;
    auto it = cache.find(path);
    if (it != cache.end()) {
        if (auto sp = it->second.lock()) return sp;   // 命中且仍存活：直接复用
        cache.erase(it);                              // 引用已全部释放：清悬挂项
    }
    // UTF-8 路径统一走 Assets（中文路径可用）；行为与 fopen 完全一致
    FILE* f = openFileUtf8(path, "rb");
    if (!f) return nullptr;
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n <= 0) { std::fclose(f); return nullptr; }
    auto buf = std::make_shared<std::vector<uint8_t>>((size_t)n);
    size_t rd = std::fread(buf->data(), 1, buf->size(), f);
    std::fclose(f);
    if (rd != buf->size()) return nullptr;
    cache.emplace(path, buf);
    return buf;
}

bool Font::loadFromFile(const char* path, float pixelHeight) {
    ttf_ = loadFontFile(path);
    if (!ttf_) { UI_LOG_ERROR("font: cannot open %s", path); return false; }
    pixelHeight_ = pixelHeight;
    UI_LOG_INFO("font loaded: %s (%zu bytes, %.0fpx)", path, ttf_->size(), pixelHeight);
    return true;
}


bool Font::init(RenderDevice* dev) {
    if (!ttf_ || ttf_->empty()) { UI_LOG_ERROR("font: no ttf data"); return false; }
    dev_ = dev;
    bakedCps_.clear();
    for (uint32_t c = 32; c < 127; ++c) bakedCps_.insert(c);
    return rebuildAtlas(dev);
}

bool Font::rebake(RenderDevice* dev, float pixelHeight) {
    if (!ttf_ || ttf_->empty() || !dev || pixelHeight <= 0.f) return false;
    if (pixelHeight == pixelHeight_) return true;   // 同字号幂等（无谓重建省 pack+上传）
    pixelHeight_ = pixelHeight;
    // bakedCps_ 原样保留（含 ensureGlyphs 动态加入的中文字符）：
    // 仅换 raster 密度重新 pack，字集不丢
    return rebuildAtlas(dev);
}

bool Font::ensureGlyphs(RenderDevice* dev, const char* utf8Text) {
    if (!dev || !utf8Text) return false;
    if (!ttf_ || ttf_->empty()) return false;
    dev_ = dev;
    if (!baked_) {
        bakedCps_.clear();
        for (uint32_t c = 32; c < 127; ++c) bakedCps_.insert(c);
    }
    // 差集收集：本帧文本中未烘焙的码点（排序保证同输入同布局，稳定复现；
    // int 类型对齐 stbtt_pack_range.array_of_unicode_codepoints 的 int* 签名）
    std::vector<int> newCps;
    const char* s = utf8Text;
    const char* e = s + std::char_traits<char>::length(utf8Text);
    while (s < e) {
        uint32_t cp = decodeUtf8(s, e);
        if (!cp) break;
        if (bakedCps_.find(cp) == bakedCps_.end()) {
            bakedCps_.insert(cp);
            newCps.push_back(cp);
        }
    }
    if (newCps.empty() && baked_) return true;
    // 诊断开关（字体偏差 bug 二分定位）：UI_FONT_INC=0 禁用增量烘焙，
    // 强制回落 988-15 的全量 rebuildAtlas 语义——若 MT 启动 3 秒偏差
    // 随之消失，根因域即锁定「增量烘焙 × 多线程录制」交互
    static const bool incEnabled = [] {
        const char* e = std::getenv("UI_FONT_INC");
        return !(e && e[0] == '0');
    }();
    if (incEnabled && !newCps.empty() && baked_ && pcAlive_) {
        // ---- 增量烘焙：活态分配器只 pack 新字形（毫秒级）----
        // 旧字形矩形不动 → UV 不变；分配失败（图集满）回落全量扩容
        std::sort(newCps.begin(), newCps.end());
        std::vector<stbtt_packedchar> np(newCps.size());
        stbtt_pack_range range;
        std::memset(&range, 0, sizeof(range));
        range.font_size = pixelHeight_;
        range.array_of_unicode_codepoints = newCps.data();
        range.num_chars = (int)newCps.size();
        range.chardata_for_range = np.data();
        if (stbtt_PackFontRanges(&pc_, ttf_->data(), 0, &range, 1) != 0) {
            // 新字形度量入表（GetPackedQuad 输出带 advance 的基线相对 quad）。
            // x/y 必须每字形归零：GetPackedQuad 内部 *xpos += xadvance 累加笔位，
            // 跨迭代复用会把前序新字形 advance 累进本字 x0/advance → 坐标错位
            // （与 rebuildAtlas 全量路径的循环内声明写法严格一致）
            for (size_t i = 0; i < newCps.size(); ++i) {
                float x = 0.f, y = 0.f;
                stbtt_aligned_quad q;
                stbtt_GetPackedQuad(&np[i], atlasW_, atlasH_, 0, &x, &y, &q, 0);
                Packed p;
                p.x0 = q.x0; p.y0 = q.y0; p.x1 = q.x1; p.y1 = q.y1;
                p.u0 = q.s0; p.v0 = q.t0; p.u1 = q.s1; p.v1 = q.t1;
                p.advance = x;
                glyphs_[newCps[i]] = p;
            }
            packed_.insert(packed_.end(), np.begin(), np.end());
            packedCps_.insert(packedCps_.end(), newCps.begin(), newCps.end());
            // mono → RGBA（整图转换，成员缓冲复用零分配）→ 纹理重建上传。
            // 工作线程分片（RenderThread 模式 2/3）：百万级字节纯数据并行
            // 无顺序依赖；单/双线程模式自动退化串行（parallelFor 内置）
            rgba_.resize(mono_.size() * 4);
            ThreadScheduler::get().parallelFor(mono_.size(), [this](size_t b, size_t e) {
                for (size_t i = b; i < e; ++i) {
                    rgba_[i * 4 + 0] = 255;
                    rgba_[i * 4 + 1] = 255;
                    rgba_[i * 4 + 2] = 255;
                    rgba_[i * 4 + 3] = mono_[i];
                }
            });
            if (texId_) { dev_->destroyTexture(texId_); texId_ = 0; }
            texId_ = dev_->createTexture(atlasW_, atlasH_, rgba_.data());
            UI_LOG_INFO("font atlas incremental: +%zu glyphs (total %zu, %dx%d)",
                        newCps.size(), bakedCps_.size(), atlasW_, atlasH_);
            return true;
        }
        // 图集满 → PackEnd 后走全量扩容路径
        stbtt_PackEnd(&pc_); pcAlive_ = false;
        // 回落前把 newCps 留在 bakedCps_（全量会 pack 它们）
    }
    return rebuildAtlas(dev);
}

bool Font::rebuildAtlas(RenderDevice* dev) {
    if (!ttf_ || ttf_->empty() || bakedCps_.empty()) return false;

    stbtt_fontinfo info;
    int offset = stbtt_GetFontOffsetForIndex(ttf_->data(), 0);
    if (!stbtt_InitFont(&info, ttf_->data(), offset)) {
        UI_LOG_ERROR("font: stbtt_InitFont failed");
        return false;
    }
    int a, d, g;
    stbtt_GetFontVMetrics(&info, &a, &d, &g);
    float scale = stbtt_ScaleForPixelHeight(&info, pixelHeight_);
    ascent_  = a * scale;
    descent_ = d * scale;
    lineGap_ = g * scale;

    std::vector<int> cps(bakedCps_.begin(), bakedCps_.end());
    std::sort(cps.begin(), cps.end());

    int atlasW = atlasW_, atlasH = atlasH_;
    if (atlasW < 1024) atlasW = 1024;
    if (atlasH < 1024) atlasH = 1024;

    bool ok = false;
    while (atlasW <= 4096 && atlasH <= 4096) {
        // 旧活态上下文先收尾（换图集尺寸/扩容必须重开分配器）
        if (pcAlive_) { stbtt_PackEnd(&pc_); pcAlive_ = false; }
        mono_.assign((size_t)atlasW * atlasH, 0);
        if (!stbtt_PackBegin(&pc_, mono_.data(), atlasW, atlasH, 0, 1, nullptr)) {
            atlasW *= 2; atlasH *= 2; continue;
        }
        pcAlive_ = true;   // 成功后保持活态：后续增量 pack 复用分配器
        stbtt_PackSetOversampling(&pc_, 2, 2);
        packed_.resize(cps.size());
        stbtt_pack_range range;
        std::memset(&range, 0, sizeof(range));
        range.font_size = pixelHeight_;
        range.array_of_unicode_codepoints = cps.data();
        range.num_chars = (int)cps.size();
        range.chardata_for_range = packed_.data();
        ok = stbtt_PackFontRanges(&pc_, ttf_->data(), 0, &range, 1) != 0;

        if (ok) {
            // mono → RGBA（成员缓冲复用 + 工作线程分片，与增量路径同款）→ 纹理重建上传
            rgba_.assign(mono_.size() * 4, 0);
            ThreadScheduler::get().parallelFor(mono_.size(), [this](size_t b, size_t e) {
                for (size_t i = b; i < e; ++i) {
                    rgba_[i * 4 + 0] = 255;
                    rgba_[i * 4 + 1] = 255;
                    rgba_[i * 4 + 2] = 255;
                    rgba_[i * 4 + 3] = mono_[i];
                }
            });
            if (texId_) { dev_->destroyTexture(texId_); texId_ = 0; }
            texId_ = dev_->createTexture(atlasW, atlasH, rgba_.data());
            atlasW_ = atlasW; atlasH_ = atlasH;
            // 码点表同步（增量路径据此续接 packed_）
            packedCps_.assign(cps.begin(), cps.end());
            break;
        }
        atlasW *= 2; atlasH *= 2;
    }

    if (!ok) {
        if (pcAlive_) { stbtt_PackEnd(&pc_); pcAlive_ = false; }
        UI_LOG_ERROR("font: atlas too small even at %dx%d for %zu glyphs",
                     atlasW, atlasH, cps.size());
        return false;
    }

    glyphs_.clear();
    for (size_t i = 0; i < cps.size(); ++i) {
        float x = 0.f, y = 0.f;
        stbtt_aligned_quad q;
        stbtt_GetPackedQuad(packed_.data(), atlasW_, atlasH_, (int)i, &x, &y, &q, 0);
        Packed p;
        p.x0 = q.x0; p.y0 = q.y0; p.x1 = q.x1; p.y1 = q.y1;
        p.u0 = q.s0; p.v0 = q.t0; p.u1 = q.s1; p.v1 = q.t1;
        p.advance = x;
        glyphs_[uint32_t(cps[i])] = p;
    }
    baked_ = true;
    UI_LOG_INFO("font atlas rebuilt: %zu glyphs, %dx%d, tex=%u",
                cps.size(), atlasW_, atlasH_, texId_);
    return true;
}

bool Font::getQuad(uint32_t codepoint, GlyphQuad& out) const {
    auto it = glyphs_.find(codepoint);
    if (it == glyphs_.end()) it = glyphs_.find(0x3F);
    if (it == glyphs_.end()) return false;
    const Packed& p = it->second;
    out.x0 = p.x0; out.y0 = p.y0; out.x1 = p.x1; out.y1 = p.y1;
    out.u0 = p.u0; out.v0 = p.v0; out.u1 = p.u1; out.v1 = p.v1;
    out.advance = p.advance;
    return true;
}

bool Font::hasAllGlyphs(const char* utf8Text) const {
    if (!utf8Text || !baked_) return false;
    const char* s = utf8Text;
    const char* e = s + std::char_traits<char>::length(utf8Text);
    while (s < e) {
        uint32_t cp = decodeUtf8(s, e);
        if (!cp) break;
        if (glyphs_.find(cp) == glyphs_.end()) return false;
    }
    return true;
}

float Font::measureText(const char* utf8) const {
    if (!utf8) return 0.f;
    const char* s = utf8;
    const char* e = utf8 + std::char_traits<char>::length(utf8);
    float w = 0.f;
    while (s < e) {
        uint32_t cp = decodeUtf8(s, e);
        if (!cp) break;
        GlyphQuad q;
        if (getQuad(cp, q)) w += q.advance;
    }
    return w * renderScale_;
}

} // namespace ui