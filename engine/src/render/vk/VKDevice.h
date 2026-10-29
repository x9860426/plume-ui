#pragma once
#include "../RenderDevice.h"
#include <atomic>
#include <vector>
#include <unordered_map>

#if defined(UIENGINE_HAS_VULKAN) && UIENGINE_HAS_VULKAN
#define UI_VK_ON 1
#include <vulkan/vulkan.h>
#else
#define UI_VK_ON 0
#endif

namespace ui {

class VKDevice : public RenderDevice {
public:
    VKDevice();
    ~VKDevice() override;

    bool init(void* nativeHandle, int w, int h) override;
    void shutdown() override;
    BackendType type() const override { return BackendType::Vulkan; }

    // ---- 多线程录制/回放（RenderThread 架构）----
    void setRecording(FramePacket* pkt) override;
    bool isRecording() const override { return rec_ != nullptr; }
    void replay(const FramePacket& pkt) override;

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

    // ---- 窗口截图（引擎内拍照，见 RenderDevice.h 契约注释）----
    void requestScreenshot(const char* tag) override;

    bool fullyCapable() const;

private:
#if UI_VK_ON
    VkInstance               instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice         physical_ = VK_NULL_HANDLE;
    VkDevice                 device_ = VK_NULL_HANDLE;
    VkQueue                  queue_ = VK_NULL_HANDLE;
    uint32_t                 queueFamily_ = 0;
    VkSurfaceKHR             surface_ = VK_NULL_HANDLE;
    VkSwapchainKHR           swapchain_ = VK_NULL_HANDLE;
    VkFormat                 format_ = VK_FORMAT_UNDEFINED;
    VkExtent2D               extent_ = {};
    VkRenderPass             renderPass_ = VK_NULL_HANDLE;
    VkCommandPool            cmdPool_ = VK_NULL_HANDLE;
    VkCommandBuffer          cmd_ = VK_NULL_HANDLE;
    VkSemaphore              imageAvailable_ = VK_NULL_HANDLE;
    VkSemaphore              renderFinished_ = VK_NULL_HANDLE;
    VkFence                  fence_ = VK_NULL_HANDLE;
    // ---- 离屏渲染目标（真·分辨率降档；尺寸=逻辑尺寸 tw_ x th_）----
    // renderPass 渲染到 msaaImage_（MSAA）→ resolve 到 offscreenImage_（单样本），
    // presentImpl 末尾 blit 到交换链 image（1:1=NEAREST / 缩放=LINEAR）。
    // 交换链 image 仅作 blit 目标 + present（fbs_/views_ 随之消亡）。
    VkImage                  msaaImage_ = VK_NULL_HANDLE;
    VkDeviceMemory           msaaMem_ = VK_NULL_HANDLE;
    VkImageView              msaaView_ = VK_NULL_HANDLE;
    VkSampleCountFlagBits    msaaSamples_ = VK_SAMPLE_COUNT_1_BIT;
    VkImage                  offscreenImage_ = VK_NULL_HANDLE;   // resolve 目标 + blit 源
    VkDeviceMemory           offscreenMem_ = VK_NULL_HANDLE;
    VkImageView              offscreenView_ = VK_NULL_HANDLE;
    VkFramebuffer            fb_ = VK_NULL_HANDLE;               // 唯一 fb（离屏，不绑交换链）
    int                      tw_ = 0, th_ = 0;                   // 渲染目标逻辑尺寸
    uint32_t                 offscreenW_ = 0, offscreenH_ = 0;   // 已建离屏尺寸（幂等判据）
    std::vector<VkImage>     images_;

    VkPipeline               pipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout         pipelineLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout    descLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool         descPool_ = VK_NULL_HANDLE;
    VkSampler                sampler_ = VK_NULL_HANDLE;

    VkBuffer                 vbo_ = VK_NULL_HANDLE;
    VkDeviceMemory           vboMem_ = VK_NULL_HANDLE;
    VkBuffer                 ibo_ = VK_NULL_HANDLE;
    VkDeviceMemory           iboMem_ = VK_NULL_HANDLE;
    void*                    vboMapped_ = nullptr;
    void*                    iboMapped_ = nullptr;
    VkDeviceSize             vboCapacity_ = 0;
    VkDeviceSize             iboCapacity_ = 0;

    struct Texture {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory mem = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkDescriptorSet dset = VK_NULL_HANDLE;
    };
    std::unordered_map<uint32_t, Texture> textures_;
    std::vector<Texture>     retired_;   // 帧内销毁的纹理（延迟到 present 后释放，
                                         // 避免本帧 cmds_ 中悬垂的 dset 导致驱动崩溃）
    uint32_t nextTexId_ = 1;
    uint32_t whiteTexId_ = 0;

