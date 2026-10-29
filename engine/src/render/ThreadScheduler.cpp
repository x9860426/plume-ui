#include "ThreadScheduler.h"
#include "RenderDevice.h"
#include "../core/Log.h"

namespace ui {

ThreadScheduler& ThreadScheduler::get() {
    static ThreadScheduler inst;
    return inst;
}

ThreadScheduler::~ThreadScheduler() {
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
void ThreadScheduler::configure(int mode) {
    if (mode < 0 || mode > 3) mode = 0;
    // ---- 后端限制：OpenGL 不支持多线程，强制降为单线程 ----
    // GL 上下文跨线程不安全，OpenGL 后端下任何 mode>0 都会导致渲染问题；
    // setMultithreadAllowed(false) 后此检查生效，configure 自动降级
    if (!multithreadAllowed_ && mode > 0) {
        UI_LOG_WARN("thread scheduler: multithread not allowed by current backend, "
                    "forcing single-thread (requested mode=%d)", mode);
        mode = 0;
    }
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
        thread_ = std::thread(&ThreadScheduler::renderLoop, this);
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
    adaptiveWorkers_ = (int)nw;
    for (unsigned i = 0; i < nw; ++i)
        workers_.emplace_back(&ThreadScheduler::workerLoop, this);

    // 初始化工作线程利用率统计起点
    if (nw > 0 && workerStatT_.time_since_epoch().count() == 0)
        workerStatT_ = std::chrono::steady_clock::now();

    mode_ = mode;
    UI_LOG_INFO("thread scheduler mode set: %d (%s, pool depth=%d)",
                mode,
                mode == 0 ? "single" :
                mode == 1 ? "compute+render" :
                mode == 2 ? "compute+render+2workers" : "compute+render+allworkers",
                kPoolDepth);
}

// ============================================================
// 后端多线程许可设置（切换渲染后端时调用）
// OpenGL → false：不允许多线程，若当前处于多线程模式立即切回单线程
// Vulkan  → true：允许多线程
// ============================================================
void ThreadScheduler::setMultithreadAllowed(bool allowed) {
    multithreadAllowed_ = allowed;
    // 若禁止且当前已处于多线程模式，立即强制切回单线程
    // （不排空管线直接改标志会有 in-flight 包在渲染线程回放——
    //   需走 configure(0) 正常排空切换路径）
    if (!allowed && mode_ > 0) {
        UI_LOG_INFO("thread scheduler: backend disallows multithread, "
                    "forcing single-thread (was mode=%d)", mode_);
        configure(0);
    }
}

FramePacket* ThreadScheduler::acquire(RenderDevice* dev) {
    std::unique_lock<std::mutex> lk(mu_);
    auto& pool = pools_[dev];
    if (!pool.pkts[0]) {                    // 懒初始化（首次使用建三缓冲）
        for (int i = 0; i < kPoolDepth; ++i)
            pool.pkts[i].reset(new FramePacket);
    }
    // 三缓冲全忙 → 阻塞等渲染线程归还（自然反压，深度 ≤3）
    bool willBlock = true;
    for (int i = 0; i < kPoolDepth; ++i)
        if (!pool.inUse[i]) { willBlock = false; break; }
    if (willBlock) {
        const auto blockStart = std::chrono::steady_clock::now();
        ++acquireBlocks_;
        freeCv_.wait(lk, [&] {
            for (int i = 0; i < kPoolDepth; ++i)
                if (!pool.inUse[i]) return true;
            return false;
        });
        acquireBlockMs_ += std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - blockStart).count();
    }
    // 取首个空闲包
    int slot = 0;
    for (int i = 0; i < kPoolDepth; ++i)
        if (!pool.inUse[i]) { slot = i; break; }
    FramePacket* pkt = pool.pkts[slot].get();
    pool.inUse[slot] = true;
    pkt->reset();                           // 清旧帧数据（保容量）
    return pkt;
}

void ThreadScheduler::submit(RenderDevice* dev, FramePacket* pkt) {
    std::lock_guard<std::mutex> lk(mu_);
    queue_.push_back({dev, pkt});
    ++inflight_;
    cv_.notify_one();
}

void ThreadScheduler::releasePool(RenderDevice* dev) {
    // 调用前提：pause 状态（无 in-flight），直接释放该设备全部包
    std::lock_guard<std::mutex> lk(mu_);
    pools_.erase(dev);
}

void ThreadScheduler::pause() {
    std::unique_lock<std::mutex> lk(mu_);
    if (!running_) return;
    // 等队列排空 + 正在回放的包归还 + 渲线程释放粘性上下文。
    ctxReq_ = true;
    cv_.notify_all();
    doneCv_.wait(lk, [this] {
        return inflight_ == 0 && queue_.empty() && ctxIdle_;
    });
    paused_ = true;
}

void ThreadScheduler::resume() {
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
// 两处释放——pause 挂起前（ctxReq_ 握手）与线程退出前（stopReq_）。
// ============================================================
void ThreadScheduler::renderLoop() {
    std::unique_lock<std::mutex> lk(mu_);
    while (true) {
        cv_.wait(lk, [this] {
            return stopReq_ || ctxReq_ || (!paused_ && !queue_.empty());
        });
        if (stopReq_) break;
        if (ctxReq_) {
            // pause 请求：释放粘性上下文后回 wait 睡眠。
            if (lastDev_) { lastDev_->releaseThreadContext(); lastDev_ = nullptr; }
            ctxReq_ = false;
            ctxIdle_ = true;
            doneCv_.notify_all();
            continue;
        }
        Job j = queue_.front();
        queue_.pop_front();
        lk.unlock();
        const auto repT0 = std::chrono::steady_clock::now();
        j.dev->replay(*j.pkt);              // 回放全程不持锁（GL/VK 调用）
        const auto repT1 = std::chrono::steady_clock::now();
        lk.lock();
        // 回放耗时统计（每秒一行日志：与主线程 perf 行对照）
        replayMsSum_ += std::chrono::duration<double, std::milli>(repT1 - repT0).count();
        ++replayN_;
        if (replayStatT_.time_since_epoch().count() == 0) replayStatT_ = repT0;
        if (repT1 - replayStatT_ >= std::chrono::seconds(1)) {
            if (replayN_ > 0) {
                UI_LOG_INFO("ts: replay %d pkts avg=%.2fms depth=%d blocks=%lld "
                            "(render-thread load)",
                            replayN_, replayMsSum_ / replayN_, (int)inflight_,
                            (long long)acquireBlocks_);
                // 自适应：回放耗时高时帧率受限，日志已含指标供调优
            }
            replayMsSum_ = 0.0; replayN_ = 0; replayStatT_ = repT1;
        }
        lastDev_ = j.dev;                   // 记住粘性绑定对象（挂起前解绑）
        auto it = pools_.find(j.dev);
        if (it != pools_.end()) {
            auto& pool = it->second;
            for (int i = 0; i < kPoolDepth; ++i)
                if (pool.pkts[i].get() == j.pkt) { pool.inUse[i] = false; break; }
        }
        --inflight_;
        freeCv_.notify_all();               // 唤醒 acquire 等待者
        doneCv_.notify_all();               // 唤醒 pause 等待者
    }
    // 线程退出前释放粘性上下文
    if (lastDev_) { lastDev_->releaseThreadContext(); lastDev_ = nullptr; }
}

void ThreadScheduler::workerLoop() {
    std::unique_lock<std::mutex> lk(mu_);
    while (true) {
        wCv_.wait(lk, [this] { return wStop_ || !wjobs_.empty(); });
        if (wStop_) break;
        auto job = std::move(wjobs_.back());
        wjobs_.pop_back();
        lk.unlock();
        const auto wT0 = std::chrono::steady_clock::now();
        job();
        const auto wT1 = std::chrono::steady_clock::now();
        lk.lock();
        workerBusyMs_ += std::chrono::duration<double, std::milli>(wT1 - wT0).count();
        ++wDone_;
        wCv_.notify_all();
    }
}

// ============================================================
// 并行分片：把 [0,count) 切成 worker+1 段（调用线程也干活），
// 全部完成后返回。池空/小任务退化串行（避免线程切换开销反噬）。
//
// 改进（vs RenderThread）：
//   - 阈值 8192→2048：更积极并行化（字体图集等中等任务也受益）
//   - 最小分片 512：避免过小分片同步开销反噬
//   - 分片数 = min(worker+1, count/512)：按任务量自适应
// ============================================================
void ThreadScheduler::parallelFor(size_t count,
                               const std::function<void(size_t, size_t)>& fn) {
    if (count == 0) return;
    // 阈值降低：2048 以上才考虑并行（原 8192）
    if (workers_.empty() || count < 2048) { fn(0, count); return; }

    std::unique_lock<std::mutex> lk(mu_);
    const size_t nw = workers_.size();
    // 最小分片 512：分片数不超过 count/512，避免过小任务
    const size_t maxChunks = count / 512;
    const size_t numChunks = std::min(nw + 1, maxChunks > 0 ? maxChunks : 1);
    if (numChunks <= 1) { fn(0, count); return; }
    const size_t chunk = (count + numChunks - 1) / numChunks;
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

// ============================================================
// 性能统计快照（调试/调优观测；线程安全）
// ============================================================
ThreadScheduler::Stats ThreadScheduler::stats() const {
    std::lock_guard<std::mutex> lk(mu_);
    Stats s;
    s.replayCount = replayN_;
    s.replayAvgMs = replayN_ > 0 ? replayMsSum_ / replayN_ : 0.0;
    s.acquireBlocks = acquireBlocks_;
    s.acquireBlockMs = acquireBlockMs_;
    s.pipelineDepth = (int)inflight_;
    s.workerCount = (int)workers_.size();
    // 工作线程利用率 = 忙碌时间 / 存活时间
    if (workerStatT_.time_since_epoch().count() != 0) {
        double totalMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - workerStatT_).count();
        if (totalMs > 0.0)
            s.workerUtil = workerBusyMs_ / (totalMs * (workers_.empty() ? 1 : workers_.size()));
    }
    return s;
}

} // namespace ui