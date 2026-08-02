// 07 31 2026, 15 30
/* purpose
* Implements the per-packet packet-loss roll with optional burst behavior.
* Rolls a fresh loss probability per packet from the preset range, and
* occasionally enters a short high-loss burst (bad wifi) when configured.
* Does NOT own queues, packet classification, or the simulator state.
*/

#include "network/badconn/badconn-internal.h"

namespace badconn {

bool badConnRollLoss(const BadConnLoss& block, BadConnRng& rng,
                     BadConnBurstState& burst)
{
    if (!block.enabled)
        return false;

    // Occasionally enter a short burst of high loss (bad wifi/interference).
    if (!burst.inBurst && block.burstProbability > 0.0f &&
        rng.nextUnit() < block.burstProbability)
    {
        burst.inBurst = true;
        burst.packetsRemaining = rng.nextInt(2, 6);
    }

    float probability;
    if (burst.inBurst)
    {
        probability = block.burstPercent / 100.0f;
        if (--burst.packetsRemaining <= 0)
            burst.inBurst = false;
    }
    else
    {
        probability =
            (block.minPercent +
             (block.maxPercent - block.minPercent) * rng.nextUnit()) / 100.0f;
    }
    return rng.nextUnit() < probability;
}

} // namespace badconn
