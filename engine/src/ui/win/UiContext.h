// AI生成
#pragma once
// ============================================================
// 全局 UI 上下文（字体 / 缩放 / 屏幕尺寸 / 全局视觉参数）
// 初始化时由主界面注入一次；全局视觉参数由设置面板实时写入，
// 系统 chrome（底板/标题栏/阴影）只从这里读取——单源计算，
// 任何窗口模块禁止自存这些参数（规范：零硬编码、系统统一）。
//
// 视觉参数公式逐行对照参考 ui_core.cpp L236-239（原三档厚度等级已删除，
// 公式按中档基准"厚度=1"固化常量；粗细连续调节由 thicknessUser 滑条负责）：
//   BorderWidth() = (0.5 + 1×0.5) × thicknessUser × scale = 1.0 × thicknessUser × scale
//   ShadowBlur()  = 1 + 1 = 2                    （多层叠层阴影层数）
//   ShadowAlpha() = (15 + 1×15)/255 × 强度        （叠层阴影基准透明度）
//   RadiusScale() = 0.7 + 1×0.3 = 1.0 × 圆润强度  （所有圆角 = 基准值 × 该系数）
// ============================================================
#include "WinCommon.h"
#include "../../app/AppFwd.h"

namespace ui {

struct UiContext {
    Font* font = nullptr;        // 常规字体
    Font* smallFont = nullptr;   // 小字体
    Font* bigFont = nullptr;     // 大字体
    float scale = 1.0f;          // 系统缩放系数（逻辑像素 → 屏幕像素）
    int winW = 1280, winH = 720; // 当前屏幕/画布尺寸（像素）


    // ---- 视觉增强参数（主题工作室"按钮"页实时调节，全引擎即时生效；F1 重置为默认） ----
    bool  sdfAA = true;            // SDF 软边抗锯齿总开关（着色器 aaWidth 0.51↔2.0）
    bool  baseAA = true;           // 底级（主界面）层抗锯齿独立开关（关=主界面硬边，悬浮窗不受影响）
    bool  floatWinAA = true;       // 悬浮窗系统层抗锯齿全局开关（关=所有悬浮窗+悬浮球硬边；单窗可用实例 aaDisabled 覆盖）
    float radiusScaleUser = 1.0f;  // 圆润强度（0=直角，1=默认，2=极圆；乘进派生圆角）
    float shadowStrength = 1.0f;   // 阴影强度乘子（0=无阴影，1=默认）
    float thicknessUser = 1.0f;    // 厚度百分比乘子（0=无线框，1=默认，2=极粗；乘进边框线宽）

