// AI生成
#include "WindowManager.h"
#include "UiContext.h"
#include "HandlerRegistry.h"
#include "RegionSystem.h"
#include "DropdownSystem.h"
#include "SplitterSystem.h"
#include "SandboxSystem.h"
#include "WindowButtons.h"
#include "WindowConfig.h"
#include "WinHandlers.h"
#include "../Element.h"
#include "../../app/AppFwd.h"   // appSuspendWindows()：子窗口禁悬浮窗短路判定

namespace ui {

// 旧全局单例 g_winMgr 已删除：多应用架构后每应用独立
// （AppFwd.h 宏转发到 AppManager 当前应用的 winMgr 成员）

// 悬浮球点击判定阈值：按下后拖动超过该距离视为"拖动"而非"点击恢复"
static constexpr float BALL_DRAG_THRESHOLD = 6.0f;

void WindowManager::init() {
    RegisterAllWindowHandlers();  // 配置驱动注册（新增窗口零引擎改动）
}

int WindowManager::openWindow(const char* configId) {
    return g_layerStack.createWindow(configId);
}

int WindowManager::openWarning(const char* title, const char* msg) {
    int id = g_layerStack.createWindow("warning");
    if (WindowInstance* w = g_layerStack.findWindow(id)) {
        if (title) w->warningTitle = title;
        if (msg)   w->warningMsg = msg;
    }
    return id;
}

// ============================================================
// 帧首：全部输入路由
// ============================================================
void WindowManager::handleInput(Input& in) {
    Vec2 m = in.mouse();
    // 帧号自增（全局单源：调试/录制序号；周期动画请用真实时间）
    g_uiCtx.frameId++;
    // 鼠标位置镜像到全局单源（调试窗「输入」页展示，参考 GetCursorPos）
    g_uiCtx.mouseX = m.x;
    g_uiCtx.mouseY = m.y;
    mouseConsumed_ = false;
    // 子窗口禁悬浮窗（allowSuspendWindows=false）：本应用内所有
    // 悬浮窗/悬浮窗/悬浮球交互全停——不消费任何输入（主界面照常）
    if (appSuspendWindows()) return;

    // ---- 统一文本输入：悬垂校验 → 字符流（Enter 提交可能改变焦点，
    //      随后的快捷键豁免判定用新状态） ----
    textInput_.validateOwners();
    textInput_.feedChars(in);

    // ---- 按下（左键） ----
    if (in.mousePressed(0)) {
        mouseConsumed_ = onMousePressed(m);
    }
    // ---- 释放（左键） ----
    if (in.mouseReleased(0)) {
        onMouseReleased(m);
    }
    // ---- 移动（拖拽推进 + hover 刷新） ----
    onMouseMove(m, in.mouseDown(0));

    // ---- 滚轮 ----
    if (in.wheelDelta() != 0.0f) {
        onWheel(m, in.wheelDelta());
    }

    // ---- 快捷键 ----
    onKeys(in);
}

// ============================================================
// 按下路由（规范 §9.1 完整实现）
// ============================================================
bool WindowManager::onMousePressed(const Vec2& m) {
    // 文本失焦提交：任何点击发生 → 当前焦点输入框统一提交（点击输入框
    // 本身时，其处理器 click 会 requestFocus 重新获焦并预填新值）
    textInput_.blurAll();

    // 0. 悬浮球最高优先（任何层任何球先于面板命中）
    if (WindowInstance* bw = g_layerStack.hitTestBall(m)) {
        activeId_ = bw->id;
        mode_ = MODE_BALL;
        ballMoved_ = false;
        offX_ = (int)(m.x - bw->ball.x);
        offY_ = (int)(m.y - bw->ball.y);
        bw->ball.dragging = true;
        bw->ball.downMouseX = (int)m.x;
        bw->ball.downMouseY = (int)m.y;
        return true;  // 消费
    }

    // 1. 面板命中测试（层高→低、层内栈顶→栈底，唯一最上层命中窗）
    WindowInstance* hit = g_layerStack.hitTest(m);

    if (!hit) {
        // 未命中任何窗口面板：
        // a) 栈顶窗策略允许"点击外部关闭" → 关闭栈顶（仅栈顶生效，规范 §2.3）
        WindowInstance* top = g_layerStack.topWindow();
        if (top && top->config() &&
            top->config()->closePolicy == CLOSE_OUTSIDE_CLICK) {
            g_layerStack.closeWindow(top->id);
            return true;  // 消费（关闭动作本身即本次点击的结果）
        }
        // b) 存在遮罩屏障窗 → 事件被遮罩吸收（规范 §2.7）
        if (g_layerStack.topOverlayBarrier()) return true;
        // c) 无遮罩 → 穿透到主界面
        return false;
    }

    // 2. 命中窗口：若非全局栈顶 → 先聚焦（bringToFront，规范 §2.4）
    WindowInstance* top = g_layerStack.topWindow();
    bool isTopmost = (top == hit);
    if (!isTopmost) {
        g_layerStack.bringToFront(hit->id);
        // chrome 区（标题栏按钮/标题栏拖拽/缩放手柄）：意图明确，
        // 聚焦后本次点击继续执行（对齐参考 window_interaction.cpp L44-53
        // 置顶后不消费点击——远程触摸/多窗恢复场景点关闭一次生效）
        // 内容区仍按 clickThroughToFront 判定（防误触下层控件）
        if (wbtn::handlePress(*hit, m) == wbtn::BtnAction::None &&
            !hit->config()->clickThroughToFront)
            return true;  // 消费（聚焦即结果）
    }

    // 3. 栈顶窗正常交互（优先级从高到低）
    // 3.1 标题栏按钮（关闭/悬浮球/置顶/固定大小/折叠）
    int btnAction = (int)wbtn::handlePress(*hit, m);
    if (btnAction != (int)wbtn::BtnAction::None) {
        execBtnAction(*hit, btnAction, m);
        return true;  // 消费
    }
    // 3.2 系统滚动条（区域系统统一派发，扩展代码不参与）
    if (g_regions.handlePress(*hit, m)) {
        activeId_ = hit->id;
        mode_ = MODE_SCROLLBAR;
        return true;  // 消费
    }
    // 3.3 内容区点击 → 分发给窗口处理器（注册表配置驱动）
    const WinHandler* h = g_handlers.find(hit->config()->handlerName);
    if (h && h->click && h->click(*hit, m)) {
        activeId_ = hit->id;
        mode_ = MODE_CONTENT;  // 后续 drag/release 由系统转发给处理器
        return true;  // 处理器已消费
    }
    // 3.4 内容未命中：
    //     a) 无标题栏可拖窗口 → 空白处按下即整窗拖动
    //        （对齐参考设置/侧边设置/停靠窗的整窗拖动语义）
    //     b) 其余 → 点击落在窗口空白处，纯消费（不穿透到下层）
    //     锁定（fixedSize）：位置+尺寸全锁，整窗拖动同样禁止
    if (hit->isDraggable() && !hit->hasTitleBar() && !hit->fixedSize) {
        activeId_ = hit->id;
        mode_ = MODE_MOVE;
        offX_ = (int)(m.x - hit->x);
        offY_ = (int)(m.y - hit->y);
        return true;
    }
    return true;
}

// ============================================================
// 移动路由
// ============================================================
void WindowManager::onMouseMove(const Vec2& m, bool anyButtonDown) {
    // ---- 拖拽中：按 activeId 现查窗口（查不到则安全复位，规范 §9.2） ----
    if (mode_ != MODE_NONE) {
        WindowInstance* w = g_layerStack.findWindow(activeId_);
        if (!w) {  // 窗口已被删除：安全取消本次交互
            mode_ = MODE_NONE;
            activeId_ = -1;
            g_regions.endDrag();
            return;
        }
        switch (mode_) {
        case MODE_MOVE: {
            w->x = m.x - offX_;
            w->y = m.y - offY_;
            // 屏幕边界约束（参考 window_interaction.cpp：窗口不可拖出屏幕外）
            // 折叠时实际高度只有标题栏，按折叠后高度约束
            float hh = w->collapsed ? wbtn::titleBarHeight(*w) : w->h;
            w->x = std::max(0.0f, std::min(w->x, (float)g_uiCtx.winW - w->w));
            w->y = std::max(0.0f, std::min(w->y, (float)g_uiCtx.winH - hh));
            break;
        }
        case MODE_RESIZE: {
            const WindowConfig* cfg = w->config();
            float minW = cfg ? cfg->minW * g_uiCtx.scale : 100.0f;
            float minH = cfg ? cfg->minH * g_uiCtx.scale : 80.0f;
            // 尺寸上限：右/下不得超出屏幕（悬浮窗大小不能超过屏幕），
            // 极小屏兜底仍保配置最小尺寸
            float maxW = (float)g_uiCtx.winW - w->x;
            float maxH = (float)g_uiCtx.winH - w->y;
            w->w = std::max(minW, std::min(m.x - w->x + offX_, maxW));
            w->h = std::max(minH, std::min(m.y - w->y + offY_, maxH));
            break;
        }
        case MODE_SCROLLBAR:
            g_regions.handleDrag(*w, m);
            break;
        case MODE_BALL:
            w->ball.x = m.x - offX_;
            w->ball.y = m.y - offY_;
            // 按下点位移超过阈值 → 标记为拖动（释放时不恢复窗口）；
            // 拖动即刻取消半隐藏并恢复透明度（对照参考 L2073-2074）
            if (std::abs(m.x - w->ball.downMouseX) > BALL_DRAG_THRESHOLD ||
                std::abs(m.y - w->ball.downMouseY) > BALL_DRAG_THRESHOLD) {
                ballMoved_ = true;
                w->ball.hidden = false;
                w->ball.alpha = 1.0f;
                w->ball.idleTime = 0;
            }
            break;
        case MODE_CONTENT: {
            // 内容拖拽：系统转发给窗口处理器（滑条/SV区等，扩展代码不写事件路由）
            const WinHandler* h = w->config() ? g_handlers.find(w->config()->handlerName) : nullptr;
            if (h && h->drag) h->drag(*w, m);
            break;
        }
        default:
            break;
        }
        return;  // 拖拽中不刷新 hover
    }

    // ---- 空闲：刷新全部窗口 hover 态（命中窗更新，其余清除） ----
    if (anyButtonDown) return;  // 主界面拖拽中不刷新
    WindowInstance* hit = g_layerStack.hitTest(m);
    for (WindowInstance* w : g_layerStack.allWindows()) {
        bool inThis = (w == hit);
        wbtn::updateHover(*w, inThis ? m : Vec2(-99999, -99999));
        g_regions.updateHover(*w, inThis ? m : Vec2(-99999, -99999));
        const WinHandler* h = w->config() ? g_handlers.find(w->config()->handlerName) : nullptr;
        if (h && h->hover) h->hover(*w, inThis ? m : Vec2(-99999, -99999));
    }
}

// ============================================================
// 释放路由
// ============================================================
void WindowManager::onMouseReleased(const Vec2& m) {
    // 悬浮球释放：拖动 → 靠边吸附；未拖动 → 点击恢复窗口（或唤醒半隐藏球）
    if (mode_ == MODE_BALL) {
        if (WindowInstance* w = g_layerStack.findWindow(activeId_)) {
            w->ball.dragging = false;
            if (ballMoved_) {
                // 靠边吸附（对照参考 ui_input.cpp L2062-2075）：球心落在屏幕
                // 边缘 margin(40*scale) 内 → 松手吸到该边；取消半隐藏与闲置计时
                float r = w->ball.r;
                float margin = 40.0f * g_uiCtx.scale;
                float W = (float)g_uiCtx.winW, H = (float)g_uiCtx.winH;
                bool atLeft = w->ball.x < margin;
                bool atRight = w->ball.x > W - margin;
                bool atTop = w->ball.y < margin;
                bool atBottom = w->ball.y > H - margin;
                if (atLeft) w->ball.x = r;
                else if (atRight) w->ball.x = W - r;
                if (atTop) w->ball.y = r;
                else if (atBottom) w->ball.y = H - r;
                w->ball.hidden = false;
                w->ball.alpha = 1.0f;
                w->ball.idleTime = 0;
            } else if (w->ball.hidden) {
                // 点击半隐藏球 → 唤醒：恢复显示并夹回屏幕内（对照参考 L2076-2083）
                w->ball.hidden = false;
                w->ball.alpha = 1.0f;
                float r = w->ball.r;
                float W = (float)g_uiCtx.winW, H = (float)g_uiCtx.winH;
                if (w->ball.x <= r) w->ball.x = r;
                else if (w->ball.x >= W - r) w->ball.x = W - r;
                if (w->ball.y <= r) w->ball.y = r;
                else if (w->ball.y >= H - r) w->ball.y = H - r;
                w->ball.idleTime = 0;
            } else {
                // 点击球恢复窗口（对齐参考 ui_input.cpp L2085-2099）：
                // 球在左半屏 → 窗口贴球右侧；右半屏 → 窗口贴球左侧；
                // 窗口垂直中心对齐球心；整体 clamp 屏幕内；恢复时从 0 淡入
                w->ball.active = false;
                float nx, ny;
                if (w->ball.x < (float)g_uiCtx.winW * 0.5f)
                    nx = w->ball.x + w->ball.r;
                else
                    nx = w->ball.x - w->ball.r - w->w;
                ny = w->ball.y - w->h * 0.5f;
                nx = std::max(0.0f, std::min(nx, (float)g_uiCtx.winW - w->w));
                ny = std::max(0.0f, std::min(ny, (float)g_uiCtx.winH - w->h));
                w->x = nx;
                w->y = ny;
                w->alpha = 0.0f;
                w->targetAlpha = 1.0f;
            }
        }
    }
    if (mode_ == MODE_SCROLLBAR) g_regions.endDrag();
    if (mode_ == MODE_CONTENT) {
        // 内容释放：系统转发给窗口处理器（复位模块内拖拽标志）
        if (WindowInstance* w = g_layerStack.findWindow(activeId_)) {
            const WinHandler* h = w->config() ? g_handlers.find(w->config()->handlerName) : nullptr;
            if (h && h->release) h->release(*w, m);
        }
    }
    mode_ = MODE_NONE;
    activeId_ = -1;
}

// ============================================================
// 滚轮路由（穿透语义 + 区域滚动，规范 §2.7/§9.4）
// ============================================================
bool WindowManager::onWheel(const Vec2& m, float delta) {
    // 从栈顶向栈底找第一个命中的窗口
    WindowInstance* hit = g_layerStack.hitTest(m);
    if (!hit) {
        // 未命中任何面板：有遮罩屏障 → 吸收；否则穿透到主界面
        return g_layerStack.topOverlayBarrier() != nullptr;
    }
    // handler 滚轮回调优先（模块内自绘滚动容器：下拉列表等，浮层盖在区域系统之上）
    if (!hit->disposed && hit->config()) {
        const WinHandler* h = g_handlers.find(hit->config()->handlerName);
        if (h && h->wheel && h->wheel(*hit, m, delta)) return true;
    }
    // 区域系统次之（滚动容器滚轮，扩展代码零参与）
    if (g_regions.handleWheel(*hit, m, delta)) return true;
    // 命中的窗口有遮罩 → 吸收（不穿透）；无遮罩 → 穿透到主界面
    return hit->hasOverlay();
}

// ============================================================
// 快捷键路由（Esc = 关闭栈顶；开窗不再用 0-9 快捷键，
// 改由「其他」页悬浮窗按钮墙 / 系统调用打开 —— 原 0-9 开窗已按需求删除）
// ============================================================
void WindowManager::onKeys(Input& in) {
    // 文本编辑中：Esc 优先给输入框（放弃编辑恢复原值，不关窗）；
    // 方向键/Home/End/Delete 走 caretKeys 状态机（单击 1 格/双击跳首尾/
    // 长按 0.8s 连发，GLFW 键码非可打印无 Char 流；真实时间驱动）
    if (textInput_.hasFocus()) {
        if (in.keyPressed(256 /*GLFW_KEY_ESCAPE*/)) textInput_.cancelFocus();
        textInput_.caretKeys(in);
        return;
    }

    if (in.keyPressed(256 /*GLFW_KEY_ESCAPE*/)) {
        if (WindowInstance* top = g_layerStack.topWindow())
            g_layerStack.closeWindow(top->id);
    }
}

// ============================================================
// 标题栏按钮动作执行（统一按钮系统的动作枚举 → 系统动作）
// ============================================================
void WindowManager::execBtnAction(WindowInstance& win, int action, const Vec2& m) {
    switch ((wbtn::BtnAction)action) {
    case wbtn::BtnAction::Close:
        g_layerStack.closeWindow(win.id);
        break;
    case wbtn::BtnAction::FloatBall: {
        // 激活悬浮球：窗口收起为球，球出现在窗口右上角
        // 半径对齐参考 ui_input.cpp L1017：24 * scale；重置半隐藏残留
        win.ball.active = true;
        win.ball.dragging = false;
        win.ball.r = 24.0f * g_uiCtx.scale;
        win.ball.x = win.x + win.w - win.ball.r - 8.0f * g_uiCtx.scale;
        win.ball.y = win.y + wbtn::titleBarHeight(win) * 0.5f;
        win.ball.hidden = false;
        win.ball.alpha = 1.0f;
        win.ball.idleTime = 0;
        break;
    }
    case wbtn::BtnAction::PinTop: {
        // 置顶唯一：同一时刻仅一个窗口 topmost，
        // 启用新置顶时强制取消其他所有窗口的旧置顶
        if (!win.topmost)
            for (WindowInstance* w : g_layerStack.allWindows())
                if (w != &win) w->topmost = false;
        win.topmost = !win.topmost;
        g_layerStack.bringToFront(win.id);
        break;
    }
    case wbtn::BtnAction::FixSize:
        win.fixedSize = !win.fixedSize;
        break;
    case wbtn::BtnAction::Collapse:
        win.collapsed = !win.collapsed;
        break;
    case wbtn::BtnAction::Resize:
        activeId_ = win.id;
        mode_ = MODE_RESIZE;
        offX_ = (int)(win.x + win.w - m.x);  // 手柄内侧偏移
        offY_ = (int)(win.y + win.h - m.y);
        break;
    case wbtn::BtnAction::DragTitle:
        // 锁定窗口：禁止标题栏拖动（位置锁定）
        if (win.fixedSize) break;
        activeId_ = win.id;
        mode_ = MODE_MOVE;
        offX_ = (int)(m.x - win.x);
        offY_ = (int)(m.y - win.y);
        break;
    default:
        break;
    }
}

// ============================================================
// 帧尾：完整帧时序（规范 §9.5）
// ============================================================
void WindowManager::frame(RenderDevice* dev, float dt) {
    // 子窗口禁悬浮窗：显示侧全停（悬浮窗/悬浮球不渲染、待打开队列不创建、
    // 颜色选择器不注入）；但淡出推进/区域收帧/物理回收保留——万一栈内
    // 有残留窗口（快照迁移带入/将来动态切换许可），清理时序照常推进，
    // 不会卡死在「看不见但占着」状态（无状态删除铁律）
    if (appSuspendWindows()) {
        g_layerStack.updateFade(dt);
        g_regions.endFrame();
        g_dropdowns.endFrame();
        g_splitters.endFrame();
        g_sandboxes.endFrame();
        g_layerStack.flushDisposed();
        return;
    }
    dev->setAAEnabled(g_uiCtx.sdfAA);  // 0. SDF 软边 AA 总开关（主题工作室实时切换）
    g_layerStack.updateFade(dt);   // 1. 淡入淡出推进
    g_layerStack.openPending();    // 2. 待打开队列统一创建
    PollColorPickerBinding();      // 3. 颜色选择器目标注入（模块间参数传递）
    g_layerStack.renderAll(dev);   // 4. 分层渲染（chrome+内容+滚动条+按钮）
    g_layerStack.renderBalls(dev); // 5. 悬浮球最后画（位于一切之上）
    g_regions.endFrame();          // 6. 区域系统收帧（未续期区域失活+clamp）
    g_dropdowns.endFrame();        //    下拉系统收帧（未续期失活+关闭展开）
    g_splitters.endFrame();        //    分割器系统收帧（未续期失活+复位拖拽）
    g_sandboxes.endFrame();        //    沙盘系统收帧（未续期失活+复位平移）
    g_layerStack.flushDisposed();  // 7. 物理回收淡出完成的窗口
}

} // namespace ui