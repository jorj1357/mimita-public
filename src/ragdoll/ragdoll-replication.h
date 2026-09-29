// 09 29 2026
/* purpose
* Own the wire-agnostic limb replication types shared by the client sender, the
* server relay, and remote presentation: a fixed six-limb pose and a small
* two-sample interpolation buffer.
* Header-only and free of engine/transport dependencies so it can be unit tested.
* Does NOT own the solver, the network transport, or rendering.
*/
#pragma once

#include <cstdint>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// Fixed order of replicated limbs. Owner and remote must agree on this index
// to part mapping; the wire carries only the ordered frames.
constexpr int kRagdollLimbCount = 6;

inline const char* ragdollReplicatedPartName(int index)
{
    static const char* const kNames[kRagdollLimbCount] = {
        "torso", "head", "leftArm", "rightArm", "leftLeg", "rightLeg"
    };
    return (index >= 0 && index < kRagdollLimbCount) ? kNames[index] : "";
}

// Modes mirrored on the wire so a receiver can choose presentation. Keep in
// sync with AimBodyConfig / ragdoll mode.
enum RagdollReplicationMode : uint8_t {
    RAGDOLL_NET_OFF = 0,
    RAGDOLL_NET_RAGDOLL = 1,
    RAGDOLL_NET_PHYSICAL = 2,
    RAGDOLL_NET_HYBRID = 3,
};

struct RagdollLimbState {
    glm::vec3 position{0.0f};
    glm::quat orientation{1.0f, 0.0f, 0.0f, 0.0f};
    // Damage hitbox for this limb, computed by the owner with the exact same
    // formula the client uses for hit detection (world AABB center + node-local
    // half). The server validates against these so a shot at a visually-posed
    // limb registers on that limb, not the static default pose.
    glm::vec3 hitCenter{0.0f};
    glm::vec3 hitHalf{0.0f};
};

// Maps a replicated limb index to a damage body part (matches
// WeaponExecution::HitBodyPart: 0 torso, 1 head, 2 leg), or -1 for none.
enum RagdollReplicatedBodyPart : int {
    RAGDOLL_BP_TORSO = 0,
    RAGDOLL_BP_HEAD = 1,
    RAGDOLL_BP_LEG = 2,
};

inline int ragdollReplicatedBodyPart(int limbIndex)
{
    if (limbIndex == 1) return RAGDOLL_BP_HEAD;
    if (limbIndex == 4 || limbIndex == 5) return RAGDOLL_BP_LEG;
    return RAGDOLL_BP_TORSO;
}

// Absolute world pose of each replicated limb (canonical body frame).
struct RagdollReplicationPose {
    bool active = false;
    uint8_t mode = RAGDOLL_NET_OFF;
    uint32_t sourceTick = 0;
    uint8_t count = 0;
    RagdollLimbState limbs[kRagdollLimbCount];
};

inline RagdollReplicationPose interpolateReplicatedPose(
    const RagdollReplicationPose& a,
    const RagdollReplicationPose& b,
    float t)
{
    RagdollReplicationPose out = b;
    out.active = a.active || b.active;
    out.count = b.count;
    for (int i = 0; i < out.count && i < kRagdollLimbCount; ++i) {
        out.limbs[i].position =
            glm::mix(a.limbs[i].position, b.limbs[i].position, t);
        out.limbs[i].orientation = glm::normalize(
            glm::slerp(a.limbs[i].orientation, b.limbs[i].orientation, t));
        out.limbs[i].hitCenter =
            glm::mix(a.limbs[i].hitCenter, b.limbs[i].hitCenter, t);
        out.limbs[i].hitHalf =
            glm::mix(a.limbs[i].hitHalf, b.limbs[i].hitHalf, t);
    }
    return out;
}

// Two-sample buffer with a render delay. Poses arrive at a low rate; the
// renderer samples behind real time so it always sits between two frames.
struct RagdollReplicationState {
    bool hasData = false;
    bool active = false;
    RagdollReplicationPose prev;
    RagdollReplicationPose target;
    double prevMs = 0.0;
    double targetMs = 0.0;

    void push(const RagdollReplicationPose& pose, double nowMs)
    {
        if (!hasData) {
            prev = pose;
            target = pose;
            prevMs = nowMs;
            targetMs = nowMs;
            hasData = true;
            active = pose.active;
            return;
        }
        prev = target;
        prevMs = targetMs;
        target = pose;
        targetMs = nowMs;
        active = pose.active;
    }

    // Sample the pose at (nowMs - delaySeconds). Returns false when there is no
    // data or the stream is inactive.
    bool sample(double nowMs, double delaySeconds, RagdollReplicationPose& out) const
    {
        if (!hasData || !active)
            return false;

        const double t = nowMs - delaySeconds * 1000.0;
        if (t <= prevMs || targetMs <= prevMs) {
            out = target;
        } else if (t >= targetMs) {
            out = target;
        } else {
            const float alpha =
                (float)((t - prevMs) / (targetMs - prevMs));
            out = interpolateReplicatedPose(prev, target, alpha);
        }
        out.active = true;
        return true;
    }
};
