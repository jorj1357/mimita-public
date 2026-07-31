// 07 31 2026, 15 30
/* purpose
* Implements the blackout state machine for simulated total packet silence.
* Starts blackouts probabilistically per tick, respecting cooldown and duration.
* Keeps blackout purely directional (in, out, or both) without transport teardown.
* Does NOT own queues, packet classification, or the simulator state.
*/

#include "network/badconn/badconn-internal.h"

#include <cmath>

namespace badconn {

void badConnTickBlackout(BadConnBlackoutState& state, const BadConnBlackout& block,
                         BadConnRng& rng, uint64_t now, bool& started, bool& ended)
{
    started = false;
    ended = false;
    if (!block.enabled)
    {
        state.active = false;
        return;
    }

    if (state.active)
    {
        if (now >= state.endMs)
        {
            state.active = false;
            state.lastEndMs = now;
            ended = true;
        }
        return;
    }

    if (block.startProbabilityPerSecond <= 0.0)
        return;
    if (block.cooldownMs > 0 && now - state.lastEndMs < static_cast<uint64_t>(block.cooldownMs))
        return;

    // Approximate per-tick probability from the per-second rate (60 Hz assumption).
    const double dtSeconds = 1.0 / 60.0;
    const double tickProbability =
        1.0 - std::pow(1.0 - block.startProbabilityPerSecond, dtSeconds);
    if (rng.nextUnit() >= tickProbability)
        return;

    state.active = true;
    state.endMs = now + static_cast<uint64_t>(rng.nextInt(block.minMs, block.maxMs));
    started = true;
}

} // namespace badconn
