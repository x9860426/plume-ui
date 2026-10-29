#include "Element.h"
#include "../core/Log.h"
#include <cstring>
#include <cmath>
#include <algorithm>

namespace ui {

static uint32_t decodeUtf8(const char*& s, const char* end) {
    if (s >= end) return 0;
    uint8_t c = uint8_t(*s++);
    if (c < 0x80) return c;
    if ((c & 0xE0) == 0xC0) {
        uint32_t r = c & 0x1F; if (s < end) r = (r << 6) | (*s++ & 0x3F); return r;
    }
    if ((c & 0xF0) == 0xE0) {
        uint32_t r = c & 0x0F;
        if (s < end) r = (r << 6) | (*s++ & 0x3F);
        if (s < end) r = (r << 6) | (*s++ & 0x3F);
        return r;
    }
    if ((c & 0xF8) == 0xF0) {
        uint32_t r = c & 0x07;
        if (s < end) r = (r << 6) | (*s++ & 0x3F);
        if (s < end) r = (r << 6) | (*s++ & 0x3F);
        if (s < end) r = (r << 6) | (*s++ & 0x3F);
        return r;
    }
    return 0x3F;
}


static float clampRadius(float rad, float w, float h) {
    if (rad * 2 > w) rad = w * 0.5f;
    if (rad * 2 > h) rad = h * 0.5f;
    return rad;
}

void drawRect(RenderDevice* dev, const Rect& r, const Color& c) {
    if (!dev) return;
    float x0 = r.pos.x, y0 = r.pos.y;
    float x1 = x0 + r.size.x, y1 = y0 + r.size.y;
    Vertex v[4] = {
        {{x0, y0}, {0, 0}, c},
        {{x1, y0}, {1, 0}, c},
        {{x1, y1}, {1, 1}, c},
        {{x0, y1}, {0, 1}, c},
    };
    uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
    dev->drawTriangles(v, 4, idx, 6, 0, false);
}

void drawBorder(RenderDevice* dev, const Rect& r, const Color& c, float t) {
    // 直角描边：SDF 模式 2 + radius=0（中心对齐、厚度精确、软边 AA）
    // 替代旧四条 drawRect 硬拼（无 AA 且厚度非整数时左上/右下不均）
    if (!dev) return;
    dev->setSDFParams(2, r.pos.x, r.pos.y, r.size.x, r.size.y, 0, 0, 0, 0, t);

    const float aaW = 2.0f;
    float x0 = r.pos.x - aaW, y0 = r.pos.y - aaW;
    float x1 = r.pos.x + r.size.x + aaW, y1 = r.pos.y + r.size.y + aaW;
    Vertex v[4] = {
        {{x0, y0}, {0, 0}, c},
        {{x1, y0}, {1, 0}, c},
        {{x1, y1}, {1, 1}, c},
        {{x0, y1}, {0, 1}, c},
    };
    uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
    dev->drawTriangles(v, 4, idx, 6, 0, false);

    dev->setSDFParams(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

void drawRoundedRect(RenderDevice* dev, const Rect& r, const Color& c, float radius) {
    if (!dev) return;
    float rad = clampRadius(radius, r.size.x, r.size.y);
    if (rad < 1.0f) { drawRect(dev, r, c); return; }

    dev->setSDFParams(1, r.pos.x, r.pos.y, r.size.x, r.size.y, rad, rad, rad, rad, 0);

    const float aaW = 2.0f;
    float x0 = r.pos.x - aaW, y0 = r.pos.y - aaW;
    float x1 = r.pos.x + r.size.x + aaW, y1 = r.pos.y + r.size.y + aaW;
    Vertex v[4] = {
        {{x0, y0}, {0, 0}, c},
        {{x1, y0}, {1, 0}, c},
        {{x1, y1}, {1, 1}, c},
        {{x0, y1}, {0, 1}, c},
    };
    uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
    dev->drawTriangles(v, 4, idx, 6, 0, false);

    dev->setSDFParams(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

// 四角独立圆角矩形：SDF 着色器原生支持四角独立半径（四槽位），
// 每角单独 clamp（不超过宽/高的一半），任意一角为 0 时该角绘制直角。
void drawCornerRadiiRect(RenderDevice* dev, const Rect& r, const Color& c,
                         float tl, float tr, float bl, float br) {
    if (!dev) return;
    float hw = r.size.x * 0.5f, hh = r.size.y * 0.5f;
    if (tl > hw) tl = hw;  if (tl > hh) tl = hh;
    if (tr > hw) tr = hw;  if (tr > hh) tr = hh;
    if (bl > hw) bl = hw;  if (bl > hh) bl = hh;
    if (br > hw) br = hw;  if (br > hh) br = hh;
    if (tl < 1.0f && tr < 1.0f && bl < 1.0f && br < 1.0f) { drawRect(dev, r, c); return; }

    // setSDFParams 形参序为 (TL, TR, BR, BL)：BR 在 BL 前（与 RenderDevice.h 一致）
    dev->setSDFParams(1, r.pos.x, r.pos.y, r.size.x, r.size.y, tl, tr, br, bl, 0);

    const float aaW = 2.0f;
    float x0 = r.pos.x - aaW, y0 = r.pos.y - aaW;
    float x1 = r.pos.x + r.size.x + aaW, y1 = r.pos.y + r.size.y + aaW;
    Vertex v[4] = {
        {{x0, y0}, {0, 0}, c},
        {{x1, y0}, {1, 0}, c},
        {{x1, y1}, {1, 1}, c},
        {{x0, y1}, {0, 1}, c},
    };
    uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
    dev->drawTriangles(v, 4, idx, 6, 0, false);

    dev->setSDFParams(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

void drawTopRoundedRect(RenderDevice* dev, const Rect& r, const Color& c, float topRadius) {
    if (!dev) return;
    float rad = clampRadius(topRadius, r.size.x, r.size.y);
    if (rad < 1.0f) { drawRect(dev, r, c); return; }

    dev->setSDFParams(1, r.pos.x, r.pos.y, r.size.x, r.size.y, rad, rad, 0, 0, 0);

    const float aaW = 2.0f;
    float x0 = r.pos.x - aaW, y0 = r.pos.y - aaW;
    float x1 = r.pos.x + r.size.x + aaW, y1 = r.pos.y + r.size.y + aaW;
    Vertex v[4] = {
        {{x0, y0}, {0, 0}, c},
        {{x1, y0}, {1, 0}, c},
        {{x1, y1}, {1, 1}, c},
        {{x0, y1}, {0, 1}, c},
    };
    uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
    dev->drawTriangles(v, 4, idx, 6, 0, false);

    dev->setSDFParams(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

void drawBottomRoundedRect(RenderDevice* dev, const Rect& r, const Color& c, float bottomRadius) {
    if (!dev) return;
    float rad = clampRadius(bottomRadius, r.size.x, r.size.y);
    if (rad < 1.0f) { drawRect(dev, r, c); return; }

    dev->setSDFParams(1, r.pos.x, r.pos.y, r.size.x, r.size.y, 0, 0, rad, rad, 0);

    const float aaW = 2.0f;
    float x0 = r.pos.x - aaW, y0 = r.pos.y - aaW;
    float x1 = r.pos.x + r.size.x + aaW, y1 = r.pos.y + r.size.y + aaW;
    Vertex v[4] = {
        {{x0, y0}, {0, 0}, c},
        {{x1, y0}, {1, 0}, c},
        {{x1, y1}, {1, 1}, c},
        {{x0, y1}, {0, 1}, c},
    };
    uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
    dev->drawTriangles(v, 4, idx, 6, 0, false);

    dev->setSDFParams(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

void drawLeftRoundedRect(RenderDevice* dev, const Rect& r, const Color& c, float leftRadius) {
    if (!dev) return;
    float rad = clampRadius(leftRadius, r.size.x, r.size.y);
    if (rad < 1.0f) { drawRect(dev, r, c); return; }

    dev->setSDFParams(1, r.pos.x, r.pos.y, r.size.x, r.size.y, rad, 0, 0, rad, 0);

    const float aaW = 2.0f;
    float x0 = r.pos.x - aaW, y0 = r.pos.y - aaW;
    float x1 = r.pos.x + r.size.x + aaW, y1 = r.pos.y + r.size.y + aaW;
    Vertex v[4] = {
        {{x0, y0}, {0, 0}, c},
        {{x1, y0}, {1, 0}, c},
        {{x1, y1}, {1, 1}, c},
        {{x0, y1}, {0, 1}, c},
    };
    uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
    dev->drawTriangles(v, 4, idx, 6, 0, false);

    dev->setSDFParams(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

void drawRightRoundedRect(RenderDevice* dev, const Rect& r, const Color& c, float rightRadius) {
    if (!dev) return;
    float rad = clampRadius(rightRadius, r.size.x, r.size.y);
    if (rad < 1.0f) { drawRect(dev, r, c); return; }

    dev->setSDFParams(1, r.pos.x, r.pos.y, r.size.x, r.size.y, 0, rad, rad, 0, 0);

    const float aaW = 2.0f;
    float x0 = r.pos.x - aaW, y0 = r.pos.y - aaW;
    float x1 = r.pos.x + r.size.x + aaW, y1 = r.pos.y + r.size.y + aaW;
    Vertex v[4] = {
        {{x0, y0}, {0, 0}, c},
        {{x1, y0}, {1, 0}, c},
        {{x1, y1}, {1, 1}, c},
        {{x0, y1}, {0, 1}, c},
    };
    uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
    dev->drawTriangles(v, 4, idx, 6, 0, false);

    dev->setSDFParams(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

void drawRoundedBorder(RenderDevice* dev, const Rect& r, const Color& c, float thickness, float radius) {
    if (!dev) return;
    float rad = clampRadius(radius, r.size.x, r.size.y);
    if (rad < 1.0f) { drawBorder(dev, r, c, thickness); return; }

    dev->setSDFParams(2, r.pos.x, r.pos.y, r.size.x, r.size.y, rad, rad, rad, rad, thickness);

    const float aaW = 2.0f;
    float x0 = r.pos.x - aaW, y0 = r.pos.y - aaW;
    float x1 = r.pos.x + r.size.x + aaW, y1 = r.pos.y + r.size.y + aaW;
    Vertex v[4] = {
        {{x0, y0}, {0, 0}, c},
        {{x1, y0}, {1, 0}, c},
        {{x1, y1}, {1, 1}, c},
        {{x0, y1}, {0, 1}, c},
    };
    uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
    dev->drawTriangles(v, 4, idx, 6, 0, false);

    dev->setSDFParams(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

void drawTopRoundedBorder(RenderDevice* dev, const Rect& r, const Color& c, float thickness, float topRadius) {
    if (!dev) return;
    float rad = clampRadius(topRadius, r.size.x, r.size.y);
    if (rad < 1.0f) { drawBorder(dev, r, c, thickness); return; }

    dev->setSDFParams(2, r.pos.x, r.pos.y, r.size.x, r.size.y, rad, rad, 0, 0, thickness);

    const float aaW = 2.0f;
    float x0 = r.pos.x - aaW, y0 = r.pos.y - aaW;
    float x1 = r.pos.x + r.size.x + aaW, y1 = r.pos.y + r.size.y + aaW;
    Vertex v[4] = {
        {{x0, y0}, {0, 0}, c},
        {{x1, y0}, {1, 0}, c},
        {{x1, y1}, {1, 1}, c},
        {{x0, y1}, {0, 1}, c},
    };
    uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
    dev->drawTriangles(v, 4, idx, 6, 0, false);

    dev->setSDFParams(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

void drawBottomRoundedBorder(RenderDevice* dev, const Rect& r, const Color& c, float thickness, float bottomRadius) {
    if (!dev) return;
    float rad = clampRadius(bottomRadius, r.size.x, r.size.y);
    if (rad < 1.0f) { drawBorder(dev, r, c, thickness); return; }

    dev->setSDFParams(2, r.pos.x, r.pos.y, r.size.x, r.size.y, 0, 0, rad, rad, thickness);

    const float aaW = 2.0f;
    float x0 = r.pos.x - aaW, y0 = r.pos.y - aaW;
    float x1 = r.pos.x + r.size.x + aaW, y1 = r.pos.y + r.size.y + aaW;
    Vertex v[4] = {
        {{x0, y0}, {0, 0}, c},
        {{x1, y0}, {1, 0}, c},
        {{x1, y1}, {1, 1}, c},
        {{x0, y1}, {0, 1}, c},
    };
    uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
    dev->drawTriangles(v, 4, idx, 6, 0, false);

    dev->setSDFParams(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

void drawLeftRoundedBorder(RenderDevice* dev, const Rect& r, const Color& c, float thickness, float leftRadius) {
    if (!dev) return;
    float rad = clampRadius(leftRadius, r.size.x, r.size.y);
    if (rad < 1.0f) { drawBorder(dev, r, c, thickness); return; }

    dev->setSDFParams(2, r.pos.x, r.pos.y, r.size.x, r.size.y, rad, 0, 0, rad, thickness);

    const float aaW = 2.0f;
    float x0 = r.pos.x - aaW, y0 = r.pos.y - aaW;
    float x1 = r.pos.x + r.size.x + aaW, y1 = r.pos.y + r.size.y + aaW;
    Vertex v[4] = {
        {{x0, y0}, {0, 0}, c},
        {{x1, y0}, {1, 0}, c},
        {{x1, y1}, {1, 1}, c},
        {{x0, y1}, {0, 1}, c},
    };
    uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
    dev->drawTriangles(v, 4, idx, 6, 0, false);

    dev->setSDFParams(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

void drawRightRoundedBorder(RenderDevice* dev, const Rect& r, const Color& c, float thickness, float rightRadius) {
    if (!dev) return;
    float rad = clampRadius(rightRadius, r.size.x, r.size.y);
    if (rad < 1.0f) { drawBorder(dev, r, c, thickness); return; }

    dev->setSDFParams(2, r.pos.x, r.pos.y, r.size.x, r.size.y, 0, rad, rad, 0, thickness);

    const float aaW = 2.0f;
    float x0 = r.pos.x - aaW, y0 = r.pos.y - aaW;
    float x1 = r.pos.x + r.size.x + aaW, y1 = r.pos.y + r.size.y + aaW;
    Vertex v[4] = {
        {{x0, y0}, {0, 0}, c},
        {{x1, y0}, {1, 0}, c},
        {{x1, y1}, {1, 1}, c},
        {{x0, y1}, {0, 1}, c},
    };
    uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
    dev->drawTriangles(v, 4, idx, 6, 0, false);

    dev->setSDFParams(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

void drawShadow(RenderDevice* dev, const Rect& r, float blur, float offsetY, const Color& c, float radius) {
    if (!dev || blur <= 0) return;
    float rad = clampRadius(radius, r.size.x, r.size.y);

    dev->setSDFParams(3, r.pos.x, r.pos.y + offsetY, r.size.x, r.size.y, rad, rad, rad, rad, blur);

    const float aaW = 2.0f;
    float x0 = r.pos.x - blur - aaW;
    float y0 = r.pos.y + offsetY - blur - aaW;
    float x1 = r.pos.x + r.size.x + blur + aaW;
    float y1 = r.pos.y + offsetY + r.size.y + blur + aaW;
    Vertex v[4] = {
        {{x0, y0}, {0, 0}, c},
        {{x1, y0}, {1, 0}, c},
        {{x1, y1}, {1, 1}, c},
        {{x0, y1}, {0, 1}, c},
    };
    uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
    dev->drawTriangles(v, 4, idx, 6, 0, false);

    dev->setSDFParams(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

void drawBottomShadow(RenderDevice* dev, const Rect& r, float blur, float offsetY, const Color& c, float bottomRadius) {
    if (!dev || blur <= 0) return;
    float rad = clampRadius(bottomRadius, r.size.x, r.size.y);

    dev->setSDFParams(3, r.pos.x, r.pos.y + offsetY, r.size.x, r.size.y, 0, 0, rad, rad, blur);

    const float aaW = 2.0f;
    float x0 = r.pos.x - blur - aaW;
    float y0 = r.pos.y + offsetY - blur - aaW;
    float x1 = r.pos.x + r.size.x + blur + aaW;
    float y1 = r.pos.y + offsetY + r.size.y + blur + aaW;
    Vertex v[4] = {
        {{x0, y0}, {0, 0}, c},
        {{x1, y0}, {1, 0}, c},
        {{x1, y1}, {1, 1}, c},
        {{x0, y1}, {0, 1}, c},
    };
    uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
    dev->drawTriangles(v, 4, idx, 6, 0, false);

    dev->setSDFParams(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}


void drawGradientRect(RenderDevice* dev, const Rect& r, const Color& top, const Color& bottom) {
    if (!dev) return;
    float x0 = r.pos.x, y0 = r.pos.y;
    float x1 = x0 + r.size.x, y1 = y0 + r.size.y;
    Vertex v[4] = {
        {{x0, y0}, {0, 0}, top},
        {{x1, y0}, {1, 0}, top},
        {{x1, y1}, {1, 1}, bottom},
        {{x0, y1}, {0, 1}, bottom},
    };
    uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
    dev->drawTriangles(v, 4, idx, 6, 0, false);
}

void drawRoundedGradientRect(RenderDevice* dev, const Rect& r, const Color& top, const Color& bottom, float radius) {
    if (!dev) return;
    float rad = clampRadius(radius, r.size.x, r.size.y);
    if (rad < 1.0f) { drawGradientRect(dev, r, top, bottom); return; }

    dev->setSDFParams(1, r.pos.x, r.pos.y, r.size.x, r.size.y, rad, rad, rad, rad, 0);

    const float aaW = 2.0f;
    float x0 = r.pos.x - aaW, y0 = r.pos.y - aaW;
    float x1 = r.pos.x + r.size.x + aaW, y1 = r.pos.y + r.size.y + aaW;
    Vertex v[4] = {
        {{x0, y0}, {0, 0}, top},
        {{x1, y0}, {1, 0}, top},
        {{x1, y1}, {1, 1}, bottom},
        {{x0, y1}, {0, 1}, bottom},
    };
    uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
    dev->drawTriangles(v, 4, idx, 6, 0, false);

    dev->setSDFParams(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

// 左右水平渐变圆角矩形（进度条/滑条填充：左 accent → 右 accent2）
void drawRoundedGradientRectH(RenderDevice* dev, const Rect& r, const Color& left, const Color& right, float radius) {
    if (!dev) return;
    float rad = clampRadius(radius, r.size.x, r.size.y);

    if (rad >= 1.0f)
        dev->setSDFParams(1, r.pos.x, r.pos.y, r.size.x, r.size.y, rad, rad, rad, rad, 0);

    const float aaW = 2.0f;
    float x0 = r.pos.x - aaW, y0 = r.pos.y - aaW;
    float x1 = r.pos.x + r.size.x + aaW, y1 = r.pos.y + r.size.y + aaW;
    // 左列 = left 右列 = right：vColor 顶点插值即水平渐变
    Vertex v[4] = {
        {{x0, y0}, {0, 0}, left},
        {{x1, y0}, {1, 0}, right},
        {{x1, y1}, {1, 1}, right},
        {{x0, y1}, {0, 1}, left},
    };
    uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
    dev->drawTriangles(v, 4, idx, 6, 0, false);

    if (rad >= 1.0f)
        dev->setSDFParams(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}


void drawTopRoundedGradientRect(RenderDevice* dev, const Rect& r, const Color& top, const Color& bottom, float topRadius) {
    if (!dev) return;
    float rad = clampRadius(topRadius, r.size.x, r.size.y);
    if (rad < 1.0f) { drawGradientRect(dev, r, top, bottom); return; }

    dev->setSDFParams(1, r.pos.x, r.pos.y, r.size.x, r.size.y, rad, rad, 0, 0, 0);

    const float aaW = 2.0f;
    float x0 = r.pos.x - aaW, y0 = r.pos.y - aaW;
    float x1 = r.pos.x + r.size.x + aaW, y1 = r.pos.y + r.size.y + aaW;
    Vertex v[4] = {
        {{x0, y0}, {0, 0}, top},
        {{x1, y0}, {1, 0}, top},
        {{x1, y1}, {1, 1}, bottom},
        {{x0, y1}, {0, 1}, bottom},
    };
    uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
    dev->drawTriangles(v, 4, idx, 6, 0, false);

    dev->setSDFParams(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}


void drawArc(RenderDevice* dev, float cx, float cy, float radius, float penWidth,
             float startAngleDeg, float sweepAngleDeg, const Color& c, bool roundCap) {
    if (!dev || sweepAngleDeg <= 0 || radius <= 0 || penWidth <= 0) return;
    // SDF 弧描边（模式7，单 draw call 软边 AA；替代旧多边形条带硬边实现）
    drawArcEllipse(dev, cx, cy, radius, radius, penWidth, startAngleDeg, sweepAngleDeg, c);
    if (roundCap) {
        // 圆头：两端各一个 SDF 实心圆（半径 = 半线宽，软边 AA）
        float halfPen = penWidth * 0.5f;
        float aStart = startAngleDeg * 3.14159265f / 180.0f;
        float aEnd = (startAngleDeg + sweepAngleDeg) * 3.14159265f / 180.0f;
        fillCircle(dev, cx + radius * cosf(aStart), cy + radius * sinf(aStart), halfPen, c);
        fillCircle(dev, cx + radius * cosf(aEnd), cy + radius * sinf(aEnd), halfPen, c);
    }
}


void drawText(RenderDevice* dev, Font* font, const char* utf8,
              const Vec2& pos, const Color& c, float scale) {
    if (!dev || !font || !font->isReady() || !utf8) return;
    if (!font->hasAllGlyphs(utf8)) {
        font->ensureGlyphs(dev, utf8);
        if (!font->isReady()) return;
    }
    float actualScale = scale * font->renderScale();
    // 性能优化：顶点/索引缓冲 static thread_local 跨帧复用——clear() 保
    // 容量，文字渲染（每帧最高频路径）零堆分配（旧实现每次调用局部
    // 构造 vector 均触发堆分配+释放）。本函数独占缓冲，与其他图元绘
    // 制函数互不别名（仅自递归不安全，drawText 无自递归）
    static thread_local std::vector<Vertex> verts;
    static thread_local std::vector<uint32_t> idx;
    verts.clear();
    idx.clear();
    verts.reserve(std::strlen(utf8) * 4);
    idx.reserve(std::strlen(utf8) * 6);

    float penX = 0.f;
    float baseline = pos.y + font->ascent() * actualScale;
    const char* s = utf8;
    const char* e = s + std::strlen(utf8);
    while (s < e) {
        uint32_t cp = decodeUtf8(s, e);
        if (!cp) break;
        GlyphQuad q;
        if (!font->getQuad(cp, q)) continue;
        float x0 = pos.x + (penX + q.x0) * actualScale;
        float y0 = baseline + q.y0 * actualScale;
        float x1 = pos.x + (penX + q.x1) * actualScale;
        float y1 = baseline + q.y1 * actualScale;
        uint32_t b = (uint32_t)verts.size();
        verts.push_back({{x0, y0}, {q.u0, q.v0}, c});
        verts.push_back({{x1, y0}, {q.u1, q.v0}, c});
        verts.push_back({{x1, y1}, {q.u1, q.v1}, c});
        verts.push_back({{x0, y1}, {q.u0, q.v1}, c});
        idx.push_back(b + 0); idx.push_back(b + 1); idx.push_back(b + 2);
        idx.push_back(b + 0); idx.push_back(b + 2); idx.push_back(b + 3);
        penX += q.advance;
    }
    if (!verts.empty())
        dev->drawTriangles(verts.data(), (int)verts.size(),
                           idx.data(), (int)idx.size(),
                           font->textureId(), true);
}


// ============================================================
// SDF 图元公共入口：四边形 quad 覆盖图元包围盒（外扩 AA 边距），
// 着色器按 mode 计算距离场做软边抗锯齿（GL/VK 同一管线）：
//   4=椭圆填充  5=椭圆描边  6=线段  7=椭圆弧描边  8=三角形填充
// 参数槽位：模式 1/4/5/7 → (x,y,w,h)=包围矩形（椭圆内切）；
//   6 → (p0,p1)=线段两端点；8 → (p0,p1)=顶点0/1，(tl,tr)=顶点2
// ============================================================
static void sdfQuad(RenderDevice* dev, const Rect& r, const Color& c, int mode,
                    float p0x, float p0y, float p1x, float p1y,
                    float tl, float tr, float br, float bl, float th) {
    if (!dev) return;
    const float aaW = 2.0f;
    Vertex v[4] = {
        {{r.pos.x - aaW, r.pos.y - aaW}, {0, 0}, c},
        {{r.pos.x + r.size.x + aaW, r.pos.y - aaW}, {1, 0}, c},
        {{r.pos.x + r.size.x + aaW, r.pos.y + r.size.y + aaW}, {1, 1}, c},
        {{r.pos.x - aaW, r.pos.y + r.size.y + aaW}, {0, 1}, c},
    };
    uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
    dev->setSDFParams(mode, p0x, p0y, p1x, p1y, tl, tr, br, bl, th);
    dev->drawTriangles(v, 4, idx, 6, 0, false);
    dev->setSDFParams(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

void drawLine(RenderDevice* dev, float x1, float y1, float x2, float y2, float thickness, const Color& c) {
    if (!dev || thickness <= 0) return;
    // SDF 线段（平端点）：p0=端点A p1=端点B，quad=线段包围盒外扩半线宽
    float pad = thickness * 0.5f;
    float x0 = std::min(x1, x2) - pad, y0 = std::min(y1, y2) - pad;
    float xM = std::max(x1, x2) + pad, yM = std::max(y1, y2) + pad;
    sdfQuad(dev, Rect(Vec2(x0, y0), Vec2(xM - x0, yM - y0)), c, 6,
            x1, y1, x2, y2, 0, 0, 0, 0, thickness);
}

void fillCircle(RenderDevice* dev, float cx, float cy, float radius, const Color& c) {
    if (!dev || radius <= 0) return;
    // SDF 椭圆填充：内切于包围矩形（软边 AA，悬浮球/按钮图形共用）
    sdfQuad(dev, Rect(Vec2(cx - radius, cy - radius), Vec2(radius * 2, radius * 2)),
            c, 4, cx - radius, cy - radius, radius * 2, radius * 2, 0, 0, 0, 0, 0);
}

void drawCircle(RenderDevice* dev, float cx, float cy, float radius, float penWidth, const Color& c) {
    if (!dev || radius <= 0 || penWidth <= 0) return;
    // SDF 椭圆描边：内切矩形=椭圆包围盒，quad 外扩 penWidth
    float e = radius + penWidth;
    sdfQuad(dev, Rect(Vec2(cx - e, cy - e), Vec2(e * 2, e * 2)),
            c, 5, cx - radius, cy - radius, radius * 2, radius * 2, 0, 0, 0, 0, penWidth);
}

void fillTriangle(RenderDevice* dev, float x1, float y1, float x2, float y2, float x3, float y3, const Color& c) {
    if (!dev) return;
    // SDF 三角形填充：p0/p1/p2 = 三顶点（软边 AA，图钉尖共用）
    float x0 = std::min(x1, std::min(x2, x3)), xM = std::max(x1, std::max(x2, x3));
    float y0 = std::min(y1, std::min(y2, y3)), yM = std::max(y1, std::max(y2, y3));
    sdfQuad(dev, Rect(Vec2(x0, y0), Vec2(xM - x0, yM - y0)),
            c, 8, x1, y1, x2, y2, x3, y3, 0, 0, 0);
}

void fillEllipse(RenderDevice* dev, float cx, float cy, float rx, float ry, const Color& c) {
    if (!dev || rx <= 0 || ry <= 0) return;
    // SDF 椭圆填充：内切于包围矩形（软边 AA，悬浮球高光/阴影共用）
    sdfQuad(dev, Rect(Vec2(cx - rx, cy - ry), Vec2(rx * 2, ry * 2)),
            c, 4, cx - rx, cy - ry, rx * 2, ry * 2, 0, 0, 0, 0, 0);
}

void drawEllipseOutline(RenderDevice* dev, float cx, float cy, float rx, float ry, float penWidth, const Color& c) {
    if (!dev || rx <= 0 || ry <= 0 || penWidth <= 0) return;
    int segments = std::max(16, (int)(std::max(rx, ry) * 0.5f) + 16);
    for (int i = 0; i < segments; i++) {
        float a1 = i * 2.0f * 3.14159265f / segments;
        float a2 = (i + 1) * 2.0f * 3.14159265f / segments;
        drawLine(dev, cx + rx * cosf(a1), cy + ry * sinf(a1),
                 cx + rx * cosf(a2), cy + ry * sinf(a2), penWidth, c);
    }
}

void fillPolygon(RenderDevice* dev, const Vec2* pts, int count, const Color& c) {
    if (!dev || count < 3) return;
    // 性能优化：缓冲跨帧复用（同 drawText，clear() 保容量零堆分配）
    static thread_local std::vector<uint32_t> idx;
    static thread_local std::vector<Vertex> verts;
    idx.clear();
    verts.clear();
    idx.reserve((count - 2) * 3);
    for (int i = 1; i < count - 1; i++) {
        idx.push_back(0);
        idx.push_back(i);
        idx.push_back(i + 1);
    }
    verts.reserve(count);
    for (int i = 0; i < count; i++) {
        verts.push_back({pts[i], {0, 0}, c});
    }
    dev->drawTriangles(verts.data(), count, idx.data(), (int)idx.size(), 0, false);
}

void fillPolygonConcave(RenderDevice* dev, const Vec2* pts, int count, const Color& c) {
    if (!dev || count < 3) return;
    float cx = 0, cy = 0;
    for (int i = 0; i < count; i++) { cx += pts[i].x; cy += pts[i].y; }
    cx /= count; cy /= count;
    // 性能优化：缓冲跨帧复用（同 fillPolygon）
    static thread_local std::vector<Vertex> verts;
    static thread_local std::vector<uint32_t> idx;
    verts.clear();
    idx.clear();
    verts.reserve(count + 1);
    idx.reserve(count * 3);
    verts.push_back({{cx, cy}, {0, 0}, c});
    for (int i = 0; i < count; i++) {
        verts.push_back({pts[i], {0, 0}, c});
    }
    for (int i = 0; i < count; i++) {
        idx.push_back(0);
        idx.push_back(1 + i);
        idx.push_back(1 + (i + 1) % count);
    }
    dev->drawTriangles(verts.data(), (int)verts.size(), idx.data(), (int)idx.size(), 0, false);
}

void drawPolygonOutline(RenderDevice* dev, const Vec2* pts, int count, float penWidth, const Color& c) {
    if (!dev || count < 2) return;
    for (int i = 0; i < count; i++) {
        int j = (i + 1) % count;
        drawLine(dev, pts[i].x, pts[i].y, pts[j].x, pts[j].y, penWidth, c);
    }
}

void fillPie(RenderDevice* dev, float cx, float cy, float rx, float ry, float startAngleDeg, float sweepAngleDeg, const Color& c) {
    if (!dev || rx <= 0 || ry <= 0 || sweepAngleDeg <= 0) return;
    int segments = std::max(4, (int)(sweepAngleDeg / 5.0f) + 4);
    // 性能优化：缓冲跨帧复用（同 fillPolygon）
    static thread_local std::vector<Vertex> verts;
    static thread_local std::vector<uint32_t> idx;
    verts.clear();
    idx.clear();
    verts.reserve(segments + 1);
    idx.reserve(segments * 3);
    verts.push_back({{cx, cy}, {0, 0}, c});
    float startRad = startAngleDeg * 3.14159265f / 180.0f;
    for (int i = 0; i <= segments; i++) {
        float a = startRad + (sweepAngleDeg * 3.14159265f / 180.0f) * i / segments;
        verts.push_back({{cx + rx * cosf(a), cy + ry * sinf(a)}, {0, 0}, c});
    }
    for (int i = 0; i < segments; i++) {
        idx.push_back(0);
        idx.push_back(1 + i);
        idx.push_back(1 + i + 1);
    }
    dev->drawTriangles(verts.data(), (int)verts.size(), idx.data(), (int)idx.size(), 0, false);
}

void drawArcEllipse(RenderDevice* dev, float cx, float cy, float rx, float ry, float penWidth, float startAngleDeg, float sweepAngleDeg, const Color& c) {
    if (!dev || rx <= 0 || ry <= 0 || penWidth <= 0 || sweepAngleDeg <= 0) return;
    // SDF 椭圆弧描边：内切矩形=椭圆包围盒，tr=起始角 br=扫过角，单 draw call 整弧软边
    float e = std::max(rx, ry) + penWidth;
    sdfQuad(dev, Rect(Vec2(cx - e, cy - e), Vec2(e * 2, e * 2)),
            c, 7, cx - rx, cy - ry, rx * 2, ry * 2, 0, startAngleDeg, sweepAngleDeg, 0, penWidth);
}

void drawDiagonalGradientRect(RenderDevice* dev, const Rect& r, const Color& c1, const Color& c2) {
    if (!dev) return;
    Color mid = {(c1.r + c2.r) * 0.5f, (c1.g + c2.g) * 0.5f, (c1.b + c2.b) * 0.5f, (c1.a + c2.a) * 0.5f};
    float x0 = r.pos.x, y0 = r.pos.y;
    float x1 = x0 + r.size.x, y1 = y0 + r.size.y;
    Vertex v[4] = {
        {{x0, y0}, {0, 0}, c1},
        {{x1, y0}, {1, 0}, mid},
        {{x1, y1}, {1, 1}, c2},
        {{x0, y1}, {0, 1}, mid},
    };
    uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
    dev->drawTriangles(v, 4, idx, 6, 0, false);
}

void drawHorizontalGradientRect(RenderDevice* dev, const Rect& r, const Color& left, const Color& right) {
    if (!dev) return;
    float x0 = r.pos.x, y0 = r.pos.y;
    float x1 = x0 + r.size.x, y1 = y0 + r.size.y;
    Vertex v[4] = {
        {{x0, y0}, {0, 0}, left},
        {{x1, y0}, {1, 0}, right},
        {{x1, y1}, {1, 1}, right},
        {{x0, y1}, {0, 1}, left},
    };
    uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
    dev->drawTriangles(v, 4, idx, 6, 0, false);
}

void fillRadialGradientCircle(RenderDevice* dev, float cx, float cy, float maxR, const Color& centerColor, const Color& edgeColor) {
    if (!dev || maxR <= 0) return;
    int rings = 8;
    for (int i = rings; i >= 1; i--) {
        float t = (float)i / rings;
        float r = maxR * t;
        Color c = {
            centerColor.r + (edgeColor.r - centerColor.r) * t,
            centerColor.g + (edgeColor.g - centerColor.g) * t,
            centerColor.b + (edgeColor.b - centerColor.b) * t,
            centerColor.a + (edgeColor.a - centerColor.a) * t
        };
        fillCircle(dev, cx, cy, r, c);
    }
    fillCircle(dev, cx, cy, maxR * 0.1f, centerColor);
}

} // namespace ui