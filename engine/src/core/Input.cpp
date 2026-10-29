#include "Input.h"
#include <cstring>

namespace ui {

void Input::beginFrame() {
    std::memcpy(mousePrev_, mouseHeld_, sizeof(mouseHeld_));
    std::memcpy(keyPrev_,  keyHeld_,  sizeof(keyHeld_));
    wheelDelta_ = 0.f;
    chars_.clear();

}

void Input::apply(const Event& e) {
    switch (e.kind) {
    case EventKind::MouseMove:
        mouse_ = e.mouse;
        break;
    case EventKind::MouseDown:
        if (e.button >= 0 && e.button < 3) mouseHeld_[e.button] = true;
        mouse_ = e.mouse;
        break;
    case EventKind::MouseUp:
        if (e.button >= 0 && e.button < 3) mouseHeld_[e.button] = false;
        mouse_ = e.mouse;
        break;
    case EventKind::MouseWheel:
        wheelDelta_ += e.wheel;
        break;
    case EventKind::KeyDown:
        if (e.key >= 0 && e.key < kKeyCap) keyHeld_[e.key] = true;
        break;
    case EventKind::KeyUp:
        if (e.key >= 0 && e.key < kKeyCap) keyHeld_[e.key] = false;
        break;
    case EventKind::Char:
        chars_.push_back(e.codepoint);
        break;
    case EventKind::Close:
        closeRequested_ = true;
        break;
    default: break;
    }
}

bool Input::mouseDown(int btn) const {
    return btn >= 0 && btn < 3 && mouseHeld_[btn];
}
bool Input::mousePressed(int btn) const {
    return btn >= 0 && btn < 3 && mouseHeld_[btn] && !mousePrev_[btn];
}
bool Input::mouseReleased(int btn) const {
    return btn >= 0 && btn < 3 && !mouseHeld_[btn] && mousePrev_[btn];
}
bool Input::keyDown(int key) const {
    return key >= 0 && key < kKeyCap && keyHeld_[key];
}
bool Input::keyPressed(int key) const {
    return key >= 0 && key < kKeyCap && keyHeld_[key] && !keyPrev_[key];
}
bool Input::keyReleased(int key) const {
    return key >= 0 && key < kKeyCap && !keyHeld_[key] && keyPrev_[key];
}

} // namespace ui