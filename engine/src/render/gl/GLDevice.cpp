// AI生成
#include "GLDevice.h"
#include "../../core/Log.h"
#include <atomic>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// PNG 编码（截图落盘用；实现宏只在此编译单元定义，全工程唯一）
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>

namespace ui {

// ============================================================
// GL 调用序列追踪（UI_TRACE=1 时输出；直通/回放同格式，
// 两条路径日志逐行 diff 即可定位多线程渲染差异根因）
//   行首来源标记：D=直通（主线程立即执行）/ R=录制（主线程写包）/
//                P=回放（渲染线程执行包）
//   对比口径：single 的 D 流 vs mt 的 P 流（均为实际 GL 效果）；
//             mt 的 R 流 vs P 流（回放忠实性）。DRAW 行带顶点/索引
//             校验和（vcrc/icrc）——命令序列一致但像素有差异时，
//             差异必在顶点/UV 数据（字体偏差=顶点错位的直接证据）
//   UI_TRACE_FROM_FRAME=N : 跳过前 N 帧（等待动画稳定/窗口打开后再记录，
//                           避免 fps 差异导致的两边采样时段错位）
//   UI_TRACE_MAX=M        : 最多输出 M 行（默认 4000）
// ============================================================
static bool glTraceEnabled() {
    static const bool on = [] {
        const char* e = std::getenv("UI_TRACE");
        return e && e[0] == '1';
    }();
    return on;
}
static void glTraceSrc(char src, const char* fmt, ...) {
    static const bool on = [] {
        const char* e = std::getenv("UI_TRACE");
        return e && e[0] == '1';
    }();
    if (!on) return;
    static const int fromFrame = [] {
        const char* e = std::getenv("UI_TRACE_FROM_FRAME");
        return e ? std::atoi(e) : 0;
    }();
    static const int maxLines = [] {
        const char* e = std::getenv("UI_TRACE_MAX");
        return e && std::atoi(e) > 0 ? std::atoi(e) : 4000;
    }();
    // 帧号计数：以 BEGIN 命令为帧边界（三种来源均经过此处；
    // 同一帧录制(R)先于回放(P)计数，从帧数差异可分离两流）
    // 注：检测格式化后的 buf（包装转发后 fmt 前缀不可靠）
    static std::atomic<int> s_frame{ 0 };
    // 行数配额按来源分桶（D/R/P 各自 maxLines）：R 与 P 并发输出时
    // 共享配额会互相挤占截断（P 流缺帧 → 对比误报）
    static std::atomic<int> s_lines[3]{ {0}, {0}, {0} };
    char buf[256];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (buf[0] == 'B' && buf[1] == 'E' && buf[2] == 'G') {
        s_frame.fetch_add(1, std::memory_order_relaxed);
    }
    if (s_frame.load(std::memory_order_relaxed) < fromFrame) return;
    if (s_lines[src == 'D' ? 0 : (src == 'R' ? 1 : 2)]
            .fetch_add(1, std::memory_order_relaxed) > maxLines) return;
    UI_LOG_INFO("T %c %s", src, buf);
}
static void glTrace(const char* fmt, ...) {
    if (!glTraceEnabled()) return;   // 热路径零开销（不格式化）
    va_list ap; va_start(ap, fmt);
    char buf[256];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    glTraceSrc('D', "%s", buf);
}
static void glTraceR(const char* fmt, ...) {
    if (!glTraceEnabled()) return;
    va_list ap; va_start(ap, fmt);
    char buf[256];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    glTraceSrc('R', "%s", buf);
}
static void glTraceP(const char* fmt, ...) {
    if (!glTraceEnabled()) return;
    va_list ap; va_start(ap, fmt);
    char buf[256];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    glTraceSrc('P', "%s", buf);
}

// 像素校验和（trace 专用）：逐 4 字节按 uint32 累加——用于对比直通/
// 回放两侧上传的纹理内容是否逐位一致（TEX+ 命令行带 crc=）
static unsigned glTraceCrc(const uint8_t* p, size_t n) {
    unsigned s = 0;
    for (size_t i = 0; i + 4 <= n; i += 4)
        s += (unsigned)p[i] | ((unsigned)p[i + 1] << 8) |
             ((unsigned)p[i + 2] << 16) | ((unsigned)p[i + 3] << 24);
    return s;
}

static const char* kVS = R"(
#version 330 core
uniform vec2 uResolution;
in vec2 aPos;
in vec2 aUV;
in vec4 aColor;
out vec2 vUV;
out vec4 vColor;
void main(){
    vec2 ndc = vec2(aPos.x / uResolution.x * 2.0 - 1.0,
                   1.0 - aPos.y / uResolution.y * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
    vUV = aUV;
    vColor = aColor;
}
)";
static const char* kFS = R"(
#version 330 core
in vec2 vUV;
in vec4 vColor;
out vec4 frag;
uniform sampler2D uTex;
uniform vec2 uResolution;
uniform int  uSDFMode = 0;
uniform vec2 uRectPos;
uniform vec2 uRectSize;
uniform float uRadiusTL;
uniform float uRadiusTR;
uniform float uRadiusBR;
uniform float uRadiusBL;
uniform float uBorderThickness;
uniform float uAAOn = 1.0;

float sdfRoundedBox4(vec2 p, vec2 halfSize, float rTL, float rTR, float rBR, float rBL) {
    float r = (p.x < 0.0 && p.y < 0.0) ? rTL :
              (p.x > 0.0 && p.y < 0.0) ? rTR :
              (p.x > 0.0 && p.y > 0.0) ? rBR : rBL;
    vec2 q = abs(p) - halfSize + r;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

void main(){
    if (uSDFMode == 0) {
        frag = vColor * texture(uTex, vUV);
    } else {
        vec2 pixelPos = vec2(gl_FragCoord.x, uResolution.y - gl_FragCoord.y);
        vec2 center = uRectPos + uRectSize * 0.5;
        vec2 p = pixelPos - center;
        vec2 halfSize = uRectSize * 0.5;
        float aaWidth = mix(0.51, 2.0, clamp(uAAOn, 0.0, 1.0));

        if (uSDFMode == 1) {
            float dist = sdfRoundedBox4(p, halfSize, uRadiusTL, uRadiusTR, uRadiusBR, uRadiusBL);
            float alpha = clamp(0.5 - dist / aaWidth, 0.0, 1.0);
            frag = vec4(vColor.rgb, vColor.a * alpha);
        } else if (uSDFMode == 3) {
            float dist = sdfRoundedBox4(p, halfSize, uRadiusTL, uRadiusTR, uRadiusBR, uRadiusBL);
            float alpha = clamp(1.0 - max(dist, 0.0) / uBorderThickness, 0.0, 1.0);
            frag = vec4(vColor.rgb, vColor.a * alpha);
        } else if (uSDFMode == 4) {
            // 椭圆填充：内切于 uRectPos/uRectSize 包围矩形（halfSize = 半径）
            float dist = (length(p / halfSize) - 1.0) * min(halfSize.x, halfSize.y);
            float alpha = clamp(0.5 - dist / aaWidth, 0.0, 1.0);
            frag = vec4(vColor.rgb, vColor.a * alpha);
        } else if (uSDFMode == 5) {
            // 椭圆描边：uBorderThickness = 线宽
            float dist = abs((length(p / halfSize) - 1.0) * min(halfSize.x, halfSize.y))
                       - uBorderThickness * 0.5;
            float alpha = clamp(0.5 - dist / aaWidth, 0.0, 1.0);
            frag = vec4(vColor.rgb, vColor.a * alpha);
        } else if (uSDFMode == 6) {
            // 线段（平端点）：uRectPos=端点A uRectSize=端点B uBorderThickness=线宽
            vec2 pa = pixelPos - uRectPos;
            vec2 ba = uRectSize - uRectPos;
            float h = clamp(dot(pa, ba) / max(dot(ba, ba), 1e-6), 0.0, 1.0);
            float dist = length(pa - ba * h) - uBorderThickness * 0.5;
            float alpha = clamp(0.5 - dist / aaWidth, 0.0, 1.0);
            frag = vec4(vColor.rgb, vColor.a * alpha);
        } else if (uSDFMode == 7) {
            // 椭圆弧描边：uRadiusTR=起始角 uRadiusBR=扫过角（度，屏幕坐标顺时针）
            float dist = abs((length(p / halfSize) - 1.0) * min(halfSize.x, halfSize.y))
                       - uBorderThickness * 0.5;
            float aDeg = mod(degrees(atan(p.y, p.x)) - uRadiusTR + 720.0, 360.0);
            float alpha = clamp(0.5 - dist / aaWidth, 0.0, 1.0) * step(aDeg, uRadiusBR);
            frag = vec4(vColor.rgb, vColor.a * alpha);
        } else if (uSDFMode == 8) {
            // 三角形填充：p0=uRectPos p1=uRectSize p2=(uRadiusTL,uRadiusTR)
            vec2 p2 = vec2(uRadiusTL, uRadiusTR);
            vec2 e0 = uRectSize - uRectPos, e1 = p2 - uRectSize, e2 = uRectPos - p2;
            vec2 v0 = pixelPos - uRectPos, v1 = pixelPos - uRectSize, v2 = pixelPos - p2;
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
            float outerDist = sdfRoundedBox4(p, halfSize, uRadiusTL, uRadiusTR, uRadiusBR, uRadiusBL);
            vec2 innerHalf = halfSize - vec2(uBorderThickness);
            float innerRadiusTL = max(0.0, uRadiusTL - uBorderThickness);
            float innerRadiusTR = max(0.0, uRadiusTR - uBorderThickness);
            float innerRadiusBR = max(0.0, uRadiusBR - uBorderThickness);
            float innerRadiusBL = max(0.0, uRadiusBL - uBorderThickness);
            float innerDist = sdfRoundedBox4(p, innerHalf, innerRadiusTL, innerRadiusTR, innerRadiusBR, innerRadiusBL);
            float outerAlpha = clamp(0.5 - outerDist / aaWidth, 0.0, 1.0);
            float innerAlpha = clamp(0.5 + innerDist / aaWidth, 0.0, 1.0);
            float alpha = outerAlpha * innerAlpha;
            frag = vec4(vColor.rgb, vColor.a * alpha);
        }
    }
}
)";

static unsigned compile(unsigned type, const char* src) {
    unsigned s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    int ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024]; glGetShaderInfoLog(s, 1024, nullptr, log);
        UI_LOG_ERROR("shader compile failed: %s", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

GLDevice::GLDevice() = default;
GLDevice::~GLDevice() { shutdown(); }

bool GLDevice::init(void* nativeHandle, int w, int h) {
    native_ = nativeHandle;
    w_ = w; h_ = h;
    glfwMakeContextCurrent((GLFWwindow*)nativeHandle);
    // ---- 垂直同步默认关闭（不锁帧），UI_VSYNC=1 恢复锁定刷新率 ----
    // 旧默认 vsync=1 锁 60fps（vsync 即「锁帧代码」）。反转为默认 0：
    // 性能观测/极限吞吐不再受刷新率上限；需要稳定帧率时设 UI_VSYNC=1。
    { const char* e = std::getenv("UI_VSYNC"); glfwSwapInterval(e && e[0] == '1' ? 1 : 0); }
    int version = gladLoadGL(glfwGetProcAddress);
    if (!version) {
        UI_LOG_ERROR("gladLoadGL failed");
        return false;
    }
    UI_LOG_INFO("OpenGL %d.%d loaded", GLAD_VERSION_MAJOR(version),
                GLAD_VERSION_MINOR(version));

    if (!compileShaders()) return false;

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ibo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
    glBindVertexArray(0);

    // 1x1 white texture for solid-color draws
    uint8_t white[4] = {255, 255, 255, 255};
    glGenTextures(1, &whiteTex_);
    glBindTexture(GL_TEXTURE_2D, whiteTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_MULTISAMPLE);

    // 离屏 MSAA 样本数与窗口默认帧缓冲一致（视觉行为不变）；
    // 默认目标尺寸=物理尺寸（跟随窗口档；setTargetSize 后按需重建）
    glGetIntegerv(GL_SAMPLES, &msaaSamplesGL_);
    if (msaaSamplesGL_ < 0) msaaSamplesGL_ = 0;
    tw_ = w; th_ = h;
    targetDirty_ = true;
    ensureOffscreenMSAA();

    ready_ = true;
    return true;
}

bool GLDevice::compileShaders() {
    unsigned vs = compile(GL_VERTEX_SHADER, kVS);
    unsigned fs = compile(GL_FRAGMENT_SHADER, kFS);
    if (!vs || !fs) return false;
    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glBindAttribLocation(program_, 0, "aPos");
    glBindAttribLocation(program_, 1, "aUV");
    glBindAttribLocation(program_, 2, "aColor");
    glLinkProgram(program_);
    int ok = 0; glGetProgramiv(program_, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024]; glGetProgramInfoLog(program_, 1024, nullptr, log);
        UI_LOG_ERROR("program link failed: %s", log);
        return false;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    locResolution_ = glGetUniformLocation(program_, "uResolution");
    locTex_        = glGetUniformLocation(program_, "uTex");
    locSDFMode_    = glGetUniformLocation(program_, "uSDFMode");
    locRectPos_    = glGetUniformLocation(program_, "uRectPos");
    locRectSize_   = glGetUniformLocation(program_, "uRectSize");
    locRadiusTL_   = glGetUniformLocation(program_, "uRadiusTL");
    locRadiusTR_   = glGetUniformLocation(program_, "uRadiusTR");
    locRadiusBR_   = glGetUniformLocation(program_, "uRadiusBR");
    locRadiusBL_   = glGetUniformLocation(program_, "uRadiusBL");
    locBorderThk_  = glGetUniformLocation(program_, "uBorderThickness");
    locAAOn_       = glGetUniformLocation(program_, "uAAOn");
    return true;
}

void GLDevice::shutdown() {
    if (!ready_) return;
    flushBatch();                     // 待机批清空（纹理/缓冲即将销毁）
    for (auto& kv : textures_) glDeleteTextures(1, &kv.second);
    textures_.clear();
    batchV_.clear(); batchV_.shrink_to_fit();
    batchI_.clear(); batchI_.shrink_to_fit();
    if (whiteTex_) glDeleteTextures(1, &whiteTex_);
    if (resolveFbo_) glDeleteFramebuffers(1, &resolveFbo_);
    if (resolveTex_) glDeleteTextures(1, &resolveTex_);
    if (msaaFbo_) glDeleteFramebuffers(1, &msaaFbo_);
    if (msaaColorRB_) glDeleteRenderbuffers(1, &msaaColorRB_);
    resolveFbo_ = resolveTex_ = msaaFbo_ = msaaColorRB_ = 0;
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (ibo_) glDeleteBuffers(1, &ibo_);
    if (program_) glDeleteProgram(program_);
    ready_ = false;
}

// ---- 离屏 FBO 重建（tw_/th_ 或样本数变化时；调用方须持 GL 上下文）----
// MSAA renderbuffer 附件（样本数=窗口默认帧缓冲实际值，视觉一致）；
// 单样本 resolve 纹理 FBO 供非 1:1 放大路径使用（MSAA read fb 的 blit
// 规范要求 NEAREST，无法直接 LINEAR 放大——先 NEAREST resolve 再放大）。
void GLDevice::ensureOffscreenMSAA() {
    if (!targetDirty_) return;
    targetDirty_ = false;
    if (tw_ <= 0 || th_ <= 0) return;
    // 先销毁旧资源再建新（尺寸/样本变化）
    if (resolveFbo_) glDeleteFramebuffers(1, &resolveFbo_);
    if (resolveTex_) glDeleteTextures(1, &resolveTex_);
    if (msaaFbo_) glDeleteFramebuffers(1, &msaaFbo_);
    if (msaaColorRB_) glDeleteRenderbuffers(1, &msaaColorRB_);

    glGenRenderbuffers(1, &msaaColorRB_);
    glBindRenderbuffer(GL_RENDERBUFFER, msaaColorRB_);
    if (msaaSamplesGL_ > 0)
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaaSamplesGL_,
                                         GL_RGBA8, tw_, th_);
    else
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, tw_, th_);
    glGenFramebuffers(1, &msaaFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, msaaFbo_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER, msaaColorRB_);
    // resolve 纹理（LINEAR 过滤，非 1:1 放大路径的中间站）
    glGenTextures(1, &resolveTex_);
    glBindTexture(GL_TEXTURE_2D, resolveTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tw_, th_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &resolveFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, resolveFbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, resolveTex_, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, msaaFbo_);   // 恢复渲染目标
    glTrace("OFFSCREEN %dx%d samples=%d", tw_, th_, msaaSamplesGL_);
}

