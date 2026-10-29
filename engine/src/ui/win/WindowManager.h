#pragma once
// ============================================================
// 窗口管理器（统一输入路由 + 帧时序，规范 §六/§九）
//
// 职责（全部系统统一处理，扩展代码零参与）：
//   1. 输入路由：按下/移动/释放/滚轮/快捷键 → 正确的窗口/按钮/滚动条/悬浮球
//      - 悬浮球最高优先（规范 §9.1）
//      - 命中唯一消费：一次点击只归一个窗（规范 §2.2）
//      - 遮罩屏障：有遮罩窗打开时，遮罩区域点击被吸收（规范 §2.7）
//      - 外部关闭仅栈顶生效（规范 §2.3）
//      - 点下层窗聚焦：bringToFront + 消费（规范 §2.4）
//   2. id驱动跨帧交互：只存 activeId + 模式 + 偏移，每帧按 id 现查（规范 §9）
//   3. 帧时序（规范 §9.5）：
//      输入 → updateFade → openPending → renderAll → renderBalls
//      → RegionSystem.endFrame → flushDisposed
//   4. 快捷键 0-9：按配置表索引打开窗口（配置驱动，零硬编码）
//
// MainUI 接入只需两行：
//   winMgr.handleInput(in);   // 帧首：全部输入路由
//   winMgr.frame(dev, dt);    // 帧尾：淡入淡出+渲染+回收
// ============================================================
#include "WinCommon.h"
#include "LayerStack.h"
#include "TextInputSystem.h"
#include "../../app/AppFwd.h"

namespace ui {

// ====== 跨帧交互模式（规范 §9：全局仅存 id + 模式 + 偏移，不存窗口指针） ======
enum InteractMode {
    MODE_NONE = 0,       // 空闲
    MODE_MOVE,           // 标题栏拖拽移动窗口
    MODE_RESIZE,         // 右下角手柄缩放窗口
    MODE_SCROLLBAR,      // 系统滚动条拖拽
    MODE_BALL,           // 悬浮球拖拽
    MODE_REGION_SCROLL,  // 系统区域滚动（滚轮在滚动容器内）
    MODE_CONTENT,        // 窗口内容拖拽（handler.click 消费后，系统转发 drag/release）
};

class WindowManager {
public:
    // ---- 初始化（main 启动时调用一次：注册全部窗口处理器） ----
    void init();

    // ---- 帧首：全部输入路由（返回值仅调试用） ----
    void handleInput(Input& in);

    // ---- 帧尾：淡入淡出 → 待打开 → 分层渲染 → 悬浮球 → 区域收帧 → 物理回收 ----
    void frame(RenderDevice* dev, float dt);

    // ---- 便捷打开（配置驱动；遍历回调中请用 g_layerStack.pendingOpen） ----
    int  openWindow(const char* configId);

    // ---- 打开警告窗并注入内容（业务文本由打开方提供） ----
    int  openWarning(const char* title, const char* msg);

    // ---- 统一文本输入系统（输入框唯一认知：焦点/字符流/光标相位全系统管理） ----
    // 页面与悬浮窗处理器同协议接入（requestFocus / focused / caretOn）；
    // 字符流在 handleInput 帧首由系统统一消费，外部无需转发
    TextInputSystem& textInput() { return textInput_; }

    // ---- 交互状态查询（调试/主界面判断点击是否被悬浮窗系统消费） ----
    bool consumedThisFrame() const { return mouseConsumed_; }
    int  activeId() const { return activeId_; }
    int  mode() const { return mode_; }

private:
    // 按下路由（左键）
    bool onMousePressed(const Vec2& m);
    // 移动路由（拖拽推进 + hover 刷新）
    void onMouseMove(const Vec2& m, bool anyButtonDown);
    // 释放路由（结束当前模式）
    void onMouseReleased(const Vec2& m);
    // 滚轮路由（穿透语义 + 区域滚动）
    bool onWheel(const Vec2& m, float delta);
    // 快捷键路由（0-9 = 配置表索引，Escape = 关闭栈顶）
    void onKeys(Input& in);

    // 执行标题栏按钮动作（统一按钮系统返回的动作枚举）
    void execBtnAction(WindowInstance& win, int action, const Vec2& m);

    // ---- id驱动交互状态（规范 §9，禁止存窗口指针） ----
    int  activeId_ = -1;       // 当前交互窗口实例 id
    int  mode_ = MODE_NONE;    // 当前交互模式
    int  offX_ = 0, offY_ = 0; // 按下时的偏移（拖拽/缩放/球）
    bool mouseConsumed_ = false; // 本帧按下是否被悬浮窗系统消费
    bool ballMoved_ = false;   // 悬浮球本次按下是否发生拖动（区分点击恢复）
    TextInputSystem textInput_; // 统一文本输入系统（每应用一份；焦点/字符流/光标单源）
};

// 旧全局单例 g_winMgr：多应用架构后由 AppFwd.h 宏转发到当前应用的
// winMgr 成员（每应用独立输入路由/交互状态，彻底分离）

} // namespace ui