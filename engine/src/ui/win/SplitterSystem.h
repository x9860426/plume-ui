#pragma once
// ============================================================
// 分割器系统（系统派发：统一收编分割条/分隔面板的绘制与拖拽）
//
// 扩展代码只做四件事：
//   1. render 期：g_splitters.draw(dev, win, req, alpha);     // 声明分割+画条
//   2. click 开头：g_splitters.handlePress(win, m).consumed;   // 命中进入拖拽
//   3. drag / release：g_splitters.handleDrag(win, m); / endDrag();
//   4. hover 里：g_splitters.updateHover(win, m);
//
// 系统自动处理（扩展代码禁止自写）：
//   分割条三态(普通/hover/拖拽中) / 把手 grip 绘制 / 拖拽位置写值 /
//   拖动范围 clamp / 吞点击防穿透（handlePress 返回 consumed）/
//   帧末失活（切页/窗口隐藏自动收起拖拽态）
//
// 分割位置语义（与下拉 selected 同款声明式写值）：
//   req.target 非 null 时拖动中系统直接 *target = newPos（屏幕坐标，
//   业务零代码）；req.dirty 非 null 且值变化时置 *dirty = true。
//   业务 render 按该值布局两侧面板（几何单源在业务 layout）。
//
// 嵌套分割：多个 splitter 各自独立 id，可在一个窗口内任意组合
//   （左|右 再上|下嵌套等），系统按 winId+id 独立续期互不干扰。
// ============================================================
#include "WinCommon.h"
#include "WindowInstance.h"
#include <string>
#include <vector>

namespace ui {

// 分割申请请求（barRect 为屏幕绝对坐标，与 RegionSystem/DropdownSystem 一致）
struct SplitterRequest {
    Rect barRect;              // 分割条热区（屏幕绝对；建议厚度 10×scale 好命中）
    bool vertical = true;      // true=垂直条（分左右两栏，拖动改 x）；false=水平条（分上下，拖动改 y）
    float minPos = 0;          // 分割位置下限（屏幕坐标；两侧面板最小尺寸由业务定）
    float maxPos = 0;          // 分割位置上限（必须 > minPos）
    float* target = nullptr;   // 分割位置写值目标（屏幕坐标；null = 业务自理）
    bool* dirty = nullptr;     // 位置变化时置 true（可选，配合重排/重配）
    const char* id = "";       // 逻辑名（窗口内唯一，跨帧续期 hover/拖拽态）
};

// handlePress 返回（常用只看 consumed）
struct SplitterHit {
    bool consumed = false;     // 是否消费点击（吞穿透，进入拖拽）
    const char* id = nullptr;  // 命中分割条 id（target=null 场景自理用）
};

// 分割条持久状态（跨帧保留 hover/拖拽；按 winId+id 续期）
struct SplitterState {
    int winId = 0;
    std::string id;
    // 本帧声明（draw 填充，交互期读）
    Rect barRect;
    bool vertical = true;
    float minPos = 0, maxPos = 0;
    float* target = nullptr;
    bool* dirty = nullptr;
    // 跨帧交互态
    bool hover = false;
    bool dragging = false;
    float dragOff = 0;            // 按下点相对分割中心的偏移（拖动保持不跳变）
    Vec2  lastWinPos;             // 上帧窗口左上（窗口移动检测：target 同步平移）
    bool  winPosInited = false;
    uint64_t frameStamp = 0;
    bool active = false;          // 本帧是否 draw 续期
};

class SplitterSystem {
public:
    // 渲染期：声明分割条 + 画视觉条（胶囊底 + 三态边框 + grip 把手）。
    // 返回当前拖拽态（拖拽中业务可锁其他交互）
    bool draw(RenderDevice* dev, WindowInstance& win, const SplitterRequest& req, float alpha);

    // 交互期：按下（热区命中 → 进入拖拽并返回 consumed 吞穿透）
    SplitterHit handlePress(WindowInstance& win, const Vec2& m);
    // 交互期：拖拽更新（拖动位置 clamp 后写 *target；每帧调用）
    void handleDrag(WindowInstance& win, const Vec2& m);
    // 交互期：拖拽结束（统一复位全部拖拽态）
    void endDrag();
    // 交互期：hover 更新（m=(-1e9,-1e9) 时全 false = 自动清空）
    void updateHover(WindowInstance& win, const Vec2& m);

    // 帧末：未续期失活（frameStamp 停在旧帧 → active=false, dragging=false）
    void endFrame();
    // 窗口物理删除时清理其全部分割条状态
    void removeWindow(int winId);

private:
    SplitterState* findOrCreate(WindowInstance& win, const char* id);
    std::vector<SplitterState> items_;
    uint64_t frame_ = 0;
};

} // namespace ui