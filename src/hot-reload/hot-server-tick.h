// 09 23 2026
/* purpose
* Define the generic, hot-replaceable server fixed-tick orchestration policy and
* the ONE implementation shared by the cold EXE fallback and the hot provider.
* The EXE owns the loop, sockets, threads, persistent state, and the load-bearing
* call sequence; a hot module owns the orchestration decisions (catch-up cap,
* snapshot cadence, phase gates, shutdown request, diagnostics).
* POD only: no STL, ServerPlayer, sockets, or engine objects cross the boundary.
* Does NOT own the loop, transport, or match state.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

namespace MimitaNet {

static constexpr std::uint64_t GAME_SIG_SERVER_TICK =
    gameHash("sig.network.tick.v1");

namespace HotServerTickImpl {

// Preserve the current cold orchestration exactly. A hot provider may override
// any field; `handled = 1` means the cold loop applies the returned values.
inline void evaluate(GameServerTickV1& r)
{
    r.outCatchupSteps = 0u;                     // 0 = leave the cold cap
    r.outRequestShutdown = r.autoExitDue ? 1u : 0u;
    r.outSnapshotDue = r.snapshotDue ? 1u : 0u;
    r.outRunGameplayDomain = r.gameplayDomainDue ? 1u : 0u;
    r.outRunPostMovement = 1u;
    r.outAdvanceGeneration = 1u;
    r.handled = 1u;
    r.result = 1u;
    if (r.reason[0] == '\0')
        r.reason[0] = '\0';
}

} // namespace HotServerTickImpl

} // namespace MimitaNet
