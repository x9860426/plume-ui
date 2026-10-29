#pragma once
#include "RenderTypes.h"
#include <cstdint>
#include <vector>

namespace ui {

// ============================================================
// 跨线程帧包（多线程渲染架构核心数据结构）
//
// 架构：计算线程（主线程）录制 → 渲染线程回放。
//   计算线程：事件/输入/UI 逻辑/几何生成——所有状态变换（顶点缩放、
//             SDF 参数乘法、裁剪矩形翻转/缩放）在录制时完成；
//   渲染线程：按命令流原样执行 GL/VK 调用——零状态纯解释器。
//
// 逐位一致保证：录制顺序 = 命令顺序 = 回放顺序；单线程直通路径与
// 录制回放路径产生完全相同的 GL/VK 调用序列与参数。
//
// 线程安全：packet 同一时刻只归属一个线程（计算线程录制中 / 队列
// 等待 / 渲染线程回放中 / 池中空闲），由 RenderThread 的锁保证。
// ============================================================
struct FramePacket {
    // ---- GL 命令流（录制侧已完成全部状态变换，回放器直接执行）----
    struct Cmd {
        enum Kind : uint8_t {
            BeginFrame,     // w/h = 帧缓冲尺寸（glViewport 复位 + 裁剪复位）
            EndFrame,       // GL 侧无操作（合批在录制侧完成，无残留）
            Present,        // glfwSwapBuffers
            Clear,          // cr/cg/cb/ca 清屏色
            Viewport,       // x/y/w/h（直通值，与现状一致不缩放）
            SetScissor,     // x/y/w/h = 已变换 GL 值（乘缩放 + Y 翻转后）
            ResetScissor,   // 禁用裁剪
            Draw,           // 几何切片 + 纹理 + SDF 参数（全部已变换）
            CreateTex,      // texId/texW/texH + pixels 切片 [pixOff, pixOff+pixLen)
            DestroyTex,     // texId（按录制序执行，帧内重建图集安全）
        } kind;
        float cr = 0, cg = 0, cb = 0, ca = 1;          // Clear
        int   x = 0, y = 0, w = 0, h = 0;              // Viewport/SetScissor/BeginFrame
        uint32_t vOff = 0, vCnt = 0;                   // Draw 顶点切片
        uint32_t iOff = 0, iCnt = 0;                   // Draw 索引切片
        uint32_t tex = 0; uint8_t textured = 0;        // Draw 逻辑纹理 id
        int   sdfMode = 0;                             // Draw SDF 模式（0=普通合批）
        float sx0 = 0, sy0 = 0, sw0 = 0, sh0 = 0;      // SDF rect（已乘缩放）
        float rtl = 0, rtr = 0, rbr = 0, rbl = 0;      // 圆角（按模式语义已乘）
        float thk = 0, aa = 1;                         // 边框厚度（已乘）/ AA 开关
        float rw = 0, rh = 0;                          // resolution 快照（录制侧 w_/h_）
        uint32_t texId = 0; int texW = 0, texH = 0;    // CreateTex
        uint32_t pixOff = 0, pixLen = 0;               // CreateTex 像素池切片
    };
    std::vector<Cmd> cmds;

    // ---- VK 载荷（VKDevice 原生命令结构；录制侧只填纯数据，不查
    //      回放侧的 textures_ 映射——dset 查找挪到回放时按 texId 进行）----
    struct VKCmd {
        uint32_t texId = 0;        // 逻辑纹理（0 = 白纹理）
        uint8_t  textured = 0;
        uint32_t indexOffset = 0, indexCount = 0;
        int    sdfMode = 0;
        float  sdfX = 0, sdfY = 0, sdfW = 0, sdfH = 0;
        float  sdfRadiusTL = 0, sdfRadiusTR = 0, sdfRadiusBR = 0, sdfRadiusBL = 0;
        float  sdfThickness = 0, aaOn = 1;
        int    scissorOn = 0, scX = 0, scY = 0, scW = 0, scH = 0;
    };
    struct VKTexCmd {
        bool destroy = false;      // true=销毁（回放帧末 retired 释放）
        uint32_t id = 0;
        int w = 0, h = 0;
        std::vector<uint8_t> rgba; // create 时的像素（destroy 为空）
    };
    std::vector<VKCmd> vkCmds;
    std::vector<VKTexCmd> vkTexCmds;
    float vkCr = 0.10f, vkCg = 0.11f, vkCb = 0.14f, vkCa = 1.f;  // 清屏色快照

    // ---- 共享几何/像素载荷（两后端共用；池化跨帧保容量零分配）----
    std::vector<Vertex> verts;
    std::vector<uint32_t> indices;
    std::vector<uint8_t> pixels;   // GL CreateTex 像素池（按命令顺序追加）

    // 复位跨帧复用（clear 保容量：录制/回放稳定后零堆分配）
    void reset() {
        cmds.clear(); vkCmds.clear(); vkTexCmds.clear();
        verts.clear(); indices.clear(); pixels.clear();
    }
};

} // namespace ui