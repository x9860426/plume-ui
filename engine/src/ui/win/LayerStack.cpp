// AI生成
#include "LayerStack.h"
#include "UiContext.h"
#include "HandlerRegistry.h"
#include "RegionSystem.h"
#include "DropdownSystem.h"
#include "SplitterSystem.h"
#include "SandboxSystem.h"
#include "WindowButtons.h"
#include "WinControls.h"
#include "../Element.h"
#include "../Theme.h"
#include "../../core/Assets.h"
#include <algorithm>
#include <cmath>
#include <nlohmann/json.hpp>

namespace ui {

// 淡入淡出速度（每秒透明度变化量；对齐参考实现 0.08/帧@60fps ≈ 4.8/s）
static constexpr float FADE_SPEED = 5.0f;
// 新窗口级联偏移步长（像素，避免多窗完全重叠）
static constexpr float CASCADE_STEP = 24.0f;

// 旧全局单例 g_layerStack 已删除：多应用架构后每应用独立
// （AppFwd.h 宏转发到 AppManager 当前应用的 layerStack 成员）

// ============================================================
// 叠层阴影（1:1 对照参考 ui_core.cpp DrawShadow L429-435）：
//   for i = blur..1：画一层右下偏移 i 的圆角矩形，
//   透明度 = alpha255 × (blur-i+1)/blur/2（越内层越浓）。
// 与 SDF 模糊晕圈阴影完全不同——参考是"多层渐深叠层"质感。
// ============================================================
static void drawLayeredShadow(RenderDevice* dev, const Rect& r, float radius,
                              float blurAdd, float alphaAdd, float winAlpha) {
    int blur = (int)(g_uiCtx.shadowBlur() + blurAdd);
    if (blur < 1) blur = 1;
    float a255 = g_uiCtx.shadowAlpha() * 255.0f + alphaAdd;  // 参考 alpha 单位 0-255
    for (int i = blur; i >= 1; i--) {
        float al = a255 * (float)(blur - i + 1) / (float)blur / 2.0f / 255.0f;
        al *= winAlpha;
        if (al <= 0.0f) continue;
        Rect layer(Vec2(r.pos.x + i, r.pos.y + i), r.size);
        drawRoundedRect(dev, layer, Color(0, 0, 0, al), radius);
    }
}

// ============================================================
// 构造：四个预定义层级（层序 = LayerId 数值序）
// ============================================================
LayerStack::LayerStack() {
    layers_.resize(LAYER_COUNT);
    layers_[LAYER_SCENE].id  = LAYER_SCENE;  layers_[LAYER_SCENE].name  = "SCENE";
    layers_[LAYER_PLAYER].id = LAYER_PLAYER; layers_[LAYER_PLAYER].name = "PLAYER";
    layers_[LAYER_DEV].id    = LAYER_DEV;    layers_[LAYER_DEV].name    = "DEV";
    layers_[LAYER_GLOBAL].id = LAYER_GLOBAL; layers_[LAYER_GLOBAL].name = "GLOBAL";
}

// ============================================================
// 层序（绘制从底到顶）：setupLayers 可自定义（嵌入宿主按需裁剪/
// 重排层级），默认 0..LAYER_COUNT-1。renderAll 正序绘制、命中
// 类接口逆序查询——全部经 g_layerOrder，改一处全局生效
// ============================================================
static std::vector<int> g_layerOrder = { LAYER_SCENE, LAYER_PLAYER, LAYER_DEV, LAYER_GLOBAL };

void LayerStack::setupLayers(const int* layerIds, int count) {
    if (!layerIds || count <= 0) {   // 空参数 = 恢复默认序
        g_layerOrder = { LAYER_SCENE, LAYER_PLAYER, LAYER_DEV, LAYER_GLOBAL };
        return;
    }
    std::vector<int> order;
    for (int i = 0; i < count; i++) {
        int id = layerIds[i];
        if (id < 0 || id >= LAYER_COUNT) continue;     // 越界项忽略
        bool dup = false;
        for (int v : order) if (v == id) { dup = true; break; }
        if (!dup) order.push_back(id);                 // 重复项忽略
    }
    if (!order.empty()) g_layerOrder = order;
}

int LayerStack::layerCount() {
    return (int)g_layerOrder.size();
}

Layer& LayerStack::layer(int layerId) {
    return layers_[layerId >= 0 && layerId < (int)layers_.size() ? layerId : LAYER_DEV];
}

const Layer& LayerStack::layer(int layerId) const {
    return layers_[layerId >= 0 && layerId < (int)layers_.size() ? layerId : LAYER_DEV];
}

// ============================================================
// 初始位置尺寸（配置驱动；countInLayer = 层内窗口数，用于级联偏移）
// ============================================================
static void placeWindow(WindowInstance& win, const WindowConfig* cfg, size_t countInLayer) {
    float scale = g_uiCtx.scale;
    if (hasFlag(cfg->flags, WIN_FULLSCREEN)) {
        win.x = 0; win.y = 0;
        win.w = (float)g_uiCtx.winW;
        win.h = (float)g_uiCtx.winH;
    } else {
        win.w = cfg->defaultW * scale;
        win.h = cfg->defaultH * scale;
        // 层内已有窗口越多偏移越大（级联），并 clamp 到屏幕内
        float off = (float)(countInLayer % 8) * CASCADE_STEP * scale;
        win.x = 120.0f * scale + off;
        win.y = 80.0f * scale + off;
        win.x = std::min(win.x, std::max(0.0f, (float)g_uiCtx.winW - win.w));
        win.y = std::min(win.y, std::max(0.0f, (float)g_uiCtx.winH - win.h));
    }
}

// ============================================================
// 窗口生命周期
// ============================================================
int LayerStack::createWindowByIndex(int configIndex, int layerId) {
    const WindowConfig* cfg = FindWindowConfigByIndex(configIndex);
    if (!cfg) return -1;

    // 单例语义（规范：默认单例，WIN_MULTI_INSTANCE 才允许多实例）
    if (!hasFlag(cfg->flags, WIN_MULTI_INSTANCE)) {
        // 查重含「淡出中」（disposed 未物理删除）的实例：
        // 参考 g_floatWin 是全局单例，再次打开 = 复活同一实例
        //（window_sync.cpp L129-139 + ui_input.cpp L1960-1966 单例复用语义）
        WindowInstance* exist = nullptr;
        size_t existCount = 0;
        for (auto& L : layers_) {
            for (auto& w : L.windows) {
                if (w->config() && strcmp(w->config()->id, cfg->id) == 0) {
                    exist = w.get();
                    existCount = L.windows.size() - 1;  // 级联计数排除自己
                }
            }
        }
        if (exist) {
            // 重新显示（完全对齐参考打开路径 ui_input.cpp L1960-1966）：
            // visible=true + 取消废弃 + alpha=0 重新淡入 + tabSel 归零 + 置顶
            exist->visible = true;
            exist->disposed = false;
            exist->fadingOut = false;
            exist->alpha = 0.0f;
            exist->targetAlpha = 1.0f;
            exist->animScale = 0.85f;    // 复活同样弹出动画
            exist->animTarget = 1.0f;
            exist->tabSel = 0;
            exist->ball.active = false;   // 球态窗口复活为面板
            placeWindow(*exist, cfg, existCount);
            bringToFront(exist->id);
            return exist->id;
        }
    }

    // 归属层：显式指定优先，否则读配置表
    if (layerId < 0) layerId = cfg->layer;
    Layer& L = layer(layerId);

    auto win = std::make_unique<WindowInstance>();
    win->id = nextId_++;
    win->configIndex = configIndex;
    win->alpha = 0.0f;                 // 从 0 淡入
    win->targetAlpha = 1.0f;
    win->animScale = 0.85f;            // 弹出动画起点（updateFade 推进到 1）

    placeWindow(*win, cfg, L.windows.size());

    // 悬浮球初始标签 = 窗口标题（动态标题优先，如"调试 #3"）
    if (cfg->dynamicTitle) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%s #%d", cfg->title, win->id);
        win->titleOverride = buf;
    }
    win->ball.label = win->titleText();

