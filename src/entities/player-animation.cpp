#include "player.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <unordered_map>

#include "animation-rotation.h"
#include "animation-movement-state.h"
#include "debug/structured-log.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-melee-config.h"
#include "config.h"
#include "entities/aimbody-config.h"
#include "entities/player-avatar-animations.h"
#include "gamemode/gamemode.h"
#include "physics/config.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "network/net_common.h"
#include "network/community-match-client.h"
#include "perf/perf-spike.h"
using namespace MimitaNet;

static bool gAnimationPriorityDebug = false;

void setAnimationPriorityDebug(bool enabled) { gAnimationPriorityDebug = enabled; }
bool animationPriorityDebug() { return gAnimationPriorityDebug; }

struct AnimOverlayResult {
    glm::vec3 translation{0.0f};
    glm::vec3 rotation{0.0f};
    glm::vec3 scale{1.0f};
};

// Per-tick clip sampling must not allocate. The part names point into the
// selected clip's keyframe maps, which stay alive for the whole sample.
struct AnimOverlayBuffer {
    static constexpr int kMaxParts = 64;
    struct Entry {
        const std::string* name = nullptr;
        AnimOverlayResult value;
    };
    Entry entries[kMaxParts];
    int count = 0;

    const AnimOverlayResult* find(const std::string& name) const
    {
        for (int i = 0; i < count; ++i) {
            if (*entries[i].name == name)
                return &entries[i].value;
        }
        return nullptr;
    }