    // ---- 其他UI 风格参数（主题工作室"其他UI"页配置；F1 重置为默认） ----
    bool  checkboxShowTick = true;    // 复选框显示勾（开=绘制 SDF 对勾，关=仅色块）
    int   switchStyle = 0;            // 开关按钮风格（内部 0基：0=系统圆润 胶囊+圆滑块，1=方形 直角+方块，2=半圆润 小圆角+圆角方块，对照使命.py）
                                      // 3..4 = 「特别绘制」2 模板（游戏ID 4..5：机械/渐变）
                                      // 游戏ID：0=跟随 1=系统默认 2..5 对应上列；见 StyleTpl.h sw
    int   switchExtra = 0;            // 开关扩展绘制（0=无 1=中心微光 2=花朵 3=雪花 4=齿轮 5=星月 6=宝石变色 7=蝙蝠标记 8=星辉 9=太阳光芒 10=金属拨杆 11=猫爪印 12=镜面光泽，对照轮回/春天/冬天/时间/塔罗牌/王国/吸血鬼/星空/潮汐/工业化/猫2/液体金属.py）
    int   sliderStyle = 0;            // 滑动条绘制（内部 0基：0=默认 1=霓虹细轨 2=竖柄霓虹 3=春意描边 4=像素霓虹 5=圆润 6=值文字条）
                                      // 7..10 = 「中间带值」4 模板（游戏ID 8..11：值-圆柄/分段/霓虹/大圆），值文字整框居中
                                      // 游戏ID：0=跟随 1=系统默认 2..11 对应上列；见 StyleTpl.h slider。填充统一 accent→accent2 渐变
    int   ringStyle = 0;              // 环形进度绘制（0=系统默认进度环 1=加载环；对照 pctl::drawCircularProgress；「圆形的单独模板」）
    int   splitterStyle = 0;          // 分割线绘制（0=默认胶囊 1=细线 2=双线 3=点状 4=渐变，参考滑动条风格；SplitterSystem.draw 按此分支）
    int   ballStyle = 0;              // 悬浮球绘制（内部 0基渲染值：0=默认圆形 1=方形（圆角随 radiusScale）2..3=「特别绘制」2 模板）
                                      // 3..4 = 「特别绘制」2 模板（游戏ID 3..4：玻璃/渐变）
                                      // 游戏ID：0=跟随 1=系统默认（圆形）2=方形 3..4 对应上列；见 StyleTpl.h ball
    int   scrollbarStyle = 0;         // 滚动条绘制模板（内部 0 基渲染值：0=系统默认细轨 1=胶囊·带上下按钮 2=胶囊·无按钮；
                                      // 游戏ID：0=系统跟随 1=系统默认 2=胶囊带按钮 3=胶囊无按钮，见 StyleTpl.h scrollbar；
                                      // RegionSystem 按此单源取轨道宽度/按钮，并据此为内容自动让位 contentW）
    int   btnStyle = 0;               // 按钮绘制模板（内部 0基渲染值：0=系统默认 1=方形 2=圆润）
                                      // 游戏ID：0=系统跟随 1=系统默认 2=方形 3=圆润（内部=ID-1，见 StyleTpl.h）
    int   checkStyle = 0;             // 复选框勾绘制模板（内部 0基：0=系统默认（原折线勾）1=参考帧勾）
                                      // 游戏ID：0=系统跟随 1=系统默认 2=参考帧勾（见 StyleTpl.h check）
    int   dropdownStyle = 0;          // 下拉按钮绘制模板（内部 0基：0=系统默认（单块+三角）1=圆角胶囊 2=空心描边）
                                      // 游戏ID：0=系统跟随 1=系统默认 2=圆角胶囊 3=空心描边（见 StyleTpl.h dropdown）
                                      // DropdownSystem 按此单源分支；扩展代码禁止自绘下拉按钮。
    // 「其余控件」绘制模板（区域⑤ 行尾下拉写回；当前均单一样式，体系预留：新变体只加常量、不改旧号）
    int   tabStyle = 0;               // 页签项（pctl::drawTabItem）游戏ID：0=跟随 1=系统默认
    int   sideStyle = 0;              // 侧栏项（pctl::drawSideItem）
    int   segStyle = 0;               // 分段按钮（pctl::drawSegItem）
    int   stepperStyle = 0;           // 计数器（pctl::drawStepper）
    int   inputStyle = 0;             // 输入框（wctl::drawInput）
    int   progressStyle = 0;          // 条形进度（wctl::drawProgress 单源分发）：内部 0=默认胶囊 1..3=「特别绘制」3 模板
                                      // 游戏ID：0=跟随 1=系统默认 2..4 对应上列（分段/霓虹/斜纹）；见 StyleTpl.h progress
    // ---- 文本系统兜底（系统派发：字体行高超出控件可用高 → 自动等比缩小）----
    // 由 wctl::uiTextFit 单一实现消费；wctl/pctl 全部文本原语统一走它。
    bool  textAutoFit    = true;      // 文本超界自动缩小总开关（关 = 按原样绘制，可能超界）
    bool  textAutoFitLog = true;      // 自动缩小时在终端报告"哪里字体超了、系统已暴力兜底"
    float uiScaleUser = 1.0f;         // 全局页面缩放倍率（0.8=80% ~ 1.5=150%，滑条松手提交；乘进 scale_，主界面/悬浮窗/字号/悬浮球全等比跟随，MainUI 检测变化自动 resize）
    bool  circGrad = true;            // 圆形进度条渐变色（开=沿弧 accent→accent2 渐变，关=纯 accent）
    int   renderRes = 0;              // 全局渲染分辨率（0=跟随窗口 1=360p 2=480p 3=720p 4=1080p 5=2K 6=4K；UI 按逻辑分辨率布局，画面整体缩放到实际窗口）
    bool  renderResDirty = false;     // 分辨率变更标记（main 循环检测后重启窗口生效；F1 重置同样触发）
    // ---- 多线程渲染（进程级 RenderThread 单例，帧末生效） ----
    // 0=单线程直通（计算+渲染同线程，最稳）
    // 1=双线程：计算线程（事件/逻辑/录制）+ 渲染线程（回放+present）
    // 2=四线程：双线程 + 2 工作线程（可并行 CPU 密集段）
    // 3=全部线程：双线程 + (硬件核心-2) 工作线程
    // dirty 电平语义与 renderResDirty 一致：主题工作室置位 → main 帧末
    // pause 段内 configure 生效（排空管线后切换，无半途帧）
    int   threadMode = 0;
    bool  threadModeDirty = false;

    // ---- 应用窗口边框模式（系统边框 / 完全自绘）----
    // 0 = 系统原生标题栏（- 口 X 由 WM 绘制，默认）
    // 1 = 完全自绘：无边框窗口 + 引擎自画 - 口 X/标题（AppWindow 应用标题栏）
    // 切换必须重启整个应用窗口渲染：GLFW_DECORATED 只在创建瞬间生效，
    // 运行期无法改样式 → 置 windowFrameDirty，宿主快照重启全部应用
    // （与 F2 切后端同一套快照迁移链路，悬浮窗布局不丢；GL/VK 双后端
    // 走同一条窗口创建通路，双渲染天然一致）
    int   windowFrame = 0;
    bool  windowFrameDirty = false;
    // 自绘标题栏占位高（逻辑像素）：由 AppWindow 按当前 scale 写入，
    // MainUI 布局整体下移让位（单源：只有这里定义"内容从哪里开始"）；
    // 0 = 系统边框模式（内容从 0 开始，与原行为完全一致）
    float appTitleBarH = 0.0f;

