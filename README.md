# Plume UI 🐦

Plume UI 是一个轻盈、优雅的 C++17 UI 引擎。名称取自 Plume（羽毛），寓意悬浮窗与轻盈交互体验。

引擎聚焦**悬浮窗体系**与**系统派发**：扩展代码只做"声明 + 回调"，滚动、裁剪、滚动条、下拉、分割、沙盘、键路由、图片解码、窗口 chrome 全部由系统统一处理。

**当前状态：引擎已可完整运行并持续更新**——双渲染后端（OpenGL 3.3 / Vulkan）均已落地，`main` 分支正常构建、运行，提交按需持续推送。项目仍处于演进期：API 与目录结构可能调整，LICENSE 尚未定稿，暂未打正式版本号。

## 特性

- **双渲染后端（OpenGL 3.3 / Vulkan）**：`RenderDevice` 唯一渲染抽象，UI 层只产生 Vertex/索引/纹理命令；运行时 **F2 热切换**，全部应用窗口快照迁移、悬浮窗布局不丢。
- **多应用架构**：一个 `AppWindow` = 一个完整独立的 UI 应用（独立悬浮窗栈 / 输入路由 / 区域滚动 / GL 上下文 / 视觉参数）；支持**主子级应用窗口**（父关子关、级联摆放、可禁悬浮窗）；F4 新建、X/Esc 关闭，全部关闭即退出。
- **悬浮窗体系**：`WindowManager` + `LayerStack` + `RegionSystem` 系统派发——窗口底板/标题栏/关闭键/悬浮球统一绘制；滚轮容器自动滚动、裁剪、胶囊滚动条；窗口支持收起为悬浮球、淡入淡出、停靠。
- **系统派发控件系统**：`DropdownSystem`（下拉）、`TextInputSystem`（输入框/数字键盘/身份证输入器）、`SplitterSystem`（分割条拖拽）、`SandboxSystem`（无限画布 pan+zoom+网格+LOD）、`ImageSystem`（图片解码/重采样/设备维度缓存）、`KeyRouter`（键路由）。
- **UI 控件模板编号体系（`StyleTpl.h` 单源）**：17 个控件族、62 个内置绘制模板。**报编号即点名模板**——`0=系统跟随`、`1=系统默认`、`2..N=内置变体`；编号一律显式常量，新模板可"中间插入"，旧号永不漂移。
- **主题系统**：35 组主题槽位（`themes/theme_XX.json` 34 套启动热加载）；**主题工作室**可视化编辑颜色与视觉增强参数（SDF 抗锯齿、圆润强度、阴影强度、边框厚度、全局页面缩放、渲染分辨率），主题/参数即时生效、F1 一键重置。
- **多线程渲染（`ThreadScheduler` 进程级单例）**：0 单线程直通 / 1 双线程 / 2 四线程 / 3 全核（硬件核心-2）；三缓冲帧包池、自适应工作线程、parallelFor 并行分片；OpenGL 强制单线程、Vulkan 可多线程；运行时可下拉热切换（排空管线后生效）。
- **窗口边框模式**：系统原生标题栏 / **完全自绘无边框**（引擎自画 - □ X、窗口圆角、F11 全屏隐藏顶栏）。
- **键路由 KeyRouter**：热键唯一判定入口，作用域三档（全局 / 指定 1 底 / 限制 N 底），文本输入焦点优先让位，**无隐式全局兜底**；建表即审计（全局键必须写明理由）。
- **字体系统**：外部 TTF（stb_truetype）按需烘焙位图图集；图集满保护（不再每帧 4096² 全量重建）、文本超界自动等比缩小、行高兜底。
- **统一单位契约**：`scale` 是逻辑像素 → 屏幕像素唯一换算因子；`WindowInstance` 屏幕像素 / `WindowConfig` 逻辑像素 / `RegionRequest` 屏幕绝对坐标 / `pageScale(h)` 页面缩放单源。
- **调试窗**：帧率、帧耗时历史曲线（环形缓冲）、输入状态镜像、渲染线程性能统计（回放耗时 / acquire 反压 / 流水线深度 / 工作线程利用率）。

