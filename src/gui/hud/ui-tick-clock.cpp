// 07 30 2026, 13 26
/* purpose
* Implements the fixed-step 60 Hz UI tick clock.
* Uses steady_clock for monotonic time and accumulator-based stepping.
* Does NOT own rendering state, gameplay tick, or any non-UI timing.
*/
#include "ui-tick-clock.h"
#include <algorithm>

UiTickClock::UiTickClock()
    : mTick(0)
    , mAccumulator(0.0)
    , mLastTime(std::chrono::steady_clock::now())
{
}

uint64_t UiTickClock::tick(uint64_t maxSteps)
{
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - mLastTime).count();
    mLastTime = now;

    if (elapsed > 0.1)
        elapsed = 0.1;

    mAccumulator += elapsed;
    uint64_t steps = 0;
    while (mAccumulator >= TICK_DT && steps < maxSteps)
    {
        mAccumulator -= TICK_DT;
        ++mTick;
        ++steps;
    }
    return steps;
}

uint64_t UiTickClock::getElapsedTicks(uint64_t sinceTick) const
{
    return mTick - sinceTick;
}

double UiTickClock::getElapsedSeconds() const
{
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now - mLastTime).count();
}