// ---- 离屏画面 → 默认帧缓冲（present 前调用）----
// 1:1（跟随窗口）：MSAA FBO → 默认 fb，NEAREST（含 resolve，逐位一致）；
// 非 1:1（强制分辨率档）：先 NEAREST resolve 到单样本 FBO，再 LINEAR
// 放大到窗口（两步均规范合法）。
void GLDevice::blitToScreen() {
    if (tw_ <= 0 || th_ <= 0 || w_ <= 0 || h_ <= 0) return;
    const bool same = (tw_ == w_ && th_ == h_);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, msaaFbo_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    if (same) {
        glBlitFramebuffer(0, 0, tw_, th_, 0, 0, w_, h_,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);
    } else {
        // 第一步：MSAA resolve（规范要求 NEAREST）
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveFbo_);
        glBlitFramebuffer(0, 0, tw_, th_, 0, 0, tw_, th_,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);
        // 第二步：单样本纹理 LINEAR 放大到窗口
        glBindFramebuffer(GL_READ_FRAMEBUFFER, resolveFbo_);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, tw_, th_, 0, 0, w_, h_,
                          GL_COLOR_BUFFER_BIT, GL_LINEAR);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);   // 归位（截图读默认 fb）
}

void GLDevice::resize(int w, int h) { w_ = w; h_ = h; }

