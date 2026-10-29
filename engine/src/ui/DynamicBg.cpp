#include "DynamicBg.h"
#include "Element.h"
#include <cmath>
#include <GLFW/glfw3.h>

namespace ui {

static const float PI = 3.14159265f;
static const float TAU = 6.28318f;

DynamicBgConfig g_dynBg = { DYN_BG_NONE, 1.0f, 50, 0, false };
static float g_dynBgTime = 0.0f;

void updateDynamicBg(float deltaMs) {
    g_dynBgTime += deltaMs * g_dynBg.speed * 0.001f;
}

void resetDynamicBg() {
    g_dynBg.type = DYN_BG_NONE;
    g_dynBg.speed = 1.0f;
    g_dynBg.density = 50;
    g_dynBg.colorScheme = 0;
    g_dynBg.enabled = false;
    g_dynBgTime = 0.0f;
}

// ===== Helper functions =====

static void fillRectDyn(RenderDevice* dev, float x1, float y1, float x2, float y2, int r, int gg, int b, int a) {
    drawRect(dev, Rect(x1, y1, x2 - x1, y2 - y1), Color::rgb(r, gg, b, a));
}

static void drawLineDyn(RenderDevice* dev, float x1, float y1, float x2, float y2, int r, int gg, int b, int a, float thick) {
    drawLine(dev, x1, y1, x2, y2, thick, Color::rgb(r, gg, b, a));
}

static void fillCircleDyn(RenderDevice* dev, float cx, float cy, float radius, int r, int gg, int b, int a) {
    if (radius < 0.5f) return;
    fillEllipse(dev, cx, cy, radius, radius, Color::rgb(r, gg, b, a));
}

static void drawCircleDyn(RenderDevice* dev, float cx, float cy, float radius, int r, int gg, int b, int a, float thick) {
    if (radius < 0.5f) return;
    drawEllipseOutline(dev, cx, cy, radius, radius, thick, Color::rgb(r, gg, b, a));
}

static void fillQuadDyn(RenderDevice* dev, Vec2 p1, Vec2 p2, Vec2 p3, Vec2 p4, int r, int gg, int b, int a) {
    Vec2 pts[4] = {p1, p2, p3, p4};
    fillPolygon(dev, pts, 4, Color::rgb(r, gg, b, a));
}

static void fillPolyDyn(RenderDevice* dev, Vec2* pts, int count, int r, int gg, int b, int a) {
    fillPolygonConcave(dev, pts, count, Color::rgb(r, gg, b, a));
}

static void drawPolyClosedDyn(RenderDevice* dev, Vec2* pts, int count, int r, int gg, int b, int a, float thick) {
    drawPolygonOutline(dev, pts, count, thick, Color::rgb(r, gg, b, a));
}

static void fillTriangleDyn(RenderDevice* dev, Vec2 p1, Vec2 p2, Vec2 p3, int r, int gg, int b, int a) {
    fillTriangle(dev, p1.x, p1.y, p2.x, p2.y, p3.x, p3.y, Color::rgb(r, gg, b, a));
}

// ===== 1 Polar Night =====
static void drawDynPolarNight(RenderDevice* dev, int w, int h) {
    float t = g_dynBgTime;
    float sw = (float)w, sh = (float)h;
    int steps = 50;
    float stepH = sh / steps;
    for (int i = 0; i < steps; i++) {
        float f = (float)i / (steps - 1);
        int r = (int)(4 + (8 - 4) * f);
        int gg = (int)(10 + (16 - 10) * f);
        int b = (int)(24 + (36 - 24) * f);
        fillRectDyn(dev, 0, i * stepH, sw, i * stepH + stepH + 1, r, gg, b, 255);
    }
    for (int i = 0; i < 60; i++) {
        uint32_t hh = (uint32_t)i * 2654435761u;
        float sx = (hh % 1000) / 1000.0f * sw;
        float sy = ((hh >> 10) % 1000) / 1000.0f * sh * 0.4f;
        float tw = 0.5f + 0.5f * sinf(t * 1.0f + i * 0.6f);
        fillRectDyn(dev, sx, sy, sx + 1.5f, sy + 1.5f, 200, 220, 255, (int)(40 + 100 * tw));
    }
    int bands = 3;
    for (int band = 0; band < bands; band++) {
        float baseY = sh * (0.12f + 0.08f * band);
        float amp = 50.0f + 15.0f * band;
        float phase = t * (0.6f + 0.2f * band) + band * 2.0f;
        int nr = 0, ng = 0, nb = 0;
        if (band == 0) { nr = 10; ng = 220; nb = 120; }
        else if (band == 1) { nr = 20; ng = 180; nb = 200; }
        else { nr = 120; ng = 60; nb = 200; }
        int segs = 100;
        for (int i = 0; i < segs; i++) {
            float f0 = (float)i / segs;
            float f1 = (float)(i + 1) / segs;
            float x0 = f0 * sw;
            float x1 = f1 * sw;
            float wave = sinf(f0 * 5.0f + phase) * amp + sinf(f0 * 11.0f + phase * 1.3f) * amp * 0.3f;
            float y0 = baseY + wave;
            float y1 = baseY + sinf(f1 * 5.0f + phase) * amp + sinf(f1 * 11.0f + phase * 1.3f) * amp * 0.3f;
            float env = sinf(f0 * PI);
            int a = (int)(130 * env);
            int thick = 8 + (int)(6 * env);
            fillQuadDyn(dev,
                Vec2(x0, y0 - thick), Vec2(x1, y1 - thick),
                Vec2(x1, y1 + thick), Vec2(x0, y0 + thick),
                nr, ng, nb, a);
            fillQuadDyn(dev,
                Vec2(x0, y0 - thick * 2), Vec2(x1, y1 - thick * 2),
                Vec2(x1, y1 + thick * 2), Vec2(x0, y0 + thick * 2),
                nr, ng, nb, a / 4);
        }
    }
    float mtnY = sh * 0.82f;
    int mtnCount = 12;
    float mtnW = sw / mtnCount;
    for (int i = 0; i <= mtnCount; i++) {
        uint32_t hh = (uint32_t)i * 40503u;
        float peakH = 60.0f + (hh % 80);
        float bx = i * mtnW;
        float prevBx = (i - 1) * mtnW;
        fillTriangleDyn(dev,
            Vec2(prevBx, mtnY), Vec2(bx, mtnY),
            Vec2(prevBx + mtnW * 0.5f, mtnY - peakH),
            15, 20, 35, 255);
    }
    fillRectDyn(dev, 0, mtnY, sw, sh, 20, 28, 45, 255);
}

// ===== 2 Water Wave =====
static void drawDynWaterWave(RenderDevice* dev, int w, int h) {
    float t = g_dynBgTime;
    float sw = (float)w, sh = (float)h;
    fillRectDyn(dev, 0, 0, sw, sh, 4, 10, 24, 255);
    int sources = 5;
    for (int src = 0; src < sources; src++) {
        uint32_t hh = (uint32_t)src * 2654435761u;
        float sx = (hh % 1000) / 1000.0f * sw;
        float sy = ((hh >> 10) % 1000) / 1000.0f * sh;
        float cycle = 3.0f + (hh % 100) / 100.0f;
        float phase = fmodf(t + src * 0.7f, cycle) / cycle;
        for (int ring = 0; ring < 4; ring++) {
            float r = phase * sw * 0.5f + ring * 40.0f;
            if (r < 5 || r > sw * 0.7f) continue;
            float fade = 1.0f - phase;
            int a = (int)(180 * fade * (1.0f - (float)ring / 4));
            drawCircleDyn(dev, sx, sy, r, 80, 180, 220, a, 2.0f);
        }
    }
}

// ===== 3 Hexagon =====
static void drawDynHexagon(RenderDevice* dev, int w, int h) {
    float t = g_dynBgTime;
    float sw = (float)w, sh = (float)h;
    fillRectDyn(dev, 0, 0, sw, sh, 6, 8, 14, 255);
    float hexR = 26.0f;
    float hexW = hexR * 1.732f;
    float hexH = hexR * 1.5f;
    for (int row = -1; row < (int)(sh / hexH) + 2; row++) {
        for (int col = -1; col < (int)(sw / hexW) + 2; col++) {
            float cx = col * hexW + (row & 1 ? hexW * 0.5f : 0);
            float cy = row * hexH;
            uint32_t hh = (uint32_t)(row * 73856093u) ^ (uint32_t)(col * 19349663u);
            float pulse = 0.5f + 0.5f * sinf(t * 1.2f + (float)(hh % 1000) * 0.01f);
            int bright = (int)(20 + 50 * pulse);
            Vec2 pts[6];
            for (int k = 0; k < 6; k++) {
                float a = (float)k / 6.0f * TAU + 0.5236f;
                pts[k] = Vec2(cx + cosf(a) * hexR * 0.9f, cy + sinf(a) * hexR * 0.9f);
            }
            fillPolyDyn(dev, pts, 6, bright, bright + 8, bright + 16, 255);
            drawPolyClosedDyn(dev, pts, 6, bright + 30, bright + 40, bright + 50, 200, 1.0f);
        }
    }
}

// ===== 4 Bubble =====
static void drawDynBubble(RenderDevice* dev, int w, int h) {
    float t = g_dynBgTime;
    float sw = (float)w, sh = (float)h;
    int steps = 40;
    float stepH = sh / steps;
    for (int i = 0; i < steps; i++) {
        float f = (float)i / (steps - 1);
        int r = (int)(4 + (10 - 4) * f);
        int gg = (int)(20 + (35 - 20) * f);
        int b = (int)(35 + (50 - 35) * f);
        fillRectDyn(dev, 0, i * stepH, sw, i * stepH + stepH + 1, r, gg, b, 255);
    }
    int bubbles = 50;
    for (int i = 0; i < bubbles; i++) {
        uint32_t hh = (uint32_t)i * 2654435761u;
        float baseX = (hh % 1000) / 1000.0f * sw;
        float speed = 30.0f + ((hh >> 10) % 1000) / 1000.0f * 60.0f;
        float wobble = sinf(t * 1.5f + i * 0.4f) * 25.0f;
        float yy = fmodf(t * speed + i * 41.0f, sh + 40);
        float xx = baseX + wobble;
        yy = sh - yy;
        float br = 4.0f + ((hh >> 20) % 1000) / 1000.0f * 16.0f;
        drawCircleDyn(dev, xx, yy, br, 180, 230, 255, 160, 1.5f);
        fillCircleDyn(dev, xx - br * 0.3f, yy - br * 0.3f, br * 0.25f, 220, 240, 255, 120);
    }
}

// ===== 5 Star Vortex =====
static void drawDynStarVortex(RenderDevice* dev, int w, int h) {
    float t = g_dynBgTime;
    float sw = (float)w, sh = (float)h;
    fillRectDyn(dev, 0, 0, sw, sh, 1, 1, 4, 255);
    float cx = sw * 0.5f;
    float cy = sh * 0.5f;
    float maxR = sw * 0.5f;
    int stars = 120;
    for (int i = 0; i < stars; i++) {
        uint32_t hh = (uint32_t)i * 2654435761u;
        float baseR = 30.0f + ((hh >> 10) % 1000) / 1000.0f * maxR;
        float baseAngle = (hh % 1000) / 1000.0f * TAU;
        float orbitSpeed = 1.5f / (baseR / maxR + 0.2f);
        float angle = baseAngle + t * orbitSpeed;
        int trailSegs = 8;
        for (int s = 0; s < trailSegs; s++) {
            float trailF = (float)s / trailSegs;
            float a = angle - trailF * 0.15f * orbitSpeed;
            float r = baseR + trailF * 3.0f;
            float x0 = cx + cosf(a) * r;
            float y0 = cy + sinf(a) * r;
            float a2 = angle - (float)(s + 1) / trailSegs * 0.15f * orbitSpeed;
            float r2 = baseR + (float)(s + 1) / trailSegs * 3.0f;
            float x1 = cx + cosf(a2) * r2;
            float y1 = cy + sinf(a2) * r2;
            int alpha = (int)(200 * (1.0f - trailF));
            drawLineDyn(dev, x0, y0, x1, y1, 200, 220, 255, alpha, 1.5f);
        }
        float sx = cx + cosf(angle) * baseR;
        float sy = cy + sinf(angle) * baseR;
        int sz = 1 + (int)(((hh >> 20) % 100) / 100.0f * 2.5f);
        fillRectDyn(dev, sx - sz, sy - sz, sx + sz, sy + sz, 255, 255, 230, 255);
    }
    int diskSegs = 64;
    for (int ring = 0; ring < 5; ring++) {
        float r0 = 20.0f + ring * 8.0f;
        float r1 = 24.0f + ring * 8.0f;
        int cr = 255;
        int cg = 180 - ring * 25;
        int cb = 40 + ring * 10;
        if (cg < 60) cg = 60;
        for (int i = 0; i < diskSegs; i++) {
            float a0 = (float)i / diskSegs * TAU + t * (2.0f - ring * 0.2f);
            float a1 = (float)(i + 1) / diskSegs * TAU + t * (2.0f - ring * 0.2f);
            int a = 200 - ring * 30;
            fillQuadDyn(dev,
                Vec2(cx + cosf(a0) * r0, cy + sinf(a0) * r0 * 0.3f),
                Vec2(cx + cosf(a1) * r0, cy + sinf(a1) * r0 * 0.3f),
                Vec2(cx + cosf(a1) * r1, cy + sinf(a1) * r1 * 0.3f),
                Vec2(cx + cosf(a0) * r1, cy + sinf(a0) * r1 * 0.3f),
                cr, cg, cb, a);
        }
    }
    fillCircleDyn(dev, cx, cy, 18.0f, 0, 0, 0, 255);
    drawCircleDyn(dev, cx, cy, 20.0f, 255, 200, 100, 200, 2.0f);
    drawCircleDyn(dev, cx, cy, 28.0f, 255, 150, 50, 80, 1.0f);
}

// ===== 6 Audio Spectrum =====
static void drawDynAudioSpectrum(RenderDevice* dev, int w, int h) {
    float t = g_dynBgTime;
    float sw = (float)w, sh = (float)h;
    fillRectDyn(dev, 0, 0, sw, sh, 4, 4, 8, 255);
    int bars = 64;
    float barW = sw / bars;
    float baseY = sh * 0.85f;
    for (int i = 0; i < bars; i++) {
        float freq = (float)i / bars;
        float h1 = sinf(t * 3.0f + freq * 10.0f) * 0.5f + 0.5f;
        float h2 = sinf(t * 5.0f + freq * 6.0f + 1.0f) * 0.5f + 0.5f;
        float h3 = sinf(t * 2.0f + freq * 15.0f + 2.0f) * 0.5f + 0.5f;
        float amp = (h1 * 0.5f + h2 * 0.3f + h3 * 0.2f);
        float envelope = sinf(freq * PI);
        float barH = amp * envelope * sh * 0.6f + 10.0f;
        int r = (int)(50 + 200 * freq);
        int gg = (int)(200 - 100 * freq);
        int b = (int)(200 + 55 * (1.0f - freq));
        fillRectDyn(dev, i * barW + 1, baseY - barH, (i + 1) * barW - 1, baseY, r, gg, b, 220);
        fillRectDyn(dev, i * barW + 1, baseY + barH * 0.3f, (i + 1) * barW - 1, baseY + barH * 0.3f + 3, r, gg, b, 100);
    }
    int waveSegs = 100;
    float prevWX = 0, prevWY = 0;
    for (int i = 0; i <= waveSegs; i++) {
        float f = (float)i / waveSegs;
        float x = f * sw;
        float wave1 = sinf(t * 4.0f + f * 20.0f) * 30.0f;
        float wave2 = sinf(t * 6.0f + f * 8.0f + 1.5f) * 20.0f;
        float y = sh * 0.3f + wave1 + wave2;
        if (i > 0) {
            drawLineDyn(dev, prevWX, prevWY, x, y, 0, 255, 200, 180, 2.0f);
        }
        prevWX = x; prevWY = y;
    }
}

// ===== 7 Wave Interference =====
static void drawDynWaveInterfer(RenderDevice* dev, int w, int h) {
    float t = g_dynBgTime;
    float sw = (float)w, sh = (float)h;
    fillRectDyn(dev, 0, 0, sw, sh, 3, 6, 16, 255);
    int sources = 4;
    float srcX[4], srcY[4];
    for (int i = 0; i < sources; i++) {
        uint32_t hh = (uint32_t)i * 2654435761u;
        srcX[i] = (hh % 1000) / 1000.0f * sw;
        srcY[i] = ((hh >> 10) % 1000) / 1000.0f * sh;
    }
    int rings = 12;
    for (int src = 0; src < sources; src++) {
        for (int ring = 0; ring < rings; ring++) {
            float phase = fmodf(t * 0.8f + ring * 0.5f + src * 0.3f, (float)rings) / rings;
            float r = phase * sw * 0.6f;
            if (r < 5) continue;
            float fade = 1.0f - phase;
            int a = (int)(120 * fade);
            int cr = 60 + src * 40;
            int cg = 140 + src * 20;
            int cb = 200 - src * 30;
            drawCircleDyn(dev, srcX[src], srcY[src], r, cr, cg, cb, a, 1.5f);
        }
    }
    int gridStep = 8;
    for (int y = 0; y < sh; y += gridStep) {
        for (int x = 0; x < sw; x += gridStep) {
            float total = 0;
            for (int src = 0; src < sources; src++) {
                float dx = x - srcX[src], dy = y - srcY[src];
                float dist = sqrtf(dx * dx + dy * dy);
                total += sinf(dist * 0.05f - t * 2.0f);
            }
            float intensity = (total / sources) * 0.5f + 0.5f;
            if (intensity > 0.7f) {
                fillRectDyn(dev, x, y, x + gridStep, y + gridStep,
                    (int)(40 + 80 * intensity), (int)(100 + 100 * intensity),
                    (int)(160 + 95 * intensity), (int)(200 * (intensity - 0.7f) / 0.3f));
            }
        }
    }
    for (int src = 0; src < sources; src++) {
        fillCircleDyn(dev, srcX[src], srcY[src], 5.0f, 255, 255, 200, 255);
    }
}

// ===== 8 Digital Rain =====
static void drawDynDigitalRain(RenderDevice* dev, int w, int h) {
    float t = g_dynBgTime;
    float sw = (float)w, sh = (float)h;
    fillRectDyn(dev, 0, 0, sw, sh, 0, 2, 4, 255);
    int cols = 40;
    float colW = sw / cols;
    int charSize = 14;
    for (int c = 0; c < cols; c++) {
        uint32_t hh = (uint32_t)c * 2654435761u;
        float speed = 30.0f + ((hh >> 10) % 1000) / 1000.0f * 60.0f;
        float offset = ((hh % 1000) / 1000.0f) * sh;
        float headY = fmodf(t * speed + offset, sh + 100.0f) - 50.0f;
        int trailLen = 15 + (hh % 10);
        for (int tr = 0; tr < trailLen; tr++) {
            float y = headY - tr * charSize;
            if (y < 0 || y > sh) continue;
            float fade = 1.0f - (float)tr / trailLen;
            int a = (int)(255 * fade * fade);
            int r = 0, gg, b;
            if (tr == 0) { gg = 255; b = 255; r = 200; }
            else if (tr < 3) { gg = 255; b = 180; r = 0; }
            else { gg = (int)(180 + 75 * fade); b = (int)(40 + 60 * fade); r = 0; }
            float x = c * colW + colW * 0.5f;
            int glyphType = (int)fmodf(t * 3.0f + c * 7.0f + tr * 11.0f, 6.0f);
            float size = charSize * 0.35f;
            float rot = glyphType * 0.52f + t * 0.5f;
            if (glyphType < 3) {
                Vec2 pts[3];
                for (int k = 0; k < 3; k++) {
                    float a2 = (float)k / 3 * TAU + rot;
                    pts[k] = Vec2(x + cosf(a2) * size, y + sinf(a2) * size);
                }
                fillPolyDyn(dev, pts, 3, r, gg, b, a);
            } else if (glyphType < 5) {
                Vec2 pts[4];
                for (int k = 0; k < 4; k++) {
                    float a2 = (float)k / 4 * TAU + rot;
                    pts[k] = Vec2(x + cosf(a2) * size, y + sinf(a2) * size);
                }
                fillPolyDyn(dev, pts, 4, r, gg, b, a);
            } else {
                fillRectDyn(dev, x - size, y - size * 0.5f, x + size, y + size * 0.5f, r, gg, b, a);
            }
        }
    }
}

// ===== 9 Energy Shield =====
static void drawDynEnergyShield(RenderDevice* dev, int w, int h) {
    float t = g_dynBgTime;
    float sw = (float)w, sh = (float)h;
    fillRectDyn(dev, 0, 0, sw, sh, 2, 5, 15, 255);
    float cx = sw * 0.5f, cy = sh * 0.5f;
    float shieldR = sw * 0.35f;
    int hexRings = 4;
    for (int ring = hexRings; ring >= 0; ring--) {
        float ringR = shieldR * (float)ring / hexRings;
        int hexCount = 6 + ring * 4;
        for (int hex = 0; hex < hexCount; hex++) {
            float hexAngle = (float)hex / hexCount * TAU + t * 0.1f * (ring + 1);
            float hexCx = cx + cosf(hexAngle) * ringR;
            float hexCy = cy + sinf(hexAngle) * ringR;
            float hexSize = 12.0f + sinf(t * 2.0f + hex * 0.5f + ring) * 4.0f;
            float pulse = sinf(t * 1.5f + ring * 0.8f + hex * 0.3f) * 0.5f + 0.5f;
            Vec2 hexPts[6];
            for (int k = 0; k < 6; k++) {
                float a = (float)k / 6 * TAU + t * 0.2f;
                hexPts[k] = Vec2(hexCx + cosf(a) * hexSize, hexCy + sinf(a) * hexSize);
            }
            int cr = (int)(0 + 100 * pulse);
            int cg = (int)(150 + 105 * pulse);
            int cb = (int)(200 + 55 * pulse);
            drawPolyClosedDyn(dev, hexPts, 6, cr, cg, cb, (int)(100 + 100 * pulse), 1.5f);
            if (pulse > 0.6f) {
                fillPolyDyn(dev, hexPts, 6, cr * 0.3f, cg * 0.3f, cb * 0.3f, (int)(60 * pulse));
            }
        }
    }
    int arcs = 8;
    for (int arc = 0; arc < arcs; arc++) {
        float arcAngle = (float)arc / arcs * TAU + t * 0.5f;
        float arcR1 = shieldR * 0.3f;
        float arcR2 = shieldR * 1.1f;
        float pulse = sinf(t * 3.0f + arc * 0.8f) * 0.5f + 0.5f;
        drawLineDyn(dev, cx + cosf(arcAngle) * arcR1, cy + sinf(arcAngle) * arcR1,
                  cx + cosf(arcAngle) * arcR2, cy + sinf(arcAngle) * arcR2,
                  0, 200, 255, (int)(80 + 120 * pulse), 2.0f);
    }
    fillCircleDyn(dev, cx, cy, 8.0f + sinf(t * 4.0f) * 3.0f, 0, 255, 220, 255);
    fillCircleDyn(dev, cx, cy, 20.0f + sinf(t * 4.0f) * 5.0f, 0, 200, 255, 60);
}

// ===== 10 Honeycomb Matrix =====
static void drawDynHoneycombMatrix(RenderDevice* dev, int w, int h) {
    float t = g_dynBgTime;
    float sw = (float)w, sh = (float)h;
    fillRectDyn(dev, 0, 0, sw, sh, 4, 6, 14, 255);
    float hexSize = 25.0f;
    float hexW = hexSize * 1.732f;
    float hexH = hexSize * 1.5f;
    int cols = (int)(sw / hexW) + 2;
    int rows = (int)(sh / hexH) + 2;
    float cx = sw * 0.5f, cy = sh * 0.5f;
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            float hx = c * hexW + (r % 2) * hexW * 0.5f;
            float hy = r * hexH;
            float dx = hx - cx, dy = hy - cy;
            float dist = sqrtf(dx * dx + dy * dy);
            float wave = sinf(t * 1.5f - dist * 0.01f) * 0.5f + 0.5f;
            float pulse = sinf(t * 2.0f + dist * 0.008f) * 0.5f + 0.5f;
            float size = hexSize * (0.7f + 0.3f * wave);
            Vec2 hexPts[6];
            for (int k = 0; k < 6; k++) {
                float a = (float)k / 6 * TAU + 0.5236f;
                hexPts[k] = Vec2(hx + cosf(a) * size, hy + sinf(a) * size);
            }
            int cr = (int)(40 + 80 * wave);
            int cg = (int)(100 + 120 * pulse);
            int cb = (int)(30 + 60 * (1.0f - pulse));
            drawPolyClosedDyn(dev, hexPts, 6, cr, cg, cb, (int)(80 + 100 * wave), 1.5f);
            if (wave > 0.6f) {
                fillPolyDyn(dev, hexPts, 6, cr * 0.4f, cg * 0.4f, cb * 0.4f, (int)(80 * wave));
            }
            if (pulse > 0.8f) {
                fillCircleDyn(dev, hx, hy, 3.0f, 200, 255, 150, 200);
            }
        }
    }
}

