#pragma once
#include "FramePacket.h"
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace ui {

class RenderDevice;

// ============================================================
// 统一线程调度器（替换 RenderThread，进程级单例）
//
// 架构升级要点（在 RenderThread 基础上）：
//   1. 三缓冲帧包池（原双缓冲）：流水线深度 ≤3 帧，计算线程录制 N+2
//      时渲染线程回放 N、N+1 在队列——acquire 反压概率显著降低，
//      高帧率场景（Vulkan 120fps 目标）下吞吐提升；
//   2. parallelFor 阈值降低（8192→2048）+ 最小分片 512：更积极并行化
//      CPU 密集段（字体图集像素转换），小任务也受益；
//   3. 增强性能统计：渲染线程回放耗时 + acquire 阻塞次数 + 流水线
//      深度采样 + 工作线程利用率——双渲染后端帧率调优可观测；
//   4. 自适应工作线程数：帧耗时稳定保持、波动大时缩减（减少同步
//      开销反噬），平滑跟随负载。
//
// 模式（与 RenderThread 完全兼容，主题工作室下拉切换）：
//   0 = 单线程直通（计算+渲染同线程，最稳）
//   1 = 双线程：计算线程（主）+ 渲染线程（回放+present）
//   2 = 四线程：双线程 + 2 工作线程（CPU 分片）
//   3 = 全部线程：双线程 + (硬件核心-2) 工作线程
//
// 同步协议（与 RenderThread 一致，零行为变更）：
//   - 三缓冲 packet 池（每设备 3 个）：acquire 取空闲包，全忙则阻塞
//     等渲染线程归还（自然反压，流水线深度 ≤3 帧）；
//   - submit 入队后渲染线程按序回放（多窗口串行 = 窗口间绘制顺序
//     与单线程完全一致）；
//   - pause()：排空队列 + 等待回放完成 + 线程挂起。窗口创建/销毁/
//     后端切换/线程模式切换前必须 pause（GL 上下文互斥 + 设备生命
//     周期安全），操作完 resume()。
//
// 线程安全约定：设备裸指针作池 key——设备生命周期内使用，销毁设备
// 前必须 pause + releasePool。
// ============================================================
class ThreadScheduler {
public:
    static ThreadScheduler& get();

    // 线程模式切换（帧边界调用；内部自动排空旧管线重建新管线）
    void configure(int mode);
    int  mode() const { return mode_; }
    bool multithread() const { return mode_ > 0; }   // false=主循环走直通路径

    // ---- 后端多线程许可（OpenGL 内核不支持多线程，强制单线程）----
    // setMultithreadAllowed(false)：标记当前后端不允许多线程，
    //   后续 configure 任何 mode>0 都会被强制降为 0（单线程直通）；
    //   若当前已处于多线程模式，立即 configure(0) 切回单线程。
    // setMultithreadAllowed(true)：当前后端允许多线程（如 Vulkan）。
    // 切换渲染后端时必须调用此方法对齐许可，否则 OpenGL 下多线程
    // 渲染必定出问题（GL 上下文跨线程不安全）。
    void setMultithreadAllowed(bool allowed);
    bool multithreadAllowed() const { return multithreadAllowed_; }

    // ---- 多线程路径（multithread() 时主循环调用）----
    FramePacket* acquire(RenderDevice* dev);          // 取该设备空闲包（阻塞至归还）
    void         submit(RenderDevice* dev, FramePacket* pkt);
    void         releasePool(RenderDevice* dev);      // 设备销毁前释放其池（须先 pause）

    // ---- 同步点 ----
    void pause();    // 排空 + 挂起（窗口创建/销毁/模式切换前）
    void resume();

    // ---- 工作线程池（模式 2/3 启用；fn(begin,end) 分片执行）----
    // 池空或工作量过小自动退化为调用线程串行（单线程模式零开销）
    void parallelFor(size_t count, const std::function<void(size_t, size_t)>& fn);

    // ---- 性能统计查询（调试/调优观测；线程安全快照）----
    struct Stats {
        double  replayAvgMs = 0.0;     // 渲染线程回放平均耗时
        int     replayCount = 0;       // 本统计窗口回放包数
        int64_t acquireBlocks = 0;     // acquire 阻塞累计次数（反压指标）
        double  acquireBlockMs = 0.0;  // acquire 阻塞累计耗时
        int     pipelineDepth = 0;     // 当前流水线深度（in-flight 包数）
        int     workerCount = 0;       // 当前工作线程数
        double  workerUtil = 0.0;      // 工作线程利用率（0-1）
    };
    Stats stats() const;

private:
    ThreadScheduler() = default;
    ~ThreadScheduler();
    ThreadScheduler(const ThreadScheduler&) = delete;
    ThreadScheduler& operator=(const ThreadScheduler&) = delete;

    void renderLoop();
    void workerLoop();

    int  mode_ = 0;
    bool multithreadAllowed_ = true;   // 当前后端是否允许多线程（OpenGL=false）
    bool running_ = false;    // 渲染线程存活
    bool paused_  = false;    // pause 状态（渲染线程挂起等待）
    bool stopReq_ = false;
    // ---- 上下文释放握手（粘性绑定的配套协议）----
    bool ctxReq_  = false;
    bool ctxIdle_ = true;
    RenderDevice* lastDev_ = nullptr;   // 渲线程最后回放的设备（挂起前解绑对象）

    std::thread thread_;
    mutable std::mutex mu_;
    std::condition_variable cv_;        // 渲染线程：新任务/停止/恢复
    std::condition_variable doneCv_;    // pause 等待排空
    std::condition_variable freeCv_;    // acquire 等待包归还

    struct Job { RenderDevice* dev; FramePacket* pkt; };
    std::deque<Job> queue_;
    size_t inflight_ = 0;               // 已 submit 未回放归还数

    // ---- 三缓冲帧包池（原双缓冲升级：深度 3 降低反压）----
    static constexpr int kPoolDepth = 3;
    struct Pool {
        std::unique_ptr<FramePacket> pkts[kPoolDepth];
        bool inUse[kPoolDepth] = {false, false, false};
    };
    std::unordered_map<RenderDevice*, Pool> pools_;

    // 工作线程池
    std::vector<std::thread> workers_;
    std::vector<std::function<void()>> wjobs_;   // 待领取任务
    size_t wDone_ = 0;                           // 已完成数（配合 wCv_ 等待）
    bool  wStop_ = false;
    std::condition_variable wCv_;

    // ---- 性能统计 ----
    double  replayMsSum_ = 0.0;                  // 本窗口回放累计毫秒
    int     replayN_ = 0;                        // 本窗口回放包数
    std::chrono::steady_clock::time_point replayStatT_;
    int64_t acquireBlocks_ = 0;                  // acquire 阻塞累计次数
    double  acquireBlockMs_ = 0.0;               // acquire 阻塞累计耗时
    // 工作线程利用率统计
    double  workerBusyMs_ = 0.0;                 // 工作线程忙碌累计毫秒
    double  workerTotalMs_ = 0.0;                // 工作线程存活累计毫秒
    std::chrono::steady_clock::time_point workerStatT_;
    // 自适应工作线程数（帧耗时波动跟随）
    int     adaptiveWorkers_ = 0;                // 当前自适应工作线程数上限
    double  frameMsEMA_ = 0.0;                   // 帧耗时指数移动平均
};

} // namespace ui