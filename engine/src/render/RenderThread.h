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
// 渲染线程 + 帧包池 + 工作线程池（线程模型总控，进程级单例）
//
// 模式（UI 主题工作室「其他UI」下拉切换，帧边界生效）：
//   0 = 单线程直通（现状路径，计算+渲染同线程，最稳）
//   1 = 双线程：计算线程（主线程）+ 渲染线程（回放+present）
//   2 = 四线程：双线程 + 2 个工作线程（图集像素转换等 CPU 分片）
//   3 = 全部线程：双线程 + (硬件核心数-2) 个工作线程
//
// 同步协议：
//   - 双缓冲 packet 池（每设备 2 个）：acquire 取空闲包，无空闲则
//     阻塞等渲染线程归还（自然反压，流水线深度 ≤2 帧）；
//   - submit 入队后渲染线程按序回放（多窗口 packet 串行 = 窗口
//     间绘制顺序与单线程完全一致）；
//   - pause()：排空队列 + 等待回放完成 + 线程挂起。窗口创建/销毁/
//     后端切换/线程模式切换前必须 pause（GL 上下文互斥 + 设备生命
//     周期安全），操作完 resume()。
//
// 线程安全约定：设备裸指针作池 key——设备生命周期内使用，销毁设备
// 前必须 pause + releasePool。
// ============================================================
class RenderThread {
public:
    static RenderThread& get();

    // 线程模式切换（帧边界调用；内部自动排空旧管线重建新管线）
    void configure(int mode);
    int  mode() const { return mode_; }
    bool multithread() const { return mode_ > 0; }   // false=主循环走直通路径

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

private:
    RenderThread() = default;
    ~RenderThread();
    RenderThread(const RenderThread&) = delete;
    RenderThread& operator=(const RenderThread&) = delete;

    void renderLoop();
    void workerLoop();

    int  mode_ = 0;
    bool running_ = false;    // 渲染线程存活
    bool paused_  = false;    // pause 状态（渲染线程挂起等待）
    bool stopReq_ = false;
    // ---- 上下文释放握手（粘性绑定的配套协议）----
    // ctxReq_：pause 请求渲线程释放粘性绑定的 GL 上下文（电平）；
    // ctxIdle_：渲线程已解绑/从未绑定（pause 等待的完成信号）。
    // 初始 true：渲线程未启动或未回放过任何包时视为「空闲」。
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

    struct Pool {
        std::unique_ptr<FramePacket> pkts[2];
        bool inUse[2] = {false, false};
    };
    std::unordered_map<RenderDevice*, Pool> pools_;

    // 工作线程池
    std::vector<std::thread> workers_;
    std::vector<std::function<void()>> wjobs_;   // 待领取任务
    size_t wDone_ = 0;                           // 已完成数（配合 wCv_ 等待）
    bool  wStop_ = false;
    std::condition_variable wCv_;

    // ---- 渲线程回放耗时统计（每秒一行日志：与主线程 perf 行对照，
    //      双线程模式的流水线结构可见——计算耗时 vs 回放耗时分离）----
    double  replayMsSum_ = 0.0;                  // 本秒回放累计毫秒
    int     replayN_ = 0;                        // 本秒回放包数
    std::chrono::steady_clock::time_point replayStatT_;   // 本秒起点（epoch=0 表示未起表）
};

} // namespace ui