void GLDevice::beginFrame() {
    if (!ready_) return;
    flushBatch();                     // 帧级防御：上帧残留批不得跨帧（上帧 endFrame 已收尾，此处兜底）
    ensureOffscreenMSAA();            // 离屏目标按需重建（尺寸变化时）
    if (msaaFbo_) glBindFramebuffer(GL_FRAMEBUFFER, msaaFbo_);
    if (rec_) {                       // 录制模式：清屏/视口复位命令化（回放线程执行）
        scissorStack_.clear();        // 裁剪栈录制侧维护（帧级防御同直通）
        FramePacket::Cmd c{}; c.kind = FramePacket::Cmd::BeginFrame;
        c.w = tw_; c.h = th_;         // 视口=渲染目标逻辑尺寸（回放侧绑 FBO）
        rec_->cmds.push_back(c);
        glTraceR("BEGIN %d %d", tw_, th_);
        return;
    }
    glViewport(0, 0, tw_, th_);
    scissorStack_.clear();            // 帧级防御：裁剪栈失衡不跨帧累积
    glDisable(GL_SCISSOR_TEST);
    glTrace("BEGIN %d %d", tw_, th_);
}

void GLDevice::endFrame() {
    flushBatch();                     // 帧尾收尾：最后一批必须画出来（present 前）
    if (rec_) {
        FramePacket::Cmd c{}; c.kind = FramePacket::Cmd::EndFrame;
        rec_->cmds.push_back(c);
    }
}

