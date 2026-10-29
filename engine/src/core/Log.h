#pragma once
#include <cstdio>

namespace ui {

enum class LogLevel : int { Trace = 0, Info, Warn, Error };

class Log {
public:
    static void setLevel(LogLevel l);
    static LogLevel level();
    static void raw(LogLevel l, const char* tag, const char* fmt, ...);
};

#define UI_LOG_TRACE(...) ::ui::Log::raw(::ui::LogLevel::Trace, "trace", __VA_ARGS__)
#define UI_LOG_INFO(...)  ::ui::Log::raw(::ui::LogLevel::Info,  "info ", __VA_ARGS__)
#define UI_LOG_WARN(...)  ::ui::Log::raw(::ui::LogLevel::Warn,  "warn ", __VA_ARGS__)
#define UI_LOG_ERROR(...) ::ui::Log::raw(::ui::LogLevel::Error, "error", __VA_ARGS__)

} // namespace ui