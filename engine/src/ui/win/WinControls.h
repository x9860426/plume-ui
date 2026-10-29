// AI生成
#pragma once
// ============================================================
// 窗口内容共享控件原语（按钮/滑条/复选框）
// 供各窗口处理器复用的内容区控件绘制，全部主题色驱动。
// 控件只负责"画"，不负责交互路由（路由由 WindowManager 统一派发，
// 处理器用布局函数单源计算命中矩形）。
// ============================================================
#include "WinCommon.h"

namespace ui {

namespace wctl {

// ====== 任意逻辑像素字号文本（矩形内垂直居中；align 0=左 1=中 2=右） ======
// 参考 GDI+ Font(px*g_scale) 的等价实现：单一大字号图集 + 顶点缩放渲染
void drawTextScaled(RenderDevice* dev, Font* f, const char* utf8, const Rect& r,
                    const Color& c, float px, int align = 0);

// ====== 主题色按钮（垂直渐变底 + hover 变体 + 边框 + 居中文字） ======
// 对齐参考 FillGradientRoundedRect(btnBg1→btnBg2 / hover btnHover1→btnHover2)。
// px > 0 按逻辑像素字号渲染（参考 12/13px）；px = 0 用字体默认尺寸（兼容旧调用）。
// zoom：沙盘等局部缩放场景传视口倍率（默认 1）；圆角/边框与矩形同比例缩放。
// id：按钮绘制模板「游戏ID」（见 StyleTpl.h）。0=系统跟随（跟随全局 g_uiCtx.btnStyle）；
//     1=系统默认 2=方形 3=圆润。不传即 0（跟随全局）。
void drawBtn(RenderDevice* dev, const Rect& r, const char* utf8,
             bool hover, float alpha, float px = 0.0f, float zoom = 1.0f,
             int id = 0);

// ====== 次按钮（参考 DrawButton btnIdx!=0：inputBg@220 底 + 细边框） ======
// 边框 hover accent@180 / 普通 inputBorder@120；文字 hover accent / 普通 text；
// 对齐参考警告窗"取消"/设置窗"取消"的画法（L1057-1061）。
// id：同 drawBtn（0=跟随全局 g_uiCtx.btnStyle；1=系统默认 2=方形 3=圆润）
void drawBtnSecondary(RenderDevice* dev, const Rect& r, const char* utf8,
                      bool hover, float alpha, float px = 0.0f, int id = 0);

// ====== 线性进度条原语（胶囊轨道 + 渐变/纯色填充；悬浮窗/页面通用） ======
// t 0~1（越界钳制）；填充色随 g_uiCtx.circGrad 开关联动：
//   开 = accent→accent2 水平渐变（与环形进度同开关），关 = 纯 accent
void drawProgress(RenderDevice* dev, const Rect& r, float t, float alpha = 1.0f);

// ====== 条形进度「特别绘制」族（内部 1..3 = 游戏ID 2..4；对照 g_uiCtx.progressStyle）======
// style = 内部风格值：1=分段 2=霓虹 3=斜纹。
// 由 wctl::drawProgress 在 g_uiCtx.progressStyle>0 时统一分发；主题色单源、零裸色。
// 本函数只画「轨道 + 填充」，左上标签 / 右上百分比仍由 pctl::drawProgressBar 在外层绘制。
void drawProgressStyled(RenderDevice* dev, const Rect& r, float t, float alpha, int style);

// ====== 文本输入框原语（表单/搜索框通用；悬浮窗/页面复用） ======
// 底 inputBg@240 + 圆角 inputRadius；边框焦点态 accent@200 / 普通 inputBorder@120；
// value 空且 placeholder 非空 → 灰色占位提示；focused && caretOn → 绘制光标竖线
// caretByte：光标的 UTF-8 字节偏移（-1 = 文末；中间编辑定位用；
//   与 TextInputSystem::placeCaret/moveCaret 配合，单源换算见下方三函数）
// px 逻辑字号（默认 12，与旧表单一致）

// ---- 输入框文本几何/光标换算（单源；drawInput 与 TextInputSystem 共用） ----
// 文字区矩形（左内边 10*scale 单源；改内边距只动此处）
Rect inputTextRect(const Rect& r);
// 光标字节偏移 → 光标竖线相对文字区 x（前缀测量，UTF-8 感知）
float caretXFromByte(Font* f, const char* value, int caretByte, float px = 12.0f);
// 文字区相对 x → 最近字符边界字节偏移（鼠标点击定位；UTF-8 感知）
int byteFromCaretX(Font* f, const char* value, float localX, float px = 12.0f);

void drawInput(RenderDevice* dev, const Rect& r, const char* value,
               const char* placeholder, bool focused, bool caretOn,
               float alpha, float px = 12.0f, int caretByte = -1);


// ====== 复选框（全引擎唯一绘制，默认对照参考 DrawCheckBox 1:1） ======
// 选中 = checkCheckedBg + 边框(hover 高亮 accent) + 折线勾；
// 未选 = checkBg@200 + checkBorder@140(hover 高亮 accent)。
// 勾显隐由 g_uiCtx.checkboxShowTick 全局单源控制；勾形由 id（「复选框」游戏ID）单源控制：
//   id=0 跟随全局 g_uiCtx.checkStyle；1=系统默认（原折线勾）；2=参考帧勾（精确三段折线）。
// 所有复选框（主界面/悬浮窗/游戏面板/主题工作室/dock）必须统一调用此函数。
void drawCheck(RenderDevice* dev, const Rect& r, bool checked,
               bool hover, float alpha, int id = 0);

// ====== 下拉按钮「特别绘制」族（内部 1..2 = 游戏ID 2..3；对照 g_uiCtx.dropdownStyle）======
// style = 内部风格值：1=圆角胶囊 2=空心描边。
// 由 DropdownSystem::draw 在 g_uiCtx.dropdownStyle>=1 时统一分发；主题色单源、零裸色。
// 每 case 自绘「底 + 边框 + 预览文字 + 右侧向下三角」；扩展代码禁止自绘下拉按钮。
void drawComboBtnStyled(RenderDevice* dev, const Rect& r, const char* previewText,
                        bool hover, bool open, float alpha, Font* font, int style);

// ====== 开关（对照参考悬浮窗 dock 简化版 L1446-1452：胶囊+右滑块，恒开态） ======
// 底形状随 g_uiCtx.switchStyle（0=系统圆润 1=方形 2=半圆润）；
// 扩展图案随 g_uiCtx.switchExtra（在圆形滑块上叠加装饰）。
void drawToggleOn(RenderDevice* dev, const Rect& r, float alpha);

// ====== 开关「特别绘制」族（内部 3..4 = 游戏ID 4..5；对照 g_uiCtx.switchStyle）=====
// style = 内部风格值：3=机械 4=渐变。
// on/off 双态；全部主题色单源、零裸色。由 drawToggleOn / pctl::drawToggle 统一分发。
// 末尾若 g_uiCtx.switchExtra>0 且 ≠6，叠加扩展图案于滑块中心（与既有风格行为一致）。
void drawToggleStyled(RenderDevice* dev, const Rect& r, bool on, float alpha,
                      float zoom, int style);

// ====== 悬浮球「特别绘制」族（内部 2..3 = 游戏ID 3..4；对照 g_uiCtx.ballStyle）=====
// style = 内部风格值：2=玻璃 3=渐变。
// 由 LayerStack::renderBall 在 g_uiCtx.ballStyle>1 时统一分发；主题色单源、零裸色。
// 各 case 自绘阴影与高光；白色标签由 renderBall 在“两个最上层”统一叠加。
void drawBallStyled(RenderDevice* dev, float cx, float cy, float r, float alpha, int style);

// ====== 开关扩展图案（对照 py 脚本自绘开关，颜色全部主题色派生） ======
// switchExtra: 0=无 1=中心微光(轮回) 2=花朵(春天) 3=雪花(冬天) 4=齿轮(时间)
//              5=星月(塔罗牌) 6=宝石变色(王国) 7=蝙蝠标记(吸血鬼) 8=星辉(星空)
//              9=太阳(潮汐) 10=拨杆(工业化) 11=猫爪(猫2) 12=镜面(液体金属)
// 在滑块中心 (c, 半径 r) 绘制图案；extra==6 时额外覆盖底色（on accent/off bg2）。
void drawSwitchExtra(RenderDevice* dev, const Rect& toggle, const Vec2& c,
                     float r, bool on, float alpha);

// ====== 滑条（槽+填充+手柄；绘制风格随 g_uiCtx.sliderStyle） ======
// sliderStyle: 0=默认 1=霓虹细轨(暗黑) 2=竖柄霓虹(暗黑垂直) 3=春意描边(春天) 4=像素霓虹(电玩)
// 所有风格填充统一 accent → accent2 左右渐变（与默认公式一致）
// zoom：沙盘等局部缩放场景传视口倍率（默认 1 = 全局一致标准外观）；
// 槽高/手柄/圆角/边框整体 ×zoom 与 groove 矩形同比例，缩放后外观不失调。
void drawSlider(RenderDevice* dev, const Rect& groove, float t,
                bool hover, float alpha, float zoom = 1.0f);

// ====== 滑条取值：屏幕 x 坐标 → [0,1]（clamp） ======
float sliderValueAt(const Rect& groove, float x);

// ====== 值文字滑条（对照参考帧滑块主绘制移植：整框 + 高亮抓块 + 居中值文字） ======
// 与 drawSlider 的"槽+手柄"不同：整条就是一个圆角框（输入框质感），
// 抓块占满框高、按 t 在框内滑动（宽 = min(框高×2, 框宽)），
// 值文字绘制在整框中心（不随抓块移动）。valueText 为空则只画滑块。
// zoom：同 drawSlider（沙盘等局部缩放场景整体等比）。
void drawSliderValue(RenderDevice* dev, const Rect& groove, float t,
                     const char* valueText, bool hover, float alpha,
                     float zoom = 1.0f);

// ====== 「中间带值」滑条族（内部 7..10 = 游戏ID 8..11；值文字整框居中） ======
// variant = 内部风格值 - 7（0..3）：
//   0 值-圆柄 1 值-分段 2 值-霓虹 3 值-大圆
// 共同点：valueText 绘制在整框中心；差异仅在槽/填充/手柄几何与配色。
// 由 wctl::drawSlider（sliderStyle>=7）统一分发；全部主题色单源、零裸色。
void drawSliderValueBar(RenderDevice* dev, const Rect& groove, float t,
                        const char* valueText, bool hover, float alpha,
                        float zoom, int variant);

// ====== 系统文本自适应（字体行高超出可用高 → 自动等比缩小；系统暴力兜底） ======
// 全局开关 g_uiCtx.textAutoFit（关=按原样绘制）；超界时若 g_uiCtx.textAutoFitLog
// 则终端告警一次（报告"哪里字体超了"）。返回"适配后的绘制缩放"：
// 未超界 = baseDrawScale 原样；超界 = 按比例缩小后的缩放（调用方用它同时缩放
// measure/行高，保持居中一致）。availH<=0 视为不约束。绝对单源，所有文本原语统一走它。
float uiTextFit(Font* f, const char* utf8, float baseDrawScale, float availH);

// ====== 左对齐/居中文字（垂直居中，窗口内容区排版基础件） ======
void drawTextLeftAt(RenderDevice* dev, Font* f, const char* utf8,
                    const Vec2& pos, float h, const Color& c);
void drawTextCenterAt(RenderDevice* dev, Font* f, const char* utf8,
                      const Rect& r, const Color& c);

// ====== 字号换算单源：屏幕实际像素高 → 可传给 px 参数的值 =====
// 场景：沙盘缩放等需要"文字实际屏幕高度=指定像素"时使用（内部含悬浮窗
// 放大系数与全局 scale 补偿，业务禁止自算）
float fontPxForScreen(float screenPx);

} // namespace wctl
} // namespace ui