void GLDevice::present() {
    flushBatch();                     // 三重保险（endFrame 已 flush，异常路径兜底）
    if (rec_) {
        FramePacket::Cmd c{}; c.kind = FramePacket::Cmd::Present;
        rec_->cmds.push_back(c);
        glTraceR("SWAP");
        return;
    }
    blitToScreen();                   // 离屏画面 → 默认帧缓冲（resolve + 缩放）
    captureSnapshotGL();              // swap 前读帧缓冲（blit 后=最终画面）
    if (native_) glfwSwapBuffers((GLFWwindow*)native_);
    glTrace("SWAP");
}

// ============================================================
// 窗口截图（引擎内拍照，迁移自 999 引擎 AutoScreenshotModule）
// 直读帧缓冲：无任务栏/光标/遮挡/DPI 噪声，最小化也能拍。
// 请求-消费模式：requestScreenshot 任意线程置标记（tag 先写后
// store-release），present（直通=主线程 / 回放=渲染线程 Present
// 命令）swap 前 load-acquire 消费——同一帧数据同一时序，直通/
// 回放两路径截图语义完全一致。
// ============================================================
void GLDevice::requestScreenshot(const char* tag) {
    if (!tag || !tag[0]) tag = "default";
    std::strncpy(shotTag_, tag, sizeof(shotTag_) - 1);   // 先写 tag
    shotTag_[sizeof(shotTag_) - 1] = 0;
    shotReq_.store(true, std::memory_order_release);     // 后置请求（release 保 tag 可见）
}

void GLDevice::captureSnapshotGL() {
    // exchange 原子取走请求（无请求时零开销一次 atomic load）
    if (!shotReq_.exchange(false, std::memory_order_acquire)) return;
    if (!ready_ || w_ <= 0 || h_ <= 0) return;
    // GL_RGB 3 字节/像素 + PACK_ALIGNMENT 1（行宽任意时也不补位）
    std::vector<uint8_t> px((size_t)w_ * h_ * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w_, h_, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    // GL 原点在左下 → 逐行上下翻转成 PNG 顶部左上序
    {
        std::vector<uint8_t> row((size_t)w_ * 3);
        for (int y = 0; y < h_ / 2; ++y) {
            uint8_t* a = px.data() + (size_t)y * w_ * 3;
            uint8_t* b = px.data() + (size_t)(h_ - 1 - y) * w_ * 3;
            std::memcpy(row.data(), a, row.size());
            std::memcpy(a, b, row.size());
            std::memcpy(b, row.data(), row.size());
        }
    }
    writeScreenshotPNG(shotTag_, px.data(), w_, h_);
}

// 截图落盘公共函数（GL/VK 共用；PNG 编码实现唯一在本编译单元）
bool RenderDevice::writeScreenshotPNG(const char* tag, const uint8_t* rgb, int w, int h) {
    std::string fn = std::string("shot-") + tag + ".png";
    if (stbi_write_png(fn.c_str(), w, h, 3, rgb, w * 3)) {
        UI_LOG_INFO("screenshot saved: %s (%dx%d)", fn.c_str(), w, h);
        return true;
    }
    UI_LOG_ERROR("screenshot failed: %s", fn.c_str());
    return false;
}

void GLDevice::clear(const Color& c) {
    flushBatch();                     // 清屏前必须画完已累积批（否则被清掉）
    if (rec_) {
        FramePacket::Cmd k{}; k.kind = FramePacket::Cmd::Clear;
        k.cr = c.r; k.cg = c.g; k.cb = c.b; k.ca = c.a;
        rec_->cmds.push_back(k);
        glTraceR("CLEAR %.3f %.3f %.3f %.3f", c.r, c.g, c.b, c.a);
        return;
    }
    glClearColor(c.r, c.g, c.b, c.a);
    glClear(GL_COLOR_BUFFER_BIT);
    glTrace("CLEAR %.3f %.3f %.3f %.3f", c.r, c.g, c.b, c.a);
}

void GLDevice::setViewport(int x, int y, int w, int h) {
    flushBatch();                     // 视口变化拆批（批内视口必须恒定）
    if (rec_) {
        FramePacket::Cmd c{}; c.kind = FramePacket::Cmd::Viewport;
        c.x = x; c.y = y; c.w = w; c.h = h;
        rec_->cmds.push_back(c);
        glTraceR("VIEW %d %d %d %d", x, y, w, h);
        return;
    }
    glViewport(x, y, w, h);
    glTrace("VIEW %d %d %d %d", x, y, w, h);
}

void GLDevice::setScissorRect(int x, int y, int w, int h) {
    flushBatch();                     // 裁剪状态变化拆批（GL 全局状态，批内必须恒定）
    if (rec_) {
        // 变换在录制侧完成（GL 帧缓冲左下原点 Y 翻转；离屏 1:1 无缩放），
        // 回放器直接 glScissor——两路径参数逐位一致
        FramePacket::Cmd c{}; c.kind = FramePacket::Cmd::SetScissor;
        c.x = x; c.y = th_ - y - h;
        c.w = w; c.h = h;
        rec_->cmds.push_back(c);
        glTraceR("SCIS %d %d %d %d", x, th_ - y - h, w, h);
        return;
    }
    // GL 帧缓冲原点在左下，y 翻转（基准=渲染目标逻辑高 th_）
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, th_ - y - h, w, h);
    glTrace("SCIS %d %d %d %d", x, th_ - y - h, w, h);
}