    // ---- 应用窗口圆润值（其他页「应用窗口」区滑条；自绘模式下落地）----
    // 0 = 直角，1 = 默认（= WINDOW_ROUND_BASE 逻辑像素），2 = 极圆。
    // AppWindow 帧内用 Window::setRoundCorner(值×基准×物理/逻辑比) 把无边框
    // 窗口外角按此裁圆；随应用快照迁移（切换/重启后保持）。
    float windowRound = 1.0f;
    static constexpr float WINDOW_ROUND_BASE = 16.0f;  // 圆润值 1.0 对应的逻辑像素半径

    // ---- 全屏（F11）隐藏顶部 UI（其他页「应用窗口」区开关）----
    // 开启后：F11 全屏状态下自绘标题栏（- □ X）让位为 0 → 内容铺满整屏，
    // 顶部不再压着一层 UI。仅无边框自绘模式有意义（系统边框模式本就无自绘顶栏）。
    // 随应用快照迁移；一开一关即时生效（AppWindow 每帧同步 appTitleBarH）。
    bool fullscreenHideBar = false;

    // ---- 快捷键开关位掩码（默认全开 = 现行为不变） ----
    // 按需清位 = 对应快捷键失效（嵌入场景保留部分键给宿主时用；
    // MainUI 与 AppWindow 统一查此单源）
    enum HotkeyBits : uint32_t {
        HOTKEY_F1_RESET       = 1u << 0,   // F1 重置视觉参数（MainUI 判定）
        HOTKEY_F2_BACKEND     = 1u << 1,   // F2 切渲染后端（AppWindow 判定）
        HOTKEY_F3_THEME       = 1u << 2,   // F3 切主题（MainUI 判定）
        HOTKEY_F4_NEW_APP     = 1u << 3,   // F4 新应用窗口（AppWindow 判定）
        HOTKEY_F11_FULLSCREEN = 1u << 4,   // F11 全屏切换（AppWindow 判定）
    };
    uint32_t hotkeyMask = 0xFFFFFFFFu;
    bool  rememberLayout = false;     // 悬浮窗布局记忆总开关（默认关：启动不恢复、退出不保存，悬浮窗统统默认关闭；
                                      // 仅代码显式开启时 layout.json 读写生效——重启后自动开窗属极少需求）

    // ---- 运行时统计（主循环每秒采样写入，调试窗读取展示） ----
    unsigned frameId = 0;       // 帧号（WindowManager::handleInput 帧首自增；
                                // 调试/录制序号用。周期相位请走真实时间 dt，勿用帧号）
    int fps = 0;                  // 每秒帧率（参考 g_fps）
    float frameMs = 0.0f;         // 本帧耗时（毫秒，主循环实测）
    float frameMsHist[64] = {};   // 帧耗时历史（环形缓冲，调试窗性能页曲线）
    int frameMsHead = 0;          // 环形写指针
    int frameMsCount = 0;         // 已积累样本数（≤64，均值只算已写入部分）

    // ---- 输入/主界面状态镜像（调试窗「输入」页展示；WindowManager/MainUI 每帧写入单源） ----
    float mouseX = 0, mouseY = 0; // 最近鼠标位置（屏幕像素，参考 GetCursorPos）
    int mainTabSel = 0;           // 主界面当前标签页（参考 g_tabSel）
    

    // ---- 派生视觉参数（全引擎唯一计算点，公式对照参考 ui_core.cpp，
    //      中档基准"厚度=1"已固化进常量；连续调节走 thicknessUser/radiusScaleUser） ----
    // 边框线宽：基准 1.0 逻辑像素 × 厚度百分比乘子 × 系统缩放
    float borderWidth()  const { return 1.0f * thicknessUser * scale; }
    // 阴影层数：多层叠层阴影的层数（中档基准 1+1）
    float shadowBlur()   const { return 2.0f; }
    // 阴影基准透明度（0-1）：中档基准 30/255 × 阴影强度
    float shadowAlpha()  const { return (30.0f / 255.0f) * shadowStrength; }
    // 圆角系数：中档基准 1.0 × 圆润强度，所有圆角 = 基准值 × 该系数
    float radiusScale()  const { return radiusScaleUser; }
};

// 旧全局单例 g_uiCtx：多应用架构后由 AppFwd.h 宏转发到当前应用的
// ctx 成员（每应用独立的尺寸/缩放/视觉参数，彻底分离）

} // namespace ui