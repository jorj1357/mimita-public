// 09 24 2026
/* purpose
* Define the generic, hot-replaceable NPC movement/facing intent policy and the
* ONE implementation shared by the cold EXE fallback and the hot provider. The
* EXE owns entity storage, sensors, config, and applying the result; a hot module
* owns the facing-mode timer, desired facing, turn-speed limiting, and the final
* MovementIntent/AimIntent. This is migration Phase 5a: NPC facing/aim intent is
* behavior, not mechanism.
* POD only: no STL or engine objects cross the boundary.
* Does NOT own navigation, combat, the loop, transport, or damage.
*/
#pragma once

#include <cmath>
#include <cstdint>

#include "hot-reload/game-api.h"

namespace MimitaNet {

namespace HotNpcIntentImpl {

// LCG matching src/npc/npc-spawn.cpp random01 byte-for-byte so the fallback and
// the hot provider consume the same RNG sequence.
inline float random01(std::uint32_t& state)
{
    state = state * 1664525u + 1013904223u;
    return static_cast<float>((state >> 8) & 0x00ffffffu) /
           static_cast<float>(0x01000000u);
}

// v with z zeroed and normalized, or the fallback when degenerate.
inline void planarNormal(const float v[3], const float fallback[3], float out[3])
{
    out[0] = v[0];
    out[1] = v[1];
    out[2] = 0.0f;
    const float len = std::sqrt(out[0] * out[0] + out[1] * out[1]);
    if (len < 0.0001f) {
        out[0] = fallback[0];
        out[1] = fallback[1];
        out[2] = 0.0f;
        return;
    }
    out[0] /= len;
    out[1] /= len;
}

inline void evaluate(NpcIntentPolicyV1& r)
{
    r.handled = 1u;
    r.result = 1u;

    // Facing-mode timing: switch between aim-at-target (dominant) and
    // face-movement (brief). Airborne/grounded is irrelevant.
    if (r.aimAtTargetMax <= 0.0f) {
        r.facingTargetMode = 1u;
    } else if (r.facingModeTimer <= 0.0f) {
        r.facingTargetMode = r.facingTargetMode ? 0u : 1u;
        if (r.facingTargetMode) {
            const float minT = r.aimAtTargetMin;
            const float maxT = r.aimAtTargetMax > minT ? r.aimAtTargetMax : minT;
            r.facingModeTimer = minT + random01(r.rngState) * (maxT - minT);
        } else {
            const float minT = r.faceMovementMin;
            const float maxT = r.faceMovementMax > minT ? r.faceMovementMax : minT;
            r.facingModeTimer = minT + random01(r.rngState) * (maxT - minT);
        }
    } else {
        r.facingModeTimer -= r.dt;
    }

    const float fallbackFwd[3] = {1.0f, 0.0f, 0.0f};
    float desired[3];
    const float eyeZ = r.pos[2] + 0.8f;

    auto aimAtTarget = [&](float out[3]) {
        const float toTarget[3] = {
            r.targetPos[0] - r.pos[0],
            r.targetPos[1] - r.pos[1],
            (r.targetPos[2] + 0.8f) - eyeZ};
        const float len = std::sqrt(toTarget[0] * toTarget[0] +
                                    toTarget[1] * toTarget[1] +
                                    toTarget[2] * toTarget[2]);
        float aim[3];
        if (len > 0.001f) {
            aim[0] = toTarget[0] / len;
            aim[1] = toTarget[1] / len;
            aim[2] = toTarget[2] / len;
        } else {
            aim[0] = 1.0f;
            aim[1] = 0.0f;
            aim[2] = 0.0f;
        }
        planarNormal(aim, fallbackFwd, out);
    };

    if (r.facingTargetMode && r.hasTarget) {
        aimAtTarget(desired);
    } else if (r.movementPressed) {
        const float move[3] = {r.rawMoveX, r.rawMoveY, 0.0f};
        planarNormal(move, fallbackFwd, desired);
    } else if (r.hasTarget) {
        aimAtTarget(desired);
    } else {
        desired[0] = r.currentFacing[0];
        desired[1] = r.currentFacing[1];
        desired[2] = 0.0f;
    }

    // Turn-speed limiting: rotate currentFacing toward desiredFwd.
    const float maxTurnAngle = r.turnSpeed * r.dt;
    float dot = r.currentFacing[0] * desired[0] +
                r.currentFacing[1] * desired[1] +
                r.currentFacing[2] * desired[2];
    if (dot < -1.0f) dot = -1.0f;
    if (dot > 1.0f) dot = 1.0f;
    const float angleDiff = std::acos(dot) * 57.2957795f;
    float facing[3];
    if (angleDiff > maxTurnAngle && maxTurnAngle > 0.0f) {
        const float t = maxTurnAngle / angleDiff;
        facing[0] = r.currentFacing[0] + (desired[0] - r.currentFacing[0]) * t;
        facing[1] = r.currentFacing[1] + (desired[1] - r.currentFacing[1]) * t;
        facing[2] = r.currentFacing[2] + (desired[2] - r.currentFacing[2]) * t;
        const float len = std::sqrt(facing[0] * facing[0] +
                                    facing[1] * facing[1] +
                                    facing[2] * facing[2]);
        if (len > 0.0001f) {
            facing[0] /= len;
            facing[1] /= len;
            facing[2] /= len;
        }
    } else {
        facing[0] = desired[0];
        facing[1] = desired[1];
        facing[2] = desired[2];
    }
    r.outFacing[0] = facing[0];
    r.outFacing[1] = facing[1];
    r.outFacing[2] = facing[2];
    r.outYaw = std::atan2(facing[1], facing[0]) * 57.2957795f;

    // Movement intent passthrough (phase 5a owns aiming; move vector unchanged).
    r.outMoveX = r.rawMoveX;
    r.outMoveY = r.rawMoveY;
    r.outMovementPressed = r.movementPressed;
    r.outJump = r.jump;
    r.outDash = r.dash;
    r.outDownDash = r.downDash;
}

} // namespace HotNpcIntentImpl

} // namespace MimitaNet