void GLDevice::resetScissorRect() {
    flushBatch();
    if (rec_) {
        FramePacket::Cmd c{}; c.kind = FramePacket::Cmd::ResetScissor;
        rec_->cmds.push_back(c);
        glTraceR("SCIS OFF");
        return;
    }
    glDisable(GL_SCISSOR_TEST);
    glTrace("SCIS OFF");
}

void GLDevice::pushScissorRect(int x, int y, int w, int h) {
    flushBatch();
    // 嵌套裁剪求交（系统层）：glScissor 是"置位"而非"求交"，若不手动相交，
    // 嵌套区域（如页面内嵌滚动容器）会覆盖外层裁剪、把内容画到外层可视区之外。
    // 与栈顶取交集后再入栈，保证嵌套区域永远不越出父区域。
    if (!scissorStack_.empty()) {
        const int px = scissorStack_.back()[0], py = scissorStack_.back()[1];
        const int pw = scissorStack_.back()[2], ph = scissorStack_.back()[3];
        int x1 = x > px ? x : px;
        int y1 = y > py ? y : py;
        int x2 = (x + w) < (px + pw) ? (x + w) : (px + pw);
        int y2 = (y + h) < (py + ph) ? (y + h) : (py + ph);
        if (x2 < x1) x2 = x1;
        if (y2 < y1) y2 = y1;
        x = x1; y = y1; w = x2 - x1; h = y2 - y1;
    }
    scissorStack_.push_back({x, y, w, h});   // 栈两模式共用（录制侧维护）
    setScissorRect(x, y, w, h);              // 录制模式内部展开为 SetScissor 命令
}

void GLDevice::popScissorRect() {
    flushBatch();
    if (!scissorStack_.empty()) scissorStack_.pop_back();
    if (scissorStack_.empty()) {
        resetScissorRect();                  // 录制模式展开为 ResetScissor 命令
    } else {
        const auto& t = scissorStack_.back();
        setScissorRect(t[0], t[1], t[2], t[3]);
    }
}

uint32_t GLDevice::createTexture(int w, int h, const uint8_t* rgba) {
    if (rec_) {
        // 录制模式：像素拷入包池 + CreateTex 命令；逻辑 id 由计算线程
        // 分配（nextId_ 仅计算线程写），GL 纹理对象回放时建立映射
        uint32_t id = nextId_++;
        size_t n = (size_t)w * h * 4;
        FramePacket::Cmd c{}; c.kind = FramePacket::Cmd::CreateTex;
        c.texId = id; c.texW = w; c.texH = h;
        c.pixOff = (uint32_t)rec_->pixels.size();
        c.pixLen = (uint32_t)n;
        rec_->pixels.insert(rec_->pixels.end(), rgba, rgba + n);
        rec_->cmds.push_back(c);
        if (glTraceEnabled())
            glTraceR("TEX+ %u %d %d crc=%08x %02x%02x%02x%02x", id, w, h,
                     glTraceCrc(rgba, n), rgba[0], rgba[1], rgba[2], rgba[3]);
        return id;
    }
    unsigned t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    uint32_t id = nextId_++;
    textures_[id] = t;
    if (glTraceEnabled()) {
        size_t n = (size_t)w * h * 4;
        glTrace("TEX+ %u %d %d crc=%08x %02x%02x%02x%02x", id, w, h,
                glTraceCrc(rgba, n), rgba[0], rgba[1], rgba[2], rgba[3]);
    }
    return id;
}

void GLDevice::destroyTexture(uint32_t id) {
    flushBatch();                     // 批内可能仍引用该纹理（帧内重建图集），先画完再销毁
    if (rec_) {
        FramePacket::Cmd c{}; c.kind = FramePacket::Cmd::DestroyTex;
        c.texId = id;
        rec_->cmds.push_back(c);
        glTraceR("TEX- %u", id);
        return;
    }
    auto it = textures_.find(id);
    if (it == textures_.end()) {
        glTrace("TEX-! %u", id);     // 映射缺失：销毁不存在的纹理（异常信号）
        return;
    }
    glTrace("TEX- %u", id);
    glDeleteTextures(1, &it->second);
    textures_.erase(it);
}

void GLDevice::setSDFParams(int mode, float x, float y, float w, float h,
                            float rTL, float rTR, float rBR, float rBL, float thickness) {
    sdfMode_ = mode;
    sdfX_ = x; sdfY_ = y; sdfW_ = w; sdfH_ = h;
    sdfRadiusTL_ = rTL; sdfRadiusTR_ = rTR; sdfRadiusBR_ = rBR; sdfRadiusBL_ = rBL;
    sdfThickness_ = thickness;
}

void GLDevice::setAAEnabled(bool on) { aaOn_ = on ? 1.0f : 0.0f; }

