// 09 23 2026
/* purpose
* The afad20a animation contract, restored as a hot C++ evaluator.
*
* afad20a had exactly three clip states (idle, walk, return_to_idle) plus pose
* OVERLAYS (dash, freeze) and weapon arm poses. Dash and freeze were not clips:
* they blended a fixed pose over the locomotion base with a small timer
* (snapIn / blendIn / hold / blendOut). Each part was then eased toward the
* target by an exact semi-implicit spring (translation 90/16, rotation 80/14),
* and idle added a procedural sway.
*
* This header owns that math and its per-entity state so the live pose can be
* produced from C++ (locomotionSource: "cpp") while the JSON keyframe sampler
* remains selectable. It is plain data in/out and publishes nothing by itself.
* Hot-only header. Does NOT link into the EXE.
*/
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "hot-reload/game-api.h"
#include "hot-reload/hot-animation-clips.h"

namespace Afad20a {

// Per-entity persistent animation state (spring value/velocity + overlay
// timers). Local-only; reconstructed deterministically from the same facts.
static constexpr std::uint64_t HOT_AFAD_STATE_COMPONENT = gameHash("AfadAnimState");
static constexpr std::uint32_t HOT_AFAD_STATE_VERSION = 1;

struct State {
    std::uint32_t version;
    std::uint32_t reserved;
    std::uint64_t currentClip;      // gameHash("idle"/"walk"/"return_to_idle")
    float animStateTime;            // seconds into the clip
    float weaponSwayTime;           // free-running clock for idle sway
    float dashPoseTimer;            // < 0 = inactive
    float freezePoseTimer;          // < 0 = inactive
    std::uint32_t prevFreezeActive;
    float aimBodyPitch;
    float springTransValue[HotAnim::PartCount][3];
    float springTransVel[HotAnim::PartCount][3];
    float springRotValue[HotAnim::PartCount][3];
    float springRotVel[HotAnim::PartCount][3];
};

struct PartPose {
    float trans[3];
    float rot[3];
};

struct Overlay {
    PartPose part[HotAnim::PartCount];
    float blendInTime;
    float blendOutTime;
    std::uint8_t snapIn;
};

inline float smoothstep01(float t)
{
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return t * t * (3.0f - 2.0f * t);
}

// afad20a dash pose (config/player-procedural.json dashPose at afad20a).
inline const Overlay& dashPose()
{
    static const Overlay ov = [] {
        Overlay o{};
        o.part[HotAnim::PartTorso].rot[0] = 15.0f; o.part[HotAnim::PartTorso].rot[2] = -5.0f;
        o.part[HotAnim::PartTorso].trans[0] = 0.25f;
        o.part[HotAnim::PartHead].rot[2] = -15.0f;
        o.part[HotAnim::PartHead].trans[0] = 0.48f; o.part[HotAnim::PartHead].trans[1] = -0.1f;
        o.part[HotAnim::PartLeftArm].rot[2] = 60.0f;
        o.part[HotAnim::PartLeftArm].trans[0] = 0.6f;
        o.part[HotAnim::PartRightArm].rot[2] = -80.0f;
        o.part[HotAnim::PartRightArm].trans[0] = 0.5f; o.part[HotAnim::PartRightArm].trans[1] = -0.2f;
        o.part[HotAnim::PartRightArm].trans[2] = 0.1f;
        o.part[HotAnim::PartLeftLeg].rot[2] = -85.0f;
        o.part[HotAnim::PartRightLeg].rot[2] = 75.0f;
        o.blendInTime = 0.01f;
        o.blendOutTime = 0.1f;
        o.snapIn = 1;
        return o;
    }();
    return ov;
}

// afad20a freeze pose (config/player-procedural.json freezePose at afad20a).
inline const Overlay& freezePose()
{
    static const Overlay ov = [] {
        Overlay o{};
        o.part[HotAnim::PartTorso].rot[0] = 18.0f; o.part[HotAnim::PartTorso].rot[2] = -10.0f;
        o.part[HotAnim::PartTorso].trans[0] = 0.15f; o.part[HotAnim::PartTorso].trans[1] = -0.25f;
        o.part[HotAnim::PartHead].rot[0] = -20.0f; o.part[HotAnim::PartHead].rot[2] = -35.0f;
        o.part[HotAnim::PartHead].trans[0] = 0.3f; o.part[HotAnim::PartHead].trans[1] = -0.4f;
        o.part[HotAnim::PartHead].trans[2] = 0.2f;
        o.part[HotAnim::PartLeftArm].rot[0] = -20.0f; o.part[HotAnim::PartLeftArm].rot[1] = 20.0f;
        o.part[HotAnim::PartLeftArm].rot[2] = 5.0f;
        o.part[HotAnim::PartLeftArm].trans[0] = 0.48f; o.part[HotAnim::PartLeftArm].trans[1] = -0.3f;
        o.part[HotAnim::PartRightArm].rot[0] = -50.0f; o.part[HotAnim::PartRightArm].rot[1] = -15.0f;
        o.part[HotAnim::PartRightArm].rot[2] = -25.0f;
        o.part[HotAnim::PartRightArm].trans[0] = 0.12f; o.part[HotAnim::PartRightArm].trans[1] = -0.4f;
        o.part[HotAnim::PartLeftLeg].rot[0] = -22.0f; o.part[HotAnim::PartLeftLeg].rot[2] = 30.0f;
        o.part[HotAnim::PartLeftLeg].trans[0] = 0.2f; o.part[HotAnim::PartLeftLeg].trans[1] = -0.3f;
        o.part[HotAnim::PartLeftLeg].trans[2] = -0.1f;
        o.part[HotAnim::PartRightLeg].rot[0] = -12.0f; o.part[HotAnim::PartRightLeg].rot[2] = -30.0f;
        o.part[HotAnim::PartRightLeg].trans[0] = 0.1f; o.part[HotAnim::PartRightLeg].trans[1] = -0.3f;
        o.part[HotAnim::PartRightLeg].trans[2] = 0.1f;
        o.blendInTime = 0.0f;
        o.blendOutTime = 0.0f;
        o.snapIn = 1;
        return o;
    }();
    return ov;
}

// afad20a idle procedural sway (config/player-procedural.json idle + strength).
struct IdleParams {
    float armRotationDeg = 1.0f;
    float legRotationDeg = 1.0f;
    float torsoRotationDeg = 2.0f;
    float headRotationDeg = 1.5f;
    float armSpeed = 0.5f;
    float legSpeed = 0.8f;
    float torsoSpeed = 0.5f;
    float headSpeed = 1.0f;
    float breathingAmount = 0.02f;
    float breathingSpeed = 1.0f;
    float debugStrength = 1.0f;
};
inline const IdleParams& idleParams()
{
    static const IdleParams p;
    return p;
}

inline void applyOverlay(HotAnim::Pose& pose, const Overlay& ov, float weight,
                         std::uint32_t mask)
{
    if (weight <= 0.0f)
        return;
    if (weight > 1.0f)
        weight = 1.0f;
    for (std::uint32_t p = 0; p < HotAnim::PartCount; ++p) {
        if ((mask & (1u << p)) == 0)
            continue;
        for (int k = 0; k < 3; ++k) {
            pose.part[p].trans[k] += (ov.part[p].trans[k] - pose.part[p].trans[k]) * weight;
            pose.part[p].rot[k] += (ov.part[p].rot[k] - pose.part[p].rot[k]) * weight;
        }
        pose.mask |= (1u << p);
    }
}

// afad20a dash overlay weight. Returns the blend weight and advances the timer.
inline float dashWeight(State& s, bool didDash, float dt)
{
    const Overlay& dp = dashPose();
    if (didDash && s.dashPoseTimer < 0.0f)
        s.dashPoseTimer = 0.0f;
    float weight = 0.0f;
    if (s.dashPoseTimer >= 0.0f) {
        if (dp.snapIn && s.dashPoseTimer == 0.0f) {
            weight = 1.0f;
            s.dashPoseTimer = dp.blendInTime;
        }
        s.dashPoseTimer += dt;
        const float hold = 0.15f;
        const float total = dp.blendInTime + hold + dp.blendOutTime;
        if (s.dashPoseTimer <= dp.blendInTime && dp.blendInTime > 1e-5f) {
            weight = smoothstep01(s.dashPoseTimer / dp.blendInTime);
        } else if (s.dashPoseTimer <= dp.blendInTime + hold) {
            weight = 1.0f;
        } else if (s.dashPoseTimer <= total && dp.blendOutTime > 1e-5f) {
            const float t = (s.dashPoseTimer - dp.blendInTime - hold) / dp.blendOutTime;
            weight = 1.0f - smoothstep01(t);
        } else {
            weight = 0.0f;
            s.dashPoseTimer = -1.0f;
        }
    }
    return weight;
}

// afad20a freeze overlay weight.
inline float freezeWeight(State& s, bool freezeActive, float dt)
{
    const Overlay& fp = freezePose();
    const bool wasActive = s.prevFreezeActive != 0u;
    s.prevFreezeActive = freezeActive ? 1u : 0u;
    if (freezeActive && !wasActive)
        s.freezePoseTimer = 0.0f;
    else if (!freezeActive && wasActive)
        s.freezePoseTimer = 0.0f;

    float weight = 0.0f;
    if (s.freezePoseTimer >= 0.0f) {
        s.freezePoseTimer += dt;
        if (freezeActive) {
            if (fp.snapIn) {
                weight = 1.0f;
                s.freezePoseTimer = fp.blendInTime;
            } else if (s.freezePoseTimer <= fp.blendInTime && fp.blendInTime > 1e-5f) {
                weight = smoothstep01(s.freezePoseTimer / fp.blendInTime);
            } else {
                weight = 1.0f;
            }
        } else {
            if (s.freezePoseTimer <= fp.blendOutTime && fp.blendOutTime > 1e-5f) {
                weight = 1.0f - smoothstep01(s.freezePoseTimer / fp.blendOutTime);
            } else {
                weight = 0.0f;
                s.freezePoseTimer = -1.0f;
            }
        }
    }
    return weight;
}

inline void applyIdleSway(HotAnim::Pose& pose, float time, bool freezeActive)
{
    const IdleParams& c = idleParams();
    const float str = c.debugStrength;
    const float breathMult = freezeActive ? 0.2f : 1.0f;
    const float armGate = freezeActive ? 0.0f : 1.0f;
    pose.part[HotAnim::PartLeftArm].rot[0] +=
        std::sin(time * c.armSpeed) * c.armRotationDeg * str * armGate;
    pose.part[HotAnim::PartRightArm].rot[0] +=
        std::sin(time * c.armSpeed + 1.5f) * c.armRotationDeg * str * armGate;
    pose.part[HotAnim::PartLeftLeg].rot[0] +=
        std::sin(time * c.legSpeed) * c.legRotationDeg * str * armGate;
    pose.part[HotAnim::PartRightLeg].rot[0] +=
        std::sin(time * c.legSpeed + 2.0f) * c.legRotationDeg * str * armGate;
    pose.part[HotAnim::PartTorso].rot[2] +=
        std::sin(time * c.torsoSpeed) * c.torsoRotationDeg * str;
    pose.part[HotAnim::PartTorso].trans[1] +=
        std::sin(time * c.breathingSpeed) * c.breathingAmount * breathMult * str;
    pose.part[HotAnim::PartHead].rot[0] +=
        std::sin(time * c.headSpeed) * c.headRotationDeg * str;
    pose.mask |= HotAnim::MaskFull;
}

// Exact afad20a springVec3 easing, applied to every masked part. The value is
// the applied pose; the velocity persists across ticks in State.
inline void springStep(State& s, HotAnim::Pose& pose, float dt)
{
    const float safeDt = std::min(dt, 0.05f);
    for (std::uint32_t p = 0; p < HotAnim::PartCount; ++p) {
        if ((pose.mask & (1u << p)) == 0)
            continue;
        for (int k = 0; k < 3; ++k) {
            float accel = (pose.part[p].trans[k] - s.springTransValue[p][k]) * 90.0f -
                          s.springTransVel[p][k] * 16.0f;
            s.springTransVel[p][k] += accel * safeDt;
            s.springTransValue[p][k] += s.springTransVel[p][k] * safeDt;
            pose.part[p].trans[k] = s.springTransValue[p][k];

            accel = (pose.part[p].rot[k] - s.springRotValue[p][k]) * 80.0f -
                    s.springRotVel[p][k] * 14.0f;
            s.springRotVel[p][k] += accel * safeDt;
            s.springRotValue[p][k] += s.springRotVel[p][k] * safeDt;
            pose.part[p].rot[k] = s.springRotValue[p][k];
        }
    }
}

// The afad20a locomotion state machine. `moving` is the input-triggered walk
// flag; `returnToIdleAvailable` is true when the return_to_idle clip exists.
inline void stepState(State& s, bool moving, bool returnToIdleAvailable,
                      float returnDurationSeconds, float dt)
{
    const std::uint64_t kIdle = gameHash("idle");
    const std::uint64_t kWalk = gameHash("walk");
    const std::uint64_t kReturn = gameHash("return_to_idle");
    if (s.currentClip == 0)
        s.currentClip = kIdle;

    std::uint64_t active = s.currentClip;
    if (moving) {
        active = kWalk;
    } else if (s.currentClip == kWalk) {
        active = returnToIdleAvailable ? kReturn : kIdle;
    } else if (s.currentClip == kReturn) {
        if (s.animStateTime >= returnDurationSeconds)
            active = kIdle;
    }

    if (active != s.currentClip) {
        s.currentClip = active;
        s.animStateTime = (active == kWalk) ? (1.0f / 60.0f) : 0.0f;
    }
    s.animStateTime += dt;
}

// Deterministic self-test for the afad20a animation evaluator. Verifies the
// three-state machine, the dash/freeze overlay weights, the exact spring, and
// idle-sway finiteness. A failure rejects the candidate before activation.
inline bool runAfad20aAnimationSelfTest(char* message, std::uint32_t messageSize)
{
    auto fail = [&](const char* m) {
        if (message && messageSize)
            std::snprintf(message, messageSize, "%s", m);
        return false;
    };
    const float dt = 1.0f / 60.0f;
    const std::uint64_t kIdle = gameHash("idle");
    const std::uint64_t kWalk = gameHash("walk");
    const std::uint64_t kReturn = gameHash("return_to_idle");

    State s{};
    s.dashPoseTimer = -1.0f;
    s.freezePoseTimer = -1.0f;
    stepState(s, false, true, 10.0f / 60.0f, dt);
    if (s.currentClip != kIdle)
        return fail("afad state: does not start idle");
    stepState(s, true, true, 10.0f / 60.0f, dt);
    if (s.currentClip != kWalk)
        return fail("afad state: idle -> walk failed");
    stepState(s, false, true, 10.0f / 60.0f, dt);
    if (s.currentClip != kReturn)
        return fail("afad state: walk -> return_to_idle failed");
    for (int i = 0; i < 20; ++i)
        stepState(s, false, true, 10.0f / 60.0f, dt);
    if (s.currentClip != kIdle)
        return fail("afad state: return_to_idle -> idle failed");

    State d{};
    d.dashPoseTimer = -1.0f;
    if (!(dashWeight(d, true, dt) > 0.99f))
        return fail("afad dash: snapIn weight not 1");

    State fz{};
    fz.freezePoseTimer = -1.0f;
    if (!(freezeWeight(fz, true, dt) > 0.99f))
        return fail("afad freeze: active weight not 1");

    State s1{}, s2{};
    HotAnim::Pose p1{}, p2{};
    p1.mask = HotAnim::MaskFull;
    p2.mask = HotAnim::MaskFull;
    p1.part[HotAnim::PartTorso].rot[0] = 30.0f;
    p2.part[HotAnim::PartTorso].rot[0] = 30.0f;
    for (int i = 0; i < 10; ++i) {
        springStep(s1, p1, dt);
        springStep(s2, p2, dt);
    }
    if (!(p1.part[HotAnim::PartTorso].rot[0] > 0.0f))
        return fail("afad spring: did not move toward target");
    if (std::memcmp(&p1, &p2, sizeof(HotAnim::Pose)) != 0)
        return fail("afad spring: not deterministic");

    HotAnim::Pose sw{};
    sw.mask = HotAnim::MaskFull;
    applyIdleSway(sw, 1.234f, false);
    for (std::uint32_t p = 0; p < HotAnim::PartCount; ++p)
        for (int k = 0; k < 3; ++k)
            if (!std::isfinite(sw.part[p].rot[k]) ||
                !std::isfinite(sw.part[p].trans[k]))
                return fail("afad sway: non-finite");

    if (message && messageSize)
        std::snprintf(message, messageSize, "%s", "afad20a animation invariants ok");
    return true;
}

} // namespace Afad20a
