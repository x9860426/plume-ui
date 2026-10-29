// AI生成
#pragma once
// ============================================================
// 下拉选择系统（系统派发：统一收编下拉按钮的绘制与交互）
//
// 扩展代码只做三件事：
//   1. render 期：g_dropdowns.draw(dev, win, req, alpha);    // 声明下拉+画按钮
//   2. render 末：g_dropdowns.renderPopups(dev, win, alpha);  // 画展开浮层（覆盖下方）
//   3. click/hover/wheel 开头调三交互函数（系统遍历判断命中）
//
// 系统自动处理（扩展代码禁止自写）：
//   按钮三态(普通/hover/开) / 展开-收起 / 弹出位置自适应翻转 /
//   列表裁剪滚动 / 迷你滚动条 / 项 hover 高亮 / 选中项文字强调 /
//   打开即定位选中项到可见区中部 / 吞点击防穿透 / 帧末失活 / 单开
//
// 改选写值：req.target 非 null 时系统直接 *target = newIndex（业务零代码）；
//   req.dirty 非 null 时值变化置 *dirty = true。target = null 时业务据返回 id 自理。
//
// 收编根源：原主题工作室 6 行下拉内联实现 + 概览页 pctl 第二套下拉，
//   AI 新建悬浮窗时各写一遍下拉全是问题。现全引擎下拉唯一走本系统。
// ============================================================
#include "WinCommon.h"
#include "WindowInstance.h"
#include <string>
#include <vector>

namespace ui {

// 下拉申请请求（btnRect 为屏幕绝对坐标，与 RegionSystem 一致）
struct DropdownRequest {
    Rect btnRect;                          // 按钮矩形（屏幕绝对）
    const char* const* items = nullptr;    // 选项文本数组（render 至下次 click 间有效）
    int itemCount = 0;                     // 选项数
    int selected = 0;                      // 当前选中索引（读取展示）
    int* target = nullptr;                 // 改选写值目标（null = 业务自理）
    bool* dirty = nullptr;                 // 值变化时置 true（可选，配合重启/重配）
    const char* id = "";                   // 逻辑名（窗口内唯一，跨帧续期展开/滚动）
    float itemH = 0;                       // 列表项高（0 = 默认 20×scale）
    Font* font = nullptr;                  // 文字字体（nullptr = g_uiCtx.smallFont）
    float upBound = 0, downBound = 0;      // 弹出约束区屏幕 y（0 = 不约束，列表可任意弹）
};

// handlePress 返回（常用只看 consumed）
struct DropdownHit {
    bool consumed = false;       // 是否消费点击（吞穿透）
    const char* id = nullptr;    // 命中下拉 id（选项命中时，target=null 场景自理用）
    int newIndex = -1;           // 新选中索引
};

// 下拉持久状态（跨帧保留展开/滚动；按 winId+id 续期）
struct DropdownState {
    int winId = 0;
    std::string id;
    // 本帧声明（draw 填充，交互期/renderPopups 读）
    Rect btnRect;
    Rect listRect;
    const char* const* items = nullptr;
    int itemCount = 0;
    int selected = 0;
    int* target = nullptr;
    bool* dirty = nullptr;
    float itemH = 20.0f;
    Font* font = nullptr;
    float upBound = 0, downBound = 0;   // 弹出约束区屏幕 y（0 = 不约束）；弹出层裁剪到区内
    // 跨帧交互态
    bool open = false;
    bool hoverBtn = false;
    int hoverItem = -1;
    float scroll = 0;              // 列表滚动偏移（项为单位）
    uint64_t frameStamp = 0;
    bool active = false;           // 本帧是否 draw 续期
};

class DropdownSystem {
public:
    // 渲染期：声明下拉 + 画按钮（列表浮层由 renderPopups 统一画）。返回当前 open 态
    bool draw(RenderDevice* dev, WindowInstance& win, const DropdownRequest& req, float alpha);
    // 渲染期：画本窗口所有 open 下拉的列表浮层（内容画完后调一次，覆盖下方）
    void renderPopups(RenderDevice* dev, WindowInstance& win, float alpha);

    // 交互期：按下（返回是否消费 + 命中信息；target 非 null 时系统已写值）
    DropdownHit handlePress(WindowInstance& win, const Vec2& m);
    // 交互期：hover 更新（m=(-1e9,-1e9) 时全 false = 自动清空）
    void updateHover(WindowInstance& win, const Vec2& m);
    // 交互期：滚轮（展开列表内滚动；返回 true = 已消费）
    bool handleWheel(WindowInstance& win, const Vec2& m, float delta);

    // 帧末：未续期失活（frameStamp 停在旧帧 → active=false, open=false）
    void endFrame();
    // 窗口物理删除时清理其全部下拉状态
    void removeWindow(int winId);

private:
    DropdownState* findOrCreate(WindowInstance& win, const char* id);
    void layoutList(DropdownState& st, const DropdownRequest& req);
    std::vector<DropdownState> items_;
    uint64_t frame_ = 0;
};

} // namespace ui