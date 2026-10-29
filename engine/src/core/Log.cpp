#include "Log.h"
#include <cstdarg>
#include <cstdio>
#include <mutex>

namespace ui {

static LogLevel g_level = LogLevel::Info;

void Log::setLevel(LogLevel l) { g_level = l; }
LogLevel Log::level() { return g_level; }

// 输出互斥：多线程并发日志（录制线程 R 流 + 渲染线程 P 流 + UI 日志）
// 必须整行原子落盘——分段 printf 在线程切换时会交错粘连，破坏
// trace 行结构（逐行 diff 的前提是行完整）。锁内三段 fputs + 一次
// flush，代价可忽略（日志非热路径）。
void Log::raw(LogLevel l, const char* tag, const char* fmt, ...) {
    if (static_cast<int>(l) < static_cast<int>(g_level)) return;
    const char* mark = "TWE";
    char ch = mark[static_cast<int>(l) % 4];
    char head[48];
    std::snprintf(head, sizeof(head), "[%c][%s] ", ch, tag);
    char body[4096];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(body, sizeof(body), fmt, ap);
    va_end(ap);
    static std::mutex mu;
    std::lock_guard<std::mutex> lk(mu);
    std::fputs(head, stdout);
    std::fputs(body, stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);
}

} // namespace ui