// ===== 11 Ripple Wave =====
static void drawDynRippleWave(RenderDevice* dev, int w, int h) {
    float t = g_dynBgTime;
    float sw = (float)w, sh = (float)h;
    fillRectDyn(dev, 0, 0, sw, sh, 5, 10, 20, 255);
    int drops = 6;
    struct Drop { float x, y; float phase; };
    Drop dropsArr[6];
    for (int i = 0; i < drops; i++) {
        uint32_t hh = (uint32_t)i * 2654435761u;
        dropsArr[i].x = (hh % 1000) / 1000.0f * sw;
        dropsArr[i].y = ((hh >> 10) % 1000) / 1000.0f * sh;
        dropsArr[i].phase = (hh % 100) / 100.0f * 3.0f;
    }
    for (int d = 0; d < drops; d++) {
        float cycle = 2.0f;
        float phase = fmodf(t + dropsArr[d].phase, cycle) / cycle;
        for (int ring = 0; ring < 5; ring++) {
            float ringPhase = phase + ring * 0.15f;
            if (ringPhase > 1.0f) continue;
            float r = ringPhase * sw * 0.3f;
            float fade = 1.0f - ringPhase;
            int a = (int)(180 * fade);
            drawCircleDyn(dev, dropsArr[d].x, dropsArr[d].y, r, 100, 180, 255, a, 2.0f);
            drawCircleDyn(dev, dropsArr[d].x, dropsArr[d].y, r * 0.7f, 150, 200, 255, a / 2, 1.0f);
        }
        if (phase < 0.1f) {
            fillCircleDyn(dev, dropsArr[d].x, dropsArr[d].y, 5.0f * (1.0f - phase / 0.1f), 200, 220, 255, 200);
        }
    }
    for (int y = 0; y < sh; y += 4) {
        for (int x = 0; x < sw; x += 4) {
            float total = 0;
            for (int d = 0; d < drops; d++) {
                float dx = x - dropsArr[d].x;
                float dy = y - dropsArr[d].y;
                float dist = sqrtf(dx * dx + dy * dy);
                total += sinf(dist * 0.04f - t * 3.0f + dropsArr[d].phase) / (1.0f + dist * 0.005f);
            }
            float intensity = total * 0.5f + 0.5f;
            if (intensity > 0.55f) {
                fillRectDyn(dev, x, y, x + 4, y + 4,
                    (int)(40 + 60 * intensity), (int)(80 + 100 * intensity),
                    (int)(140 + 115 * intensity), (int)(120 * (intensity - 0.55f) / 0.45f));
            }
        }
    }
}

