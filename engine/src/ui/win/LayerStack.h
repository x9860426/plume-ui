#pragma once
// ============================================================
// 层级栈（窗口管理核心，规范 §三/§七/§九）
//
// 四个预定义层级（渲染/命中顺序 = 层序，Lua 暴露编号见 LayerId）：
//   LAYER_SCENE(0)   场景底：游戏画面/主视图
//   LAYER_PLAYER(1)  玩家级：Lua 插件悬浮窗（-1 级）
//                    → 绑定场景 context，切场景整组隐藏、切回重现
//                      （显示/隐藏语义，非销毁，状态全保留）
//                    → 层内可叠放，但永远压不过开发级/警告级
//   LAYER_DEV(2)     开发级：一般 Lua / 硬编码工具窗（0 级）
//   LAYER_GLOBAL(3)  警告级：全局警告，永远最上（1 级）
//
// 窗口存放规则（规范：稳定堆对象 + 物理序=栈序）：
//   - 每个窗口是 unique_ptr 持有的堆对象，地址终生不变
//   - vector 内「物理序 = 栈序」（尾部 = 层内最上层），排序已废除
//   - bringToFront = 移动指针到层尾，不消耗 zOrder 空间
//     （仅层内置顶；玩家级窗口无法通过任何调用超越层序）
//   - 跨帧交互状态一律以实例 id 为凭证现查，禁止缓存指针
//
// 窗口生命周期（规范：帧末统一回收 + 待打开队列）：
//   createWindow（淡入）→ 交互 → closeWindow（标记 disposed + 淡出）
//   → 帧末 flushDisposed 物理删除
//   遍历/回调中需要开新窗 → pendingOpen 入队 → openPending 帧末统一创建
//
// 帧时序（规范 §9.5，由 WindowManager 驱动）：
//   处理输入 → updateFade → openPending → renderAll → renderBalls
//   → flushDisposed
// ============================================================
#include "WinCommon.h"
#include "WindowInstance.h"
#include "../../app/AppFwd.h"
#include <memory>
#include <vector>

namespace ui {

// ====== 单个层级（一组同层窗口，物理序=栈序） ======
struct Layer {
    int id = 0;                          // LayerId
    const char* name = "";               // 层名（调试用）
    bool visible = true;                 // 整层显隐（切场景）
    float alpha = 1.0f;                  // 整层透明度（乘进窗口 alpha）
    std::vector<std::unique_ptr<WindowInstance>> windows; // 尾部=层内栈顶
};

// ====== 待打开窗口请求（规范：遍历中途开窗走队列，帧末统一创建） ======
struct PendingOpen {
    int configIndex = -1;   // 配置表索引
    int layerId = -1;       // 目标层（-1 = 配置表默认层）
};

// ====== 层级栈 ======
class LayerStack {
public:
    LayerStack();  // 初始化四个预定义层级

    // ---- 自定义层级方案（初始化期设置，运行期只读） ----
    // layerIds 按"绘制从底到顶"排列（默认 {0,1,2,3} = SCENE 最底 GLOBAL 最顶）；
    // 传 nullptr / count<=0 恢复默认序。合法层 id 0..LAYER_COUNT-1，越界与重复项忽略
    static void setupLayers(const int* layerIds, int count);
    static int  layerCount();          // 当前层序长度（渲染/命中的实际层数）

    // ---- 层访问 ----
    Layer&       layer(int layerId);
    const Layer& layer(int layerId) const;

    // ---- 窗口生命周期 ----
    // 创建窗口实例：尺寸/层级/单例性全部读配置表（零硬编码）。
    // layerId 传 -1 用配置表默认层。
    // 单例窗口（无 WIN_MULTI_INSTANCE）已存在时：置顶 + 复显，返回已有 id。
    // 返回 -1 = 配置名不存在。
    // 注意：遍历窗口列表的回调中禁止直接调用，请改用 pendingOpen()
    int  createWindow(const char* configId, int layerId = -1);
    int  createWindowByIndex(int configIndex, int layerId = -1);
    // 玩家级窗口创建（Lua 插件入口）：强制 LAYER_PLAYER + 绑定场景 context
    int  createPlayerWindow(const char* configId, int sceneContext);
    // 场景切换：旧场景玩家窗整组隐藏（visible=false，实例/位置/状态保留），
    // 切回原场景整组重新显示；开发级/警告级常驻
    void setSceneContext(int sceneContext);
    int  sceneContext() const { return sceneContext_; }
    // 关闭：标记 disposed 并淡出，淡出完成后由 flushDisposed 物理删除
    void closeWindow(int windowId);
    // 置顶：层内移到栈尾（移动 unique_ptr，实例堆地址不变）
    void bringToFront(int windowId);
    // 按上下文批量销毁（切场景整组淡出，原子分组）
    void destroyContext(int contextId);
    // 关闭全部窗口
    void closeAll();
    // 整层显隐
    void setLayerVisible(int layerId, bool visible);

