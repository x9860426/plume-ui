#pragma once
#include "../core/Math.h"

#include "../render/RenderDevice.h"
#include "Font.h"
#include <string>
#include <vector>

namespace ui {

void drawRect(RenderDevice* dev, const Rect& r, const Color& c);
void drawBorder(RenderDevice* dev, const Rect& r, const Color& c, float thickness);
void drawText(RenderDevice* dev, Font* font, const char* utf8,
              const Vec2& pos, const Color& c, float scale = 1.0f);

// 滚动容器统一走 RegionSystem（win/RegionSystem.h）：申请区域即获得
// 裁剪/滚轮/滚动条绘制与拖拽/hover/clamp 全套系统能力，禁用私有实现

void drawRoundedRect(RenderDevice* dev, const Rect& r, const Color& c, float radius);
// 四角独立圆角矩形（规范：每角半径独立配置，0=直角；系统 chrome 专用）
void drawCornerRadiiRect(RenderDevice* dev, const Rect& r, const Color& c,
                         float tl, float tr, float bl, float br);
void drawTopRoundedRect(RenderDevice* dev, const Rect& r, const Color& c, float topRadius);
void drawBottomRoundedRect(RenderDevice* dev, const Rect& r, const Color& c, float bottomRadius);
void drawLeftRoundedRect(RenderDevice* dev, const Rect& r, const Color& c, float leftRadius);
void drawRightRoundedRect(RenderDevice* dev, const Rect& r, const Color& c, float rightRadius);
void drawRoundedBorder(RenderDevice* dev, const Rect& r, const Color& c, float thickness, float radius);
void drawTopRoundedBorder(RenderDevice* dev, const Rect& r, const Color& c, float thickness, float topRadius);
void drawBottomRoundedBorder(RenderDevice* dev, const Rect& r, const Color& c, float thickness, float bottomRadius);
void drawLeftRoundedBorder(RenderDevice* dev, const Rect& r, const Color& c, float thickness, float leftRadius);
void drawRightRoundedBorder(RenderDevice* dev, const Rect& r, const Color& c, float thickness, float rightRadius);
void drawShadow(RenderDevice* dev, const Rect& r, float blur, float offsetY, const Color& c, float radius = 0.0f);
void drawBottomShadow(RenderDevice* dev, const Rect& r, float blur, float offsetY, const Color& c, float bottomRadius = 0.0f);
void drawGradientRect(RenderDevice* dev, const Rect& r, const Color& top, const Color& bottom);
void drawRoundedGradientRect(RenderDevice* dev, const Rect& r, const Color& top, const Color& bottom, float radius);
void drawRoundedGradientRectH(RenderDevice* dev, const Rect& r, const Color& left, const Color& right, float radius);
void drawTopRoundedGradientRect(RenderDevice* dev, const Rect& r, const Color& top, const Color& bottom, float topRadius);
void drawArc(RenderDevice* dev, float cx, float cy, float radius, float penWidth,
             float startAngleDeg, float sweepAngleDeg, const Color& c, bool roundCap = false);
void drawLine(RenderDevice* dev, float x1, float y1, float x2, float y2, float thickness, const Color& c);
void fillCircle(RenderDevice* dev, float cx, float cy, float radius, const Color& c);
void drawCircle(RenderDevice* dev, float cx, float cy, float radius, float penWidth, const Color& c);
void fillTriangle(RenderDevice* dev, float x1, float y1, float x2, float y2, float x3, float y3, const Color& c);
void fillEllipse(RenderDevice* dev, float cx, float cy, float rx, float ry, const Color& c);
void drawEllipseOutline(RenderDevice* dev, float cx, float cy, float rx, float ry, float penWidth, const Color& c);
void fillPolygon(RenderDevice* dev, const Vec2* pts, int count, const Color& c);
void fillPolygonConcave(RenderDevice* dev, const Vec2* pts, int count, const Color& c);
void drawPolygonOutline(RenderDevice* dev, const Vec2* pts, int count, float penWidth, const Color& c);
void fillPie(RenderDevice* dev, float cx, float cy, float rx, float ry, float startAngleDeg, float sweepAngleDeg, const Color& c);
void drawArcEllipse(RenderDevice* dev, float cx, float cy, float rx, float ry, float penWidth, float startAngleDeg, float sweepAngleDeg, const Color& c);
void drawDiagonalGradientRect(RenderDevice* dev, const Rect& r, const Color& c1, const Color& c2);
void drawHorizontalGradientRect(RenderDevice* dev, const Rect& r, const Color& left, const Color& right);
void fillRadialGradientCircle(RenderDevice* dev, float cx, float cy, float maxR, const Color& centerColor, const Color& edgeColor);



} // namespace ui