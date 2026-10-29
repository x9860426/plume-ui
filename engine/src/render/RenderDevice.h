#pragma once
#include "RenderTypes.h"
#include "FramePacket.h"
#include <cstdint>

namespace ui {

// Abstract render device. UI engine is fully decoupled from the backend
// through this interface. Two backends provided:
//   * OpenGL 3.3 core (render/gl/GLDevice)  -- fully working
//   * Vulkan         (render/vk/VKDevice)  -- skeleton, needs Vulkan SDK
class RenderDevice {
public:
    virtual ~RenderDevice() = default;

    // ---- 多线程录制/回放（RenderThread 架构）----
    // setRecording(pkt)：进入录制模式——后续所有接口调用只写 packet
    //   （主线程/计算线程），不执行任何 GL/VK 调用；
    // setRecording(nullptr)：退出录制模式（直通模式，接口直接执行）；
    // replay(pkt)：回放一帧命令流（渲染线程；直通模式不用）。
    // 默认空实现：后端不支持时录制请求被忽略（安全降级为直通）。
    virtual void setRecording(FramePacket* pkt) { (void)pkt; }
    virtual bool isRecording() const { return false; }
    virtual void replay(const FramePacket& pkt) { (void)pkt; }
    // 释放「渲染线程粘性绑定」的上下文（RenderThread::pause 挂起前调用）。
    // 背景：GL 后端 replay 采用粘性绑定（同窗口连续包不再重复
    // makeCurrent——WGL 跨线程上下文迁移有可观测驱动开销，逐包迁移
    // 会使双线程模式零收益）；代价是挂起前必须显式解绑，否则主线程
    // 无法安全接管 GL（设备创建/销毁/模式切回直通）。
    // 默认空实现：VK 等无线程亲和上下文的后端无需处理。
    virtual void releaseThreadContext() {}

    // nativeHandle: GLFWwindow* (platform-specific). w/h in pixels.
    virtual bool init(void* nativeHandle, int w, int h) = 0;
    virtual void shutdown() = 0;
    virtual BackendType type() const = 0;

    virtual void resize(int w, int h) = 0;

    // Frame lifecycle. UI engine fills a vertex/index buffer between
    // beginFrame/endFrame; present swaps to screen.
    virtual void beginFrame() = 0;
    virtual void endFrame()   = 0;
    virtual void present()    = 0;

    virtual void clear(const Color& c) = 0;
    virtual void setViewport(int x, int y, int w, int h) = 0;

    virtual void setScissorRect(int x, int y, int w, int h) = 0;
    virtual void resetScissorRect() = 0;

    // 嵌套裁剪栈：push 后所有绘制被限制，pop 恢复上一级（栈空则禁用）。
    // 默认空实现（后端不支持时退化为无裁剪，语义安全）。
    virtual void pushScissorRect(int x, int y, int w, int h) { (void)x; (void)y; (void)w; (void)h; }
    virtual void popScissorRect() {}

    // SDF 软边抗锯齿总开关（主题工作室实时切换；默认开）
    virtual void setAAEnabled(bool on) {}

    // ---- 窗口截图（引擎内拍照，测试/渲染对比基建）----
    // 替代系统级 CopyFromScreen：直读帧缓冲——无任务栏/其他窗口/光标/
    // 遮挡/DPI 换算噪声；窗口最小化或被遮挡时同样可拍（读的是帧缓冲
    // 而非屏幕）。请求-消费模式（迁移自 999 引擎 AutoScreenshotModule
    // 的 queue/flush 设计，适配多线程）：
    //   requestScreenshot(tag)：任意线程置请求（线程安全，tag 短名）；
    //   设备在下一次 present（直通=主线程 / 录制=渲染线程回放）swap
    //   前读回帧缓冲 → 翻转 Y → PNG 写当前工作目录 shot-<tag>.png
    //   → 打日志（测试脚本轮询文件出现即可）。
    // 默认空实现：后端未支持时安全降级（无文件产出，脚本超时可见）。
    virtual void requestScreenshot(const char* tag) { (void)tag; }

    // 截图落盘公共函数（GL/VK 共用）：RGB888 顶部左上序像素 →
    // shot-<tag>.png（当前工作目录）→ 成功打 info 日志。
    // 实现位于 GLDevice.cpp（PNG 编码 stb_image_write 唯一编译单元）。
    static bool writeScreenshotPNG(const char* tag, const uint8_t* rgb, int w, int h);

    // ---- 渲染目标逻辑尺寸（真·分辨率降档入口）----
    // 语义：设备所有绘制 1:1 光栅化到「逻辑尺寸离屏目标」，present 时
    // 一次性 blit 到实际窗口/交换链（1:1=NEAREST 逐位一致；缩放=LINEAR）。
    // 光栅化像素数 = 逻辑尺寸（360p 档即 640x360，GPU 负载真实下降），
    // 顶点/SDF/裁剪不再做任何缩放（离屏内坐标=逻辑坐标）。
    // 与物理窗口尺寸相同（跟随窗口档）时 blit 1:1，视觉与直通一致。
    // 多线程纪律：实现内只写纯数据（尺寸 + dirty 标记），离屏资源
    // 创建延迟到持上下文/渲染线程的帧路径（GL=beginFrame/replay，
    // VK=presentImpl）——与 resizePending_ 同模式。
    virtual void setTargetSize(int w, int h) { (void)w; (void)h; }

    // Texture (RGBA8, 1 byte per channel). Returns 0 on failure.
    virtual uint32_t createTexture(int w, int h, const uint8_t* rgba) = 0;
    virtual void destroyTexture(uint32_t id) = 0;

    // Immediate draw of a triangle list. Backends may upload a transient
    // VBO/IBO each call (UI batches are small). `textured=false` uses a
    // 1x1 white internal texture so the same shader path applies.
    virtual void drawTriangles(const Vertex* verts, int vcount,
                               const uint32_t* indices, int icount,
                               uint32_t textureId, bool textured) = 0;

    virtual void setSDFParams(int mode, float x, float y, float w, float h,
                              float rTL, float rTR, float rBR, float rBL, float thickness) {}
};

} // namespace ui