// AI生成
// ============================================================
// 下拉选择系统实现（系统派发：统一收编下拉按钮的绘制与交互）
//
// 实现源自原 WinHandlers_ThemeStudio.cpp tab3 六行下拉的完整闭环，
// 抽取为全引擎唯一实现：自适应翻转弹出 / 裁剪滚动 / 迷你滚动条 /
// 打开定位选中项 / 吞点击防穿透 / 单开 / 帧末失活。
// ============================================================
#include "DropdownSystem.h"
#include "WinControls.h"          // wctl::drawTextLeftAt
#include "UiContext.h"
#include "StyleTpl.h"             // tpl::internalOf（「下拉按钮」游戏ID 单源分派）
#include "../Element.h"
#include "../Theme.h"
#include <algorithm>

namespace ui {

// 下拉统一样式常量（逻辑像素，使用处乘 scale）
static constexpr float kListGap    = 2.0f;   // 按钮底与列表顶间距
static constexpr float kRadius     = 4.0f;   // 按钮/列表圆角
static constexpr float kBtnBorderW = 1.0f;   // 按钮边框线宽
static constexpr float kArrowOffX  = 12.0f;  // 箭头距按钮右边的偏移
static constexpr float kTextPadX   = 8.0f;   // 按钮文字左内边距
static constexpr float kTextPadY   = 2.0f;   // 按钮文字上内边距
static constexpr float kItemPadX   = 6.0f;   // 列表项文字左内边距
static constexpr float kClipInset  = 2.0f;   // 列表裁剪内缩（避圆角）
static constexpr float kScrollbarW = 3.0f;   // 迷你滚动条宽度
static constexpr float kScrollbarPad = 3.0f; // 迷你滚动条轨道内边距
static constexpr int   kMaxVisible = 5;      // 下拉列表一次最多显示项数（超出滚动）

DropdownState* DropdownSystem::findOrCreate(WindowInstance& win, const char* id) {
    for (auto& d : items_) {
        if (d.winId == win.id && d.id == id) return &d;
    }
    DropdownState d;
    d.winId = win.id;
    d.id = id;
    items_.push_back(d);
    return &items_.back();
}

void DropdownSystem::layoutList(DropdownState& st, const DropdownRequest& req) {
    float s = g_uiCtx.scale;
    float itemH = st.itemH;
    // 系统规则：列表一次最多显示 kMaxVisible 项，超出靠滚轮/滚动条
    float need = itemH * (float)req.itemCount;
    float cap  = itemH * (float)kMaxVisible;
    if (need > cap) need = cap;
    float btnBottom = req.btnRect.pos.y + req.btnRect.size.y;
    float ly = btnBottom + kListGap * s;
    float listY, lh;
    if (req.upBound > 0 && req.downBound > 0) {
        // 自适应翻转：向下空间不足时向上翻（列表底贴按钮顶）；
        // 两侧都不足取更大侧压缩高度，保证列表完整可选且不出约束区
        float downSpace = req.downBound - ly;
        float upSpace = req.btnRect.pos.y - req.upBound;
        if (need <= downSpace) { listY = ly; lh = need; }
        else if (need <= upSpace) { listY = req.btnRect.pos.y - need; lh = need; }
        else if (downSpace >= upSpace) { listY = ly; lh = downSpace; }
        else { listY = req.btnRect.pos.y - upSpace; lh = upSpace; }
        if (lh > cap) lh = cap;                 // 再压到最多 kMaxVisible 项
        if (lh < itemH) lh = itemH;             // 极端保底：至少一项高
        // 区内裁剪：列表整体不得越出 [upBound, downBound]
        if (listY + lh > req.downBound) listY = req.downBound - lh;
        if (listY < req.upBound) listY = req.upBound;
    } else {
        listY = ly; lh = need;
        if (lh < itemH) lh = itemH;
    }
    st.listRect = Rect(Vec2(req.btnRect.pos.x, listY), Vec2(req.btnRect.size.x, lh));
}

bool DropdownSystem::draw(RenderDevice* dev, WindowInstance& win,
                          const DropdownRequest& req, float alpha) {
    if (!dev) return false;
    DropdownState* st = findOrCreate(win, req.id);
    st->frameStamp = frame_;
    st->active = true;
    st->btnRect = req.btnRect;
    st->items = req.items;
    st->itemCount = req.itemCount;
    st->selected = req.selected;
    st->target = req.target;
    st->dirty = req.dirty;
    st->itemH = req.itemH > 0 ? req.itemH : (20.0f * g_uiCtx.scale);
    st->font = req.font;
    st->upBound = req.upBound;
    st->downBound = req.downBound;
    // 系统规划：滚动容器可无限上下，按钮为固定屏幕坐标；一旦按钮滚出可视区
    // （无法显示）→ 自动收起，不残留展开态（无需页面逐帧自行判断/打补丁）
    if (req.upBound > 0 && req.downBound > 0) {
        float bTop = req.btnRect.pos.y;
        float bBot = req.btnRect.pos.y + req.btnRect.size.y;
        bool visible = (bBot > req.upBound) && (bTop < req.downBound);
        if (!visible) st->open = false;
    }
    layoutList(*st, req);

    ThemeColors& c = currentColors();
    float s = g_uiCtx.scale;
    Font* font = req.font ? req.font : g_uiCtx.smallFont;
    bool open = st->open;

    // 按钮绘制：全引擎下拉按钮唯一单源。模板由「下拉按钮」游戏ID 单源控制
    // （g_uiCtx.dropdownStyle / tpl::dropdown）：
    //   内部 0 = 系统默认（单块 bg2 底 + 边框 + 左对齐文字 + 右侧三角，默认行为不变）
    //   内部 0 = 系统默认（单块 bg2 底 + 边框 + 左对齐文字 + 右侧三角，默认行为不变）
    //   内部 1..2 = 「特别绘制」族（游戏ID 2..3：圆角胶囊 / 空心描边 → wctl::drawComboBtnStyled）
    // 扩展代码禁止自绘下拉按钮。
    const char* vname = (req.selected >= 0 && req.selected < req.itemCount)
                      ? req.items[req.selected] : "-";
    int ds = tpl::internalOf(0, g_uiCtx.dropdownStyle);
    if (ds >= 1) {
        wctl::drawComboBtnStyled(dev, req.btnRect, vname, st->hoverBtn, open, alpha, font, ds);
    } else {
        // 按钮底（bg2 半透，默认原样）
        drawRoundedRect(dev, req.btnRect,
                        Color(c.bg2.r, c.bg2.g, c.bg2.b, 160.0f / 255.0f * alpha),
                        kRadius * s);
        // 按钮边框：开或 hover → accent，普通 → inputBorder
        Color bc = (open || st->hoverBtn) ?
            Color(c.accent.r, c.accent.g, c.accent.b, 180.0f / 255.0f * alpha) :
            Color(c.inputBorder.r, c.inputBorder.g, c.inputBorder.b, 180.0f / 255.0f * alpha);
        drawRoundedBorder(dev, req.btnRect, bc, kBtnBorderW * s, kRadius * s);
        // 当前选中项文字
        wctl::drawTextLeftAt(dev, font, vname,
                             Vec2(req.btnRect.pos.x + kTextPadX * s, req.btnRect.pos.y + kTextPadY * s),
                             req.btnRect.size.y - 4.0f * s,
                             Color(c.text.r, c.text.g, c.text.b, 220.0f / 255.0f * alpha));
        // 下拉箭头（矢量小三角，SDF 软边）
        float ax = req.btnRect.pos.x + req.btnRect.size.x - kArrowOffX * s;
        float ay = req.btnRect.pos.y + req.btnRect.size.y * 0.5f;
        fillTriangle(dev, ax - 4.0f * s, ay - 2.0f * s, ax + 4.0f * s, ay - 2.0f * s,
                     ax, ay + 3.0f * s,
                     Color(c.text.r, c.text.g, c.text.b, 160.0f / 255.0f * alpha));
    }
    return open;
}

void DropdownSystem::renderPopups(RenderDevice* dev, WindowInstance& win, float alpha) {
    if (!dev) return;
    ThemeColors& c = currentColors();
    float s = g_uiCtx.scale;
    for (auto& st : items_) {
        if (st.winId != win.id || !st.active || !st.open) continue;
        const Rect& lr = st.listRect;
        int total = st.itemCount;
        int visCount = (int)(lr.size.y / st.itemH + 0.01f);
        if (visCount > total) visCount = total;
        int maxScroll = total - visCount;
        int startIdx = (int)st.scroll;
        if (startIdx > maxScroll) startIdx = maxScroll;
        if (startIdx < 0) startIdx = 0;
        Font* font = st.font ? st.font : g_uiCtx.smallFont;
        // 区内裁剪：整个弹出层限制在 [upBound, downBound]（区域外一律不显示）
        bool bounded = (st.upBound > 0 && st.downBound > 0);
        if (bounded) {
            dev->pushScissorRect((int)lr.pos.x, (int)st.upBound,
                                 (int)lr.size.x, (int)(st.downBound - st.upBound));
        }

        // 列表底 + 边框
        drawRoundedRect(dev, lr,
                        Color(c.tabBg.r, c.tabBg.g, c.tabBg.b, 250.0f / 255.0f * alpha),
                        kRadius * s);
        drawRoundedBorder(dev, lr,
                          Color(c.inputBorder.r, c.inputBorder.g, c.inputBorder.b, 200.0f / 255.0f * alpha),
                          kBtnBorderW * s, kRadius * s);
        // 项区裁剪（滚动越界部分不绘制；内缩避开圆角边框）
        int px = (int)(lr.pos.x + kClipInset * s);
        int py = (int)(lr.pos.y + kClipInset * s);
        int pw = (int)(lr.pos.x + lr.size.x - kClipInset * s) - px;
        int ph = (int)(lr.pos.y + lr.size.y - kClipInset * s) - py;
        dev->pushScissorRect(px, py, pw, ph);
        for (int j = 0; j < visCount; j++) {
            int item = startIdx + j;   // 实际项号 = 滚动偏移 + 可见行
            if (item >= total) break;
            Rect ir(Vec2(lr.pos.x + kClipInset * s, lr.pos.y + j * st.itemH),
                    Vec2(lr.size.x - 10.0f * s, st.itemH));  // 右侧留滚动条空间
            if (item == st.hoverItem)
                drawRoundedRect(dev, ir,
                                Color(c.accent.r, c.accent.g, c.accent.b, 60.0f / 255.0f * alpha),
                                3.0f * s);
            bool sel = (item == st.selected);
            wctl::drawTextLeftAt(dev, font, st.items[item],
                                 Vec2(ir.pos.x + kItemPadX * s, ir.pos.y + kTextPadY * s),
                                 ir.size.y - 4.0f * s,
                                 sel ? Color(c.accent.r, c.accent.g, c.accent.b, alpha)
                                     : Color(c.text.r, c.text.g, c.text.b, 200.0f / 255.0f * alpha));
        }
        dev->popScissorRect();
        // 迷你滚动条（内容超出列表高度时显示：轨道 + accent 拇指指示位置）
        if (maxScroll > 0) {
            float barW = kScrollbarW * s;
            float trackX = lr.pos.x + lr.size.x - barW - kScrollbarPad * s;
            float trackY = lr.pos.y + kScrollbarPad * s;
            float trackH = lr.size.y - kScrollbarPad * 2.0f * s;
            drawRect(dev, Rect(Vec2(trackX, trackY), Vec2(barW, trackH)),
                     Color(c.inputBorder.r, c.inputBorder.g, c.inputBorder.b,
                           100.0f / 255.0f * alpha));
            float thumbH = trackH * (float)visCount / (float)total;
            if (thumbH < barW * 1.5f) thumbH = barW * 1.5f;
            float thumbY = trackY + (trackH - thumbH) *
                           ((float)startIdx / (float)maxScroll);
            drawRect(dev, Rect(Vec2(trackX, thumbY), Vec2(barW, thumbH)),
                     Color(c.accent.r, c.accent.g, c.accent.b, 170.0f / 255.0f * alpha));
        }
        if (bounded) dev->popScissorRect();
    }
}

DropdownHit DropdownSystem::handlePress(WindowInstance& win, const Vec2& m) {
    DropdownHit result;
    // 1. open 下拉的列表命中 → 选项选中写值 + 关闭（吞点击防穿透）
    for (int i = (int)items_.size() - 1; i >= 0; i--) {
        DropdownState& st = items_[i];
        if (st.winId != win.id || !st.active || !st.open) continue;
        if (!st.listRect.contains(m)) continue;
        int total = st.itemCount;
        int visCount = (int)(st.listRect.size.y / st.itemH + 0.01f);
        if (visCount > total) visCount = total;
        int maxScroll = total - visCount;
        int startIdx = (int)st.scroll;
        if (startIdx > maxScroll) startIdx = maxScroll;
        if (startIdx < 0) startIdx = 0;
        int row = (int)((m.y - st.listRect.pos.y) / st.itemH);
        int idx = startIdx + row;   // 实际项号 = 滚动偏移 + 可见行（与渲染一致）
        if (idx >= 0 && idx < total) {
            if (st.target) *st.target = idx;
            if (st.dirty && st.selected != idx) *st.dirty = true;
            st.selected = idx;
            result.id = st.id.c_str();
            result.newIndex = idx;
        }
        st.open = false;
        result.consumed = true;
        return result;
    }
    // 2. 按钮命中 → 切换 open（单开：先关本窗口所有其他 open）
    for (int i = (int)items_.size() - 1; i >= 0; i--) {
        DropdownState& st = items_[i];
        if (st.winId != win.id || !st.active) continue;
        if (!st.btnRect.contains(m)) continue;
        for (auto& d2 : items_) {
            if (d2.winId == win.id && d2.open && &d2 != &st) d2.open = false;
        }
        st.open = !st.open;
        if (st.open) {
            // 打开即定位当前选中项到可见区中部（长列表打开就能看到选中态）
            int total = st.itemCount;
            int vis = (int)(st.listRect.size.y / st.itemH + 0.01f);
            if (vis > total) vis = total;
            int maxS = total - vis;
            st.scroll = maxS > 0 ? (float)(st.selected - vis / 2) : 0.0f;
            if (st.scroll < 0) st.scroll = 0;
            if (st.scroll > (float)maxS) st.scroll = (float)maxS;
        }
        result.consumed = true;
        return result;
    }
    // 3. 有 open 下拉但点在列表外 → 关闭（吞点击防穿透）
    for (auto& st : items_) {
        if (st.winId == win.id && st.active && st.open) {
            st.open = false;
            result.consumed = true;
            return result;
        }
    }
    return result;
}

void DropdownSystem::updateHover(WindowInstance& win, const Vec2& m) {
    for (auto& st : items_) {
        if (st.winId != win.id || !st.active) continue;
        st.hoverBtn = st.btnRect.contains(m);
        st.hoverItem = -1;
        if (st.open && st.listRect.contains(m)) {
            int total = st.itemCount;
            int visCount = (int)(st.listRect.size.y / st.itemH + 0.01f);
            if (visCount > total) visCount = total;
            int maxScroll = total - visCount;
            int startIdx = (int)st.scroll;
            if (startIdx > maxScroll) startIdx = maxScroll;
            if (startIdx < 0) startIdx = 0;
            int row = (int)((m.y - st.listRect.pos.y) / st.itemH);
            int idx = startIdx + row;
            if (idx >= 0 && idx < total) st.hoverItem = idx;
        }
    }
}

bool DropdownSystem::handleWheel(WindowInstance& win, const Vec2& m, float delta) {
    if (delta == 0.0f) return false;
    for (auto& st : items_) {
        if (st.winId != win.id || !st.active || !st.open) continue;
        if (!st.listRect.contains(m)) continue;
        int total = st.itemCount;
        int visCount = (int)(st.listRect.size.y / st.itemH + 0.01f);
        if (visCount > total) visCount = total;
        int maxScroll = total - visCount;
        if (maxScroll <= 0) return true;   // 无需滚动也消费（防穿透滚动到底层内容）
        st.scroll -= delta;                 // 滚轮上 = 内容上移 = 查看更早的项
        if (st.scroll < 0.0f) st.scroll = 0.0f;
        if (st.scroll > (float)maxScroll) st.scroll = (float)maxScroll;
        return true;
    }
    return false;
}

void DropdownSystem::endFrame() {
    // 本帧未续期（frameStamp 停在旧帧）→ 失活（切页整组隐藏 / 窗口隐藏 / tab 切换）
    for (auto& st : items_) {
        if (st.frameStamp != frame_) {
            st.active = false;
            st.open = false;
            st.hoverBtn = false;
            st.hoverItem = -1;
        }
    }
    frame_++;
}

void DropdownSystem::removeWindow(int winId) {
    items_.erase(std::remove_if(items_.begin(), items_.end(),
        [winId](const DropdownState& d) { return d.winId == winId; }), items_.end());
}

} // namespace ui