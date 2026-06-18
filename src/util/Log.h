#pragma once
#include <string_view>
#include <format>

namespace LightRec::Util {

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error
};

void LogMessage(LogLevel level, std::string_view message);

template <typename... Args>
void LogTrace(std::format_string<Args...> fmt, Args&&... args) {
    LogMessage(LogLevel::Trace, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void LogDebug(std::format_string<Args...> fmt, Args&&... args) {
    LogMessage(LogLevel::Debug, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void LogInfo(std::format_string<Args...> fmt, Args&&... args) {
    LogMessage(LogLevel::Info, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void LogWarn(std::format_string<Args...> fmt, Args&&... args) {
    LogMessage(LogLevel::Warn, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void LogError(std::format_string<Args...> fmt, Args&&... args) {
    LogMessage(LogLevel::Error, std::format(fmt, std::forward<Args>(args)...));
}

} // namespace LightRec::Util
