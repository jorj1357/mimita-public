// 07 31 2026, 15 30
/* purpose
* Declares the disagreement broadcast rate-limit state and decision helper.
* Tracks the last emitted server tick and decides when a new disagreement may go out.
* Keeps the 60-tick window logic standalone and dependency-free for unit testing.
* Does NOT own packet schemas, config loading, or disagreement effect spawning.
*/

#pragma once

#include <cstdint>

namespace MimitaNet {

struct DisagreementRateLimitState
{
    uint32_t lastEmitTick = 0;
};

// Returns true when a disagreement may be broadcast at this tick and records
// the tick. The first event is always allowed; afterwards at least
// minTicksBetween server ticks must pass (tick subtraction handles wraparound).
inline bool shouldEmitDisagreement(DisagreementRateLimitState& state,
                                   uint32_t tick, uint32_t minTicksBetween)
{
    if (state.lastEmitTick != 0 && tick - state.lastEmitTick < minTicksBetween)
        return false;
    state.lastEmitTick = tick;
    return true;
}

} // namespace MimitaNet
