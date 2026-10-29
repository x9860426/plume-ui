# Plume UI

Plume UI 是一个轻盈、优雅的 UI 引擎。名称取自 Plume（羽毛），寓意悬浮窗与轻盈交互体验。

## 目标

- 提供可复用的 UI 引擎与组件能力
- 支持在商业产品中使用
- 允许使用者保留自己的业务代码与修改不公开
- 禁止将本 UI 引擎本身作为独立商品出售

## 架构

项目分为两条并行线:

- `engine/`: C++ 主引擎。面向正式项目，强调性能、稳定性和严格的生命周期管理。
- `bindings/python/`: Python 调用层。优先通过 pybind11 调用 C++ 主引擎；未编译原生扩展时自动回退到轻量纯 Python 实现，适合工具、脚本和原型。

分支:

- `main`: 汇总分支
- `cpp-engine`: C++ 主引擎开发线
- `python-binding`: Python 绑定与轻量调用层开发线

## 授权说明 (重要)

本项目拟采用自定义 Source-Available 许可证，不是 OSI 定义下的严格开源许可证。

允许:

- 在个人或商业项目中使用
- 修改源码并用于自己的产品
- 将修改后的代码保留为私有
- 随产品分发编译后的产物

禁止:

- 单独出售、出租、转售本 UI 引擎
- 将本 UI 引擎或其实质性衍生版本包装成独立 UI 引擎/SDK/模板进行商业售卖
- 使用项目名称或作者名义进行误导性宣传

完整条款以 [LICENSE](LICENSE) 为准。当前许可证为草案，正式发布前建议请专业律师审阅。

## 开发

### C++ 主引擎

```powershell
cmake -S engine -B build/engine
cmake --build build/engine --config Release
ctest --test-dir build/engine -C Release --output-on-failure
```

### Python 调用层

```powershell
python -m pip install -e bindings/python
python bindings/python/examples/quickstart.py
```

未安装原生扩展时，Python 会自动使用轻量回退实现。

## 状态

- [x] 本地仓库初始化
- [x] 确定项目名称
- [x] 创建 GitHub 远程仓库
- [x] 确定技术栈与目录结构
- [x] 建立 C++ 主引擎骨架
- [x] 建立 Python 调用层骨架
- [ ] 完成首个可运行 UI 窗口

## 目录

```text
plume-ui/
  engine/                  C++ 主引擎
    include/plume/         C++ 公共头文件
    src/                   引擎实现
    tests/                 测试
  bindings/python/         Python 绑定与轻量 API
    plume/                 Python 包
    src/                   pybind11 原生绑定
  docs/                    设计与文档
  README.md
  LICENSE
  .gitignore
```
