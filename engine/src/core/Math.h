#pragma once
#include <cmath>
#include <cstdint>

namespace ui {

struct Vec2 {
    float x = 0.f, y = 0.f;
    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}
    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
};

struct Vec3 {
    float x = 0.f, y = 0.f, z = 0.f;
    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
};

struct Vec4 {
    float x = 0.f, y = 0.f, z = 0.f, w = 1.f;
    Vec4() = default;
    Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
};

struct Color {
    float r = 1.f, g = 1.f, b = 1.f, a = 1.f;
    Color() = default;
    Color(float r_, float g_, float b_, float a_ = 1.f) : r(r_), g(g_), b(b_), a(a_) {}
    static Color rgb(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255) {
        return Color(r_ / 255.f, g_ / 255.f, b_ / 255.f, a_ / 255.f);
    }
    static Color Black()   { return rgb(0, 0, 0); }
    static Color White()   { return rgb(255, 255, 255); }
    static Color Red()     { return rgb(220, 60, 60); }
    static Color Green()   { return rgb(60, 200, 90); }
    static Color Blue()    { return rgb(60, 120, 220); }
    static Color Gray()    { return rgb(120, 120, 130); }
    static Color Dark()    { return rgb(28, 30, 36); }
    static Color Light()   { return rgb(200, 205, 215); }
    static Color Yellow()  { return rgb(240, 220, 60); }
    static Color Cyan()    { return rgb(60, 200, 220); }
    static Color Magenta() { return rgb(220, 60, 180); }
};

struct Rect {
    Vec2 pos;
    Vec2 size;
    Rect() = default;
    Rect(float x, float y, float w, float h) : pos(x, y), size(w, h) {}
    Rect(const Vec2& p, const Vec2& s) : pos(p), size(s) {}
    bool contains(const Vec2& p) const {
        return p.x >= pos.x && p.x <= pos.x + size.x &&
               p.y >= pos.y && p.y <= pos.y + size.y;
    }
};

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

inline void HSVtoRGB(float h, float s, float v, int& r, int& g, int& b) {
    h = h / 60.0f;
    float c = v * s;
    float x = c * (1.0f - fabsf(fmodf(h, 2.0f) - 1.0f));
    float m = v - c;
    float r1, g1, b1;
    if (h < 1) { r1 = c; g1 = x; b1 = 0; }
    else if (h < 2) { r1 = x; g1 = c; b1 = 0; }
    else if (h < 3) { r1 = 0; g1 = c; b1 = x; }
    else if (h < 4) { r1 = 0; g1 = x; b1 = c; }
    else if (h < 5) { r1 = x; g1 = 0; b1 = c; }
    else { r1 = c; g1 = 0; b1 = x; }
    r = (int)((r1 + m) * 255);
    g = (int)((g1 + m) * 255);
    b = (int)((b1 + m) * 255);
}

inline Color HSVtoColor(float h, float s, float v, float a = 1.0f) {
    int r, g, b;
    HSVtoRGB(h, s, v, r, g, b);
    return Color(r / 255.f, g / 255.f, b / 255.f, a);
}

} // namespace ui