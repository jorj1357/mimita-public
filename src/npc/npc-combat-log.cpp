// 08 09 2026, 12 45
/* purpose
* Always-on structured log for NPC combat behavior, written by BOTH the client
* and server processes so their numerical views can be compared.
* Writes to logs/<MM-DD-YYYY>/NPC_log_<HHMMSS>.txt (same dated-folder format as
* the main logger). Each line is tagged with the writing process (proc=client|server).
* Does NOT own any gameplay logic; it only appends diagnostic lines.
* Does NOT gate on debug flags - it logs indefinitely, one file per game run.
*/

#include "npc/npc-combat-log.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>

#include "debug/log-manager.h"

namespace {

FILE* gNpcLog = nullptr;
std::string gNpcProc = "unknown";

const char* wallClock()
{
    static char buf[32];
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_s(&tm, &t);
    std::strftime(buf, sizeof(buf), "%H:%M:%S", &tm);
    return buf;
}

void ensureOpen()
{
    if (gNpcLog)
        return;
    std::string path = LogManager::managedFilePath("NPC");
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    gNpcLog = fopen(path.c_str(), "a");
    if (!gNpcLog)
        return;
    fprintf(gNpcLog, "=== NPC LOG start ===\n");
    fflush(gNpcLog);
}

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
    ensureOpen();
    if (!gNpcLog)
        return;

    fprintf(gNpcLog, "[%s] proc=%s ", wallClock(), gNpcProc.c_str());

    va_list args;
    va_start(args, fmt);
    vfprintf(gNpcLog, fmt, args);
    va_end(args);

    fprintf(gNpcLog, "\n");
    fflush(gNpcLog);
}
