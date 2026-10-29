// AI生成
#pragma once
// ============================================================
// 统一文本输入系统（全引擎输入框唯一认知）
//
// 架构对齐悬浮窗体系（规范铁律 1.3 系统派发 / 1.5 真实时间 / 1.6 稳定堆对象）：
//
//   EditText       —— 编辑状态（缓冲/过滤/上限/提交回调）。
//                     由页面/窗口处理器持有，挂在堆稳定 State 内
//                     （win getState<T> / MainUI pageState<T>）。
//                     控件只声明"有什么"，不写交互逻辑。
//
//   TextInputSystem —— 每应用一份（WindowManager 成员），统一管理：
//       焦点路由   requestFocus / blurAll（点击他处系统自动提交）
//       字符流     feedChars（退格 UTF-8 安全删 / Enter 提交 / 过滤追加）
//       光标相位   caretOn（steady_clock 真实时间 500ms，全引擎单源）
//       快捷键豁免 hasFocus（编辑中 0-9/Esc/F1-F11 系统代管）
//       Esc 放弃   cancelFocus（KeyDown 通路，恢复 orig 快照）
//       悬垂校验   validateOwners（焦点窗口销毁 → 静默清，回调不触发）
//
// 提交时序（全部经 commitFocus_ → onCommit，业务在回调里解析写值）：
//       Enter / 点击输入框外 / 切页 / 框架控件消费 / 窗口销毁前的最后一帧
// Esc = 放弃编辑恢复原值（不回调）。
//
//   业务接入三行（悬浮窗与页面同协议）：
//   State 声明   EditText edit{u8"占位", 64, TEXT_ANY};
//   click 命中   g_winMgr.textInput().requestFocus(TEXT_OWNER_WINDOW, win.id, edit);
//                （需鼠标定位光标时紧跟 placeCaret(edit, 框矩形, m.x)）
//   render 绘制  wctl::drawInput(dev, r, focused ? edit.buf : shown,
//                                placeholder, focused,
//                                focused && sys.caretOn(), a, 12.0f,
//                                focused ? edit.caret : -1);
// ============================================================
#include "WinCommon.h"
#include <functional>
#include <string>

namespace ui {

// ---- 字符过滤模式 ----
enum TextFilter {
    TEXT_ANY = 0,    // 任意可打印字符（UTF-8，中文 3 字节/字）
    TEXT_INT,        // 仅数字（R/G/B/Alpha 数值框）
    TEXT_HEX,        // hex 字符 + # 仅首位（Hex 颜色框）
    TEXT_NUM,        // 数值：数字 + 至多一个小数点 + 仅首位负号（计数器/步进器数值框）
    TEXT_IDCARD,     // 身份证：0-9 与 X/x（小写自动归一为大写 X）
};

// ---- 焦点持有者类别（悬垂校验用：窗口需查栈存活；页面生命周期由 MainUI 闭环） ----
enum TextOwnerKind {
    TEXT_OWNER_WINDOW = 0,   // 悬浮窗（ownerId = WindowInstance::id）
    TEXT_OWNER_PAGE,         // 主界面页面（ownerId = 页面索引，仅调试用）
};

// ====== 编辑状态（控件只声明，交互全归系统） ======
struct EditText {
    std::string buf;          // 编辑缓冲（UTF-8；系统唯一写入点）
    std::string orig;         // 进入编辑时快照（Esc 放弃恢复）
    std::string placeholder;  // 空值占位提示（绘制层读取）
    int  caret = 0;           // 光标字节偏移（0..buf.size()；系统维护，绘制层只读）
    int  maxBytes = 0;        // 缓冲字节上限（0 = 不限）
    int  filter = TEXT_ANY;   // 字符过滤模式
    bool focused = false;     // 焦点态（系统维护，控件只读）

    // 提交回调（Enter/失焦时点，业务在此解析写值；Esc 不触发）
    std::function<void(const std::string&)> onCommit;

