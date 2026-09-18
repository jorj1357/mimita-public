// 09 17 2026
/* purpose
* Always-on structured log for NPC combat behavior, emitted by BOTH the client
* and server processes so their numerical views can be compared.
* Each message becomes one NPC event in the single authoritative events.jsonl
* stream; the writing process is tagged via LiveIdentity, so client and server
* lines never need separate files.
* Does NOT own any gameplay logic; it only emits diagnostic events.
* Does NOT gate on debug flags - it logs indefinitely.
*/

#include "npc/npc-combat-log.h"

#include <cstdarg>
#include <cstdio>
#include <string>

#include "debug/structured-log.h"

namespace {

std::string gNpcProc = "unknown";

} // namespace

void npcLogSetProc(const char* proc)
{
    if (proc)
        gNpcProc = proc;
}

const char* npcLogProc()
{
    return gNpcProc.c_str();
}

void npcLog(const char* fmt, ...)
{
    if (!fmt)
        return;

    char message[2048] = {};
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    debug::Event ev;
    ev.category = "NPC";
    ev.name = "npc.combat";
    ev.level = debug::Level::Info;
    ev.message = message;
    ev.fields = {{"proc", gNpcProc}};
    ev.sourceFile = "npc-combat-log.cpp";
    ev.functionName = "npcLog";
    ev.aggregationKey = "NPC:npc.combat";
    debug::logEvent(ev);
}