## 架构

项目分为两条并行线：

- `engine/`：C++ 主引擎（89 个源文件）。面向正式项目，强调性能、稳定性和严格的生命周期管理。
- `bindings/python/`：Python 调用层。优先通过 pybind11 调用 C++ 主引擎；未编译原生扩展时自动回退到轻量纯 Python 实现，适合工具、脚本和原型。

分支：

- `main`：汇总分支
- `cpp-engine`：C++ 主引擎开发线
- `python-binding`：Python 绑定与轻量调用层开发线

### 引擎模块划分

```text
engine/
├── CMakeLists.txt          # C++17；依赖经 DEPS_CACHE 离线目录接入
├── src/
│   ├── main.cpp            多应用主循环 + 命令行入口（--vk/--apps/--threads/--shot …）
│   ├── app/                AppWindow / AppContent：多应用窗口、内容处理器、键路由建表
│   ├── core/               Math / Log / Event / Input / Assets
│   ├── platform/           GLFW 窗口封装（尺寸/圆角/全屏/边框模式）
│   ├── render/             RenderDevice 抽象 + gl/ + vk/ + RenderThread/ThreadScheduler
│   └── ui/                 MainUI 主界面框架 + page/ 页面系统 + win/ 悬浮窗系统
│       ├── win/            WindowManager · LayerStack · RegionSystem · DropdownSystem ·
│       │                   TextInputSystem · SplitterSystem · SandboxSystem ·
│       │                   ImageSystem · KeyRouter · HandlerRegistry · WindowButtons ·
│       │                   WinControls · StyleTpl（模板编号单源）· UiContext（单位契约/视觉参数）
│       └── page/           PageConfig（配置表驱动）· PageHandler（处理器注册表）· 五个页面
├── themes/                 主题 JSON（启动热加载，主题工作室可导入导出）
├── assets/                 字体等资源（assets/fonts/ 放任意 TTF）
├── shaders/                UI 着色器（GLSL 源码 + SPIR-V）
└── third_party/            glad 离线生成（gl core 3.3）
```

设计要点：

- **配置表驱动**：主界面页面（`PageConfig`）、悬浮窗（`WindowConfig`）、内容处理器（`AppContentHandler`）都由配置表 + 处理器注册表驱动——新增页面/窗口 = 追加配置 + 注册处理器，框架零改动。
- **系统派发**：滚动/裁剪/滚动条/下拉/分割/沙盘/热键判定集中在各 `*System` 模块，扩展代码禁止自绘自判。
- **快照迁移**：悬浮窗实例不含 GL 资源（纯绘制数据），跨上下文/跨后端整体 move 安全——切后端、切分辨率、切边框模式都是"打快照 → 销毁 → 按快照重建"，用户布局不丢。
- **每个应用独立 GL 上下文**：多应用彻底隔离，零跨上下文陷阱。

## 构建