// ===== 12 Spectrum Wave =====
static void drawDynSpectrumWave(RenderDevice* dev, int w, int h) {
    float t = g_dynBgTime;
    float sw = (float)w, sh = (float)h;
    fillRectDyn(dev, 0, 0, sw, sh, 2, 2, 8, 255);
    int bars = 64;
    float barW = sw / bars;
    for (int bar = 0; bar < bars; bar++) {
        float f = (float)bar / bars;
        float wave1 = sinf(t * 2.0f + f * TAU * 2.0f) * 0.5f + 0.5f;
        float wave2 = sinf(t * 1.5f + f * TAU * 3.0f + 1.0f) * 0.5f + 0.5f;
        float wave3 = sinf(t * 3.0f + f * TAU + 2.0f) * 0.5f + 0.5f;
        float height = (wave1 * 0.4f + wave2 * 0.3f + wave3 * 0.3f) * sh * 0.6f;
        float x = bar * barW;
        int cr = (int)(255 * (1.0f - f));
        int cg = (int)(255 * (f < 0.5f ? f * 2.0f : 1.0f - (f - 0.5f) * 2.0f));
        int cb = (int)(255 * f);
        fillRectDyn(dev, x, sh - height, x + barW - 1, sh, cr, cg, cb, 220);
        fillRectDyn(dev, x, sh - height - 5, x + barW - 1, sh - height, cr, cg, cb, 255);
        float mirrorHeight = height * 0.3f;
        fillRectDyn(dev, x, 0, x + barW - 1, mirrorHeight, cr, cg, cb, 60);
        for (int particle = 0; particle < 3; particle++) {
            float py = sh - height + (float)particle / 3 * height;
            float px = x + barW * 0.5f + sinf(t * 4.0f + bar * 0.2f + particle) * barW * 0.3f;
            fillCircleDyn(dev, px, py, 1.5f, 255, 255, 255, 150);
        }
    }
    for (int line = 0; line < 3; line++) {
        float lineY = sh * (0.3f + 0.15f * line);
        Vec2 prev;
        bool hasPrev = false;
        for (int x = 0; x <= sw; x += 8) {
            float f = (float)x / sw;
            float wave = sinf(t * (1.0f + line * 0.5f) + f * TAU * (2.0f + line)) * 20.0f;
            float y = lineY + wave;
            if (hasPrev) {
                int cr = (int)(100 + 155 * sinf(t + line));
                int cg = (int)(50 + 100 * cosf(t * 0.7f + line * 0.5f));
                int cb = (int)(150 + 105 * sinf(t * 0.5f + line * 0.3f));
                drawLineDyn(dev, prev.x, prev.y, (float)x, y, cr, cg, cb, 150, 2.0f);
            }
            prev = Vec2((float)x, y);
            hasPrev = true;
        }
    }
}