    int id = win->id;
    L.windows.push_back(std::move(win));  // 入栈 = 层内栈顶
    return id;
}

int LayerStack::createWindow(const char* configId, int layerId) {
    return createWindowByIndex(FindWindowConfigIndex(configId), layerId);
}

void LayerStack::rescale(float oldScale, float newScale,
                         float oldW, float oldH, float newW, float newH) {
    if (oldScale <= 0.0f || newScale <= 0.0f) return;
    float k  = newScale / oldScale;                      // 尺寸缩放比
    float kx = oldW > 0.5f ? newW / oldW : 1.0f;         // 水平位置映射比
    float ky = oldH > 0.5f ? newH / oldH : 1.0f;
    for (auto& L : layers_) {
        for (auto& w : L.windows) {
            if (w->disposed) continue;
            const WindowConfig* cfg = w->config();
            if (cfg && hasFlag(cfg->flags, WIN_FULLSCREEN)) {
                w->x = 0; w->y = 0;
                w->w = newW; w->h = newH;
                continue;
            }
            // 尺寸：按 scale 比例缩放，clamp 到配置最小尺寸
            w->w *= k;
            w->h *= k;
            if (cfg) {
                w->w = std::max(w->w, cfg->minW * newScale);
                w->h = std::max(w->h, cfg->minH * newScale);
            }
            // 尺寸上限：不可超过屏幕（防异常几何累积放大后关闭按钮跑到屏外）
            w->w = std::min(w->w, newW);
            w->h = std::min(w->h, newH);
            // 位置：按主窗口新旧尺寸比例映射（保持相对位置）
            w->x *= kx;
            w->y *= ky;
            // clamp：窗口至少 40px 留在新窗口内（四方向）
            float margin = 40.0f * newScale;
            if (w->x > newW - margin) w->x = std::max(0.0f, newW - margin);
            if (w->y > newH - margin) w->y = std::max(0.0f, newH - margin);
            if (w->x + w->w < margin) w->x = 0.0f;
            if (w->y + w->h < margin) w->y = 0.0f;
            // 悬浮球：位置按比例映射，半径跟随新 scale
            w->ball.x *= kx;
            w->ball.y *= ky;
            w->ball.r = 24.0f * newScale;
            w->ball.x = std::min(std::max(w->ball.x, w->ball.r), newW - w->ball.r);
            w->ball.y = std::min(std::max(w->ball.y, w->ball.r), newH - w->ball.r);
        }
    }
}

