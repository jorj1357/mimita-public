#include "debug-log.h"
#include "structured-log.h"

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

StructuredCategory structuredCategory(Debug::Category category)
{
    switch (category)
    {
        case Debug::Category::GLB: return StructuredCategory::Glb;
        case Debug::Category::Collision: return StructuredCategory::Collision;
        case Debug::Category::Physics: return StructuredCategory::Physics;
        case Debug::Category::Render: return StructuredCategory::Rendering;
        case Debug::Category::NpcCombat: return StructuredCategory::NpcCombat;
        case Debug::Category::NpcMovement: return StructuredCategory::NpcMovement;
        case Debug::Category::Ragdoll: return StructuredCategory::Ragdoll;
        case Debug::Category::Replay: return StructuredCategory::Replay;
        case Debug::Category::Weapons: return StructuredCategory::Weapons;
        case Debug::Category::Animation: return StructuredCategory::Animation;
        case Debug::Category::Gui: return StructuredCategory::Gui;
        case Debug::Category::Networking: return StructuredCategory::Network;
        case Debug::Category::Audio: return StructuredCategory::Audio;
        case Debug::Category::World: return StructuredCategory::World;
        case Debug::Category::Duel: return StructuredCategory::Duel;
        case Debug::Category::Auth: return StructuredCategory::Auth;
        case Debug::Category::Chat: return StructuredCategory::Chat;
        case Debug::Category::Avatar: return StructuredCategory::Avatar;
        case Debug::Category::Vip: return StructuredCategory::Vip;
        default: return StructuredCategory::General;
    }
}

const char* legacyCategoryKey(Debug::Category category)
{
    switch (category) {
        case Debug::Category::GLB: return "GLB";
        case Debug::Category::Collision: return "COLLISION";
        case Debug::Category::Physics: return "PHYSICS";
        case Debug::Category::Render: return "RENDER";
        case Debug::Category::NpcCombat: return "NPC_COMBAT";
        case Debug::Category::NpcMovement: return "NPC_MOVE";
        case Debug::Category::Ragdoll: return "RAGDOLL";
        case Debug::Category::Replay: return "REPLAY";
        case Debug::Category::Weapons: return "WEAPONS";
        case Debug::Category::Animation: return "ANIMATION";
        case Debug::Category::Gui: return "GUI";
        case Debug::Category::Networking: return "NETWORK";
        case Debug::Category::Audio: return "AUDIO";
        case Debug::Category::World: return "WORLD";
        case Debug::Category::Duel: return "DUEL";
        case Debug::Category::Auth: return "AUTH";
        case Debug::Category::Avatar: return "AVATAR";
        case Debug::Category::Chat: return "CHAT";
        case Debug::Category::Vip: return "VIP";
        default: return "GENERAL";
    }
}

}

bool Debug::enabled(Category category)
{
    return StructuredLogger::instance().shouldLog(structuredCategory(category), StructuredLevel::Verbose);
}

void Debug::startupReport()
{
    log(Category::General, "legacy debug facade routed through config/debuglogger.json\n");
}

void Debug::log(Category category, const char* fmt, ...)
{
    if (!enabled(category))
        return;

    va_list args;
    va_start(args, fmt);
    StructuredLogger::instance().writeVFormatted(structuredCategory(category), StructuredLevel::Verbose,
        "debug-log.cpp", 0, "Debug::log", fmt, args);
    va_end(args);
}

void Debug::logOnce(Category category, const char* key, const char* fmt, ...)
{
    if (!enabled(category))
        return;

    static std::unordered_set<std::string> printed;
    if (!printed.insert(std::string(legacyCategoryKey(category)) + ":" + key).second)
        return;

    va_list args;
    va_start(args, fmt);
    StructuredLogger::instance().writeVFormatted(structuredCategory(category), StructuredLevel::Verbose,
        "debug-log.cpp", 0, "Debug::logOnce", fmt, args);
    va_end(args);
}

void Debug::logThrottled(Category category, const char* key, float intervalSeconds, const char* fmt, ...)
{
    if (!enabled(category))
        return;

    static std::unordered_map<std::string, double> lastPrint;
    // Use stack buffer to avoid heap allocation for throttle key
    char keyBuf[128];
    const char* catName = legacyCategoryKey(category);
    int catLen = (int)std::strlen(catName);
    int keyLen = (int)std::strlen(key);
    if (catLen + 1 + keyLen < (int)sizeof(keyBuf)) {
        std::memcpy(keyBuf, catName, catLen);
        keyBuf[catLen] = ':';
        std::memcpy(keyBuf + catLen + 1, key, keyLen);
        keyBuf[catLen + 1 + keyLen] = '\0';
    } else {
        // Fallback to heap for very long keys (rare)
        std::string fullKey = std::string(catName) + ":" + key;
        double now = secondsNow();
        auto it = lastPrint.find(fullKey);
        if (it != lastPrint.end() && now - it->second < intervalSeconds)
            return;
        lastPrint[fullKey] = now;
        va_list args;
        va_start(args, fmt);
        StructuredLogger::instance().writeVFormatted(structuredCategory(category), StructuredLevel::Verbose,
            "debug-log.cpp", 0, "Debug::logThrottled", fmt, args);
        va_end(args);
        return;
    }
    double now = secondsNow();
    auto it = lastPrint.find(keyBuf);
    if (it != lastPrint.end() && now - it->second < intervalSeconds)
        return;
    lastPrint[std::string(keyBuf)] = now;

    va_list args;
    va_start(args, fmt);
    StructuredLogger::instance().writeVFormatted(structuredCategory(category), StructuredLevel::Verbose,
        "debug-log.cpp", 0, "Debug::logThrottled", fmt, args);
    va_end(args);
}

void Debug::warn(Category category, const char* fmt, ...)
{
    if (!enabled(category))
        return;
    va_list args;
    va_start(args, fmt);
    StructuredLogger::instance().writeVFormatted(structuredCategory(category), StructuredLevel::Important,
        "debug-log.cpp", 0, "Debug::warn", fmt, args);
    va_end(args);
}

void Debug::error(Category category, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    StructuredLogger::instance().writeVFormatted(structuredCategory(category), StructuredLevel::Errors,
        "debug-log.cpp", 0, "Debug::error", fmt, args);
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
    StructuredLogger::instance().writeVFormatted(structuredCategory(category),
        isError ? StructuredLevel::Errors : (isWarning ? StructuredLevel::Important : StructuredLevel::Verbose),
        "debug-log.cpp", 0, "Debug::logAuto", fmt, args);
    va_end(args);
}