// ===== 13 Matrix Code =====
static void drawDynMatrixCode(RenderDevice* dev, int w, int h) {
    float t = g_dynBgTime;
    float sw = (float)w, sh = (float)h;
    fillRectDyn(dev, 0, 0, sw, sh, 0, 1, 3, 255);
    int cols = 50;
    float colW = sw / cols;
    int glyphSize = 12;
    for (int c = 0; c < cols; c++) {
        uint32_t hh = (uint32_t)c * 2654435761u;
        float speed = 40.0f + ((hh >> 10) % 1000) / 1000.0f * 80.0f;
        float offset = ((hh % 1000) / 1000.0f) * sh;
        float headY = fmodf(t * speed + offset, sh + 200.0f) - 100.0f;
        int trailLen = 20 + (hh % 15);
        for (int tr = 0; tr < trailLen; tr++) {
            float y = headY - tr * glyphSize;
            if (y < 0 || y > sh) continue;
            float fade = 1.0f - (float)tr / trailLen;
            int a = (int)(255 * fade * fade);
            int r = 0, gg, b;
            if (tr == 0) { gg = 255; b = 255; r = 200; }
            else if (tr < 3) { gg = 255; b = 180; r = 0; }
            else { gg = (int)(180 + 75 * fade); b = (int)(40 + 60 * fade); r = 0; }
            float x = c * colW + colW * 0.5f;
            int glyphType = (int)fmodf(t * 3.0f + c * 7.0f + tr * 11.0f, 6.0f);
            float size = glyphSize * 0.35f;
            float rot = glyphType * 0.52f + t * 0.5f;
            if (glyphType < 3) {
                Vec2 pts[3];
                for (int k = 0; k < 3; k++) {
                    float a2 = (float)k / 3 * TAU + rot;
                    pts[k] = Vec2(x + cosf(a2) * size, y + sinf(a2) * size);
                }
                fillPolyDyn(dev, pts, 3, r, gg, b, a);
            } else if (glyphType < 5) {
                Vec2 pts[4];
                for (int k = 0; k < 4; k++) {
                    float a2 = (float)k / 4 * TAU + rot;
                    pts[k] = Vec2(x + cosf(a2) * size, y + sinf(a2) * size);
                }
                fillPolyDyn(dev, pts, 4, r, gg, b, a);
            } else {
                fillRectDyn(dev, x - size, y - size * 0.5f, x + size, y + size * 0.5f, r, gg, b, a);
            }
        }
    }
    for (int flash = 0; flash < 3; flash++) {
        float flashY = sh * (0.2f + 0.3f * flash) + sinf(t * 0.5f + flash) * 50.0f;
        fillRectDyn(dev, 0, flashY, sw, flashY + 2.0f, 0, 255, 100, 30);
    }
}

// ===== 14 Quantum Field =====
static void drawDynQuantumField(RenderDevice* dev, int w, int h) {
    float t = g_dynBgTime;
    float sw = (float)w, sh = (float)h;
    fillRectDyn(dev, 0, 0, sw, sh, 2, 3, 10, 255);
    int gridSize = 20;
    float cellW = sw / gridSize;
    float cellH = sh / gridSize;
    for (int r = 0; r < gridSize; r++) {
        for (int c = 0; c < gridSize; c++) {
            float cx = c * cellW + cellW * 0.5f;
            float cy = r * cellH + cellH * 0.5f;
            float dx = cx - sw * 0.5f, dy = cy - sh * 0.5f;
            float dist = sqrtf(dx * dx + dy * dy);
            float wave = sinf(t * 1.5f - dist * 0.012f) * 0.5f + 0.5f;
            float phase = sinf(t * 2.0f + c * 0.3f + r * 0.2f) * 0.5f + 0.5f;
            float energy = wave * phase;
            if (energy > 0.5f) {
                float size = cellW * 0.15f * energy;
                int cr = (int)(30 + 100 * energy);
                int cg = (int)(80 + 120 * energy);
                int cb = (int)(150 + 105 * energy);
                fillCircleDyn(dev, cx, cy, size, cr, cg, cb, (int)(100 + 100 * energy));
                if (energy > 0.7f) {
                    drawLineDyn(dev, cx - cellW * 0.3f, cy, cx + cellW * 0.3f, cy, cr, cg, cb, 80, 0.5f);
                    drawLineDyn(dev, cx, cy - cellH * 0.3f, cx, cy + cellH * 0.3f, cr, cg, cb, 80, 0.5f);
                }
            }
            if (c < gridSize - 1) {
                float ncx = (c + 1) * cellW + cellW * 0.5f;
                float ndx = ncx - sw * 0.5f;
                float ndist = sqrtf(ndx * ndx + dy * dy);
                float nwave = sinf(t * 1.5f - ndist * 0.012f) * 0.5f + 0.5f;
                float avgEnergy = (wave + nwave) * 0.5f * phase;
                if (avgEnergy > 0.4f) {
                    int a = (int)(60 * avgEnergy);
                    drawLineDyn(dev, cx, cy, ncx, cy, 0, 150, 255, a, 0.5f);
                }
            }
            if (r < gridSize - 1) {
                float ncy = (r + 1) * cellH + cellH * 0.5f;
                float ndy = ncy - sh * 0.5f;
                float ndist = sqrtf(dx * dx + ndy * ndy);
                float nwave = sinf(t * 1.5f - ndist * 0.012f) * 0.5f + 0.5f;
                float avgEnergy = (wave + nwave) * 0.5f * phase;
                if (avgEnergy > 0.4f) {
                    int a = (int)(60 * avgEnergy);
                    drawLineDyn(dev, cx, cy, cx, ncy, 0, 150, 255, a, 0.5f);
                }
            }
        }
    }
    int particles = 30;
    for (int p = 0; p < particles; p++) {
        uint32_t hh = (uint32_t)p * 2654435761u;
        float pAngle = (hh % 1000) / 1000.0f * TAU + t * 0.5f;
        float pR = ((hh >> 10) % 1000) / 1000.0f * sw * 0.4f;
        float px = sw * 0.5f + cosf(pAngle) * pR;
        float py = sh * 0.5f + sinf(pAngle) * pR;
        float pulse = sinf(t * 4.0f + p * 0.5f) * 0.5f + 0.5f;
        fillCircleDyn(dev, px, py, 2.0f + pulse * 3.0f, 0, 255, 200, (int)(150 + 105 * pulse));
    }
}

// ===== 15 Vortex Matrix =====
static void drawDynVortexMatrix(RenderDevice* dev, int w, int h) {
    float t = g_dynBgTime;
    float sw = (float)w, sh = (float)h;
    fillRectDyn(dev, 0, 0, sw, sh, 3, 4, 12, 255);
    float cx = sw * 0.5f, cy = sh * 0.5f;
    int rings = 12;
    for (int ring = 0; ring < rings; ring++) {
        float ringR = sw * 0.05f + ring * sw * 0.035f;
        float ringRot = t * (1.5f - ring * 0.1f) + ring * 0.4f;
        int cells = 8 + ring * 2;
        for (int cell = 0; cell < cells; cell++) {
            float cellAngle = (float)cell / cells * TAU + ringRot;
            float cellX = cx + cosf(cellAngle) * ringR;
            float cellY = cy + sinf(cellAngle) * ringR * 0.85f;
            float pulse = sinf(t * 2.0f + ring * 0.5f + cell * 0.3f) * 0.5f + 0.5f;
            float size = 6.0f + 4.0f * pulse;
            float rot = cellAngle + t * 0.5f;
            int sides = 4 + ring % 3;
            Vec2 pts[7];
            for (int k = 0; k < sides; k++) {
                float a = (float)k / sides * TAU + rot;
                pts[k] = Vec2(cellX + cosf(a) * size, cellY + sinf(a) * size);
            }
            int cr = (int)(40 + 100 * pulse + 30 * ring);
            int cg = (int)(80 + 120 * pulse);
            int cb = (int)(150 + 105 * (1.0f - pulse));
            int a = (int)(100 + 120 * pulse);
            fillPolyDyn(dev, pts, sides, cr * 0.3f, cg * 0.3f, cb * 0.3f, a / 2);
            drawPolyClosedDyn(dev, pts, sides, cr, cg, cb, a, 1.5f);
            if (pulse > 0.7f) {
                fillCircleDyn(dev, cellX, cellY, 2.0f, 255, 220, 150, 220);
            }
        }
    }
    fillCircleDyn(dev, cx, cy, 10.0f + sinf(t * 4.0f) * 3.0f, 255, 240, 200, 255);
    fillCircleDyn(dev, cx, cy, 20.0f + sinf(t * 4.0f) * 5.0f, 200, 150, 100, 60);
}

// ===== 16 Lava Matrix =====
static void drawDynLavaMatrix(RenderDevice* dev, int w, int h) {
    float t = g_dynBgTime;
    float sw = (float)w, sh = (float)h;
    fillRectDyn(dev, 0, 0, sw, sh, 6, 3, 1, 255);
    int cols = 16, rows = 11;
    float cw = sw / cols, ch = sh / rows;
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            float cx = c * cw + cw * 0.5f;
            float cy = r * ch + ch * 0.5f;
            float dx = cx - sw * 0.5f, dy = cy - sh * 0.5f;
            float dist = sqrtf(dx * dx + dy * dy);
            float wave = sinf(t * 1.2f - dist * 0.012f) * 0.5f + 0.5f;
            float pulse = sinf(t * 2.0f + c * 0.3f + r * 0.2f) * 0.5f + 0.5f;
            float heat = wave * pulse;
            float size = cw * 0.3f * (0.4f + 0.6f * heat);
            int cr = (int)(100 + 155 * heat);
            int cg = (int)(20 + 80 * heat * heat);
            int cb = (int)(5 + 15 * heat);
            int a = (int)(80 + 150 * heat);
            fillCircleDyn(dev, cx, cy, size, cr, cg, cb, a);
            if (heat > 0.7f) {
                fillCircleDyn(dev, cx, cy, size * 0.4f, 255, 240, 200, 255);
                drawLineDyn(dev, cx - cw * 0.3f, cy, cx + cw * 0.3f, cy, cr, cg, cb, 100, 0.5f);
                drawLineDyn(dev, cx, cy - ch * 0.3f, cx, cy + ch * 0.3f, cr, cg, cb, 100, 0.5f);
            }
            if (c < cols - 1) {
                float ncx = (c + 1) * cw + cw * 0.5f;
                float ndx = ncx - sw * 0.5f;
                float ndist = sqrtf(ndx * ndx + dy * dy);
                float nwave = sinf(t * 1.2f - ndist * 0.012f) * 0.5f + 0.5f;
                float avgHeat = (heat + nwave * pulse) * 0.5f;
                if (avgHeat > 0.4f) {
                    drawLineDyn(dev, cx, cy, ncx, cy, cr, cg, cb, (int)(80 * avgHeat), 1.0f);
                }
            }
            if (r < rows - 1) {
                float ncy = (r + 1) * ch + ch * 0.5f;
                float ndy = ncy - sh * 0.5f;
                float ndist = sqrtf(dx * dx + ndy * ndy);
                float nwave = sinf(t * 1.2f - ndist * 0.012f) * 0.5f + 0.5f;
                float avgHeat = (heat + nwave * pulse) * 0.5f;
                if (avgHeat > 0.4f) {
                    drawLineDyn(dev, cx, cy, cx, ncy, cr, cg, cb, (int)(80 * avgHeat), 1.0f);
                }
            }
        }
    }
    for (int s = 0; s < 25; s++) {
        uint32_t hh = (uint32_t)s * 2654435761u;
        float sx = (hh % 1000) / 1000.0f * sw;
        float sy = sh - fmodf(t * 25.0f + s * 30.0f, sh);
        float flicker = sinf(t * 5.0f + s * 2.0f) * 0.5f + 0.5f;
        fillCircleDyn(dev, sx, sy, 1.5f + flicker, 255, (int)(150 + 105 * flicker), 50, (int)(100 + 155 * flicker));
    }
}