int LayerStack::createPlayerWindow(const char* configId, int sceneCtx) {
    // 玩家级入口（Lua 插件悬浮窗）：强制 LAYER_PLAYER，忽略配置表层级——
    // 玩家代码的窗口永远压不过开发级/警告级（层级隔离是安全边界）
    int idx = FindWindowConfigIndex(configId);
    if (idx < 0) return -1;
    int id = createWindowByIndex(idx, LAYER_PLAYER);
    // 单例复显路径同样要刷新场景绑定（窗口可能来自旧场景）
    if (WindowInstance* w = findWindow(id)) w->sceneContext = sceneCtx;
    return id;
}

void LayerStack::setSceneContext(int sceneCtx) {
    // 场景切换（显示/隐藏语义，非销毁）：
    //   旧场景玩家窗直接消失（visible=false；实例/位置/滚动状态全保留）
    //   切回原场景整组重新显示（visible=true，无重开动画）
    //   开发级/警告级常驻（开发工具不随场景消失）
    sceneContext_ = sceneCtx;
    for (auto& w : layer(LAYER_PLAYER).windows) {
        if (w->disposed) continue;
        w->visible = (w->sceneContext == sceneCtx);
    }
}

void LayerStack::closeWindow(int windowId) {
    if (WindowInstance* w = findWindow(windowId)) {
        if (!w->disposed) {
            w->fadingOut = true;
            w->targetAlpha = 0.0f;
            w->animTarget = 0.85f;     // 收缩动画（与淡出同步）
            w->disposed = true;  // 仅标记，帧末 flushDisposed 物理删除
        }
    }
}

void LayerStack::bringToFront(int windowId) {
    for (auto& L : layers_) {
        for (size_t i = 0; i < L.windows.size(); i++) {
            if (L.windows[i]->id == windowId) {
                // 移动 unique_ptr 到层尾（物理序=栈序，排序已废除）
                std::unique_ptr<WindowInstance> w = std::move(L.windows[i]);
                L.windows.erase(L.windows.begin() + i);
                L.windows.push_back(std::move(w));
                return;
            }
        }
    }
}

void LayerStack::destroyContext(int contextId) {
    for (auto& L : layers_)
        for (auto& w : L.windows)
            if (w->config() && w->config()->contextId == contextId && !w->disposed) {
                w->fadingOut = true;
                w->targetAlpha = 0.0f;
                w->animTarget = 0.85f;
                w->disposed = true;
            }
}

void LayerStack::closeAll() {
    for (auto& L : layers_)
        for (auto& w : L.windows)
            if (!w->disposed) {
                w->fadingOut = true;
                w->targetAlpha = 0.0f;
                w->animTarget = 0.85f;
                w->disposed = true;
            }
}

void LayerStack::setLayerVisible(int layerId, bool visible) {
    layer(layerId).visible = visible;
}

// ============================================================
// 待打开队列（规范 §9.5：帧末统一创建，杜绝遍历中途失效）
// ============================================================
void LayerStack::pendingOpen(int configIndex, int layerId) {
    if (configIndex < 0) return;
    pending_.push_back({configIndex, layerId});
}

void LayerStack::openPending() {
    if (pending_.empty()) return;
    std::vector<PendingOpen> queue;
    queue.swap(pending_);
    for (auto& p : queue) createWindowByIndex(p.configIndex, p.layerId);
}

// ============================================================
// 查询
// ============================================================
WindowInstance* LayerStack::findWindow(int windowId) {
    for (auto& L : layers_)
        for (auto& w : L.windows)
            if (w->id == windowId && !w->disposed) return w.get();
    return nullptr;
}

WindowInstance* LayerStack::findWindowByConfig(const char* configId) {
    for (auto& L : layers_)
        for (auto& w : L.windows)
            if (!w->disposed && w->config() && strcmp(w->config()->id, configId) == 0)
                return w.get();
    return nullptr;
}

WindowInstance* LayerStack::topWindow() {
    // 层序从高到低（g_layerOrder 逆序）、层内栈顶(尾)→栈底(头)，第一个可交互窗口即全局栈顶
    for (int oi = (int)g_layerOrder.size() - 1; oi >= 0; oi--) {
        auto& wins = layers_[g_layerOrder[oi]].windows;
        for (int wi = (int)wins.size() - 1; wi >= 0; wi--) {
            WindowInstance* w = wins[wi].get();
            if (w->disposed || !w->visible || w->alpha < 0.001f) continue;
            if (w->clickThrough()) continue;
            return w;
        }
    }
    return nullptr;
}

