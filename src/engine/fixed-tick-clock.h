// 2026-11-13
/* purpose
* Own the fixed 60 Hz simulation accumulator for one game generation.
* Converts per-frame delta seconds into bounded whole simulation ticks and a
* presentation alpha, so tick/render orchestration has one clock owner.
* Does NOT run simulation, own GL state, or decide catch-up policy.
*/
#pragma once

class FixedTickClock
{
public:
    static constexpr double TICK_HZ = 60.0;
    static constexpr double TICK_DT = 1.0 / 60.0;

    // Add one frame's delta, clamped so a stall cannot queue more than
    // maxCatchupTicks of simulation time.
    void advance(double deltaSeconds, int maxCatchupTicks);

    // True when at least one whole tick is pending.
    bool due() const { return mAccumulator >= TICK_DT; }

    // Consume exactly one tick after running it.
    void consume() { mAccumulator -= TICK_DT; }

    // Fraction of a tick remaining, for presentation interpolation.
    double alpha() const { return mAccumulator / TICK_DT; }

    double accumulator() const { return mAccumulator; }

    void reset() { mAccumulator = 0.0; }

private:
    double mAccumulator = 0.0;
};
