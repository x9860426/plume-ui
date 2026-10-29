#include "RenderThread.h"
#include "RenderDevice.h"
#include "../core/Log.h"

namespace ui {

RenderThread& RenderThread::get() {
    static RenderThread inst;
    return inst;
}

RenderThread::~RenderThread() {
    std::unique_lock<std::mutex> lk(mu_);
    // 进程退出兜底：排空后停渲染线程与工作池（正常流程 main 已 configure(0)）
    doneCv_.wait(lk, [this] { return inflight_ == 0 && queue_.empty(); });
    stopReq_ = true;
    paused_ = false;
    cv_.notify_all();
    lk.unlock();
    if (thread_.joinable()) thread_.join();
    wStop_ = true;
    wCv_.notify_all();
    for (auto& t : workers_) if (t.joinable()) t.join();
    workers_.clear();
}

// ============================================================
// 线程模式切换（帧边界调用）
// 顺序：排空当前管线 → 停旧线程组 → 按新模式重建。
// 设备与纹理对象不动——变的是「谁来执行回放」。
// ============================================================
void RenderThread::configure(int mode) {
    if (mode < 0 || mode > 3) mode = 0;
    std::unique_lock<std::mutex> lk(mu_);
    // 排空：切换时不得有录制中/排队中的包（主循环帧边界保证）
    doneCv_.wait(lk, [this] { return inflight_ == 0 && queue_.empty(); });

    // ---- 停旧工作池 ----
    wStop_ = true;
    wCv_.notify_all();
    lk.unlock();
    for (auto& t : workers_) if (t.joinable()) t.join();
    lk.lock();
    workers_.clear();
    wjobs_.clear(); wDone_ = 0;
    wStop_ = false;

    // ---- 渲染线程按需启停 ----
    const bool needThread = (mode > 0);
    if (needThread && !running_) {
        running_ = true; paused_ = false; stopReq_ = false;
        thread_ = std::thread(&RenderThread::renderLoop, this);
    } else if (!needThread && running_) {
        stopReq_ = true;
        cv_.notify_all();
        lk.unlock();
        if (thread_.joinable()) thread_.join();   // join 前 renderLoop 已释放粘性上下文
        lk.lock();
        running_ = false; paused_ = false; stopReq_ = false;
        ctxReq_ = false; ctxIdle_ = true;         // 线程已死：握手状态归零
        lastDev_ = nullptr;
    }

    // ---- 新工作池 ----
    // 模式 2：固定 2 worker；模式 3：硬件核心-2（留出计算+渲染两个主角色）
    unsigned nw = 0;
    if (mode == 2)      nw = 2;
    else if (mode == 3) nw = std::thread::hardware_concurrency() > 2
                               ? std::thread::hardware_concurrency() - 2 : 1;
    for (unsigned i = 0; i < nw; ++i)
        workers_.emplace_back(&RenderThread::workerLoop, this);

    mode_ = mode;
    UI_LOG_INFO("render thread mode set: %d (%s)", mode,
                mode == 0 ? "single" :
                mode == 1 ? "compute+render" :
                mode == 2 ? "compute+render+2workers" : "compute+render+allworkers");
}

FramePacket* RenderThread::acquire(RenderDevice* dev) {
    std::unique_lock<std::mutex> lk(mu_);
    auto& pool = pools_[dev];
    if (!pool.pkts[0]) {                    // 懒初始化（首次使用建双缓冲）
        pool.pkts[0].reset(new FramePacket);
        pool.pkts[1].reset(new FramePacket);
    }
    // 双缓冲全忙 → 阻塞等渲染线程归还（自然反压，深度 ≤2）
    freeCv_.wait(lk, [&] {
        return !pool.inUse[0] || !pool.inUse[1];
    });
    FramePacket* pkt = pool.inUse[0] ? pool.pkts[1].get() : pool.pkts[0].get();
    pool.inUse[pool.pkts[0].get() == pkt ? 0 : 1] = true;
    pkt->reset();                           // 清旧帧数据（保容量）
    return pkt;
}

void RenderThread::submit(RenderDevice* dev, FramePacket* pkt) {
    std::lock_guard<std::mutex> lk(mu_);
    queue_.push_back({dev, pkt});
    ++inflight_;
    cv_.notify_one();
}

void RenderThread::releasePool(RenderDevice* dev) {
    // 调用前提：pause 状态（无 in-flight），直接释放该设备全部包
    std::lock_guard<std::mutex> lk(mu_);
    pools_.erase(dev);
}

void RenderThread::pause() {
    std::unique_lock<std::mutex> lk(mu_);
    if (!running_) return;
    // 等队列排空 + 正在回放的包归还 + 渲线程释放粘性上下文。
    // pause 语义：渲染线程此刻不持任何设备操作与 GL 上下文，
    // 主线程可安全做 GL 上下文切换/设备创建销毁（GL 粘性绑定下
    // 上下文在回放期间归渲染线程持有，挂起前由 ctxReq_ 握手释放）
    ctxReq_ = true;
    cv_.notify_all();
    doneCv_.wait(lk, [this] {
        return inflight_ == 0 && queue_.empty() && ctxIdle_;
    });
    paused_ = true;
}

void RenderThread::resume() {
    std::lock_guard<std::mutex> lk(mu_);
    paused_ = false;
    ctxIdle_ = false;   // 新包回放将重新粘性绑定（ctxReq_ 已由渲线程复位）
    cv_.notify_all();
}

// ============================================================
// 渲染线程主循环：按提交顺序逐包回放（多窗口串行 = 与单线程
// 完全一致的窗口间绘制顺序）；回放完成即归还池并唤醒等待者。
//
// GL 上下文策略：回放期间粘性持有（见 GLDevice::replay），仅在
// 两处释放——pause 挂起前（ctxReq_ 握手，主线程 pause 返回后可
// 安全接管 GL）与线程退出前（stopReq_ 路径，join 后 configure
// 重建/切回直通无「上下文 current 在已死线程」死角）。
// ============================================================
void RenderThread::renderLoop() {
    std::unique_lock<std::mutex> lk(mu_);
    while (true) {
        cv_.wait(lk, [this] {
            return stopReq_ || ctxReq_ || (!paused_ && !queue_.empty());
        });
        if (stopReq_) break;
        if (ctxReq_) {
            // pause 请求：释放粘性上下文后回 wait 睡眠（谓词全假）。
            // 此刻队列必空（pause 先排空才置 ctxReq_），无漏单风险
            if (lastDev_) { lastDev_->releaseThreadContext(); lastDev_ = nullptr; }
            ctxReq_ = false;
            ctxIdle_ = true;
            doneCv_.notify_all();   // 唤醒 pause 等待者
            continue;
        }
        Job j = queue_.front();
        queue_.pop_front();
        lk.unlock();
        const auto repT0 = std::chrono::steady_clock::now();   // 回放耗时统计起点
        j.dev->replay(*j.pkt);              // 回放全程不持锁（GL/VK 调用）
        const auto repT1 = std::chrono::steady_clock::now();
        lk.lock();
        // 每秒一行回放耗时日志：与主线程 perf: 行对照（双线程模式
        // 流水线结构可见——计算/回放各自负载、反压等待可推断）
        replayMsSum_ += std::chrono::duration<double, std::milli>(repT1 - repT0).count();
        ++replayN_;
        if (replayStatT_.time_since_epoch().count() == 0) replayStatT_ = repT0;
        if (repT1 - replayStatT_ >= std::chrono::seconds(1)) {
            if (replayN_ > 0)
                UI_LOG_INFO("rt: replay %d pkts avg=%.2fms (render-thread load)",
                            replayN_, replayMsSum_ / replayN_);
            replayMsSum_ = 0.0; replayN_ = 0; replayStatT_ = repT1;
        }
        lastDev_ = j.dev;                   // 记住粘性绑定对象（挂起前解绑）
        auto it = pools_.find(j.dev);
        if (it != pools_.end()) {
            auto& pool = it->second;
            for (int i = 0; i < 2; ++i)
                if (pool.pkts[i].get() == j.pkt) { pool.inUse[i] = false; break; }
        }
        --inflight_;
        freeCv_.notify_all();               // 唤醒 acquire 等待者
        doneCv_.notify_all();               // 唤醒 pause 等待者
    }
    // 线程退出前释放粘性上下文（configure 停线程 → join → 重建/
    // 直通接管；不释放则主线程 makeCurrent 失败）
    if (lastDev_) { lastDev_->releaseThreadContext(); lastDev_ = nullptr; }
}

void RenderThread::workerLoop() {
    std::unique_lock<std::mutex> lk(mu_);
    while (true) {
        wCv_.wait(lk, [this] { return wStop_ || !wjobs_.empty(); });
        if (wStop_) break;
        auto job = std::move(wjobs_.back());
        wjobs_.pop_back();
        lk.unlock();
        job();
        lk.lock();
        ++wDone_;
        wCv_.notify_all();
    }
}

// ============================================================
// 并行分片：把 [0,count) 切成 worker+1 段（调用线程也干活），
// 全部完成后返回。池空/小任务退化串行（避免线程切换开销反噬）。
// 用途：字体图集 mono→RGBA 转换等可并行 CPU 密集段。
// ============================================================
void RenderThread::parallelFor(size_t count,
                               const std::function<void(size_t, size_t)>& fn) {
    if (count == 0) return;
    if (workers_.empty() || count < 8192) { fn(0, count); return; }

    std::unique_lock<std::mutex> lk(mu_);
    const size_t nw = workers_.size();
    const size_t chunk = (count + nw) / (nw + 1);
    wjobs_.clear();
    wDone_ = 0;
    size_t begin = 0, submitted = 0;
    while (begin < count && submitted < nw) {
        size_t end = begin + chunk;
        if (end > count) end = count;
        const size_t b = begin, e = end;
        wjobs_.push_back([&fn, b, e] { fn(b, e); });
        begin = end;
        ++submitted;
    }
    wCv_.notify_all();
    if (begin < count) {                    // 主调用线程承担剩余尾段
        lk.unlock();
        fn(begin, count);
        lk.lock();
    }
    wCv_.wait(lk, [&] { return wDone_ == submitted; });
}

} // namespace ui