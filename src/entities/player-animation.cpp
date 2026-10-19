#include "player.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "animation-rotation.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-melee-config.h"
#include "config.h"
#include "entities/aimbody-config.h"
#include "entities/player-avatar-animations.h"
#include "physics/config.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "network/net_common.h"
#include "network/community-match-client.h"
using namespace MimitaNet;

struct AnimOverlayResult {
    glm::vec3 translation{0.0f};
    glm::vec3 rotation{0.0f};
    glm::vec3 scale{1.0f};
};

static glm::mat4 poseMatrix(const ProceduralPose& pose)
{
    glm::mat4 m = glm::translate(glm::mat4(1.0f), pose.translation);
    m = glm::rotate(m, glm::radians(pose.rotationEuler.x), glm::vec3(1, 0, 0));
    m = glm::rotate(m, glm::radians(pose.rotationEuler.y), glm::vec3(0, 1, 0));
    m = glm::rotate(m, glm::radians(pose.rotationEuler.z), glm::vec3(0, 0, 1));
    m = glm::scale(m, pose.scale);
    return m;
}

// Per-keyframe interpolation easing (see docs/specs/animations/animations.md).
static float applyAnimEase(float t, AnimInterp interp)
{
    t = std::clamp(t, 0.0f, 1.0f);
    switch (interp) {
        case AnimInterp::Smooth:    return t * t * (3.0f - 2.0f * t);
        case AnimInterp::EaseIn:    return t * t;
        case AnimInterp::EaseOut:   return 1.0f - (1.0f - t) * (1.0f - t);
        case AnimInterp::EaseInOut: return t < 0.5f ? 2.0f * t * t
                                                    : 1.0f - 2.0f * (1.0f - t) * (1.0f - t);
        case AnimInterp::Step:      return 0.0f;
        case AnimInterp::Linear:
        default:                    return t;
    }
}

static float mapRuntimeTickToAuthoredTick(const AnimClip& clip, float runtimeTick)
{
    if (!clip.hasMeleePhaseTimeline)
        return runtimeTick;

    for (std::size_t phase = 0; phase < 3; ++phase) {
        const float runtimeStart = (float)clip.phaseTicks[phase];
        const float runtimeEnd = (float)clip.phaseTicks[phase + 1];
        if (runtimeTick < runtimeEnd || phase == 2) {
            const float sourceStart = (float)clip.phaseSourceTicks[phase];
            const float sourceEnd = (float)clip.phaseSourceTicks[phase + 1];
            const float amount = std::clamp(
                (runtimeTick - runtimeStart) / (runtimeEnd - runtimeStart), 0.0f, 1.0f);
            return sourceStart + (sourceEnd - sourceStart) * amount;
        }
    }
    return runtimeTick;
}

static glm::vec3 springVec3(SpringState& spring, const glm::vec3& target, float stiffness, float damping, float dt)
{
    float safeDt = std::min(dt, 0.05f);
    glm::vec3 acceleration = (target - spring.value) * stiffness - spring.velocity * damping;
    spring.velocity += acceleration * safeDt;
    spring.value += spring.velocity * safeDt;
    return spring.value;
}

static std::unordered_map<std::string, AnimOverlayResult> interpolateAnimClip(
    const AnimClip& clip, float timeSeconds, float speedScale = 1.0f,
    float forcedLoopedTick = -1.0f, float forcedSampleDurationTicks = -1.0f)
{
    std::unordered_map<std::string, AnimOverlayResult> result;
    if (clip.keyframes.empty()) return result;

    float tickTime = timeSeconds * 60.0f * speedScale;
    float duration = forcedLoopedTick >= 0.0f && forcedSampleDurationTicks > 0.0f
        ? forcedSampleDurationTicks : (float)clip.durationTicks;
    if (duration <= 0.0f) duration = 1.0f;

    float loopedTick;
    if (forcedLoopedTick >= 0.0f) {
        // Melee phase sampling: the action owner maps a phase tick to a
        // normalized clip sample; do not advance by time or wrap.
        loopedTick = std::clamp(forcedLoopedTick, 0.0f, duration - 1.0f);
    } else {
        loopedTick = clip.loop ? std::fmod(tickTime, duration)
                               : std::min(tickTime, duration - 1.0f);
        if (loopedTick < 0.0f) loopedTick += duration;
    }

    const AnimKeyframe* prev = &clip.keyframes[0];
    const AnimKeyframe* next = &clip.keyframes[0];

    for (size_t i = 0; i < clip.keyframes.size(); ++i) {
        if ((float)clip.keyframes[i].tick <= loopedTick) prev = &clip.keyframes[i];
        if ((float)clip.keyframes[i].tick >= loopedTick) { next = &clip.keyframes[i]; break; }
    }

    float range;
    float t;

    if (prev == next && clip.loop && clip.keyframes.size() > 1) {
        const AnimKeyframe* last = &clip.keyframes.back();
        prev = last;
        float wrappedDist = (duration - (float)last->tick) + (float)next->tick;
        float currentDist = loopedTick - (float)last->tick;
        if (currentDist < 0.0f) currentDist += duration;
        range = wrappedDist;
        t = (range > 0.001f) ? currentDist / range : 0.0f;
    } else {
        range = (float)(next->tick - prev->tick);
        t = (range > 0.001f) ? (loopedTick - (float)prev->tick) / range : 0.0f;
    }
    t = applyAnimEase(t, prev->interp);

    std::vector<std::string> allParts;
    for (const auto& p : prev->parts) allParts.push_back(p.first);
    for (const auto& p : next->parts) {
        bool found = false;
        for (const auto& existing : allParts) {
            if (existing == p.first) { found = true; break; }
        }
        if (!found) allParts.push_back(p.first);
    }

    for (const auto& partName : allParts) {
        AnimOverlayResult overlay;
        auto prevIt = prev->parts.find(partName);
        auto nextIt = next->parts.find(partName);

        glm::vec3 prevTrans = (prevIt != prev->parts.end()) ? prevIt->second.translation : glm::vec3(0.0f);
        glm::vec3 nextTrans = (nextIt != next->parts.end()) ? nextIt->second.translation : glm::vec3(0.0f);
        glm::quat prevRot = (prevIt != prev->parts.end())
            ? prevIt->second.rotationQuat : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        glm::quat nextRot = (nextIt != next->parts.end())
            ? nextIt->second.rotationQuat : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        glm::vec3 prevScale = (prevIt != prev->parts.end()) ? prevIt->second.scale : glm::vec3(1.0f);
        glm::vec3 nextScale = (nextIt != next->parts.end()) ? nextIt->second.scale : glm::vec3(1.0f);

        overlay.translation = glm::mix(prevTrans, nextTrans, t);
        // Canonical quaternion channels interpolate with shortest-arc SLERP.
        overlay.rotation = AnimRotation::eulerXyzDegreesFromQuat(
            AnimRotation::slerpShortest(prevRot, nextRot, t));
        overlay.scale = glm::mix(prevScale, nextScale, t);
        result[partName] = overlay;
    }

    return result;
}