    std::vector<Vertex>   verts_;
    std::vector<uint32_t> indices_;
    // VK 命令结构移至 FramePacket::VKCmd（跨线程 POD；录制侧记逻辑
    // 纹理 id，dset 查找挪到回放时进行——计算线程不读 textures_ 映射）
    std::vector<FramePacket::VKCmd> cmds_;


    // 裁剪栈当前状态（VK 帧缓冲原点在左上，与 UI 坐标一致，无需翻转）
    struct ScRect { int x, y, w, h; };
    std::vector<ScRect>  scStack_;
    ScRect               scCur_{0, 0, 0, 0};
    bool                 scOn_ = false;
    float                aaOn_ = 1.0f;   // SDF 软边 AA（1=开 0=硬边）


    int    sdfMode_ = 0;
    float  sdfX_ = 0, sdfY_ = 0, sdfW_ = 0, sdfH_ = 0;
    float  sdfRadiusTL_ = 0, sdfRadiusTR_ = 0, sdfRadiusBR_ = 0, sdfRadiusBL_ = 0;
    float  sdfThickness_ = 0;

    void*                    native_ = nullptr;
    bool                     ready_  = false;

    bool recreate();
    void destroySwapchainDeps();
    // 离屏目标（msaa/offscreen image+view+fb）按需重建：tw_/th_ 变化或
    // 首次（renderPass_ 已存在前提）；调用方须在渲染线程/设备空闲点
    void ensureOffscreen();
    bool createPipeline();
    // forceId 非 0 时用指定 id 注册（回放路径：id 由录制侧分配，不推进
    // nextTexId_）；为 0 时自增分配（直通路径）
    uint32_t createTextureInternal(int w, int h, const uint8_t* rgba,
                                   uint32_t forceId = 0);
    void freeTexture(const Texture& t);
    void flushRetired();
    // 帧提交实现（数据源参数化：直通=成员，回放=帧包；渲染线程执行）
    void presentImpl(const std::vector<Vertex>& verts,
                     const std::vector<uint32_t>& indices,
                     const std::vector<FramePacket::VKCmd>& cmds,
                     const Color& clearColor);
    VkCommandBuffer beginOneShot();
    void endOneShot(VkCommandBuffer cmd);
    void transitionImageLayout(VkCommandBuffer cmd, VkImage image,
                               VkImageLayout oldLayout, VkImageLayout newLayout);
    void ensureBuffers(VkDeviceSize vbytes, VkDeviceSize ibytes);
    bool createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags props, VkBuffer& buf, VkDeviceMemory& mem);
#endif
    // ---- 多线程渲染：交换链重建请求（跨线程） ----
    // 多线程模式下渲染线程可能正在回放本设备（presentImpl 执行中），
    // 主线程 resize 不能立即重建（vkDeviceWaitIdle/销毁交换链与回放
    // 竞争）——只挂请求，presentImpl 开头由渲染线程串行消费；
    // 单线程直通路径 resize() 内立即重建，此标志恒 false 零开销
    std::atomic<bool> resizePending_{false};

    // ---- 渲染目标尺寸变更请求（跨线程）----
    // setTargetSize 主线程只写纯数据（tw_/th_ + 此标记），presentImpl
    // 开头（渲染线程）waitFence 后消费——GPU 空闲点重建离屏资源
    std::atomic<bool> targetDirty_{true};   // 首帧必建

    // ---- 多线程录制状态（无条件编译：setRecording/isRecording 接口签名
    //      依赖 rec_，stub 构建也必须可编译；stub 实现为空置 nullptr）----
    // 录制目标（计算线程持有；nullptr=直通）。几何数据录制时直写 rec_
    //（零拷贝、包池容量跨帧复用），present 打包纹理命令。
    FramePacket* rec_ = nullptr;
    // 帧内外累积的纹理创建/销毁命令（present 时移交包；跨帧不清——
    // 两帧之间的图集烘焙命令随下一个包走，不丢失）
    std::vector<FramePacket::VKTexCmd> texCmdsRec_;

    Color  clearColor_{0.10f, 0.11f, 0.14f, 1.f};
    bool   drawWarned_ = false;

    // ---- 窗口截图（请求-消费，跨线程安全；同 GLDevice 语义）----
    // tag 先写后 store-release；presentImpl（渲染线程/主线程直通）
    // renderPass 后 blit 到 staging，present 完成等 fence 读回调 PNG
    void ensureShotStaging();          // 懒创建/尺寸变化重建 host-visible staging
    std::atomic<bool> shotReq_{false};
    char shotTag_[64] = {0};
#if UI_VK_ON
    VkImage        shotImg_ = VK_NULL_HANDLE;      // 线性平铺读回目标
    VkDeviceMemory shotMem_ = VK_NULL_HANDLE;
    VkExtent2D     shotExtent_ = {};               // staging 建立时尺寸（≠extent_ 时重建）
#endif
};

} // namespace ui