    // ---- 主窗口缩放跟随 ----
    // 主窗口 resize 时统一调整所有窗口（含隐藏）：尺寸按 scale 比例缩放
    // （保留用户 resize 过的相对大小，clamp 到配置 minW/minH），
    // 位置按主窗口新旧尺寸比例映射（保持相对位置）并 clamp 留边，
    // 悬浮球位置/半径同步跟随；全屏窗直接铺满新尺寸
    void rescale(float oldScale, float newScale,
                 float oldW, float oldH, float newW, float newH);

    // ---- 待打开队列（规范 §9.5：帧末 openPending 统一创建） ----
    void pendingOpen(int configIndex, int layerId = -1);
    void openPending();

    // ---- 查询 ----
    WindowInstance* findWindow(int windowId);
    WindowInstance* findWindowByConfig(const char* configId); // 单例语义查重
    // 全局栈顶：所有层中最上面的可交互窗口（CLOSE_OUTSIDE_CLICK 仅它生效）
    WindowInstance* topWindow();
    // 最上层带遮罩的窗口（输入屏障语义：overlayAlpha > 0，规范 §2.7）
    WindowInstance* topOverlayBarrier();
    std::vector<WindowInstance*> allWindows();

    // ---- 布局记忆（layout.json：位置/尺寸/置顶/固定/折叠/球态跨重启保留） ----
    // 退出时保存（模态/警告窗不入档——重启弹回模态体验差）；
    // path 相对运行目录。返回是否写档成功
    bool saveLayout(const char* path);
    // 启动时恢复：读档 → 重建窗口（跳过级联偏移直接写几何）→
    // rescale 把旧屏幕坐标映射到当前屏幕。返回恢复窗口数（-1 = 无档/读档失败）
    int  loadLayout(const char* path, float curW, float curH);
    // 一键整理：平铺（网格铺满屏幕）/ 层叠（默认尺寸 + 级联偏移），
    // 作用于所有可见面板窗（球态窗口不参与）
    void tileWindows(float screenW, float screenH);
    void cascadeWindows(float screenW, float screenH);

    // ---- 交互命中 ----
    // 悬浮球命中（最高优先，规范 §9.1）：仅 ball.active 窗口的球矩形参与
    WindowInstance* hitTestBall(const Vec2& m);
    // 面板命中测试：层从高到低、层内栈顶→栈底，返回唯一最上层命中窗。
    // 跳过：不可见/淡尽/已废弃/点击穿透/悬浮球态（球态时窗口本体不参与命中）
    WindowInstance* hitTest(const Vec2& m);

    // ---- 渲染 ----
    // 分层渲染：层从低到高，每层两趟（先非置顶后置顶）；
    // 单窗管线：全屏遮罩(若配置) → 系统chrome(阴影/四角圆角底板/标题栏)
    //          → handler内容(注册表分发) → 滚动条 → 标题栏按钮 → 缩放手柄
    void renderAll(RenderDevice* dev);
    // 悬浮球最后画，位于一切之上（规范 §7.2）
    void renderBalls(RenderDevice* dev);

    // ---- 帧推进 ----
    // 淡入淡出推进（dt：秒，速度与帧率无关）
    void updateFade(float dt);
    // 帧末清理：物理删除淡出完成的窗口，并同步清理其区域系统状态
    void flushDisposed();

private:
    // 单窗口完整渲染管线（chrome 由系统统一绘制，扩展代码只画内容）
    void renderWindow(RenderDevice* dev, WindowInstance& win, float alpha);
    void renderBall(RenderDevice* dev, WindowInstance& win);

    std::vector<Layer> layers_;
    std::vector<PendingOpen> pending_;  // 待打开队列
    int nextId_ = 1;                    // 实例 id 分配器（从 1 起，0 = 无效）
    int sceneContext_ = 0;              // 当前场景（玩家级窗口的分组依据）
};

// 旧全局单例 g_layerStack：多应用架构后由 AppFwd.h 宏转发到当前应用的
// layerStack 成员（每应用独立悬浮窗栈，彻底分离）

} // namespace ui