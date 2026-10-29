#include "Background.h"
#include "Element.h"
#include "win/UiContext.h"
#include "core/Log.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

// stb 图片解码（全局唯一实现点：背景图片懒加载）
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace ui {

static const float PI = 3.14159265f;

static void drawHeartShape(RenderDevice* dev, float x, float y, float size, const Color& color) {
    float r = size / 4;
    fillEllipse(dev, x - r, y - r * 0.8f - r, r, r, color);
    fillEllipse(dev, x + r, y - r * 0.8f - r, r, r, color);
    fillTriangle(dev, x - 2*r, y - r*0.5f, x + 2*r, y - r*0.5f, x, y + 1.5f*r, color);
}

static void drawStarShape(RenderDevice* dev, float x, float y, float size, const Color& color, float angleOffset) {
    Vec2 pts[10];
    float innerRatio = 0.4f;
    for (int i = 0; i < 10; i++) {
        float theta = (i * 36 + angleOffset) * PI / 180.0f;
        float r = (i % 2 == 0) ? size / 2 : size / 2 * innerRatio;
        pts[i] = Vec2(x + r * sinf(theta), y - r * cosf(theta));
    }
    fillPolygonConcave(dev, pts, 10, color);
}

static void drawCloudShape(RenderDevice* dev, float cx, float cy, float size, const Color& color) {
    fillEllipse(dev, cx, cy - size*0.05f, size*0.6f, size*0.4f, color);
    fillEllipse(dev, cx - size*0.55f, cy + size*0.05f, size*0.5f, size*0.35f, color);
    fillEllipse(dev, cx + size*0.55f, cy + size*0.05f, size*0.5f, size*0.35f, color);
}

static void drawCornerDecor(RenderDevice* dev, int w, int h, int len, float penWidth, const Color& c) {
    drawLine(dev, 0, len, 0, 0, penWidth, c);
    drawLine(dev, 0, 0, len, 0, penWidth, c);
    drawLine(dev, w, len, w, 0, penWidth, c);
    drawLine(dev, w, 0, w-len, 0, penWidth, c);
    drawLine(dev, 0, h-len, 0, h, penWidth, c);
    drawLine(dev, 0, h, len, h, penWidth, c);
    drawLine(dev, w, h-len, w, h, penWidth, c);
    drawLine(dev, w, h, w-len, h, penWidth, c);
}

static void drawBgSimple(RenderDevice* dev, int w, int h, ThemeColors& c) {
    drawRect(dev, Rect(0, 0, (float)w, (float)h), c.bg1);
}

static void drawBgTime(RenderDevice* dev, int w, int h, ThemeColors& c) {
    drawDiagonalGradientRect(dev, Rect(0, 0, (float)w, (float)h), c.bg1, c.bg2);
    Color gearColors[3] = {Color::rgb(184,134,11,35), Color::rgb(218,165,32,30), Color::rgb(205,133,63,25)};
    float positions[3][3] = {{w*0.15f,h*0.2f,60},{w*0.85f,h*0.7f,55},{w*0.75f,h*0.25f,45}};
    for (int i = 0; i < 3; i++) {
        float cx = positions[i][0], cy = positions[i][1], r = positions[i][2];
        float rotAng = i * 15 * PI / 180.0f;
        fillEllipse(dev, cx, cy, r, r, gearColors[i]);
        drawEllipseOutline(dev, cx, cy, r, r, 1.5f, Color::rgb(120,80,8,50));
        fillEllipse(dev, cx, cy, r*0.35f, r*0.35f, Color::rgb(120,80,8,25));
        for (int angle = 0; angle < 360; angle += 30) {
            float rad = angle * PI / 180.0f + rotAng;
            float tx = cx + r * 0.85f * cosf(rad);
            float ty = cy + r * 0.85f * sinf(rad);
            drawRect(dev, Rect(tx-3, ty-3, 6, 6), gearColors[i]);
        }
    }
    float dialCx = w*0.7f, dialCy = h*0.45f, dialR = 130;
    fillEllipse(dev, dialCx, dialCy, dialR, dialR, Color::rgb(232,220,200,20));
    drawEllipseOutline(dev, dialCx, dialCy, dialR, dialR, 3, Color::rgb(184,134,11,45));
    for (int minute = 0; minute < 60; minute++) {
        float angle = (minute*6-90)*PI/180.0f;
        float outer = dialR*0.9f;
        float inner = (minute%5==0)?dialR*0.7f:dialR*0.8f;
        drawLine(dev, dialCx+inner*cosf(angle), dialCy+inner*sinf(angle),
                 dialCx+outer*cosf(angle), dialCy+outer*sinf(angle),
                 minute%5==0?2.0f:1.0f, Color::rgb(184,134,11,minute%5==0?70:40));
    }
    float hourAngle = (10*30-90)*PI/180.0f;
    float minAngle = (10*6-90)*PI/180.0f;
    drawLine(dev, dialCx, dialCy, dialCx+45*cosf(hourAngle), dialCy+45*sinf(hourAngle), 3, Color::rgb(139,105,20,80));
    drawLine(dev, dialCx, dialCy, dialCx+70*cosf(minAngle), dialCy+70*sinf(minAngle), 3, Color::rgb(139,105,20,80));
    float hgx = w-70, hgy = h-70;
    Vec2 hourglassPts[] = {{hgx-15,hgy-20},{hgx+15,hgy-20},{hgx+5,hgy},{hgx+15,hgy+20},{hgx-15,hgy+20},{hgx-5,hgy}};
    fillPolygonConcave(dev, hourglassPts, 6, Color::rgb(210,180,140,25));
    drawPolygonOutline(dev, hourglassPts, 6, 2, Color::rgb(184,134,11,60));
    Vec2 sandPts[] = {Vec2(hgx-10,hgy-5), Vec2(hgx+10,hgy-5), Vec2(hgx+3,hgy+5), Vec2(hgx-3,hgy+5)};
    fillPolygon(dev, sandPts, 4, Color::rgb(218,165,32,70));
    for (int i = 0; i < 25; i++) {
        int x = (i*113+17)%w, y = (i*97+31)%h, size = 1+(i%3);
        fillEllipse(dev, x+size/2.0f, y+size/2.0f, size/2.0f, size/2.0f, Color::rgb(255,215,0,i%2==0?70:30));
    }
}

static void drawBgGame(RenderDevice* dev, int w, int h, ThemeColors& c) {
    drawRect(dev, Rect(0,0,(float)w,(float)h), c.bg1);
    Color gridColor = Color::rgb(255,0,200,30);
    int gridSize = 20;
    for (int x = 0; x < w; x += gridSize) drawLine(dev, x, 0, x, h, 0.5f, gridColor);
    for (int y = 0; y < h; y += gridSize) drawLine(dev, 0, y, w, y, 0.5f, gridColor);
    Color scanColor = Color::rgb(0,255,204,15);
    for (int y = 0; y < h; y += 3) drawLine(dev, 0, y, w, y, 1, scanColor);
    Color triColor = Color::rgb(255,0,200,80);
    fillTriangle(dev, 10,10,50,10,10,50, triColor);
    fillTriangle(dev, w-10,10,w-50,10,w-10,50, triColor);
    fillTriangle(dev, 10,h-10,50,h-10,10,h-50, triColor);
    fillTriangle(dev, w-10,h-10,w-50,h-10,w-10,h-50, triColor);
    Color pixelColor = Color::rgb(0,255,204,50);
    for (int i = 0; i < 10; i++) {
        int x = (i*73)%w, y = (i*137)%h;
        drawRect(dev, Rect((float)x,(float)y,4,4), pixelColor);
    }
}

static void drawBgKawaii(RenderDevice* dev, int w, int h, ThemeColors& c) {
    drawGradientRect(dev, Rect(0,0,(float)w,(float)h), c.bg1, c.bg2);
    Color heartColors[4] = {Color::rgb(0xff,0x99,0xaa),Color::rgb(0xff,0x85,0xc0),Color::rgb(0xff,0xb6,0xc1),Color::rgb(0xff,0x69,0xb4)};
    Color starColors[4] = {Color::rgb(0xff,0xd7,0x00),Color::rgb(0xff,0xf0,0xf5),Color::rgb(0xff,0xc0,0xcb),Color::rgb(0xff,0xff,0xff)};
    int seed = 42;
    for (int i = 0; i < 8; i++) {
        seed = seed*1103515245+12345; float x = (float)((seed>>8)%90+5)/100*w;
        seed = seed*1103515245+12345; float y = (float)((seed>>8)%85+5)/100*h;
        seed = seed*1103515245+12345; float sz = (float)((seed>>8)%15+15);
        drawHeartShape(dev, x, y, sz, heartColors[i%4]);
    }
    for (int i = 0; i < 10; i++) {
        seed = seed*1103515245+12345; float x = (float)((seed>>8)%90+5)/100*w;
        seed = seed*1103515245+12345; float y = (float)((seed>>8)%85+5)/100*h;
        seed = seed*1103515245+12345; float sz = (float)((seed>>8)%12+8);
        seed = seed*1103515245+12345; float ang = (float)((seed>>8)%360);
        drawStarShape(dev, x, y, sz, starColors[i%4], ang);
    }
    drawCloudShape(dev, w*0.15f, h*0.12f, 40, Color::rgb(255,255,255,160));
    drawCloudShape(dev, w*0.85f, h*0.08f, 35, Color::rgb(255,255,255,160));
    drawCloudShape(dev, w*0.5f, h*0.2f, 50, Color::rgb(255,255,255,160));
}

static void drawBgScifi(RenderDevice* dev, int w, int h, ThemeColors& c) {
    drawDiagonalGradientRect(dev, Rect(0,0,(float)w,(float)h), c.bg1, c.bg4);
    drawDiagonalGradientRect(dev, Rect(0,0,(float)w,(float)h), Color::rgb(0,212,255,20), Color::rgb(123,47,247,30));
    Color gridColor = Color::rgb(0,212,255,40);
    int step = 40;
    for (int x = 0; x < w; x += step) drawLine(dev, x, 0, x, h, 1, gridColor);
    for (int y = 0; y < h; y += step) drawLine(dev, 0, y, w, y, 1, gridColor);
    drawCornerDecor(dev, w, h, 20, 2, Color::rgb(0,212,255,80));
    Color dotColor = Color::rgb(0,255,200,80);
    int dotPos[5][2] = {{120,50},{300,200},{500,100},{700,300},{800,80}};
    for (int i = 0; i < 5; i++) fillEllipse(dev, dotPos[i][0], dotPos[i][1], 3, 3, dotColor);
    drawLine(dev, 0, h/2, w, h/2, 1, Color::rgb(0,212,255,30));
}

static void drawBgArcade(RenderDevice* dev, int w, int h, ThemeColors& c) {
    drawRect(dev, Rect(0,0,(float)w,(float)h), Color::rgb(26,0,51));
    Color gridColor = Color::rgb(0,255,255,30);
    int step = 16;
    for (int x = 0; x < w; x += step) drawLine(dev, x, 0, x, h, 1, gridColor);
    for (int y = 0; y < h; y += step) drawLine(dev, 0, y, w, y, 1, gridColor);
    Color pixColors[3] = {Color::rgb(255,0,255,80),Color::rgb(0,255,255,80),Color::rgb(255,255,0,80)};
    int seed = 99;
    for (int i = 0; i < 40; i++) {
        seed = seed*1103515245+12345; int gx = (seed>>8)%(w/step+1);
        seed = seed*1103515245+12345; int gy = (seed>>8)%(h/step+1);
        drawRect(dev, Rect((float)(gx*step),(float)(gy*step),(float)step,(float)step), pixColors[seed%3]);
    }
    Color starColor = Color::rgb(255,255,0,180);
    int starPos[5][2] = {{60,80},{200,150},{500,90},{700,200},{850,100}};
    for (int i = 0; i < 5; i++) {
        int sx = starPos[i][0], sy = starPos[i][1];
        drawRect(dev, Rect((float)(sx-2),(float)(sy-8),4,16), starColor);
        drawRect(dev, Rect((float)(sx-8),(float)(sy-2),16,4), starColor);
    }
    int jx = 50, jy = h-80;
    drawEllipseOutline(dev, jx+20, jy+20, 20, 20, 2, Color::rgb(0,255,255,150));
    drawLine(dev, jx+20, jy+40, jx+20, jy+60, 2, Color::rgb(0,255,255,150));
    drawEllipseOutline(dev, jx+20, jy+20, 10, 10, 2, Color::rgb(0,255,255,150));
    for (int i = 0; i < 4; i++) {
        int bx = w-80, by = h-60+i*30;
        fillEllipse(dev, bx+10, by+10, 10, 10, Color::rgb(255,0,255,120));
        drawEllipseOutline(dev, bx+10, by+10, 10, 10, 2, Color::rgb(0,255,255,255));
    }
    drawCornerDecor(dev, w, h, 20, 2, Color::rgb(255,255,0,80));
}

static void drawBgVampire(RenderDevice* dev, int w, int h, ThemeColors& c) {
    drawDiagonalGradientRect(dev, Rect(0,0,(float)w,(float)h), c.bg1, c.bg2);
    fillEllipse(dev, 80, 70, 45, 45, Color::rgb(180,30,30,60));
    for (int i = 0; i < 8; i++) {
        int x = (i*117+43)%w, y = (i*73+23)%(h/2)+20;
        Vec2 batPts[] = {Vec2(x,y),Vec2(x-15,y-12),Vec2(x-25,y+5),Vec2(x,y+10),Vec2(x+25,y+5),Vec2(x+15,y-12)};
        fillPolygonConcave(dev, batPts, 6, Color::rgb(30,10,10,100));
        drawPolygonOutline(dev, batPts, 6, 1, Color::rgb(60,20,20,120));
    }
    for (int i = 0; i < 10; i++) {
        int dx = (i*83+11)%w, dy = (i*67+31)%h;
        fillEllipse(dev, dx, dy, 2, 4, Color::rgb(139,0,0,80));
    }
    Color gothicColor = Color::rgb(160,90,44,70);
    drawArcEllipse(dev, 10, 10, 20, 20, 2, 0, 90, gothicColor);
    drawArcEllipse(dev, 20, 20, 15, 15, 2, 0, 90, gothicColor);
    drawArcEllipse(dev, w-10, 10, 20, 20, 2, 90, 90, gothicColor);
    drawArcEllipse(dev, w-20, 20, 15, 15, 2, 90, 90, gothicColor);
    drawArcEllipse(dev, 10, h-10, 20, 20, 2, 180, 90, gothicColor);
    drawArcEllipse(dev, 20, h-20, 15, 15, 2, 180, 90, gothicColor);
    drawArcEllipse(dev, w-10, h-10, 20, 20, 2, 270, 90, gothicColor);
    drawArcEllipse(dev, w-20, h-20, 15, 15, 2, 270, 90, gothicColor);
    Vec2 coffinPts[] = {Vec2(w/2-30,h/2-40),Vec2(w/2+30,h/2-40),Vec2(w/2+20,h/2+40),Vec2(w/2-20,h/2+40)};
    fillPolygon(dev, coffinPts, 4, Color::rgb(80,30,30,40));
    drawPolygonOutline(dev, coffinPts, 4, 2, Color::rgb(100,40,40,80));
}

static void drawBgSky(RenderDevice* dev, int w, int h, ThemeColors& c) {
    drawGradientRect(dev, Rect(0,0,(float)w,(float)h), c.bg1, Color::rgb(0xf5,0xfa,0xff));
    Color cloudColor = Color::rgb(255,255,255,210);
    fillEllipse(dev, w*0.1f+40, h*0.2f+20, 40, 20, cloudColor);
    fillEllipse(dev, w*0.15f+30, h*0.22f+17, 30, 17, cloudColor);
    fillEllipse(dev, w*0.05f+35, h*0.25f+15, 35, 15, cloudColor);
    fillEllipse(dev, w*0.7f+50, h*0.1f+22, 50, 22, cloudColor);
    fillEllipse(dev, w*0.75f+35, h*0.12f+20, 35, 20, cloudColor);
    fillEllipse(dev, w*0.65f+40, h*0.15f+15, 40, 15, cloudColor);
    fillEllipse(dev, w*0.4f+60, h*0.7f+25, 60, 25, cloudColor);
    fillEllipse(dev, w*0.45f+45, h*0.72f+20, 45, 20, cloudColor);
    fillEllipse(dev, w*0.35f+50, h*0.75f+17, 50, 17, cloudColor);
}

static void drawBgSpace(RenderDevice* dev, int w, int h, ThemeColors& c) {
    drawDiagonalGradientRect(dev, Rect(0,0,(float)w,(float)h), c.bg1, c.bg3);
    drawDiagonalGradientRect(dev, Rect(0,0,(float)w,(float)h), Color::rgb(0,168,255,15), Color::rgb(123,47,247,20));
    int seed = 42;
    Color starColor = Color::rgb(255,255,255,180);
    for (int i = 0; i < 80; i++) {
        seed = seed*1103515245+12345; int x = (seed>>8)%(w-20)+10;
        seed = seed*1103515245+12345; int y = (seed>>8)%(h-20)+10;
        seed = seed*1103515245+12345; int sz = (seed%3==0)?2:1;
        fillEllipse(dev, x+sz/2.0f, y+sz/2.0f, sz/2.0f, sz/2.0f, starColor);
    }
    Color orbitColor = Color::rgb(0,168,255,25);
    int cx = w/2, cy = h/2;
    for (int r = 50; r < (w<h?w:h)/2; r += 80) drawEllipseOutline(dev, cx, cy, r, r, 1.5f, orbitColor);
    drawCornerDecor(dev, w, h, 25, 1, Color::rgb(0,229,255,80));
}

static void drawBgDark(RenderDevice* dev, int w, int h, ThemeColors& c) {
    drawGradientRect(dev, Rect(0,0,(float)w,(float)h), c.bg1, c.bg2);
    Color gridColor = Color::rgb(0,210,255,30);
    int step = 40;
    for (int x = 0; x < w; x += step) drawLine(dev, x, 0, x, h, 1, gridColor);
    for (int y = 0; y < h; y += step) drawLine(dev, 0, y, w, y, 1, gridColor);
    int seed = 42;
    Color dotColor = Color::rgb(0,210,255,100);
    for (int i = 0; i < 30; i++) {
        seed = seed*1103515245+12345; int px = (seed>>8)%w;
        seed = seed*1103515245+12345; int py = (seed>>8)%h;
        fillEllipse(dev, px, py, 2, 2, dotColor);
    }
}

// ===== MORE_THEMES_MARKER =====

static void drawBgCyberpunk(RenderDevice* dev, int w, int h, ThemeColors& c) {
    drawDiagonalGradientRect(dev, Rect(0,0,(float)w,(float)h), c.bg1, c.bg2);
    Color gridColor = Color::rgb(0,255,204,25);
    int step = 50;
    for (int x = 0; x < w; x += step) drawLine(dev, x, 0, x, h, 1, gridColor);
    for (int y = 0; y < h; y += step) drawLine(dev, 0, y, w, y, 1, gridColor);
    Color glitchColor = Color::rgb(255,0,255,40);
    int seed = 42;
    for (int i = 0; i < 8; i++) {
        seed = seed*1103515245+12345; int x1 = (seed>>8)%(w-100)+50;
        seed = seed*1103515245+12345; int y1 = (seed>>8)%(h-100)+50;
        drawLine(dev, x1, y1, x1+30, y1+5, 2, glitchColor);
    }
    Color scanColor = Color::rgb(0,255,204,20);
    for (int y = 0; y < h; y += 2) drawLine(dev, 0, y, w, y, 1, scanColor);
    drawCornerDecor(dev, w, h, 30, 2, Color::rgb(255,0,255,60));
}

static void drawBgGuofeng(RenderDevice* dev, int w, int h, ThemeColors& c) {
    drawRect(dev, Rect(0,0,(float)w,(float)h), c.bg1);
    fillEllipse(dev, w*0.8f, h*0.25f, 35, 35, Color::rgb(192,64,64,180));
    Color mtColor = Color::rgb(62,39,35,60);
    Vec2 leftMt[] = {Vec2(0,h*0.7f),Vec2(w*0.15f,h*0.5f),Vec2(w*0.3f,h*0.65f),Vec2(w*0.45f,h*0.45f),Vec2(w*0.5f,h*0.7f)};
    fillPolygon(dev, leftMt, 5, mtColor);
    Vec2 rightMt[] = {Vec2(w*0.5f,h*0.7f),Vec2(w*0.6f,h*0.55f),Vec2(w*0.75f,h*0.4f),Vec2(w*0.9f,h*0.6f),Vec2(w,h*0.7f)};
    fillPolygon(dev, rightMt, 5, mtColor);
    Color birdColor = Color::rgb(62,39,35,80);
    int birdPos[3][2] = {{(int)(w*0.2f),(int)(h*0.15f)},{(int)(w*0.35f),(int)(h*0.12f)},{(int)(w*0.5f),(int)(h*0.18f)}};
    for (int i = 0; i < 3; i++) {
        int bx = birdPos[i][0], by = birdPos[i][1];
        drawLine(dev, bx, by, bx+8, by-8, 1.5f, birdColor);
        drawLine(dev, bx, by, bx-8, by-8, 1.5f, birdColor);
    }
    Color cloudColor = Color::rgb(192,160,96,40);
    fillEllipse(dev, w*0.1f, h*0.1f, 50, 20, cloudColor);
    fillEllipse(dev, w*0.9f, h*0.15f, 40, 15, cloudColor);
}

static void drawBgTarot(RenderDevice* dev, int w, int h, ThemeColors& c) {
    drawDiagonalGradientRect(dev, Rect(0,0,(float)w,(float)h), c.bg1, c.bg2);
    int seed = 77;
    for (int i = 0; i < 50; i++) {
        seed = seed*1103515245+12345; int x = (seed>>8)%w;
        seed = seed*1103515245+12345; int y = (seed>>8)%h;
        seed = seed*1103515245+12345; int sz = (seed%4==0)?3:1;
        seed = seed*1103515245+12345; int alpha = 100+(seed%156);
        fillEllipse(dev, x+sz/2.0f, y+sz/2.0f, sz/2.0f, sz/2.0f, Color::rgb(1,205,92,alpha));
    }
    float moonX = w*0.85f, moonY = h*0.2f, moonR = 45;
    fillEllipse(dev, moonX, moonY, moonR+25, moonR+25, Color::rgb(201,165,92,20));
    fillEllipse(dev, moonX, moonY, moonR+15, moonR+15, Color::rgb(201,165,92,40));
    fillRadialGradientCircle(dev, moonX, moonY, moonR, Color::rgb(232,220,200,220), Color::rgb(201,165,92,100));
    fillEllipse(dev, moonX+8, moonY-5, moonR*0.85f, moonR*0.85f, Color::rgb(13,13,43,200));
    Vec2 constellation[] = {Vec2(w*0.15f,h*0.3f),Vec2(w*0.25f,h*0.2f),Vec2(w*0.35f,h*0.35f),Vec2(w*0.45f,h*0.25f),Vec2(w*0.55f,h*0.4f),Vec2(w*0.4f,h*0.5f),Vec2(w*0.2f,h*0.45f)};
    Color constLineColor = Color::rgb(201,165,92,60);
    for (int i = 0; i < 6; i++) drawLine(dev, constellation[i].x, constellation[i].y, constellation[i+1].x, constellation[i+1].y, 1, constLineColor);
    Color constStarColor = Color::rgb(232,220,200,180);
    for (int i = 0; i < 7; i++) fillEllipse(dev, constellation[i].x, constellation[i].y, 2, 2, constStarColor);
    Color cardColor = Color::rgb(201,165,92,50);
    int cs = 30;
    drawLine(dev, 10, 10, 10+cs, 10, 1.5f, cardColor);
    drawLine(dev, 10, 10, 10, 10+cs, 1.5f, cardColor);
    drawArcEllipse(dev, 20, 20, 10, 10, 1.5f, 180, 90, cardColor);
    drawLine(dev, w-10, 10, w-10-cs, 10, 1.5f, cardColor);
    drawLine(dev, w-10, 10, w-10, 10+cs, 1.5f, cardColor);
    drawArcEllipse(dev, w-20, 20, 10, 10, 1.5f, 270, 90, cardColor);
    drawLine(dev, 10, h-10, 10+cs, h-10, 1.5f, cardColor);
    drawLine(dev, 10, h-10, 10, h-10-cs, 1.5f, cardColor);
    drawArcEllipse(dev, 20, h-20, 10, 10, 1.5f, 90, 90, cardColor);
    drawLine(dev, w-10, h-10, w-10-cs, h-10, 1.5f, cardColor);
    drawLine(dev, w-10, h-10, w-10, h-10-cs, 1.5f, cardColor);
    drawArcEllipse(dev, w-20, h-20, 10, 10, 1.5f, 0, 90, cardColor);
}

static void drawBgRetroShip(RenderDevice* dev, int w, int h, ThemeColors& c) {
    drawDiagonalGradientRect(dev, Rect(0,0,(float)w,(float)h), c.bg1, c.bg2);
    drawDiagonalGradientRect(dev, Rect(0,0,(float)w,(float)h), Color::rgb(212,160,23,30), Color::rgb(255,215,0,20));
    int margin = 20;
    drawRoundedBorder(dev, Rect((float)margin,(float)margin,(float)(w-2*margin),(float)(h-2*margin)), Color::rgb(212,160,23,50), 2, 10);
    drawRoundedBorder(dev, Rect((float)(margin+8),(float)(margin+8),(float)(w-2*(margin+8)),(float)(h-2*(margin+8))), Color::rgb(255,215,0,40), 1.5f, 8);
    Color flickerColor = Color::rgb(255,215,0,120);
    int seed = 55;
    for (int i = 0; i < 12; i++) {
        seed = seed*1103515245+12345; int x = (seed>>8)%(w-40)+20;
        fillEllipse(dev, x, 25, 3, 3, flickerColor);
        fillEllipse(dev, x, h-25, 3, 3, flickerColor);
    }
}

static void drawBgSummer(RenderDevice* dev, int w, int h, ThemeColors& c) {
    drawGradientRect(dev, Rect(0,0,(float)w,(float)h), c.bg1, c.bg3);
    float sunCx = w*0.8f, sunCy = h*0.2f, sunR = 40;
    fillEllipse(dev, sunCx, sunCy, sunR, sunR, Color::rgb(255,140,66,200));
    Color rayColor = Color::rgb(255,140,66,100);
    for (int i = 0; i < 12; i++) {
        float ang = i*30*PI/180.0f;
        drawLine(dev, sunCx+sunR*cosf(ang), sunCy+sunR*sinf(ang), sunCx+(sunR+20)*cosf(ang), sunCy+(sunR+20)*sinf(ang), 2, rayColor);
    }
    Color cloudColor = Color::rgb(255,255,255,180);
    int cloudPos[3][3] = {{(int)(w*0.1f),(int)(h*0.15f),30},{(int)(w*0.35f),(int)(h*0.1f),35},{(int)(w*0.6f),(int)(h*0.25f),25}};
    for (int i = 0; i < 3; i++) {
        int cx = cloudPos[i][0], cy = cloudPos[i][1], sz = cloudPos[i][2];
        fillEllipse(dev, cx, cy-sz*0.35f, sz, sz*0.7f, cloudColor);
        fillEllipse(dev, cx+sz*0.75f, cy-sz*0.25f, sz*0.75f, sz*0.5f, cloudColor);
    }
    Color waveColor = Color::rgb(0,150,200,80);
    for (int x = 0; x < w; x += 20) {
        int wy = (int)(h*0.7f+sinf(x*0.05f)*8);
        drawLine(dev, x, wy, x+20, wy, 2, waveColor);
    }
    Color waveColor2 = Color::rgb(0,100,180,60);
    for (int x = 0; x < w; x += 15) {
        int wy = (int)(h*0.8f+sinf(x*0.04f)*6);
        drawLine(dev, x, wy, x+15, wy, 1.5f, waveColor2);
    }
    fillEllipse(dev, 104, h-47, 4, 2, Color::rgb(255,200,100,100));
    fillEllipse(dev, 253, h-33, 3, 2, Color::rgb(255,200,100,100));
    drawStarShape(dev, 180, h-45, 15, Color::rgb(255,160,80,120), -90);
    float melonCx = 350, melonCy = h-40, melonR = 20;
    fillPie(dev, melonCx, melonCy, melonR, melonR, 0, 180, Color::rgb(80,180,80,80));
    fillPie(dev, melonCx, melonCy, melonR-3, melonR-3, 0, 180, Color::rgb(255,80,80,100));
    fillEllipse(dev, melonCx-5, melonCy-8, 1.5f, 1.5f, Color::rgb(50,30,20,150));
    fillEllipse(dev, melonCx+3, melonCy-5, 1.5f, 1.5f, Color::rgb(50,30,20,150));
    fillEllipse(dev, melonCx-2, melonCy-12, 1.5f, 1.5f, Color::rgb(50,30,20,150));
    Vec2 conePts[] = {Vec2(420,h-30),Vec2(440,h-30),Vec2(430,h-10)};
    fillPolygon(dev, conePts, 3, Color::rgb(180,120,60,120));
    fillEllipse(dev, 430, h-42, 8, 8, Color::rgb(255,200,200,150));
    fillEllipse(dev, 430, h-49, 6, 3, Color::rgb(180,80,40,120));
    int seed = 22;
    for (int i = 0; i < 20; i++) {
        seed = seed*1103515245+12345; int x = (seed>>8)%w;
        seed = seed*1103515245+12345; int y = (seed>>8)%h;
        seed = seed*1103515245+12345; int r = (seed%4)+2;
        fillEllipse(dev, x, y, r, r, Color::rgb(255,255,200,40+(seed%30)));
    }
}

static void drawBgAngelDemon(RenderDevice* dev, int w, int h, ThemeColors& c) {
    drawDiagonalGradientRect(dev, Rect(0,0,(float)w,(float)h), c.bg1, c.bg2);
    Color wingColor = Color::rgb(201,168,76,50);
    Vec2 angelWing[] = {Vec2(20,h*0.4f),Vec2(5,h*0.3f),Vec2(0,h*0.5f),Vec2(15,h*0.6f),Vec2(30,h*0.55f),Vec2(40,h*0.65f),Vec2(50,h*0.7f),Vec2(35,h*0.75f),Vec2(20,h*0.8f),Vec2(10,h*0.85f)};
    fillPolygonConcave(dev, angelWing, 10, wingColor);
    drawEllipseOutline(dev, 40, 50, 30, 15, 2, Color::rgb(201,168,76,80));
    Vec2 demonWing[] = {Vec2(w-20,h*0.4f),Vec2(w-5,h*0.3f),Vec2(w,h*0.5f),Vec2(w-15,h*0.6f),Vec2(w-30,h*0.55f),Vec2(w-40,h*0.65f),Vec2(w-50,h*0.7f),Vec2(w-35,h*0.75f),Vec2(w-20,h*0.8f),Vec2(w-10,h*0.85f)};
    fillPolygonConcave(dev, demonWing, 10, Color::rgb(139,0,0,50));
    Vec2 leftHorn[] = {Vec2(w-30,30),Vec2(w-15,10),Vec2(w-10,35)};
    fillPolygon(dev, leftHorn, 3, Color::rgb(139,0,0,60));
    Vec2 rightHorn[] = {Vec2(w-50,25),Vec2(w-40,5),Vec2(w-35,30)};
    fillPolygon(dev, rightHorn, 3, Color::rgb(139,0,0,60));
    fillEllipse(dev, w/2.0f, h/2.0f-5, 20, 25, Color::rgb(255,80,20,40));
    fillEllipse(dev, w/2.0f, h/2.0f-5, 15, 20, Color::rgb(255,120,40,30));
    fillEllipse(dev, w/2.0f, h/2.0f-5, 10, 15, Color::rgb(255,200,80,20));
    Color swordColor = Color::rgb(201,168,76,60);
    drawLine(dev, w/2.0f-50, 20, w/2.0f+50, 60, 2, swordColor);
    drawLine(dev, w/2.0f+50, 20, w/2.0f-50, 60, 2, swordColor);
    fillEllipse(dev, w/2.0f, 40, 15, 7, Color::rgb(201,168,76,40));
    int seed = 33;
    for (int i = 0; i < 20; i++) {
        seed = seed*1103515245+12345; int x = (seed>>8)%w;
        seed = seed*1103515245+12345; int y = (seed>>8)%h;
        fillEllipse(dev, x, y, 1, 1, i%3==0?Color::rgb(201,168,76,80):Color::rgb(139,0,0,80));
    }
}

static void drawBgGhost(RenderDevice* dev, int w, int h, ThemeColors& c) {
    drawDiagonalGradientRect(dev, Rect(0,0,(float)w,(float)h), c.bg1, c.bg2);
    drawDiagonalGradientRect(dev, Rect(0,0,(float)w,(float)h), Color::rgb(110,255,160,20), Color::rgb(160,80,200,15));
    int ghostPos[3][2] = {{(int)(w*0.2f),(int)(h*0.3f)},{(int)(w*0.5f),(int)(h*0.2f)},{(int)(w*0.8f),(int)(h*0.35f)}};
    for (int gi = 0; gi < 3; gi++) {
        int gx = ghostPos[gi][0], gy = ghostPos[gi][1];
        int gw = 40, gh = 50;
        fillEllipse(dev, gx, gy, gw/2.0f, gh/2.0f, Color::rgb(200,200,220,30));
        Vec2 ghostBody[] = {Vec2(gx+gw/2,gy),Vec2(gx+gw/2,gy+gh/3),Vec2(gx+gw/3,gy+gh/3+5),Vec2(gx,gy+gh/3),Vec2(gx-gw/3,gy+gh/3+5),Vec2(gx-gw/2,gy+gh/3),Vec2(gx-gw/2,gy)};
        fillPolygonConcave(dev, ghostBody, 7, Color::rgb(200,200,220,30));
        fillEllipse(dev, gx-6, gy-10, 2, 3, Color::rgb(110,255,160,100));
        fillEllipse(dev, gx+6, gy-10, 2, 3, Color::rgb(110,255,160,100));
    }
    int seed = 66;
    for (int i = 0; i < 12; i++) {
        seed = seed*1103515245+12345; int x = (seed>>8)%w;
        seed = seed*1103515245+12345; int y = (seed>>8)%h;
        seed = seed*1103515245+12345; int sz = (seed%3)+3;
        fillEllipse(dev, x, y, sz*2, sz*2, Color::rgb(110,255,160,100));
        fillEllipse(dev, x, y, sz, sz, Color::rgb(110,255,160,200));
    }
    Color chainColor = Color::rgb(110,255,160,50);
    int chainX = 15;
    for (int i = 0; i < 7; i++) {
        int cy = h-120+i*14;
        drawEllipseOutline(dev, chainX+6, cy+4, 6, 4, 1.5f, chainColor);
        if (i > 0) drawLine(dev, chainX+6, cy, chainX+6, cy-6, 1.5f, chainColor);
    }
    Color tombColor = Color::rgb(110,255,160,40);
    drawRect(dev, Rect((float)(w-60),(float)(h-80),40,60), tombColor);
    drawBorder(dev, Rect((float)(w-60),(float)(h-80),40,60), Color::rgb(110,255,160,80), 1);
    drawArcEllipse(dev, w-40, h-80, 20, 10, 1, 180, 180, Color::rgb(110,255,160,80));
    drawCornerDecor(dev, w, h, 25, 1, Color::rgb(110,255,160,60));
}

static void drawBgPuzzle(RenderDevice* dev, int w, int h, ThemeColors& c) {
    drawDiagonalGradientRect(dev, Rect(0,0,(float)w,(float)h), c.bg1, c.bg2);
    Color puzzleColors[5] = {Color::rgb(255,140,66,40),Color::rgb(245,150,170,40),Color::rgb(150,200,240,40),Color::rgb(170,230,130,40),Color::rgb(240,200,100,40)};
    int s = 80;
    int piecePos[5][2] = {{w/2-s-10,h/2-s-10},{w/2+10,h/2-s-10},{w/2-s-10,h/2+10},{w/2+10,h/2+10},{w/2-s/2,h/2-s/2}};
    for (int p = 0; p < 5; p++) {
        int px = piecePos[p][0], py = piecePos[p][1];
        drawRect(dev, Rect((float)px,(float)py,(float)s,(float)s), puzzleColors[p%5]);
        drawBorder(dev, Rect((float)px,(float)py,(float)s,(float)s), Color::rgb(255,255,255,80), 1.5f);
    }
    int seed = 88;
    Color dotColors[5] = {Color::rgb(255,140,66,120),Color::rgb(245,150,170,120),Color::rgb(150,200,240,120),Color::rgb(170,230,130,120),Color::rgb(240,200,100,120)};
    for (int i = 0; i < 30; i++) {
        seed = seed*1103515245+12345; int x = (seed>>8)%w;
        seed = seed*1103515245+12345; int y = (seed>>8)%h;
        seed = seed*1103515245+12345; int r = (seed%3)+2;
        fillEllipse(dev, x, y, r, r, dotColors[seed%5]);
    }
}

static void drawBgStarry(RenderDevice* dev, int w, int h, ThemeColors& c) {
    drawDiagonalGradientRect(dev, Rect(0,0,(float)w,(float)h), c.bg1, c.bg2);
    drawDiagonalGradientRect(dev, Rect(0,0,(float)w,(float)h), Color::rgb(80,120,200,30), Color::rgb(120,100,180,40));
    int nebulaPos[3][3] = {{(int)(w*0.3f),(int)(h*0.4f),60},{(int)(w*0.7f),(int)(h*0.6f),50},{(int)(w*0.5f),(int)(h*0.2f),40}};
    for (int i = 0; i < 3; i++) {
        int cx = nebulaPos[i][0], cy = nebulaPos[i][1], r = nebulaPos[i][2];
        fillRadialGradientCircle(dev, cx, cy, r, Color::rgb(120,100,180,40), Color::rgb(120,100,180,0));
    }
    int seed = 42;
    Color starColors[6] = {Color::rgb(255,255,255,200),Color::rgb(255,255,200,180),Color::rgb(200,220,255,160),Color::rgb(255,200,150,140),Color::rgb(200,150,255,120),Color::rgb(150,200,255,100)};
    for (int i = 0; i < 120; i++) {
        seed = seed*1103515245+12345; int x = (seed>>8)%w;
        seed = seed*1103515245+12345; int y = (seed>>8)%h;
        seed = seed*1103515245+12345; int sz = (seed%5==0)?2:1;
        fillEllipse(dev, x+sz/2.0f, y+sz/2.0f, sz/2.0f, sz/2.0f, starColors[seed%6]);
    }
    Color crossColor = Color::rgb(255,255,255,150);
    int crossPos[3][2] = {{(int)(w*0.15f),(int)(h*0.15f)},{(int)(w*0.85f),(int)(h*0.25f)},{(int)(w*0.45f),(int)(h*0.6f)}};
    for (int i = 0; i < 3; i++) {
        int sx = crossPos[i][0], sy = crossPos[i][1];
        drawLine(dev, sx-4, sy, sx+4, sy, 1, crossColor);
        drawLine(dev, sx, sy-4, sx, sy+4, 1, crossColor);
    }
    Vec2 orion[] = {Vec2(w*0.6f,h*0.3f),Vec2(w*0.65f,h*0.35f),Vec2(w*0.7f,h*0.4f),Vec2(w*0.68f,h*0.5f),Vec2(w*0.62f,h*0.5f),Vec2(w*0.58f,h*0.45f)};
    Color constLineColor = Color::rgb(200,220,255,50);
    for (int i = 0; i < 5; i++) drawLine(dev, orion[i].x, orion[i].y, orion[i+1].x, orion[i+1].y, 1, constLineColor);
    Color constStarColor = Color::rgb(255,255,255,200);
    for (int i = 0; i < 6; i++) fillEllipse(dev, orion[i].x, orion[i].y, 2, 2, constStarColor);
    Color meteorColor = Color::rgb(200,220,255,120);
    drawLine(dev, w*0.1f, h*0.05f, w*0.2f, h*0.15f, 1.5f, meteorColor);
    drawLine(dev, w*0.8f, h*0.1f, w*0.9f, h*0.2f, 1.5f, meteorColor);
    drawLine(dev, w*0.3f, h*0.7f, w*0.4f, h*0.8f, 1.5f, meteorColor);
}

static void drawBgSpring(RenderDevice* dev, int w, int h, ThemeColors& c) {
    drawGradientRect(dev, Rect(0,0,(float)w,(float)h), c.bg1, c.bg2);
    drawHorizontalGradientRect(dev, Rect(0,0,(float)w,(float)h), Color::rgb(255,255,200,50), Color::rgb(255,255,200,0));
    int seed = 22;
    Color petalColors[3] = {Color::rgb(244,143,177,120),Color::rgb(255,200,200,100),Color::rgb(255,180,200,80)};
    for (int i = 0; i < 25; i++) {
        seed = seed*1103515245+12345; int x = (seed>>8)%w;
        seed = seed*1103515245+12345; int y = (seed>>8)%h;
        fillEllipse(dev, x, y, 4, 4, petalColors[i%3]);
    }
    int butterflyPos[3][2] = {{(int)(w*0.2f),(int)(h*0.3f)},{(int)(w*0.7f),(int)(h*0.5f)},{(int)(w*0.4f),(int)(h*0.7f)}};
    for (int i = 0; i < 3; i++) {
        int bx = butterflyPos[i][0], by = butterflyPos[i][1];
        fillEllipse(dev, bx-4, by-3, 8, 5, Color::rgb(244,143,177,150));
        fillEllipse(dev, bx-3, by+2, 6, 3, Color::rgb(244,143,177,150));
        fillEllipse(dev, bx, by, 1, 4, Color::rgb(100,80,60,200));
    }
    Color leafColor = Color::rgb(107,142,35,60);
    fillEllipse(dev, 40, h-50, 40, 10, leafColor);
    fillEllipse(dev, 50, h-70, 30, 7, leafColor);
    fillEllipse(dev, w-35, h-30, 35, 9, leafColor);
    fillEllipse(dev, w-25, h-50, 25, 6, leafColor);
    float rcx = w*0.5f, rcy = h*0.9f, rr = 120;
    drawArcEllipse(dev, rcx, rcy, rr, rr, 3, 180, 90, Color::rgb(255,100,100,40));
    drawArcEllipse(dev, rcx, rcy, rr-5, rr-5, 3, 180, 90, Color::rgb(255,200,100,40));
    drawArcEllipse(dev, rcx, rcy, rr-10, rr-10, 3, 180, 90, Color::rgb(100,200,255,40));
    Color dewColor = Color::rgb(200,230,255,120);
    int dewPos[10][2] = {{(int)(w*0.15f),(int)(h*0.6f)},{(int)(w*0.3f),(int)(h*0.8f)},{(int)(w*0.5f),(int)(h*0.55f)},{(int)(w*0.7f),(int)(h*0.75f)},{(int)(w*0.85f),(int)(h*0.65f)},{(int)(w*0.25f),(int)(h*0.45f)},{(int)(w*0.6f),(int)(h*0.85f)},{(int)(w*0.9f),(int)(h*0.4f)},{(int)(w*0.4f),(int)(h*0.35f)},{(int)(w*0.55f),(int)(h*0.65f)}};
    for (int i = 0; i < 10; i++) fillEllipse(dev, dewPos[i][0], dewPos[i][1], 2, 2, dewColor);
}

// ===== MORE_THEMES2_MARKER =====

static void drawBgDawn(RenderDevice* dev, int w, int h, ThemeColors& c) {
    drawDiagonalGradientRect(dev, Rect(0,0,(float)w,(float)h), c.bg1, c.bg2);
    drawGradientRect(dev, Rect(0,0,(float)w,(float)h), Color::rgb(255,140,0,50), Color::rgb(20,10,40,0));
    drawGradientRect(dev, Rect(0,0,(float)w,(float)h), Color::rgb(255,200,100,20), Color::rgb(100,50,20,0));
    Color rayColor = Color::rgb(230,180,40,40);
    float srcX = w*0.5f, srcY = h;
    for (int i = -11; i <= 11; i++) {
        float ang = i*8*PI/180.0f;
        drawLine(dev, srcX, srcY, srcX+(h*1.5f)*sinf(ang), srcY-h*1.5f*cosf(ang), 1.5f, rayColor);
    }
    Color cloudColor = Color::rgb(230,180,40,40);
    int cloudPos[5][2] = {{(int)(w*0.15f),(int)(h*0.4f)},{(int)(w*0.35f),(int)(h*0.35f)},{(int)(w*0.55f),(int)(h*0.45f)},{(int)(w*0.75f),(int)(h*0.38f)},{(int)(w*0.9f),(int)(h*0.5f)}};
    for (int i = 0; i < 5; i++) {
        int cx = cloudPos[i][0], cy = cloudPos[i][1];
        fillEllipse(dev, cx, cy, 70, 20, cloudColor);
        fillEllipse(dev, cx, cy-5, 40, 15, cloudColor);
        fillEllipse(dev, cx-10, cy-5, 30, 10, cloudColor);
    }
    int seed = 11;
    Color starColor = Color::rgb(255,255,255,60);
    for (int i = 0; i < 15; i++) {
        seed = seed*1103515245+12345; int x = (seed>>8)%w;
        seed = seed*1103515245+12345; int y = (seed>>8)%(int)(h*0.3f);
        fillEllipse(dev, x, y, 0.5f, 0.5f, starColor);
    }
    Color topStarColor = Color::rgb(255,255,255,80);
    int topStarPos[8][2] = {{(int)(w*0.1f),(int)(h*0.05f)},{(int)(w*0.3f),(int)(h*0.08f)},{(int)(w*0.5f),(int)(h*0.03f)},{(int)(w*0.7f),(int)(h*0.06f)},{(int)(w*0.9f),(int)(h*0.04f)},{(int)(w*0.2f),(int)(h*0.12f)},{(int)(w*0.6f),(int)(h*0.15f)},{(int)(w*0.8f),(int)(h*0.1f)}};
    for (int i = 0; i < 8; i++) fillEllipse(dev, topStarPos[i][0], topStarPos[i][1], 1.5f, 1.5f, topStarColor);
    Color rippleColor = Color::rgb(230,180,40,30);
    for (int i = 0; i < 3; i++) {
        int ry = (int)(h*0.85f+i*15);
        drawEllipseOutline(dev, w*0.3f, ry, 40+i*10, 10, 1, rippleColor);
    }
}

static void drawBgMoon(RenderDevice* dev, int w, int h, ThemeColors& c) {
    drawDiagonalGradientRect(dev, Rect(0,0,(float)w,(float)h), c.bg1, c.bg2);
    drawDiagonalGradientRect(dev, Rect(0,0,(float)w,(float)h), Color::rgb(170,185,217,20), Color::rgb(200,210,240,10));
    float moonX = w*0.8f, moonY = h*0.2f, moonR = 40;
    fillEllipse(dev, moonX, moonY, moonR+20, moonR+20, Color::rgb(170,185,217,30));
    fillEllipse(dev, moonX, moonY, moonR, moonR, Color::rgb(200,210,240,180));
    fillEllipse(dev, moonX+8, moonY-5, moonR*0.85f, moonR*0.85f, Color::rgb(11,14,26,200));
    Color rayColor = Color::rgb(200,210,240,30);
    for (int i = 0; i < 12; i++) {
        float ang = i*30*PI/180.0f;
        drawLine(dev, moonX+moonR*cosf(ang), moonY+moonR*sinf(ang), moonX+(moonR+25)*cosf(ang), moonY+(moonR+25)*sinf(ang), 1, rayColor);
    }
    int seed = 55;
    Color smallStarColor = Color::rgb(255,255,255,150);
    for (int i = 0; i < 50; i++) {
        seed = seed*1103515245+12345; int x = (seed>>8)%w;
        seed = seed*1103515245+12345; int y = (seed>>8)%h;
        fillEllipse(dev, x, y, 0.5f, 0.5f, smallStarColor);
    }
    Color bigStarColor = Color::rgb(200,220,255,180);
    int bigStarPos[15][2] = {{(int)(w*0.1f),(int)(h*0.15f)},{(int)(w*0.3f),(int)(h*0.1f)},{(int)(w*0.5f),(int)(h*0.25f)},{(int)(w*0.15f),(int)(h*0.4f)},{(int)(w*0.4f),(int)(h*0.5f)},{(int)(w*0.6f),(int)(h*0.15f)},{(int)(w*0.25f),(int)(h*0.6f)},{(int)(w*0.7f),(int)(h*0.4f)},{(int)(w*0.35f),(int)(h*0.7f)},{(int)(w*0.55f),(int)(h*0.35f)},{(int)(w*0.9f),(int)(h*0.5f)},{(int)(w*0.05f),(int)(h*0.55f)},{(int)(w*0.45f),(int)(h*0.8f)},{(int)(w*0.65f),(int)(h*0.65f)},{(int)(w*0.85f),(int)(h*0.7f)}};
    for (int i = 0; i < 15; i++) fillEllipse(dev, bigStarPos[i][0], bigStarPos[i][1], 2, 2, bigStarColor);
    drawCornerDecor(dev, w, h, 30, 1, Color::rgb(200,210,240,50));
}

static void drawBgWood(RenderDevice* dev, int w, int h, ThemeColors& c) {
    drawGradientRect(dev, Rect(0,0,(float)w,(float)h), c.bg1, c.bg2);
    Color grainColor = Color::rgb(139,90,43,25);
    for (int i = 0; i < h; i += 8) {
        int offset = (i/16)%3-1;
        drawLine(dev, 0, i+offset, w, i+offset, 1, grainColor);
    }
    Color leafColor = Color::rgb(107,142,35,80);
    fillEllipse(dev, w-20, 11, 15, 6, leafColor);
    fillEllipse(dev, w-40, 22, 20, 7, leafColor);
    fillEllipse(dev, w-60, 30, 12, 5, leafColor);
    fillEllipse(dev, 22, 12, 17, 7, leafColor);
    Color ringColor = Color::rgb(139,90,43,60);
    drawArcEllipse(dev, 30, h-40, 20, 20, 1.5f, 0, 180, ringColor);
    drawArcEllipse(dev, 30, h-40, 15, 15, 1.5f, 0, 180, ringColor);
}

static void drawBgRobot(RenderDevice* dev, int w, int h, ThemeColors& c) {
    drawRect(dev, Rect(0,0,(float)w,(float)h), c.bg1);
    Color gridColor = Color::rgb(0,212,255,25);
    int step = 20;
    for (int x = 0; x < w; x += step) drawLine(dev, x, 0, x, h, 0.5f, gridColor);
    for (int y = 0; y < h; y += step) drawLine(dev, 0, y, w, y, 0.5f, gridColor);
    Color traceColor = Color::rgb(0,212,255,60);
    int hLines[] = {60,150,250,350};
    for (int i = 0; i < 4; i++) drawLine(dev, 30, hLines[i], w-30, hLines[i], 2, traceColor);
    int vLines[] = {100,300,500,700};
    for (int i = 0; i < 4; i++) drawLine(dev, vLines[i], 30, vLines[i], h-30, 2, traceColor);
    Color nodeColor = Color::rgb(0,212,255,120);
    int nodePos[6][2] = {{100,60},{300,150},{500,250},{700,350},{100,250},{300,350}};
    for (int i = 0; i < 6; i++) fillEllipse(dev, nodePos[i][0], nodePos[i][1], 3, 3, nodeColor);
    drawEllipseOutline(dev, 0, 0, 15, 15, 2, Color::rgb(0,212,255,80));
    drawEllipseOutline(dev, w, h, 15, 15, 2, Color::rgb(0,212,255,80));
}

static void drawBgDeath(RenderDevice* dev, int w, int h, ThemeColors& c) {
    drawDiagonalGradientRect(dev, Rect(0,0,(float)w,(float)h), c.bg1, c.bg2);
    drawDiagonalGradientRect(dev, Rect(0,0,(float)w,(float)h), Color::rgb(139,0,0,30), Color::rgb(50,20,10,20));
    Color scytheColor = Color::rgb(139,0,0,80);
    int handleX = w-80;
    drawLine(dev, handleX, 80, handleX, h-80, 2, scytheColor);
    drawLine(dev, handleX, 80, handleX-60, 100, 2, scytheColor);
    drawLine(dev, handleX-60, 100, handleX-50, 180, 2, scytheColor);
    drawLine(dev, handleX-50, 180, handleX-30, 200, 2, scytheColor);
    drawLine(dev, handleX-30, 200, handleX-45, 230, 2, scytheColor);
    int seed = 44;
    for (int i = 0; i < 5; i++) {
        seed = seed*1103515245+12345; int x = (seed>>8)%(w-100)+50;
        seed = seed*1103515245+12345; int y = (seed>>8)%h;
        fillEllipse(dev, x, y, 5, 8, Color::rgb(139,0,0,80));
        drawLine(dev, x-2, y-14, x-2, y-22, 1, Color::rgb(255,50,50,120));
        drawLine(dev, x+2, y-14, x+2, y-20, 1, Color::rgb(255,50,50,120));
        drawLine(dev, x, y-16, x, y-24, 1, Color::rgb(255,50,50,120));
    }
}

static void drawBgRiver(RenderDevice* dev, int w, int h, ThemeColors& c) {
    drawGradientRect(dev, Rect(0,0,(float)w,(float)h), c.bg1, c.bg3);
    Color waveColor = Color::rgb(125,204,199,60);
    for (int x = 0; x < w; x += 15) {
        int wy = (int)(h*0.5f+sinf(x*0.03f)*5);
        drawLine(dev, x, wy, x+15, wy, 1.5f, waveColor);
    }
    for (int x = 0; x < w; x += 20) {
        int wy = (int)(h*0.7f+sinf(x*0.04f)*6);
        drawLine(dev, x, wy, x+20, wy, 1.5f, waveColor);
    }
    Color waveColor2 = Color::rgb(125,204,199,40);
    for (int x = 0; x < w; x += 25) {
        int wy = (int)(h*0.85f+sinf(x*0.02f)*4);
        drawLine(dev, x, wy, x+25, wy, 1, waveColor2);
    }
    int seed = 77;
    Color bubbleColor = Color::rgb(255,255,255,80);
    Color highlightColor = Color::rgb(255,255,255,150);
    for (int i = 0; i < 18; i++) {
        seed = seed*1103515245+12345; int x = (seed>>8)%w;
        seed = seed*1103515245+12345; int y = (seed>>8)%(int)(h*0.8f)+(int)(h*0.2f);
        seed = seed*1103515245+12345; int r = (seed%4)+2;
        fillEllipse(dev, x, y, r, r, bubbleColor);
        fillEllipse(dev, x-r/4, y-r/4, r/4, r/4, highlightColor);
    }
    Color fishColor = Color::rgb(61,204,199,100);
    int fishPos[4][2] = {{(int)(w*0.2f),(int)(h*0.6f)},{(int)(w*0.5f),(int)(h*0.5f)},{(int)(w*0.8f),(int)(h*0.7f)},{(int)(w*0.35f),(int)(h*0.75f)}};
    for (int i = 0; i < 4; i++) {
        int fx = fishPos[i][0], fy = fishPos[i][1];
        fillEllipse(dev, fx, fy, 8, 4, fishColor);
        Vec2 tail[] = {Vec2(fx+8,fy),Vec2(fx+14,fy-4),Vec2(fx+14,fy+4)};
        fillPolygon(dev, tail, 3, fishColor);
        fillEllipse(dev, fx-5, fy, 1, 1, Color::rgb(0,0,0,200));
    }
}

static void drawBgOcean(RenderDevice* dev, int w, int h, ThemeColors& c) {
    drawGradientRect(dev, Rect(0,0,(float)w,(float)h), c.bg1, c.bg3);
    drawRect(dev, Rect(0, h*0.6f, (float)w, (float)(h*0.4f)), Color::rgb(27,140,163,80));
    for (int layer = 0; layer < 3; layer++) {
        int baseY = (int)(h*(0.55f+layer*0.12f));
        Color wColor = Color::rgb(27+layer*20,140+layer*30,163+layer*20,60-layer*15);
        for (int x = 0; x < w; x += 20) {
            int wy = baseY+(int)(sinf(x*0.02f+layer)*8);
            int wy2 = baseY+(int)(sinf((x+20)*0.02f+layer)*8);
            Vec2 waveQuad[] = {Vec2(x,wy),Vec2(x+20,wy2),Vec2(x+20,h),Vec2(x,h)};
            fillPolygon(dev, waveQuad, 4, wColor);
        }
    }
    int seed = 33;
    Color bubbleColor = Color::rgb(79,195,247,60);
    for (int i = 0; i < 10; i++) {
        seed = seed*1103515245+12345; int x = (seed>>8)%w;
        seed = seed*1103515245+12345; int y = (seed>>8)%(int)(h*0.4f)+(int)(h*0.6f);
        fillEllipse(dev, x, y, 2, 2, bubbleColor);
    }
}

static void drawBgFire(RenderDevice* dev, int w, int h, ThemeColors& c) {
    drawDiagonalGradientRect(dev, Rect(0,0,(float)w,(float)h), c.bg1, c.bg2);
    drawGradientRect(dev, Rect(0,0,(float)w,(float)h), Color::rgb(200,60,20,150), Color::rgb(255,80,10,30));
    Vec2 flamePts[] = {Vec2(w*0.3f,h),Vec2(w*0.25f,h*0.6f),Vec2(w*0.4f,h*0.3f),Vec2(w*0.5f,h*0.2f),Vec2(w*0.6f,h*0.3f),Vec2(w*0.75f,h*0.6f),Vec2(w*0.7f,h)};
    fillPolygonConcave(dev, flamePts, 7, Color::rgb(255,87,34,60));
    Vec2 innerPts[] = {Vec2(w*0.35f,h),Vec2(w*0.33f,h*0.7f),Vec2(w*0.45f,h*0.45f),Vec2(w*0.5f,h*0.38f),Vec2(w*0.55f,h*0.45f),Vec2(w*0.67f,h*0.7f),Vec2(w*0.65f,h)};
    fillPolygonConcave(dev, innerPts, 7, Color::rgb(255,160,0,40));
}

static void drawBgWerewolf(RenderDevice* dev, int w, int h, ThemeColors& c) {
    drawDiagonalGradientRect(dev, Rect(0,0,(float)w,(float)h), c.bg1, c.bg2);
    float moonX = w-100, moonY = 100, moonR = 80;
    fillEllipse(dev, moonX, moonY, moonR+20, moonR+20, Color::rgb(139,0,0,40));
    fillEllipse(dev, moonX, moonY, moonR, moonR, Color::rgb(139,0,0,150));
    Color treeColor = Color::rgb(160,90,44,100);
    int tx = 40;
    drawLine(dev, tx, h-40, tx, h-120, 2, treeColor);
    drawLine(dev, tx, h-100, tx-30, h-140, 2, treeColor);
    drawLine(dev, tx, h-90, tx+30, h-130, 2, treeColor);
    drawLine(dev, tx, h-110, tx-25, h-160, 2, treeColor);
    drawLine(dev, tx, h-105, tx+25, h-155, 2, treeColor);
    Color clawColor = Color::rgb(139,0,0,80);
    int clawPos[5][2] = {{(int)(w*0.3f),(int)(h*0.7f)},{(int)(w*0.5f),(int)(h*0.8f)},{(int)(w*0.6f),(int)(h*0.6f)},{(int)(w*0.8f),(int)(h*0.75f)},{(int)(w*0.4f),(int)(h*0.5f)}};
    for (int i = 0; i < 5; i++) {
        int cx = clawPos[i][0], cy = clawPos[i][1];
        for (int j = 0; j < 3; j++) drawArcEllipse(dev, cx+j*8+10, cy+5, 10, 5, 1.5f, 0, 180, clawColor);
    }
    int bloodPos[4][2] = {{(int)(w*0.2f),(int)(h*0.3f)},{(int)(w*0.7f),(int)(h*0.4f)},{(int)(w*0.45f),(int)(h*0.6f)},{(int)(w*0.85f),(int)(h*0.5f)}};
    for (int i = 0; i < 4; i++) {
        fillEllipse(dev, bloodPos[i][0], bloodPos[i][1], 3, 6, Color::rgb(139,0,0,80));
    }
}

static void drawBgKingdom(RenderDevice* dev, int w, int h, ThemeColors& c) {
    drawDiagonalGradientRect(dev, Rect(0,0,(float)w,(float)h), c.bg1, c.bg2);
    fillRadialGradientCircle(dev, w/2.0f, h/2.0f, 150, Color::rgb(201,168,76,30), Color::rgb(201,168,76,0));
    Color castleColor = Color::rgb(10,27,47,80);
    drawRect(dev, Rect(0, h-120, (float)w, 120), castleColor);
    int towerX[] = {(int)(w*0.2f),(int)(w*0.5f),(int)(w*0.8f)};
    for (int i = 0; i < 3; i++) {
        drawRect(dev, Rect((float)(towerX[i]-15),(float)(h-200),30,120), castleColor);
        drawRect(dev, Rect((float)(towerX[i]-5),(float)(h-220),10,20), castleColor);
    }
    Vec2 crownPts[] = {Vec2(50,80),Vec2(60,50),Vec2(70,70),Vec2(80,40),Vec2(90,70),Vec2(100,50),Vec2(110,80)};
    fillPolygonConcave(dev, crownPts, 7, Color::rgb(201,168,76,60));
    drawPolygonOutline(dev, crownPts, 7, 2, Color::rgb(201,168,76,100));
    fillEllipse(dev, 67, 57, 2, 2, Color::rgb(201,168,76,120));
    fillEllipse(dev, 67, 77, 2, 2, Color::rgb(201,168,76,120));
    Vec2 shieldPts[] = {Vec2(w-100,80),Vec2(w-100,100),Vec2(w-90,130),Vec2(w-80,130),Vec2(w-80,80)};
    fillPolygon(dev, shieldPts, 5, Color::rgb(201,168,76,40));
    drawPolygonOutline(dev, shieldPts, 5, 2, Color::rgb(201,168,76,80));
    drawLine(dev, w-85, 100, w-65, 100, 2, Color::rgb(201,168,76,80));
    drawLine(dev, w-75, 95, w-75, 110, 2, Color::rgb(201,168,76,80));
    Color scrollColor = Color::rgb(201,168,76,50);
    drawArcEllipse(dev, w-110, h-40, 20, 10, 1, 180, 180, scrollColor);
    drawArcEllipse(dev, w-110, h-30, 20, 10, 1, 0, 180, scrollColor);
    drawLine(dev, w-130, h-40, w-130, h-30, 1, scrollColor);
    drawLine(dev, w-90, h-40, w-90, h-30, 1, scrollColor);
}

static void drawBgComputer(RenderDevice* dev, int w, int h, ThemeColors& c) {
    drawDiagonalGradientRect(dev, Rect(0,0,(float)w,(float)h), c.bg1, c.bg2);
    drawDiagonalGradientRect(dev, Rect(0,0,(float)w,(float)h), Color::rgb(0,191,255,20), Color::rgb(0,100,200,10));
    Color traceColor = Color::rgb(0,191,255,50);
    int hTraces[] = {40,100,180,260,340,420};
    for (int i = 0; i < 6; i++) drawLine(dev, 20, hTraces[i], w-20, hTraces[i], 1.5f, traceColor);
    int vTraces[] = {80,200,350,500,650,800};
    for (int i = 0; i < 6; i++) { if (vTraces[i] < w) drawLine(dev, vTraces[i], 20, vTraces[i], h-20, 1.5f, traceColor); }
    int extraH[] = {70,140,220,300};
    for (int i = 0; i < 4; i++) {
        drawLine(dev, 50, extraH[i], 150, extraH[i], 1.5f, traceColor);
        drawLine(dev, 250, extraH[i], 400, extraH[i], 1.5f, traceColor);
    }
    Color dotColor = Color::rgb(0,191,255,120);
    int dotPos[16][2] = {{80,40},{200,100},{350,180},{500,260},{650,340},{800,420},{80,180},{200,260},{350,340},{500,420},{80,100},{200,40},{350,100},{500,180},{650,260},{800,100}};
    for (int i = 0; i < 16; i++) { if (dotPos[i][0] < w) fillEllipse(dev, dotPos[i][0], dotPos[i][1], 3, 3, dotColor); }
    float chipX = w*0.35f, chipY = h*0.4f, chipW = 160, chipH = 100;
    drawRoundedBorder(dev, Rect(chipX, chipY, chipW, chipH), Color::rgb(0,191,255,80), 2, 5);
    for (int i = 0; i < 8; i++) {
        drawLine(dev, chipX+5+i*20, chipY, chipX+5+i*20, chipY-10, 2, Color::rgb(0,191,255,80));
        drawLine(dev, chipX+5+i*20, chipY+chipH, chipX+5+i*20, chipY+chipH+10, 2, Color::rgb(0,191,255,80));
    }
    Color dataColor = Color::rgb(0,255,200,150);
    int dataPos[6][2] = {{(int)(w*0.2f),(int)(h*0.3f)},{(int)(w*0.6f),(int)(h*0.5f)},{(int)(w*0.3f),(int)(h*0.7f)},{(int)(w*0.7f),(int)(h*0.2f)},{(int)(w*0.5f),(int)(h*0.8f)},{(int)(w*0.8f),(int)(h*0.6f)}};
    for (int i = 0; i < 6; i++) fillEllipse(dev, dataPos[i][0], dataPos[i][1], 4, 4, dataColor);
    Color portColor = Color::rgb(0,191,255,60);
    drawBorder(dev, Rect(5,5,25,15), portColor, 1.5f);
    drawBorder(dev, Rect((float)(w-30),5,25,15), portColor, 1.5f);
    drawBorder(dev, Rect(5,(float)(h-20),25,15), portColor, 1.5f);
    drawBorder(dev, Rect((float)(w-30),(float)(h-20),25,15), portColor, 1.5f);
}

static void drawBgMission(RenderDevice* dev, int w, int h, ThemeColors& c) {
    drawDiagonalGradientRect(dev, Rect(0,0,(float)w,(float)h), c.bg1, c.bg2);
    Color gridColor = Color::rgb(255,102,0,30);
    int step = 40;
    for (int x = 0; x < w; x += step) drawLine(dev, x, 0, x, h, 0.5f, gridColor);
    for (int y = 0; y < h; y += step) drawLine(dev, 0, y, w, y, 0.5f, gridColor);
    float radarCx = 80, radarCy = 80;
    Color radarColor = Color::rgb(255,102,0,80);
    for (int r = 20; r <= 60; r += 20) drawEllipseOutline(dev, radarCx, radarCy, r, r, 1.5f, radarColor);
    drawLine(dev, radarCx-60, radarCy, radarCx+60, radarCy, 1.5f, radarColor);
    drawLine(dev, radarCx, radarCy-60, radarCx, radarCy+60, 1.5f, radarColor);
    fillPie(dev, radarCx, radarCy, 60, 60, 0, 45, Color::rgb(255,102,0,30));
    float crossCx = w-80, crossCy = h-80;
    int crossSize = 25;
    drawLine(dev, crossCx-crossSize, crossCy, crossCx+crossSize, crossCy, 2, radarColor);
    drawLine(dev, crossCx, crossCy-crossSize, crossCx, crossCy+crossSize, 2, radarColor);
    drawEllipseOutline(dev, crossCx, crossCy, crossSize/2, crossSize/2, 2, radarColor);
    Color targetColor = Color::rgb(255,102,0,100);
    drawBorder(dev, Rect((float)(w-120),60,60,40), targetColor, 1.5f);
    drawLine(dev, w-120, 60, w-60, 100, 1.5f, targetColor);
    drawLine(dev, w-60, 60, w-120, 100, 1.5f, targetColor);
    Color statusColor = Color::rgb(255,102,0,60);
    drawRect(dev, Rect(20, h-30, 200, 20), Color::rgb(255,102,0,20));
    drawBorder(dev, Rect(20, h-30, 200, 20), statusColor, 1);
    drawRect(dev, Rect(240, h-30, 100, 20), Color::rgb(255,102,0,20));
    drawBorder(dev, Rect(240, h-30, 100, 20), statusColor, 1);
    drawRect(dev, Rect(360, h-30, 150, 20), Color::rgb(255,102,0,20));
    drawBorder(dev, Rect(360, h-30, 150, 20), statusColor, 1);
}

static void drawBgWinter(RenderDevice* dev, int w, int h, ThemeColors& c) {
    drawGradientRect(dev, Rect(0,0,(float)w,(float)h), c.bg1, c.bg2);
    fillEllipse(dev, w-80, 70, 50, 50, Color::rgb(200,230,255,30));
    fillEllipse(dev, w-70, 80, 40, 40, Color::rgb(200,230,255,150));
    int seed = 42;
    Color snowColor = Color::rgb(255,255,255,180);
    for (int i = 0; i < 35; i++) {
        seed = seed*1103515245+12345; int x = (seed>>8)%w;
        seed = seed*1103515245+12345; int y = (seed>>8)%h;
        seed = seed*1103515245+12345; int sz = (seed%3)+2;
        fillEllipse(dev, x, y, sz, sz, snowColor);
    }
    Color starColor = Color::rgb(255,255,255,120);
    int starPos[8][2] = {{(int)(w*0.2f),(int)(h*0.3f)},{(int)(w*0.5f),(int)(h*0.15f)},{(int)(w*0.8f),(int)(h*0.4f)},{(int)(w*0.1f),(int)(h*0.5f)},{(int)(w*0.6f),(int)(h*0.25f)},{(int)(w*0.35f),(int)(h*0.45f)},{(int)(w*0.9f),(int)(h*0.15f)},{(int)(w*0.45f),(int)(h*0.6f)}};
    for (int i = 0; i < 8; i++) {
        int sx = starPos[i][0], sy = starPos[i][1];
        for (int j = 0; j < 6; j++) {
            float ang = j*60*PI/180.0f;
            drawLine(dev, sx, sy, sx+10*cosf(ang), sy+10*sinf(ang), 1.5f, starColor);
        }
    }
    Color iceColor = Color::rgb(200,230,255,80);
    drawLine(dev, 10, h-10, 10, h-50, 1, iceColor);
    drawLine(dev, 10, h-50, 25, h-40, 1, iceColor);
    drawLine(dev, 10, h-30, 20, h-20, 1, iceColor);
    drawLine(dev, 5, h-15, 15, h-35, 1, iceColor);
    drawLine(dev, 15, h-45, 30, h-55, 1, iceColor);
    drawLine(dev, w-10, h-10, w-10, h-50, 1, iceColor);
    drawLine(dev, w-10, h-50, w-25, h-40, 1, iceColor);
    drawLine(dev, w-10, h-30, w-20, h-20, 1, iceColor);
    drawLine(dev, w-5, h-15, w-15, h-35, 1, iceColor);
    drawLine(dev, w-15, h-45, w-30, h-55, 1, iceColor);
    Color mt3Color = Color::rgb(200,230,255,60);
    Vec2 mtPts[] = {Vec2(0,h*0.75f),Vec2(w*0.1f,h*0.6f),Vec2(w*0.2f,h*0.7f),Vec2(w*0.3f,h*0.65f),Vec2(w*0.4f,h*0.55f),Vec2(w*0.5f,h*0.7f),Vec2(w*0.6f,h*0.6f),Vec2(w*0.7f,h*0.5f),Vec2(w*0.85f,h*0.65f),Vec2(w,h*0.7f),Vec2(w,h),Vec2(0,h)};
    fillPolygon(dev, mtPts, 12, mt3Color);
}

static void drawBgSamsara(RenderDevice* dev, int w, int h, ThemeColors& c) {
    drawDiagonalGradientRect(dev, Rect(0,0,(float)w,(float)h), c.bg1, c.bg2);
    float cx = w*0.5f, cy = h*0.5f;
    fillRadialGradientCircle(dev, cx, cy, 200, Color::rgb(201,168,76,20), Color::rgb(75,53,32,0));
    Color ringColor = Color::rgb(201,168,76,80);
    drawEllipseOutline(dev, cx, cy, 180, 180, 2, ringColor);
    drawEllipseOutline(dev, cx, cy, 140, 140, 2, ringColor);
    Color spokeColor = Color::rgb(155,89,182,60);
    for (int i = 0; i < 6; i++) {
        float ang = i*60*PI/180.0f;
        drawLine(dev, cx+140*cosf(ang), cy+140*sinf(ang), cx+180*cosf(ang), cy+180*sinf(ang), 1.5f, spokeColor);
    }
    Color lotusColor = Color::rgb(230,180,100,60);
    for (int i = 0; i < 8; i++) {
        float ang = i*45*PI/180.0f;
        drawEllipseOutline(dev, cx+25*cosf(ang), cy+25*sinf(ang), 15, 15, 2, lotusColor);
    }
    fillEllipse(dev, cx, cy, 20, 20, Color::rgb(201,168,76,50));
    Color flowerColor = Color::rgb(220,50,50,30);
    for (int i = 0; i < 6; i++) {
        float ang = i*60*PI/180.0f;
        Vec2 petalPts[] = {Vec2(100,100),Vec2(100+15*cosf(ang+0.3f),100-20),Vec2(100+10*cosf(ang+0.5f),100-40),Vec2(100,100-50),Vec2(100-10*cosf(ang+0.5f),100-40),Vec2(100-15*cosf(ang+0.3f),100-20)};
        fillPolygonConcave(dev, petalPts, 6, flowerColor);
    }
    fillEllipse(dev, 100, 100, 8, 8, Color::rgb(255,255,200,40));
    Color lotus2Color = Color::rgb(180,220,180,25);
    for (int i = 0; i < 8; i++) {
        float ang = i*45*PI/180.0f;
        drawEllipseOutline(dev, w-120+20*cosf(ang), h-120+20*sinf(ang), 10, 10, 2, lotus2Color);
    }
    fillEllipse(dev, w-120, h-120, 12, 12, Color::rgb(255,255,150,30));
    Color arrowColor = Color::rgb(201,168,76,30);
    drawArcEllipse(dev, 30, h-30, 25, 25, 3, 30, 300, arrowColor);
    drawLine(dev, 45, h-48, 52, h-38, 3, arrowColor);
    drawLine(dev, 45, h-48, 35, h-40, 3, arrowColor);
}

static void drawBgMagic(RenderDevice* dev, int w, int h, ThemeColors& c) {
    drawDiagonalGradientRect(dev, Rect(0,0,(float)w,(float)h), c.bg1, c.bg2);
    drawDiagonalGradientRect(dev, Rect(0,0,(float)w,(float)h), Color::rgb(123,75,148,30), Color::rgb(201,162,39,40));
    float cx = w*0.5f, cy = h*0.5f, radius = 80;
    drawEllipseOutline(dev, cx, cy, radius, radius, 2, Color::rgb(201,162,39,100));
    Vec2 hexPts[6];
    for (int i = 0; i < 6; i++) {
        float ang = (i*60-30)*PI/180.0f;
        hexPts[i] = Vec2(cx+radius*0.7f*cosf(ang), cy+radius*0.7f*sinf(ang));
    }
    drawPolygonOutline(dev, hexPts, 6, 1.5f, Color::rgb(123,75,148,80));
    Vec2 tri1[] = {Vec2(cx,cy-radius*0.6f),Vec2(cx-radius*0.52f,cy+radius*0.3f),Vec2(cx+radius*0.52f,cy+radius*0.3f)};
    drawPolygonOutline(dev, tri1, 3, 2, Color::rgb(201,162,39,120));
    Vec2 tri2[] = {Vec2(cx,cy+radius*0.6f),Vec2(cx-radius*0.52f,cy-radius*0.3f),Vec2(cx+radius*0.52f,cy-radius*0.3f)};
    drawPolygonOutline(dev, tri2, 3, 2, Color::rgb(201,162,39,120));
    Color particleColor = Color::rgb(240,230,140,120);
    int particlePos[5][2] = {{(int)(cx-200),(int)(cy-100)},{(int)(cx+180),(int)(cy-80)},{(int)(cx-50),(int)(cy-150)},{(int)(cx+120),(int)(cy+130)},{(int)(cx-180),(int)(cy+120)}};
    for (int i = 0; i < 5; i++) fillEllipse(dev, particlePos[i][0], particlePos[i][1], 3, 3, particleColor);
    drawCornerDecor(dev, w, h, 30, 1, Color::rgb(201,162,39,50));
}

static void drawBgGeneric(RenderDevice* dev, int w, int h, ThemeColors& c) {
    if (c.useGradient) {
        drawDiagonalGradientRect(dev, Rect(0,0,(float)w,(float)h), c.bg1, c.bg2);
    } else {
        drawRect(dev, Rect(0,0,(float)w,(float)h), c.bg1);
    }
}

// ============================================================
// 自定义背景系统（bgType 100-103，主题工作室「自定义」子模式编辑）
//   100 纯色 / 101 四角渐变 / 102 网格 / 103 图片
//   200 动态背景（主界面叠加层 drawDynamicBg 渲染，此处画兜底底色）
// g_bgCustom 为背景系统全局配置；图片走懒加载（stb 解码 → GPU 纹理缓存，
// 路径变更自动重载，失败自动回退深色底）。
// ============================================================
BgCustomConfig g_bgCustom;

static RenderDevice* s_bgDev = nullptr;      // 纹理生命周期持有（drawBackground 时记录）
static std::string   s_bgLoadedPath;         // 已解码路径（与 imgPath 不一致时触发重载）

void releaseBgImageTexture() {
    if (g_bgCustom.imgTex != 0 && s_bgDev) s_bgDev->destroyTexture(g_bgCustom.imgTex);
    g_bgCustom.imgTex = 0;
    g_bgCustom.imgW = 0;
    g_bgCustom.imgH = 0;
    s_bgLoadedPath.clear();
}

// 图片懒加载：路径有效且纹理未建/已换图时解码（失败置 imgLoaded=false 回退）
// 读取文件字节（Windows 下转宽字符路径：stbi_load 内部 fopen 为 ANSI，
// 中文路径（如 E:\工作区\...）会打开失败，对照参考项目 GDI+ 宽字符加载）
static unsigned char* loadFileBytes(const std::string& pathUtf8, size_t* outSize) {
#ifdef _WIN32
    int n = MultiByteToWideChar(CP_UTF8, 0, pathUtf8.c_str(), -1, nullptr, 0);
    if (n <= 1) return nullptr;
    std::wstring wpath((size_t)n - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, pathUtf8.c_str(), -1, &wpath[0], n);
    FILE* f = _wfopen(wpath.c_str(), L"rb");
#else
    FILE* f = std::fopen(pathUtf8.c_str(), "rb");
#endif
    if (!f) return nullptr;
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (sz <= 0) { std::fclose(f); return nullptr; }
    unsigned char* buf = (unsigned char*)std::malloc((size_t)sz);
    if (!buf) { std::fclose(f); return nullptr; }
    size_t rd = std::fread(buf, 1, (size_t)sz, f);
    std::fclose(f);
    if (rd != (size_t)sz) { std::free(buf); return nullptr; }
    *outSize = (size_t)sz;
    return buf;
}

static bool ensureBgImage(RenderDevice* dev) {
    if (!g_bgCustom.imgLoaded || g_bgCustom.imgPath.empty()) return false;
    if (g_bgCustom.imgTex != 0 && g_bgCustom.imgPath == s_bgLoadedPath) return true;
    releaseBgImageTexture();
    s_bgDev = dev;
    int iw = 0, ih = 0, ch = 0;
    // 内存解码（绕开 stbi_load 的 ANSI fopen 限制，中文/Unicode 路径可用）
    size_t sz = 0;
    unsigned char* bytes = loadFileBytes(g_bgCustom.imgPath, &sz);
    if (!bytes) {
        UI_LOG_WARN("背景图打开失败: %s", g_bgCustom.imgPath.c_str());
        g_bgCustom.imgLoaded = false;
        return false;
    }
    unsigned char* data = stbi_load_from_memory(bytes, (int)sz, &iw, &ih, &ch, 4);
    std::free(bytes);
    if (!data || iw <= 0 || ih <= 0) {
        UI_LOG_WARN("背景图解码失败: %s", g_bgCustom.imgPath.c_str());
        g_bgCustom.imgLoaded = false;
        return false;
    }
    uint32_t tex = dev->createTexture(iw, ih, data);
    stbi_image_free(data);
    if (tex == 0) { g_bgCustom.imgLoaded = false; return false; }
    g_bgCustom.imgTex = tex;
    g_bgCustom.imgW = iw;
    g_bgCustom.imgH = ih;
    s_bgLoadedPath = g_bgCustom.imgPath;
    UI_LOG_INFO("背景图已加载: %s (%dx%d)", g_bgCustom.imgPath.c_str(), iw, ih);
    return true;
}

// 纹理四边形（srcRect 为源图像素子矩形；alpha 乘进顶点色）
static void drawImageQuad(RenderDevice* dev, uint32_t tex, int texW, int texH,
                          const Rect& dst, float sx, float sy, float sw, float sh,
                          float alpha) {
    if (tex == 0 || texW <= 0 || texH <= 0 || sw <= 0 || sh <= 0) return;
    float u0 = sx / texW, v0 = sy / texH;
    float u1 = (sx + sw) / texW, v1 = (sy + sh) / texH;
    Vertex v[4] = {
        { Vec2(dst.pos.x,              dst.pos.y),              Vec2(u0, v0), Color(1, 1, 1, alpha) },
        { Vec2(dst.pos.x + dst.size.x, dst.pos.y),              Vec2(u1, v0), Color(1, 1, 1, alpha) },
        { Vec2(dst.pos.x + dst.size.x, dst.pos.y + dst.size.y), Vec2(u1, v1), Color(1, 1, 1, alpha) },
        { Vec2(dst.pos.x,              dst.pos.y + dst.size.y), Vec2(u0, v1), Color(1, 1, 1, alpha) },
    };
    const uint32_t idx[6] = { 0, 1, 2, 0, 2, 3 };
    dev->drawTriangles(v, 4, idx, 6, tex, true);
}

static Color mixColor(const Color& a, const Color& b, float t) {
    return Color(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t,
                 a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t);
}

// 四角渐变：细分 8x8 网格逐顶点双线性插值（角色 + 强度→透明度），
// 近似参考 GDI+ PathGradientBrush 的中心平均色→四角过渡效果
static void drawCornerGradient(RenderDevice* dev, const Rect& r, const BgCustomConfig& cfg,
                               float alpha) {
    const int N = 8;
    static Vertex v[(N + 1) * (N + 1)];
    static uint32_t idx[N * N * 6];
    for (int j = 0; j <= N; j++) {
        float ty = (float)j / N;                    // 0=顶 1=底
        for (int i = 0; i <= N; i++) {
            float tx = (float)i / N;                // 0=左 1=右
            Color top    = mixColor(cfg.gradColors[0], cfg.gradColors[1], tx);  // 左上→右上
            Color bottom = mixColor(cfg.gradColors[3], cfg.gradColors[2], tx);  // 左下→右下
            Color col    = mixColor(top, bottom, ty);
            float aTop    = (cfg.gradIntensity[0] + (cfg.gradIntensity[1] - cfg.gradIntensity[0]) * tx) / 100.0f;
            float aBottom = (cfg.gradIntensity[3] + (cfg.gradIntensity[2] - cfg.gradIntensity[3]) * tx) / 100.0f;
            float av = clampf(aTop + (aBottom - aTop) * ty, 0.0f, 1.0f) * alpha;
            v[j * (N + 1) + i] = { Vec2(r.pos.x + tx * r.size.x, r.pos.y + ty * r.size.y),
                                   Vec2(0, 0), Color(col.r, col.g, col.b, av) };
        }
    }
    int k = 0;
    for (int j = 0; j < N; j++) {
        for (int i = 0; i < N; i++) {
            int v0 = j * (N + 1) + i, v1 = v0 + 1;
            int v2 = v0 + N + 1,     v3 = v2 + 1;
            idx[k++] = v0; idx[k++] = v1; idx[k++] = v2;
            idx[k++] = v1; idx[k++] = v3; idx[k++] = v2;
        }
    }
    dev->drawTriangles(v, (N + 1) * (N + 1), idx, N * N * 6, 0, false);
}

// 网格背景：底色平铺 + 等距网格线（间距/粗细/透明度全配置驱动）
static void drawGridBg(RenderDevice* dev, const Rect& r, const BgCustomConfig& cfg,
                       float alpha) {
    drawRect(dev, r, Color(cfg.gridBgColor.r, cfg.gridBgColor.g, cfg.gridBgColor.b, alpha));
    Color line = Color(cfg.gridLineColor.r, cfg.gridLineColor.g, cfg.gridLineColor.b,
                       clampf(cfg.gridLineAlpha / 255.0f, 0.0f, 1.0f) * alpha);
    float step = (float)cfg.gridSpacing;
    if (step < 2.0f) step = 2.0f;
    float lw = (float)cfg.gridLineWidth;
    if (lw < 1.0f) lw = 1.0f;
    for (float gx = r.pos.x; gx <= r.pos.x + r.size.x; gx += step)
        drawLine(dev, gx, r.pos.y, gx, r.pos.y + r.size.y, lw, line);
    for (float gy = r.pos.y; gy <= r.pos.y + r.size.y; gy += step)
        drawLine(dev, r.pos.x, gy, r.pos.x + r.size.x, gy, lw, line);
}

// 图片背景：三种填充模式（0=拉伸 1=平铺 2=居中裁剪），透明度可调
static void drawImageBg(RenderDevice* dev, const Rect& r, float alpha) {
    if (!ensureBgImage(dev)) {
        drawRect(dev, r, Color(0.12f, 0.12f, 0.12f, alpha));
        return;
    }
    float a = clampf(g_bgCustom.imgAlpha / 100.0f, 0.0f, 1.0f) * alpha;
    float iw = (float)g_bgCustom.imgW, ih = (float)g_bgCustom.imgH;
    if (g_bgCustom.imgFillMode == 0) {           // 拉伸：全图 → 全屏
        drawImageQuad(dev, g_bgCustom.imgTex, g_bgCustom.imgW, g_bgCustom.imgH,
                      r, 0, 0, iw, ih, a);
    } else if (g_bgCustom.imgFillMode == 1) {    // 平铺：原图尺寸逐块铺满
        for (float ty = r.pos.y; ty < r.pos.y + r.size.y; ty += ih)
            for (float tx = r.pos.x; tx < r.pos.x + r.size.x; tx += iw) {
                float tw = (tx + iw > r.pos.x + r.size.x) ? (r.pos.x + r.size.x - tx) : iw;
                float th = (ty + ih > r.pos.y + r.size.y) ? (r.pos.y + r.size.y - ty) : ih;
                drawImageQuad(dev, g_bgCustom.imgTex, g_bgCustom.imgW, g_bgCustom.imgH,
                              Rect(Vec2(tx, ty), Vec2(tw, th)), 0, 0, tw, th, a);
            }
    } else {                                     // 裁剪：源图居中裁出同比例区域 → 全屏
        float srcRatio = iw / ih, dstRatio = r.size.x / r.size.y;
        float sw = iw, sh = ih;
        if (srcRatio > dstRatio) sw = ih * dstRatio;
        else                     sh = iw / dstRatio;
        drawImageQuad(dev, g_bgCustom.imgTex, g_bgCustom.imgW, g_bgCustom.imgH,
                      r, (iw - sw) * 0.5f, (ih - sh) * 0.5f, sw, sh, a);
    }
}

void drawBackground(RenderDevice* dev, int themeIndex, int w, int h) {
    if (!dev) return;
    s_bgDev = dev;   // 记录设备指针：releaseBgImageTexture 销毁纹理用
    ThemeColors& c = g_themes[themeIndex].colors;
    int bgType = g_themes[themeIndex].bgType;
    switch (bgType) {
        case 0: drawBgSimple(dev, w, h, c); break;
        case 1: drawBgTime(dev, w, h, c); break;
        case 2: drawBgGame(dev, w, h, c); break;
        case 3: drawBgKawaii(dev, w, h, c); break;
        case 4: drawBgScifi(dev, w, h, c); break;
        case 5: drawBgArcade(dev, w, h, c); break;
        case 6: drawBgVampire(dev, w, h, c); break;
        case 7: drawBgSky(dev, w, h, c); break;
        case 8: drawBgSpace(dev, w, h, c); break;
        case 9: drawBgDark(dev, w, h, c); break;
        case 10: drawBgCyberpunk(dev, w, h, c); break;
        case 11: drawBgGuofeng(dev, w, h, c); break;
        case 12: drawBgTarot(dev, w, h, c); break;
        case 13: drawBgRetroShip(dev, w, h, c); break;
        case 14: drawBgSummer(dev, w, h, c); break;
        case 15: drawBgAngelDemon(dev, w, h, c); break;
        case 16: drawBgGhost(dev, w, h, c); break;
        case 17: drawBgPuzzle(dev, w, h, c); break;
        case 18: drawBgStarry(dev, w, h, c); break;
        case 19: drawBgSpring(dev, w, h, c); break;
        case 20: drawBgDawn(dev, w, h, c); break;
        case 21: drawBgMoon(dev, w, h, c); break;
        case 22: drawBgWood(dev, w, h, c); break;
        case 23: drawBgRobot(dev, w, h, c); break;
        case 24: drawBgDeath(dev, w, h, c); break;
        case 25: drawBgRiver(dev, w, h, c); break;
        case 26: drawBgOcean(dev, w, h, c); break;
        case 27: drawBgFire(dev, w, h, c); break;
        case 28: drawBgWerewolf(dev, w, h, c); break;
        case 29: drawBgKingdom(dev, w, h, c); break;
        case 30: drawBgComputer(dev, w, h, c); break;
        case 31: drawBgMission(dev, w, h, c); break;
        case 32: drawBgWinter(dev, w, h, c); break;
        case 33: drawBgSamsara(dev, w, h, c); break;
        case 34: drawBgMagic(dev, w, h, c); break;
        // ---- 自定义背景（主题工作室「自定义」子模式应用后写入） ----
        case 100: drawRect(dev, Rect(0, 0, (float)w, (float)h),
                            Color(g_bgCustom.solidColor.r, g_bgCustom.solidColor.g,
                                  g_bgCustom.solidColor.b, 1.0f)); break;
        case 101: drawCornerGradient(dev, Rect(0, 0, (float)w, (float)h), g_bgCustom, 1.0f); break;
        case 102: drawGridBg(dev, Rect(0, 0, (float)w, (float)h), g_bgCustom, 1.0f); break;
        case 103: drawImageBg(dev, Rect(0, 0, (float)w, (float)h), 1.0f); break;
        // 动态背景：主界面 drawDynamicBg 叠加渲染，这里画兜底底色
        case 200: drawBgGeneric(dev, w, h, c); break;
        default: drawBgGeneric(dev, w, h, c); break;
    }
}

// 背景槽位 → 颜色指针（颜色选择器写回用；槽位定义与主题工作室一致）
//   0=纯色 1-4=渐变四角（左上/右上/左下/右下）5=网格底色 6=网格线色
Color* BgSlotColor(int slot) {
    switch (slot) {
        case 0: return &g_bgCustom.solidColor;
        case 1: return &g_bgCustom.gradColors[0];
        case 2: return &g_bgCustom.gradColors[1];
        case 3: return &g_bgCustom.gradColors[2];
        case 4: return &g_bgCustom.gradColors[3];
        case 5: return &g_bgCustom.gridBgColor;
        case 6: return &g_bgCustom.gridLineColor;
    }
    return nullptr;
}

// 自定义背景预览（主题工作室「自定义」子模式 70px 预览条）
// 结构：bg2 帧底 → 按主窗口比例内缩的实际预览区 → 按类型绘制 → 内框描边
void drawBgCustomPreview(RenderDevice* dev, float x, float y, float w, float h, ThemeColors& c) {
    if (!dev) return;
    s_bgDev = dev;
    drawRect(dev, Rect(Vec2(x, y), Vec2(w, h)),
             Color(c.bg2.r, c.bg2.g, c.bg2.b, 200.0f / 255.0f));

    // 按主窗口宽高比内缩（预览区所见即主界面所得）
    float winRatio = (float)g_uiCtx.winW / (float)(g_uiCtx.winH > 0 ? g_uiCtx.winH : 1);
    float previewRatio = w / h;
    float innerW, innerH, innerX, innerY;
    if (winRatio > previewRatio) {
        innerW = w;  innerH = w / winRatio;
        innerX = x;  innerY = y + (h - innerH) * 0.5f;
    } else {
        innerH = h;  innerW = h * winRatio;
        innerX = x + (w - innerW) * 0.5f;
        innerY = y;
    }
    if (innerW < 1) innerW = 1;
    if (innerH < 1) innerH = 1;
    Rect inner(Vec2(innerX, innerY), Vec2(innerW, innerH));

    if (g_bgCustom.type == 0) {
        drawRect(dev, inner, Color(g_bgCustom.solidColor.r, g_bgCustom.solidColor.g,
                                   g_bgCustom.solidColor.b, 1.0f));
    } else if (g_bgCustom.type == 1) {
        drawCornerGradient(dev, inner, g_bgCustom, 1.0f);
    } else if (g_bgCustom.type == 2) {
        drawGridBg(dev, inner, g_bgCustom, 1.0f);
    } else if (g_bgCustom.type == 3) {
        drawImageBg(dev, inner, 1.0f);
        if (!g_bgCustom.imgLoaded && g_uiCtx.smallFont) {
            const char* tip = "未加载图片";
            float tw = g_uiCtx.smallFont->measureText(tip);
            float lh = g_uiCtx.smallFont->scaledLineHeight();
            drawText(dev, g_uiCtx.smallFont, tip,
                     Vec2(inner.pos.x + (inner.size.x - tw) * 0.5f,
                          inner.pos.y + (inner.size.y - lh) * 0.5f),
                     Color(0.78f, 0.78f, 0.78f, 0.47f));
        }
    }
    drawBorder(dev, inner, Color(c.inputBorder.r, c.inputBorder.g, c.inputBorder.b,
                                 150.0f / 255.0f), 1.0f);
}

void drawBgTemplatePreview(RenderDevice* dev, int templateIdx, float x, float y, float w, float h, ThemeColors& c) {
    if (!dev) return;
    dev->setScissorRect((int)x, (int)y, (int)w, (int)h);
    int iw = (int)w; if (iw < 1) iw = 1;
    int ih = (int)h; if (ih < 1) ih = 1;
    switch (templateIdx) {
        case 0: drawBgSimple(dev, iw, ih, c); break;
        case 1: drawBgTime(dev, iw, ih, c); break;
        case 2: drawBgGame(dev, iw, ih, c); break;
        case 3: drawBgKawaii(dev, iw, ih, c); break;
        case 4: drawBgScifi(dev, iw, ih, c); break;
        case 5: drawBgArcade(dev, iw, ih, c); break;
        case 6: drawBgVampire(dev, iw, ih, c); break;
        case 7: drawBgSky(dev, iw, ih, c); break;
        case 8: drawBgSpace(dev, iw, ih, c); break;
        case 9: drawBgDark(dev, iw, ih, c); break;
        case 10: drawBgCyberpunk(dev, iw, ih, c); break;
        case 11: drawBgGuofeng(dev, iw, ih, c); break;
        case 12: drawBgTarot(dev, iw, ih, c); break;
        case 13: drawBgRetroShip(dev, iw, ih, c); break;
        case 14: drawBgSummer(dev, iw, ih, c); break;
        case 15: drawBgAngelDemon(dev, iw, ih, c); break;
        case 16: drawBgGhost(dev, iw, ih, c); break;
        case 17: drawBgPuzzle(dev, iw, ih, c); break;
        case 18: drawBgStarry(dev, iw, ih, c); break;
        case 19: drawBgSpring(dev, iw, ih, c); break;
        case 20: drawBgDawn(dev, iw, ih, c); break;
        case 21: drawBgMoon(dev, iw, ih, c); break;
        case 22: drawBgWood(dev, iw, ih, c); break;
        case 23: drawBgRobot(dev, iw, ih, c); break;
        case 24: drawBgDeath(dev, iw, ih, c); break;
        case 25: drawBgRiver(dev, iw, ih, c); break;
        case 26: drawBgOcean(dev, iw, ih, c); break;
        case 27: drawBgFire(dev, iw, ih, c); break;
        case 28: drawBgWerewolf(dev, iw, ih, c); break;
        case 29: drawBgKingdom(dev, iw, ih, c); break;
        case 30: drawBgComputer(dev, iw, ih, c); break;
        case 31: drawBgMission(dev, iw, ih, c); break;
        case 32: drawBgWinter(dev, iw, ih, c); break;
        case 33: drawBgSamsara(dev, iw, ih, c); break;
        case 34: drawBgMagic(dev, iw, ih, c); break;
        default: drawBgGeneric(dev, iw, ih, c); break;
    }
    dev->resetScissorRect();
}

} // namespace ui