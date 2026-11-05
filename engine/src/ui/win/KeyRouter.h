#pragma once
// AI生成
// ============================================================
// 键路由系统（KeyRouter）—— 热键收编的唯一实现
//
// 收编动机（对照《规范总纲》「同一逻辑出现第 2 次就该收编」）：
//   热键判定原先散落在页面里；搬到应用帧全局判定虽「唯一」却丢了作用域。
//   收编的形状 = 作用域本身成为一条数据：一张统一注册表，每行声明
//   「哪个键 + 在哪些底生效 + 动作」，系统唯一判定、命中即消费。
//
// 作用域三档（同一字段，白名单长度决定，不存在第二套机制）：
//   A 全局      KSCOPE_ALL                            → 所有内容页都响应
//   B 指定 1 底 KSCOPE_ONLY, {"game"}                 → 只在 game 底响应
//   C 限制 N 底 KSCOPE_ONLY, {"start","game","base"}  → 只在这 3 个底响应
//   C 档若靠「每个底各写一份键表」实现就是重复实现，必须走这张表。
//
// 判定优先级（顺序不可调）：
//   0. 文本输入有焦点 → 整条链让位（字符流/光标/Esc 放弃编辑归输入框）
//   1. 统一注册表：按登记顺序找「键 + 修饰 + 作用域」三者都命中的绑定
//   2. 动作返回 true = 消费、链条终止；false = 这次不接，继续找下一条
//   3. 都没命中 → 丢弃。**没有隐式全局兜底**——「全局」只能显式申请
//      （KSCOPE_ALL + reason），不能靠搬判定位置混进来。
//
// 铁律：
//   - 页面/处理器里禁止再出现 `if (in.keyPressed(...))`，一律表里登记
//   - 全局申请（KSCOPE_ALL）必须写 reason；建表时系统审计并列清单
//   - 键表只声明键码/修饰/作用域/动作，不自己判焦点、不自己排优先级
//   - 内容页名用注册表里的 name（AppContentHandler::name），不写裸字符串比较
// ============================================================
#include <cstdint>
#include <functional>
#include <vector>

namespace ui {

class Input;

// 修饰位（与 Input::keyMods 返回的位口径一致；Input 已把 GLFW mods 归一到这里）
enum KeyMod : uint32_t {
    KMOD_NONE  = 0u,
    KMOD_CTRL  = 1u << 0,
    KMOD_SHIFT = 1u << 1,
    KMOD_ALT   = 1u << 2,
};

// 作用域档位：全局 / 仅白名单内容页（1 个或 N 个都是它）
enum KeyScope : uint32_t {
    KSCOPE_ALL  = 0,   // 全局：所有内容页
    KSCOPE_ONLY = 1,   // 限定：只在 contents 列出的内容页生效
};

// 一条按键绑定：键码 + 修饰 + 动作 + 作用域 + 申请理由
// act 返回 true = 已消费；false = 不接，继续问下一条
struct KeyBinding {
    int                        key      = 0;      // GLFW 键码（256=Esc 290=F1 …）
    uint32_t                   mods     = KMOD_NONE;
    std::function<bool()>      act;
    KeyScope                   scope    = KSCOPE_ALL;
    std::vector<const char*>   contents;          // KSCOPE_ONLY 白名单（内容页 name）
    const char*                reason   = nullptr; // KSCOPE_ALL 必填：为什么必须跨页
};

class KeyRouter {
public:
    // ---- 声明入口（每帧由 AppWindow 填入统一注册表 + 当前内容页名）----
    void setBindings(const std::vector<KeyBinding>* table) { table_ = table; }

    // ---- 系统唯一判定：文本焦点 → 注册表（作用域匹配）→ 丢弃 ----
    // textFocus 由调用方给（WindowManager::textInput().hasFocus()），
    // contentName = 当前内容处理器 name（换页自动换值，作用域随之生效）
    bool route(Input& in, bool textFocus, const char* contentName);

    // ---- 作用域审计（建表时调一次，不是每帧）----
    // 逐条列出作用域与理由；全局缺 reason、限定档白名单为空即告警。
    static void audit(const std::vector<KeyBinding>* table);

    // ---- 诊断（调试窗/日志用：刚被哪个作用域吃掉）----
    const char* lastScope() const { return lastScope_; }
    int         lastKey()   const { return lastKey_; }

private:
    static bool scopeHit(const KeyBinding& b, const char* contentName);

    const std::vector<KeyBinding>* table_      = nullptr;  // 统一注册表（唯一键表）
    const char*                    lastScope_  = "";
    int                            lastKey_    = 0;
};

} // namespace ui