WindowInstance* LayerStack::topOverlayBarrier() {
    // 遮罩屏障语义（规范 §2.7）：只看 overlayAlpha > 0，与 WIN_MODAL 标志无关；
    // 隐藏窗口（场景切走的玩家窗）不构成屏障
    for (int oi = (int)g_layerOrder.size() - 1; oi >= 0; oi--) {
        auto& wins = layers_[g_layerOrder[oi]].windows;
        for (int wi = (int)wins.size() - 1; wi >= 0; wi--) {
            WindowInstance* w = wins[wi].get();
            if (w->disposed || !w->visible || w->alpha < 0.001f) continue;
            if (w->hasOverlay()) return w;
        }
    }
    return nullptr;
}

std::vector<WindowInstance*> LayerStack::allWindows() {
    std::vector<WindowInstance*> out;
    for (auto& L : layers_)
        for (auto& w : L.windows)
            if (!w->disposed) out.push_back(w.get());
    return out;
}

// ============================================================
// 交互命中
// ============================================================
WindowInstance* LayerStack::hitTestBall(const Vec2& m) {
    // 悬浮球最高优先：任何窗口的球命中都先于面板命中
    for (auto& L : layers_) {
        if (!L.visible) continue;
        for (auto& w : L.windows) {
            if (w->disposed || !w->visible || !w->ball.active || w->alpha < 0.001f) continue;
            float dx = m.x - w->ball.x, dy = m.y - w->ball.y;
            if (dx * dx + dy * dy <= w->ball.r * w->ball.r) return w.get();
            // 方形/特别绘制风格：外接圆未中时再按 2r×2r 方形判定（圆角区域已含在方形内）
            if (g_uiCtx.ballStyle >= 1) {
                if (std::abs(dx) <= w->ball.r && std::abs(dy) <= w->ball.r)
                    return w.get();
            }
        }
    }
    return nullptr;
}

WindowInstance* LayerStack::hitTest(const Vec2& m) {
    // 层序从高到低（g_layerOrder 逆序）、层内栈顶→栈底：首个命中面板的可交互窗唯一消费点击
    for (int oi = (int)g_layerOrder.size() - 1; oi >= 0; oi--) {
        const int li = g_layerOrder[oi];
        if (!layers_[li].visible) continue;
        auto& wins = layers_[li].windows;
        for (int wi = (int)wins.size() - 1; wi >= 0; wi--) {
            WindowInstance* w = wins[wi].get();
            if (w->disposed || !w->visible || w->alpha < 0.001f) continue;
            if (w->clickThrough()) continue;
            if (w->ball.active) continue;  // 球态窗口：本体不参与命中，球单独命中
            if (w->contains(m, wbtn::titleBarHeight(*w))) return w;
        }
    }
    return nullptr;
}

// ============================================================
// 渲染
// ============================================================

void LayerStack::renderAll(RenderDevice* dev) {
    // 层序正序绘制（g_layerOrder 从底到顶；默认序 = 原数组序，行为不变）
    for (int li : g_layerOrder) {
        auto& L = layers_[li];
        if (!L.visible) continue;
        // 第一趟：非置顶窗口；第二趟：置顶窗口（置顶 = 最后绘制）
        for (int pass = 0; pass < 2; pass++) {
            for (auto& w : L.windows) {
                if (!w->visible || w->disposed || w->alpha < 0.001f) continue;
                if (w->ball.active) continue;  // 球态：不画面板，球由 renderBalls 画
                if ((pass == 0) == w->isPinned()) continue;
                // 窗口级 AA：全局悬浮窗开关 && 实例未单独禁用
                // （玩家接入/三方引擎投屏可按窗口保留原始画质硬边渲染）
                dev->setAAEnabled(g_uiCtx.sdfAA && g_uiCtx.floatWinAA && !w->aaDisabled);
                renderWindow(dev, *w, w->alpha * L.alpha);
            }
        }
    }
    dev->setAAEnabled(g_uiCtx.sdfAA);   // 恢复全局（不泄漏到后续绘制）
}

