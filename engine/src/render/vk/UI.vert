#version 450
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;
layout(location = 0) out vec2 vUV;
layout(location = 1) out vec4 vColor;
layout(push_constant) uniform PC { vec2 uResolution; } pc;
void main() {
    vec2 ndc = vec2(aPos.x / pc.uResolution.x * 2.0 - 1.0,
                    aPos.y / pc.uResolution.y * 2.0 - 1.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
    vUV = aUV;
    vColor = aColor;
}