依赖：CMake ≥ 3.16、C++17 编译器（MinGW-w64 / MSVC 均可）、Ninja（可选）。第三方依赖（GLFW 源码、stb、nlohmann/json）通过 `DEPS_CACHE` 目录离线接入，glad 已随 `engine/third_party/` 附带；Vulkan 需本机安装 [Vulkan SDK](https://vulkan.lunarg.com/)，未安装时自动降级为 stub，仅 OpenGL 可用。

```powershell
# DEPS_CACHE 需包含 glfw-src / stb-src / nlohmann_json-src 三个源码目录
cmake -G Ninja -S engine -B build/engine `
      -DCMAKE_BUILD_TYPE=Release `
      -DDEPS_CACHE=E:/工作区/_deps_cache
cmake --build build/engine
```

MSVC 命令行构建：

```powershell
cmake -S engine -B build/engine -DDEPS_CACHE=E:/工作区/_deps_cache
cmake --build build/engine --config Release
```

## 运行

```powershell
build/engine/uiengine_demo.exe                      # 默认 OpenGL
build/engine/uiengine_demo.exe --vk                 # 尝试 Vulkan
build/engine/uiengine_demo.exe --open theme_studio --shot demo   # 启动即开悬浮窗 + 引擎内自动截图
```

命令行参数：

| 参数 | 说明 |
|---|---|
| `--vk` / `--vulkan` · `--gl` / `--opengl` | 指定渲染后端 |
| `--apps N` | 初始应用窗口数（默认 1） |
| `--threads 0..3` | 初始线程模式（单线程/双线程/四线程/全核） |
| `--framemode 0/1` | 窗口边框模式：系统边框 / 完全自绘 |
| `--hidebar` | 预置「全屏（F11）隐藏顶部 UI」开关 |
| `--winsize WxH` | 启动后强制窗口逻辑尺寸（走完整 resize 适配链） |
| `--open <configId>` | 启动即打开指定悬浮窗（验证/截图入口） |
| `--shot <tag>` | 延时后引擎内自动拍照落盘 `shot-<tag>.png` |
| `--shotdelay N` | 拍照延时秒（默认 5） |
| `--switchafter S:m1[:m2…]` | 运行到 S 秒按序切换线程模式（验证 pause/configure/resume 链路） |
| `--frametoggleafter S` | S 秒后翻转边框模式（验证快照重启链路） |
| `--fsafter S` | S 秒后进入全屏 |

内置热键（应用级键路由表，作用域见代码注释）：

| 键 | 动作 |
|---|---|
| `F1` | 重置视觉参数到默认（main 内容页） |
| `F2` | 运行时切换渲染后端（OpenGL ⇄ Vulkan，快照迁移全部应用；未编译 Vulkan 时忽略） |
| `F3` | 循环切换主题（cycleTheme 单源） |
| `F4` | 新建应用窗口 |
| `F11` | 全屏切换 |
| `Esc` | 关闭栈顶悬浮窗（文本编辑中的 Esc 优先归输入框放弃编辑） |

## 内置窗口与页面

悬浮窗（配置表驱动，共 15 个）：主题工作室、颜色选择器、数字键盘、身份证输入器、设置、登录、关于、警告、侧边设置、停靠-LR、停靠-TB、调试、分割器测试、沙盘测试、悬浮球测试。

主界面 5 页：概览（输入框/下拉/步进器/分段按钮）、**UI 模板**（控件模板编号总览 + 下拉切换）、分割（分割器拖拽样板）、沙盘（无限画布 pan+zoom+LOD）、其他（滑条/开关/复选/应用窗口与渲染设置）。

## UI 控件模板编号（StyleTpl 单源）

规则：`0=系统跟随`（不点名 = 跟随全局默认）；`1=系统默认`；`2..N=内置变体`。新模板只加常量、不改旧号。当前共 **17 个控件族、62 个内置模板**：

| 控件族 | 内置模板（编号 ≥ 1） |
|---|---|
| 按钮 | 1 系统默认 · 2 方形 · 3 圆润 |
| 滑动条 | 1 系统默认 · 2 霓虹细轨 · 3 竖柄霓虹 · 4 春意描边 · 5 像素霓虹 · 6 圆润 · 7 值文字条 · 8 值-圆柄 · 9 值-分段 · 10 值-霓虹 · 11 值-大圆 |
| 环形进度 | 1 进度环 · 2 加载环 |
| 开关 | 1 系统默认 · 2 方形 · 3 半圆润 · 4 机械 · 5 渐变 |
| 开关图案 | 1 无 · 2 中心微光 · 3 花朵 · 4 雪花 · 5 齿轮 · 6 星月 · 7 宝石变色 · 8 蝙蝠标记 · 9 星辉 · 10 太阳光芒 · 11 金属拨杆 · 12 猫爪印 · 13 镜面光泽 |
| 分割线 | 1 系统默认 · 2 细线 · 3 双线 · 4 点状 · 5 渐变 |
| 悬浮球 | 1 圆形 · 2 方形 · 3 玻璃 · 4 渐变 |
| 沙盘 | 1 有限沙盘 · 2 无限沙盘 |
| 滚动条 | 1 细轨 · 2 胶囊·带上下按钮 · 3 胶囊·无按钮（默认） |
| 下拉按钮 | 1 系统默认 · 2 圆角胶囊 · 3 空心描边 |
| 条形进度 | 1 系统默认 · 2 分段 · 3 霓虹 · 4 斜纹 |
| 复选框 | 1 系统默认 · 2 参考帧勾 |
| 页签 / 侧栏项 / 分段按钮 / 计数器 / 输入框 | 1 系统默认 |

## 工程铁律（扩展代码必读）

- 同一窗口内**下拉 id 必须唯一**（系统已兜底：重复 id 自动派生 `name#2…`，但不应依赖）。
- 带滑条 / 分割线的窗口**必须成对注册 `drag` + `release`**，否则"点一下能改值、按住拖不动"。
- 显示图片一律走 `ImageSystem`（缓存键含设备维度，设备重建即清缓存）。
- 热键一律登记进 `KeyRouter` 注册表，禁止在页面/处理器里散写 `keyPressed`。
- 单位契约：`WindowInstance` 是屏幕像素，`WindowConfig` 尺寸字段是逻辑像素；扩展绘制坐标系统不乘 `scale`。
- 扩展内容排到标题行右侧时，右边界取 `wbtn::rightButtonsWidth()`，不要手算避让量。
- 滚轮容器内布局用 `contentW`（系统按滚动条模板实时让位），不要用视口全宽。

## 授权说明（重要）

本项目拟采用自定义 Source-Available 许可证，不是 OSI 定义下的严格开源许可证。

允许：

- 在个人或商业项目中使用
- 修改源码并用于自己的产品
- 将修改后的代码保留为私有
- 随产品分发编译后的产物

禁止：

- 单独出售、出租、转售本 UI 引擎
- 将本 UI 引擎或其实质性衍生版本包装成独立 UI 引擎/SDK/模板进行商业售卖
- 使用项目名称或作者名义进行误导性宣传

完整条款以 [LICENSE](LICENSE) 为准。当前许可证为草案，正式发布前建议请专业律师审阅。

## 状态

Plume UI 目前**已不是"跑通第一个窗口"的初期阶段**：引擎整体可运行、可持续开发，下列能力均已在 `main` 落地并随提交持续更新。

已完成 / 可用：

- **双渲染后端**：OpenGL 3.3 与 Vulkan 两套 `RenderDevice` 实现均可用，运行时 F2 热切换；未安装 Vulkan SDK 时自动降级为 stub（仅 OpenGL）。
- **多应用窗口体系**：主界面 5 页 + 15 个配置表驱动的悬浮窗，独立上下文、独立输入路由。
- **系统派发控件系统**：下拉 / 输入框 / 分割 / 沙盘 / 图片 / 键路由全部收编为系统统一处理。
- **控件模板编号体系**：17 控件族、62 个内置模板，`StyleTpl.h` 单源。
- **主题系统**：34 套主题启动热加载 + 主题工作室可视化编辑。
- **多线程渲染调度**：`ThreadScheduler` 0/1/2/3 档运行时切换。
- **Python 调用层**：pybind11 绑定 + 未编译时的纯 Python 回退实现。

持续进行中：

- [ ] Python 绑定与 C++ 主引擎的接口对齐和版本化
- [ ] 文档整理（API 参考、移植手册归并进 `docs/`）
- [ ] 示例与截图、Release 打包
- [ ] LICENSE 定稿（当前为自定义 Source-Available 草案，正式发布前建议律师审阅）
