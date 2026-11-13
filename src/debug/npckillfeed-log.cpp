#include "debug/npckillfeed-log.h"

#include <cstdarg>
#include <cstdio>

#include "debug/debug-log.h"

void NpcKillfeedLog::init()
{
}

void NpcKillfeedLog::log(const char* fmt, ...)
{
    if (!fmt) return;
    char body[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(body, sizeof(body), fmt, args);
    va_end(args);
    Debug::log(Debug::Category::NpcCombat, "[NPC KILLFEED] %s\n", body);
}
