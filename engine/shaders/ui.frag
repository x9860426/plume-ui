#version 450

layout(push_constant) uniform PC {
    vec2 uResolution;
    int  uSDFMode;
    int  _pad;
    vec2 uRectPos;
    vec2 uRectSize;
    float uRadiusTL;
    float uRadiusTR;
    float uRadiusBR;
    float uRadiusBL;
    float uBorderThickness;
    float aaOn;
} pc;

layout(location = 0) in vec2 vUV;
layout(location = 1) in vec4 vColor;

layout(location = 0) out vec4 frag;

layout(set = 0, binding = 0) uniform sampler2D uTex;

float sdfRoundedBox4(vec2 p, vec2 halfSize, float rTL, float rTR, float rBR, float rBL) {
    float r = (p.x < 0.0 && p.y < 0.0) ? rTL :
              (p.x > 0.0 && p.y < 0.0) ? rTR :
              (p.x > 0.0 && p.y > 0.0) ? rBR : rBL;
    vec2 q = abs(p) - halfSize + r;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

void main() {
    if (pc.uSDFMode == 0) {
        frag = vColor * texture(uTex, vUV);
    } else {
        vec2 pixelPos = gl_FragCoord.xy;
        vec2 center = pc.uRectPos + pc.uRectSize * 0.5;
        vec2 p = pixelPos - center;
        vec2 halfSize = pc.uRectSize * 0.5;
        float aaWidth = mix(0.51, 2.0, clamp(pc.aaOn, 0.0, 1.0));

        if (pc.uSDFMode == 1) {
            float dist = sdfRoundedBox4(p, halfSize, pc.uRadiusTL, pc.uRadiusTR, pc.uRadiusBR, pc.uRadiusBL);
            float alpha = clamp(0.5 - dist / aaWidth, 0.0, 1.0);
            frag = vec4(vColor.rgb, vColor.a * alpha);
        } else if (pc.uSDFMode == 3) {
            float dist = sdfRoundedBox4(p, halfSize, pc.uRadiusTL, pc.uRadiusTR, pc.uRadiusBR, pc.uRadiusBL);
            float alpha = clamp(1.0 - max(dist, 0.0) / pc.uBorderThickness, 0.0, 1.0);
            frag = vec4(vColor.rgb, vColor.a * alpha);
        } else if (pc.uSDFMode == 4) {
            // 椭圆填充：内切于 uRectPos/uRectSize 包围矩形（halfSize = 半径）
            float dist = (length(p / halfSize) - 1.0) * min(halfSize.x, halfSize.y);
            float alpha = clamp(0.5 - dist / aaWidth, 0.0, 1.0);
            frag = vec4(vColor.rgb, vColor.a * alpha);
        } else if (pc.uSDFMode == 5) {
            // 椭圆描边：uBorderThickness = 线宽
            float dist = abs((length(p / halfSize) - 1.0) * min(halfSize.x, halfSize.y))
                       - pc.uBorderThickness * 0.5;
            float alpha = clamp(0.5 - dist / aaWidth, 0.0, 1.0);
            frag = vec4(vColor.rgb, vColor.a * alpha);
        } else if (pc.uSDFMode == 6) {
            // 线段（平端点）：uRectPos=端点A uRectSize=端点B uBorderThickness=线宽
            vec2 pa = pixelPos - pc.uRectPos;
            vec2 ba = pc.uRectSize - pc.uRectPos;
            float h = clamp(dot(pa, ba) / max(dot(ba, ba), 1e-6), 0.0, 1.0);
            float dist = length(pa - ba * h) - pc.uBorderThickness * 0.5;
            float alpha = clamp(0.5 - dist / aaWidth, 0.0, 1.0);
            frag = vec4(vColor.rgb, vColor.a * alpha);
        } else if (pc.uSDFMode == 7) {
            // 椭圆弧描边：uRadiusTR=起始角 uRadiusBR=扫过角（度，屏幕坐标顺时针）
            float dist = abs((length(p / halfSize) - 1.0) * min(halfSize.x, halfSize.y))
                       - pc.uBorderThickness * 0.5;
            float aDeg = mod(degrees(atan(p.y, p.x)) - pc.uRadiusTR + 720.0, 360.0);
            float alpha = clamp(0.5 - dist / aaWidth, 0.0, 1.0) * step(aDeg, pc.uRadiusBR);
            frag = vec4(vColor.rgb, vColor.a * alpha);
        } else if (pc.uSDFMode == 8) {
            // 三角形填充：p0=uRectPos p1=uRectSize p2=(uRadiusTL,uRadiusTR)
            vec2 p2 = vec2(pc.uRadiusTL, pc.uRadiusTR);
            vec2 e0 = pc.uRectSize - pc.uRectPos, e1 = p2 - pc.uRectSize, e2 = pc.uRectPos - p2;
            vec2 v0 = pixelPos - pc.uRectPos, v1 = pixelPos - pc.uRectSize, v2 = pixelPos - p2;
            vec2 pq0 = v0 - e0 * clamp(dot(v0, e0) / dot(e0, e0), 0.0, 1.0);
            vec2 pq1 = v1 - e1 * clamp(dot(v1, e1) / dot(e1, e1), 0.0, 1.0);
            vec2 pq2 = v2 - e2 * clamp(dot(v2, e2) / dot(e2, e2), 0.0, 1.0);
            float s = sign(e0.x * e2.y - e0.y * e2.x);
            vec2 d = min(min(vec2(dot(pq0, pq0), s * (v0.x * e0.y - v0.y * e0.x)),
                             vec2(dot(pq1, pq1), s * (v1.x * e1.y - v1.y * e1.x))),
                         vec2(dot(pq2, pq2), s * (v2.x * e2.y - v2.y * e2.x)));
            float dist = -sqrt(d.x) * sign(d.y);
            float alpha = clamp(0.5 - dist / aaWidth, 0.0, 1.0);
            frag = vec4(vColor.rgb, vColor.a * alpha);
        } else {
            float outerDist = sdfRoundedBox4(p, halfSize, pc.uRadiusTL, pc.uRadiusTR, pc.uRadiusBR, pc.uRadiusBL);
            vec2 innerHalf = halfSize - vec2(pc.uBorderThickness);
            float innerRadiusTL = max(0.0, pc.uRadiusTL - pc.uBorderThickness);
            float innerRadiusTR = max(0.0, pc.uRadiusTR - pc.uBorderThickness);
            float innerRadiusBR = max(0.0, pc.uRadiusBR - pc.uBorderThickness);
            float innerRadiusBL = max(0.0, pc.uRadiusBL - pc.uBorderThickness);
            float innerDist = sdfRoundedBox4(p, innerHalf, innerRadiusTL, innerRadiusTR, innerRadiusBR, innerRadiusBL);
            float outerAlpha = clamp(0.5 - outerDist / aaWidth, 0.0, 1.0);
            float innerAlpha = clamp(0.5 + innerDist / aaWidth, 0.0, 1.0);
            float alpha = outerAlpha * innerAlpha;
            frag = vec4(vColor.rgb, vColor.a * alpha);
        }
    }
}