// 07 31 2026, 15 30
/* purpose
* Implements the per-packet reordering roll and hold-time selection.
* Rolls a fresh reorder probability per packet from the preset range.
* Binds the reorder hold to the preset window so delay stays bounded.
* Does NOT own queues, packet classification, or the simulator state.
*/

#include "network/badconn/badconn-internal.h"

#include <algorithm>

namespace badconn {

bool badConnRollReorder(const BadConnReorder& block, BadConnRng& rng)
{
    if (!block.enabled)
        return false;
    const float probability =
        (block.minPercent + (block.maxPercent - block.minPercent) * rng.nextUnit()) / 100.0f;
    return rng.nextUnit() < probability;
}

int badConnReorderDelayMs(const BadConnReorder& block)
{
    return std::clamp(block.window * 6, 10, 240);
}

} // namespace badconn