void LayerStack::renderWindow(RenderDevice* dev, WindowInstance& win, float alpha) {
    const WindowConfig* cfg = win.config();
    if (!dev || !cfg) return;

    ThemeColors& c = currentColors();
    float scale = g_uiCtx.scale;
    float a = alpha;
    float titleH = wbtn::titleBarHeight(win);

    // ---- 0. 开关动画几何缩放（窗口中心 0.85↔1.0，仅渲染期临时改写，画完恢复） ----
    // 所有下游（panel/标题栏/handler 布局/裁剪）均基于 win.x/y/w/h 计算 → 整体一致缩放
    const float animK = win.animScale;
    const float oX = win.x, oY = win.y, oW = win.w, oH = win.h;
    if (animK < 0.999f) {
        float ccx = oX + oW * 0.5f, ccy = oY + oH * 0.5f;
        win.x = ccx - oW * 0.5f * animK;
        win.y = ccy - oH * 0.5f * animK;
        win.w = oW * animK;
        win.h = oH * animK;
    }
    Rect panel = win.panelRect(titleH);

    // ---- 1. 全屏遮罩（若配置） ----
    // 参考 isTop 语义（ui_floatwin.cpp L2092）：仅最顶层带遮罩的窗口画遮罩，
    // 多个遮罩窗同开时不叠加（叠加会导致半透明黑过深）。
    if (cfg->overlayAlpha > 0.5f && topOverlayBarrier() == &win) {
        float ov = cfg->overlayAlpha / 255.0f * a;
        drawRect(dev, {Vec2(0, 0), Vec2((float)g_uiCtx.winW, (float)g_uiCtx.winH)},
                 Color(0, 0, 0, ov));
    }

    // ---- 2. 系统 chrome：阴影 + 四角独立圆角底板 + 标题栏 ----
    // （WIN_TRANSPARENT 窗口跳过全部 chrome，内容全权交给 handler）
    if (!win.isTransparent() && win.isFloating()) {
        // 圆角 = 配置表四角 × 系统缩放 × 厚度派生圆角系数（单源计算）
        float rs = g_uiCtx.radiusScale();
        float tl = cfg->cornerRadii.topLeft     * scale * rs;
        float tr = cfg->cornerRadii.topRight    * scale * rs;
        float bl = cfg->cornerRadii.bottomLeft  * scale * rs;
        float br = cfg->cornerRadii.bottomRight * scale * rs;
        float maxR = std::max(std::max(tl, tr), std::max(bl, br));

        // 叠层阴影（参考 DrawShadow：多层右下偏移渐深；加值配置驱动）
        drawLayeredShadow(dev, panel, maxR,
                          cfg->shadowBlurAdd, cfg->shadowAlphaAdd, a);

        // 窗口底板：四角独立圆角（规范 §4，系统统一绘制，扩展代码不画）
        // 透明度配置驱动（参考：标准235 / 设置230 / 调试240）
        drawCornerRadiiRect(dev, panel,
                            Color(c.groupTitleBg.r, c.groupTitleBg.g, c.groupTitleBg.b,
                                  cfg->panelAlpha / 255.0f * a),
                            tl, tr, bl, br);

        // 标题栏：顶部两角继承窗口圆角，底部直角（与内容区无缝衔接）
        // 底透明度配置驱动（参考：标准240 / 调试245）
        if (win.hasTitleBar()) {
            Rect tb(panel.pos, Vec2(panel.size.x, titleH));
            drawCornerRadiiRect(dev, tb,
                                Color(c.tabSelBg.r, c.tabSelBg.g, c.tabSelBg.b,
                                      cfg->titleBarAlpha / 255.0f * a),
                                tl, tr, 0, 0);
            // 标题文本（参考 14px Bold tabSelText 左 12px；调试窗 13px——字号配置驱动）
            wctl::drawTextScaled(dev, g_uiCtx.font, win.titleText(),
                                 Rect(Vec2(panel.pos.x + 12.0f * scale, panel.pos.y),
                                      Vec2(panel.size.x - 60.0f * scale, titleH)),
                                 Color(c.tabSelText.r, c.tabSelText.g, c.tabSelText.b, a),
                                 cfg->titleFontPx, 0);
        }

        // 外框描边（参考 ui_floatwin.cpp L1014-1015：accent + BorderWidth()，
        // 画在标题栏之后覆盖全窗轮廓，统一半径取四角最大值；
        // 透明度配置驱动（参考：标准100 / 设置60），0 = 不画）
        if (cfg->borderAlpha > 0.5f) {
            drawRoundedBorder(dev, panel,
                              Color(c.accent.r, c.accent.g, c.accent.b,
                                    cfg->borderAlpha / 255.0f * a),
                              g_uiCtx.borderWidth(), maxR);
        }
    }

    // ---- 3. 内容回调（注册表分发，配置驱动，零 if/switch） ----
    // 窗口级裁剪：handler 内容限制在面板矩形内（参考项目悬浮窗为独立
    // HWND 被系统天然裁剪；此处用 scissor 等价实现，折叠时面板只剩
    // 标题栏高度，内容自动隐藏）。透明窗内容全权交给 handler，不裁。
    const WinHandler* h = g_handlers.find(cfg->handlerName);
    bool clipContent = !win.isTransparent();
    if (clipContent) {
        int px = (int)std::floor(panel.pos.x);
        int py = (int)std::floor(panel.pos.y);
        int pw = (int)std::ceil(panel.pos.x + panel.size.x) - px;
        int ph = (int)std::ceil(panel.pos.y + panel.size.y) - py;
        dev->pushScissorRect(px, py, pw, ph);
    }
    if (h && h->render) h->render(dev, win, a);

    // ---- 4. 滚动条（区域系统统一派发，扩展代码不参与） ----
    g_regions.renderScrollbars(dev, win, a);

    // ---- 5. 标题栏按钮（统一按钮系统，flags 控制） ----
    wbtn::drawTitleBarButtons(dev, win, a);

    // ---- 6. 右下角缩放手柄（可缩放且未折叠且未固定大小） ----
    wbtn::drawResizeGrip(dev, win, a);

    if (clipContent) dev->popScissorRect();

    // 开关动画：恢复原始几何（渲染期临时缩放不落地）
    if (animK < 0.999f) {
        win.x = oX; win.y = oY; win.w = oW; win.h = oH;
    }
}

