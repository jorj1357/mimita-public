// 09 12 2026
/* purpose
* Implements the per-process live-code identity.
* Does NOT own the journal or notifications.
*/
#include "live-code/live-identity.h"

#include <windows.h>

namespace {

const char* gProcess = "client";
std::uint64_t gSessionId = 0;
std::uint64_t gSimulationTick = 0;

} // namespace

namespace LiveIdentity {

void setProcess(const char* processName)
{
    gProcess = (processName && *processName) ? processName : "client";
}

const char* process()
{
    return gProcess;
}

int pid()
{
    return (int)GetCurrentProcessId();
}

void setSessionId(std::uint64_t sessionId)
{
    gSessionId = sessionId;
}

std::uint64_t sessionId()
{
    return gSessionId;
}

void setSimulationTick(std::uint64_t tick)
{
    gSimulationTick = tick;
}

std::uint64_t simulationTick()
{
    return gSimulationTick;
}

} // namespace LiveIdentity
