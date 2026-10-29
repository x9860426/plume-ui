// AI生成
// ============================================================
// 沙盘系统实现（pan+zoom viewport + 网格 + 交互）
// 变换：screen = world * zoom + pan（pan = 世界原点屏幕坐标）
// 缩放围绕鼠标：保持鼠标点世界坐标不变 → newPan = m - world * newZoom
// 网格密度自适应：zoom 太密×2 太疏÷2，线对齐 pan
// ============================================================
#include "SandboxSystem.h"
#include "UiContext.h"
#include "WinControls.h"
#include "../Element.h"
#include "../Theme.h"
#include <cmath>
#include <cstdio>
#include <algorithm>

namespace ui {

// 世界单位语义：逻辑像素（与全局 UI 一致）；世界→屏幕 = ×zoom×scale+pan
// （scale 进变换：zoom=1 时沙盘控件与全局标准控件同尺寸同外观）
static float worldScale(float zoom) { return zoom * g_uiCtx.scale; }

// 可视框（有限沙盘）：世界外扩 worldPad 的显示范围——不管缩放多少，
// 视口内能看到的范围恒为 (W+2pad)×(H+2pad)；平移以它为界（暗区最多显示 pad）
static Rect visualFrame(const SandboxState& st) {
    return Rect(Vec2(-st.worldPad, -st.worldPad),
                Vec2(st.worldW + st.worldPad * 2.0f,
                     st.worldH + st.worldPad * 2.0f));
}

// 可视框钳制（有限沙盘）：pan 使可视框边界贴视口为限（世界外最多显示 pad）。
// 不存在"框比视口小"分支——begin 的动态 minZoom 保证可视框恒 ≥ 视口铺满
static void clampPan(SandboxState& st) {
    if (st.worldW <= 0 || st.worldH <= 0) return;
    float ws = worldScale(st.zoom);
    Rect vf = visualFrame(st);
    float wPix = vf.size.x * ws, hPix = vf.size.y * ws;
    // 可视框屏幕 x = 世界(-pad) 映射处 = pan - pad*ws
    float fx0 = st.pan.x - st.worldPad * ws;
    float fy0 = st.pan.y - st.worldPad * ws;
    float vx0 = st.viewRect.pos.x, vx1 = st.viewRect.pos.x + st.viewRect.size.x;
    float vy0 = st.viewRect.pos.y, vy1 = st.viewRect.pos.y + st.viewRect.size.y;
    if (fx0 > vx0) st.pan.x -= fx0 - vx0;                 // 框左界贴视口左为限
    if (fx0 + wPix < vx1) st.pan.x += vx1 - (fx0 + wPix); // 框右界贴视口右为限
    if (fy0 > vy0) st.pan.y -= fy0 - vy0;
    if (fy0 + hPix < vy1) st.pan.y += vy1 - (fy0 + hPix);
}

SandboxState* SandboxSystem::findOrCreate(WindowInstance& win, const char* id) {
    for (auto& sb : items_) {
        if (sb.winId == win.id && sb.id == id) return &sb;
    }
    SandboxState sb;
    sb.winId = win.id;
    sb.id = id;
    items_.push_back(sb);
    return &items_.back();
}

void SandboxSystem::drawGrid(RenderDevice* dev, const SandboxState& st, float alpha) {
    ThemeColors& c = currentColors();
    // 网格世界间距 50 逻辑像素 → 屏幕 = 50×zoom×scale（密度自适应防过密过疏）
    float grid = 50.0f * st.zoom * g_uiCtx.scale;
    while (grid < 20.0f) grid *= 2.0f;
    while (grid > 100.0f) grid *= 0.5f;
    // 线色对比度自适应（系统单源；任何主题可见，业务零判断）：
    // 候选 inputBorder(原视觉) → text → accent，取与背景亮度差 ≥40 的首个；
    // 全不足取差最大者——主题里 inputBorder 与输入框/卡片底色相近（如
    // 输入第2色 ≈ 分组卡片第2色）时自动换色，辅助线不融入背景。
    // 亮色主题线加深一档（浅底深线），暗色主题保持原视觉。
    auto lum = [](const Color& col) {
        return 0.299f * col.r + 0.587f * col.g + 0.114f * col.b;
    };
    float bgL = lum(c.bg1);
    const Color* cands[3] = { &c.inputBorder, &c.text, &c.accent };
    const Color* pick = cands[0];
    float bestDiff = -1.0f;
    for (const Color* cand : cands) {
        float d = std::fabs(lum(*cand) - bgL);
        if (d >= 40.0f) { pick = cand; break; }
        if (d > bestDiff) { bestDiff = d; pick = cand; }
    }
    float lineA = (bgL > 127.5f) ? 75.0f : 60.0f;
    Color gc(pick->r, pick->g, pick->b, lineA / 255.0f * alpha);
    // 绘制范围：无限沙盘=全视口；有限沙盘=世界内（沙盘外暗区无辅助线）
    float x0 = st.viewRect.pos.x, x1 = st.viewRect.pos.x + st.viewRect.size.x;
    float y0 = st.viewRect.pos.y, y1 = st.viewRect.pos.y + st.viewRect.size.y;
    if (st.worldW > 0 && st.worldH > 0) {
        float ws = worldScale(st.zoom);
        if (st.pan.x > x0) x0 = st.pan.x;
        if (st.pan.y > y0) y0 = st.pan.y;
        if (st.pan.x + st.worldW * ws < x1) x1 = st.pan.x + st.worldW * ws;
        if (st.pan.y + st.worldH * ws < y1) y1 = st.pan.y + st.worldH * ws;
        if (x0 >= x1 || y0 >= y1) return;
    }
    float startX = st.pan.x - std::floor((st.pan.x - x0) / grid) * grid;
    for (float x = startX; x <= x1; x += grid)
        drawLine(dev, x, y0, x, y1, 1.0f, gc);
    float startY = st.pan.y - std::floor((st.pan.y - y0) / grid) * grid;
    for (float y = startY; y <= y1; y += grid)
        drawLine(dev, x0, y, x1, y, 1.0f, gc);
}

// 世界边界可视化（系统派发；有限沙盘自动生效，业务零代码）：
//   两层语义：|暗（最多 pad=200）| 长×宽 |暗（最多 pad=200）|
//     世界内 = 提亮底（正常沙盘内容区）/ 世界外 = 暗遮罩（clampPan 保证
//     最多只能看到 pad 的沙盘外）/ 世界边界 = accent 描边。
//   不管缩放多少可视范围恒为 (W+2pad)×(H+2pad)（begin 动态 minZoom 保证
//   可视框恒铺满视口，再小即超框故为缩放下限）。
//   主题自适应（按背景亮度分流，业务零判断）：
//     暗色主题：世界内 bg2@40 提亮 / 界外黑@50 压暗
//     亮色主题：世界内白@55 提亮 / 界外黑@22 轻压（浅底避免脏灰）
void SandboxSystem::drawWorldBounds(RenderDevice* dev, const SandboxState& st, float alpha) {
    if (st.worldW <= 0 || st.worldH <= 0) return;
    ThemeColors& c = currentColors();
    float bgLum = 0.299f * c.bg1.r + 0.587f * c.bg1.g + 0.114f * c.bg1.b;
    bool lightTheme = bgLum > 127.5f;
    // 两层语义：|暗（≤pad）| 长×宽 |暗（≤pad）|
    //   世界内 = 提亮底（正常沙盘内容区）；世界外 = 暗遮罩（clampPan 保证
    //   最多只能看到 pad=200 的沙盘外）；世界边界 = accent 描边
    Color inner = lightTheme ?
        Color(255, 255, 255, 55.0f / 255.0f * alpha) :
        Color(c.bg2.r, c.bg2.g, c.bg2.b, 40.0f / 255.0f * alpha);
    Color dim = lightTheme ?
        Color(0, 0, 0, 22.0f / 255.0f * alpha) :
        Color(0, 0, 0, 50.0f / 255.0f * alpha);
    float ws = worldScale(st.zoom);
    // 世界屏幕矩形
    float wx0 = st.pan.x, wy0 = st.pan.y;
    float wx1 = st.pan.x + st.worldW * ws, wy1 = st.pan.y + st.worldH * ws;
    float vx0 = st.viewRect.pos.x, vx1 = st.viewRect.pos.x + st.viewRect.size.x;
    float vy0 = st.viewRect.pos.y, vy1 = st.viewRect.pos.y + st.viewRect.size.y;
    // 世界与视口交叠区
    float ix0 = wx0 > vx0 ? wx0 : vx0, iy0 = wy0 > vy0 ? wy0 : vy0;
    float ix1 = wx1 < vx1 ? wx1 : vx1;
    float iy1 = wy1 < vy1 ? wy1 : vy1;
    // 1. 世界内提亮底
    if (ix0 < ix1 && iy0 < iy1)
        drawRect(dev, Rect(Vec2(ix0, iy0), Vec2(ix1 - ix0, iy1 - iy0)), inner);
    // 2. 世界外暗遮罩（视口减世界差集 = 沙盘外，最多 200 可见）
    if (iy0 > vy0)
        drawRect(dev, Rect(Vec2(vx0, vy0), Vec2(st.viewRect.size.x, iy0 - vy0)), dim);
    if (iy1 < vy1)
        drawRect(dev, Rect(Vec2(vx0, iy1), Vec2(st.viewRect.size.x, vy1 - iy1)), dim);
    float oy0 = iy0 > vy0 ? iy0 : vy0, oy1 = iy1 < vy1 ? iy1 : vy1;
    if (ix0 > vx0 && oy0 < oy1)
        drawRect(dev, Rect(Vec2(vx0, oy0), Vec2(ix0 - vx0, oy1 - oy0)), dim);
    if (ix1 < vx1 && oy0 < oy1)
        drawRect(dev, Rect(Vec2(ix1, oy0), Vec2(vx1 - ix1, oy1 - oy0)), dim);
    // 3. 世界边界 accent 描边（最醒目一层：沙盘长×宽范围）
    if (ix0 < ix1 && iy0 < iy1) {
        Color bc(c.accent.r, c.accent.g, c.accent.b, 170.0f / 255.0f * alpha);
        Rect wr(Vec2(ix0, iy0), Vec2(ix1 - ix0, iy1 - iy0));
        drawBorder(dev, wr, bc, g_uiCtx.borderWidth());
    }
}

SandboxHandle SandboxSystem::begin(RenderDevice* dev, WindowInstance& win,
                                  const SandboxRequest& req) {
    SandboxHandle h;
    if (!dev || req.viewRect.size.x <= 0 || req.viewRect.size.y <= 0) return h;
    SandboxState* st = findOrCreate(win, req.id);
    st->frameStamp = frame_;
    st->active = true;
    st->viewRect = req.viewRect;
    st->minZoom = req.minZoom;
    st->maxZoom = req.maxZoom;
    st->panEnabled = req.panEnabled;
    st->zoomEnabled = req.zoomEnabled;
    st->showGrid = req.showGrid;
    st->worldW = req.worldW;
    st->worldH = req.worldH;
    st->worldPad = (req.worldPad > 0 ? req.worldPad : 200.0f);
    if (!st->inited) {
        st->zoom = req.initZoom;
        // 初始定位：世界中心对准视口中心（有限）；无限沙盘世界原点在视口中心
        float ws = worldScale(st->zoom);
        st->pan = Vec2(
            req.viewRect.pos.x + req.viewRect.size.x * 0.5f - st->worldW * ws * 0.5f,
            req.viewRect.pos.y + req.viewRect.size.y * 0.5f - st->worldH * ws * 0.5f);
        st->inited = true;
    }
    // 窗口移动同步：视口左上位移 = 窗口拖动量 → pan 同步平移（pan 是屏幕
    // 绝对坐标，不补偿会被 clampPan 强拉回视口，表现为沙盘被"强行推动"）
    if (st->viewInited) {
        st->pan.x += req.viewRect.pos.x - st->lastViewPos.x;
        st->pan.y += req.viewRect.pos.y - st->lastViewPos.y;
    }
    st->lastViewPos = req.viewRect.pos;
    st->viewInited = true;
    // 动态最小缩放：可视框恰好铺满视口的倍率（视口 = 框×zoom×scale）。
    // 语义：不管缩放多少只能显示 (W+2pad)×(H+2pad)——再小视口会超出可视框
    // 出现空白，故以此为下限（例：视口=世界 200×200、pad=200 时 = 200/600=33%）。
    // 窗口 resize 后每帧重算自动生效。
    if (st->worldW > 0 && st->worldH > 0) {
        Rect vf = visualFrame(*st);
        float gs = g_uiCtx.scale;
        float dynMin = st->viewRect.size.x / (vf.size.x * gs);
        float dynMinY = st->viewRect.size.y / (vf.size.y * gs);
        if (dynMinY > dynMin) dynMin = dynMinY;
        if (dynMin > st->minZoom) st->minZoom = dynMin;
        if (st->zoom < st->minZoom) st->zoom = st->minZoom;   // 视口变大后补钳
    }
    clampPan(*st);
    dev->pushScissorRect((int)req.viewRect.pos.x, (int)req.viewRect.pos.y,
                        (int)req.viewRect.size.x, (int)req.viewRect.size.y);
    // 背景层系统派发：有限沙盘边界可视化（内亮/外遮罩/描边）→ 网格辅助线
    drawWorldBounds(dev, *st, 1.0f);
    if (st->showGrid) drawGrid(dev, *st, 1.0f);
    h.valid = true;
    h.winId = win.id;
    h.id = st->id.c_str();
    h.viewRect = req.viewRect;
    h.zoom = st->zoom;
    h.pan = st->pan;
    h.panning = st->panning;
    h.worldW = st->worldW;
    h.worldH = st->worldH;
    h.showZoomHud = req.showZoomHud;
    return h;
}

void SandboxSystem::end(RenderDevice* dev, const SandboxHandle& h, float alpha) {
    if (!h.valid || !dev) return;
    dev->popScissorRect();
    if (!h.showZoomHud) return;
    ThemeColors& c = currentColors();
    float s = g_uiCtx.scale;
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", (int)(h.zoom * 100.0f + 0.5f));
    float bw = 48.0f * s, bh = 20.0f * s;
    Rect hud(Vec2(h.viewRect.pos.x + h.viewRect.size.x - bw - 6.0f * s,
                  h.viewRect.pos.y + h.viewRect.size.y - bh - 6.0f * s), Vec2(bw, bh));
    drawRoundedRect(dev, hud, Color(c.bg2.r, c.bg2.g, c.bg2.b, 200.0f / 255.0f * alpha), 4.0f * s);
    wctl::drawTextCenterAt(dev, g_uiCtx.smallFont, buf, hud,
                          Color(c.text.r, c.text.g, c.text.b, 220.0f / 255.0f * alpha));
}

bool SandboxSystem::handleWheel(WindowInstance& win, const Vec2& m, float delta,
                               bool ctrl, bool shift) {
    if (delta == 0.0f) return false;
    for (auto& st : items_) {
        if (st.winId != win.id || !st.active) continue;
        if (!st.viewRect.contains(m)) continue;
        if (ctrl && st.zoomEnabled) {
            float factor = delta > 0 ? 1.1f : (1.0f / 1.1f);
            float newZoom = st.zoom * factor;
            if (newZoom < st.minZoom) newZoom = st.minZoom;
            if (newZoom > st.maxZoom) newZoom = st.maxZoom;
            // 鼠标锚定：保持鼠标点的世界坐标不变（反变换含 scale）
            Vec2 world((m.x - st.pan.x) / worldScale(st.zoom),
                       (m.y - st.pan.y) / worldScale(st.zoom));
            st.pan.x = m.x - world.x * worldScale(newZoom);
            st.pan.y = m.y - world.y * worldScale(newZoom);
            st.zoom = newZoom;
            clampPan(st);
            return true;
        } else if (st.panEnabled) {
            float step = 40.0f;
            if (shift) st.pan.x += delta * step;
            else st.pan.y -= delta * step;
            clampPan(st);
            return true;
        }
    }
    return false;
}

bool SandboxSystem::handlePress(WindowInstance& win, const Vec2& m, bool panModifier) {
    for (auto& st : items_) {
        if (st.winId != win.id || !st.active) continue;
        if (!st.viewRect.contains(m)) continue;
        if (panModifier && st.panEnabled) {
            st.panning = true;
            st.panDragOff = Vec2(m.x - st.pan.x, m.y - st.pan.y);
            return true;
        }
        return false;
    }
    return false;
}

void SandboxSystem::handleDrag(WindowInstance& win, const Vec2& m) {
    for (auto& st : items_) {
        if (st.winId != win.id || !st.active || !st.panning) continue;
        st.pan = Vec2(m.x - st.panDragOff.x, m.y - st.panDragOff.y);
        clampPan(st);
    }
}

void SandboxSystem::endDrag() {
    for (auto& st : items_) st.panning = false;
}

void SandboxSystem::updateHover(WindowInstance& win, const Vec2& m) {
    for (auto& st : items_) {
        if (st.winId != win.id || !st.active) continue;
        st.hover = st.viewRect.contains(m);
    }
}

bool SandboxSystem::resetView(WindowInstance& win, const char* id) {
    for (auto& st : items_) {
        if (st.winId != win.id || st.id != id) continue;
        st.zoom = 1.0f;
        float ws = worldScale(st.zoom);
        st.pan = Vec2(st.viewRect.pos.x + st.viewRect.size.x * 0.5f - st.worldW * ws * 0.5f,
                      st.viewRect.pos.y + st.viewRect.size.y * 0.5f - st.worldH * ws * 0.5f);
        if (st.zoom < st.minZoom) st.zoom = st.minZoom;
        clampPan(st);
        st.panning = false;
        return true;
    }
    return false;
}

void SandboxSystem::endFrame() {
    for (auto& st : items_) {
        if (st.frameStamp != frame_) {
            st.active = false;
            st.panning = false;
            st.hover = false;
        }
    }
    frame_++;
}

void SandboxSystem::removeWindow(int winId) {
    items_.erase(std::remove_if(items_.begin(), items_.end(),
        [winId](const SandboxState& sb) { return sb.winId == winId; }), items_.end());
}

const SandboxState* SandboxSystem::find(int winId, const char* id) const {
    for (const auto& st : items_) {
        if (st.winId == winId && st.id == id) return &st;
    }
    return nullptr;
}

Vec2 SandboxSystem::worldToScreen(const SandboxHandle& h, const Vec2& world) const {
    return Vec2(world.x * worldScale(h.zoom) + h.pan.x,
                world.y * worldScale(h.zoom) + h.pan.y);
}

float SandboxSystem::worldToScreenSize(const SandboxHandle& h, float worldLen) const {
    return worldLen * worldScale(h.zoom);
}

Rect SandboxSystem::worldToScreenRect(const SandboxHandle& h, const Vec2& world,
                                       const Vec2& worldSize) const {
    Vec2 p = worldToScreen(h, world);
    return Rect(Vec2(p.x - worldSize.x * worldScale(h.zoom) * 0.5f,
                     p.y - worldSize.y * worldScale(h.zoom) * 0.5f),
                Vec2(worldSize.x * worldScale(h.zoom),
                     worldSize.y * worldScale(h.zoom)));
}

float SandboxSystem::nodeFontPx(const SandboxHandle& h, float worldPx) const {
    // 世界逻辑字号 ×zoom（px 语义 = 逻辑字号；drawTextScaled 自动乘 scale）
    return worldPx * h.zoom;
}

void SandboxSystem::drawNode(RenderDevice* dev, const SandboxHandle& h,
                              const SandboxNode& n, float alpha) {
    if (!dev || !h.valid) return;
    // 越界反馈（有限沙盘）：节点中心在世界矩形外 → stderr 警告（每沙盘去重一次）。
    // 注：几何在运行时才确定（世界大小/节点坐标可为变量），编译期无法静态判定；
    // 代码反馈采用"运行时首犯警告"，跑一次即可在控制台看到全部越界点。
    if (h.worldW > 0 && h.worldH > 0 &&
        (n.world.x < 0 || n.world.x > h.worldW ||
         n.world.y < 0 || n.world.y > h.worldH)) {
        for (auto& sb : items_) {
            if (sb.winId != h.winId || sb.id != h.id) continue;
            if (!sb.outWarned) {
                sb.outWarned = true;
                std::fprintf(stderr,
                    "[SandboxSystem] 警告: 沙盘'%s'节点'%s'世界坐标(%.0f,%.0f)"
                    "超出有限世界(%.0fx%.0f)——内容绘制在世界边界外\n",
                    h.id ? h.id : "?", n.label ? n.label : "-",
                    n.world.x, n.world.y, h.worldW, h.worldH);
            }
            break;
        }
    }
    ThemeColors& c = currentColors();
    float s = g_uiCtx.scale;
    Vec2 p = worldToScreen(h, n.world);
    // 配色单源：colorMode → ThemeColors（业务零裸色）
    const Color* base = (n.colorMode == 0) ? &c.accent :
                        (n.colorMode == 1) ? &c.bg3 : &c.btnBg1;
    Color fill(base->r, base->g, base->b, 220.0f / 255.0f * alpha);
    Color txt(c.text.r, c.text.g, c.text.b, 230.0f / 255.0f * alpha);
    Font* f = g_uiCtx.smallFont;
    float corner = 4.0f;

    if (n.mode == 0) {
        // 跟随缩放：块 ×zoom×scale（与固定模式 ×scale 同基准；块小于 2px 保底
        // 不消失；文字低于 lodLow 阈值不画——缩太小文字不可读）
        float sz = worldToScreenSize(h, n.size);
        if (sz < 2.0f) sz = 2.0f;
        Rect r(Vec2(p.x - sz * 0.5f, p.y - sz * 0.5f), Vec2(sz, sz));
        drawRoundedRect(dev, r, fill, corner * h.zoom * s);
        if (n.label && f && h.zoom > n.lodLow) {
            float tw = 64.0f * h.zoom * s;
            drawText(dev, f, n.label,
                     Vec2(p.x - tw * 0.5f, p.y + sz * 0.5f + 2.0f * h.zoom * s),
                     txt, h.zoom);
        }
    } else if (n.mode == 1) {
        // 固定屏幕尺寸：不随 zoom（只有 pan），文字用系统居中排版件
        float sz = n.size * s;
        Rect r(Vec2(p.x - sz * 0.5f, p.y - sz * 0.5f), Vec2(sz, sz));
        drawRoundedRect(dev, r, fill, corner * s);
        if (n.label && f) {
            Rect tr(Vec2(p.x - 50.0f * s, p.y + sz * 0.5f + 2.0f * s),
                    Vec2(100.0f * s, 16.0f * s));
            wctl::drawTextCenterAt(dev, f, n.label, tr, txt);
        }
    } else {
        // LOD 分级：≥lodHigh 完整 / 区间只文字 / <lodLow 只画点
        if (h.zoom >= n.lodHigh) {
            float sz = n.size * s;
            Rect r(Vec2(p.x - sz * 0.5f, p.y - sz * 0.5f), Vec2(sz, sz));
            drawRoundedRect(dev, r, fill, corner * s);
            if (n.label && f) {
                Rect tr(Vec2(p.x - 50.0f * s, p.y + sz * 0.5f + 2.0f * s),
                        Vec2(100.0f * s, 16.0f * s));
                wctl::drawTextCenterAt(dev, f, n.label, tr, txt);
            }
        } else if (h.zoom >= n.lodLow) {
            if (n.label && f) {
                Rect tr(Vec2(p.x - 50.0f * s, p.y - 8.0f * s),
                        Vec2(100.0f * s, 16.0f * s));
                wctl::drawTextCenterAt(dev, f, n.label, tr, txt);
            }
        } else {
            fillCircle(dev, p.x, p.y, 3.0f * s, fill);
        }
    }
}

} // namespace ui