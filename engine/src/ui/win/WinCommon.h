#pragma once
// ============================================================
// 窗口系统统一基础头（src/ui/win/ 模块的唯一类型入口）
//
// 设计目的（规范：高扩展性 / 系统派发优先 / 零修补）：
//   win/ 目录下所有头文件一律包含本头，一次获得全部基础设施
//   类型（数学 / 输入 / 渲染设备 / 字体 / 主题 / 绘制原语），
//   杜绝"各文件零散前向声明、遗漏频出、命名空间不一致"的
//   修补式问题——新增窗口/控件/处理器时永远不需要再关心
//   头文件依赖，只管包含 WinCommon.h。
//
// 依赖分层（全部为更底层模块，无循环依赖）：
//   core/Math.h           → Vec2/Rect/Color/clampf 等数学基础
//   core/Input.h          → 鼠标/键盘/滚轮输入状态（WindowManager 路由用）
//   render/RenderDevice.h → 渲染设备（SDF 圆角 / 裁剪 / 三角形绘制）
//   ui/Font.h             → 字体（measureText / scaledLineHeight）
//   ui/Theme.h            → 主题色（g_themes / currentColors）
//   ui/Element.h          → 绘制原语（drawRoundedRect / drawText / drawShadow…）
//
// 例外：WindowConfig.h 是纯数据配置表（只依赖 <cstdint>），
//   刻意不包含本头——配置表与渲染无关，保持零依赖可移植。
// ============================================================
#include "../../core/Math.h"
#include "../../core/Input.h"
#include "../../render/RenderDevice.h"
#include "../Font.h"
#include "../Theme.h"
#include "../Element.h"
#include "../../app/AppFwd.h"