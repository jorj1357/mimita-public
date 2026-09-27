#include "attack-timeline.h"

void AttackTimeline::reset()
{
    phase_ = Phase::Idle;
    phaseTick_ = 0;
    startupTicks_ = 0;
    activeTicks_ = 0;
    recoveryTicks_ = 0;
}

void AttackTimeline::start(uint32_t startupTicks, uint32_t activeTicks,
                           uint32_t recoveryTicks)
{
    startupTicks_ = startupTicks;
    activeTicks_ = activeTicks;
    recoveryTicks_ = recoveryTicks;
    phase_ = Phase::Startup;
    phaseTick_ = 0;
    enterNextNonEmptyPhase();
}

void AttackTimeline::enterNextNonEmptyPhase()
{
    while (phase_ != Phase::Idle)
    {
        const uint32_t duration =
            phase_ == Phase::Startup ? startupTicks_ :
            phase_ == Phase::Active ? activeTicks_ : recoveryTicks_;
        if (duration != 0)
            return;

        phaseTick_ = 0;
        if (phase_ == Phase::Startup)
            phase_ = Phase::Active;
        else if (phase_ == Phase::Active)
            phase_ = Phase::Recovery;
        else
            phase_ = Phase::Idle;
    }
}

void AttackTimeline::advanceOneTick()
{
    if (phase_ == Phase::Idle)
        return;

    ++phaseTick_;
    const uint32_t duration =
        phase_ == Phase::Startup ? startupTicks_ :
        phase_ == Phase::Active ? activeTicks_ : recoveryTicks_;
    if (phaseTick_ < duration)
        return;

    phaseTick_ = 0;
    if (phase_ == Phase::Startup)
        phase_ = Phase::Active;
    else if (phase_ == Phase::Active)
        phase_ = Phase::Recovery;
    else
        phase_ = Phase::Idle;
    enterNextNonEmptyPhase();
}

