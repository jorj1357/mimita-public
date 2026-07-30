// 07 30 2026, 13 26
/* purpose
* Provides a lightweight client-side fixed-step UI tick clock (60 Hz).
* Uses monotonic time and an accumulator so UI timing is independent of render FPS.
* Drives chat fade, notification timers, cursor blink, and other deterministic UI timing.
* Does NOT run heavy gameplay simulation, own GL state, or replace the simulation tick clock.
*/
#pragma once

#include <cstdint>
#include <chrono>

class UiTickClock
{
public:
    static constexpr uint64_t TICK_HZ = 60;
    static constexpr double TICK_DT = 1.0 / 60.0;

    UiTickClock();

    // Call once per frame. Returns number of UI ticks advanced this call (usually 0 or 1).
    uint64_t tick(uint64_t maxSteps = 5);

    uint64_t getTick() const { return mTick; }
    uint64_t getElapsedTicks(uint64_t sinceTick) const;

    // Seconds since clock was created (monotonic)
    double getElapsedSeconds() const;

private:
    uint64_t mTick = 0;
    double mAccumulator = 0.0;
    std::chrono::steady_clock::time_point mLastTime;
};