void LayerStack::renderBall(RenderDevice* dev, WindowInstance& win) {
    // 悬浮球（1:1 对照参考 ui_floatwin.cpp DrawFloatBallGeneric L2030-2055）：
    // 黑@50 偏移(+2,+4)椭圆阴影 → accent 球体 → accent2@200 上半高光椭圆
    // → accent 1.5f 描边 → 白 9px 标签文字居中（无标签画白点）
    // 半隐藏态球独立透明度 ball.alpha 参与（贴边只露一半时 0.5）
    ThemeColors& c = currentColors();
    float a = win.alpha * win.ball.alpha;
    float r = win.ball.r;
    float cx = win.ball.x, cy = win.ball.y;
    // 标签绘制矩形（圆/方通用：外接球体）
    Rect labelRect(Vec2(cx - r, cy - r), Vec2(r * 2, r * 2));

    if (g_uiCtx.ballStyle > 1) {
        // ---- 「特别绘制」族（内部 2..11 = 游戏ID 3..12）：委托 wctl::drawBallStyled 统一绘制 ----
        wctl::drawBallStyled(dev, cx, cy, r, a, g_uiCtx.ballStyle);
    } else if (g_uiCtx.ballStyle == 1) {
        // ---- 方形风格：圆角方形球，圆角跟随系统圆润度（radiusScale 0=直角 ~ 2=近圆） ----
        float rad = r * 0.45f * g_uiCtx.radiusScale();
        if (rad < 0.0f) rad = 0.0f;
        if (rad > r) rad = r;
        Rect br(Vec2(cx - r, cy - r), Vec2(r * 2, r * 2));
        Rect sh(Vec2(cx - r + 2.0f, cy - r + 4.0f), Vec2(r * 2, r * 2));
        // 阴影：黑色 50×a 偏移 (+2, +4)（同圆形版）
        drawRoundedRect(dev, sh, Color(0, 0, 0, 50.0f / 255.0f * a), rad);
        // 球体：accent@255 圆角方形
        drawRoundedRect(dev, br, Color(c.accent.r, c.accent.g, c.accent.b, a), rad);
        // 上半高光：accent2@200（顶部对齐的圆角条，对应圆形版上半椭圆 1.3r）
        Rect hl(Vec2(cx - r, cy - r), Vec2(r * 2, 1.3f * r));
        drawRoundedRect(dev, hl,
                        Color(c.accent2.r, c.accent2.g, c.accent2.b,
                              200.0f / 255.0f * a), rad);
        // 边框：accent@255 1.5f 描边
        drawRoundedBorder(dev, br, Color(c.accent.r, c.accent.g, c.accent.b, a),
                          1.5f, rad);
    } else {
        // ---- 默认圆形 ----
        // 阴影：黑色 50×a，椭圆偏移 (+2, +4)
        fillEllipse(dev, cx + 2.0f, cy + 4.0f, r, r, Color(0, 0, 0, 50.0f / 255.0f * a));
        // 球体：accent@255 实心圆
        fillCircle(dev, cx, cy, r, Color(c.accent.r, c.accent.g, c.accent.b, a));
        // 上半高光：accent2@200 椭圆（参考 FillEllipse(cx-r, cy-0.7r, 2r, 1.3r)）
        fillEllipse(dev, cx, cy - 0.05f * r, r, 0.65f * r,
                    Color(c.accent2.r, c.accent2.g, c.accent2.b, 200.0f / 255.0f * a));
        // 边框：accent@255 1.5f 描边
        drawCircle(dev, cx, cy, r, 1.5f, Color(c.accent.r, c.accent.g, c.accent.b, a));
    }
    // 标签：白 9px Bold 居中（标题单源 titleText()）
    if (g_uiCtx.font && g_uiCtx.font->isReady() && !win.ball.label.empty()) {
        wctl::drawTextScaled(dev, g_uiCtx.font, win.ball.label.c_str(),
                             labelRect, Color(1, 1, 1, a), 9.0f, 1);
    } else {
        fillCircle(dev, cx, cy, r * 0.3f, Color(1, 1, 1, a));
    }
}

void LayerStack::renderBalls(RenderDevice* dev) {
    // 悬浮球位于一切之上（规范 §7.2）：renderAll 之后调用
    // 悬浮球属于悬浮窗系统：跟随全局悬浮窗 AA 开关（球无实例覆盖语义，统一处理）
    dev->setAAEnabled(g_uiCtx.sdfAA && g_uiCtx.floatWinAA);
    for (auto& L : layers_) {
        if (!L.visible) continue;
        for (auto& w : L.windows) {
            if (w->disposed || !w->visible || !w->ball.active || w->alpha < 0.001f) continue;
            renderBall(dev, *w);
        }
    }
    dev->setAAEnabled(g_uiCtx.sdfAA);   // 恢复全局
}

