// AI生成
#include "KeyRouter.h"
#include "../../core/Input.h"
#include "../../core/Log.h"
#include <cstring>
#include <string>

namespace ui {

// 作用域判定（唯一实现）：全局档恒命中；限定档逐项比对当前内容页 name。
// 白名单长度决定「指定 1 底」还是「限制 N 底」——同一份代码，不是两套机制。
bool KeyRouter::scopeHit(const KeyBinding& b, const char* contentName) {
    if (b.scope == KSCOPE_ALL) return true;
    if (!contentName || !contentName[0]) return false;
    for (const char* name : b.contents) {
        if (name && std::strcmp(name, contentName) == 0) return true;
    }
    return false;
}

bool KeyRouter::route(Input& in, bool textFocus, const char* contentName) {
    // 0. 文本输入优先：编辑中的 Esc/方向键/字符都归输入框，热键一律不抢
    if (textFocus || !table_) return false;

    for (const KeyBinding& b : *table_) {
        if (b.key == 0 || !in.keyPressed(b.key)) continue;
        // 修饰位只比 Ctrl/Shift/Alt 三位：GLFW 的 mods 还带 CAPS/NUM 等状态位，
        // 整体比较会让「按着大写锁定就失效」，这里按位掩掉再比
        uint32_t mods = in.keyMods(b.key) & (KMOD_CTRL | KMOD_SHIFT | KMOD_ALT);
        if (mods != b.mods) continue;
        // 作用域与键同等地位：不命中就当这条绑定不存在，继续找下一条
        if (!scopeHit(b, contentName)) continue;
        if (!b.act) continue;
        if (b.act()) {                     // 动作接下了 → 消费
            lastScope_ = (b.scope == KSCOPE_ALL) ? "global" : "only";
            lastKey_   = b.key;
            return true;
        }
        // 动作返回 false = 这个键这次不接（典型：game 底 Esc 只在存档页里接），
        // 继续问下一条，绝不当成已消费
    }
    return false;
}

void KeyRouter::audit(const std::vector<KeyBinding>* table) {
    if (!table || table->empty()) {
        UI_LOG_INFO("keyrouter: 0 key binding(s) registered");
        return;
    }
    UI_LOG_INFO("keyrouter: %d binding(s) registered", (int)table->size());
    for (const KeyBinding& b : *table) {
        if (b.scope == KSCOPE_ALL) {
            if (!b.reason || !b.reason[0]) {
                UI_LOG_WARN("keyrouter: key=%d 申请全局却缺少 reason", b.key);
                continue;
            }
            UI_LOG_INFO("keyrouter [A 全局 ] key=%d reason=%s", b.key, b.reason);
        } else {
            if (b.contents.empty()) {
                UI_LOG_WARN("keyrouter: key=%d 限定档白名单为空（永远不命中）", b.key);
                continue;
            }
            // 白名单拼进一行日志：便于核对「只在这几个底生效」
            std::string names;
            for (const char* n : b.contents) {
                if (!names.empty()) names += ",";
                names += (n ? n : "?");
            }
            UI_LOG_INFO("keyrouter [%s] key=%d contents=%s",
                        b.contents.size() == 1 ? "B 1底 " : "C N底 ",
                        b.key, names.c_str());
        }
    }
}

} // namespace ui
