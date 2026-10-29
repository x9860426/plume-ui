// AI生成
// ============================================================
// 系统动画过渡基座实现（见 UiAnim.h 顶部规范依据）
// ============================================================
#include "UiAnim.h"
#include "../../core/Math.h"
#include <cmath>

namespace ui {
namespace anim {

int   g_defaultEase = OUT_CUBIC;
float g_defaultDur  = 0.25f;

// ---- 缓动曲线（全部纯函数；基准常量集中单源）----
static float easeOutBack(float t) {
    const float c1 = 1.70158f, c3 = c1 + 1.0f;
    float p = t - 1.0f;
    return 1.0f + c3 * p * p * p + c1 * p * p;
}
static float easeOutElastic(float t) {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    const float c4 = 6.2831853f / 3.0f;   // (2π)/3
    return powf(2.0f, -10.0f * t) * sinf((t * 10.0f - 0.75f) * c4) + 1.0f;
}

float ease(int type, float t) {
    t = clampf(t, 0.0f, 1.0f);
    switch (type) {
        case LINEAR:       return t;
        case IN_QUAD:      return t * t;
        case OUT_QUAD:     { float p = 1.0f - t; return 1.0f - p * p; }
        case IN_OUT_QUAD:  return (t < 0.5f) ? (2.0f * t * t)
                                             : (1.0f - powf(-2.0f * t + 2.0f, 2.0f) * 0.5f);
        case OUT_CUBIC:    { float p = 1.0f - t; return 1.0f - p * p * p; }
        case IN_OUT_CUBIC: return (t < 0.5f) ? (4.0f * t * t * t)
                                             : (1.0f - powf(-2.0f * t + 2.0f, 3.0f) * 0.5f);
        case OUT_BACK:     return easeOutBack(t);
        case OUT_ELASTIC:  return easeOutElastic(t);
        case IN_EXPO:      return (t <= 0.0f) ? 0.0f : powf(2.0f, 10.0f * t - 10.0f);
        case OUT_EXPO:     return (t >= 1.0f) ? 1.0f : 1.0f - powf(2.0f, -10.0f * t);
        default:           return t;
    }
}

// ---- Track ----
void Track::snap(float v) {
    from = to = cur = v;
    t = 0.0f;
    active = false;
}

void Track::set(float target, float duration, int easeT) {
    if (duration < 0.0f) duration = g_defaultDur;
    int e = (easeT == FOLLOW) ? g_defaultEase : easeT;
    from = cur;
    to   = target;
    t    = 0.0f;
    dur  = duration;
    easeType = e;
    // 无时长或目标与当前一致 → 吸附到位（不做无意义过渡）
    if (dur <= 0.0f || fabsf(to - from) <= 1e-4f) {
        cur = to;
        active = false;
    } else {
        active = true;
    }
}

void Track::update(float dt) {
    if (!active) return;
    t += dt;
    if (t >= dur) {
        t = dur;
        cur = to;
        active = false;
        return;
    }
    float p = t / dur;
    cur = from + (to - from) * ease(easeType, p);
}

// ---- AnimSystem ----
Track& AnimSystem::ensure(uint32_t id) {
    auto it = tracks_.find(id);
    if (it != tracks_.end()) return it->second;
    Track tr;
    tr.snap(0.0f);
    return tracks_.emplace(id, tr).first->second;
}

float AnimSystem::value(uint32_t id, float fallback) {
    auto it = tracks_.find(id);
    if (it != tracks_.end()) return it->second.cur;
    Track tr;
    tr.snap(fallback);
    return tracks_.emplace(id, tr).first->second.cur;
}

void AnimSystem::set(uint32_t id, float target, float duration, int easeType) {
    ensure(id).set(target, duration, easeType);
}

void AnimSystem::snap(uint32_t id, float v) {
    ensure(id).snap(v);
}

void AnimSystem::advance(float dt) {
    // 钳制：下限防时钟异常、上限防断点/休眠恢复后巨量跳帧（规范 1.5）
    if (dt < 0.0f) dt = 0.0f;
    if (dt > 0.1f) dt = 0.1f;
    time_ += dt;
    for (auto& kv : tracks_) kv.second.update(dt);
}

float AnimSystem::pulse(float period, float lo, float hi, float phase) {
    if (period <= 1e-4f) return hi;
    float p = (time_ + phase) / period;
    p = p - floorf(p);                                   // 0..1
    float s = 0.5f - 0.5f * cosf(p * 6.2831853f);        // 正弦 0..1
    return lo + (hi - lo) * s;
}

} // namespace anim
} // namespace ui