// AI生成
// ============================================================
// 多应用窗口主循环（升级于 2026-10-19）
//
// 架构：一个 AppWindow = 一个完整独立 UI 应用（含分离的悬浮窗栈）。
// 默认创建 1 个应用窗口（行为与旧单窗口版完全一致）；
// F4 新建应用窗口、X/Esc 关闭单个应用、全部关闭 = 退出程序。
//
// ★主子级应用窗口（本轮升级）：
//   - 调试面板「扩展应用窗口」→ 子窗口请求（NewAppRequest）：
//     绑定父级（父关子关）+ 480x320 + "test" 内容 + 禁悬浮窗
//   - 子窗口默认渲染位置 = 父窗口屏幕位置 + 级联偏移；
//     请求可显式指定 x/y（渲染位置）与 w/h（窗口大小）
//   - 父子关系跨重启保持：renderRes 重启（解绑→重建→重绑新 id）、
//     F2 切后端（快照记 parentIdx 下标，按序重建重绑）
//
// 全局动作（帧末统一处理，遍历中不修改容器——规范铁律）：
//   F2（GLFW 291）切后端：全部应用打快照 → 销毁 → 按快照数重建
//     （悬浮窗栈整体迁移，不丢布局；内容/父子/许可全保持）
//   renderRes 变更：仅该应用快照重启（每应用独立分辨率）
//   F4（GLFW 293）/扩展按钮新应用窗口：帧末按请求参数创建
// ============================================================
#include "app/AppWindow.h"
#include "app/AppFwd.h"
#include "render/ThreadScheduler.h"
#include "core/Log.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

using namespace ui;

static const char* backendName(Backend b) {
    return b == Backend::OpenGL ? "OpenGL" : "Vulkan";
}

