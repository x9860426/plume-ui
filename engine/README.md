# 999研究UI — C++ UI 引擎

一套研究型 C++ UI 引擎，**主引擎负责计算/事件/输入**，**UI 引擎负责渲染**，支持 **双渲染后端（OpenGL / Vulkan）** 与 **外部 TTF 字体**。

## 目录结构

```
src/
├── core/        主引擎：Math/Log/Event/Input/Engine（计算与处理）
├── render/      渲染抽象层 + 后端
│   ├── RenderTypes.h / RenderDevice.h   抽象接口
│   ├── gl/      OpenGL 3.3 后端（完整可用）
│   └── vk/      Vulkan 后端（骨架，需 Vulkan SDK）
├── platform/    窗口与系统事件（GLFW）
├── ui/          UI 引擎：Font/Element/Text/Button/UIEngine
└── main.cpp     示例：双后端切换 + 文字 + 可点击按钮
```

## 构建

```bash
cmake -B build -S . -DUIENGINE_BUILD_VULKAN=ON
cmake --build build --config Release
```

依赖通过 CMake `FetchContent` 自动拉取：GLFW、glad、stb。Vulkan 需本机安装 [Vulkan SDK](https://vulkan.lunarg.com/)；未安装时自动降级为 stub，仅 OpenGL 可用。

## 外部字体

把任意 `.ttf` 放到 `assets/fonts/`，示例默认加载 `assets/fonts/font.ttf`。字符集按需生成位图图集上传 GPU。

## 运行

```bash
build/Release/uiengine_demo.exe        # 默认 OpenGL
build/Release/uiengine_demo.exe --vk   # 尝试 Vulkan
```

## 架构要点

- `RenderDevice` 为唯一渲染抽象，UI 层只产生 `Vertex/索引/纹理` 命令，与后端解耦。
- `Engine` 持有 `Window + RenderDevice + UIEngine`，主循环驱动事件分发 → 输入更新 → UI 布局 → 渲染 → present。
- `Font` 用 `stb_truetype` 从外部 TTF 生成 atlas 纹理，按字符返回 UV/尺寸，UI 文本逐字形拼 quad。
- 双后端可在运行时通过命令行参数切换，无需重编译。