// ============================================================
// 分割器系统实现（系统派发：统一收编分割条的绘制与拖拽）
//
// 视觉：热区居中胶囊条（4×scale 厚）+ 三态边框（普通 inputBorder /
// hover accent / 拖拽中 accent 加深）。
// 交互：按下记录偏移 → 拖动 clamp 写值 → 释放复位；帧末未续期失活。
// ============================================================
#include "SplitterSystem.h"
#include "UiContext.h"
#include "../Element.h"
#include "../Theme.h"
#include <algorithm>

namespace ui {

// 分割条统一样式常量（逻辑像素，使用处乘 scale）
static constexpr float kBarThickness = 4.0f;    // 视觉条厚度
static constexpr float kBarEndInset  = 2.0f;    // 视觉条端部内缩（避热区边缘）


SplitterState* SplitterSystem::findOrCreate(WindowInstance& win, const char* id) {
    for (auto& sp : items_) {
        if (sp.winId == win.id && sp.id == id) return &sp;
    }
    SplitterState sp;
    sp.winId = win.id;
    sp.id = id;
    items_.push_back(sp);
    return &items_.back();
}

// 热区 → 视觉条矩形（居中 + 端部内缩）
static Rect splitterVisualRect(const Rect& bar, bool vertical, float s) {
    if (vertical) {
        float x = bar.pos.x + (bar.size.x - kBarThickness * s) * 0.5f;
        return Rect(Vec2(x, bar.pos.y + kBarEndInset * s),
                    Vec2(kBarThickness * s, bar.size.y - kBarEndInset * 2.0f * s));
    }
    float y = bar.pos.y + (bar.size.y - kBarThickness * s) * 0.5f;
    return Rect(Vec2(bar.pos.x + kBarEndInset * s, y),
                Vec2(bar.size.x - kBarEndInset * 2.0f * s, kBarThickness * s));
}

bool SplitterSystem::draw(RenderDevice* dev, WindowInstance& win,
                           const SplitterRequest& req, float alpha) {
    if (!dev) return false;
    SplitterState* st = findOrCreate(win, req.id);
    st->frameStamp = frame_;
    st->active = true;
    st->barRect = req.barRect;
    st->vertical = req.vertical;
    st->minPos = req.minPos;
    st->maxPos = req.maxPos;
    st->target = req.target;
    st->dirty = req.dirty;

    // 窗口移动同步：窗口位移 → target（屏幕绝对坐标）同步平移，分割线在
    // 悬浮窗内看起来保持原地（对照 SandboxSystem lastViewPos 同款设计）。
    // 注：不能用条中心检测——条位置本身就是 target 算出的（条=target+窗口
    // 偏移），窗口移动时条不动，检测不到位移；必须用窗口原点。
    // 首帧记录基准；拖拽中不平移（拖拽本身在写值）。
    {
        Vec2 winPos(win.x, win.y);
        if (st->winPosInited && !st->dragging && st->target) {
            float dx = winPos.x - st->lastWinPos.x;
            float dy = winPos.y - st->lastWinPos.y;
            if (st->vertical && dx != 0.0f) *st->target += dx;
            if (!st->vertical && dy != 0.0f) *st->target += dy;
        }
        st->lastWinPos = winPos;
        st->winPosInited = true;
    }

    ThemeColors& c = currentColors();
    float s = g_uiCtx.scale;
    Rect vr = splitterVisualRect(req.barRect, req.vertical, s);
    float thick = st->vertical ? vr.size.x : vr.size.y;
    float radius = thick * 0.5f;   // 胶囊圆角 = 厚度一半

    // 三态主色：拖拽中 accent@220 / hover accent@180 / 普通 inputBorder@140
    Color bc = st->dragging ?
        Color(c.accent.r, c.accent.g, c.accent.b, 220.0f / 255.0f * alpha) :
        (st->hover ?
            Color(c.accent.r, c.accent.g, c.accent.b, 180.0f / 255.0f * alpha) :
            Color(c.inputBorder.r, c.inputBorder.g, c.inputBorder.b, 140.0f / 255.0f * alpha));
    int style = g_uiCtx.splitterStyle;
    if (style == 0) {
        // 默认胶囊：bg2 底 + 三态边框
        Color bg = st->dragging ?
            Color(c.accent.r, c.accent.g, c.accent.b, 90.0f / 255.0f * alpha) :
            Color(c.bg2.r, c.bg2.g, c.bg2.b, 200.0f / 255.0f * alpha);
        drawRoundedRect(dev, vr, bg, radius);
        drawRoundedBorder(dev, vr, bc, 1.0f * s, radius);
    } else if (style == 1) {
        // 细线：1.5×scale 细线
        float lw = 1.5f * s;
        Rect lr = st->vertical ?
            Rect(Vec2(vr.pos.x + (vr.size.x - lw) * 0.5f, vr.pos.y), Vec2(lw, vr.size.y)) :
            Rect(Vec2(vr.pos.x, vr.pos.y + (vr.size.y - lw) * 0.5f), Vec2(vr.size.x, lw));
        drawRect(dev, lr, bc);
    } else if (style == 2) {
        // 双线：两条平行细线，间距 4×scale
        float lw = 1.0f * s, gap = 4.0f * s;
        for (int k = 0; k < 2; k++) {
            float off = (k - 0.5f) * gap;
            Rect lr = st->vertical ?
                Rect(Vec2(vr.pos.x + vr.size.x * 0.5f + off - lw * 0.5f, vr.pos.y), Vec2(lw, vr.size.y)) :
                Rect(Vec2(vr.pos.x, vr.pos.y + vr.size.y * 0.5f + off - lw * 0.5f), Vec2(vr.size.x, lw));
            drawRect(dev, lr, bc);
        }
    } else if (style == 3) {
        // 点状：虚线段（段长 6×scale 间隔 4×scale）
        float seg = 6.0f * s, gap = 4.0f * s, lw = 2.0f * s;
        float len = st->vertical ? vr.size.y : vr.size.x;
        for (float p = 0; p < len; p += seg + gap) {
            float dl = (p + seg <= len) ? seg : (len - p);
            Rect lr = st->vertical ?
                Rect(Vec2(vr.pos.x + (vr.size.x - lw) * 0.5f, vr.pos.y + p), Vec2(lw, dl)) :
                Rect(Vec2(vr.pos.x + p, vr.pos.y + (vr.size.y - lw) * 0.5f), Vec2(dl, lw));
            drawRect(dev, lr, bc);
        }
    } else {
        // 纯色：accent 胶囊 + 三态边框
        Color fc = Color(c.accent.r, c.accent.g, c.accent.b, 200.0f / 255.0f * alpha);
        drawRoundedRect(dev, vr, fc, radius);
        drawRoundedBorder(dev, vr, bc, 1.0f * s, radius);
    }

    return st->dragging;
}

SplitterHit SplitterSystem::handlePress(WindowInstance& win, const Vec2& m) {
    SplitterHit result;
    // 热区命中 → 进入拖拽（记录偏移，拖动保持不跳变）+ 吞点击防穿透
    for (int i = (int)items_.size() - 1; i >= 0; i--) {
        SplitterState& st = items_[i];
        if (st.winId != win.id || !st.active) continue;
        if (!st.barRect.contains(m)) continue;
        st.dragging = true;
        float center = st.vertical ?
            (st.barRect.pos.x + st.barRect.size.x * 0.5f) :
            (st.barRect.pos.y + st.barRect.size.y * 0.5f);
        st.dragOff = st.vertical ? (m.x - center) : (m.y - center);
        result.id = st.id.c_str();
        result.consumed = true;
        return result;
    }
    return result;
}

void SplitterSystem::handleDrag(WindowInstance& win, const Vec2& m) {
    for (auto& st : items_) {
        if (st.winId != win.id || !st.active || !st.dragging) continue;
        float pos = st.vertical ? (m.x - st.dragOff) : (m.y - st.dragOff);
        // 拖动范围 clamp（max<=min 视为未配置有效范围，不钳制）
        if (st.maxPos > st.minPos) {
            if (pos < st.minPos) pos = st.minPos;
            if (pos > st.maxPos) pos = st.maxPos;
        }
        if (st.target && *st.target != pos) {
            *st.target = pos;
            if (st.dirty) *st.dirty = true;
        }
    }
}

void SplitterSystem::endDrag() {
    for (auto& st : items_) st.dragging = false;
}

void SplitterSystem::updateHover(WindowInstance& win, const Vec2& m) {
    for (auto& st : items_) {
        if (st.winId != win.id || !st.active) continue;
        st.hover = st.barRect.contains(m);
    }
}

void SplitterSystem::endFrame() {
    // 本帧未续期（frameStamp 停在旧帧）→ 失活（切页/窗口隐藏/tab 切换；
    // 拖拽中失活同时复位 dragging，防止隐藏后残留拖拽态）
    for (auto& st : items_) {
        if (st.frameStamp != frame_) {
            st.active = false;
            st.dragging = false;
            st.hover = false;
        }
    }
    frame_++;
}

void SplitterSystem::removeWindow(int winId) {
    items_.erase(std::remove_if(items_.begin(), items_.end(),
        [winId](const SplitterState& sp) { return sp.winId == winId; }), items_.end());
}

} // namespace ui