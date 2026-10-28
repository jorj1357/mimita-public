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
    bool crouching, bool moving)
{
    if (!grounded)
        return verticalSpeed > 1.0f ? AnimationMovementState::Jump
                                    : AnimationMovementState::Fall;
    if (crouching)
        return moving ? AnimationMovementState::CrouchWalk
                      : AnimationMovementState::Crouch;
    if (moving)
        return horizontalSpeed > 12.0f ? AnimationMovementState::Run
                                       : AnimationMovementState::Walk;
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
