#pragma once
#include "Event.h"
#include <vector>

namespace ui {

class Input {
public:
    void beginFrame();
    void apply(const Event& e);

    Vec2 mouse() const { return mouse_; }
    bool mouseDown(int btn) const;   // held this frame
    bool mousePressed(int btn) const; // went down this frame
    bool mouseReleased(int btn) const;

    float wheelDelta() const { return wheelDelta_; }

    bool keyDown(int key) const;
    bool keyPressed(int key) const;
    bool keyReleased(int key) const;

    const std::vector<unsigned int>& chars() const { return chars_; }
    void clearChars() { chars_.clear(); }  // 窗口文本焦点消费后清空（防主界面重复处理）

    bool closeRequested() const { return closeRequested_; }
    void setCloseRequested(bool v) { closeRequested_ = v; }

private:
    Vec2 mouse_{0, 0};
    bool mouseHeld_[3]   = {false, false, false};
    bool mousePrev_[3]   = {false, false, false};

    float wheelDelta_    = 0.f;

    static constexpr int kKeyCap = 512;
    bool keyHeld_[kKeyCap]  = {false};
    bool keyPrev_[kKeyCap]  = {false};

    std::vector<unsigned int> chars_;
    bool closeRequested_ = false;
};

} // namespace ui