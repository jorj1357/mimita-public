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
        case Debug::Category::NpcCombat: return "NPC_COMBAT";
        case Debug::Category::NpcMovement: return "NPC_MOVE";
        case Debug::Category::Ragdoll: return "RAGDOLL";
        case Debug::Category::Replay: return "REPLAY";
        case Debug::Category::Weapons: return "WEAPONS";
        case Debug::Category::Animation: return "ANIM";
        case Debug::Category::Gui: return "GUI";
        case Debug::Category::Networking: return "NET";
        case Debug::Category::Audio: return "AUDIO";
        case Debug::Category::World: return "WORLD";
        case Debug::Category::Duel: return "DUEL";
        case Debug::Category::Auth: return "AUTH";
        case Debug::Category::Chat: return "CHAT";
        case Debug::Category::Vip: return "VIP";
        default: return "DEBUG";
    }
}

const char* categoryColor(Debug::Category category)
{
    switch (category)
    {
        case Debug::Category::Gui: return "\033[36m";   // Cyan
        case Debug::Category::Weapons: return "\033[31m";  // Red
        case Debug::Category::NpcCombat: return "\033[31m"; // Red
        case Debug::Category::Physics: return "\033[33m";   // Yellow
        case Debug::Category::Collision: return "\033[33m"; // Yellow
        case Debug::Category::Replay: return "\033[35m";    // Magenta
        case Debug::Category::Networking: return "\033[32m"; // Green
        case Debug::Category::Ragdoll: return "\033[38;5;214m"; // Orange
        case Debug::Category::Render: return "\033[34m";    // Blue
        case Debug::Category::Audio: return "\033[36m";     // Cyan
        case Debug::Category::Animation: return "\033[36m"; // Cyan
        case Debug::Category::Duel: return "\033[35m";      // Magenta
        case Debug::Category::World: return "\033[32m";     // Green
        case Debug::Category::NpcMovement: return "\033[33m"; // Yellow
        case Debug::Category::Chat: return "\033[36m";   // Cyan
        case Debug::Category::Vip: return "\033[95m";    // Bright magenta
        default: return "\033[0m";  // Reset
    }
}

void printv(const char* level, Debug::Category category, const char* fmt, va_list args)
{
    std::printf("%s[%.3f][%s][%s]\033[0m ", categoryColor(category), secondsNow(), level, categoryName(category));
    std::vprintf(fmt, args);
    std::printf("\033[0m");
}

}

bool Debug::enabled(Category category)
{
    switch (category)
    {
        case Category::GLB: return DebugConfig::ENABLE_DEBUG_LOGS && DebugConfig::GLB_VERBOSE;
        case Category::Collision: return DebugConfig::COLLISION_VERBOSE ||
                                          DebugConfig::DEBUG_COLLISION_SYSTEM ||
                                          DebugConfig::DEBUG_COLLISION_TRACE ||
                                          DebugConfig::DEBUG_COLLISION_DIAGNOSTICS;
        case Category::Physics: return DebugConfig::PHYSICS_VERBOSE;
        case Category::Render: return DebugConfig::ENABLE_DEBUG_LOGS && DebugConfig::RENDER_VERBOSE;
        case Category::NpcCombat: return DebugConfig::DEBUG_NPC_COMBAT;
        case Category::NpcMovement: return DebugConfig::DEBUG_NPC_MOVEMENT;
        case Category::Ragdoll: return DebugConfig::DEBUG_RAGDOLL;
        case Category::Replay: return DebugConfig::DEBUG_REPLAY;
        case Category::Weapons: return DebugConfig::ENABLE_DEBUG_LOGS ||
                                       DebugConfig::DEBUG_RECOIL ||
                                       DebugConfig::DEBUG_RELOAD;
        case Category::Animation: return DebugConfig::DEBUG_ANIMATION;
        case Category::Gui: return DebugConfig::DEBUG_UI;
        case Category::Networking: return DebugConfig::DEBUG_NETWORKING;
        case Category::Audio: return DebugConfig::DEBUG_SOUND;
        case Category::World: return DebugConfig::ENABLE_DEBUG_LOGS;
        case Category::Duel: return DebugConfig::DEBUG_DUEL;
        case Category::Auth: return DebugConfig::ENABLE_DEBUG_LOGS ||
                                   DebugConfig::DEBUG_AUTH;
        case Category::Chat: return DebugConfig::ENABLE_DEBUG_LOGS ||
                                   DebugConfig::DEBUG_CHAT;
        case Category::Vip: return DebugConfig::ENABLE_DEBUG_LOGS ||
                                  DebugConfig::DEBUG_NETWORKING;
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
