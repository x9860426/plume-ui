// AI生成
#pragma once
// ============================================================
// 主界面页面配置表（仿 win/WindowConfig.h：纯数据零渲染依赖）
//
// 主界面 tab 页由配置表声明，MainUI 框架不写死任何页面：
// 新增一个页面 = 追加一条配置 + 实现一组页面处理器 + 注册，
// MainUI 框架代码零改动（对齐悬浮窗"配置表驱动"扩展模式）。
//
// 字段语义对照悬浮窗 WindowConfig：
//   id          ← 窗口逻辑名（snake_case 查表键）
//   title       ← 窗口标题（这里=tab 标签文字）
//   handlerName ← 处理器注册表键（PageHandlerRegistry 分发）
//   icon/desc   ← 主界面特有：内容面板头部大图标与描述行
// ============================================================
#include <cstdint>

namespace ui {

struct PageConfig {
    const char* id;            // 页面逻辑名（snake_case，全局唯一）
    const char* title;         // tab 标签文字（UTF-8）
    const char* icon;          // 内容面板头部大图标（bigFont 渲染）
    const char* desc;          // 内容面板头部描述行（smallFont 渲染）
    const char* handlerName;   // 页面处理器注册表键（配置驱动分发）
};

// ---- 页面配置表（新增页面在此追加；tab 顺序 = 数组顺序） ----
static const PageConfig PAGE_CONFIGS[] = {
    { "overview",  u8"概览", u8"◆", u8"控件 — 可编辑输入框、下拉、步进器、分段按钮", "page_overview"  },
    { "directory", u8"UI模板", u8"★", u8"UI 控件模板 — 每控件名字+编号，下拉切换（1=系统默认）", "page_directory" },
    { "splitter",  u8"分割", u8"◈", u8"分割器 — 系统派发 splitter 上下/左右分割拖拽",  "page_splitter"  },
    { "sandbox",   u8"沙盘", u8"▣", u8"沙盘 — 无限画布 pan+zoom 缩放+控件交互测试",     "page_sandbox"   },
    { "options",   u8"其他", u8"●", u8"选项 — 滑动条、开关、复选框",                "page_options"   },
};

static const int PAGE_COUNT = (int)(sizeof(PAGE_CONFIGS) / sizeof(PAGE_CONFIGS[0]));

// 按逻辑名反查配置索引（-1 = 未找到）
inline int FindPageIndex(const char* id) {
    for (int i = 0; i < PAGE_COUNT; i++)
        if (id && PAGE_CONFIGS[i].id == id) return i;
    return -1;
}

} // namespace ui