// 渲染目标逻辑尺寸（纯数据写入：跨线程安全，FBO 重建延迟到帧路径）
void GLDevice::setTargetSize(int w, int h) {
    if (w <= 0 || h <= 0 || (w == tw_ && h == th_)) return;
    tw_ = w; th_ = h;
    targetDirty_ = true;              // 持上下文线程消费（beginFrame/replay）
}

// ============================================================
// 帧内合批（性能优化，渲染效果逐位一致）
//
// 旧路径：每个 UI 元素一次 drawTriangles = 2×glBufferData（驱动重分配）
// + 全套 uniform + draw call；数百元素/帧 → 数千次 GL 调用/帧。
// 新路径：SDFMode=0（纯色/纹理，无 per-draw uniform）的连续同纹理
// 绘制追加进批缓冲，状态变化（纹理切换/SDF 绘制/裁剪/视口/清屏）
// 时 flush 一次上传 + 一次 draw call。文字（字体图集）是最大受益者。
// 批缓冲 clear 保容量 → 跨帧复用零再分配。
// ============================================================
void GLDevice::flushBatch() {
    if (batchI_.empty()) { batchValid_ = false; return; }
    if (rec_) {
        // 录制模式：批数据追加进帧包 + Draw 命令（合批语义完整保留——
        // 一批=回放一次上传+一次 draw call，与直通路径调用序列一致）
        FramePacket::Cmd c{}; c.kind = FramePacket::Cmd::Draw;
        c.vOff = (uint32_t)rec_->verts.size();
        c.vCnt = (uint32_t)batchV_.size();
        rec_->verts.insert(rec_->verts.end(), batchV_.begin(), batchV_.end());
        c.iOff = (uint32_t)rec_->indices.size();
        c.iCnt = (uint32_t)batchI_.size();
        rec_->indices.insert(rec_->indices.end(), batchI_.begin(), batchI_.end());
        c.tex = batchTex_; c.textured = (batchTex_ != 0) ? 1 : 0;
        c.sdfMode = 0;
        c.rw = (float)tw_; c.rh = (float)th_;   // NDC 基准=渲染目标逻辑尺寸
        rec_->cmds.push_back(c);
        glTraceR("DRAW tex=%u icnt=%zu vcrc=%08x icrc=%08x sdf=0",
                 batchTex_, batchI_.size(),
                 glTraceCrc((const uint8_t*)batchV_.data(),
                            batchV_.size() * sizeof(Vertex)),
                 glTraceCrc((const uint8_t*)batchI_.data(),
                            batchI_.size() * sizeof(uint32_t)));
        batchV_.clear();
        batchI_.clear();
        batchValid_ = false;
        return;
    }
    glUseProgram(program_);
    glUniform2f(locResolution_, float(tw_), float(th_));   // NDC 基准=离屏逻辑尺寸
    glUniform1i(locSDFMode_, 0);      // 批内恒为非 SDF 模式
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, batchV_.size() * sizeof(Vertex),
                 batchV_.data(), GL_STREAM_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, batchI_.size() * sizeof(uint32_t),
                 batchI_.data(), GL_STREAM_DRAW);

    unsigned tex = whiteTex_;
    if (batchTex_) {
        auto it = textures_.find(batchTex_);
        if (it != textures_.end()) tex = it->second;
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i(locTex_, 0);

    glDrawElements(GL_TRIANGLES, (GLsizei)batchI_.size(), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
    glTrace("DRAW tex=%u icnt=%zu vcrc=%08x icrc=%08x sdf=0",
            batchTex_, batchI_.size(),
            glTraceCrc((const uint8_t*)batchV_.data(),
                       batchV_.size() * sizeof(Vertex)),
            glTraceCrc((const uint8_t*)batchI_.data(),
                       batchI_.size() * sizeof(uint32_t)));
    batchV_.clear();
    batchI_.clear();
    batchValid_ = false;
}

void GLDevice::drawTriangles(const Vertex* verts, int vcount,
                             const uint32_t* indices, int icount,
                             uint32_t textureId, bool textured) {
    if (!ready_ || icount <= 0) return;

    // ---- 快路径：非 SDF 绘制进批（文字/纯色/图片，占绝对 majority）----
    if (sdfMode_ == 0) {

        uint32_t tex = (textured && textureId) ? textureId : 0;
        if (batchValid_ && batchTex_ != tex) flushBatch();   // 纹理切换拆批
        uint32_t base = (uint32_t)batchV_.size();
        // 离屏 1:1 光栅化：顶点=逻辑坐标，零缩放直拷
        batchV_.insert(batchV_.end(), verts, verts + vcount);
        batchI_.reserve(batchI_.size() + (size_t)icount);
        for (int i = 0; i < icount; ++i) batchI_.push_back(indices[i] + base);
        batchTex_ = tex;
        batchValid_ = true;
        // 批容量上限：防单批无限膨胀（驱动单次上传压力），超限先画
        if (batchV_.size() >= 65536) flushBatch();
        return;
    }

    // ---- SDF 慢路径：rect/radius 等为 per-draw uniform，无法合批 ----
    flushBatch();                     // 保序：SDF 绘制前画完已累积批
    // 离屏 1:1：SDF quad 顶点=逻辑坐标，零缩放直用
    if (rec_) {
        // 录制模式：SDF 参数随 Draw 命令走，回放线程发 uniform——
        // 与直通路径相同的 uniform 序列与值（1:1 无缩放，原值快照）
        FramePacket::Cmd c{}; c.kind = FramePacket::Cmd::Draw;
        c.vOff = (uint32_t)rec_->verts.size();
        c.vCnt = (uint32_t)vcount;
        rec_->verts.insert(rec_->verts.end(), verts, verts + vcount);
        c.iOff = (uint32_t)rec_->indices.size();
        c.iCnt = (uint32_t)icount;
        rec_->indices.insert(rec_->indices.end(), indices, indices + icount);
        uint32_t tid = (textured && textureId) ? textureId : 0;
        c.tex = tid; c.textured = tid ? 1 : 0;
        c.sdfMode = sdfMode_;
        c.aa = aaOn_;
        c.rw = (float)tw_; c.rh = (float)th_;
        c.sx0 = sdfX_; c.sy0 = sdfY_;
        c.sw0 = sdfW_; c.sh0 = sdfH_;
        c.rtl = sdfRadiusTL_; c.rtr = sdfRadiusTR_;
        c.rbr = sdfRadiusBR_; c.rbl = sdfRadiusBL_;
        c.thk = sdfThickness_;
        rec_->cmds.push_back(c);
        glTraceR("DRAW tex=%u icnt=%d vcrc=%08x icrc=%08x sdf=%d rect=%.1f,%.1f %.1fx%.1f r=%.1f",
                 (textured && textureId) ? textureId : 0u, icount,
                 glTraceCrc((const uint8_t*)verts, (size_t)vcount * sizeof(Vertex)),
                 glTraceCrc((const uint8_t*)indices, (size_t)icount * sizeof(uint32_t)),
                 sdfMode_, c.sx0, c.sy0, c.sw0, c.sh0, c.rtl);
        return;
    }
    glUseProgram(program_);
    glUniform2f(locResolution_, float(tw_), float(th_));
    glUniform1i(locSDFMode_, sdfMode_);
    if (locAAOn_ >= 0) glUniform1f(locAAOn_, aaOn_);
    if (sdfMode_ != 0 && locRectPos_ >= 0) {
        // SDF 参数原值下发（gl_FragCoord 是离屏目标像素，1:1 对齐逻辑坐标）
        glUniform2f(locRectPos_, sdfX_, sdfY_);
        glUniform2f(locRectSize_, sdfW_, sdfH_);
        glUniform1f(locRadiusTL_, sdfRadiusTL_);
        glUniform1f(locRadiusTR_, sdfRadiusTR_);
        glUniform1f(locRadiusBR_, sdfRadiusBR_);
        glUniform1f(locRadiusBL_, sdfRadiusBL_);
        glUniform1f(locBorderThk_, sdfThickness_);
    }

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, vcount * sizeof(Vertex), verts, GL_STREAM_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, icount * sizeof(uint32_t), indices, GL_STREAM_DRAW);

    unsigned tex = whiteTex_;
    if (textured && textureId) {
        auto it = textures_.find(textureId);
        if (it != textures_.end()) tex = it->second;
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i(locTex_, 0);

    glDrawElements(GL_TRIANGLES, icount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
    glTrace("DRAW tex=%u icnt=%d vcrc=%08x icrc=%08x sdf=%d rect=%.1f,%.1f %.1fx%.1f r=%.1f",
            (textured && textureId) ? textureId : 0u, icount,
            glTraceCrc((const uint8_t*)verts, (size_t)vcount * sizeof(Vertex)),
            glTraceCrc((const uint8_t*)indices, (size_t)icount * sizeof(uint32_t)),
            sdfMode_, sdfX_, sdfY_, sdfW_, sdfH_, sdfRadiusTL_);
}

// ============================================================
// 帧包回放（渲染线程执行）
//
// 零状态纯解释器：命令流里所有参数在录制侧已变换完成（顶点缩放/
// SDF 乘法/裁剪翻转），回放只做 GL 调用——与单线程直通路径产生
// 完全相同的调用序列 → 渲染效果逐位一致。
// 上下文归属：回放线程独占 GL 上下文（粘性绑定，见下方
// g_glBound 注释），计算线程在录制模式下不触碰任何 GL 调用。
// ============================================================
namespace {
// 渲染线程粘性上下文绑定（性能修复：多线程模式零收益的根因）
// 旧实现：replay 每包 makeCurrent(win) → 回放 → makeCurrent(null)。
// Windows/WGL 跨线程上下文迁移有可观测驱动开销（全局锁+状态同步），
// 逐包迁移两次/帧恰好吃掉双线程并行的全部收益（稳态 acquire 反压
// → 流水线退化为端到端串行）。
// 新策略：thread_local 记录本线程已绑定窗口，同窗连续包零迁移；
// 解绑只发生在 RenderThread 挂起/退出前的 releaseThreadContext()
// （pause 协议握手保证主线程接管时无「上下文 current 在挂起线程」
// 死角）。thread_local 与主线程隔离：直通路径每帧 makeCurrent 不受
// 影响；多设备（多应用）连续包切换窗口时自动迁移（跨上下文本就无法省略）。
thread_local GLFWwindow* g_glBound = nullptr;   // 本线程当前绑定窗口（粘性）
}

void GLDevice::releaseThreadContext() {
    if (g_glBound) {
        glfwMakeContextCurrent(nullptr);
        g_glBound = nullptr;
    }
}

void GLDevice::replay(const FramePacket& p) {
    if (!ready_ || !native_) return;
    GLFWwindow* win = (GLFWwindow*)native_;
    if (g_glBound != win) {              // 粘性绑定：同窗连续包零迁移
        glfwMakeContextCurrent(win);
        g_glBound = win;
    }
    for (const auto& c : p.cmds) {
        switch (c.kind) {
        case FramePacket::Cmd::BeginFrame:
            ensureOffscreenMSAA();          // 离屏目标按需重建（渲染线程持上下文）
            if (msaaFbo_) glBindFramebuffer(GL_FRAMEBUFFER, msaaFbo_);
            glViewport(0, 0, c.w, c.h);     // c.w/c.h=录制侧逻辑尺寸快照
            glDisable(GL_SCISSOR_TEST);
            glTraceP("BEGIN %d %d", c.w, c.h);
            break;
        case FramePacket::Cmd::EndFrame:
            break;   // 合批在录制侧完成，无尾批残留
        case FramePacket::Cmd::Present:
            blitToScreen();                 // 离屏 → 默认帧缓冲（resolve + 缩放）
            captureSnapshotGL();            // 渲染线程消费截图请求（blit 后=最终画面）
            glfwSwapBuffers(win);
            glTraceP("SWAP");
            break;
        case FramePacket::Cmd::Clear:
            glClearColor(c.cr, c.cg, c.cb, c.ca);
            glClear(GL_COLOR_BUFFER_BIT);
            glTraceP("CLEAR %.3f %.3f %.3f %.3f", c.cr, c.cg, c.cb, c.ca);
            break;
        case FramePacket::Cmd::Viewport:
            glViewport(c.x, c.y, c.w, c.h);
            glTraceP("VIEW %d %d %d %d", c.x, c.y, c.w, c.h);
            break;
        case FramePacket::Cmd::SetScissor:
            glEnable(GL_SCISSOR_TEST);
            glScissor(c.x, c.y, c.w, c.h);
            glTraceP("SCIS %d %d %d %d", c.x, c.y, c.w, c.h);
            break;
        case FramePacket::Cmd::ResetScissor:
            glDisable(GL_SCISSOR_TEST);
            glTraceP("SCIS OFF");
            break;
        case FramePacket::Cmd::CreateTex: {
            unsigned t = 0;
            glGenTextures(1, &t);
            glBindTexture(GL_TEXTURE_2D, t);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, c.texW, c.texH, 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, p.pixels.data() + c.pixOff);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            textures_[c.texId] = t;      // 逻辑 id → GL 对象映射（回放侧维护）
            if (glTraceEnabled()) {
                const uint8_t* px = p.pixels.data() + c.pixOff;
                glTraceP("TEX+ %u %d %d crc=%08x %02x%02x%02x%02x", c.texId, c.texW, c.texH,
                         glTraceCrc(px, c.pixLen), px[0], px[1], px[2], px[3]);
            }
            break;
        }
        case FramePacket::Cmd::DestroyTex: {
            auto it = textures_.find(c.texId);
            if (it != textures_.end()) {
                glTraceP("TEX- %u", c.texId);
                glDeleteTextures(1, &it->second);
                textures_.erase(it);
            } else {
                glTraceP("TEX-! %u", c.texId);
                // 诊断探针：销毁不存在的映射（trace 关闭时也可见）——
                // 直通侧同场景打 TEX-! 时为对称噪声，仅 MT 侧出现则
                // 说明包序错乱（与 draw unmapped 同源）
                UI_LOG_WARN("replay: destroy unmapped tex %u", c.texId);
            }
            break;
        }
        case FramePacket::Cmd::Draw: {
            glUseProgram(program_);
            glUniform2f(locResolution_, c.rw, c.rh);
            glUniform1i(locSDFMode_, c.sdfMode);
            if (c.sdfMode != 0) {
                // SDF per-draw uniform（录制侧已乘缩放，按模式语义原样下发）
                if (locAAOn_ >= 0) glUniform1f(locAAOn_, c.aa);
                if (locRectPos_ >= 0) {
                    glUniform2f(locRectPos_, c.sx0, c.sy0);
                    glUniform2f(locRectSize_, c.sw0, c.sh0);
                    glUniform1f(locRadiusTL_, c.rtl);
                    glUniform1f(locRadiusTR_, c.rtr);
                    glUniform1f(locRadiusBR_, c.rbr);
                    glUniform1f(locRadiusBL_, c.rbl);
                    glUniform1f(locBorderThk_, c.thk);
                }
            }
            glBindVertexArray(vao_);
            glBindBuffer(GL_ARRAY_BUFFER, vbo_);
            glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(c.vCnt * sizeof(Vertex)),
                         p.verts.data() + c.vOff, GL_STREAM_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(c.iCnt * sizeof(uint32_t)),
                         p.indices.data() + c.iOff, GL_STREAM_DRAW);
            unsigned tex = whiteTex_;
            if (c.textured) {
                auto it = textures_.find(c.tex);
                if (it != textures_.end()) {
                    tex = it->second;
                } else {
                    // 诊断探针（字体偏差 bug 定位）：Draw 引用了映射中不存在
                    // 的纹理 id → 静默回退白纹理（症状=文字消失/图案变白）。
                    // 正常帧包顺序保证 CreateTex 先于引用它的 Draw 入包，命中
                    // 此分支说明纹理命令与绘制命令跨包错序或包被丢弃
                    UI_LOG_WARN("replay: draw unmapped tex %u (sdf=%d icnt=%u)",
                                c.tex, c.sdfMode, c.iCnt);
                }
            }
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, tex);
            glUniform1i(locTex_, 0);
            glDrawElements(GL_TRIANGLES, (GLsizei)c.iCnt, GL_UNSIGNED_INT, nullptr);
            glBindVertexArray(0);
            // 格式与录制/直通对称：sdf=0（批绘制）无 rect 段——逐行
            // diff 才不被格式伪差异干扰（vcrc/icrc 才是比较主体）
            if (c.sdfMode != 0)
                glTraceP("DRAW tex=%u icnt=%u vcrc=%08x icrc=%08x sdf=%d rect=%.1f,%.1f %.1fx%.1f r=%.1f",
                         c.tex, c.iCnt,
                         glTraceCrc((const uint8_t*)(p.verts.data() + c.vOff),
                                    (size_t)c.vCnt * sizeof(Vertex)),
                         glTraceCrc((const uint8_t*)(p.indices.data() + c.iOff),
                                    (size_t)c.iCnt * sizeof(uint32_t)),
                         c.sdfMode, c.sx0, c.sy0, c.sw0, c.sh0, c.rtl);
            else
                glTraceP("DRAW tex=%u icnt=%u vcrc=%08x icrc=%08x sdf=0",
                         c.tex, c.iCnt,
                         glTraceCrc((const uint8_t*)(p.verts.data() + c.vOff),
                                    (size_t)c.vCnt * sizeof(Vertex)),
                         glTraceCrc((const uint8_t*)(p.indices.data() + c.iOff),
                                    (size_t)c.iCnt * sizeof(uint32_t)));
            break;
        }
        }
    }
    // 上下文不在此归还：粘性绑定持续到 RenderThread 挂起/退出前的
    // releaseThreadContext()（pause 协议负责）——逐包解绑的跨线程
    // 迁移开销会使多线程模式零收益（见文件头粘性绑定注释）
}

} // namespace ui