    EditText() = default;
    EditText(const char* ph, int maxB, int flt)
        : placeholder(ph ? ph : ""), maxBytes(maxB), filter(flt) {}
};

// ====== 文本输入系统（每应用一份，WindowManager 成员） ======
class TextInputSystem {
public:
    // 焦点申请（点击输入框时调用）：旧焦点自动提交后切换
    void requestFocus(int ownerKind, int ownerId, EditText& t);
    // 主动释放并提交（仅当 t 是当前焦点）
    void releaseFocus(EditText& t);
    // 全部失焦（提交）：切页 / 框架消费 / 开新窗前调用
    void blurAll();

    // ---- 查询 ----
    bool hasFocus() const { return focus_ != nullptr; }
    bool focused(const EditText& t) const { return focus_ == &t; }

    // 字符流消费（WindowManager::handleInput 帧首转发；内部统一处理
    // 退格 8 / 回车 13 / 可打印过滤追加；无焦点时字符流静默丢弃）
    void feedChars(Input& in);

    // Esc 放弃编辑（GLFW Esc 走 KeyDown 不产 Char，由 onKeys 转发）：
    // 恢复 orig 快照并释放焦点，不触发 onCommit
    void cancelFocus();

    // 光标闪烁相位（真实时间 500ms；帧率无关，铁律 1.5）
    bool caretOn() const;

    // ---- 光标编辑（系统单源；方向键/Home/End/Delete 经 caretKeys 状态机） ----
    // 左右移动一个 UTF-8 字符（dir = -1 左 / +1 右，跳过全部续字节）
    void moveCaret(int dir);
    void caretHome();                              // 光标 → 行首（字节 0）
    void caretEnd();                               // 光标 → 行尾（buf 末）
    void deleteForward();                          // Delete 键：删光标处字符
    // 鼠标点击定位：box = 输入框屏幕矩形（与 wctl::drawInput 同框），
    // mouseX = 屏幕鼠标 x；内部经 wctl::byteFromCaretX 单源换算
    void placeCaret(EditText& t, const Rect& box, float mouseX);

    // 方向键统一状态机（编辑态每帧由 onKeys 转发 Input）：
    //   左/右键 单击（边沿）          → 移动 1 格
    //   左/右键 长按（按住 0.8s 不松）→ 进入连发，每 0.06s 移 1 格
    //   上/下键 双击（0.15s 内同键两击）→ 上 = 跳行首 / 下 = 跳行尾（单击无操作）
    // （全部 steady_clock 真实时间，帧率无关，铁律 1.5）
    void caretKeys(Input& in);

    // 悬垂校验（每帧输入前）：焦点窗口已销毁 → 静默清焦点
    // （WindowInstance 物理删除后 State 同亡，onCommit 不可再安全触发）
    void validateOwners();

private:
    void commitFocus_();   // 清焦点 → onCommit（先清再调，回调内可安全再申请）
    void clearFocus_();    // 仅清（不回调）
    bool feedChar_(EditText& t, unsigned int ch);
    double nowSec_() const;               // steady_clock 秒（状态机时间源）
    void touchActivity_();                // 记录编辑活动时刻（光标常亮窗口起点）

    EditText* focus_ = nullptr;          // 当前焦点（堆稳定 State 内，owner 存活期内有效）
    int ownerKind_ = TEXT_OWNER_WINDOW;
    int ownerId_ = -1;

    // 最后编辑/移动时刻（秒；caretOn 活动窗口判定。mutable：const 查询内更新）
    mutable double lastActivityClock_ = -1e9;

    // ---- 方向键状态机（caretKeys；单击/双击/长按单源） ----
    struct CaretKeyState {
        int    key = 0;              // 按住中的方向键（GLFW 码；0 = 无）
        double pressClock = 0.0;     // 本次按下时刻（秒）
        double repeatClock = 0.0;    // 下次连发应发时刻（秒；长按 0.8s 后启用）
        int    lastReleaseKey = 0;   // 上次释放的方向键（双击同键检测）
        double lastReleaseClock = -1e9;  // 上次释放时刻（秒）
    };
    CaretKeyState caretKey_;
};

} // namespace ui