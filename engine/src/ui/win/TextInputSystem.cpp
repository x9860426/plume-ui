// AI生成
// ============================================================
// 统一文本输入系统实现
// 字符处理 / 焦点路由 / 光标相位全部单源实现，
// 页面与悬浮窗处理器不再各写编辑状态机（根治三处重复）。
// ============================================================
#include "TextInputSystem.h"
#include "LayerStack.h"
#include "UiContext.h"
#include "WinControls.h"
#include <chrono>
#include <cstring>

namespace ui {

// ---- UTF-8 编码插入（单源；在光标字节偏移处插入） ----
static void insertUtf8Char(std::string& s, int pos, unsigned int ch) {
    char buf[4];
    int n = 0;
    if (ch < 128) {
        buf[n++] = (char)ch;
    } else if (ch < 0x800) {
        buf[n++] = (char)(0xC0 | (ch >> 6));
        buf[n++] = (char)(0x80 | (ch & 0x3F));
    } else {
        buf[n++] = (char)(0xE0 | (ch >> 12));
        buf[n++] = (char)(0x80 | ((ch >> 6) & 0x3F));
        buf[n++] = (char)(0x80 | (ch & 0x3F));
    }
    s.insert((size_t)pos, buf, (size_t)n);
}

// ---- UTF-8 步进（单源；光标移动/前后删除共用） ----
// 光标前一字符的字节数（0 = 已在行首）
static int prevCharLen(const std::string& s, int caret) {
    if (caret <= 0) return 0;
    int i = caret - 1;
    while (i > 0 && ((unsigned char)s[i] & 0xC0) == 0x80) i--;   // 0x80..0xBF = 续字节
    return caret - i;
}
// 光标处字符的字节数（0 = 已在行尾）
static int nextCharLen(const std::string& s, int caret) {
    int n = (int)s.size();
    if (caret >= n) return 0;
    int len = 1;
    while (caret + len < n && ((unsigned char)s[caret + len] & 0xC0) == 0x80) len++;
    return len;
}

// ============================================================
// 焦点路由
// ============================================================

void TextInputSystem::requestFocus(int ownerKind, int ownerId, EditText& t) {
    if (focus_ == &t) {                     // 重复点击同一框：仅刷新归属
        ownerKind_ = ownerKind;
        ownerId_ = ownerId;
        touchActivity_();                   // 重复点击也是活动：常亮半周期起步
        return;
    }
    if (focus_) commitFocus_();             // 旧焦点提交（点击切换输入框语义）
    focus_ = &t;
    ownerKind_ = ownerKind;
    ownerId_ = ownerId;
    t.focused = true;
    t.orig = t.buf;                         // Esc 放弃恢复快照
    t.caret = (int)t.buf.size();            // 进入编辑：光标落行尾
    touchActivity_();                       // 进入编辑：光标常亮起步（不先闪半拍）
}

void TextInputSystem::releaseFocus(EditText& t) {
    if (focus_ == &t) commitFocus_();
}

void TextInputSystem::blurAll() {
    if (focus_) commitFocus_();
}

void TextInputSystem::commitFocus_() {
    if (!focus_) return;
    EditText* t = focus_;
    clearFocus_();                          // 先清再回调：回调内可安全再申请焦点
    if (t->onCommit) t->onCommit(t->buf);
}

void TextInputSystem::clearFocus_() {
    if (focus_) focus_->focused = false;
    focus_ = nullptr;
    ownerId_ = -1;
}

void TextInputSystem::cancelFocus() {
    if (!focus_) return;
    focus_->buf = focus_->orig;             // 放弃编辑：恢复进入时快照
    focus_->caret = (int)focus_->buf.size();
    clearFocus_();
}

// ============================================================
// 字符流
// ============================================================

bool TextInputSystem::feedChar_(EditText& t, unsigned int ch) {
    if (ch == 8) {                          // 退格：删光标前一字符（UTF-8 安全）
        int n = prevCharLen(t.buf, t.caret);
        if (n > 0) {
            t.buf.erase((size_t)(t.caret - n), (size_t)n);
            t.caret -= n;
        }
        return true;
    }
    if (ch == 13) {                         // 回车：提交并释放
        commitFocus_();
        return true;
    }
    if (ch < 32 || ch >= 0x10000) return false;   // 其余控制字符丢弃
    bool full = (t.maxBytes > 0 && t.buf.size() >= (size_t)t.maxBytes);
    switch (t.filter) {
    case TEXT_INT:
        if (ch < '0' || ch > '9') return false;
        if (full) return false;
        t.buf.insert((size_t)t.caret, 1, (char)ch);
        t.caret += 1;
        return true;
    case TEXT_NUM: {                        // 数值：数字 + 至多一个小数点 + 至多一个负号
        if (ch == '-') {                    // 负号可随时输入（不再要求缓冲区为空）
            if (full || t.buf.find('-') != std::string::npos) return false;  // 唯一负号
            t.buf.insert((size_t)0, 1, '-');    // 插到最前（符号位）
            t.caret += 1;                       // 插入点在光标之前 → 光标后移 1，保持原位
            return true;
        }
        bool ok = false;
        if (ch >= '0' && ch <= '9') ok = true;
        else if (ch == '.' && t.buf.find('.') == std::string::npos) ok = true;  // 小数点至多一个
        if (!ok || full) return false;
        t.buf.insert((size_t)t.caret, 1, (char)ch);
        t.caret += 1;
        return true;
    }
    case TEXT_HEX: {
        bool hexCh = (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') ||
                     (ch >= 'A' && ch <= 'F');
        if (hexCh && !full) {
            t.buf.insert((size_t)t.caret, 1, (char)ch);
            t.caret += 1;
            return true;
        }
        if (ch == '#' && t.buf.empty()) {   // # 仅允许首位
            t.buf.insert((size_t)t.caret, 1, '#');
            t.caret += 1;
            return true;
        }
        return false;
    }
    case TEXT_IDCARD: {                     // 身份证：0-9 与 X（小写归一为大写）
        if (full) return false;
        if (ch >= '0' && ch <= '9') {
            t.buf.insert((size_t)t.caret, 1, (char)ch);
            t.caret += 1;
            return true;
        }
        if (ch == 'x' || ch == 'X') {
            t.buf.insert((size_t)t.caret, 1, 'X');
            t.caret += 1;
            return true;
        }
        return false;
    }
    default:                                // TEXT_ANY：任意可打印 UTF-8
        if (full) return false;
        insertUtf8Char(t.buf, t.caret, ch);
        t.caret += nextCharLen(t.buf, t.caret);   // 前移刚插入的字符宽
        return true;
    }
}

void TextInputSystem::feedChars(Input& in) {
    const auto& chars = in.chars();
    if (chars.empty()) return;
    if (focus_) {
        for (unsigned int ch : chars)
            if (feedChar_(*focus_, ch)) touchActivity_();   // 有效编辑 → 光标常亮
    }
    in.clearChars();   // 输入框系统是字符流唯一消费方（无主字符不落主界面）
}

// ============================================================
// 光标相位 / 悬垂校验
// ============================================================

// 编辑活动常亮窗口：与闪烁半周期一致（活动后 0.5s 内不闪，
// 停止移动/输入即恢复闪烁——长按连发期间光标稳定可见）
static constexpr double kCaretBlinkResume = 0.5;

bool TextInputSystem::caretOn() const {
    if (nowSec_() - lastActivityClock_ < kCaretBlinkResume)
        return true;                        // 活动窗口：常亮不闪
    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    return (ms / 500) % 2 == 0;
}

void TextInputSystem::touchActivity_() {
    lastActivityClock_ = nowSec_();
}

// ============================================================
// 光标编辑（方向键/Home/End/Delete/鼠标点击；系统单源）
// ============================================================

void TextInputSystem::moveCaret(int dir) {
    if (!focus_) return;
    EditText& t = *focus_;
    int old = t.caret;
    if (dir < 0) {
        t.caret -= prevCharLen(t.buf, t.caret);
    } else if (dir > 0) {
        t.caret += nextCharLen(t.buf, t.caret);
    }
    if (t.caret < 0) t.caret = 0;
    if (t.caret > (int)t.buf.size()) t.caret = (int)t.buf.size();
    if (t.caret != old) touchActivity_();   // 实际移动才刷新常亮窗口
}

void TextInputSystem::caretHome() {
    if (!focus_) return;
    if (focus_->caret != 0) { focus_->caret = 0; touchActivity_(); }
}

void TextInputSystem::caretEnd() {
    if (!focus_) return;
    int end = (int)focus_->buf.size();
    if (focus_->caret != end) { focus_->caret = end; touchActivity_(); }
}

void TextInputSystem::deleteForward() {
    if (!focus_) return;
    EditText& t = *focus_;
    int n = nextCharLen(t.buf, t.caret);
    if (n > 0) {
        t.buf.erase((size_t)t.caret, (size_t)n);
        touchActivity_();
    }
}

void TextInputSystem::placeCaret(EditText& t, const Rect& box, float mouseX) {
    if (focus_ != &t) return;               // 非当前焦点：忽略（防串扰）
    t.caret = wctl::byteFromCaretX(g_uiCtx.font, t.buf.c_str(),
                                   mouseX - wctl::inputTextRect(box).pos.x);
    touchActivity_();                       // 点击定位：光标常亮起步
}

// ============================================================
// 方向键状态机（左右单击/长按连发；上下双击跳首尾）
// ============================================================

// 交互规格（用户定义）：
//   左/右键  单击 = 移 1 格；按住 0.8s 不松 = 连发（每 0.06s 一格）
//   上/下键  双击（0.15s 内同键两击）= 上跳行首 / 下跳行尾；单击无操作
//            （单行编辑框上下无移动语义，专用于双击跳转）
static constexpr double kCaretDoubleTapWindow = 0.15;   // 双击窗口（秒，宜窄防误触）
static constexpr double kCaretHoldDelay       = 0.80;   // 长按启动延迟（秒）
static constexpr double kCaretRepeatInterval  = 0.06;   // 连发间隔（秒）
static constexpr int    kCaretKeyLeft  = 263;           // GLFW_KEY_LEFT
static constexpr int    kCaretKeyRight = 262;           // GLFW_KEY_RIGHT
static constexpr int    kCaretKeyUp    = 265;           // GLFW_KEY_UP
static constexpr int    kCaretKeyDown  = 264;           // GLFW_KEY_DOWN

double TextInputSystem::nowSec_() const {
    return std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

void TextInputSystem::caretKeys(Input& in) {
    // Home/End/Delete：简单边沿（无连发语义）
    if (in.keyPressed(268 /*GLFW_KEY_HOME*/))   caretHome();
    if (in.keyPressed(269 /*GLFW_KEY_END*/))    caretEnd();
    if (in.keyPressed(261 /*GLFW_KEY_DELETE*/)) deleteForward();

    double now = nowSec_();

    // 释放边沿：左右按住态结束；上下键释放记入双击窗口
    const int watchKeys[3] = {caretKey_.key, kCaretKeyUp, kCaretKeyDown};
    for (int k : watchKeys) {
        if (!k || !in.keyReleased(k)) continue;
        caretKey_.lastReleaseKey = k;
        caretKey_.lastReleaseClock = now;
        if (k == caretKey_.key) caretKey_.key = 0;
    }

    // 上下键：双击跳转（0.15s 内同键两击；单击无操作）
    const int vkeys[2] = {kCaretKeyUp, kCaretKeyDown};
    for (int k : vkeys) {
        if (!in.keyPressed(k)) continue;
        bool doubleTap = (k == caretKey_.lastReleaseKey &&
                          now - caretKey_.lastReleaseClock < kCaretDoubleTapWindow);
        if (doubleTap) {
            if (k == kCaretKeyUp) caretHome(); else caretEnd();
            caretKey_.lastReleaseKey = 0;   // 防三击连跳（第三击回到无操作）
        }
    }

    // 左右键：单击 1 格（长按连发见下方按住推进；无双击语义）
    const int keys[2] = {kCaretKeyLeft, kCaretKeyRight};
    for (int k : keys) {
        if (!in.keyPressed(k)) continue;
        moveCaret(k == kCaretKeyLeft ? -1 : +1);
        caretKey_.key = k;
        caretKey_.pressClock = now;
        caretKey_.repeatClock = now;
    }

    // 按住推进：焦点丢失/键已松开 → 复位；0.8s 未松 → 连发
    if (!caretKey_.key) return;
    if (!focus_ || !in.keyDown(caretKey_.key)) {
        caretKey_.key = 0;                 // 失焦/事件丢失兜底复位
        return;
    }
    double held = now - caretKey_.pressClock;
    if (held < kCaretHoldDelay) return;
    // 下次应发时刻 = max(repeatClock, pressClock + 长按延迟)；
    // 首发对齐按下后 0.8s，此后每 0.06s 一格（while 补帧率波动欠账）
    double due = caretKey_.repeatClock;
    double firstDue = caretKey_.pressClock + kCaretHoldDelay;
    if (due < firstDue) due = firstDue;
    while (due <= now) {
        moveCaret(caretKey_.key == kCaretKeyLeft ? -1 : +1);
        due += kCaretRepeatInterval;
    }
    caretKey_.repeatClock = due;           // 存下次应发时刻
}

void TextInputSystem::validateOwners() {
    if (!focus_) return;
    if (ownerKind_ == TEXT_OWNER_WINDOW) {
        WindowInstance* w = g_layerStack.findWindow(ownerId_);
        if (!w || w->disposed) clearFocus_();   // 窗口已销毁：State 同亡，静默清
    }
    // 页面焦点生命周期由 MainUI 闭环管理（switchPage / 框架消费 blurAll，
    // 页面 State 切页保留且 MainUI 与本系统同亡），无需校验
}

} // namespace ui