// ===== 17 Dark Matter Stream =====
static void drawDynDarkMatter(RenderDevice* dev, int w, int h) {
    float t = g_dynBgTime;
    float sw = (float)w, sh = (float)h;
    fillRectDyn(dev, 0, 0, sw, sh, 5, 3, 15, 255);
    int streams = 6;
    for (int stream = 0; stream < streams; stream++) {
        uint32_t hh = (uint32_t)stream * 2654435761u;
        float startY = ((hh % 1000) / 1000.0f) * sh;
        float amplitude = 30.0f + ((hh >> 10) % 1000) / 1000.0f * 50.0f;
        float frequency = 0.5f + ((hh >> 20) % 1000) / 1000.0f * 1.0f;
        float phase = (hh % 100) / 100.0f * TAU;
        int segs = 80;
        Vec2 prev;
        bool hasPrev = false;
        for (int s = 0; s <= segs; s++) {
            float f = (float)s / segs;
            float x = f * sw;
            float wave1 = sinf(f * TAU * frequency + t * 0.8f + phase) * amplitude;
            float wave2 = sinf(f * TAU * frequency * 2.0f + t * 1.5f + phase) * amplitude * 0.3f;
            float y = startY + wave1 + wave2;
            if (hasPrev) {
                int cr = (int)(60 + 80 * sinf(t * 0.5f + stream));
                int cg = (int)(30 + 50 * cosf(t * 0.7f + stream * 0.5f));
                int cb = (int)(120 + 100 * sinf(t * 0.3f + stream * 0.8f));
                int a = (int)(100 + 100 * sinf(t * 2.0f + f * TAU + stream));
                drawLineDyn(dev, prev.x, prev.y, x, y, cr, cg, cb, a, 2.0f);
            }
            prev = Vec2(x, y);
            hasPrev = true;
        }
    }
    int particles = 60;
    for (int p = 0; p < particles; p++) {
        uint32_t hh = (uint32_t)p * 2654435761u;
        float streamIdx = hh % streams;
        float baseY = ((uint32_t)(streamIdx * 2654435761u) % 1000) / 1000.0f * sh;
        float px = (hh % 1000) / 1000.0f * sw;
        float drift = sinf(t * 0.5f + p * 0.3f) * 20.0f;
        float py = baseY + drift;
        float pulse = sinf(t * 3.0f + p * 0.7f) * 0.5f + 0.5f;
        fillCircleDyn(dev, px, py, 2.0f + 2.0f * pulse, 100, 60, 180, (int)(120 + 135 * pulse));
        fillCircleDyn(dev, px, py, 6.0f, 100, 60, 180, 30);
    }
}

// ===== 18 Spacetime Grid =====
static void drawDynSpacetimeGrid(RenderDevice* dev, int w, int h) {
    float t = g_dynBgTime;
    float sw = (float)w, sh = (float)h;
    fillRectDyn(dev, 0, 0, sw, sh, 2, 2, 8, 255);
    float cx = sw * 0.5f, cy = sh * 0.5f;
    int gridCols = 20, gridRows = 14;
    float gw = sw / gridCols, gh = sh / gridRows;
    for (int r = 0; r <= gridRows; r++) {
        Vec2 prev;
        bool hasPrev = false;
        for (int c = 0; c <= gridCols; c++) {
            float px = c * gw, py = r * gh;
            float dx = px - cx, dy = py - cy;
            float dist = sqrtf(dx * dx + dy * dy);
            float warp = sinf(t * 1.0f - dist * 0.01f) * 15.0f;
            float warpX = px + (dx / (dist + 0.1f)) * warp;
            float warpY = py + (dy / (dist + 0.1f)) * warp;
            if (hasPrev) {
                float wave = sinf(t * 1.5f - dist * 0.012f) * 0.5f + 0.5f;
                int cr = (int)(30 + 80 * wave);
                int cg = (int)(80 + 120 * wave);
                int cb = (int)(150 + 105 * wave);
                int a = (int)(60 + 120 * wave);
                drawLineDyn(dev, prev.x, prev.y, warpX, warpY, cr, cg, cb, a, 1.0f);
            }
            prev = Vec2(warpX, warpY);
            hasPrev = true;
        }
    }
    for (int c = 0; c <= gridCols; c++) {
        Vec2 prev;
        bool hasPrev = false;
        for (int r = 0; r <= gridRows; r++) {
            float px = c * gw, py = r * gh;
            float dx = px - cx, dy = py - cy;
            float dist = sqrtf(dx * dx + dy * dy);
            float warp = sinf(t * 1.0f - dist * 0.01f) * 15.0f;
            float warpX = px + (dx / (dist + 0.1f)) * warp;
            float warpY = py + (dy / (dist + 0.1f)) * warp;
            if (hasPrev) {
                float wave = sinf(t * 1.5f - dist * 0.012f) * 0.5f + 0.5f;
                int cr = (int)(30 + 80 * wave);
                int cg = (int)(80 + 120 * wave);
                int cb = (int)(150 + 105 * wave);
                int a = (int)(60 + 120 * wave);
                drawLineDyn(dev, prev.x, prev.y, warpX, warpY, cr, cg, cb, a, 1.0f);
            }
            prev = Vec2(warpX, warpY);
            hasPrev = true;
        }
    }
    fillCircleDyn(dev, cx, cy, 5.0f + sinf(t * 4.0f) * 2.0f, 255, 255, 240, 255);
    fillCircleDyn(dev, cx, cy, 15.0f + sinf(t * 4.0f) * 4.0f, 200, 200, 255, 60);
}

// ===== 19 Photon Matrix =====
static void drawDynPhotonMatrix(RenderDevice* dev, int w, int h) {
    float t = g_dynBgTime;
    float sw = (float)w, sh = (float)h;
    fillRectDyn(dev, 0, 0, sw, sh, 1, 3, 8, 255);
    int cols = 16, rows = 12;
    float cw = sw / cols, ch = sh / rows;
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            float cx = c * cw + cw * 0.5f;
            float cy = r * ch + ch * 0.5f;
            float phase = t * 2.0f + (r + c) * 0.4f;
            float pulse = sinf(phase) * 0.5f + 0.5f;
            float size = 3.0f + 8.0f * pulse;
            int cr = (int)(80 + 175 * pulse);
            int cg = (int)(150 + 105 * sinf(phase + 1.0f));
            int cb = (int)(200 + 55 * cosf(phase + 2.0f));
            fillCircleDyn(dev, cx, cy, size, cr, cg, cb, (int)(100 + 155 * pulse));
            if (pulse > 0.5f) {
                drawLineDyn(dev, cx, cy, cx + cw * 0.4f, cy, cr, cg, cb, 80, 1.0f);
                drawLineDyn(dev, cx, cy, cx - cw * 0.4f, cy, cr, cg, cb, 80, 1.0f);
                drawLineDyn(dev, cx, cy, cx, cy + ch * 0.4f, cr, cg, cb, 80, 1.0f);
                drawLineDyn(dev, cx, cy, cx, cy - ch * 0.4f, cr, cg, cb, 80, 1.0f);
            }
        }
    }
    int flows = 20;
    for (int i = 0; i < flows; i++) {
        uint32_t hh = (uint32_t)i * 2654435761u;
        float fx = fmodf(t * 80.0f + (hh % 1000) * 0.7f, sw + 50.0f) - 25.0f;
        float fy = ((hh >> 10) % 1000) / 1000.0f * sh;
        fillCircleDyn(dev, fx, fy, 3.0f, 255, 240, 200, 220);
        drawLineDyn(dev, fx - 20.0f, fy, fx, fy, 255, 240, 200, 100, 1.5f);
    }
}

// ===== 20 Plasma Matrix =====
static void drawDynPlasmaMatrix(RenderDevice* dev, int w, int h) {
    float t = g_dynBgTime;
    float sw = (float)w, sh = (float)h;
    fillRectDyn(dev, 0, 0, sw, sh, 2, 3, 8, 255);
    int cols = 16, rows = 12;
    float cw = sw / cols, ch = sh / rows;
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            float cx = c * cw + cw * 0.5f;
            float cy = r * ch + ch * 0.5f;
            float dx = cx - sw * 0.5f, dy = cy - sh * 0.5f;
            float dist = sqrtf(dx * dx + dy * dy);
            float energy = sinf(t * 2.0f - dist * 0.02f) * 0.5f + 0.5f;
            float pulse2 = sinf(t * 3.0f + (r + c) * 0.4f) * 0.5f + 0.5f;
            float cellSize = cw * 0.3f * (0.4f + 0.6f * energy);
            int sides = 4 + (r + c) % 3;
            float rot = t * 0.5f + dist * 0.003f + (r * c) * 0.1f;
            Vec2 pts[7];
            for (int k = 0; k < sides; k++) {
                float a = (float)k / sides * TAU + rot;
                pts[k] = Vec2(cx + cosf(a) * cellSize, cy + sinf(a) * cellSize);
            }
            int cr = (int)(100 + 155 * energy);
            int cg = (int)(50 + 100 * pulse2);
            int cb = (int)(180 + 75 * (1.0f - energy));
            fillPolyDyn(dev, pts, sides, cr * 0.4f, cg * 0.4f, cb * 0.4f, 150);
            drawPolyClosedDyn(dev, pts, sides, cr, cg, cb, (int)(180 + 75 * energy), 1.2f);
            if (energy > 0.7f) {
                fillCircleDyn(dev, cx, cy, 2.0f, 255, 255, 220, 255);
            }
        }
    }
}