int main(int argc, char** argv) {
    Backend backend = Backend::OpenGL;
    int initialApps = 1;   // 初始应用窗口数（--apps N；默认 1 个）
    int threadModeInit = -1;   // 初始线程模式（--threads N：0-3；-1=未指定沿用默认单线程）
    std::string openWin;       // 启动即打开的悬浮窗（--open <configId>，如 theme_studio；验证入口）
    int forceW = 0, forceH = 0;  // 启动后强制窗口逻辑尺寸（--winsize WxH：模拟用户 resize，走完整适配链）
    std::string shotTag;         // 引擎内自动拍照（--shot <tag>：延时后首窗口帧缓冲直读落盘 shot-<tag>.png，
                                 // 替代系统级 CopyFromScreen——无任务栏/遮挡/DPI 噪声）
    double shotDelay = 5.0;      // 拍照延时秒（--shotdelay N：等动画/烘焙稳定；默认 5）
    double switchAfter = -1.0;   // 运行时线程模式切换（--switchafter S:mode1[:mode2...]，
                                 // S 秒后按序切换到 mode1、再 S 秒 mode2…模拟用户在主题
                                 // 工作室下拉切换——验证 pause/configure/resume 全链路）
    std::vector<int> switchModes;  // 切换目标模式序列
    int frameModeInit = 0;       // 应用窗口边框模式（--framemode 0/1；验证入口）
    double frameToggleAfter = -1.0;  // 运行时边框模式切换（--frametoggleafter S：
                                     // S 秒后翻转首应用 windowFrame + 置 dirty，
                                     // 与「其他页开关点击」逐位同源——验证快照重启链路）
    double fsAfter = -1.0;           // 运行时进入全屏（--fsafter S：S 秒后首应用 toggleFullscreen）
    bool   hideBarCli = false;       // 预置「全屏（F11）隐藏顶部 UI」开关（--hidebar，验证入口）
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--vk" || a == "--vulkan") backend = Backend::Vulkan;
        if (a == "--gl" || a == "--opengl") backend = Backend::OpenGL;
        if (a == "--apps" && i + 1 < argc) initialApps = std::max(1, atoi(argv[++i]));
        if (a == "--threads" && i + 1 < argc) threadModeInit = atoi(argv[++i]);
        if (a == "--framemode" && i + 1 < argc) frameModeInit = atoi(argv[++i]) != 0 ? 1 : 0;
        if (a == "--frametoggleafter" && i + 1 < argc) {
            double d = atof(argv[++i]);
            if (d > 0.0) frameToggleAfter = d;
        }
        if (a == "--fsafter" && i + 1 < argc) {
            double d = atof(argv[++i]);
            if (d > 0.0) fsAfter = d;
        }
        if (a == "--hidebar") hideBarCli = true;
        if (a == "--open" && i + 1 < argc) openWin = argv[++i];
        if (a == "--shot" && i + 1 < argc) shotTag = argv[++i];
        if (a == "--shotdelay" && i + 1 < argc) {
            double d = atof(argv[++i]);
            if (d >= 0.0) shotDelay = d;
        }
        if (a == "--switchafter" && i + 1 < argc) {
            // 格式 S:mode1[:mode2...]（如 5:1:0 = 5秒切双线程、再5秒回单线程）
            std::string s = argv[++i];
            size_t c = s.find(':');
            if (c != std::string::npos) {
                double d = atof(s.substr(0, c).c_str());
                if (d > 0.0) {
                    switchAfter = d;
                    std::string rest = s.substr(c + 1);
                    size_t p = 0;
                    while (p != std::string::npos) {
                        size_t q = rest.find(':', p);
                        std::string m = (q == std::string::npos) ? rest.substr(p) : rest.substr(p, q - p);
                        if (!m.empty()) switchModes.push_back(atoi(m.c_str()));
                        p = (q == std::string::npos) ? q : q + 1;
                    }
                }
            }
        }
        if (a == "--winsize" && i + 1 < argc) {
            // 格式 WxH（如 960x540）：解析失败忽略（保持创建尺寸）
            std::string s = argv[++i];
            size_t p = s.find('x');
            if (p != std::string::npos) {
                int w = atoi(s.substr(0, p).c_str());
                int h = atoi(s.substr(p + 1).c_str());
                if (w > 0 && h > 0) { forceW = w; forceH = h; }
            }
        }
    }

    Log::setLevel(LogLevel::Info);

    // 重启迁移快照（外循环跨轮保留；空 = 全新创建默认 1 个应用）
    std::vector<AppSnapshot> snapshots;

    // 应用窗口边框模式（0=系统边框 1=完全自绘）：全局单值，跨外循环保持。
    // 取值优先级：快照（用户切换后的值）> 启动参数（默认 0）。切换走
    // 全局重启（全部应用快照重建窗口），语义与 F2 切后端一致。
    int frameMode = frameModeInit;
    bool frameToggledCli = false;   // --frametoggleafter 全进程只触发一次
                                    // （跨外循环保持：重启轮次不得复位，否则反复切换）
    bool fsToggledCli = false;      // --fsafter 全进程只触发一次

    while (true) {
        // 快照轮次：边框模式沿用快照首应用的值（切换后重启据此保持新模式）
        if (!snapshots.empty() && snapshots[0].valid)
            frameMode = snapshots[0].ctx.windowFrame;

        // ============================================================
        // 创建应用组：--apps 指定 / 快照数恢复（保序），默认 1 个。
        // 快照恢复全属性沿用：尺寸/标题/内容处理器/悬浮窗许可；
        // 父子关系按 parentIdx（快照数组内下标）重绑——先父后子，
        // 父重建后拿到新 id，子的 parentId 即指向新实例（链不断）
        // ============================================================
        int n = (int)snapshots.size() > 0 ? (int)snapshots.size() : initialApps;
        const bool fromSnapshots = !snapshots.empty();   // 快照轮次：--threads 不覆盖用户配置
        std::vector<AppWindow*> created;
        for (int i = 0; i < n; i++) {
            AppSnapshot* snap = (i < (int)snapshots.size() && snapshots[i].valid)
                                    ? &snapshots[i] : nullptr;
            int w = snap ? snap->winW : 1280;
            int h = snap ? snap->winH : 720;
            // 标题：快照沿用（子窗口「子窗 #x」等命名保持）；空则默认生成。
            // F2 切后端后：标题里的旧后端名替换为新后端名——否则切换成功
            // 但标题仍显示旧后端，用户无法感知切换（子串替换只动首个；
            // 业务自定义标题不含后端名则零影响）
            std::string title;
            if (snap && !snap->title.empty()) {
                title = snap->title;
                for (const char* nm : { "OpenGL", "Vulkan" }) {
                    size_t p = title.find(nm);
                    if (p != std::string::npos) {
                        title.replace(p, std::strlen(nm), backendName(backend));
                        break;
                    }
                }
            } else {
                title = std::string("UI引擎-") + backendName(backend);
                if (n > 1) title += " #" + std::to_string(i + 1);
            }
            const char* content = snap ? snap->content.c_str() : "main";
            bool allowWin = snap ? snap->allowSuspendWindows : true;
            // 父 id 重绑：parentIdx 指向本轮已重建完成的更早应用
            int parentId = (snap && snap->parentIdx >= 0 &&
                            snap->parentIdx < (int)created.size())
                               ? created[snap->parentIdx]->id() : -1;
            AppWindow* app = AppManager::createApp(w, h, title.c_str(), backend, snap,
                                                   content, parentId, allowWin, frameMode);
            if (!app) {
                UI_LOG_ERROR("app create failed");
                AppManager::destroyAll();
                Window::terminateGLFW();
                return 1;
            }
            // 多应用级联摆放（首个居默认位置，后续错位避免完全重叠）
            if (i > 0) app->setWindowPos(80 + 40 * i, 80 + 40 * i);
            created.push_back(app);
        }
        snapshots.clear();

        // --open <configId>：首轮启动即打开指定悬浮窗（验证入口：截图/
        // 自动化测试免人工点击；快照轮次不重复——快照里窗口已恢复）
        if (!openWin.empty() && !fromSnapshots && AppManager::first()) {
            AppManager::setCurrent(AppManager::first());
            AppManager::first()->winMgr.openWindow(openWin.c_str());
        }
        openWin.clear();

        // --winsize WxH：首轮强制窗口逻辑尺寸（模拟用户拖拽 resize：
        // framebufferSize 回调驱动设备 resize/渲染缩放/字体重烘焙全链
        // ——多线程模式下该链路的录制时序即「随窗口大小字体偏差」
        // 的疑点验证入口）
        if (forceW > 0 && forceH > 0 && !fromSnapshots && AppManager::first()) {
            AppManager::first()->window().setSize(forceW, forceH);
            UI_LOG_INFO("winsize: forced %dx%d", forceW, forceH);
        }

        // --hidebar：预置「全屏（F11）隐藏顶部 UI」开关（与「其他页开关」同源字段，
        // 仅首轮置位；验证入口——配合 --fsafter 截图核对顶栏确已让位）
        if (hideBarCli && !fromSnapshots && AppManager::first())
            AppManager::first()->ctx.fullscreenHideBar = true;

        // ---- 重建完成：线程模式对齐 + 恢复渲染线程 ----
        // 命令行 --threads N：首轮强制指定（快照轮次沿用快照恢复值，
        // 用户运行期经主题工作室所改配置优先）；随后与 RenderThread
        // 当前模式对齐生效。F2 的 pause 自帧末结构段保持至此（销毁
        // 全部→重建期间渲染线程挂起，设备/GL 上下文操作全在主线程）；
        // 常规启动轮次 resume 为无害空操作
        {
            ThreadScheduler& rt = ThreadScheduler::get();
            // ---- 后端多线程许可对齐（OpenGL 强制单线程）----
            // OpenGL 内核不支持多线程（GL "上下文跨线程不安全），切后端
            // 后必须立即设置许可：GL→false（若当前多线程会自动切回单线程）；
            // VK→true（允许多线程，用户可经主题工作室下拉启用）
            rt.setMultithreadAllowed(backend == Backend::Vulkan);
            if (threadModeInit >= 0 && !fromSnapshots && AppManager::first())
                AppManager::first()->ctx.threadMode = threadModeInit;
            int want = AppManager::first() ? AppManager::first()->ctx.threadMode : 0;
            if (want != rt.mode()) {
                glfwMakeContextCurrent(nullptr);   // 主线程让出 GL（同帧末切换语义）
                rt.configure(want);
            }
            rt.resume();
        }


        // ============================================================
        // 帧循环：逐应用驱动（多线程模式：计算线程录制 + 渲染线程回放；
        // currentApp 在计算线程栈帧内单值；多窗口包按提交序串行回放 =
        // 窗口间绘制顺序与单线程完全一致）
        // ============================================================
        bool quit = false;          // 全部应用关闭 → 程序退出
        bool backendSwitch = false; // F2 → 外循环全部重启切后端
        bool frameRestart  = false; // 边框模式切换 → 外循环全部重启重建窗口
        // --shot 自动拍照：帧循环起点（延时到达后下一帧 present 内消费，
        // 落盘完成打日志——测试脚本轮询 shot-<tag>.png 出现即可）
        const auto shotStart = std::chrono::steady_clock::now();
        bool shotDone = false;
        // --switchafter 切换进度：与 --shot 同一计时起点（帧循环开始），
        // S 秒后切第 1 个模式、再 S 秒第 2 个……测试脚本按同一时间轴编排
        int switchIdx = 0;
        while (!quit && !backendSwitch && !frameRestart) {
            // ---- 事件泵：全局一次（多窗口各自 drainEvents，避免 N 倍空泵） ----
            Window::pollAll();
            // ---- 引擎内自动拍照（--shot：延时到期对首窗口发请求；
            //      设备在下一次 present swap 前读帧缓冲落盘） ----
            if (!shotDone && !shotTag.empty() && AppManager::first()) {
                std::chrono::duration<double> el = std::chrono::steady_clock::now() - shotStart;
                if (el.count() >= shotDelay) {
                    AppManager::first()->device()->requestScreenshot(shotTag.c_str());
                    shotDone = true;   // 单次拍照（多次/定时可按需扩展）
                }
            }
            // ---- 运行时线程模式切换（--switchafter：到点对首应用置
            //      threadMode+dirty——与主题工作室下拉完全同路，帧末
            //      pause/configure/resume 消费链零改动复用，保证测试
            //      路径与用户人工切换逐位同源） ----
            if (switchIdx < (int)switchModes.size() && AppManager::first()) {
                std::chrono::duration<double> el = std::chrono::steady_clock::now() - shotStart;
                if (el.count() >= switchAfter * (double)(switchIdx + 1)) {
                    int mode = switchModes[switchIdx++];
                    auto& ctx = AppManager::first()->ctx;
                    ctx.threadMode = mode;
                    ctx.threadModeDirty = true;
                    UI_LOG_INFO("switchafter: app %d -> thread mode %d",
                                AppManager::first()->id(), mode);
                }
            }
            // ---- 运行时边框模式切换（--frametoggleafter：到点翻转首应用
            //      ctx.windowFrame + 置 dirty——与「其他页开关点击」逐位
            //      同源，验证 脏标记 → 快照重启 → 新模式重建 全链路） ----
            if (!frameToggledCli && frameToggleAfter > 0.0 && AppManager::first()) {
                std::chrono::duration<double> el = std::chrono::steady_clock::now() - shotStart;
                if (el.count() >= frameToggleAfter) {
                    auto& ctx = AppManager::first()->ctx;
                    ctx.windowFrame = ctx.windowFrame ? 0 : 1;
                    ctx.windowFrameDirty = true;
                    frameToggledCli = true;
                    UI_LOG_INFO("frametoggleafter: app %d -> windowFrame %d (restart)",
                                AppManager::first()->id(), ctx.windowFrame);
                }
            }
            // ---- 运行时进入全屏（--fsafter：验证「全屏隐藏顶部 UI」开关——
            //      与 F11 人工按键同路，toggleFullscreen 走系统 WM 通路）----
            if (!fsToggledCli && fsAfter > 0.0 && AppManager::first()) {
                std::chrono::duration<double> el = std::chrono::steady_clock::now() - shotStart;
                if (el.count() >= fsAfter) {
                    AppManager::first()->window().toggleFullscreen();
                    fsToggledCli = true;
                    UI_LOG_INFO("fsafter: app %d -> fullscreen toggle (hideBar=%d)",
                                AppManager::first()->id(),
                                AppManager::first()->ctx.fullscreenHideBar ? 1 : 0);
                }
            }
            // ---- 帧驱动（收集生命周期请求，遍历中不改容器） ----
            std::vector<NewAppRequest> newAppReqs;   // 各应用帧内发出的新建请求（可多个）
            std::vector<AppWindow*> toClose;
            std::vector<AppWindow*> toResRestart;
            bool backendSwitchReq = false;
            bool frameRestartReq  = false;   // 边框模式切换请求（其他页开关）
            int  threadModeReq = -1;    // 线程模式切换请求（-1=无；dirty 电平）
            for (auto& up : AppManager::apps()) {
                AppWindow* app = up.get();
                app->frame();
                if (app->shouldClose())            toClose.push_back(app);
                if (app->backendSwitchRequested()) backendSwitchReq = true;
                if (app->frameRestartRequested())  frameRestartReq  = true;
                if (app->newAppRequest().valid)    newAppReqs.push_back(app->newAppRequest());
                if (app->resRestartRequested())    toResRestart.push_back(app);
                // 线程模式（主题工作室「其他UI」下拉置 dirty）：帧内任意
                // 时刻置位 → 此处消费复位 → 帧末结构段生效；多应用同时
                // dirty 时后者覆盖（RenderThread 进程级，取最终一致）
                if (app->ctx.threadModeDirty) {
                    app->ctx.threadModeDirty = false;
                    // OpenGL 后端强制单线程：用户可能在主题工作室选了多线程，
                    // 但当前后端是 GL → 强制修正为单线程（与 configure 内部
                    // 限制双保险，避免 ctx.threadMode 与实际模式长期不一致
                    // 导致每帧重复 configure 尝试 + 刷屏 warning）
                    int reqMode = app->ctx.threadMode;
                    if (app->backend() == Backend::OpenGL && reqMode > 0) {
                        reqMode = 0;
                        app->ctx.threadMode = 0;   // 修正源值，消除后续重复触发
                    }
                    threadModeReq = reqMode;
                }
            }
            backendSwitch = backendSwitchReq;
            frameRestart  = frameRestartReq;

            // ---- 结构变更段（窗口创建/销毁/F2/线程模式切换）：渲染线程
            //      pause 包裹——设备生命周期与 GL 上下文归属只在主线程
            //      变动；pause 语义 = 排空队列 + 渲染线程挂起且不持任何
            //      GL 上下文（replay 用毕即还），主线程可安全接管 ----
            const bool structural = backendSwitch || frameRestart || !toClose.empty() ||
                                    !toResRestart.empty() || !newAppReqs.empty() ||
                                    threadModeReq >= 0;
            if (structural) {
                ThreadScheduler& rt = ThreadScheduler::get();
                rt.pause();
                // ---- F2 切后端：先确定新后端，OpenGL 时先切单线程 ----
                // OpenGL 内核不支持多线程，若当前处于多线程（Vulkan 下启用）
                // 直接销毁/重建会出问题——必须在销毁前先 configure(0)
                // 切回单线程（渲染线程 join 掉），再走后端切换流程
                if (backendSwitch) {
                    Backend actual = AppManager::first() ? AppManager::first()->backend() : backend;
                    Backend newBackend = (actual == Backend::OpenGL) ? Backend::Vulkan : Backend::OpenGL;
                    if (newBackend == Backend::OpenGL) {
                        rt.setMultithreadAllowed(false);  // 自动 configure(0) 若当前多线程
                    }
                }
                if (threadModeReq >= 0) {
                    // 主线程让出当前 GL 上下文（single→multi：上下文归属
                    // 移交渲染线程 replay 接管；multi→single：直通路径
                    // frame 内自行 makeCurrent，无需预占）
                    glfwMakeContextCurrent(nullptr);
                    rt.configure(threadModeReq);   // 排空旧管线后按新模式重建线程组
                }
            }

            // ---- 分辨率重启：本应用快照 → 销毁 → 同后端重建（独立生效） ----
            // 父子保持：destroyApp 会级联销毁子窗口（父关子关），但重启
            // 语义要求子活着 → 先暂解绑（parentId=-1）→ 重建父 → 重绑新 id。
            // 值修改非容器增删，遍历安全（铁律不违）
            for (AppWindow* app : toResRestart) {
                AppSnapshot s = app->takeSnapshot();
                Backend b = app->backend();
                int parentId = app->parentId();   // 父未销毁，id 不变直接沿用
                // 暂解绑子窗口（防级联销毁；收集引用仍有效——只改值不删容器）
                std::vector<AppWindow*> children;
                for (auto& up : AppManager::apps())
                    if (up->parentId() == app->id()) {
                        children.push_back(up.get());
                        up->setParentId(-1);
                    }
                AppManager::destroyApp(app);
                AppWindow* na = AppManager::createApp(
                    s.winW, s.winH, s.title.c_str(), b, &s,
                    s.content.c_str(), parentId, s.allowSuspendWindows);
                if (na)
                    for (AppWindow* c : children) c->setParentId(na->id());
            }

            // ---- 关闭请求：销毁单个应用（destroyApp 内部级联子窗口——
            // 父关子关；内部已防同帧悬空指针重复销毁） ----
            for (AppWindow* app : toClose) {
                AppManager::destroyApp(app);
            }

            // ---- 新应用窗口：按请求参数创建（请求完整描述「开什么样的
            // 窗口」——尺寸/位置/内容/父子/悬浮窗许可，零硬编码） ----
            for (NewAppRequest& req : newAppReqs) {
                // 父已亡（同帧被关闭）→ 降级为独立窗口，不绑定悬空 id
                int parentId = req.parentId;
                if (parentId > 0 && !AppManager::findById(parentId)) parentId = -1;
                std::string title = std::string("UI引擎-") + backendName(backend);
                title += (parentId > 0 ? " 子窗 #" : " #") +
                         std::to_string(AppManager::count() + 1);
                AppWindow* na = AppManager::createApp(
                    req.w, req.h, title.c_str(), backend, nullptr,
                    req.content.c_str(), parentId, req.allowSuspendWindows, frameMode);
                if (!na) continue;
                if (req.x >= 0 && req.y >= 0) {
                    na->setWindowPos(req.x, req.y);            // 请求显式指定渲染位置
                } else if (parentId > 0) {
                    AppWindow* p = AppManager::findById(parentId);
                    int px = 0, py = 0;
                    p->windowPos(px, py);
                    na->setWindowPos(px + 40, py + 40);        // 默认：父窗口位置+偏移
                } else {
                    int idx = AppManager::count();
                    na->setWindowPos(80 + 40 * idx, 80 + 40 * idx);  // 独立窗口级联
                }
                UI_LOG_INFO("app %d created (parent=%d, content=%s, %dx%d, "
                            "suspendWindows=%s, total %d)",
                            na->id(), parentId, req.content.c_str(), req.w, req.h,
                            req.allowSuspendWindows ? "on" : "off", AppManager::count());
            }

            // ---- 结构段收尾：恢复渲染线程（F2/边框模式切换除外——pause
            //      保持跨外循环，全部销毁重建完成后在外循环出口恢复） ----
            if (structural && !backendSwitch && !frameRestart) ThreadScheduler::get().resume();

            // ---- 全部应用关闭 → 程序退出 ----
            if (AppManager::count() == 0) quit = true;
        }

        // ============================================================
        // 外循环出口：真退出 or 全部重启切后端
        // ============================================================
        if (quit && !backendSwitch) break;

        if (backendSwitch) {
            // 按首个应用的实际后端取反（VK 失败自动回退 GL 后实际为 GL）
            Backend actual = AppManager::first() ? AppManager::first()->backend() : backend;
            backend = (actual == Backend::OpenGL) ? Backend::Vulkan : Backend::OpenGL;
            UI_LOG_INFO("backend switch: %s -> %s", backendName(actual), backendName(backend));
        }
        // 全部应用打快照（悬浮窗栈/区域状态/视觉参数/内容/许可/标题整体迁移）
        // → 父子关系转换：parentId（id）→ parentIdx（快照数组下标，
        //    重建时按下标顺序先父后子拿到新 id 重绑，链保持）
        std::vector<AppSnapshot> snaps;
        {
            const auto& apps = AppManager::apps();
            std::vector<std::pair<int, int>> idToIdx;   // (应用id, 容器下标)
            idToIdx.reserve(apps.size());
            for (size_t i = 0; i < apps.size(); ++i)
                idToIdx.push_back({ apps[i]->id(), (int)i });
            for (size_t i = 0; i < apps.size(); ++i) {
                AppWindow* app = apps[i].get();
                snaps.push_back(app->takeSnapshot());
                int pid = app->parentId();
                snaps.back().parentIdx = -1;
                for (const auto& kv : idToIdx)
                    if (kv.first == pid) { snaps.back().parentIdx = kv.second; break; }
            }
        }
        AppManager::destroyAll();
        snapshots = std::move(snaps);
    }

    // 布局记忆：默认关闭，仅代码显式开启 rememberLayout 才落盘
    // （首应用承载 layout.json 读写；悬浮窗布局随应用销毁不强制保存）
    if (AppManager::first()) {
        AppManager::setCurrent(AppManager::first());
        if (g_uiCtx.rememberLayout && g_layerStack.saveLayout("layout.json"))
            UI_LOG_INFO("layout: saved");
    }
    AppManager::destroyAll();
    Window::terminateGLFW();
    return 0;
}