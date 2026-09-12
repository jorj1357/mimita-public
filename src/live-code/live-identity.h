// 09 12 2026
/* purpose
* Own the process identity stamped on every live-code journal event and
* notification: process side (client/server), PID, and a run session id.
* Does NOT own the journal, notifications, or the hot loader.
*/
#pragma once

#include <cstdint>

namespace LiveIdentity {

// "client", "server", or "shared".
void setProcess(const char* processName);
const char* process();

int pid();

void setSessionId(std::uint64_t sessionId);
std::uint64_t sessionId();

// Last known simulation tick for this process, used in notifications.
void setSimulationTick(std::uint64_t tick);
std::uint64_t simulationTick();

} // namespace LiveIdentity
