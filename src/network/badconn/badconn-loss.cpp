// 07 31 2026, 15 30
/* purpose
* Implements the per-packet packet-loss roll.
* Rolls a fresh loss probability per packet from the preset range.
* Does NOT own queues, packet classification, or the simulator state.
*/

#include "network/badconn/badconn-internal.h"

namespace badconn {

bool badConnRollLoss(const BadConnLoss& block, BadConnRng& rng)
{
    if (!block.enabled)
        return false;
    const float probability =
        (block.minPercent + (block.maxPercent - block.minPercent) * rng.nextUnit()) / 100.0f;
    return rng.nextUnit() < probability;
}

} // namespace badconn