void Player::playOneShot(const std::string& clip, bool loop)
{
    if (clip.empty())
        return;
    lockedClip = clip;
    lockedClipTime = 0.0f;
    lockedClipLoop = loop;
    lockedClipPhaseTick = 0;
    lockedClipPhaseTicks = 0;
    lockedClipSampleTick = -1;
}

void Player::playOneShotPhase(const std::string& clip, int phaseTick, int phaseTicks)
{
    if (clip.empty() || phaseTicks <= 0)
        return;
    lockedClip = clip;
    lockedClipTime = 0.0f;
    // Phase clips are held until the action owner clears or replaces them; the
    // pose is sampled from the phase progress, not from elapsed time.
    lockedClipLoop = true;
    lockedClipPhaseTick = std::max(0, phaseTick);
    lockedClipPhaseTicks = phaseTicks;
    lockedClipSampleTick = -1;
}

void Player::playOneShotAtTick(const std::string& clip, int clipTick)
{
    if (clip.empty()) return;
    if (lockedClip != clip) lockedClipTime = 0.0f;
    lockedClip = clip;
    lockedClipLoop = true; // The action owner clears it at the exclusive end marker.
    lockedClipPhaseTick = 0;
    lockedClipPhaseTicks = 0;
    lockedClipSampleTick = std::max(0, clipTick);
}

bool Player::usesMeleeAuthoredPose() const
{
    // The Captain Falcon profile is the Melee fighter profile (forced role in
    // config/gamemodes/melee.json), so this avatar identifies a Melee actor.
    // The explicit mode/flag/runtime checks below are kept as intent markers.
    if (avatarName() != "captainfalcon")
        return false;
    if (animationSet && animationSet->valid && animationSet->authoredOnly)
        return true;
    if (MimitaNet::CommunityMatchClient::instance().mode() == "melee")
        return true;
    for (const auto& kv : weaponRuntimes)
        if (WeaponData::MeleeMoveSetRegistry::instance().get(kv.first) != nullptr)
            return true;
    return true;
}

static void applyAxisLocks(ProceduralPose& pose, const std::string& partName)
{
    auto it = gPlayerProcedural.axisLocks.find(partName);
    if (it == gPlayerProcedural.axisLocks.end()) return;
    if (!it->second.x) pose.rotationEuler.x = 0.0f;
    if (!it->second.y) pose.rotationEuler.y = 0.0f;
    if (!it->second.z) pose.rotationEuler.z = 0.0f;
}