// ===== 21 Neon Pulse =====
static void drawDynNeonPulse(RenderDevice* dev, int w, int h) {
    float t = g_dynBgTime;
    float sw = (float)w, sh = (float)h;
    fillRectDyn(dev, 0, 0, sw, sh, 3, 1, 8, 255);
    float cx = sw * 0.5f, cy = sh * 0.5f;
    int pulseRings = 15;
    for (int ring = 0; ring < pulseRings; ring++) {
        float phase = fmodf(t * 0.6f + (float)ring / pulseRings, 1.0f);
        float r = phase * sw * 0.5f;
        if (r < 3) continue;
        float fade = 1.0f - phase;
        int hueShift = ring % 3;
        int cr = (hueShift == 0) ? 255 : (hueShift == 1) ? 0 : 100;
        int cg = (hueShift == 0) ? 50 : (hueShift == 1) ? 255 : 200;
        int cb = (hueShift == 0) ? 150 : (hueShift == 1) ? 100 : 255;
        drawCircleDyn(dev, cx, cy, r, cr, cg, cb, (int)(200 * fade), 2.0f + 3.0f * fade);
        if (ring % 3 == 0) {
            fillCircleDyn(dev, cx, cy, r, cr, cg, cb, (int)(30 * fade));
        }
    }
    int neonLines = 12;
    for (int line = 0; line < neonLines; line++) {
        float angle = (float)line / neonLines * TAU + t * 0.2f;
        float innerR = 10.0f;
        float outerR = sw * 0.45f + sinf(t * 1.5f + line * 0.5f) * 20.0f;
        int hueShift = line % 3;
        int cr = (hueShift == 0) ? 255 : (hueShift == 1) ? 0 : 100;
        int cg = (hueShift == 0) ? 50 : (hueShift == 1) ? 255 : 200;
        int cb = (hueShift == 0) ? 150 : (hueShift == 1) ? 100 : 255;
        float pulse = sinf(t * 3.0f + line * 0.8f) * 0.5f + 0.5f;
        drawLineDyn(dev, cx + cosf(angle) * innerR, cy + sinf(angle) * innerR,
                  cx + cosf(angle) * outerR, cy + sinf(angle) * outerR,
                  cr, cg, cb, (int)(100 + 155 * pulse), 1.5f + 2.0f * pulse);
    }
    int particles = 40;
    for (int p = 0; p < particles; p++) {
        float f = (float)p / particles;
        float angle = f * TAU + t * 1.5f;
        float r = sw * 0.2f + sinf(t * 2.0f + p * 0.3f) * sw * 0.15f;
        float x = cx + cosf(angle) * r;
        float y = cy + sinf(angle) * r;
        int hueShift = p % 3;
        int cr = (hueShift == 0) ? 255 : (hueShift == 1) ? 0 : 100;
        int cg = (hueShift == 0) ? 50 : (hueShift == 1) ? 255 : 200;
        int cb = (hueShift == 0) ? 150 : (hueShift == 1) ? 100 : 255;
        fillCircleDyn(dev, x, y, 2.5f + sinf(t * 4.0f + p) * 1.5f, cr, cg, cb, 220);
    }
    fillCircleDyn(dev, cx, cy, 8.0f + sinf(t * 5.0f) * 3.0f, 255, 255, 255, 255);
    fillCircleDyn(dev, cx, cy, 20.0f + sinf(t * 5.0f) * 5.0f, 200, 200, 255, 80);
}

// ===== 22 Emerald Matrix =====
static void drawDynEmeraldMatrix(RenderDevice* dev, int w, int h) {
    float t = g_dynBgTime;
    float sw = (float)w, sh = (float)h;
    fillRectDyn(dev, 0, 0, sw, sh, 1, 8, 4, 255);
    int cols = 18, rows = 12;
    float cw = sw / cols, ch = sh / rows;
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            float cx = c * cw + cw * 0.5f;
            float cy = r * ch + ch * 0.5f;
            float dx = cx - sw * 0.5f, dy = cy - sh * 0.5f;
            float dist = sqrtf(dx * dx + dy * dy);
            float wave = sinf(t * 1.8f - dist * 0.012f) * 0.5f + 0.5f;
            float rot = t * 0.5f + dist * 0.003f + (r + c) * 0.2f;
            float size = cw * 0.3f * (0.4f + 0.6f * wave);
            bool isHex = ((r + c) % 2 == 0);
            int sides = isHex ? 6 : 4;
            Vec2 pts[6];
            for (int k = 0; k < sides; k++) {
                float a = (float)k / sides * TAU + rot + (isHex ? 0 : 0.7854f);
                pts[k] = Vec2(cx + cosf(a) * size, cy + sinf(a) * size);
            }
            int cr = (int)(20 + 60 * wave);
            int cg = (int)(120 + 135 * wave);
            int cb = (int)(40 + 80 * (1.0f - wave));
            int a = (int)(100 + 120 * wave);
            fillPolyDyn(dev, pts, sides, cr, cg, cb, a);
            drawPolyClosedDyn(dev, pts, sides, cr + 40, cg + 40, cb + 40, (int)(180 * wave + 60), 1.0f);
            if (wave > 0.7f) {
                fillCircleDyn(dev, cx, cy, 2.0f, 150, 255, 180, 220);
            }
        }
    }
    int scanLines = 3;
    for (int scan = 0; scan < scanLines; scan++) {
        float scanY = fmodf(t * 80.0f + scan * sh / 3.0f, sh);
        for (int x = 0; x < sw; x += 4) {
            float pulse = sinf(t * 5.0f + x * 0.05f + scan) * 0.5f + 0.5f;
            fillRectDyn(dev, x, scanY, x + 4, scanY + 3, 100, 255, 150, (int)(80 + 100 * pulse));
        }
    }
}

// ===== 23 Bamboo Sway =====
static void drawDynBambooSway(RenderDevice* dev, int w, int h) {
    float t = g_dynBgTime;
    float sw = (float)w, sh = (float)h;
    fillRectDyn(dev, 0, 0, sw, sh, 3, 6, 4, 255);
    for (int r = 0; r < 4; r++) {
        float bandY = sh * (0.7f + 0.075f * r);
        float bandH = sh * 0.08f;
        int a = (int)(20 + 15 * (1.0f - (float)r / 4));
        fillRectDyn(dev, 0, bandY, sw, bandY + bandH, 20 + r * 5, 40 + r * 10, 15 + r * 5, a);
    }
    int bambooCount = 10;
    for (int bamboo = 0; bamboo < bambooCount; bamboo++) {
        uint32_t hh = (uint32_t)bamboo * 2654435761u;
        float baseX = (hh % 1000) / 1000.0f * sw * 0.9f + sw * 0.05f;
        float baseHeight = sh * (0.5f + ((hh >> 10) % 1000) / 1000.0f * 0.4f);
        float sway = sinf(t * 0.5f + bamboo * 0.8f) * 15.0f;
        int segments = 8;
        float segLen = baseHeight / segments;
        float segWidth = 8.0f + ((hh >> 20) % 1000) / 1000.0f * 4.0f;
        for (int seg = 0; seg < segments; seg++) {
            float segF = (float)seg / segments;
            float segSway = sway * segF;
            float topX = baseX + segSway;
            float topY = sh - (seg + 1) * segLen;
            float botX = baseX + sway * (segF - 1.0f / segments);
            float botY = sh - seg * segLen;
            float width = segWidth * (1.0f - segF * 0.2f);
            Vec2 pts[4] = {
                Vec2(botX - width * 0.5f, botY),
                Vec2(topX - width * 0.5f, topY),
                Vec2(topX + width * 0.5f, topY),
                Vec2(botX + width * 0.5f, botY)
            };
            int cr = (int)(40 + 30 * sinf(t * 0.3f + bamboo * 0.5f));
            int cg = (int)(120 + 80 * (1.0f - segF));
            int cb = (int)(30 + 20 * (1.0f - segF));
            fillQuadDyn(dev, pts[0], pts[1], pts[2], pts[3], cr, cg, cb, 200);
            drawLineDyn(dev, topX - width * 0.5f, topY, topX + width * 0.5f, topY, 60, 100, 40, 220, 1.5f);
        }
        int leafCount = 6;
        for (int leaf = 0; leaf < leafCount; leaf++) {
            float leafF = (float)leaf / leafCount;
            float leafY = sh - (leafF * segments + 1) * segLen;
            float leafX = baseX + sway * leafF;
            float leafAngle = sinf(t * 0.8f + bamboo * 0.5f + leaf * 1.2f) * 0.5f + (leaf % 2 == 0 ? -0.5f : 0.5f);
            float leafLen = 25.0f + sinf(t * 1.0f + leaf * 0.5f) * 5.0f;
            float leafW = 6.0f;
            Vec2 leafPts[4] = {
                Vec2(leafX, leafY),
                Vec2(leafX + cosf(leafAngle) * leafLen * 0.5f - sinf(leafAngle) * leafW,
                       leafY + sinf(leafAngle) * leafLen * 0.5f + cosf(leafAngle) * leafW),
                Vec2(leafX + cosf(leafAngle) * leafLen,
                       leafY + sinf(leafAngle) * leafLen),
                Vec2(leafX + cosf(leafAngle) * leafLen * 0.5f + sinf(leafAngle) * leafW,
                       leafY + sinf(leafAngle) * leafLen * 0.5f - cosf(leafAngle) * leafW)
            };
            int cr = (int)(50 + 40 * sinf(t * 0.4f + leaf * 0.3f));
            int cg = (int)(140 + 80 * sinf(t * 0.6f + leaf * 0.5f));
            int cb = (int)(40 + 20 * cosf(t * 0.5f + leaf * 0.4f));
            fillQuadDyn(dev, leafPts[0], leafPts[1], leafPts[2], leafPts[3], cr, cg, cb, 180);
        }
    }
    int fireflies = 15;
    for (int f = 0; f < fireflies; f++) {
        uint32_t hh = (uint32_t)f * 2654435761u;
        float fx = (hh % 1000) / 1000.0f * sw + sinf(t * 0.5f + f * 0.3f) * 20.0f;
        float fy = ((hh >> 10) % 1000) / 1000.0f * sh * 0.7f + cosf(t * 0.4f + f * 0.2f) * 15.0f;
        float glow = sinf(t * 3.0f + f * 0.7f) * 0.5f + 0.5f;
        fillCircleDyn(dev, fx, fy, 2.0f + glow * 2.0f, 200, 255, 150, (int)(100 + 155 * glow));
        fillCircleDyn(dev, fx, fy, 5.0f + glow * 3.0f, 150, 220, 100, (int)(40 + 60 * glow));
    }
}

