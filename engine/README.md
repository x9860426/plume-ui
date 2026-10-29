# Plume UI

> A featherweight C++ UI engine — 轻若鸿毛，悬浮于屏。

Plume UI 是一个自研 C++17 UI 引擎。核心理念是 **系统派发**：所有交互控件（下拉、分割条、滚动区、文本输入、悬浮窗）由引擎统一收编为唯一实现，业务代码只"声明意图"，引擎全包绘制 / 交互 / 滚动 / 裁剪 / 防穿透 / 帧末失活。

名称取自 Plume（羽毛），寓意悬浮窗架构与轻盈的交互体验。

---

## 特性

- **系统派发架构** — 下拉 / 分割 / 滚动 / 输入 / 悬浮窗五套系统统一收编，业务零交互代码
- **悬浮窗架构** — `LayerStack` + `WindowManager` 配置表驱动，新窗 = 加一行配置 + 注册 handler；支持拖拽 / 缩放 / 折叠 / 悬浮球 / 多实例 / 淡出回收 / 布局记忆
- **多应用窗口** — 一个 `AppWindow` = 一个完整独立 UI 应用（独立悬浮窗栈 / 输入路由 / 视觉参数 / GL 上下文），支持父子关系与级联销毁
- **双渲染后端** — OpenGL 3.3（完整可用）与 Vulkan（实验性，需 Vulkan SDK）；运行时 F2 一键切换，经 `AppSnapshot` 整体迁移状态（悬浮窗布局 / 拖拽态 / 视觉参数不丢）
- **34 套主题** — `themes/theme_01.json ~ theme_34.json`，JSON 描述色板，运行时 F3 切换
- **控件模板编号体系** — 每种控件「游戏ID → 绘制模板」程序映射表（`StyleTpl.h` 单源）：按钮 4 / 滑条 12 / 开关 6 / 分割线 6 / 悬浮球 5 / 滚动条 4 / 下拉 4 / 进度 5 …… 报编号即调用，新模板插中间不改旧号
- **SDF 软边抗锯齿** — 圆角 / 边框 / 阴影 / 箭头均走 SDF 着色器，厚度精确、软边 AA，主界面 / 悬浮窗 / 单窗三层独立开关
- **圆润按钮 + 多模板控件** — 按钮 3 形态（默认 / 方形 / 圆润），滑条 / 分割条 / 滚动条各多种模板，全经主题工作室实时切换
- **外部 TTF 字体** — `stb_truetype` 生成 atlas 上传 GPU，按需字符集
- **多线程渲染** — 单线程 / 双线程 / 四线程 / 全核模式可选，`RenderThread` pause/configure/resume 协议保证结构变更期 GL 上下文安全

---

## 构建

**依赖**：GLFW、glad、stb、nlohmann_json（CMake 配置见 `CMakeLists.txt`）

```bash
cmake -B build -S .
cmake --build build --config Release
```