// ============================================================
// 帧推进
// ============================================================
void LayerStack::updateFade(float dt) {
    // 缩放动画速度：量程 0.15 / 0.2s（与 alpha 淡入淡出 FADE_SPEED=5 同步）
    constexpr float ANIM_SCALE_SPEED = 0.75f;
    for (auto& L : layers_)
        for (auto& w : L.windows) {
            if (w->alpha < w->targetAlpha) {
                w->alpha += FADE_SPEED * dt;
                if (w->alpha > w->targetAlpha) w->alpha = w->targetAlpha;
            } else if (w->alpha > w->targetAlpha) {
                w->alpha -= FADE_SPEED * dt;
                if (w->alpha < w->targetAlpha) w->alpha = w->targetAlpha;
            }
            // 开关缩放动画（0.85 ↔ 1.0，创建弹出/关闭收缩）
            if (w->animScale < w->animTarget) {
                w->animScale += ANIM_SCALE_SPEED * dt;
                if (w->animScale > w->animTarget) w->animScale = w->animTarget;
            } else if (w->animScale > w->animTarget) {
                w->animScale -= ANIM_SCALE_SPEED * dt;
                if (w->animScale < w->animTarget) w->animScale = w->animTarget;
            }
            // ---- 悬浮球闲置半隐藏（对照参考 ui_input.cpp UpdateFloatBallIdle L235-256） ----
            // 球停在屏幕边缘区（5%）静止 3 秒 → 半隐藏：球心完全推出屏外
            // 只露一半、透明度降半；一旦离开边缘区重新计时
            FloatBallState& b = w->ball;
            if (!b.active || b.dragging || b.hidden) continue;
            float W = (float)g_uiCtx.winW, H = (float)g_uiCtx.winH;
            bool atEdge = b.x < W * 0.05f || b.x > W * 0.95f ||
                          b.y < H * 0.05f || b.y > H * 0.95f;
            if (!atEdge) { b.idleTime = 0; continue; }
            b.idleTime += dt;
            if (b.idleTime >= 3.0f) {
                b.hidden = true;
                b.alpha = 0.5f;
                if (b.x < W * 0.05f) b.x = 0;
                else if (b.x > W * 0.95f) b.x = W;
                if (b.y < H * 0.05f) b.y = 0;
                else if (b.y > H * 0.95f) b.y = H;
            }
        }
}

void LayerStack::flushDisposed() {
    // 帧末安全点统一物理删除（规范 §四.4：从不在遍历/回调中途删除）
    for (auto& L : layers_) {
        for (size_t i = 0; i < L.windows.size();) {
            WindowInstance* w = L.windows[i].get();
            if (w->disposed && w->alpha < 0.001f) {
                g_regions.removeWindow(w->id);  // 同步清理区域系统滚动状态
                g_dropdowns.removeWindow(w->id); // 同步清理下拉系统展开/滚动状态
                g_splitters.removeWindow(w->id); // 同步清理分割器系统拖拽状态
                g_sandboxes.removeWindow(w->id); // 同步清理沙盘系统 zoom/pan 状态
                L.windows.erase(L.windows.begin() + i);
            } else {
                i++;
            }
        }
    }
}

// ============================================================
// 布局记忆（layout.json：窗口位置/尺寸/置顶/固定/折叠/球态跨重启保留）
// ============================================================
bool LayerStack::saveLayout(const char* path) {
    nlohmann::json wins = nlohmann::json::array();
    for (WindowInstance* w : allWindows()) {
        const WindowConfig* cfg = w->config();
        if (!cfg) continue;
        // 模态/警告窗不入档：重启弹回模态体验差，只记忆工具窗
        if (w->isModal() || cfg->layer == LAYER_GLOBAL) continue;
        nlohmann::json j;
        // 落盘前 clamp 到屏幕内（防异常几何写入档案：恢复后窗口超出屏幕、
        // 关闭按钮跑到屏外点不到——远程桌面分辨率切换可能产生超屏几何）
        float W = (float)g_uiCtx.winW, H = (float)g_uiCtx.winH;
        float sw = std::min(w->w, W), sh = std::min(w->h, H);
        float sx = std::min(std::max(w->x, 0.0f), W - 40.0f);
        float sy = std::min(std::max(w->y, 0.0f), H - 40.0f);
        j["id"] = cfg->id;
        j["x"] = sx;  j["y"] = sy;
        j["w"] = sw;  j["h"] = sh;
        j["topmost"] = w->topmost;
        j["fixedSize"] = w->fixedSize;
        j["collapsed"] = w->collapsed;
        j["ball"] = w->ball.active;
        if (w->ball.active) { j["bx"] = w->ball.x; j["by"] = w->ball.y; }
        wins.push_back(j);
    }
    nlohmann::json root;
    root["winW"] = g_uiCtx.winW;   // 保存时屏幕尺寸（恢复时映射到当前屏幕）
    root["winH"] = g_uiCtx.winH;
    root["windows"] = wins;
    return writeFileUtf8(path, root.dump(2));   // UTF-8 IO 单源
}

