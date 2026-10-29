// AI生成
#pragma once
// ============================================================
// 系统动画过渡基座（全引擎唯一动画系统；时间单源、零硬编码）
//
// 规范依据（E:\工作区\1-规范整理）：
//   代码规范 §1.5「动画绑定真实时间」：dt 来自 steady_clock，钳制
//     0~0.1s，禁止帧计数、禁止二次累计；本系统是全局唯一推进点。
//   系统派发规范：业务层只声明「要动画到什么值 / 多久 / 什么缓动」，
//     「怎么插值、什么时候到点」由本系统裁决（what/how 分离）。
//   零硬编码：缓动曲线、默认时长、命名集中在此单源，调用处只报编号。
//   稳定堆对象：Track 按 id 池化复用，推进期无逐帧 new。
//
// 使用者视角（报编号即可，不必写公式）：
//   g_uiCtx.anim.set(动效id, 目标值);                 一句触发过渡
//   g_uiCtx.anim.value(动效id, 缺省值);               每帧取值
//   g_uiCtx.anim.pulse(周期秒, 下限, 上限);           取呼吸/脉冲值
//   anim::ease(anim::OUT_CUBIC, t);                   纯缓动函数
// ============================================================
#include <cstdint>

namespace ui {
namespace anim {

// ---- 缓动曲线（可报编号；0 = 系统跟随）----
// 编号一律显式常量：新曲线中间插入只加常量、不改旧号（同控件模板规则）。
enum : int {
    FOLLOW       = 0,   // 系统跟随（跟随全局默认缓动 g_defaultEase）
    LINEAR       = 1,   // 线性
    IN_QUAD      = 2,   // 缓入
    OUT_QUAD     = 3,   // 缓出
    IN_OUT_QUAD  = 4,   // 缓入缓出
    OUT_CUBIC    = 5,   // 强缓出（默认）
    IN_OUT_CUBIC = 6,   // 强缓入缓出
    OUT_BACK     = 7,   // 回弹（略过冲后回落）
    OUT_ELASTIC  = 8,   // 弹性
    IN_EXPO      = 9,   // 急入
    OUT_EXPO     = 10,  // 急出
};
enum : int { EASE_COUNT = 11 };

static const char* const EASE_NAMES[] = {
    u8"系统跟随", u8"线性", u8"缓入", u8"缓出", u8"缓入缓出",
    u8"强缓出", u8"强缓入缓出", u8"回弹", u8"弹性", u8"急入", u8"急出"
};

// 全局默认缓动 / 默认过渡时长（秒）。FOLLOW 或未点名时使用；
// 由设置/主题工作室可写（单源，各调用处不自带默认值）。
extern int   g_defaultEase;
extern float g_defaultDur;

// 纯函数：把线性进度 t∈[0,1] 映射为该缓动编号的曲线值（无状态）
float ease(int type, float t);

// ---- 单条动画轨道（一个标量随真实时间过渡）----
struct Track {
    float from = 0.0f, to = 0.0f;   // 起止值
    float cur  = 0.0f;              // 当前值
    float t    = 0.0f;              // 已用时间（秒）
    float dur  = 0.25f;             // 总时长（秒；<=0 = 立即到位）
    int   easeType = OUT_CUBIC;     // 缓动编号
    bool  active = false;           // 是否处于过渡中

    void  set(float target, float duration, int easeType);  // 触发过渡
    void  snap(float v);                                    // 立即置值（无过渡）
    void  update(float dt);                                 // 推进一步（唯一入口）
    float value() const { return cur; }
    bool  done()  const { return !active; }
};

// ---- 动画系统（引擎唯一推进点；每应用一份，装在 UiContext）----
class AnimSystem {
public:
    // 系统全局时钟（秒）：每帧 advance 累加一次 dt；循环相位一律由此取，
    // 禁止用帧号（规范 1.5）。
    float time() const { return time_; }

    // 取某 id 的当前动画值；无记录则以 fallback 建立静止轨道并返回 fallback。
    float value(uint32_t id, float fallback = 0.0f);
    // 触发 id 的过渡（时长 <0 = 用 g_defaultDur；缓动 FOLLOW = 全局默认）。
    void  set(uint32_t id, float target, float duration = -1.0f, int easeType = FOLLOW);
    // 立即置值（无过渡）。
    void  snap(uint32_t id, float v);
    // 每帧唯一推进（dt 秒；由 MainUI 用真实帧步长调用一次）。
    void  advance(float dt);

    // 周期脉冲（正弦 0..1 查询；真实时间驱动，period 秒）——呼吸/霓虹闪烁单源。
    // 返回 lo..hi（默认 0..1）；phase 为相位偏移（秒）。
    float pulse(float period, float lo = 0.0f, float hi = 1.0f, float phase = 0.0f);

    void clear() { tracks_.clear(); time_ = 0.0f; }

private:
    Track& ensure(uint32_t id);
    std::unordered_map<uint32_t, Track> tracks_;
    float time_ = 0.0f;
};

} // namespace anim
} // namespace ui