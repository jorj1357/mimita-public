// 09 23 2026
/* purpose
* Generic EXE-owned historical-state lookup for lag compensation. Extracts the
* raw bracketing samples from a player's or NPC's bounded history so a hot
* `history.select` policy owns the interpolate/nearest/clamp decision.
* Does NOT own selection policy, collision, or the socket.
*/
#pragma once

#include <cstdint>

#include "hot-reload/hot-history.h"

namespace MimitaNet {

// Fill `out` with the EXE-owned bracketing samples for (domain, entityId, tick).
// Returns true when the store exists (even if only one side is present).
bool serverHistoryRawSamples(std::uint32_t domain, std::uint64_t entityId,
                             std::uint32_t targetTick,
                             GameHistorySelectV1& out);

// Kernel capability implementation for `history.query`: gets raw samples, asks
// the hot `history.select` policy for the decision, and writes the query result.
std::uint32_t serverHistoryQuery(void* host, GameHistoryQueryV1* query);

} // namespace MimitaNet
