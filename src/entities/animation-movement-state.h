#pragma once

#include <cstdint>

enum class AnimationMovementState : std::uint8_t {
    Idle,
    Walk,
    Run,
    Crouch,
    CrouchWalk,
    Jump,
    Fall
};

inline AnimationMovementState resolveAnimationMovementState(
    float horizontalSpeed, float verticalSpeed, bool grounded,
    bool crouching, bool moving, bool preferRun = false)
{
    if (!grounded)
        return verticalSpeed > 1.0f ? AnimationMovementState::Jump
                                    : AnimationMovementState::Fall;
    if (crouching)
        return moving ? AnimationMovementState::CrouchWalk
                      : AnimationMovementState::Crouch;
    if (moving) {
        // A tiny hysteresis band stops walk/run from flapping every tick when
        // an actor's speed sits on the threshold. Without it, each crossing
        // restarts the clip at tick 0 and looks like a persistent stutter.
        constexpr float kRunEnterSpeed = 12.0f;
        constexpr float kRunExitSpeed = 10.0f;
        return (horizontalSpeed > kRunEnterSpeed ||
                (preferRun && horizontalSpeed > kRunExitSpeed))
            ? AnimationMovementState::Run
            : AnimationMovementState::Walk;
    }
    return AnimationMovementState::Idle;
}

inline const char* animationMovementStateName(AnimationMovementState state)
{
    switch (state) {
        case AnimationMovementState::Idle: return "idle";
        case AnimationMovementState::Walk: return "walk";
        case AnimationMovementState::Run: return "run";
        case AnimationMovementState::Crouch: return "crouch";
        case AnimationMovementState::CrouchWalk: return "crouchWalk";
        case AnimationMovementState::Jump: return "jump";
        case AnimationMovementState::Fall: return "fall";
    }
    return "idle";
}
