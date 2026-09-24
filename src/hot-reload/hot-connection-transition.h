// 09 23 2026
/* purpose
* Define the generic, hot-replaceable connection state-machine transition policy
* and the ONE implementation shared by the cold EXE fallback and the hot
* provider. The EXE owns the transport, ICE worker, session teardown, and the
* user-visible notifications; a hot module owns the next state/stage, the retry
* cadence, the timeout classification, and the join-stage labels.
* POD only: no STL or engine objects cross the boundary.
* Does NOT own transport, ICE, teardown, or notifications.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

namespace MimitaNet {

static constexpr std::uint64_t GAME_SIG_CONNECTION_TRANSITION =
    gameHash("sig.connection.transition.v1");

namespace HotConnectionTransitionImpl {

// Preserve the current cold behavior: apply a requested state verbatim, advance
// no stage by default, and allow a reconnect attempt while under the cap and a
// token exists. A hot provider overrides any field.
inline void evaluate(GameConnectionTransitionV1& r)
{
    r.outState = r.requestedState != 0u ? r.requestedState : r.currentState;
    r.outStage = r.stage;
    r.outAction = GAME_CONNECTION_ACTION_NONE;
    r.outRetry = (r.reconnectTokenPresent && r.attempt < r.maxAttempts) ? 1u : 0u;
    r.outBackoffMs = 0u;
    r.handled = 1u;
    r.result = 1u;
    if (r.outMessage[0] == '\0')
        r.outMessage[0] = '\0';
}

} // namespace HotConnectionTransitionImpl

} // namespace MimitaNet