int LayerStack::loadLayout(const char* path, float curW, float curH) {
    std::string text;
    if (!readFileUtf8(path, text)) return -1;   // UTF-8 IO 单源
    nlohmann::json root;
    try { root = nlohmann::json::parse(text); } catch (...) { return -1; }
    if (!root.contains("windows") || !root["windows"].is_array()) return -1;
    float savedW = root.value("winW", curW);
    float savedH = root.value("winH", curH);
    float scale = g_uiCtx.scale;
    int restored = 0;
    for (auto& j : root["windows"]) {
        std::string id = j.value("id", "");
        int idx = FindWindowConfigIndex(id.c_str());
        const WindowConfig* cfg = FindWindowConfigByIndex(idx);
        if (!cfg) continue;  // 配置表已改名/删除的条目跳过
        int wid = createWindowByIndex(idx);
        WindowInstance* w = findWindow(wid);
        if (!w) continue;
        // 直接写几何（跳过级联偏移），clamp 到配置最小尺寸；
        // 上限 clamp 到当前屏幕（异常大几何落盘后恢复不至于关不掉）
        w->w = std::max(j.value("w", cfg->defaultW * scale), cfg->minW * scale);
        w->h = std::max(j.value("h", cfg->defaultH * scale), cfg->minH * scale);
        w->w = std::min(w->w, curW);
        w->h = std::min(w->h, curH);
        // 位置：确保标题栏可见（至少 40px 在屏幕内，完全屏外拉回左上）
        w->x = j.value("x", 120.0f * scale);
        w->y = j.value("y", 80.0f * scale);
        float margin = 40.0f * scale;
        if (w->x > curW - margin) w->x = std::max(0.0f, curW - margin);
        if (w->y > curH - margin) w->y = std::max(0.0f, curH - margin);
        if (w->x + w->w < margin) w->x = 0.0f;
        if (w->y + w->h < margin) w->y = 0.0f;
        w->topmost = j.value("topmost", false);
        w->fixedSize = j.value("fixedSize", false);
        w->collapsed = j.value("collapsed", false);
        if (j.value("ball", false)) {
            w->ball.active = true;
            w->ball.x = j.value("bx", w->x);
            w->ball.y = j.value("by", w->y);
            w->ball.r = 24.0f * scale;
        }
        restored++;
    }
    if (restored > 0) {
        // 旧屏幕坐标 → 当前屏幕映射 + 屏幕内 clamp（复用 rescale：
        // scale 不变只做位置映射，悬浮球同步 clamp）
        rescale(scale, scale, savedW, savedH, curW, curH);
    }
    return restored;
}

void LayerStack::tileWindows(float screenW, float screenH) {
    // 参与整理：可见非球态面板窗（球态保持原位不折腾）
    std::vector<WindowInstance*> wins;
    for (WindowInstance* w : allWindows())
        if (w->visible && !w->ball.active) wins.push_back(w);
    if (wins.empty()) return;
    int n = (int)wins.size();
    int cols = (int)std::ceil(std::sqrt((float)n));
    int rows = (int)std::ceil((float)n / (float)cols);
    float scale = g_uiCtx.scale;
    float gap = 8.0f * scale;
    float cw = (screenW - gap * (cols + 1)) / cols;
    float ch = (screenH - gap * (rows + 1)) / rows;
    for (int i = 0; i < n; i++) {
        WindowInstance* w = wins[i];
        if (w->isFullscreen()) continue;
        const WindowConfig* cfg = w->config();
        // 尺寸压入格子（clamp 配置最小尺寸，极小屏可能溢出格子属最佳努力）
        w->w = cw;
        w->h = ch;
        if (cfg) {
            w->w = std::max(w->w, cfg->minW * scale);
            w->h = std::max(w->h, cfg->minH * scale);
        }
        w->x = gap + (float)(i % cols) * (cw + gap);
        w->y = gap + (float)(i / cols) * (ch + gap);
    }
}

void LayerStack::cascadeWindows(float screenW, float screenH) {
    std::vector<WindowInstance*> wins;
    for (WindowInstance* w : allWindows())
        if (w->visible && !w->ball.active) wins.push_back(w);
    if (wins.empty()) return;
    float scale = g_uiCtx.scale;
    for (size_t i = 0; i < wins.size(); i++) {
        WindowInstance* w = wins[i];
        if (w->isFullscreen()) continue;
        const WindowConfig* cfg = w->config();
        // 层叠语义：恢复默认尺寸 + 从左上角阶梯偏移（同 placeWindow 偏移规则）
        if (cfg) { w->w = cfg->defaultW * scale; w->h = cfg->defaultH * scale; }
        float off = (float)(i % 8) * CASCADE_STEP * scale;
        w->x = 40.0f * scale + off;
        w->y = 40.0f * scale + off;
        w->x = std::min(w->x, std::max(0.0f, screenW - w->w));
        w->y = std::min(w->y, std::max(0.0f, screenH - w->h));
    }
}

} // namespace ui