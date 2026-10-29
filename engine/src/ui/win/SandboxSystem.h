// AI生成
#pragma once
// ============================================================
// 沙盘系统（系统派发：可平移可缩放的无限画布容器，又称特殊滚轮容器）
//
// 业务在世界坐标（逻辑像素）放置元素，系统提供 viewport 变换（pan+zoom）：
//   交互：Ctrl+滚轮=缩放（以鼠标为中心）/ 滚轮=竖向平移 / Shift+滚轮=横向平移
//         中键或空格+左键=拖拽平移
//   变换：screen = world * zoom + pan（pan = 世界原点映射到的屏幕坐标）
//   裁剪：begin 设视口裁剪，业务在内绘制，end 恢复
//   网格：可选画无限网格背景（密度随 zoom 自适应）
//
// 元素缩放模式由业务自选（系统不强制）：
//   - 跟随缩放：size * h.zoom（元素随画布缩放）
//   - 不缩放：size（固定屏幕大小，只有 pan）
//   - LOD：按 h.zoom 阈值切换显示（如 zoom>0.5 画图标+文字，0.2~0.5 只画文字，<0.2 只画点）
// ============================================================
#include "WinCommon.h"
#include "WindowInstance.h"
#include <string>
#include <vector>

namespace ui {

struct SandboxRequest {
    Rect viewRect;              // 沙盘视口区域（屏幕绝对坐标）
    const char* id = "";        // 逻辑名（窗口内唯一，跨帧续期 zoom/pan）
    float minZoom = 0.2f;       // 缩放下限
    float maxZoom = 5.0f;       // 缩放上限
    float initZoom = 1.0f;      // 初始缩放（首次创建）
    bool  panEnabled = true;    // 允许平移
    bool  zoomEnabled = true;   // 允许缩放
    bool  showGrid = true;      // 网格辅助线（向系统申请；默认显示；有限沙盘只画世界内）
    bool  showZoomHud = true;   // 右下角 zoom% 指示（向系统申请；默认显示）
    float worldW = 0;           // 世界宽（0=无限不钳制；>0 有限沙盘）
    float worldH = 0;           // 世界高（0=无限）
    float worldPad = 200.0f;    // 沙盘外显示余量（有限沙盘）：不管缩放多少可视范围
                                // 恒为 (W+2pad)×(H+2pad)——边缘最多显示 pad 的沙盘外；
                                // 平移以可视框为界；动态 minZoom = 可视框恰好铺满
                                // 视口的倍率（视口=世界 200×200、pad=200 时=33%），
                                // 再小视口会超出可视框出现空白，故以此为缩放下限
};

struct SandboxHandle {
    bool  valid = false;
    int   winId = 0;
    const char* id = nullptr;   // 逻辑名（指向持久状态；越界警告去重用）
    Rect  viewRect;             // 视口（屏幕绝对）
    float zoom = 1.0f;          // 当前缩放
    Vec2  pan;                  // 世界原点(0,0)映射到的屏幕坐标
    bool  panning = false;      // 平移中（业务可锁点击）
    float worldW = 0, worldH = 0;   // 世界边界（drawNode 越界检测用）
    float worldPad = 200.0f;        // 显示余量（可视框 = 世界外扩 pad）
    bool  showZoomHud = true;   // HUD 显示（end 按此画）
};

// ====== 沙盘节点声明（业务只填此结构；绘制/LOD/配色系统全包） ======
struct SandboxNode {
    Vec2  world;                // 世界坐标
    const char* label = nullptr; // 标签（null = 不画文字）
    int   mode = 0;             // 0=跟随缩放（尺寸×zoom） 1=固定屏幕尺寸 2=LOD 分级
    int   colorMode = 0;        // 0=accent 1=bg3 2=btnBg1（主题色单源，业务零配色）
    float size = 28.0f;         // 基准尺寸（世界单位）
    float lodHigh = 0.5f;       // LOD 上阈值：zoom≥此值 画完整（块+文字）
    float lodLow  = 0.2f;       // LOD 下阈值：低于只画点；区间内只画文字
};

struct SandboxState {
    int winId = 0;
    std::string id;
    Rect viewRect;
    float zoom = 1.0f;
    Vec2 pan;
    float minZoom = 0.2f, maxZoom = 5.0f;
    bool panEnabled = true, zoomEnabled = true, showGrid = true;
    float worldW = 0, worldH = 0;   // 世界边界（0=无限）
    float worldPad = 200.0f;        // 显示余量（可视框钳制单位）
    bool panning = false;
    Vec2 panDragOff;
    bool hover = false;
    bool inited = false;
    Vec2 lastViewPos;           // 上帧视口左上（窗口移动检测：pan 同步平移）
    bool viewInited = false;
    bool outWarned = false;     // 节点越界警告已发（去重；每沙盘一次）
    uint64_t frameStamp = 0;
    bool active = false;
};

class SandboxSystem {
public:
    SandboxHandle begin(RenderDevice* dev, WindowInstance& win, const SandboxRequest& req);
    void end(RenderDevice* dev, const SandboxHandle& h, float alpha);

    bool handleWheel(WindowInstance& win, const Vec2& m, float delta, bool ctrl, bool shift);
    bool handlePress(WindowInstance& win, const Vec2& m, bool panModifier);
    void handleDrag(WindowInstance& win, const Vec2& m);
    void endDrag();
    void updateHover(WindowInstance& win, const Vec2& m);

    // 视图复位（「调用」语义：把该沙盘的 pan/zoom 回到初始居中视图；
    // 无限沙盘=世界原点回视口中心，有限沙盘=世界铺满视口）
    bool resetView(WindowInstance& win, const char* id);

    void endFrame();
    void removeWindow(int winId);

    const SandboxState* find(int winId, const char* id) const;

    // 坐标变换单源（世界→屏幕）：业务禁止自己乘 zoom/pan
    Vec2  worldToScreen(const SandboxHandle& h, const Vec2& world) const;
    float worldToScreenSize(const SandboxHandle& h, float worldLen) const;
    // 世界矩形→屏幕矩形（中心锚点：world 为矩形中心，size 为世界尺寸）
    Rect  worldToScreenRect(const SandboxHandle& h, const Vec2& world,
                            const Vec2& worldSize) const;
    // 世界字号→屏幕字号（传给 wctl 控件 px 前需再经 fontPxForScreen）
    float nodeFontPx(const SandboxHandle& h, float worldPx) const;

    // 节点绘制系统派发：三模式尺寸 + LOD 阈值切换 + 主题色单源全包，
    // 业务只声明 SandboxNode（对照 g_dropdowns.draw 范式）
    void drawNode(RenderDevice* dev, const SandboxHandle& h,
                  const SandboxNode& n, float alpha);

private:
    SandboxState* findOrCreate(WindowInstance& win, const char* id);
    void drawGrid(RenderDevice* dev, const SandboxState& st, float alpha);
    void drawWorldBounds(RenderDevice* dev, const SandboxState& st, float alpha);
    std::vector<SandboxState> items_;
    uint64_t frame_ = 0;
};

} // namespace ui