void Player::updateProceduralAnimation(float dt, const glm::vec3& camForward, const glm::vec3& camPos, bool movementPressed)
{
    updatePlayerProceduralHotReload(dt);
    AimBodyConfig::instance().pollReload();

    {
        static uint64_t lastPoseProbeMs = 0;
        const uint64_t nowP = nowMs();
        if (nowP - lastPoseProbeMs >= 1000) {
            lastPoseProbeMs = nowP;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Avatar, StructuredLevel::Important,
                "anim.melee.pose_probe", avatarName().empty() ? username : avatarName(),
                "animation pose probe", (uint32_t)movementSimulationTick,
                {{"actor", username}, {"avatar", avatarName()},
                 {"mode", MimitaNet::CommunityMatchClient::instance().mode()},
                 {"authored_only", animationSet ? animationSet->authoredOnly : false},
                 {"set_valid", animationSet ? animationSet->valid : false},
                 {"runtime_count", weaponRuntimes.size()},
                 {"uses_melee_authored", usesMeleeAuthoredPose()},
                 {"crouching", crouching},
                 {"on_ground", ground.onGround},
                 {"stable_on_ground", ground.stableOnGround},
                 {"ragdoll_active", ragdollModeActive},
                 {"procedural_frozen", proceduralFrozen}});
        }
    }

    // Melee exclusive authored-pose policy: when the active mode is Melee and the
    // actor uses the Captain Falcon profile, only that profile's sampled
    // animations.json clips determine the visible rig pose. Procedural/aimbody/
    // weapon/dash/freeze pose layers and ragdoll presentation are suppressed.
    // See docs/specs/gamemodes/supersmashbrosmelee.md §57 and the "Exclusive
    // authored-pose policy" in docs/specs/animations/animations.md.
    const bool meleeExclusivePose = usesMeleeAuthoredPose();

    if (perfectPoseSkeleton.nodes.empty() ||
        perfectPoseSkeleton.restLocalTransforms.size() != perfectPoseSkeleton.nodes.size())
    {
        static uint64_t lastAnimEarlySkelLogMs = 0;
        uint64_t nowSkel = nowMs();
        if (nowSkel - lastAnimEarlySkelLogMs >= 1000)
        {
            Debug::warn(Debug::Category::Animation,
                "[ANIM EARLY RETURN] player=%s reason=invalid-skeleton "
                "modelLoaded=%d nodes=%zu restTransforms=%zu physicalParts=%zu "
                "movementPressed=%d\n",
                username.c_str(), (int)modelLoaded,
                perfectPoseSkeleton.nodes.size(),
                perfectPoseSkeleton.restLocalTransforms.size(),
                physicalBody.parts.size(),
                (int)movementPressed);
            lastAnimEarlySkelLogMs = nowSkel;
        }
        return;
    }
    if (proceduralFrozen && !meleeExclusivePose) {
        updateModelWorldTransforms();
        static uint64_t lastAnimEarlyFrozenLogMs = 0;
        uint64_t nowFrozen = nowMs();
        if (nowFrozen - lastAnimEarlyFrozenLogMs >= 1000)
        {
            Debug::warn(Debug::Category::Animation,
                "[ANIM EARLY RETURN] player=%s reason=procedural-frozen "
                "freezeActive=%d networkStateFlags=0x%04x movementPressed=%d\n",
                username.c_str(), (int)freeze.freezeActive,
                (unsigned)networkStateFlags, (int)movementPressed);
            lastAnimEarlyFrozenLogMs = nowFrozen;
        }
        return;
    }
    if (ragdollModeActive && !meleeExclusivePose) {
        updateModelWorldTransforms();
        return;
    }

    static bool loggedSkeleton = false;
    if (!loggedSkeleton) {
        loggedSkeleton = true;
        Debug::log(Debug::Category::Animation,
            "[ANIM PROFILE] entity=%s single global animation profile (gPlayerProcedural) nodes=%zu bodyParts=%zu\n",
            username.c_str(), perfectPoseSkeleton.nodes.size(), physicalBody.parts.size());
        for (const PhysicalBodyPart& bp : physicalBody.parts) {
            Debug::log(Debug::Category::Animation,
                "[ANIM BONE] %s -> nodeIndex=%d name=%s\n",
                bp.name.c_str(), bp.nodeIndex,
                bp.nodeIndex >= 0 && bp.nodeIndex < (int)perfectPoseSkeleton.nodes.size()
                    ? perfectPoseSkeleton.nodes[bp.nodeIndex].name.c_str() : "INVALID");
        }
    }

    if (glm::length(camForward) > 0.001f) {
        aimDirection = glm::normalize(camForward);
        aimPosition = camPos;
        hasAimData = true;
    }

    syncLegacyStateToLayers();
    weaponSwayTime += dt;

    glm::vec2 planarVel = glm::vec2(vel.x, vel.y);
    float speed = glm::length(planarVel);
    float move01 = std::min(speed / std::max(PHYS.moveSpeed, 0.001f), 1.6f);
    previousProceduralVelocity = vel;

    // === ANIMATION STATE MACHINE ===
    // Resolve the per-avatar rig + animation set (falls back to the global
    // profile in gPlayerProcedural). See docs/specs/animations/animations.md.
    if (avatarName() != mResolvedAnimAvatar) {
        mResolvedAnimAvatar = avatarName();
        animationSet = nullptr;
        rig = AvatarRig{};
        if (!mResolvedAnimAvatar.empty()) {
            animationSet = AvatarAnimationRegistry::instance()
                               .getAnimationSet(mResolvedAnimAvatar,
                                                (std::uint32_t)movementSimulationTick);
            const AvatarRig* r = AvatarAnimationRegistry::instance()
                                     .getRig(mResolvedAnimAvatar);
            if (r) rig = *r;
        }
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Avatar, StructuredLevel::Important,
            "anim.set.bound", mResolvedAnimAvatar,
            "player bound to avatar animation set",
            (uint32_t)movementSimulationTick,
            {{"avatar", mResolvedAnimAvatar},
             {"set_valid", animationSet && animationSet->valid},
             {"clip_count", animationSet ? animationSet->clips.size() : 0},
             {"has_rig", rig.present},
             {"rig_parts", rig.parts.size()},
             {"movement_mode", rig.movement.mode}},
            __FILE__, __LINE__, __FUNCTION__);
    } else if (!mResolvedAnimAvatar.empty()) {
        // pollReload marks changed entries stale; asking again parses and swaps
        // the set while keeping the cache address held by this Player valid.
        animationSet = AvatarAnimationRegistry::instance()
                           .getAnimationSet(mResolvedAnimAvatar,
                                            (std::uint32_t)movementSimulationTick);
    }

    auto clipFromAvatar = [&](const std::string& name) -> bool {
        return animationSet && animationSet->valid &&
               animationSet->clips.find(name) != animationSet->clips.end();
    };
    auto findClip = [&](const std::string& name) -> const AnimClip* {
        if (name.empty()) return nullptr;
        if (clipFromAvatar(name))
            return &animationSet->clips.find(name)->second;
        // Melee exclusive: never fall through to the global profile or another
        // animation set. A missing clip uses this profile's configured fallback
        // (handled by the caller) instead.
        if (meleeExclusivePose)
            return nullptr;
        auto it = gPlayerProcedural.layers.animations.find(name);
        return it != gPlayerProcedural.layers.animations.end() ? &it->second : nullptr;
    };

    const AvatarStateMap* sm = (animationSet && animationSet->valid)
        ? &animationSet->states : nullptr;
    auto locomotionClip = [&]() -> std::string {
        if (!sm) return "walk";
        const std::string& mode = rig.movement.mode;
        if (mode == "fly") return sm->fly;
        if (mode == "crawl") return sm->crawl;
        if (mode == "swim") return sm->swim;
        return sm->walk;
    };
    auto defaultClip = [&]() -> std::string { return sm ? sm->def : "idle"; };

    // One-shot lock (attack/hurt/death) owns the pose while it plays.
    if (!lockedClip.empty()) {
        const AnimClip* lc = findClip(lockedClip);
        if (!lc) {
            if (meleeExclusivePose) {
                const std::string fallback = sm ? sm->def : "idle";
                static uint64_t lastMeleeMissingClipMs = 0;
                const uint64_t nowMissing = nowMs();
                if (nowMissing - lastMeleeMissingClipMs >= 1000) {
                    lastMeleeMissingClipMs = nowMissing;
                    StructuredLogger::instance().writeEvent(
                        StructuredCategory::Avatar, StructuredLevel::Errors,
                        "anim.melee.clip_missing", avatarName(),
                        "Melee clip missing; using profile fallback", 0,
                        {{"avatar", avatarName()}, {"missing_clip", lockedClip},
                         {"fallback_clip", fallback}, {"mode", "melee"}});
                }
                lockedClip = (fallback != lockedClip && clipFromAvatar(fallback))
                    ? fallback : std::string();
            } else {
                lockedClip.clear();
            }
            lockedClipPhaseTick = 0;
            lockedClipPhaseTicks = 0;
            lockedClipSampleTick = -1;
        } else if (lockedClipPhaseTicks <= 0 && lockedClipSampleTick < 0) {
            // Time-driven one-shot. Phase-driven clips are held by the action
            // owner and are not auto-cleared by elapsed time.
            lockedClipTime += dt;
            if (!lockedClipLoop &&
                lockedClipTime >= (float)lc->durationTicks / 60.0f) {
                lockedClip.clear();
            }
        }
    }

    const AnimClip* locoClip = findClip(locomotionClip());
    bool locoInputTriggered = locoClip ? locoClip->inputTriggered : false;
    bool nowMoving = locoInputTriggered ? movementPressed : (move01 >= 0.01f);
    previousMove01 = move01;

    // Landing outranks locomotion and holds the whole authored one-shot. Keep
    // the short legacy fallback only when this profile has no landing clip.
    const AnimClip* landingClip = sm ? findClip(sm->land) : nullptr;
    const float landingDuration = landingClip
        ? (float)std::max(1, landingClip->durationTicks) / 60.0f
        : 0.12f;
    if (ground.didLand) {
        landAnimTimer = landingDuration;
    } else {
        landAnimTimer = std::max(0.0f, landAnimTimer - dt);
    }

    // Hit reaction: a brief, interruptible authored `hurt` pose on a percent
    // increase (presentation only — Melee has no hitstun).
    if (percent > animPrevPercent + 0.5f)
        hurtTimer = 0.20f;
    animPrevPercent = percent;
    hurtTimer = std::max(0.0f, hurtTimer - dt);

    // Respawn: play the `respawn` clip briefly on a dead->alive transition
    // (reuses the existing respawn lifecycle; the client sees dead flip to false).
    if (!dead && animPrevDead)
        respawnAnimTimer = 0.30f;
    else
        respawnAnimTimer = std::max(0.0f, respawnAnimTimer - dt);
    animPrevDead = dead;

    std::string activeAnim = currentAnimName;
    if (!lockedClip.empty()) {
        activeAnim = lockedClip;
    } else if (sm && meleeExclusivePose) {
        // Exclusive Melee: choose the authored state clip from the actor profile.
        // Missing optional states fall back within the same profile.
        // Raw collision contact can flicker for a tick while the actor remains
        // within the grounded grace window. Use the stable state for Melee's
        // grounded/airborne pose choice so crouch does not flash into fall.
        const bool airborne = !ground.stableOnGround;
        const bool rising = vel.z > 1.0f;
        const bool fast = speed > 12.0f;
        if (dead && !sm->death.empty())
            activeAnim = sm->death;
        else if (respawnAnimTimer > 0.0f && !sm->respawn.empty())
            activeAnim = sm->respawn;
        else if (hurtTimer > 0.0f && !sm->hurt.empty())
            activeAnim = sm->hurt;
        // Landing holds over automatic movement states until its one-shot ends.
        // Terminal states, hit reaction, and explicit locked actions still win.
        else if (landAnimTimer > 0.0f && !sm->land.empty())
            activeAnim = sm->land;
        else if (airborne && rising && !sm->jump.empty())
            activeAnim = sm->jump;
        else if (airborne && !sm->fall.empty())
            activeAnim = sm->fall;
        else if (crouching && !sm->crouch.empty())
            activeAnim = sm->crouch;
        else if (nowMoving)
            activeAnim = (fast && !sm->run.empty()) ? sm->run : locomotionClip();
        else
            activeAnim = defaultClip();
    } else if (nowMoving) {
        activeAnim = locomotionClip();
    } else if (currentAnimName != defaultClip()) {
        const AnimClip* cur = findClip(currentAnimName);
        const bool returnToIdle =
            cur && cur->tickBasedReturnToIdle && findClip("return_to_idle") &&
            currentAnimName != "return_to_idle";
        if (returnToIdle) {
            activeAnim = "return_to_idle";
        } else if (currentAnimName == "return_to_idle") {
            const AnimClip* rc = findClip("return_to_idle");
            activeAnim = (rc && animStateTime < (float)rc->durationTicks / 60.0f)
                ? "return_to_idle" : defaultClip();
        } else {
            activeAnim = defaultClip();
        }
    }

    {
        static uint64_t lastAnimStateLogMs = 0;
        uint64_t nowAnim = nowMs();
        if (nowAnim - lastAnimStateLogMs >= 1000)
        {
            DBG(Animation, "player=%s movementPressed=%d locoInputTriggered=%d "
                "speed=%.2f move01=%.3f nowMoving=%d currentAnim=%s activeAnim=%s "
                "animStateTimeBefore=%.3f",
                username.c_str(), (int)movementPressed, (int)locoInputTriggered,
                speed, move01, (int)nowMoving,
                currentAnimName.c_str(), activeAnim.c_str(), animStateTime);
            lastAnimStateLogMs = nowAnim;
        }
    }

    if (activeAnim != currentAnimName) {
        const std::string prevAnim = currentAnimName;
        currentAnimName = activeAnim;
        const bool restartLanding = ground.didLand && lockedClip.empty() && sm &&
                                    activeAnim == sm->land;
        // The transition tick itself samples the first landing pose (tick 0).
        animStateTime = restartLanding ? -dt : 0.0f;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Avatar, StructuredLevel::Important,
            "anim.state.changed", avatarName().empty() ? username : avatarName(),
            "animation state changed",
            (uint32_t)movementSimulationTick,
            {{"avatar", avatarName()},
             {"from", prevAnim}, {"to", activeAnim},
             {"moving", nowMoving},
             {"movement_mode", rig.movement.mode},
             {"animation_source", !lockedClip.empty() ? "locked_clip" :
                                  (sm && meleeExclusivePose ? "melee_locomotion" : "locomotion")},
             {"locked_clip", lockedClip},
             {"on_ground", ground.onGround},
             {"stable_on_ground", ground.stableOnGround},
             {"crouching", crouching},
             {"move01", move01},
             {"velocity", {vel.x, vel.y, vel.z}}},
            __FILE__, __LINE__, __FUNCTION__);

        if (meleeExclusivePose && lockedClip.empty()) {
            struct FlapWindow {
                uint64_t lastMs = 0;
                uint32_t changes = 0;
                std::string pair;
            };
            static std::unordered_map<std::string, FlapWindow> windows;
            const std::string& key = username.empty() ? avatarName() : username;
            FlapWindow& window = windows[key];
            const uint64_t nowFlap = nowMs();
            const std::string pair = prevAnim + "->" + activeAnim;
            if (nowFlap - window.lastMs <= 500 && window.pair == pair)
                ++window.changes;
            else
                window.changes = 1;
            window.lastMs = nowFlap;
            window.pair = pair;
            if (window.changes == 4) {
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Avatar, StructuredLevel::Errors,
                    "anim.melee.state_flap", avatarName(),
                    "Melee animation state changed repeatedly within 500ms",
                    (uint32_t)movementSimulationTick,
                    {{"actor", username}, {"from", prevAnim}, {"to", activeAnim},
                     {"changes_in_window", window.changes},
                     {"on_ground", ground.onGround},
                     {"stable_on_ground", ground.stableOnGround},
                     {"crouching", crouching},
                     {"velocity", {vel.x, vel.y, vel.z}}});
            }
        }
    } else if (ground.didLand && lockedClip.empty() && sm &&
               activeAnim == sm->land) {
        // A second landing can occur before the prior landing clip changes
        // state. Retrigger that same clip from tick 0 instead of leaving it
        // clamped at its final pose.
        animStateTime = -dt;
    }

    animStateTime += dt;

    // Bounded evidence that the Melee exclusive authored-pose policy is active
    // for this actor: identifies actor, mode, avatar, selected clip, and that
    // the forbidden pose layers were suppressed.
    if (meleeExclusivePose) {
        static uint64_t lastMeleeAuthoredLogMs = 0;
        const uint64_t nowAuthored = nowMs();
        if (nowAuthored - lastMeleeAuthoredLogMs >= 1000) {
            lastMeleeAuthoredLogMs = nowAuthored;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Avatar, StructuredLevel::Important,
                "anim.melee.authored_pose", avatarName(),
                "Melee exclusive authored pose active", (uint32_t)movementSimulationTick,
                {{"actor", username},
                 {"mode", "melee"},
                 {"avatar", avatarName()},
                 {"clip", activeAnim},
                 {"pose_policy", "authored_only"},
                 {"suppressed_layers", {"aimbody", "weapon_pose", "reload_overlay",
                                        "freeze_pose", "dash_pose", "idle_sway",
                                        "ragdoll_presentation", "global_fallback"}}},
                __FILE__, __LINE__, __FUNCTION__);
        }
    }

    // === DASH POSE TIMER ===
    if (dash.didDash && dashPoseTimer < 0.0f)
    {
        printf("[DASH ANIM] player=%s starting dash pose timer\n", username.c_str());
        dashPoseTimer = 0.0f;
    }

    float dashWeight = 0.0f;
    if (forceDashPose) {
        dashWeight = 1.0f;
    } else if (dashPoseTimer >= 0.0f) {
        const auto& dp = gPlayerProcedural.dashPose;
        if (dp.snapIn && dashPoseTimer == 0.0f) {
            dashWeight = 1.0f;
            dashPoseTimer = dp.blendInTime;
        }
        dashPoseTimer += dt;
        float holdDuration = 0.15f;
        float totalDuration = dp.blendInTime + holdDuration + dp.blendOutTime;
        if (dashPoseTimer <= dp.blendInTime) {
            float t = dashPoseTimer / dp.blendInTime;
            dashWeight = t * t * (3.0f - 2.0f * t);
        } else if (dashPoseTimer <= dp.blendInTime + holdDuration) {
            dashWeight = 1.0f;
        } else if (dashPoseTimer <= totalDuration) {
            float t = (dashPoseTimer - dp.blendInTime - holdDuration) / dp.blendOutTime;
            dashWeight = 1.0f - t * t * (3.0f - 2.0f * t);
        } else {
            dashWeight = 0.0f;
            dashPoseTimer = -1.0f;
        }
    }

    // === FREEZE POSE TIMER ===
    bool wasFreezeActive = freezePoseActive;
    freezePoseActive = freeze.freezeActive;
    if (freeze.freezeActive && !wasFreezeActive)
        freezePoseTimer = 0.0f;
    else if (!freeze.freezeActive && wasFreezeActive)
        freezePoseTimer = 0.0f;

    float freezeWeight = 0.0f;
    if (freezePoseTimer >= 0.0f) {
        freezePoseTimer += dt;
        const auto& fp = gPlayerProcedural.freezePose;
        if (freeze.freezeActive) {
            if (fp.snapIn) {
                freezeWeight = 1.0f;
                freezePoseTimer = fp.blendInTime;
            } else if (freezePoseTimer <= fp.blendInTime) {
                float t = freezePoseTimer / fp.blendInTime;
                freezeWeight = t * t * (3.0f - 2.0f * t);
            } else {
                freezeWeight = 1.0f;
            }
        } else {
            if (freezePoseTimer <= fp.blendOutTime) {
                float t = freezePoseTimer / fp.blendOutTime;
                freezeWeight = 1.0f - t * t * (3.0f - 2.0f * t);
            } else {
                freezeWeight = 0.0f;
                freezePoseTimer = -1.0f;
            }
        }
    }

    // === COMPUTE JSON KEYFRAME ANIMATION ===
    const AnimClip* activeClip = findClip(activeAnim);
    // Melee exclusive: a missing selected clip must not fall through to the rest
    // pose, AimBody, ragdoll, or another profile. Use this profile's configured
    // fallback (idle) and emit a bounded diagnostic.
    if (meleeExclusivePose && !activeClip) {
        const std::string fallback = sm ? sm->def : "idle";
        if (const AnimClip* fbClip = findClip(fallback)) {
            static uint64_t lastMeleeStateMissingMs = 0;
            const uint64_t nowStateMissing = nowMs();
            if (nowStateMissing - lastMeleeStateMissingMs >= 1000) {
                lastMeleeStateMissingMs = nowStateMissing;
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Avatar, StructuredLevel::Errors,
                    "anim.melee.clip_missing", avatarName(),
                    "Melee state clip missing; using profile fallback", 0,
                    {{"avatar", avatarName()}, {"missing_clip", activeAnim},
                     {"fallback_clip", fallback}, {"mode", "melee"}});
            }
            activeClip = fbClip;
        }
    }
    // Melee phase sampling: when the active clip is a phase-driven one-shot,
    // evaluate it at the phase-mapped sample instead of advancing by time.
    float animForcedSampleTick = -1.0f;
    float animForcedSampleDurationTicks = -1.0f;
    if (!lockedClip.empty() && activeAnim == lockedClip && lockedClipSampleTick >= 0) {
        const AnimClip* lc = findClip(lockedClip);
        const float runtimeTick = (float)lockedClipSampleTick;
        animForcedSampleTick = lc
            ? mapRuntimeTickToAuthoredTick(*lc, runtimeTick) : runtimeTick;
        if (lc && lc->hasMeleePhaseTimeline)
            animForcedSampleDurationTicks = (float)lc->phaseSourceTicks[3];
    } else if (!lockedClip.empty() && activeAnim == lockedClip && lockedClipPhaseTicks > 0) {
        const AnimClip* lc = findClip(lockedClip);
        if (lc) {
            animForcedSampleTick = AnimRotation::samplePhaseClipTick(
                lockedClipPhaseTick, lockedClipPhaseTicks, lc->durationTicks);
        }
    }
    std::unordered_map<std::string, AnimOverlayResult> animOverlay;
    if (activeClip) {
        float animSpeedScale = 1.0f;
        if (activeClip->speedBased && activeClip->speedScaleFromVelocity)
            animSpeedScale = 0.3f + move01 * 1.2f;

        // The global profile scales playback by the walk frequency knobs;
        // per-avatar clips play at their authored rate.
        if (!clipFromAvatar(activeAnim))
            animSpeedScale *= std::max(0.01f,
                gPlayerProcedural.walkFrequency *
                gPlayerProcedural.walkFrequencyMultiplier);

        if (DebugConfig::DEBUG_ANIMATION) {
            Debug::log(Debug::Category::Animation,
                "[ANIM] state=%s walkFrequency=%.2f finalSpeed=%.2f move01=%.2f nowMoving=%d\n",
                activeAnim.c_str(), gPlayerProcedural.walkFrequency,
                animSpeedScale, move01, (int)nowMoving);
        }

        animOverlay = interpolateAnimClip(*activeClip, animStateTime, animSpeedScale,
                                          animForcedSampleTick,
                                          animForcedSampleDurationTicks);
    }

    // Weapon state
    bool weaponEquipped = hasValidWeapon && (equippedSlot >= 1);
    std::string weaponId = equippedWeaponId;
    std::string poseLookupId = weaponId;
    const WeaponRuntime* currentWeaponRuntime = nullptr;
    auto runtimeIt = weaponRuntimes.find(weaponId);
    if (runtimeIt != weaponRuntimes.end())
        currentWeaponRuntime = &runtimeIt->second;
    bool isReloading = currentWeaponRuntime && currentWeaponRuntime->isReloading;
    {
        const WeaponDefinition* def = WeaponRegistry::instance().get(weaponId);
        if (def && !def->poseId.empty())
            poseLookupId = def->poseId;
    }

    auto tryWeaponPoseState = [&](const char* stateName) {
        const std::string stateKey = poseLookupId + ":" + stateName;
        auto stateIt = gPlayerProcedural.weaponPoses.find(stateKey);
        if (stateIt == gPlayerProcedural.weaponPoses.end() || !stateIt->second.useWeaponPose)
            return false;
        poseLookupId = stateKey;
        return true;
    };
    if (weaponEquipped && currentWeaponRuntime) {
        auto equipIt = currentWeaponRuntime->customFloats.find("equipTimer");
        const bool equipping = equipIt != currentWeaponRuntime->customFloats.end() && equipIt->second > 0.0f;
        if (equipping) {
            if (!tryWeaponPoseState("equipping"))
                tryWeaponPoseState("equip");
        } else if (isReloading) {
            if (!tryWeaponPoseState("reloading"))
                tryWeaponPoseState("reload");
        } else {
            if (currentWeaponRuntime->shootEffectTimer > 0.0f) {
                auto poseIt = currentWeaponRuntime->customFloats.find("weaponPoseState");
                if (poseIt != currentWeaponRuntime->customFloats.end()) {
                    if (poseIt->second == 1.0f)
                        tryWeaponPoseState("slash");
                    else if (poseIt->second == 2.0f)
                        tryWeaponPoseState("lunge");
                    else if (poseIt->second == 4.0f)
                        tryWeaponPoseState("defensive");
                    else
                        tryWeaponPoseState("shooting");
                } else if (!tryWeaponPoseState("shooting")) {
                    tryWeaponPoseState("fire");
                }
            } else if (currentWeaponRuntime->fireCooldown > 0.0f) {
                if (!tryWeaponPoseState("cooldown"))
                    tryWeaponPoseState("just_shot");
            } else if (!tryWeaponPoseState("idle")) {
                tryWeaponPoseState("equipped");
            }
        }
    }

    bool hasWeaponPose = false;
    WeaponPoseConfig* weaponPoseCfg = nullptr;
    if (weaponEquipped) {
        auto wpIt = gPlayerProcedural.weaponPoses.find(poseLookupId);
        if (wpIt != gPlayerProcedural.weaponPoses.end() && wpIt->second.useWeaponPose) {
            hasWeaponPose = true;
            weaponPoseCfg = &wpIt->second;
            Debug::log(Debug::Category::Animation,
                "[ANIM] selected weapon pose poseLookupId=%s weaponId=%s\n",
                poseLookupId.c_str(), weaponId.c_str());
        }
    }

    // === PERFECT POSE GENERATION ===
    for (PhysicalBodyPart& part : physicalBody.parts)
    {
        if (part.nodeIndex < 0 || part.nodeIndex >= (int)perfectPoseSkeleton.nodes.size())
            continue;

        ProceduralPose target;

        {
            auto ovIt = animOverlay.find(part.name);
            if (ovIt != animOverlay.end()) {
                target.translation = ovIt->second.translation;
                target.rotationEuler = ovIt->second.rotation;
                target.scale = ovIt->second.scale;
            }
        }

        // Melee exclusive authored-pose policy suppresses every non-authored
        // pose source (weapon/reload, freeze, dash, axis locks, idle sway).
        if (!meleeExclusivePose) {
        if (hasWeaponPose && weaponPoseCfg) {
            if (part.name == "leftArm") {
                target.rotationEuler = weaponPoseCfg->leftArm.rotation;
                target.translation += weaponPoseCfg->leftArm.translation;
            } else if (part.name == "rightArm") {
                target.rotationEuler = weaponPoseCfg->rightArm.rotation;
                target.translation += weaponPoseCfg->rightArm.translation;
            }
        }

        if (isReloading && (part.name == "leftArm" || part.name == "rightArm")) {
            target.rotationEuler += gPlayerProcedural.layers.reloadOverlay.rotation;
            target.translation += gPlayerProcedural.layers.reloadOverlay.translation;
        }

        // Freeze pose blending (overrides base, before dash/weapon)
        if (freezeWeight > 0.0f) {
            const auto& fp = gPlayerProcedural.freezePose;
            auto blend = [&](const glm::vec3& rot, const glm::vec3& trans) {
                target.rotationEuler = glm::mix(target.rotationEuler, rot, freezeWeight);
                target.translation = glm::mix(target.translation, trans, freezeWeight);
            };
            if (part.name == "torso")
                blend(fp.torsoRotation, fp.torsoTranslation);
            else if (part.name == "head")
                blend(fp.headRotation, fp.headTranslation);
            else if (part.name == "leftArm")
                blend(fp.leftArmRotation, fp.leftArmTranslation);
            else if (part.name == "rightArm")
                blend(fp.rightArmRotation, fp.rightArmTranslation);
            else if (part.name == "leftLeg")
                blend(fp.leftLegRotation, fp.leftLegTranslation);
            else if (part.name == "rightLeg")
                blend(fp.rightLegRotation, fp.rightLegTranslation);
        }

        if (dashWeight > 0.0f) {
            bool partHasWeaponPose = hasWeaponPose && (part.name == "leftArm" || part.name == "rightArm");
            if (!partHasWeaponPose) {
                const auto& dp = gPlayerProcedural.dashPose;
                if (part.name == "torso") {
                    target.rotationEuler = glm::mix(target.rotationEuler, dp.torsoRotation, dashWeight);
                    target.translation = glm::mix(target.translation, dp.torsoTranslation, dashWeight);
                } else if (part.name == "head") {
                    target.rotationEuler = glm::mix(target.rotationEuler, dp.headRotation, dashWeight);
                    target.translation = glm::mix(target.translation, dp.headTranslation, dashWeight);
                } else if (part.name == "leftArm") {
                    target.rotationEuler = glm::mix(target.rotationEuler, dp.leftArmRotation, dashWeight);
                    target.translation = glm::mix(target.translation, dp.leftArmTranslation, dashWeight);
                } else if (part.name == "rightArm") {
                    target.rotationEuler = glm::mix(target.rotationEuler, dp.rightArmRotation, dashWeight);
                    target.translation = glm::mix(target.translation, dp.rightArmTranslation, dashWeight);
                } else if (part.name == "leftLeg") {
                    target.rotationEuler = glm::mix(target.rotationEuler, dp.leftLegRotation, dashWeight);
                    target.translation = glm::mix(target.translation, dp.leftLegTranslation, dashWeight);
                } else if (part.name == "rightLeg") {
                    target.rotationEuler = glm::mix(target.rotationEuler, dp.rightLegRotation, dashWeight);
                    target.translation = glm::mix(target.translation, dp.rightLegTranslation, dashWeight);
                }
            }
        }

        // (Look-pitch is applied at the skeleton assignment below, after the
        // pose, so the rotation axis stays fixed regardless of the current
        // stance/animation rotation.)

        // Axis locks: clamp unwanted axes to 0
        applyAxisLocks(target, part.name);

        if (activeAnim == "idle") {
            auto& c = gPlayerProcedural;
            float t = weaponSwayTime, str = c.idleDebugStrength;
            float breathMult = freeze.freezeActive ? 0.2f : 1.0f;
            if (part.name == "leftArm")
                target.rotationEuler.x += std::sin(t * c.idleArmSpeed) * c.idleArmRotationDeg * str * (freeze.freezeActive ? 0.0f : 1.0f);
            else if (part.name == "rightArm")
                target.rotationEuler.x += std::sin(t * c.idleArmSpeed + 1.5f) * c.idleArmRotationDeg * str * (freeze.freezeActive ? 0.0f : 1.0f);
            else if (part.name == "leftLeg")
                target.rotationEuler.x += std::sin(t * c.idleLegSpeed) * c.idleLegRotationDeg * str * (freeze.freezeActive ? 0.0f : 1.0f);
            else if (part.name == "rightLeg")
                target.rotationEuler.x += std::sin(t * c.idleLegSpeed + 2.0f) * c.idleLegRotationDeg * str * (freeze.freezeActive ? 0.0f : 1.0f);
            else if (part.name == "torso") {
                target.rotationEuler.z += std::sin(t * c.idleTorsoSpeed) * c.idleTorsoRotationDeg * str;
                target.translation.y += std::sin(t * c.idleBreathingSpeed) * c.idleBreathingAmount * breathMult * str;
            } else if (part.name == "head")
                target.rotationEuler.x += std::sin(t * c.idleHeadSpeed) * c.idleHeadRotationDeg * str;
        }

        }

        // Store the clean perfect pose (exact JSON target, no smoothing)
        part.perfectPose = target;

        if (meleeExclusivePose) {
            // Exclusive authored pose: use the sampled authored transform exactly,
            // with no spring smoothing that could lag or alter it.
            part.pose = target;
        } else {
            // Physical body follows perfect pose with spring smoothing
            part.pose.translation =
                springVec3(part.translationSpring, target.translation, 90.0f, 16.0f, dt);
            part.pose.rotationEuler =
                springVec3(part.rotationSpring, target.rotationEuler, 80.0f, 14.0f, dt);
            part.pose.scale = target.scale;
        }

        // Apply to skeleton for rendering
        glm::mat4 restM = perfectPoseSkeleton.restLocalTransforms[part.nodeIndex];
        glm::mat4 poseM = poseMatrix(part.pose);
        {
            // Body-aim: limbs follow the camera look pitch (up/down). Per limb
            // and per axis (pitch/yaw/roll) the gain in config/aimbody.json says
            // how many degrees the limb rotates per degree of look pitch, around
            // that character axis. Applied AFTER the pose so the axes stay fixed
            // in the character frame regardless of stance/animation rotation —
            // pitching through the pose euler skews into roll for the
            // weapon-posed arms (~80deg rolls) and looks like tilting sideways.
            // Melee exclusive authored pose suppresses aimbody entirely.
            const AimBodyConfig& ab = AimBodyConfig::instance();
            const LimbAim* limb = ab.limb(part.name);
            if (!meleeExclusivePose && ab.enabled() && limb &&
                (limb->pitch != 0.0f || limb->yaw != 0.0f || limb->roll != 0.0f)) {
                const float desiredLookPitch = glm::degrees(std::asin(
                    glm::clamp(camForward.z, -1.0f, 1.0f)));
                const float lookPitch = AimBodyConfig::instance().smoothValue(
                    aimBodyPitch, desiredLookPitch, dt);
                aimBodyPitch = lookPitch;
                glm::mat4 q(1.0f);
                q = glm::rotate(q, glm::radians(lookPitch * limb->pitch), glm::vec3(1, 0, 0));
                q = glm::rotate(q, glm::radians(lookPitch * limb->yaw),   glm::vec3(0, 1, 0));
                q = glm::rotate(q, glm::radians(lookPitch * limb->roll),  glm::vec3(0, 0, -1));
                restM = restM * q;
                AimBodyConfig::instance().setApplied(lookPitch);
            }
        }
        perfectPoseSkeleton.nodes[part.nodeIndex].localTransform = restM * poseM;

        for (BodyPart& legacyPart : bodyParts)
        {
            if (legacyPart.nodeIndex != part.nodeIndex)
                continue;
            legacyPart.pose = part.pose;
            legacyPart.translationSpring = part.translationSpring;
            legacyPart.rotationSpring = part.rotationSpring;
            break;
        }
    }

    // Render-only rig parts (wings/stinger/tail): animated by the same clip
    // overlay, drawn by the renderer, never simulated.
    for (PhysicalBodyPart& part : physicalBody.extraParts)
    {
        if (part.nodeIndex < 0 || part.nodeIndex >= (int)perfectPoseSkeleton.nodes.size())
            continue;

        ProceduralPose target;
        auto ovIt = animOverlay.find(part.name);
        if (ovIt != animOverlay.end()) {
            target.translation = ovIt->second.translation;
            target.rotationEuler = ovIt->second.rotation;
            target.scale = ovIt->second.scale;
        }
        part.perfectPose = target;
        part.pose.translation =
            springVec3(part.translationSpring, target.translation, 90.0f, 16.0f, dt);
        part.pose.rotationEuler =
            springVec3(part.rotationSpring, target.rotationEuler, 80.0f, 14.0f, dt);
        part.pose.scale = target.scale;

        const glm::mat4 restM = perfectPoseSkeleton.restLocalTransforms[part.nodeIndex];
        perfectPoseSkeleton.nodes[part.nodeIndex].localTransform =
            restM * poseMatrix(part.pose);
    }

    if (DebugConfig::DEBUG_ANIMATION) {
        static float poseDebugTimer2 = 0.0f;
        poseDebugTimer2 -= dt;
        if (poseDebugTimer2 <= 0.0f) {
            poseDebugTimer2 = 1.0f;
            const PhysicalBodyPart* leftArm = nullptr;
            const PhysicalBodyPart* rightArm = nullptr;
            const PhysicalBodyPart* head = nullptr;
            const PhysicalBodyPart* torso = nullptr;
            const PhysicalBodyPart* leftLeg = nullptr;
            const PhysicalBodyPart* rightLeg = nullptr;
            for (const PhysicalBodyPart& part : physicalBody.parts) {
                if (part.name == "leftArm") leftArm = &part;
                if (part.name == "rightArm") rightArm = &part;
                if (part.name == "head") head = &part;
                if (part.name == "torso") torso = &part;
                if (part.name == "leftLeg") leftLeg = &part;
                if (part.name == "rightLeg") rightLeg = &part;
            }
            Debug::log(Debug::Category::Animation,
                "[ANIM] state=%s weapon=%s usePose=%d equipped=%d skeleton=%zu nodes move01=%.2f\n",
                activeAnim.c_str(), weaponId.c_str(), (int)hasWeaponPose, (int)weaponEquipped,
                perfectPoseSkeleton.nodes.size(), move01);
            if (head)
                Debug::log(Debug::Category::Animation,
                    "[ANIM] head perfectPose trans=(%.3f %.3f %.3f) rot=(%.1f %.1f %.1f)\n",
                    head->perfectPose.translation.x, head->perfectPose.translation.y, head->perfectPose.translation.z,
                    head->perfectPose.rotationEuler.x, head->perfectPose.rotationEuler.y, head->perfectPose.rotationEuler.z);
            if (torso)
                Debug::log(Debug::Category::Animation,
                    "[ANIM] torso perfectPose trans=(%.3f %.3f %.3f) rot=(%.1f %.1f %.1f)\n",
                    torso->perfectPose.translation.x, torso->perfectPose.translation.y, torso->perfectPose.translation.z,
                    torso->perfectPose.rotationEuler.x, torso->perfectPose.rotationEuler.y, torso->perfectPose.rotationEuler.z);
            if (leftArm)
                Debug::log(Debug::Category::Animation,
                    "[ANIM] leftArm perfectPose trans=(%.3f %.3f %.3f) rot=(%.1f %.1f %.1f)  physicalPose trans=(%.3f %.3f %.3f) rot=(%.1f %.1f %.1f)\n",
                    leftArm->perfectPose.translation.x, leftArm->perfectPose.translation.y, leftArm->perfectPose.translation.z,
                    leftArm->perfectPose.rotationEuler.x, leftArm->perfectPose.rotationEuler.y, leftArm->perfectPose.rotationEuler.z,
                    leftArm->pose.translation.x, leftArm->pose.translation.y, leftArm->pose.translation.z,
                    leftArm->pose.rotationEuler.x, leftArm->pose.rotationEuler.y, leftArm->pose.rotationEuler.z);
            if (rightArm)
                Debug::log(Debug::Category::Animation,
                    "[ANIM] rightArm perfectPose trans=(%.3f %.3f %.3f) rot=(%.1f %.1f %.1f)  physicalPose trans=(%.3f %.3f %.3f) rot=(%.1f %.1f %.1f)\n",
                    rightArm->perfectPose.translation.x, rightArm->perfectPose.translation.y, rightArm->perfectPose.translation.z,
                    rightArm->perfectPose.rotationEuler.x, rightArm->perfectPose.rotationEuler.y, rightArm->perfectPose.rotationEuler.z,
                    rightArm->pose.translation.x, rightArm->pose.translation.y, rightArm->pose.translation.z,
                    rightArm->pose.rotationEuler.x, rightArm->pose.rotationEuler.y, rightArm->pose.rotationEuler.z);
            if (leftLeg)
                Debug::log(Debug::Category::Animation,
                    "[ANIM] leftLeg perfectPose trans=(%.3f %.3f %.3f) rot=(%.1f %.1f %.1f)\n",
                    leftLeg->perfectPose.translation.x, leftLeg->perfectPose.translation.y, leftLeg->perfectPose.translation.z,
                    leftLeg->perfectPose.rotationEuler.x, leftLeg->perfectPose.rotationEuler.y, leftLeg->perfectPose.rotationEuler.z);
            if (rightLeg)
                Debug::log(Debug::Category::Animation,
                    "[ANIM] rightLeg perfectPose trans=(%.3f %.3f %.3f) rot=(%.1f %.1f %.1f)\n",
                    rightLeg->perfectPose.translation.x, rightLeg->perfectPose.translation.y, rightLeg->perfectPose.translation.z,
                    rightLeg->perfectPose.rotationEuler.x, rightLeg->perfectPose.rotationEuler.y, rightLeg->perfectPose.rotationEuler.z);
        }
    }

    if (DebugConfig::DEBUG_ANIM_ARMS) {
        static float armDebugTimer = 0.0f;
        armDebugTimer -= dt;
        if (armDebugTimer <= 0.0f) {
            armDebugTimer = 0.5f;
            printf("[ANIM_ARMS] state=%s weaponEquipped=%d hasWeaponPose=%d weapon=%s\n",
                   activeAnim.c_str(), (int)weaponEquipped, (int)hasWeaponPose, weaponId.c_str());
            for (const PhysicalBodyPart& part : physicalBody.parts) {
                if (part.name != "leftArm" && part.name != "rightArm") continue;
                printf("[ANIM_ARMS]   %s:\n", part.name.c_str());
                glm::vec3 animTrans(0.0f);
                glm::vec3 animRot(0.0f);
                auto ovIt = animOverlay.find(part.name);
                if (ovIt != animOverlay.end()) {
                    animTrans = ovIt->second.translation;
                    animRot = ovIt->second.rotation;
                }
                printf("[ANIM_ARMS]     animKeyframes trans=(%.3f %.3f %.3f) rot=(%.1f %.1f %.1f)\n",
                       animTrans.x, animTrans.y, animTrans.z,
                       animRot.x, animRot.y, animRot.z);
                glm::vec3 wpTrans(0.0f);
                glm::vec3 wpRot(0.0f);
                if (hasWeaponPose && weaponPoseCfg) {
                    if (part.name == "leftArm") {
                        wpTrans = weaponPoseCfg->leftArm.translation;
                        wpRot = weaponPoseCfg->leftArm.rotation;
                    } else {
                        wpTrans = weaponPoseCfg->rightArm.translation;
                        wpRot = weaponPoseCfg->rightArm.rotation;
                    }
                }
                printf("[ANIM_ARMS]     weaponPose    trans=(%.3f %.3f %.3f) rot=(%.1f %.1f %.1f)\n",
                       wpTrans.x, wpTrans.y, wpTrans.z,
                       wpRot.x, wpRot.y, wpRot.z);
                printf("[ANIM_ARMS]     perfectPose   trans=(%.3f %.3f %.3f) rot=(%.1f %.1f %.1f)\n",
                       part.perfectPose.translation.x, part.perfectPose.translation.y, part.perfectPose.translation.z,
                       part.perfectPose.rotationEuler.x, part.perfectPose.rotationEuler.y, part.perfectPose.rotationEuler.z);
                printf("[ANIM_ARMS]     physicalPose  trans=(%.3f %.3f %.3f) rot=(%.1f %.1f %.1f)\n",
                       part.pose.translation.x, part.pose.translation.y, part.pose.translation.z,
                       part.pose.rotationEuler.x, part.pose.rotationEuler.y, part.pose.rotationEuler.z);
            }
            const PhysicalBodyPart* leftArm = nullptr;
            const PhysicalBodyPart* rightArm = nullptr;
            for (const PhysicalBodyPart& part : physicalBody.parts) {
                if (part.name == "leftArm") leftArm = &part;
                if (part.name == "rightArm") rightArm = &part;
            }
            if (leftArm && rightArm) {
                printf("[ANIM_ARMS]   symmetry check:\n");
                printf("[ANIM_ARMS]     leftArm.translation.y  = %.3f  rightArm.translation.y  = %.3f  (should be negated)\n",
                       leftArm->perfectPose.translation.y, rightArm->perfectPose.translation.y);
                printf("[ANIM_ARMS]     leftArm.rotationEuler  = (%.1f %.1f %.1f)  rightArm.rotationEuler = (%.1f %.1f %.1f)\n",
                       leftArm->perfectPose.rotationEuler.x, leftArm->perfectPose.rotationEuler.y, leftArm->perfectPose.rotationEuler.z,
                       rightArm->perfectPose.rotationEuler.x, rightArm->perfectPose.rotationEuler.y, rightArm->perfectPose.rotationEuler.z);
            }
        }
    }

    if (DebugConfig::DEBUG_ANIMATION) {
        static float debugTimer = 0.0f;
        debugTimer -= dt;
        if (debugTimer <= 0.0f) {
            debugTimer = 1.0f;
            float tickTime = animStateTime * 60.0f;
            printf("[ANIM] state=%s tick=%.2f move01=%.2f keyframes=%zu\n",
                   activeAnim.c_str(), tickTime, move01,
                   activeClip ? activeClip->keyframes.size() : 0);
            for (const PhysicalBodyPart& part : physicalBody.parts) {
                printf("[ANIM]   perfectPose %s trans=(%.3f %.3f %.3f) rot=(%.1f %.1f %.1f)\n",
                       part.name.c_str(),
                       part.perfectPose.translation.x, part.perfectPose.translation.y, part.perfectPose.translation.z,
                       part.perfectPose.rotationEuler.x, part.perfectPose.rotationEuler.y, part.perfectPose.rotationEuler.z);
                printf("[ANIM]   physical  %s trans=(%.3f %.3f %.3f) rot=(%.1f %.1f %.1f)\n",
                       part.name.c_str(),
                       part.pose.translation.x, part.pose.translation.y, part.pose.translation.z,
                       part.pose.rotationEuler.x, part.pose.rotationEuler.y, part.pose.rotationEuler.z);
            }
        }
    }

    updateModelWorldTransforms();
}
