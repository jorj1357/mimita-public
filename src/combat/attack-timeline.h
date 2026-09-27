#pragma once

#include <cstdint>

// Fixed-tick timing only. Collision, damage, target tracking, and presentation
// deliberately remain owned by their existing systems.
class AttackTimeline
{
public:
    enum class Phase : uint8_t { Idle, Startup, Active, Recovery };

    void start(uint32_t startupTicks, uint32_t activeTicks, uint32_t recoveryTicks);
    void advanceOneTick();
    void reset();

    Phase phase() const { return phase_; }
    uint32_t phaseTick() const { return phaseTick_; }
    uint32_t startupTicks() const { return startupTicks_; }
    uint32_t activeTicks() const { return activeTicks_; }
    uint32_t recoveryTicks() const { return recoveryTicks_; }
    bool isActive() const { return phase_ == Phase::Active; }
    bool isCommitted() const { return phase_ != Phase::Idle; }
    bool isFinished() const { return phase_ == Phase::Idle; }

private:
    void enterNextNonEmptyPhase();

    Phase phase_ = Phase::Idle;
    uint32_t phaseTick_ = 0;
    uint32_t startupTicks_ = 0;
    uint32_t activeTicks_ = 0;
    uint32_t recoveryTicks_ = 0;
};

