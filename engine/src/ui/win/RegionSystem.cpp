// AI生成
// ============================================================
// 区域申请系统实现（规范核心：系统统一派发滚动/裁剪/滚动条）
// ============================================================
#include "RegionSystem.h"
#include "../Element.h"
#include "../Theme.h"
#include "StyleTpl.h"
#include "UiContext.h"      // 完整 UiContext（读 scrollbarStyle/scale）
#include <algorithm>
#include <chrono>

namespace ui {

// 旧全局单例 g_regions 已删除：多应用架构后每应用独立
// （AppFwd.h 宏转发到 AppManager 当前应用的 regions 成员）

// ---- 滚动条系统统一样式常量（逻辑像素，使用处乘 scale）----
static constexpr float kSliderMinH = 10.0f;   // 滑块最小高度（逻辑）
static constexpr float kWheelStep  = 40.0f;   // 滚轮/箭头按钮单步滚动（逻辑）
static constexpr float kHoldDelay  = 0.35f;   // 箭头按钮长按首次连滚延迟（秒）
static constexpr float kHoldRepeat = 0.05f;   // 箭头按钮连滚间隔（秒）

// ---- 滚动条模板单一取值（系统层唯一读取点，全项目仅此处算滚动条几何）----
// 存储字段 g_uiCtx.scrollbarStyle = 内部 0 基渲染值（见 StyleTpl.h scrollbar）
static tpl::scrollbar::Geom scrollbarGeom() {
    return tpl::scrollbar::geomOf(g_uiCtx.scrollbarStyle);
}
// 轨道整段占宽（含内边距，屏幕像素）= "全局规划"给内容让位的依据；
// 换模板（细轨↔粗胶囊）即自动改变让位量，扩展代码零改动。
static float scrollbarLane() {
    tpl::scrollbar::Geom g = scrollbarGeom();
    return (g.trackW + g.pad * 2.0f) * g_uiCtx.scale;
}
// 真实时间秒（steady_clock；长按连滚计时，与帧率无关）
static float nowSec() {
    using namespace std::chrono;
    return (float)duration_cast<duration<float>>(steady_clock::now().time_since_epoch()).count();
}

RegionState* RegionSystem::findOrCreate(WindowInstance& win, const char* id) {
    for (auto& r : regions_) {
        if (r.winId == win.id && r.id == id) return &r;
    }
    RegionState r;
    r.winId = win.id;
    r.id = id;
    r.seq = nextSeq_++;
    regions_.push_back(r);
    return &regions_.back();
}

RegionHandle RegionSystem::begin(RenderDevice* dev, WindowInstance& win, const RegionRequest& req) {
    RegionHandle h;
    h.valid = false;
    if (req.w <= 0 || req.h <= 0 || !dev) {
        // 窗口缩到布局临界（resize 拖动中）→ 立即失活已有区域：
        // 否则保留上帧 active/hasScrollbar/旧矩形，同帧 renderScrollbars
        // 会按旧窗口尺寸位置画滚动条（错位/超出底板），命中/滚轮也响应旧区域
        for (auto& r : regions_) {
            if (r.winId == win.id && r.id == req.id) {
                r.active = false;
                r.hasScrollbar = false;
                r.draggingScrollbar = false;
                r.hoverScrollbar = false;
                break;
            }
        }
        return h;
    }

    RegionState* r = findOrCreate(win, req.id);
    r->tabId = req.tabId;
    r->frameStamp = frame_;
    r->active = true;
    // 视口 = 窗口位置 + 相对偏移（屏幕绝对坐标）
    r->view = Rect(Vec2(win.x + req.x, win.y + req.y), Vec2(req.w, req.h));

    // 内容宽度：预留滚动条空间与否由 end() 根据内容高度决定；
    // begin 时先给全宽，若上一帧已有滚动条则本帧预留（避免内容跳动）。
    // 预留宽度 = 当前滚动条模板的轨道占宽（样式感知：换模板即自动改让位量）。
    float barSpace = r->hasScrollbar ? scrollbarLane() : 0.0f;

    h.valid = true;
    h.winId = win.id;
    h.regionSeq = r->seq;
    h.clipX = r->view.pos.x;
    h.clipY = r->view.pos.y;
    h.clipW = r->view.size.x;
    h.clipH = r->view.size.y;
    h.scrollY = r->scrollY;
    h.contentW = r->view.size.x - barSpace;
    r->contentW = h.contentW;           // 供交互期 getContentW 读（与绘制几何一致）
    h.hasScrollbar = r->hasScrollbar;

    // 系统统一设置裁剪（规范：扩展代码不写裁剪）
    // 全局规划自动适配：内容裁剪到 contentW（滚动条轨道独占右侧一列），
    // 即使扩展代码按全宽绘制也绝不与滚动条重叠；无滚动条时不裁（内容占全宽）。
    {
        float clipW = h.clipW - barSpace;
        if (clipW < 1.0f) clipW = 1.0f;
        dev->pushScissorRect((int)h.clipX, (int)h.clipY, (int)clipW, (int)h.clipH);
    }
    return h;
}

void RegionSystem::end(RenderDevice* dev, const RegionHandle& h, float contentH) {
    if (!h.valid || !dev) return;
    // 系统恢复裁剪（恢复到外层窗口级，而非完全禁用）
    dev->popScissorRect();

    RegionState* r = nullptr;
    for (auto& reg : regions_) {
        if (reg.winId == h.winId && reg.seq == h.regionSeq) { r = &reg; break; }
    }
    if (!r) return;

    r->contentH = contentH;
    // 自动滚动容器判定：内容超出视口 → 生成滚动条
    r->hasScrollbar = contentH > r->view.size.y + 0.5f;
    if (r->hasScrollbar) layoutScrollbar(*r);
    // scrollY clamp（窗口缩放后自动适配）
    float maxScroll = contentH - r->view.size.y;
    if (maxScroll < 0) maxScroll = 0;
    if (r->scrollY > maxScroll) r->scrollY = maxScroll;
    if (r->scrollY < 0) r->scrollY = 0;
}

void RegionSystem::layoutScrollbar(RegionState& r) {
    tpl::scrollbar::Geom g = scrollbarGeom();
    float s = g_uiCtx.scale;
    float tw  = g.trackW * s;                    // 轨道宽（模板单源）
    float pad = g.pad * s;
    float btnH = g.btnH * s;                      // 两端按钮高（0=无按钮）
    float x = r.view.pos.x + r.view.size.x - tw - pad;   // 轨道左缘（屏幕绝对）
    float top = r.view.pos.y + pad;
    float bottom = r.view.pos.y + r.view.size.y - pad;

    // 两端箭头按钮（模板带按钮时占两端；无按钮=零矩形，命中/绘制自动跳过）
    if (btnH > 0.0f) {
        r.btnUp   = Rect(Vec2(x, top), Vec2(tw, btnH));
        r.btnDown = Rect(Vec2(x, bottom - btnH), Vec2(tw, btnH));
    } else {
        r.btnUp = Rect();
        r.btnDown = Rect();
    }

    // 中间轨道段（按钮之间）
    float trackTop = top + btnH;
    float trackH   = (bottom - btnH) - trackTop;
    if (trackH < tw) trackH = tw;                // 退化保护（区域过矮）
    r.scrollbarTrack = Rect(Vec2(x, trackTop), Vec2(tw, trackH));

    float sliderH = (r.view.size.y / r.contentH) * trackH;
    if (sliderH < kSliderMinH * s) sliderH = kSliderMinH * s;
    if (sliderH > trackH) sliderH = trackH;
    float maxScroll = r.contentH - r.view.size.y;
    float sliderY = trackTop + (maxScroll > 0 ? (r.scrollY / maxScroll) * (trackH - sliderH) : 0);
    r.scrollbarSlider = Rect(Vec2(x, sliderY), Vec2(tw, sliderH));
}

// 箭头按钮/滚轮单步滚动（系统统一，clamp 后重排布局）
void RegionSystem::stepScroll(RegionState& r, int dir) {
    float maxScroll = r.contentH - r.view.size.y;
    if (maxScroll <= 0.0f) return;
    r.scrollY += dir * kWheelStep;
    if (r.scrollY < 0.0f) r.scrollY = 0.0f;
    if (r.scrollY > maxScroll) r.scrollY = maxScroll;
    layoutScrollbar(r);
}

bool RegionSystem::handleWheel(WindowInstance& win, const Vec2& m, float delta) {
    // 自后向前找第一个命中的激活区域（后申请的区域在上层）
    for (int i = (int)regions_.size() - 1; i >= 0; i--) {
        RegionState& r = regions_[i];
        if (r.winId != win.id || !r.active) continue;
        if (r.tabId != win.tabSel) continue;      // 标签未激活 → 整组不响应（规范3.5）
        if (!r.view.contains(m)) continue;
        float maxScroll = r.contentH - r.view.size.y;
        if (maxScroll <= 0) continue;             // 无滚动容器 → 不消费，允许穿透
        r.scrollY -= delta * kWheelStep;
        if (r.scrollY < 0) r.scrollY = 0;
        if (r.scrollY > maxScroll) r.scrollY = maxScroll;
        if (r.hasScrollbar) layoutScrollbar(r);
        return true;
    }
    return false;
}

bool RegionSystem::handlePress(WindowInstance& win, const Vec2& m) {
    for (int i = (int)regions_.size() - 1; i >= 0; i--) {
        RegionState& r = regions_[i];
        if (r.winId != win.id || !r.active || !r.hasScrollbar) continue;
        if (r.tabId != win.tabSel) continue;

        // ① 两端箭头按钮（模板带按钮）：单击步进 + 进入长按连滚
        if (r.btnUp.size.y > 0.0f && r.btnUp.contains(m)) {
            stepScroll(r, -1);
            r.holdDir = -1;
            r.holdNext = nowSec() + kHoldDelay;
            return true;
        }
        if (r.btnDown.size.y > 0.0f && r.btnDown.contains(m)) {
            stepScroll(r, +1);
            r.holdDir = +1;
            r.holdNext = nowSec() + kHoldDelay;
            return true;
        }

        // ② 滑块 / 轨道（含少量宽容度）→ 系统接管拖拽
        float pad = scrollbarGeom().pad * g_uiCtx.scale;
        Rect hit = Rect(Vec2(r.scrollbarTrack.pos.x - pad, r.scrollbarTrack.pos.y),
                        Vec2(r.scrollbarTrack.size.x + pad * 2.0f, r.scrollbarTrack.size.y));
        if (!hit.contains(m)) continue;
        r.draggingScrollbar = true;
        r.scrollbarDragOffY = m.y - r.scrollbarSlider.pos.y;
        // 点击轨道空白处 → 跳到对应位置
        if (!r.scrollbarSlider.contains(m)) {
            float trackH = r.scrollbarTrack.size.y;
            float sliderH = r.scrollbarSlider.size.y;
            float newY = m.y - sliderH * 0.5f - r.scrollbarTrack.pos.y;
            if (newY < 0) newY = 0;
            if (newY > trackH - sliderH) newY = trackH - sliderH;
            float maxScroll = r.contentH - r.view.size.y;
            r.scrollY = (trackH - sliderH) > 0 ? (newY / (trackH - sliderH)) * maxScroll : 0;
            layoutScrollbar(r);
        }
        return true;
    }
    return false;
}

void RegionSystem::handleDrag(WindowInstance& win, const Vec2& m) {
    float now = nowSec();
    for (auto& r : regions_) {
        if (r.winId != win.id) continue;
        // 箭头按钮长按连滚（按住不放按固定间隔持续步进；steady_clock 计时，与帧率无关）
        if (r.holdDir != 0 && now >= r.holdNext) {
            stepScroll(r, r.holdDir);
            r.holdNext = now + kHoldRepeat;
        }
        if (!r.draggingScrollbar) continue;
        float trackH = r.scrollbarTrack.size.y;
        float sliderH = r.scrollbarSlider.size.y;
        float newY = m.y - r.scrollbarDragOffY - r.scrollbarTrack.pos.y;
        if (newY < 0) newY = 0;
        if (newY > trackH - sliderH) newY = trackH - sliderH;
        float maxScroll = r.contentH - r.view.size.y;
        r.scrollY = (trackH - sliderH) > 0 ? (newY / (trackH - sliderH)) * maxScroll : 0;
        if (r.scrollY < 0) r.scrollY = 0;
        if (r.scrollY > maxScroll) r.scrollY = maxScroll;
        layoutScrollbar(r);
    }
}

void RegionSystem::updateHover(WindowInstance& win, const Vec2& m) {
    for (auto& r : regions_) {
        if (r.winId != win.id || !r.active || !r.hasScrollbar) {
            if (r.winId == win.id) { r.hoverScrollbar = false; r.hoverBtn = 0; }
            continue;
        }
        if (r.tabId != win.tabSel) { r.hoverScrollbar = false; r.hoverBtn = 0; continue; }
        // 箭头按钮 hover（0=无 1=上 2=下）
        r.hoverBtn = (r.btnUp.size.y > 0.0f && r.btnUp.contains(m)) ? 1
                   : ((r.btnDown.size.y > 0.0f && r.btnDown.contains(m)) ? 2 : 0);
        float pad = scrollbarGeom().pad * g_uiCtx.scale;
        Rect hit = Rect(Vec2(r.scrollbarTrack.pos.x - pad, r.scrollbarTrack.pos.y),
                        Vec2(r.scrollbarTrack.size.x + pad * 2.0f, r.scrollbarTrack.size.y));
        r.hoverScrollbar = hit.contains(m);
    }
}

const RegionState* RegionSystem::findRegion(int winId, const char* id) const {
    for (auto& r : regions_) {
        if (r.winId == winId && r.id == id) return &r;
    }
    return nullptr;
}

float RegionSystem::getScroll(int winId, const char* id) const {
    const RegionState* r = findRegion(winId, id);
    return r ? r->scrollY : 0.0f;
}

float RegionSystem::getContentW(int winId, const char* id) const {
    const RegionState* r = findRegion(winId, id);
    return r ? r->contentW : 0.0f;
}

// 箭头按钮（上/下）：圆角底 + 三角箭头（fillTriangle，无需字体图集）
static void drawScrollArrowBtn(RenderDevice* dev, const Rect& r, bool up, bool hov,
                               const ThemeColors& c, float alpha, float radius) {
    if (r.size.y <= 0.0f) return;
    Color bg = hov ? Color(c.accent.r, c.accent.g, c.accent.b, 70.0f / 255.0f * alpha)
                   : Color(c.groupBorder.r, c.groupBorder.g, c.groupBorder.b, 55.0f / 255.0f * alpha);
    drawRoundedRect(dev, r, bg, radius);
    float cx = r.pos.x + r.size.x * 0.5f;
    float cy = r.pos.y + r.size.y * 0.5f;
    float hw = r.size.x * 0.22f;   // 箭头半宽
    float hh = r.size.y * 0.20f;   // 箭头半高
    Color ac = hov ? Color(c.accent.r, c.accent.g, c.accent.b, 240.0f / 255.0f * alpha)
                   : Color(c.text.r, c.text.g, c.text.b, 175.0f / 255.0f * alpha);
    if (up) fillTriangle(dev, cx, cy - hh, cx - hw, cy + hh, cx + hw, cy + hh, ac);
    else    fillTriangle(dev, cx, cy + hh, cx - hw, cy - hh, cx + hw, cy - hh, ac);
}

void RegionSystem::renderScrollbars(RenderDevice* dev, WindowInstance& win, float alpha) {
    ThemeColors& c = currentColors();
    tpl::scrollbar::Geom g = scrollbarGeom();
    float tw  = g.trackW * g_uiCtx.scale;
    float rad = tw * 0.5f;                        // 胶囊/细轨统一半宽圆角
    // 系统级裁剪：滚动条几何统一裁到窗口（内容区/悬浮窗）之内。
    // 背景——内嵌滚动容器滚出可视区时，其滚动条（区域系统在页渲染之后统一补绘、
    // 此时页面裁剪已出栈）会溢出到窗口之外（表现为"超出底/超边"）。此处按
    // 当前滚动条所属区域 view ∩ 窗口矩形 重新入栈裁剪，属系统统一规划，非页面补丁。
    Rect winRect(Vec2(win.x, win.y), Vec2(win.w, win.h));
    for (auto& r : regions_) {
        if (r.winId != win.id || !r.active || !r.hasScrollbar) continue;
        if (r.tabId != win.tabSel) continue;
        // 裁剪矩形 = r.view ∩ winRect（无重叠则跳过本区域，绝不越窗）
        float cx0 = std::max(r.view.pos.x, winRect.pos.x);
        float cy0 = std::max(r.view.pos.y, winRect.pos.y);
        float cx1 = std::min(r.view.pos.x + r.view.size.x, winRect.pos.x + winRect.size.x);
        float cy1 = std::min(r.view.pos.y + r.view.size.y, winRect.pos.y + winRect.size.y);
        if (cx1 <= cx0 || cy1 <= cy0) continue;
        dev->pushScissorRect((int)cx0, (int)cy0, (int)(cx1 - cx0), (int)(cy1 - cy0));
        // 轨道（细轨=淡；粗胶囊模板略提亮，保证可见性）
        float trackA = (g.trackW > 6.0f ? 90.0f : 60.0f) / 255.0f * alpha;
        Color trackCol = Color(c.groupBorder.r, c.groupBorder.g, c.groupBorder.b, trackA);
        drawRoundedRect(dev, r.scrollbarTrack, trackCol, rad);
        // 滑块（hover/拖拽 → accent；否则 tabText）
        Color sliderCol = (r.hoverScrollbar || r.draggingScrollbar) ?
            Color(c.accent.r, c.accent.g, c.accent.b, 205.0f / 255.0f * alpha) :
            Color(c.tabText.r, c.tabText.g, c.tabText.b, 150.0f / 255.0f * alpha);
        drawRoundedRect(dev, r.scrollbarSlider, sliderCol, rad);
        // 两端箭头按钮（模板带按钮）
        if (g.btnH > 0.0f) {
            drawScrollArrowBtn(dev, r.btnUp,   true,  r.hoverBtn == 1, c, alpha, rad);
            drawScrollArrowBtn(dev, r.btnDown, false, r.hoverBtn == 2, c, alpha, rad);
        }
        dev->popScissorRect();   // 与上方 push 配对（每区域独立裁剪）
    }
}

void RegionSystem::endFrame() {
    // 本帧未续期（frameStamp 停留在旧帧）→ 失活（标签切换整组隐藏 / 窗口隐藏）
    for (auto& r : regions_) {
        if (r.frameStamp != frame_) r.active = false;
        if (!r.active) r.hoverScrollbar = false;
    }
    frame_++;
}

void RegionSystem::removeWindow(int winId) {
    regions_.erase(std::remove_if(regions_.begin(), regions_.end(),
        [winId](const RegionState& r) { return r.winId == winId; }), regions_.end());
}

} // namespace ui