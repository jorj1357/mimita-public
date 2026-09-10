#include "debug/npckillfeed-log.h"

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <string>

namespace {

FILE* gLog = nullptr;
std::string gPath;

const char* wallClock()
{
    static char buf[32];
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_s(&tm, &t);
    std::strftime(buf, sizeof(buf), "%H%M%S", &tm);
    return buf;
}

const char* dateDir()
{
    static char buf[32];
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_s(&tm, &t);
    std::strftime(buf, sizeof(buf), "%m-%d-%Y", &tm);
    return buf;
}

void ensureOpen()
{
    if (gLog)
        return;
    std::string dir = "logs/" + std::string(dateDir()) + "/tempdebuglogs";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    gPath = dir + "/npckillfeed_" + wallClock() + ".txt";
    gLog = fopen(gPath.c_str(), "a");
    if (!gLog)
        return;
    fprintf(gLog, "=== NPC KILLFEED DEBUG LOG ===\n");
    fflush(gLog);
}

} // namespace

void NpcKillfeedLog::init()
{
    ensureOpen();
}

void NpcKillfeedLog::log(const char* fmt, ...)
{
    ensureOpen();
    if (!gLog)
        return;
    fprintf(gLog, "[%s] ", wallClock());
    va_list args;
    va_start(args, fmt);
    vfprintf(gLog, fmt, args);
    va_end(args);
    fprintf(gLog, "\n");
    fflush(gLog);
}
