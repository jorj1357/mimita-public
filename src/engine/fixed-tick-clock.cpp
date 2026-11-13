// 2026-11-13
#include "engine/fixed-tick-clock.h"

#include <algorithm>

void FixedTickClock::advance(double deltaSeconds, int maxCatchupTicks)
{
    if (!(deltaSeconds > 0.0))
        return;
    const double cap = static_cast<double>(std::max(maxCatchupTicks, 1)) * TICK_DT;
    mAccumulator = std::min(mAccumulator + deltaSeconds, cap);
}
