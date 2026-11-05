# UI 控件模板编号体系 — 实施计划（重建版）

> 说明：原计划文件 `20261028-171200-936-ui-template-numbering.md` 已丢失（全盘未找到）。
> 本文件按已锁定的设计重建，作为后续实施与验收的**单一依据**（规范：文件即记忆）。
> 编写：小通/OfficeAce · 2026-10-28

---

## 0. 用户诉求（原话要点）

- 要一个页面**显示所有 UI**，每个 UI 调用有**自己的名字**；左边设置按钮，右边**按钮名字 + 编号**。
- 以后告 AI "我喜欢哪个风格、要调用哪个"，**只报编号即可**。
- 以后添加新模板要能**中间插入**，不能乱。
- 明确是 **UI 控件模板**（按钮/滑条/开关这些绘制风格变体），**不是主题色模板**。
- 不新增 tab，**大改现有 5 页**（概览/目录/分割/沙盘/其他）。

## 1. 已锁定的三条硬规则

1. **无编号 = 跟随全局默认**（调用不传 `tpl`，即 `tpl = -1` → 用 `g_uiCtx.<控件>Style`）
2. **带编号 = 系统内置绘制模板**，可点名调用
3. **给每一个 UI 绘制取名编号**；**全项目统一 1 = 系统默认**

补充：**编号一律用显式常量**（不依赖数组下标/声明顺序），新模板插中间只加常量、不改旧号。

## 2. 设计一句话

给每个控件绘制函数加**尾参数 `int tpl = -1`**：`-1` 跟随全局，`1..N` = 系统内置模板。
编号注册表集中在 `src/ui/win/StyleTpl.h`（全项目唯一单源）。

---

## 3. 本会话已完成（均随源码落盘，可编译验证）

| 项 | 文件 | 状态 |
|----|------|------|
| 修构建基线：`VKDevice.cpp` 守卫缺口（`findMemoryType` 漏在 `#if UI_VK_ON` 外；stub 分支缺 `setSDFParams`） | `src/render/vk/VKDevice.cpp` | ✅ |
| 编号注册表（按钮/滑条/开关/开关扩展/分割线/悬浮球，编号+中文名表） | `src/ui/win/StyleTpl.h`（新增） | ✅ |
| 全局按钮模板字段 `btnStyle`（默认 1） | `src/ui/win/UiContext.h` | ✅ |
| F1 重置补 `btnStyle = 1` | `src/ui/MainUI.cpp` | ✅ |
| `drawBtn`/`drawBtnSecondary` 加尾参数 `tpl = -1`，实现 1=系统默认 / 2=方形 / 3=圆润 | `src/ui/win/WinControls.h` / `.cpp` | ✅ |

Release 编译通过（`ninja` 退出码 0，产出 `uiengine_demo.exe`）。

---

## 4. 待办（文件级清单）

### 4.1 其余控件重编号到 1 基（统一"1=系统默认"）
现行内部为 0 基（0=默认），对外编号统一 1 基，用 `StyleTpl.h` 的 `toInternal/toPublic` 换算：

- `src/ui/win/UiContext.h`：`sliderStyle/switchStyle/switchExtra/splitterStyle/ballStyle` 默认值 0→1（或保持 0 基存储 + 对外 +1，二选一，需先定）
- `src/ui/win/WinControls.cpp`：`drawSlider`（滑条 0..4→1..5）、`drawToggleOn`/`drawSwitchExtra`（开关/扩展）分支值 +1
- `src/ui/page/PageControls.cpp`：`switchStyle` 比较、`sliderStyle != 0` 等 +1
- `src/ui/win/LayerStack.cpp`：`ballStyle == 1`（方形）→ `== 2`
- `src/ui/win/SplitterSystem.cpp`：`splitterStyle` 分支 +1
- `src/ui/win/WinHandlers_ThemeStudio.cpp`：七行下拉表的 `curVal`/`targets` 与候选文案数组值 +1
- `src/ui/MainUI.cpp`：F1 重置同步

### 4.2 页面重构（不新增 tab，改现有 5 页）
每个页面：**左设置按钮 / 右"控件名 + 编号" / 一个下拉调节扩展绘制**。
优先落 `目录` 页（按钮页），列出 `tpl::button::ITEMS` 全部模板（1 系统默认 / 2 方形 / 3 圆润），下拉切换写回 `g_uiCtx.btnStyle`（走 DropdownSystem 系统派发，禁止自绘下拉）。

### 4.3 验证
- Release + Debug 双编译零错误零警告
- 启动冒烟：主界面正常、fps 数量级正常
- 对照规范自查：无裸数值、无自绘滚动/裁剪/下拉
- 删除类改动全局搜索零残留

---

## 5. 构建环境（重要，本项目被移动过，路径含中文）

- 编译器：`C:\Users\mn\mingw64\bin\g++.exe`（MinGW-w64 GCC 16.2.0，规范唯一指定）
- **中文路径坑**：工程路径含中文+空格，ninja 与 gcc 走 ANSI 命令行会乱码/`ERROR_INVALID_NAME`。
  已用 **subst 虚拟盘**映射为纯 ASCII 路径后正常：
  - `subst U: "E:\工作区\UI开发\988-22 - 副本"`（工程）
  - `subst V: "E:\工作区\_deps_cache"`（依赖缓存）
- 构建目录（**固定 2 个，以后直接覆盖，不再新增**）：
  - Release：`U:/0-Release`
  - Debug：`U:/0-Debug`
- 配置（Release）：`cmake -G Ninja -S U:/ -B U:/0-Release -DCMAKE_BUILD_TYPE=Release -DDEPS_CACHE=V:/`
  （Debug 同法：`-B U:/0-Debug -DCMAKE_BUILD_TYPE=Debug`）
- 编译：`PATH="/c/Users/mn/mingw64/bin:$PATH" ninja -C U:/0-Release`
  （Debug：`ninja -C U:/0-Debug`）
- 冒烟：`timeout 6 U:/0-Release/uiengine_demo.exe`（EXIT=124 正常；先 `taskkill //F //IM uiengine_demo.exe` 释放 exe 占用）
- Vue：本机无 Vulkan SDK（`C:/VulkanSDK` 已不存在），CMake 自动降级为 stub（已修守卫，可编译）。

---

## 6. 实际改动清单汇总（累计）

新增：`src/ui/win/StyleTpl.h`（+ 待定的页面文件）
修改：`src/render/vk/VKDevice.cpp`、`src/ui/win/UiContext.h`、`src/ui/MainUI.cpp`、
      `src/ui/win/WinControls.h`、`src/ui/win/WinControls.cpp`