# 记忆 - 2026-11-05 - 热键收编为 KeyRouter 键路由系统

## 一句话

热键判定从「页面散写 / 全局硬判」收编为系统派发：KeyRouter 键路由。页面键表声明，系统唯一判定，作用域跟随当前内容页。

## 为什么收编

- 页面各写一份：同一逻辑出现第 2 次违反唯一实现，漏抄的页面按键无反应
- 搬到应用帧全局判定：唯一了但作用域没了，做不到「只在特定底触发」
- 收编形状与 RegionSystem 同形：**扩展声明 → 系统唯一判定 → 打包返回**

## 判定链（顺序不可调）

1. 文本输入有焦点 → 整条链让位
2. 内容层：当前内容页键表（换页自动换表）
3. 应用层：整应用键表（默认空，登记须写明「为什么必须跨页」）
4. 未命中即丢弃，**没有隐式全局兜底**

## 两边落地清单

| 项 | 万象沙盒 (E:\工作区\继续\万象沙盒) | 988-26 (D:\UI自研\主修改\988-26) |
|---|---|---|
| 新增 | KeyRouter.h/.cpp（同源复制） | 同左 |
| Input | +keyMods()（Ctrl/Shift/Alt 归一，掩掉 CAPS/NUM） | 同左 |
| 声明口 | AppContentHandler::hotkeys 回调 | 同左 |
| 内容页表 | game：F4 开设置、Esc 退出存档页 | main：F1 重置、F3 切主题（cycleTheme 单源） |
| 应用级表 | 空（不给全局特权） | F2 切后端、F4 新应用、F11 全屏、Esc 关栈顶（窗口级能力，登记即理由） |
| 废除 | — | UiContext::hotkeyMask/HotkeyBits、WindowManager Esc 判定、键码常量 |

## 关键细节

- 动作返回 bool：false = 不接这个键（继续问下一层）；禁止消费掉却什么都不做
- 修饰位只比三位：GLFW mods 的 CAPS/NUM 是状态位不是修饰键
- cycleTheme 对负偏移补一次取模：C++ 负数取模得负，直接写会 g_themes[-1] 越界
- 988-26 构建缓存曾指向 U: 盘（换机残留），rm 0-Release/0-Debug 后用 MinGW Ninja 重配（c++.exe / ninja.exe 均在 C:/Users/mn/mingw64/bin）
- 两项目换行口径不同：万象沙盒 CRLF，988-26 也 CRLF——编辑后核对 行数 vs CR 数即可

## 规范落点

- `E:\工作区\1-规范整理\某项目类型\热键系统规范.md`（新增）
- `规范总纲.md` 文档索引补该行
- `自研UI引擎规范.md` §2.2 输入路由表修正 + §2.2.1 键路由小节
- 收编判定：全局搜 `keyPressed(` 只允许出现在 Input / KeyRouter / TextInputSystem 三处

## 验证

- 万象沙盒：Release + Debug 全量 41 步零错误零警告，冒烟 content=game ready
- 988-26：Release + Debug 全量 63 步零错误零警告，冒烟 content=main ready

---

## 追加（03:00）作用域三档：统一注册表取代双轨

用户反馈「可以申请全局吗，不然非常难受的」「a全局，b选择1底，c限制3底」。

- **为什么再改**：第一版「内容页键表 + 全局表」两档不够 —— 「限制 3 底」若靠 3 页各抄一份就退化成散写。收敛成**一张统一注册表**，每行带作用域。
- **三档（同一字段）**：`KSCOPE_ALL`（全局，必填 reason）/ `KSCOPE_ONLY, {"game"}`（1 底）/ `KSCOPE_ONLY, {"start","game","base"}`（N 底）；系统拿当前内容页 `name` 比对白名单。
- **删掉**：`AppContentHandler::hotkeys` 回调、GameHome/MainUI 的 `keyTable_`、`setApp/setContent` 双入口。页面只提供动作接口（GameHome: `savesPageOpen/closeSavesPage`）。
- **审计**：`KeyRouter::audit()` 建表一次，逐条打印 `[A 全局] / [B 1底] / [C N底]` + 键号 + 白名单/reason；全局缺理由、限定档空名单即告警。
- **万象沙盒**：A F4 开设置 / B game 底 Esc 退存档页 / C start+game+base 三底 F3 主题循环。
- **988-26**：A F2/F11/F4/Esc 四条全局 / B main 底 F3 切主题 / C main+test 两底 F1 重置。
- **验证**：两边 Release+Debug 零错误零警告，冒烟 ready，审计日志三档齐全。
- **构建环境**：988-26 增量构建会因中文依赖缓存路径报 `FindFirstFileExA(E:/工作区/_deps_cache/...)`；全量重建（删 0-Release/0-Debug 后重配）可正常通过，非代码问题。
