#include "debug-log.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace {

using Clock = std::chrono::steady_clock;

double secondsNow()
{
    static const auto start = Clock::now();
    return std::chrono::duration<double>(Clock::now() - start).count();
}

const char* categoryName(Debug::Category category)
{
    switch (category)
    {
        case Debug::Category::GLB: return "GLB";
        case Debug::Category::Collision: return "COLLISION";
        case Debug::Category::Physics: return "PHYSICS";
        case Debug::Category::Render: return "RENDER";
        default: return "DEBUG";
    }
}

void printv(const char* level, Debug::Category category, const char* fmt, va_list args)
{
    std::printf("[%.3f][%s][%s] ", secondsNow(), level, categoryName(category));
    std::vprintf(fmt, args);
}

}

bool Debug::enabled(Category category)
{
    switch (category)
    {
        case Category::GLB: return DebugConfig::ENABLE_DEBUG_LOGS && DebugConfig::GLB_VERBOSE;
        case Category::Collision: return DebugConfig::COLLISION_VERBOSE;
        case Category::Physics: return DebugConfig::PHYSICS_VERBOSE;
        case Category::Render: return DebugConfig::ENABLE_DEBUG_LOGS && DebugConfig::RENDER_VERBOSE;
        default:
            return DebugConfig::ENABLE_DEBUG_LOGS ||
                   DebugConfig::DEBUG_TICKS ||
                   DebugConfig::DEBUG_INPUT ||
                   DebugConfig::DEBUG_COMMANDS ||
                   DebugConfig::DEBUG_NPC;
    }
}

void Debug::startupReport()
{
    if (DebugConfig::ENABLE_DEBUG_LOGS)
        std::printf("[DEBUG] verbose logging ENABLED; disable in config.h\n");
}

void Debug::log(Category category, const char* fmt, ...)
{
    if (!enabled(category))
        return;

    va_list args;
    va_start(args, fmt);
    printv("INFO", category, fmt, args);
    va_end(args);
}

void Debug::logOnce(Category category, const char* key, const char* fmt, ...)
{
    if (!enabled(category))
        return;

    static std::unordered_set<std::string> printed;
    if (!printed.insert(std::string(categoryName(category)) + ":" + key).second)
        return;

    va_list args;
    va_start(args, fmt);
    printv("INFO", category, fmt, args);
    va_end(args);
}

void Debug::logThrottled(Category category, const char* key, float intervalSeconds, const char* fmt, ...)
{
    if (!enabled(category))
        return;

    static std::unordered_map<std::string, double> lastPrint;
    std::string fullKey = std::string(categoryName(category)) + ":" + key;
    double now = secondsNow();
    auto it = lastPrint.find(fullKey);
    if (it != lastPrint.end() && now - it->second < intervalSeconds)
        return;
    lastPrint[fullKey] = now;

    va_list args;
    va_start(args, fmt);
    printv("INFO", category, fmt, args);
    va_end(args);
}

void Debug::warn(Category category, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    printv("WARN", category, fmt, args);
    va_end(args);
}

void Debug::error(Category category, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    printv("ERROR", category, fmt, args);
    va_end(args);
}

void Debug::logAuto(Category category, const char* fmt, ...)
{
    bool isWarning = std::strstr(fmt, "WARNING") != nullptr;
    bool isError = std::strstr(fmt, "ERROR") != nullptr;
    if (!isWarning && !isError && !enabled(category))
        return;

    va_list args;
    va_start(args, fmt);
    printv(isError ? "ERROR" : (isWarning ? "WARN" : "INFO"), category, fmt, args);
    va_end(args);
}