// ===== 24 Mechanical Gears =====
static void drawDynMechanicalGears(RenderDevice* dev, int w, int h) {
    float t = g_dynBgTime;
    float sw = (float)w, sh = (float)h;
    fillRectDyn(dev, 0, 0, sw, sh, 15, 15, 20, 255);
    for (int r = 0; r < 3; r++) {
        float bandY = sh * (float)r / 3;
        float bandH = sh / 3;
        int a = (int)(8 * (1.0f - (float)r / 3));
        fillRectDyn(dev, 0, bandY, sw, bandY + bandH, 25 + r * 5, 20 + r * 3, 30 + r * 5, a);
    }
    struct GearInfo { float cx, cy, radius, teeth, speed, phase; };
    GearInfo gears[6] = {
        {sw * 0.3f, sh * 0.4f, 80.0f, 16, 0.5f, 0.0f},
        {sw * 0.55f, sh * 0.35f, 50.0f, 12, -0.8f, 0.2f},
        {sw * 0.7f, sh * 0.55f, 60.0f, 14, 0.6f, 0.5f},
        {sw * 0.2f, sh * 0.7f, 45.0f, 10, -1.0f, 0.8f},
        {sw * 0.45f, sh * 0.65f, 35.0f, 8, 1.2f, 0.3f},
        {sw * 0.8f, sh * 0.25f, 30.0f, 8, -1.5f, 0.6f}
    };
    for (int gi = 0; gi < 6; gi++) {
        float cx = gears[gi].cx;
        float cy = gears[gi].cy;
        float radius = gears[gi].radius;
        int teeth = (int)gears[gi].teeth;
        float rotation = t * gears[gi].speed + gears[gi].phase;
        float toothDepth = radius * 0.2f;
        int segs = teeth * 4;
        Vec2 pts[128];
        for (int s = 0; s < segs; s++) {
            float f = (float)s / segs;
            float angle = f * TAU + rotation;
            float toothPhase = f * teeth * TAU;
            float toothWave = sinf(toothPhase);
            float r = radius;
            if (toothWave > 0.5f) r += toothDepth;
            else if (toothWave < -0.5f) r -= toothDepth * 0.3f;
            pts[s] = Vec2(cx + cosf(angle) * r, cy + sinf(angle) * r);
        }
        int cr = (int)(100 + 80 * sinf(t * 0.3f + gi));
        int cg = (int)(90 + 60 * cosf(t * 0.4f + gi * 0.5f));
        int cb = (int)(110 + 50 * sinf(t * 0.2f + gi * 0.8f));
        fillPolyDyn(dev, pts, segs, cr * 0.4f, cg * 0.4f, cb * 0.4f, 200);
        drawPolyClosedDyn(dev, pts, segs, cr + 40, cg + 40, cb + 40, 220, 2.0f);
        drawCircleDyn(dev, cx, cy, radius * 0.5f, cr + 60, cg + 60, cb + 60, 180, 2.0f);
        fillCircleDyn(dev, cx, cy, radius * 0.15f, cr + 80, cg + 80, cb + 80, 255);
        for (int spoke = 0; spoke < 4; spoke++) {
            float spokeAngle = (float)spoke / 4 * TAU + rotation;
            float spokeX1 = cx + cosf(spokeAngle) * radius * 0.2f;
            float spokeY1 = cy + sinf(spokeAngle) * radius * 0.2f;
            float spokeX2 = cx + cosf(spokeAngle) * radius * 0.5f;
            float spokeY2 = cy + sinf(spokeAngle) * radius * 0.5f;
            drawLineDyn(dev, spokeX1, spokeY1, spokeX2, spokeY2, cr + 50, cg + 50, cb + 50, 200, 3.0f);
        }
    }
    for (int i = 0; i < 20; i++) {
        uint32_t hh = (uint32_t)i * 2654435761u;
        float px = (hh % 1000) / 1000.0f * sw;
        float py = ((hh >> 10) % 1000) / 1000.0f * sh;
        float spark = sinf(t * 5.0f + i * 0.7f) * 0.5f + 0.5f;
        if (spark > 0.85f) {
            fillCircleDyn(dev, px, py, 2.0f, 255, 220, 100, (int)(200 * spark));
            drawLineDyn(dev, px - 4, py, px + 4, py, 255, 200, 80, 150, 0.5f);
            drawLineDyn(dev, px, py - 4, px, py + 4, 255, 200, 80, 150, 0.5f);
        }
    }
}

// ===== 25 Phantom Chess =====
static void drawDynPhantomChess(RenderDevice* dev, int w, int h) {
    float t = g_dynBgTime;
    float sw = (float)w, sh = (float)h;
    fillRectDyn(dev, 0, 0, sw, sh, 5, 5, 12, 255);
    int cols = 10, rows = 8;
    float cw = sw / cols, ch = sh / rows;
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            float cx = c * cw + cw * 0.5f;
            float cy = r * ch + ch * 0.5f;
            bool isLight = ((r + c) & 1) == 0;
            float wave = sinf(t * 1.5f - (cx + cy) * 0.005f) * 0.5f + 0.5f;
            float lift = wave * 8.0f;
            float size = cw * 0.4f * (0.7f + 0.3f * wave);
            int cr, cg, cb;
            if (isLight) {
                cr = (int)(60 + 80 * wave);
                cg = (int)(80 + 100 * wave);
                cb = (int)(120 + 100 * wave);
            } else {
                cr = (int)(20 + 40 * wave);
                cg = (int)(30 + 50 * wave);
                cb = (int)(50 + 70 * wave);
            }
            int a = (int)(150 + 105 * wave);
            Vec2 pts[4];
            float rot = t * 0.3f + (r + c) * 0.2f;
            for (int k = 0; k < 4; k++) {
                float ang = (float)k / 4 * TAU + rot;
                pts[k] = Vec2(cx + cosf(ang) * size, cy - lift + sinf(ang) * size);
            }
            fillPolyDyn(dev, pts, 4, cr, cg, cb, a);
            drawPolyClosedDyn(dev, pts, 4, cr + 40, cg + 40, cb + 40, 200, 1.0f);
            drawLineDyn(dev, cx, cy - lift, cx, cy + size * 0.3f, cr / 2, cg / 2, cb / 2, 100, 1.0f);
        }
    }
    int pieces = 6;
    for (int p = 0; p < pieces; p++) {
        uint32_t hh = (uint32_t)p * 2654435761u;
        int pc = hh % cols;
        int pr = (hh >> 4) % rows;
        float px = pc * cw + cw * 0.5f;
        float py = pr * ch + ch * 0.5f;
        float bob = sinf(t * 2.0f + p * 0.8f) * 5.0f;
        float psize = cw * 0.25f;
        fillCircleDyn(dev, px, py - 15.0f - bob, psize, 255, 220, 100, 220);
        fillCircleDyn(dev, px, py - 15.0f - bob, psize * 0.5f, 255, 255, 200, 255);
        drawLineDyn(dev, px, py - 15.0f - bob + psize, px, py, 200, 180, 80, 150, 2.0f);
    }
}

// ===== 26 Stardust Symphony =====
static void drawDynStardustSymphony(RenderDevice* dev, int w, int h) {
    float t = g_dynBgTime;
    float sw = (float)w, sh = (float)h;
    fillRectDyn(dev, 0, 0, sw, sh, 3, 3, 10, 255);
    int bars = 64;
    float barW = sw / bars;
    for (int b = 0; b < bars; b++) {
        float freq = (float)b / bars;
        float wave1 = sinf(t * 2.0f + freq * 10.0f) * 0.5f + 0.5f;
        float wave2 = sinf(t * 3.5f + freq * 6.0f) * 0.5f + 0.5f;
        float wave3 = sinf(t * 1.2f + freq * 15.0f) * 0.5f + 0.5f;
        float barH = sh * (0.05f + 0.35f * wave1 * wave2 + 0.15f * wave3);
        float x = b * barW;
        int cr = (int)(100 + 155 * wave1);
        int cg = (int)(50 + 150 * wave2);
        int cb = (int)(180 + 75 * wave3);
        fillRectDyn(dev, x + 1, sh - barH, x + barW - 1, sh, cr, cg, cb, 200);
        for (int s = 0; s < 8; s++) {
            float sf = (float)s / 8;
            float sy = sh - barH - sf * 30.0f - sinf(t * 5.0f + b * 0.3f + s) * 5.0f;
            float ssize = 1.5f + wave1 * 2.0f * (1.0f - sf);
            fillCircleDyn(dev, x + barW * 0.5f, sy, ssize, cr, cg, cb, (int)(200 * (1.0f - sf)));
        }
    }
    int notes = 20;
    for (int n = 0; n < notes; n++) {
        uint32_t hh = (uint32_t)n * 2654435761u;
        float nx = (hh % 1000) / 1000.0f * sw;
        float ny = ((hh >> 10) % 1000) / 1000.0f * sh * 0.5f;
        float bob = sinf(t * 2.0f + n * 0.7f) * 10.0f;
        float size = 3.0f + sinf(t * 3.0f + n) * 2.0f;
        fillCircleDyn(dev, nx, ny + bob, size, 255, 220, 150, 180);
        drawCircleDyn(dev, nx, ny + bob, size + 4.0f, 255, 200, 100, 80, 1.0f);
    }
}

