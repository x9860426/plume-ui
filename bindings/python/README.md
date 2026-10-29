# Python Binding

Plume UI 的 Python 调用层，目标是轻量、快速接入，适合工具、脚本、原型和自动化场景。

## 两种模式

1. 原生模式: 通过 pybind11 调用 C++ 主引擎，适合需要性能和完整生命周期的项目。
2. 轻量模式: 未编译原生扩展时自动回退到纯 Python 实现，启动快，但能力、性能和严格程度低于 C++ 主引擎。

## 安装

```powershell
python -m pip install -e bindings/python
```

## 快速使用

```python
from plume import Engine, EngineConfig

engine = Engine()
engine.initialize(EngineConfig(width=960, height=540))
print(engine.version())
engine.shutdown()
```

也可以直接运行：

```powershell
python bindings/python/examples/quickstart.py
```

## 说明

Python 层不会替代 C++ 主引擎。正式项目建议使用原生绑定；轻量模式主要用于快速调用和原型开发。