Vulkan 后端需本机安装 [Vulkan SDK](https://vulkan.lunarg.com/)；未安装时自动降级为 stub，仅 OpenGL 可用。

> ⚠️ Vulkan 后端当前为实验性骨架，画面一致性尚未持续验证。OpenGL 后端为推荐生产路径。

**编译器**：MinGW-w64 GCC（C++17）。其他符合 C++17 的编译器理论上可用，未持续测试。

---

## 运行

```bash
build/Release/uiengine_demo.exe                 # 默认 OpenGL
build/Release/uiengine_demo.exe --vk            # 尝试 Vulkan
build/Release/uiengine_demo.exe --apps 3        # 启动 3 个独立应用窗口
build/Release/uiengine_demo.exe --open theme_studio   # 启动即打开指定悬浮窗
```

**快捷键**

| 键 | 功能 |
|---|---|
| F1 | 重置视觉参数为默认 |
| F2 | 切换渲染后端（GL ↔ VK，快照迁移不丢布局） |
| F3 | 切换主题 |
| F4 | 新建应用窗口 |
| F11 | 全屏切换 |

**命令行参数**：`--gl/--vk`、`--apps N`、`--threads 0-3`、`--open <configId>`、`--winsize WxH`、`--framemode 0/1`、`--shot <tag>`、`--shotdelay S`、`--switchafter S:mode1[:mode2...]`、`--fsafter S`、`--hidebar`

---

## 目录结构

```
plume-ui/
├── src/
│   ├── core/        主引擎：Math / Log / Event / Input / Assets
│   ├── render/      渲染抽象 + 后端
│   │   ├── RenderDevice.h / RenderTypes.h   抽象接口
│   │   ├── gl/      OpenGL 3.3 后端
│   │   └── vk/      Vulkan 后端（实验性）
│   ├── platform/    窗口与系统事件（GLFW）
│   ├── app/         多应用架构：AppWindow / AppContent / AppFwd
│   └── ui/
│       ├── Element / Font / Theme / Background / DynamicBg   绘制原语
│       ├── MainUI              主界面（配置表驱动页面）
│       ├── page/               页面处理器（Overview / Options / Sandbox / Splitter / Directory）
│       └── win/                系统派发核心
│           ├── DropdownSystem / SplitterSystem / RegionSystem / TextInputSystem
│           ├── LayerStack / WindowManager / WindowConfig / WindowInstance
│           ├── HandlerRegistry / WinControls / WindowButtons
│           ├── SandboxSystem / UiAnim / UiContext / StyleTpl
│           └── WinHandlers_*   各悬浮窗内容处理器
├── themes/          34 套主题 JSON
├── shaders/         SDF 着色器源（ui.vert / ui.frag）+ SPIR-V 产物
├── assets/          字体等资源
├── third_party/     glad 离线产物
└── CMakeLists.txt
```

---

## 架构亮点

### 系统派发

业务代码只做三件事：

```cpp
// 1. render 期：声明下拉 + 画按钮
g_dropdowns.draw(dev, win, req, alpha);
// 2. render 末：画展开浮层
g_dropdowns.renderPopups(dev, win, alpha);
// 3. 交互期：按下 / hover / 滚轮调三交互函数
g_dropdowns.handlePress(win, m);
```

引擎自动处理：按钮三态 / 展开收起 / 弹出位置自适应翻转 / 列表裁剪滚动 / 迷你滚动条 / 项 hover 高亮 / 选中项定位到可见区中部 / 吞点击防穿透 / 帧末失活 / 单开。**改选写值由系统直接 `*target = newIndex`，业务零代码。**

同一套收编模式应用于 `SplitterSystem` / `RegionSystem` / `TextInputSystem`。禁止业务自画下拉 / 分割 / 滚动 / 裁剪——这是规范铁律。

### 几何单源

页面控件矩形由各页 `layout()` 单源计算，框架不持有任何页面控件矩形。`PAGE_CONFIGS` / `BOTTOM_BTNS` / `WindowConfig` 三张配置表驱动扩展，新页面 / 新窗 = 加一条配置，框架零改动。

### 快照迁移

`AppSnapshot` 整体迁移 `LayerStack + Regions + Dropdowns + Splitters + 视觉参数 + 父子关系`。F2 切后端 / 分辨率重启 / 边框模式切换均走同一条快照链路——重启不丢用户的悬浮窗布局与拖拽态。

### 控件模板编号

`StyleTpl.h` 是全项目唯一「游戏ID → 绘制模板」映射表：

```cpp
namespace button { enum { FOLLOW=0, DEFAULT=1, SQUARE=2, ROUND=3 }; }
namespace slider { enum { FOLLOW=0, DEFAULT=1, NEON_THIN=2, ... VALUE=7, VB_RHANDLE=8, ... }; }
```

编号一律显式常量（不依赖数组下标），新模板插中间只加常量、不改旧号，旧号永不漂移。报编号即定位控件与风格。

---

## 状态

- [x] 系统派发核心（下拉 / 分割 / 滚动 / 输入 / 悬浮窗）
- [x] 多应用窗口 + 父子关系 + 快照迁移
- [x] 34 套主题 + 控件模板编号体系
- [x] OpenGL 后端完整可用
- [x] 多线程渲染（pause/configure/resume 协议）
- [ ] Vulkan 后端画面一致性验证
- [ ] 单元 / 冒烟测试
- [ ] CI

---

## 授权

本项目采用自定义 Source-Available 许可证（见 [LICENSE](LICENSE)）。**不是 OSI 定义下的严格开源许可证。**

**允许**：在个人或商业项目中使用、修改源码用于自己的产品、将修改保留为私有、随产品分发编译产物。

**禁止**：单独出售 / 出租 / 转售本引擎；将其实质性衍生版本包装成独立 UI 引擎 / SDK / 模板商业售卖；使用项目名称或作者名义误导性宣传。

完整条款以 [LICENSE](LICENSE) 为准。正式发布前建议请专业律师审阅。