// ===== 27 Spacetime Tapestry =====
static void drawDynSpacetimeTapestry(RenderDevice* dev, int w, int h) {
    float t = g_dynBgTime;
    float sw = (float)w, sh = (float)h;
    fillRectDyn(dev, 0, 0, sw, sh, 4, 4, 12, 255);
    int cols = 20, rows = 14;
    float cw = sw / cols, ch = sh / rows;
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            float x = c * cw, y = r * ch;
            float wave1 = sinf(t * 1.5f + c * 0.3f + r * 0.2f) * 0.5f + 0.5f;
            float wave2 = sinf(t * 2.0f - c * 0.2f + r * 0.4f) * 0.5f + 0.5f;
            float twist = sinf(t * 1.0f + (c + r) * 0.15f) * 5.0f;
            int cr = (int)(80 + 175 * wave1);
            int cg = (int)(60 + 140 * wave2);
            int cb = (int)(120 + 135 * (1.0f - wave1));
            float cellSize = cw * (0.3f + 0.4f * wave1);
            float cellX = x + cw * 0.5f + twist;
            float cellY = y + ch * 0.5f + twist * 0.5f;
            if ((r + c) % 2 == 0) {
                fillRectDyn(dev, cellX - cellSize * 0.5f, cellY - cellSize * 0.5f,
                         cellX + cellSize * 0.5f, cellY + cellSize * 0.5f,
                         cr, cg, cb, (int)(120 + 100 * wave1));
            } else {
                Vec2 pts[4];
                for (int k = 0; k < 4; k++) {
                    float a = (float)k / 4 * TAU + t * 0.5f + r * 0.3f;
                    pts[k] = Vec2(cellX + cosf(a) * cellSize * 0.5f, cellY + sinf(a) * cellSize * 0.5f);
                }
                fillPolyDyn(dev, pts, 4, cr, cg, cb, (int)(120 + 100 * wave2));
            }
            if (c < cols - 1) {
                drawLineDyn(dev, cellX, cellY, (c + 1) * cw + cw * 0.5f + twist,
                          y + ch * 0.5f + twist * 0.5f, cr, cg, cb, 60, 0.5f);
            }
            if (r < rows - 1) {
                drawLineDyn(dev, cellX, cellY, x + cw * 0.5f + twist,
                          (r + 1) * ch + ch * 0.5f + twist * 0.5f, cr, cg, cb, 60, 0.5f);
            }
        }
    }
}

// ===== 28 Spacetime Butterfly =====
static void drawDynSpacetimeButterfly(RenderDevice* dev, int w, int h) {
    float t = g_dynBgTime;
    float sw = (float)w, sh = (float)h;
    fillRectDyn(dev, 0, 0, sw, sh, 3, 4, 14, 255);
    float cx = sw * 0.5f, cy = sh * 0.5f;
    int gridCols = 16, gridRows = 12;
    float gw = sw / gridCols, gh = sh / gridRows;
    for (int r = 0; r <= gridRows; r++) {
        for (int c = 0; c <= gridCols; c++) {
            float px = c * gw, py = r * gh;
            float dx = px - cx, dy = py - cy;
            float dist = sqrtf(dx * dx + dy * dy);
            float warp = sinf(t * 1.5f - dist * 0.01f) * 15.0f;
            float wx = px + cosf(t * 0.5f + dist * 0.005f) * warp;
            float wy = py + sinf(t * 0.5f + dist * 0.005f) * warp;
            int cr = (int)(40 + 80 * sinf(t * 0.3f + dist * 0.008f));
            int cg = (int)(60 + 100 * cosf(t * 0.5f + dist * 0.006f));
            int cb = (int)(100 + 120 * sinf(t * 0.4f + dist * 0.01f));
            fillCircleDyn(dev, wx, wy, 1.5f, cr, cg, cb, 150);
            if (c < gridCols) {
                float npx = (c + 1) * gw;
                float ndx = npx - cx;
                float ndist = sqrtf(ndx * ndx + dy * dy);
                float nwarp = sinf(t * 1.5f - ndist * 0.01f) * 15.0f;
                float nwx = npx + cosf(t * 0.5f + ndist * 0.005f) * nwarp;
                float nwy = py + sinf(t * 0.5f + ndist * 0.005f) * nwarp;
                drawLineDyn(dev, wx, wy, nwx, nwy, cr, cg, cb, 80, 0.8f);
            }
            if (r < gridRows) {
                float npy = (r + 1) * gh;
                float ndy = npy - cy;
                float ndist = sqrtf(dx * dx + ndy * ndy);
                float nwarp = sinf(t * 1.5f - ndist * 0.01f) * 15.0f;
                float nwx = px + cosf(t * 0.5f + ndist * 0.005f) * nwarp;
                float nwy = npy + sinf(t * 0.5f + ndist * 0.005f) * nwarp;
                drawLineDyn(dev, wx, wy, nwx, nwy, cr, cg, cb, 80, 0.8f);
            }
        }
    }
    int butterflies = 3;
    for (int bf = 0; bf < butterflies; bf++) {
        uint32_t hh = (uint32_t)bf * 2654435761u;
        float bx = cx + cosf(t * 0.2f + bf * 2.0f) * sw * 0.2f;
        float by = cy + sinf(t * 0.3f + bf * 1.5f) * sh * 0.18f;
        float flap = sinf(t * 3.0f + bf * 0.5f) * 0.5f + 0.5f;
        float wingSize = 18.0f + flap * 8.0f;
        for (int side = 0; side < 2; side++) {
            float dir = (side == 0) ? 1.0f : -1.0f;
            Vec2 wing[4] = {
                Vec2(bx, by),
                Vec2(bx + dir * wingSize, by - wingSize * 0.6f),
                Vec2(bx + dir * wingSize * 1.3f, by),
                Vec2(bx + dir * wingSize, by + wingSize * 0.6f)
            };
            int cr = (int)(120 + 135 * sinf(t * 0.5f + bf));
            int cg = (int)(100 + 120 * cosf(t * 0.7f + bf * 0.3f));
            int cb = (int)(180 + 75 * sinf(t * 0.3f + bf * 0.8f));
            fillPolyDyn(dev, wing, 4, cr, cg, cb, 160);
            drawPolyClosedDyn(dev, wing, 4, cr + 40, cg + 40, cb + 40, 220, 1.5f);
        }
        fillCircleDyn(dev, bx, by, 3.0f, 255, 240, 220, 255);
    }
    fillCircleDyn(dev, cx, cy, 5.0f + sinf(t * 2.0f) * 2.0f, 200, 220, 255, 200);
}

// ===== MORE_DYN_BG_MARKER =====

void drawDynamicBg(RenderDevice* dev, int w, int h) {
    if (!dev || !g_dynBg.enabled || g_dynBg.type == DYN_BG_NONE) return;
    // 纯渲染：只读 g_dynBgTime 状态，零推进（时间单源铁律 1.5——
    // 推进统一在 update 链 MainUI::update → updateDynamicBg(frameDt_)；
    // 旧版此处自建 glfwGetTime 计时，且 drawDynBgPreview 复入本函数
    // 造成同帧双推进：主题工作室预览开启时动画速度翻倍）
    switch (g_dynBg.type) {
        case DYN_BG_POLAR_NIGHT:    drawDynPolarNight(dev, w, h); break;
        case DYN_BG_WATER_WAVE:     drawDynWaterWave(dev, w, h); break;
        case DYN_BG_HEXAGON:        drawDynHexagon(dev, w, h); break;
        case DYN_BG_BUBBLE:         drawDynBubble(dev, w, h); break;
        case DYN_BG_STAR_VORTEX:    drawDynStarVortex(dev, w, h); break;
        case DYN_BG_AUDIO_SPECTRUM: drawDynAudioSpectrum(dev, w, h); break;
        case DYN_BG_WAVE_INTERFER:  drawDynWaveInterfer(dev, w, h); break;
        case DYN_BG_DIGITAL_RAIN:   drawDynDigitalRain(dev, w, h); break;
        case DYN_BG_ENERGY_SHIELD:  drawDynEnergyShield(dev, w, h); break;
        case DYN_BG_HONEYCOMB_MATRIX: drawDynHoneycombMatrix(dev, w, h); break;
        case DYN_BG_RIPPLE_WAVE:    drawDynRippleWave(dev, w, h); break;
        case DYN_BG_SPECTRUM_WAVE:  drawDynSpectrumWave(dev, w, h); break;
        case DYN_BG_MATRIX_CODE:    drawDynMatrixCode(dev, w, h); break;
        case DYN_BG_QUANTUM_FIELD:  drawDynQuantumField(dev, w, h); break;
        case DYN_BG_VORTEX_MATRIX:  drawDynVortexMatrix(dev, w, h); break;
        case DYN_BG_LAVA_MATRIX:    drawDynLavaMatrix(dev, w, h); break;
        case DYN_BG_DARK_MATTER:    drawDynDarkMatter(dev, w, h); break;
        case DYN_BG_SPACETIME_GRID: drawDynSpacetimeGrid(dev, w, h); break;
        case DYN_BG_PHOTON_MATRIX:  drawDynPhotonMatrix(dev, w, h); break;
        case DYN_BG_PLASMA_MATRIX:  drawDynPlasmaMatrix(dev, w, h); break;
        case DYN_BG_NEON_PULSE:     drawDynNeonPulse(dev, w, h); break;
        case DYN_BG_EMERALD_MATRIX: drawDynEmeraldMatrix(dev, w, h); break;
        case DYN_BG_BAMBOO_SWAY:    drawDynBambooSway(dev, w, h); break;
        case DYN_BG_MECHANICAL_GEARS: drawDynMechanicalGears(dev, w, h); break;
        case DYN_BG_PHANTOM_CHESS:  drawDynPhantomChess(dev, w, h); break;
        case DYN_BG_STARDUST_SYMPHONY: drawDynStardustSymphony(dev, w, h); break;
        case DYN_BG_SPACETIME_TAPESTRY: drawDynSpacetimeTapestry(dev, w, h); break;
        case DYN_BG_SPACETIME_BUTTERFLY: drawDynSpacetimeButterfly(dev, w, h); break;
        default: break;
    }
}

void drawDynBgPreview(RenderDevice* dev, float x, float y, float w, float h, float winRatio) {
    if (!dev) return;
    drawRect(dev, Rect(x, y, w, h), Color::rgb(20, 20, 30, 200));
    float innerW, innerH, innerX, innerY;
    if (winRatio > w / h) {
        innerW = w;
        innerH = w / winRatio;
        innerX = x;
        innerY = y + (h - innerH) / 2;
    } else {
        innerH = h;
        innerW = h * winRatio;
        innerX = x + (w - innerW) / 2;
        innerY = y;
    }
    if (innerW < 1) innerW = 1;
    if (innerH < 1) innerH = 1;
    int iw = (int)innerW, ih = (int)innerH;
    if (iw < 1) iw = 1;
    if (ih < 1) ih = 1;
    if (g_dynBg.type != DYN_BG_NONE) {
        dev->setScissorRect((int)innerX, (int)innerY, iw, ih);
        drawDynamicBg(dev, iw, ih);
        dev->resetScissorRect();
    }
    drawBorder(dev, Rect(innerX, innerY, innerW, innerH), Color::rgb(100, 100, 120, 150), 1);
}

} // namespace ui