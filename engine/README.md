# C++ Main Engine

Plume UI 的 C++ 主引擎，面向正式项目、性能敏感场景和长期维护。

## 特点

- C++20
- CMake 构建
- 明确的初始化 / 运行 / 关闭生命周期
- 为后续窗口、渲染、布局、事件和组件系统预留稳定 C++ API

## 构建

```powershell
cmake -S engine -B build/engine
cmake --build build/engine --config Release
ctest --test-dir build/engine -C Release --output-on-failure
```

## 当前状态

这是第一版可编译骨架，只包含引擎生命周期与配置对象。窗口系统、渲染后端和 UI 组件将在后续迭代加入。
