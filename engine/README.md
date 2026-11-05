# Plume UI — C++ 主引擎

Plume UI 的 C++ 引擎线（`engine/`）。项目总览、特性、模板编号体系与授权说明见仓库根目录 [README](../README.md)，本文件只讲引擎的构建、运行与工程约定。

## 构建

依赖：CMake ≥ 3.16、C++17 编译器（MinGW-w64 / MSVC 均可）。第三方依赖经 `DEPS_CACHE` 离线目录接入（需包含 `glfw-src` / `stb-src` / `nlohmann_json-src` 三个源码目录），glad 已随 `third_party/` 附带。Vulkan 未装 SDK 时自动降级为 stub，仅 OpenGL 可用。

```powershell
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

常用参数：`--apps N`（初始应用数）、`--threads 0..3`（线程模式）、`--framemode 0/1`（系统边框/完全自绘）、`--hidebar`（全屏隐藏顶栏）、`--winsize WxH`（强制逻辑尺寸）、`--open <configId>`、`--shot <tag>` / `--shotdelay N`、`--switchafter S:m1[:m2]`、`--frametoggleafter S`、`--fsafter S`。完整表格见根 README。

## 热键（应用级键路由表）

| 键 | 动作 |
|---|---|
| F1 | 重置视觉参数（main 页） |
| F2 | 运行时切渲染后端（OpenGL ⇄ Vulkan） |
| F3 | 循环切换主题 |
| F4 | 新建应用窗口 |
| F11 | 全屏切换 |
| Esc | 关闭栈顶悬浮窗 |

## 目录结构

```text
engine/
├── CMakeLists.txt
├── src/
│   ├── main.cpp            多应用主循环 + 命令行入口
│   ├── app/                多应用窗口 / 内容处理器 / 键路由建表
│   ├── core/               Math / Log / Event / Input / Assets
│   ├── platform/           GLFW 窗口封装
│   ├── render/             RenderDevice 抽象 + gl/ + vk/ + 多线程调度
│   └── ui/                 MainUI 框架 + page/ 页面系统 + win/ 悬浮窗系统
├── themes/                 主题 JSON（启动热加载）
├── assets/                 字体等资源
├── shaders/                UI 着色器（GLSL + SPIR-V）
└── third_party/            glad 离线生成
```

## 工程约定（改代码前必读）

- **单位契约**（`ui/win/UiContext.h` 顶部）：`scale` 是逻辑像素 → 屏幕像素唯一换算因子；`WindowInstance.x/y/w/h` 为屏幕像素；`WindowConfig` 尺寸字段为逻辑像素；`RegionRequest`/扩展绘制为屏幕绝对坐标；页面布局基准缩放取 `pageScale(h)` 单源。
- **系统派发**：滚动/裁剪/滚动条/下拉/分割/沙盘/热键集中在各 `*System` 模块，扩展代码只声明、不自绘自判。
- **模板编号**：控件绘制一律带 `tpl` 尾参（0=跟随全局，1=系统默认，2..N=内置变体），注册表单源在 `ui/win/StyleTpl.h`，新增模板只加常量不改旧号。
- **生命周期**：窗口创建/销毁/切后端/切线程模式前必须 `ThreadScheduler::pause()`（排空管线），操作完 `resume()`；OpenGL 强制单线程，Vulkan 可多线程。
- 引擎内 `*.md`（记忆/升级清单/同步清单）为开发过程文档，记录每次引擎级修复与两侧同步状态，改动公共文件后建议跑一次差异审计。

## 状态

当前引擎源码与主引擎工作目录 `988-27.0` 同步（89 个源文件），构建验证：Release/Debug 零错误零警告，`--shot` 冒烟正常。