    void add(const std::string& name, const AnimOverlayResult& value)
    {
        if (count >= kMaxParts)
            return;
        entries[count++] = {&name, value};
    }
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

static void interpolateAnimClip(
    const AnimClip& clip, float timeSeconds, AnimOverlayBuffer& result,
    float speedScale = 1.0f,
    float forcedLoopedTick = -1.0f, float forcedSampleDurationTicks = -1.0f)
{
    if (clip.keyframes.empty()) return;

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

    auto addPart = [&](const std::string& partName) {
        auto prevIt = prev->parts.find(partName);
        auto nextIt = next->parts.find(partName);

        const glm::vec3 prevTrans = (prevIt != prev->parts.end())
            ? prevIt->second.translation : glm::vec3(0.0f);
        const glm::vec3 nextTrans = (nextIt != next->parts.end())
            ? nextIt->second.translation : glm::vec3(0.0f);
        const glm::quat prevRot = (prevIt != prev->parts.end())
            ? prevIt->second.rotationQuat : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        const glm::quat nextRot = (nextIt != next->parts.end())
            ? nextIt->second.rotationQuat : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        const glm::vec3 prevScale = (prevIt != prev->parts.end())
            ? prevIt->second.scale : glm::vec3(1.0f);
        const glm::vec3 nextScale = (nextIt != next->parts.end())
            ? nextIt->second.scale : glm::vec3(1.0f);

        AnimOverlayResult overlay;
        overlay.translation = glm::mix(prevTrans, nextTrans, t);
        // Canonical quaternion channels interpolate with shortest-arc SLERP.
        overlay.rotation = AnimRotation::eulerXyzDegreesFromQuat(
            AnimRotation::slerpShortest(prevRot, nextRot, t));
        overlay.scale = glm::mix(prevScale, nextScale, t);
        result.add(partName, overlay);
    };

    for (const auto& p : prev->parts)
        addPart(p.first);
    for (const auto& p : next->parts) {
        if (prev->parts.find(p.first) == prev->parts.end())
            addPart(p.first);
    }
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
    const WeaponDefinition* def = hasValidWeapon
        ? WeaponRegistry::instance().get(equippedWeaponId) : nullptr;
    if (dead && def && def->behaviorType != WeaponBehaviorType::Melee) return false;
    if (!def) return false;
    if (!def->animationProfile.empty()) {
        const AvatarAnimationSet* profile = AvatarAnimationRegistry::instance().getAnimationSet(
            "profile:" + def->animationProfile, (uint32_t)movementSimulationTick);
        return profile && profile->valid && profile->authoredOnly;
    }
    return WeaponData::MeleeMoveSetRegistry::instance().get(def->id) != nullptr;
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
    MIMITA_PERF_SCOPE("PlayerAnimation");
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

    // Explicitly authored-only profiles suppress procedural and hybrid-body
    // pose layers. Shared weapon clips can instead layer over avatar movement.
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

    const WeaponDefinition* animationWeapon = hasValidWeapon
        ? WeaponRegistry::instance().get(equippedWeaponId) : nullptr;
    // NPC-only opt-in: one authored clip tick per fixed simulation tick,
    // independent of movement speed and per-clip playback multipliers.
    const bool npcTickLocked = isNpcActor && animationSet && animationSet->valid &&
                               animationSet->npcTickLocked;    const AvatarAnimationSet* weaponAnimationSet = animationWeapon &&
        !animationWeapon->animationProfile.empty()
        ? AvatarAnimationRegistry::instance().getAnimationSet(
            "profile:" + animationWeapon->animationProfile, (uint32_t)movementSimulationTick)
        : nullptr;
    auto clipFromAvatar = [&](const std::string& name) -> bool {
        if (weaponAnimationSet && weaponAnimationSet->valid &&
            weaponAnimationSet->clips.count(name)) return true;
        return animationSet && animationSet->valid &&
               animationSet->clips.find(name) != animationSet->clips.end();
    };
    auto findClip = [&](const std::string& name) -> const AnimClip* {
        if (name.empty()) return nullptr;
        if (weaponAnimationSet && weaponAnimationSet->valid) {
            auto it = weaponAnimationSet->clips.find(name);
            if (it != weaponAnimationSet->clips.end()) return &it->second;
        }
        if (animationSet && animationSet->valid && animationSet->clips.count(name))
            return &animationSet->clips.find(name)->second;
        // Melee exclusive: never fall through to the global profile or another
        // animation set. A missing clip uses this profile's configured fallback
        // (handled by the caller) instead.
        if (meleeExclusivePose)
            return nullptr;
        auto it = gPlayerProcedural.layers.animations.find(name);
        return it != gPlayerProcedural.layers.animations.end() ? &it->second : nullptr;
    };

    // Weapon profiles supply attack clips. Avatar profiles remain the owner of
    // locomotion, landing, and other movement states unless no avatar profile
    // is available.
    const AvatarStateMap* sm = animationSet && animationSet->valid
        ? &animationSet->states : (weaponAnimationSet && weaponAnimationSet->valid)
        ? &weaponAnimationSet->states : nullptr;
    auto locomotionClip = [&]() -> std::string {
        if (!sm) return "walk";
        const std::string& mode = rig.movement.mode;
        if (mode == "fly") return sm->fly;
        if (mode == "crawl") return sm->crawl;
        if (mode == "swim") return sm->swim;
        return sm->walk;
    };
    auto defaultClip = [&]() -> std::string {
        // A held tool may supply the actor's base idle pose. Movement states
        // still come from the avatar profile and can use the legacy arm overlay.
        if (weaponAnimationSet && weaponAnimationSet->valid &&
            weaponAnimationSet->clips.count(weaponAnimationSet->states.def))
            return weaponAnimationSet->states.def;
        return sm ? sm->def : "idle";
    };

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
            const float speed = npcTickLocked ? 1.0f
                : std::max(0.01f, lc->speedMultiplier);
            const float holdSeconds = lc->lockTicks > 0
                ? (float)lc->lockTicks / 60.0f
                : (float)std::max(1, lc->durationTicks) / (60.0f * speed);
            if (!lockedClipLoop && lockedClipTime >= holdSeconds) {
                lockedClip.clear();
            }
        }
    }

    const AnimClip* locoClip = findClip(locomotionClip());
    bool locoInputTriggered = locoClip ? locoClip->inputTriggered : false;
    bool nowMoving = locoInputTriggered ? movementPressed : (move01 >= 0.01f);
    previousMove01 = move01;

    // Landing is a low-priority pose: it plays only while standing still and
    // never blocks walking/running. Moving clears it immediately.
    const AnimClip* landingClip = sm ? findClip(sm->land) : nullptr;
    const float landingDuration = landingClip
        ? (float)std::max(1, landingClip->durationTicks) / 60.0f
        : 0.12f;
    if (nowMoving) {
        landAnimTimer = 0.0f;
    } else if (ground.didLand) {
        landAnimTimer = landingDuration;
    } else {
        landAnimTimer = std::max(0.0f, landAnimTimer - dt);
    }

    // Authored action triggers. Timers hold the one-shot pose for its clip
    // duration; the procedural overlays are suppressed while an authored clip
    // owns the state (see below).
    auto clipDurationSeconds = [&](const std::string& clipName) -> float {
        const AnimClip* c = findClip(clipName);
        if (!c) return 0.0f;
        if (c->lockTicks > 0) return (float)c->lockTicks / 60.0f;
        return (float)std::max(1, c->durationTicks) /
            (60.0f * (npcTickLocked ? 1.0f : std::max(0.01f, c->speedMultiplier)));
    };
    const std::string dashClipName = sm
        ? (sm->dash.empty() ? "dash" : sm->dash) : std::string();
    const std::string downDashClipName = sm
        ? (sm->downDash.empty() ? "downDash" : sm->downDash) : std::string();
    if (dash.didDash && !dashClipName.empty()) {
        dashAnimTimer = clipDurationSeconds(dashClipName);
        if (dashAnimTimer <= 0.0f) {
            static uint64_t lastDashMissingMs = 0;
            const uint64_t nowDash = nowMs();
            if (nowDash - lastDashMissingMs >= 1000) {
                lastDashMissingMs = nowDash;
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Avatar, StructuredLevel::Errors,
                    "anim.dash.clip_missing", avatarName(),
                    "dash state has no clip; holding a short fallback window",
                    (uint32_t)movementSimulationTick,
                    {{"actor", username}, {"avatar", avatarName()},
                     {"clip", dashClipName},
                     {"weapon", equippedWeaponId}});
            }
            dashAnimTimer = 0.15f;
        }
    } else {
        dashAnimTimer = std::max(0.0f, dashAnimTimer - dt);
    }
    if (dash.didDownDash && !downDashClipName.empty()) {
        downDashAnimTimer = clipDurationSeconds(downDashClipName);
        if (downDashAnimTimer <= 0.0f)
            downDashAnimTimer = 0.15f;
    } else {
        downDashAnimTimer = std::max(0.0f, downDashAnimTimer - dt);
    }
    knockbackAnimTimer = std::max(0.0f, knockbackAnimTimer - dt);

    if (crouching != animPrevCrouching) {
        // A new crouch transition supersedes a landing presentation.
        landAnimTimer = 0.0f;
        const std::string crouchClip = sm
            ? (crouching ? sm->crouchIn : sm->crouchOut) : std::string();
        crouchAnimTimer = crouchClip.empty() ? 0.0f : clipDurationSeconds(crouchClip);
        crouchAnimInto = crouching;
        animPrevCrouching = crouching;
    } else {
        crouchAnimTimer = std::max(0.0f, crouchAnimTimer - dt);
    }
    // Crouch-in/out are transitions for a stationary stance. Moving or taking
    // off immediately selects the new movement pose instead of finishing them.
    if (nowMoving || !ground.stableOnGround)
        crouchAnimTimer = 0.0f;

    // Tool equip clip (owned by the weapon animation profile).
    const WeaponRuntime* preRuntime = nullptr;
    auto preRuntimeIt = weaponRuntimes.find(equippedWeaponId);
    if (preRuntimeIt != weaponRuntimes.end())
        preRuntime = &preRuntimeIt->second;
    if (preRuntime) {
        auto equipIt = preRuntime->customFloats.find("equipTimer");
        if (equipIt != preRuntime->customFloats.end() && equipIt->second > 0.0f)
            equipAnimTimer = std::max(equipAnimTimer, 0.05f);
    }
    equipAnimTimer = std::max(0.0f, equipAnimTimer - dt);

    // Hit reaction: percent damage (Melee), local takeDamage, and replicated
    // health drops all feed the configured hurt tier. The health path makes
    // remote players and NPC replicas animate hurt without a local damage call.
    if (animPrevHp >= 0 && currentHp < animPrevHp) {
        const int damage = animPrevHp - currentHp;
        const AnimationRuntimeSettings& hs = animationRuntimeSettings();
        int tier = 3;
        if (hs.hurtTierMode == 1) {
            const float pct = 100.0f * (float)damage / (float)std::max(1, maxHp);
            tier = pct <= hs.hurtPercentSmallMax ? 1
                 : pct <= hs.hurtPercentNormalMax ? 2 : 3;
        } else {
            tier = damage <= hs.hurtFixedSmallMax ? 1
                 : damage <= hs.hurtFixedNormalMax ? 2 : 3;
        }
        pendingHurtTier = std::max(pendingHurtTier, tier);
    }
    animPrevHp = currentHp;
    if (percent > animPrevPercent + 0.5f)
        pendingHurtTier = std::max(pendingHurtTier, 2);
    animPrevPercent = percent;
    std::string activeHurtClip;
    if (sm && pendingHurtTier > 0) {
        if (pendingHurtTier == 1) activeHurtClip = sm->hurtSmall;
        else if (pendingHurtTier == 2) activeHurtClip = sm->hurtNormal;
        else activeHurtClip = sm->hurtLarge;
        if (activeHurtClip.empty()) activeHurtClip = sm->hurt;
        if (!activeHurtClip.empty() && findClip(activeHurtClip))
            hurtTimer = clipDurationSeconds(activeHurtClip);
        pendingHurtTier = 0;
    }
    hurtTimer = std::max(0.0f, hurtTimer - dt);

    // Respawn: play the `respawn` clip briefly on a dead->alive transition
    // (reuses the existing respawn lifecycle; the client sees dead flip to false).
    if (!dead && animPrevDead)
        respawnAnimTimer = 0.30f;
    else
        respawnAnimTimer = std::max(0.0f, respawnAnimTimer - dt);
    const bool justDied = dead && !animPrevDead;
    animPrevDead = dead;

    const AnimationRuntimeSettings& animSettings = animationRuntimeSettings();
    const std::string deathClipName = sm
        ? (!sm->deathSmall.empty() ? sm->deathSmall : sm->death)
        : std::string();
    std::string deathPresentation = animSettings.deathPresentation;
    if (CommunityMatchClient::instance().active()) {
        const Gamemode& modeConfig =
            GamemodeRegistry::instance().get(CommunityMatchClient::instance().mode());
        if (!modeConfig.deathPresentation.empty())
            deathPresentation = modeConfig.deathPresentation;
    }
    if (deathPresentation == "auto")
        deathPresentation = "animation";
    const bool deathClipAllowed = deathPresentation != "disappear" &&
        deathPresentation != "ragdoll";
    if (justDied) {
        authoredDeathTimer = deathClipAllowed && !deathClipName.empty() &&
            findClip(deathClipName) ? clipDurationSeconds(deathClipName) : 0.0f;
    } else if (!dead) {
        authoredDeathTimer = 0.0f;
    } else if (authoredDeathTimer > 0.0f) {
        authoredDeathTimer = std::max(0.0f, authoredDeathTimer - dt);
    }
    authoredDeathActive = dead && !ragdollModeActive && deathClipAllowed &&
        authoredDeathTimer > 0.0f && !deathClipName.empty() && findClip(deathClipName);

    std::string activeAnim = currentAnimName;
    nlohmann::json candidateJson;
    if (gAnimationPriorityDebug)
        candidateJson = nlohmann::json::array();
    std::string selectedState;
    int selectedPriority = -1;
    if (!lockedClip.empty()) {
        activeAnim = lockedClip;
        selectedState = "locked";
        selectedPriority = animationStatePriority("locked");
    } else if (sm) {
        MIMITA_PERF_SCOPE("AnimStateSelect");
        // Candidate scoring makes the JSON priorities authoritative. Ties keep
        // the earlier candidate, so set order still acts as a stable fallback.
        struct StateCandidate {
            std::string state;
            std::string clip;
            int priority = 0;
        };
        StateCandidate candidates[24];
        int candidateCount = 0;
        const auto& statePriorities =
            animationRuntimeSettings().statePriorities;
        auto statePriority = [&](const char* state) {
            const auto it = statePriorities.find(state);
            return it != statePriorities.end() ? it->second : 0;
        };
        auto addCandidate = [&](const char* state, const std::string& clip,
                                bool eligible) {
            if (candidateCount >= 24) return;
            const AnimClip* c = clip.empty() ? nullptr : findClip(clip);
            const int priority = statePriority(state) + (c ? c->priority : 0);
            if (gAnimationPriorityDebug) {
                candidateJson.push_back({{"state", state}, {"clip", clip},
                    {"priority", priority}, {"eligible", eligible},
                    {"clip_found", c != nullptr}});
            }
            if (!eligible || !c) return;
            candidates[candidateCount++] = {state, clip, priority};
        };

        const bool airborne = !ground.stableOnGround;
    // Landing is a one-time presentation of the grounded landing state. Any
    // move or renewed airborne state supersedes it instead of letting it
    // reappear later when movement stops.
    if (nowMoving || airborne)
        landAnimTimer = 0.0f;
        const bool preferRun = !sm->run.empty() && currentAnimName == sm->run;
        const AnimationMovementState movementState = resolveAnimationMovementState(
            speed, vel.z, ground.stableOnGround, crouching, nowMoving, preferRun);
        const std::string airFallback = !sm->fall.empty()
            ? sm->fall
            : (!sm->jump.empty() ? sm->jump : locomotionClip());
        addCandidate("death", deathClipName, authoredDeathActive);
        addCandidate("respawn", sm->respawn, respawnAnimTimer > 0.0f);
        addCandidate("hurt", activeHurtClip, hurtTimer > 0.0f);
        addCandidate("freeze", sm->freeze, freeze.freezeActive || freeze.didFreeze);
        addCandidate("downDash", downDashClipName, downDashAnimTimer > 0.0f);
        addCandidate("dash", dashClipName, dashAnimTimer > 0.0f);
        addCandidate("knockback", sm->knockback, knockbackAnimTimer > 0.0f);
        const bool landingActive = ground.stableOnGround && !nowMoving &&
                                   landAnimTimer > 0.0f && !sm->land.empty();
        if (landingActive) {
            addCandidate("land", sm->land, true);
        } else {
            switch (movementState) {
                case AnimationMovementState::Idle:
                    addCandidate("idle", defaultClip(), true);
                    break;
                case AnimationMovementState::Walk:
                    addCandidate("walk", locomotionClip(), true);
                    break;
                case AnimationMovementState::Run:
                    addCandidate("run", sm->run.empty() ? locomotionClip() : sm->run, true);
                    break;
                case AnimationMovementState::Crouch: {
                    const std::string clip = crouchAnimTimer > 0.0f
                        ? (crouchAnimInto ? sm->crouchIn : sm->crouchOut)
                        : sm->crouch;
                    addCandidate("crouch", clip, !clip.empty());
                    break;
                }
                case AnimationMovementState::CrouchWalk: {
                    std::string clip = sm->crouchWalk.empty()
                        ? locomotionClip() : sm->crouchWalk;
                    if (!findClip(clip)) clip = locomotionClip();
                    addCandidate("crouchWalk", clip, true);
                    break;
                }
                case AnimationMovementState::Jump:
                    addCandidate("jump", sm->jump.empty() ? airFallback : sm->jump, true);
                    break;
                case AnimationMovementState::Fall:
                    addCandidate("fall", airFallback, airborne);
                    break;
            }
        }
        if (equipAnimTimer > 0.0f) {
            const std::string equipClip = weaponAnimationSet && weaponAnimationSet->valid
                ? weaponAnimationSet->states.equip : sm->equip;
            addCandidate("attack", equipClip, !equipClip.empty());
        }

        for (int i = 0; i < candidateCount; ++i) {
            if (candidates[i].priority > selectedPriority) {
                selectedPriority = candidates[i].priority;
                selectedState = candidates[i].state;
                activeAnim = candidates[i].clip;
            }
        }
        if (selectedPriority < 0) {
            selectedState = "idle_fallback";
            selectedPriority = animationStatePriority("idle");
            activeAnim = defaultClip();
        }
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

    if (gAnimationPriorityDebug) {
        static uint64_t lastPriorityLogMs = 0;
        const uint64_t nowPriority = nowMs();
        if (activeAnim != currentAnimName || nowPriority - lastPriorityLogMs >= 1000) {
            lastPriorityLogMs = nowPriority;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Avatar, StructuredLevel::Important,
                "anim.priority", avatarName().empty() ? username : avatarName(),
                "animation candidate priorities and selected state",
                (uint32_t)movementSimulationTick,
                {{"actor", username}, {"avatar", avatarName()},
                 {"weapon", equippedWeaponId}, {"weapon_equipped", hasValidWeapon},
                 {"selected_state", selectedState}, {"selected_priority", selectedPriority},
                 {"selected_clip", activeAnim}, {"current_clip", currentAnimName},
                 {"locked_clip", lockedClip}, {"locked_clip_time", lockedClipTime},
                 {"locked_clip_sample_tick", lockedClipSampleTick},
                 {"weapon_pose_action", weaponPoseActionState},
                 {"dash_timer", dashAnimTimer}, {"downdash_timer", downDashAnimTimer},
                 {"hurt_timer", hurtTimer}, {"land_timer", landAnimTimer},
                 {"crouch_timer", crouchAnimTimer}, {"equip_timer", equipAnimTimer},
                 {"on_ground", ground.onGround},
                 {"stable_on_ground", ground.stableOnGround},
                 {"moving", nowMoving}, {"move01", move01},
                 {"velocity", {vel.x, vel.y, vel.z}},
                 {"candidates", candidateJson}},
                __FILE__, __LINE__, __FUNCTION__);
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

        const std::string logActor = avatarName().empty() ? username : avatarName();
        struct ChangeLogState {
            uint64_t lastMs = 0;
            uint32_t suppressed = 0;
        };
        static std::unordered_map<std::string, ChangeLogState> changeLogs;
        ChangeLogState& logState = changeLogs[logActor];
        const uint64_t nowChange = nowMs();
        const bool writeChange =
            logState.lastMs == 0 || nowChange - logState.lastMs >= 250;
        if (writeChange) {
        nlohmann::json stateFields = {
             {"avatar", avatarName()},
             {"from", prevAnim}, {"to", activeAnim},
             {"suppressed_since_last", logState.suppressed},
             {"selected_state", selectedState},
             {"selected_priority", selectedPriority},
             {"aim_layer", [&]() {
                 const AnimClip* selected = findClip(activeAnim);
                 return selected && selected->aimLayer;
             }()},
             {"moving", nowMoving},
             {"movement_mode", rig.movement.mode},
             {"animation_source", !lockedClip.empty() ? "locked_clip" :
                                  (sm && meleeExclusivePose ? "melee_locomotion" : "locomotion")},
             {"locked_clip", lockedClip},
             {"on_ground", ground.onGround},
             {"stable_on_ground", ground.stableOnGround},
             {"crouching", crouching},
             {"move01", move01},
             {"velocity", {vel.x, vel.y, vel.z}}};
        // Candidate arrays are large and hot; keep them behind the explicit
        // priority-debug switch so normal play does not pay JSON/log cost.
        if (gAnimationPriorityDebug)
            stateFields["candidates"] = candidateJson;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Avatar, StructuredLevel::Important,
            "anim.state.changed", logActor,
            "animation state changed",
            (uint32_t)movementSimulationTick, stateFields,
            __FILE__, __LINE__, __FUNCTION__);
        logState.lastMs = nowChange;
        logState.suppressed = 0;
        } else {
            ++logState.suppressed;
        }

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
    // for this actor: identifies actor, mode, avatar, selected clip, AimBody
    // selection, and the unrelated pose layers that were suppressed.
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
                 {"aim_layer", [&]() {
                     const AnimClip* selected = findClip(activeAnim);
                     return selected && selected->aimLayer;
                 }()},
                 {"suppressed_layers", {"weapon_pose", "reload_overlay",
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

    const bool authoredDashPose = sm &&
        ((!sm->dash.empty() && activeAnim == sm->dash) ||
         (!sm->downDash.empty() && activeAnim == sm->downDash) ||
         (!sm->knockback.empty() && activeAnim == sm->knockback));
    if (authoredDashPose) dashWeight = 0.0f;

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
    if (sm && !sm->freeze.empty() && activeAnim == sm->freeze)
        freezeWeight = 0.0f;

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
    AnimOverlayBuffer animOverlay;
    if (activeClip) {
        float animSpeedScale = npcTickLocked ? 1.0f
            : std::max(0.01f, activeClip->speedMultiplier);
        if (!npcTickLocked && activeClip->speedBased && activeClip->speedScaleFromVelocity)
            animSpeedScale *= 0.3f + move01 * 1.2f;

        // The global profile scales playback by the walk frequency knobs;
        // per-avatar clips play at their authored rate.
        if (!npcTickLocked && !clipFromAvatar(activeAnim))
            animSpeedScale *= std::max(0.01f,
                gPlayerProcedural.walkFrequency *
                gPlayerProcedural.walkFrequencyMultiplier);

        if (DebugConfig::DEBUG_ANIMATION) {
            Debug::log(Debug::Category::Animation,
                "[ANIM] state=%s walkFrequency=%.2f finalSpeed=%.2f move01=%.2f nowMoving=%d\n",
                activeAnim.c_str(), gPlayerProcedural.walkFrequency,
                animSpeedScale, move01, (int)nowMoving);
        }

        interpolateAnimClip(*activeClip, animStateTime, animOverlay,
                            animSpeedScale, animForcedSampleTick,
                            animForcedSampleDurationTicks);

        if (npcTickLocked) {
            static uint64_t lastNpcTickLockedMs = 0;
            const uint64_t nowTickLocked = nowMs();
            if (nowTickLocked - lastNpcTickLockedMs >= 250) {
                lastNpcTickLockedMs = nowTickLocked;
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Avatar, StructuredLevel::Important,
                    "anim.npc.ticklocked", avatarName(),
                    "NPC tick-locked animation advanced one authored tick",
                    (uint32_t)movementSimulationTick,
                    {{"actor", username}, {"avatar", avatarName()},
                     {"clip", activeAnim},
                     {"clip_tick", animStateTime * 60.0f},
                     {"speed_mps", speed}, {"playback_scale", animSpeedScale}},
                    __FILE__, __LINE__, __FUNCTION__);
            }
        }
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
    if (weaponEquipped) {
        bool equipping = false;
        if (currentWeaponRuntime) {
            auto equipIt = currentWeaponRuntime->customFloats.find("equipTimer");
            equipping = equipIt != currentWeaponRuntime->customFloats.end() && equipIt->second > 0.0f;
        }
        std::string actionPoseState = weaponPoseActionState;
        if (actionPoseState.empty() && weaponId == "chainsaw" && currentWeaponRuntime) {
            const auto revIt = currentWeaponRuntime->customFloats.find("chainsawRevMultiplier");
            if (revIt != currentWeaponRuntime->customFloats.end() && revIt->second > 1.001f)
                actionPoseState = "rev";
        }
        const bool hasActionPose = !actionPoseState.empty() &&
            tryWeaponPoseState(actionPoseState.c_str());
        if (hasActionPose) {
            // The active melee action selects its own weapon-specific pose.
        } else if (equipping) {
            if (!tryWeaponPoseState("equipping"))
                tryWeaponPoseState("equip");
        } else if (currentWeaponRuntime && isReloading) {
            if (!tryWeaponPoseState("reloading"))
                tryWeaponPoseState("reload");
        } else if (currentWeaponRuntime) {
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
        } else {
            if (!tryWeaponPoseState("idle"))
                tryWeaponPoseState("equipped");
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

    // A Blender-authored tool clip already owns the arm base pose. The legacy
    // weapon pose must not replace its arm channels before AimBody is layered.
    if (activeClip && weaponAnimationSet &&
        weaponAnimationSet->clips.count(activeAnim)) {
        hasWeaponPose = false;
        weaponPoseCfg = nullptr;
    }

    if (weaponId == "mace" && (!lockedClip.empty() || !weaponPoseActionState.empty())) {
        static uint64_t lastMaceTraceMs = 0;
        static std::string lastMaceTraceKey;
        const std::string traceSelectedWeaponPose =
            hasWeaponPose ? poseLookupId : std::string();
        const std::string traceKey = activeAnim + "|" + lockedClip + "|" +
            weaponPoseActionState + "|" + traceSelectedWeaponPose + "|" +
            std::to_string((int)nowMoving) + "|" + std::to_string((int)ground.stableOnGround);
        const uint64_t nowTrace = nowMs();
        if (traceKey != lastMaceTraceKey || nowTrace - lastMaceTraceMs >= 250) {
            lastMaceTraceKey = traceKey;
            lastMaceTraceMs = nowTrace;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Avatar, StructuredLevel::Important,
                "mace.anim.trace", avatarName().empty() ? username : avatarName(),
                "Mace action animation ownership snapshot",
                (uint32_t)movementSimulationTick,
                {{"actor", username}, {"avatar", avatarName()},
                 {"weapon", weaponId}, {"action_locked_clip", lockedClip},
                 {"locked_clip_sample_tick", lockedClipSampleTick},
                 {"selected_clip", activeAnim}, {"current_clip", currentAnimName},
                 {"selected_state", selectedState},
                 {"selected_priority", selectedPriority},
                 {"weapon_pose_action", weaponPoseActionState},
                 {"selected_weapon_pose", traceSelectedWeaponPose},
                 {"has_weapon_pose", hasWeaponPose},
                 {"active_clip_in_weapon_profile", activeClip && weaponAnimationSet &&
                    weaponAnimationSet->clips.count(activeAnim) > 0},
                 {"moving", nowMoving}, {"movement_pressed", movementPressed},
                 {"on_ground", ground.onGround},
                 {"stable_on_ground", ground.stableOnGround},
                 {"velocity", {vel.x, vel.y, vel.z}},
                 {"hitbox_action_active", !lockedClip.empty()}},
                __FILE__, __LINE__, __FUNCTION__);
        }
    }

    const std::string selectedWeaponPose = hasWeaponPose ? poseLookupId : std::string();
    if (selectedWeaponPose != lastLoggedWeaponPoseKey) {
        lastLoggedWeaponPoseKey = selectedWeaponPose;
        if (hasWeaponPose) {
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Avatar, StructuredLevel::Important,
                "weapon.pose.selected", username,
                "configured weapon pose selected for actor presentation", 0,
                {{"actor", username}, {"weapon", weaponId},
                 {"pose", selectedWeaponPose}});
        }
    }

    // Smooth the actor's look pitch once per simulation tick. Repeating this
    // inside the part loop makes the response depend on the number of limbs.
    AimBodyConfig& aimBodyConfig = AimBodyConfig::instance();
    const float desiredLookPitch = glm::degrees(std::asin(
        glm::clamp(camForward.z, -1.0f, 1.0f)));
    if (aimBodyConfig.enabled())
        aimBodyPitch = aimBodyConfig.smoothValue(aimBodyPitch, desiredLookPitch, dt);

    // === PERFECT POSE GENERATION ===
    for (PhysicalBodyPart& part : physicalBody.parts)
    {
        if (part.nodeIndex < 0 || part.nodeIndex >= (int)perfectPoseSkeleton.nodes.size())
            continue;

        ProceduralPose target;

        {
            bool partAllowed = true;
            if (activeClip && !activeClip->partMask.empty()) {
                partAllowed = std::find(activeClip->partMask.begin(),
                                        activeClip->partMask.end(),
                                        part.name) != activeClip->partMask.end();
            }
            const AnimOverlayResult* overlay = animOverlay.find(part.name);
            if (partAllowed && overlay) {
                const int rootMode = activeClip && activeClip->rootTranslationMode >= 0
                    ? activeClip->rootTranslationMode : animSettings.rootTranslationMode;
                const float rootScale = activeClip
                    ? activeClip->rootTranslationScale : 1.0f;
                target.translation = rootMode == 2
                    ? glm::vec3(0.0f) : overlay->translation * rootScale;
                target.rotationEuler = overlay->rotation;
                target.scale = overlay->scale;
            }
        }

        // Melee exclusive authored-pose policy suppresses every non-authored
        // pose source (weapon/reload, freeze, dash, axis locks, idle sway).
        if (hasWeaponPose && weaponPoseCfg) {
            if ((!meleeExclusivePose || !weaponPoseActionState.empty()) &&
                part.name == "leftArm") {
                target.rotationEuler = weaponPoseCfg->leftArm.rotation;
                target.translation += weaponPoseCfg->leftArm.translation;
            } else if ((!meleeExclusivePose || !weaponPoseActionState.empty()) &&
                       part.name == "rightArm") {
                target.rotationEuler = weaponPoseCfg->rightArm.rotation;
                target.translation += weaponPoseCfg->rightArm.translation;
            }
        }

        if (!meleeExclusivePose) {

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

        const bool configuredMeleeActionPose = hasWeaponPose &&
            !weaponPoseActionState.empty();
        if (proceduralMeleePoseActive && !configuredMeleeActionPose) {
            if (part.name == "rightArm") {
                target.rotationEuler = proceduralMeleeRightArmRotation;
                target.translation += proceduralMeleeRightArmTranslation;
            } else if (part.name == "torso") {
                target.rotationEuler = proceduralMeleeTorsoRotation;
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
            // The selected clip owns its base pose. AimBody adds look direction
            // over that pose unless this particular clip opts out. Authored-only
            // still suppresses the unrelated procedural pose layers above.
            const LimbAim* limb = aimBodyConfig.limb(part.name);
            const bool selectedClipAllowsAim = activeClip
                ? activeClip->aimLayer : !meleeExclusivePose;
            if (selectedClipAllowsAim && aimBodyConfig.enabled() && limb &&
                (limb->pitch != 0.0f || limb->yaw != 0.0f || limb->roll != 0.0f)) {
                glm::mat4 q(1.0f);
                q = glm::rotate(q, glm::radians(aimBodyPitch * limb->pitch), glm::vec3(1, 0, 0));
                q = glm::rotate(q, glm::radians(aimBodyPitch * limb->yaw),   glm::vec3(0, 1, 0));
                q = glm::rotate(q, glm::radians(aimBodyPitch * limb->roll),  glm::vec3(0, 0, -1));
                restM = restM * q;
                aimBodyConfig.setApplied(aimBodyPitch);
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
        if (const AnimOverlayResult* overlay = animOverlay.find(part.name)) {
            target.translation = overlay->translation;
            target.rotationEuler = overlay->rotation;
            target.scale = overlay->scale;
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
                if (const AnimOverlayResult* overlay =
                        animOverlay.find(part.name)) {
                    animTrans = overlay->translation;
                    animRot = overlay->rotation;
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
