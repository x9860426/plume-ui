// AI生成
#pragma once
// ============================================================
// 内置页面处理器声明（每个文件一个页面，结构对齐悬浮窗 WinHandlers 样板：
//   namespace xxx {
//     struct Layout;                 // 本页全部控件矩形（几何单源）
//     static Layout layout(env);     // render/click/hover 三处共用
//     struct State;                  // 本页私有状态（经 env.state<T>() 存取）
//   }
//   + render/click/hover/... 处理器函数 + registerXxxPage 注册入口
// ============================================================
#include "PageHandler.h"

namespace ui {

// 概览页（控件展示：输入框/下拉/步进器/分段/色块） — Pages_Overview.cpp
void RegisterOverviewPage(PageHandlerRegistry& reg);

// UI模板页（UI 控件模板总览：每控件名字+编号+下拉切换） — Pages_Directory.cpp
void RegisterDirectoryPage(PageHandlerRegistry& reg);

// 分割器页（SplitterSystem 验收：上下/左右分割 + 悬浮窗联动按钮） — Pages_Splitter.cpp
void RegisterSplitterPage(PageHandlerRegistry& reg);

// 沙盘页（SandboxSystem 验收：无限画布 pan+zoom + 控件精确交互） — Pages_Sandbox.cpp
void RegisterSandboxPage(PageHandlerRegistry& reg);

// 其他页（选项展示：滑条/开关/复选框/关于） — Pages_Options.cpp
void RegisterOptionsPage(PageHandlerRegistry& reg);

} // namespace ui