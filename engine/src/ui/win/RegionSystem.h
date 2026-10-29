// AI生成
#pragma once
// ============================================================
// 区域申请系统（规范核心重写：统一参数申请 + 系统派发）
//
// 扩展代码只做三件事：
//   1. RegionHandle h = regions.begin(dev, win, req);   // 申请区域+系统设裁剪
//   2. 在 h 内绘制内容（内容坐标可超出区域，系统自动裁剪；
//      纵向用 h.scrollY 平移；总高 > 区域高时系统自动生成滚动容器）
//   3. regions.end(dev, h, contentH);                   // 系统恢复裁剪+提交高度
//
// 系统自动处理（扩展代码禁止自写）：
//   滚轮滚动 / 滚动条绘制与拖拽 / 区域裁剪 / scrollY 维护与 clamp /
//   标签切换整组隐藏（非当前 tab 的区域不激活）/ 窗口缩放自适应
// ============================================================
#include "WinCommon.h"
#include "WindowInstance.h"
#include "../../app/AppFwd.h"
#include <string>
#include <vector>

namespace ui {

// 区域申请请求（窗口内容区内的相对坐标，逻辑像素，系统乘 scale）
struct RegionRequest {
    float x = 0, y = 0;     // 相对窗口左上角
    float w = 0, h = 0;     // 申请的区域大小
    int tabId = 0;          // 所属标签（非当前标签时区域不激活）
    const char* id = "";    // 区域逻辑名（窗口内唯一，滚动状态按此续期复用）
};

// 系统返回的区域句柄（全部为屏幕绝对坐标）
struct RegionHandle {
    bool valid = false;
    int winId = 0;
    int regionSeq = 0;              // 区域序号（同窗口内递增）
    float clipX = 0, clipY = 0;     // 可视区起点（裁剪后）
    float clipW = 0, clipH = 0;     // 可视区大小
    float scrollY = 0;              // 当前滚动偏移（系统维护，绘制内容时用 clipY - scrollY 起排）
    float contentW = 0;             // 可用内容宽度（系统预留滚动条宽度后自动扣除）
    bool hasScrollbar = false;      // 本帧是否生成滚动条（内容超出自适应）
};

// 区域持久状态（滚动位置跨帧保留；标签切换只是失活不删除）
struct RegionState {
    int winId = 0;
    std::string id;
    int tabId = 0;
    int seq = 0;                    // 窗口内分配序号
    Rect view;                      // 本帧视口（屏幕绝对）
    float scrollY = 0;
    float contentH = 0;
    bool active = false;            // 本帧是否激活（begin 续期）
    bool hasScrollbar = false;
    Rect scrollbarTrack;            // 滚动条轨道（屏幕绝对；带按钮模板时为中间段）
    Rect scrollbarSlider;           // 滚动条滑块
    Rect btnUp, btnDown;            // 两端箭头按钮（模板带按钮时有效；无按钮=零矩形）
    int hoverBtn = 0;               // 箭头按钮 hover：0=无 1=上 2=下
    int holdDir = 0;                // 箭头按钮长按连滚方向：0=无 +1=下 -1=上
    float holdNext = 0.0f;          // 下次连滚触发时刻（steady_clock 秒）
    bool hoverScrollbar = false;
    bool draggingScrollbar = false;
    float scrollbarDragOffY = 0;
    float contentW = 0;             // 可用内容宽（= view.w − 轨道预留；样式感知，供内容让位）
    uint64_t frameStamp = 0;
};

class RegionSystem {
public:
    // 渲染期：申请区域并设置裁剪（begin 语义）
    RegionHandle begin(RenderDevice* dev, WindowInstance& win, const RegionRequest& req);
    // 渲染期：恢复裁剪并提交内容总高（end 语义；contentH > 视口高 → 自动滚动容器）
    void end(RenderDevice* dev, const RegionHandle& h, float contentH);

    // 交互期：滚轮（返回 true = 已消费）
    bool handleWheel(WindowInstance& win, const Vec2& m, float delta);
    // 交互期：按下命中滚动条（返回 true = 已消费并进入拖拽）
    bool handlePress(WindowInstance& win, const Vec2& m);
    // 交互期：滚动条拖拽更新（拖拽中每帧调用）
    void handleDrag(WindowInstance& win, const Vec2& m);
    // 交互期：更新滚动条 hover 态
    void updateHover(WindowInstance& win, const Vec2& m);
    // 拖拽结束（统一复位；箭头按钮长按连滚一并停止）
    void endDrag() { for (auto& r : regions_) { r.draggingScrollbar = false; r.holdDir = 0; } }

    // 查询：模块内容回调用（读取区域内滚动偏移做命中测试）
    const RegionState* findRegion(int winId, const char* id) const;
    float getScroll(int winId, const char* id) const;
    // 查询：该区域系统让位后的可用内容宽（滚动条模板感知；未找到=0）
    float getContentW(int winId, const char* id) const;

    // 渲染期：绘制某窗口全部激活区域的滚动条（窗口内容渲染完由系统调用）
    void renderScrollbars(RenderDevice* dev, WindowInstance& win, float alpha);

    // 帧末：未续期区域失活 + scrollY clamp
    void endFrame();
    // 窗口物理删除时清理其全部区域状态
    void removeWindow(int winId);

private:
    RegionState* findOrCreate(WindowInstance& win, const char* id);
    void layoutScrollbar(RegionState& r);
    void stepScroll(RegionState& r, int dir);   // 箭头按钮/滚轮单步滚动（clamp+重排）
    std::vector<RegionState> regions_;
    uint64_t frame_ = 0;
    int nextSeq_ = 1;
};

// 旧全局单例 g_regions：多应用架构后由 AppFwd.h 宏转发到当前应用的
// regions 成员（每应用独立区域滚动状态，彻底分离）

} // namespace ui