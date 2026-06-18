#include "Log.h"
#include <windows.h>
#include <fstream>
#include <mutex>
#include <chrono>

namespace LightRec::Util {

static std::mutex g_logMutex;
static std::ofstream g_logFile;

static void EnsureLogFile() {
    if (!g_logFile.is_open()) {
        g_logFile.open("LightRec.log", std::ios::out | std::ios::app);
    }
}

static const char* GetLevelName(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO ";
        case LogLevel::Warn:  return "WARN ";
        case LogLevel::Error: return "ERROR";
        default: return "     ";
    }
}

void LogMessage(LogLevel level, std::string_view message) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    EnsureLogFile();

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    struct tm tm_info;
    localtime_s(&tm_info, &time);

    char timeBuffer[32];
    strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", &tm_info);

    std::string formattedMessage = std::format("[{}] [{}] {}\n", timeBuffer, GetLevelName(level), message);

    if (g_logFile.is_open()) {
        g_logFile << formattedMessage;
        g_logFile.flush();
    }

    // Convert to wide string for OutputDebugStringW
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, formattedMessage.c_str(), (int)formattedMessage.length(), NULL, 0);
    if (size_needed > 0) {
        std::wstring wstrTo(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, formattedMessage.c_str(), (int)formattedMessage.length(), &wstrTo[0], size_needed);
        OutputDebugStringW(wstrTo.c_str());
    }
}

} // namespace LightRec::Util
