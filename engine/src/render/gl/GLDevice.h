#pragma once
#include "../RenderDevice.h"
#include <array>
#include <atomic>
#include <unordered_map>
#include <vector>

namespace ui {

class GLDevice : public RenderDevice {
public:
    GLDevice();
    ~GLDevice() override;

    bool init(void* nativeHandle, int w, int h) override;
    void shutdown() override;
    BackendType type() const override { return BackendType::OpenGL; }

    // ---- 多线程录制/回放（RenderThread 架构）----
    void setRecording(FramePacket* pkt) override { rec_ = pkt; }
    bool isRecording() const override { return rec_ != nullptr; }
    void replay(const FramePacket& pkt) override;
    // 渲染线程粘性上下文释放（RenderThread::pause 挂起前调用；见
    // RenderDevice.h 契约注释——粘性绑定消除逐包跨线程迁移开销）
    void releaseThreadContext() override;

    // ---- 窗口截图（引擎内拍照，见 RenderDevice.h 契约注释）----
    void requestScreenshot(const char* tag) override;

    void resize(int w, int h) override;
    void beginFrame() override;
    void endFrame()   override;
    void present()    override;

    void clear(const Color& c) override;
    void setViewport(int x, int y, int w, int h) override;
    void setScissorRect(int x, int y, int w, int h) override;
    void resetScissorRect() override;
    void pushScissorRect(int x, int y, int w, int h) override;
    void popScissorRect() override;

    uint32_t createTexture(int w, int h, const uint8_t* rgba) override;
    void destroyTexture(uint32_t id) override;

    void drawTriangles(const Vertex* verts, int vcount,
                       const uint32_t* indices, int icount,
                       uint32_t textureId, bool textured) override;

    void setSDFParams(int mode, float x, float y, float w, float h,
                      float rTL, float rTR, float rBR, float rBL, float thickness) override;
    void setAAEnabled(bool on) override;
    // 渲染目标逻辑尺寸（真·分辨率降档，见 RenderDevice.h 契约注释）
    void setTargetSize(int w, int h) override;

private:
    bool compileShaders();
    // ---- 帧内合批（SDFMode=0 纯色/纹理路径）----
    // 连续相同纹理的绘制合并为一次缓冲上传 + 一次 draw call；批内
    // 顶点/索引按提交顺序追加（渲染效果与逐元素绘制逐位一致——
    // 相同数据、相同顺序、相同混合栈）。SDF 路径（uniform 逐绘制）
    // 与裁剪/视口/清屏状态变化时强制拆批保序。
    void flushBatch();
    std::vector<Vertex>    batchV_;   // 批累积顶点（clear 保容量跨帧复用）
    std::vector<uint32_t>  batchI_;   // 批累积索引（偏移=追加时批内顶点数）
    uint32_t batchTex_ = 0;           // 当前批纹理 id（0=纯色 whiteTex）
    bool     batchValid_ = false;     // 批状态已初始化（batchTex_ 有效）

    // 多线程录制目标（计算线程持有；nullptr=直通模式）。不拥有指针，
    // 生命周期由 RenderThread 池管理（acquire→setRecording→submit）
    FramePacket* rec_ = nullptr;

    void*  native_ = nullptr;
    int    w_ = 0, h_ = 0;          // 物理窗口尺寸（blit 目标区域/截图尺寸）
    bool   ready_ = false;

    // ---- 离屏渲染目标（真·分辨率降档）----
    // 所有绘制 1:1 光栅化到 tw_ x th_ 的 MSAA FBO，present 时 blit 到
    // 默认帧缓冲（1:1=NEAREST；缩放=先 NEAREST resolve 到单样本 FBO、
    // 再 LINEAR 放大——MSAA read fb 的 blit 规范要求 NEAREST）。
    // targetDirty_ 跨线程置位（setTargetSize 纯数据写入），FBO 重建延迟
    // 到持上下文线程（直通 beginFrame / 回放 replay BeginFrame）。
    int    tw_ = 0, th_ = 0;              // 渲染目标逻辑尺寸
    bool   targetDirty_ = true;           // 首帧必建
    unsigned msaaFbo_ = 0;                // MSAA 离屏帧缓冲（renderbuffer 附件）
    unsigned msaaColorRB_ = 0;            // MSAA 颜色 renderbuffer（tw_ x th_ x samples）
    int     msaaSamplesGL_ = 0;           // 窗口默认帧缓冲实际样本数（init 查询）
    unsigned resolveFbo_ = 0;             // 单样本中间 FBO（非 1:1 放大路径用）
    unsigned resolveTex_ = 0;             // 单样本纹理附件（tw_ x th_）

    unsigned program_  = 0;
    unsigned vao_      = 0;
    unsigned vbo_      = 0;
    unsigned ibo_      = 0;
    int     locResolution_ = -1;
    int     locTex_        = -1;
    int     locSDFMode_    = -1;
    int     locRectPos_    = -1;
    int     locRectSize_   = -1;
    int     locRadiusTL_   = -1;
    int     locRadiusTR_   = -1;
    int     locRadiusBR_   = -1;
    int     locRadiusBL_   = -1;
    int     locBorderThk_  = -1;
    int     locAAOn_       = -1;

    int     sdfMode_      = 0;
    float   sdfX_ = 0, sdfY_ = 0, sdfW_ = 0, sdfH_ = 0;
    float   sdfRadiusTL_ = 0, sdfRadiusTR_ = 0, sdfRadiusBR_ = 0, sdfRadiusBL_ = 0;
    float   sdfThickness_ = 0;
    float   aaOn_ = 1.0f;             // SDF 软边 AA（1=开 0=硬边）


    unsigned whiteTex_ = 0;
    uint32_t nextId_   = 1;
    std::unordered_map<uint32_t, unsigned> textures_;
    std::vector<std::array<int, 4>> scissorStack_;

    // ---- 截图请求（请求-消费模式，跨线程安全）----
    // tag 写读序：主线程先写 tag_ 再 shotReq_.store(release)；消费侧
    // （直通=主线程 present / 回放=渲染线程 Present 命令）load(acquire)
    // 后读 tag_——acquire/release 配对保证 tag 可见性，无需锁。
    void captureSnapshotGL();        // swap 前读帧缓冲 + 翻 Y + 落盘 PNG
    // 离屏 FBO 按需重建（tw_/th_ 或样本数变化时；调用方须持 GL 上下文）
    void ensureOffscreenMSAA();
    // 离屏画面 blit 到默认帧缓冲（MSAA resolve + 缩放；调用方须持上下文）
    void blitToScreen();
    std::atomic<bool> shotReq_{ false };
    char shotTag_[64] = { 0 };
};

} // namespace ui