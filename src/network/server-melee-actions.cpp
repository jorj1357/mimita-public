// 2026-10-17
/* purpose
* Owns the authoritative server Melee action runtime for Captain Falcon move sets.
* Advances windup/active/recovery from the data-driven move set, sweeps the
* action's limb-attached hitbox during active ticks, and applies the shared
* percent/knockback/impulse rule plus per-action phase impulses.
* Reuses the generic weapon registry, physical-contact shape test, movement
* external-impulse owner, and the durable structured logger.
* Does NOT own rendering, client prediction, audio playback, or HUD.
* Does NOT add per-move C++ logic; every value comes from config/weapons.json.
*/

#include "network/server.h"
#include "network/server-gamemode.h"
#include "npc/npc-behavior.h"
#include "network/network-weapons.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-melee-config.h"
#include "combat/weapon-execution.h"
#include "combat/melee-rules-config.h"
#include "config/movement-config.h"
#include "gamemode/gamemode.h"
#include "gamemode/match-roles.h"
#include "entities/player-avatar-animations.h"
#include "entities/animation-rotation.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "physics/movement/movement-step.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <limits>
#include <fstream>
#include <string>

#include <nlohmann/json.hpp>

namespace MimitaNet {
namespace {

using MeleeRules = MimitaMeleeRules::Rules;

static const MeleeRules& meleeRules()
{
    return MimitaMeleeRules::Config::instance().rules();
}

static constexpr float kMeleeReferenceSpeed = 20.0f;

static float targetExternalImpulseDecay(const ServerPlayer& target)
{
    if (const MovementConfig* movement =
            RoleMovementCache::instance().get(target.movementProfileId))
        return movement->externalImpulseDecay;
    return MovementJsonConfig::instance().config().externalImpulseDecay;
}

static float targetExternalImpulseDecay(const ServerNpc&)
{
    // ServerNpc mirrors the simulated body but does not carry a movement
    // profile id. Resolve the active mode's movement preset so Melee NPC
    // targets use the same smash decay as player targets.
    const Gamemode& mode =
        GamemodeRegistry::instance().get(serverGamemodeState().matchMode);
    if (!mode.movementPreset.empty()) {
        if (const MovementConfig* movement =
                RoleMovementCache::instance().get(mode.movementPreset))
            return movement->externalImpulseDecay;
    }
    return MovementJsonConfig::instance().config().externalImpulseDecay;
}

struct MeleeAttackerView {
    uint32_t id = 0;
    bool isNpc = false;
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    glm::vec3 forward{1.0f, 0.0f, 0.0f};
};

struct MeleeContactResult {
    float prePercent = 0.0f;
    float percentAdded = 0.0f;
    float shieldMultiplier = 1.0f;
    float force = 1.0f;
    float curveDistance = 0.0f;
    float knockbackMagnitude = 0.0f;
    glm::vec3 impulse{0.0f};
    bool shielded = false;
    float shieldDamage = 0.0f;
    bool breaksShield = false;
};

// One calculation for every attacker/target pairing. Percent uses only the
// authored base_percent and shield protection. Contact force and the pre-hit
// target percent scale knockback only.
static MeleeContactResult computeMeleeContact(
    const MeleeAttackerView& attacker, const glm::vec3& targetPosition,
    float preHitPercent, const WeaponData::MeleeAction& action,
    float targetExternalDecay, bool shielded, float shieldRemainingFraction)
{
    const MeleeRules& rules = meleeRules();
    MeleeContactResult result;
    result.prePercent = std::max(0.0f, preHitPercent);
    result.shielded = shielded;
    result.shieldMultiplier = shielded
        ? MimitaMeleeRules::Config::instance().shieldReceivedMultiplier(
              shieldRemainingFraction)
        : 1.0f;
    result.percentAdded = std::max(0.0f, action.basePercent) * result.shieldMultiplier;

    glm::vec3 dir = targetPosition - attacker.position;
    dir.z = 0.0f;
    if (glm::length(dir) < 0.001f)
        dir = attacker.forward;
    if (glm::length(dir) < 0.001f)
        dir = glm::vec3(1.0f, 0.0f, 0.0f);
    dir = glm::normalize(dir);

    const float speed = glm::length(glm::vec3(
        attacker.velocity.x, attacker.velocity.y, 0.0f));
    const float speedFactor = std::clamp(speed / kMeleeReferenceSpeed, 0.0f, 1.0f);
    const float directness = std::clamp(glm::dot(dir, attacker.forward), 0.0f, 1.0f);
    result.force = std::clamp(
        0.35f + 0.65f * directness * (0.5f + 0.5f * speedFactor),
        0.0f, 1.5f);
    result.force *= std::max(0.0f, rules.forceSpeedScale);
    result.force *= std::max(0.0f, rules.forceDirectnessScale);

    result.curveDistance =
        MimitaMeleeRules::Config::instance().curveDistance(result.prePercent);
    const float referenceImpulse = movementReferenceImpulseForDistance(
        result.curveDistance, targetExternalDecay, rules.referenceSeconds);
    const float moveScale = action.baseKnockback /
        std::max(0.001f, rules.referenceBaseImpulse);
    float magnitude = referenceImpulse * moveScale * result.force *
        action.knockbackForceScale * result.shieldMultiplier;
    magnitude = std::max(rules.forceMinImpulse,
                         std::min(magnitude, rules.forceMaxImpulse));
    result.knockbackMagnitude = magnitude;
    result.impulse = dir * magnitude +
        glm::vec3(0.0f, 0.0f, magnitude * action.knockbackVerticalFraction);
    result.shieldDamage = action.hasShieldDamage
        ? action.shieldDamage : action.basePercent;
    return result;
}

// In-progress action snapshots keyed by actor id. A Melee action keeps the
// move-set revision it started with even if config/weapons.json hot-reloads
// mid-action; new actions read the freshly published revision.
std::unordered_map<uint32_t, WeaponData::MeleeAction> g_meleeActionSnapshots;

// Per-NPC Melee action state, kept in a side map because the ServerNpc mirror
// is re-synced from the simulated NPC every tick (which would otherwise wipe
// per-action state).
struct NpcMeleeState {
    bool active = false;
    std::string actionId;
    uint32_t startTick = 0;
    glm::vec3 forward{1.0f, 0.0f, 0.0f};
    float modelYaw = 0.0f;
    glm::vec3 lastHitboxCenter{0.0f};
    std::unordered_set<uint32_t> hitTargets;
    float percent = 0.0f;
    uint32_t nextAttackTick = 0;
};
std::unordered_map<uint32_t, NpcMeleeState> g_npcMelee;

static const WeaponData::MeleeMoveSet* equippedMeleeMoveSet(const ServerPlayer& player)
{
    const WeaponDefinition* def = WeaponRegistry::instance().get(player.equippedWeaponId);
    return def && serverCommunityWeaponAllowed(player, def->id)
        ? WeaponData::MeleeMoveSetRegistry::instance().get(def->id) : nullptr;
}

static glm::vec3 horizontalForward(const glm::vec3& raw)
{
    glm::vec3 forward(raw.x, raw.y, 0.0f);
    if (!std::isfinite(forward.x) || !std::isfinite(forward.y) ||
        glm::length(forward) < 0.001f)
        return glm::vec3(1.0f, 0.0f, 0.0f);
    return glm::normalize(forward);
}

struct SampledPartPose {
    glm::vec3 translation{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
};

static float meleeEase(float t, AnimInterp interp)
{
    t = std::clamp(t, 0.0f, 1.0f);
    switch (interp) {
        case AnimInterp::Smooth: return t * t * (3.0f - 2.0f * t);
        case AnimInterp::EaseIn: return t * t;
        case AnimInterp::EaseOut: return 1.0f - (1.0f - t) * (1.0f - t);
        case AnimInterp::EaseInOut: return t < 0.5f ? 2.0f * t * t : 1.0f - 2.0f * (1.0f - t) * (1.0f - t);
        case AnimInterp::Step: return 0.0f;
        default: return t;
    }
}

static bool sampleClipPart(const AnimClip& clip, float tick, const std::string& partName,
                           SampledPartPose& pose)
{
    if (clip.keyframes.empty()) return false;
    tick = std::clamp(tick, 0.0f, (float)std::max(0, clip.durationTicks - 1));
    const AnimKeyframe* prev = &clip.keyframes.front();
    const AnimKeyframe* next = &clip.keyframes.front();
    for (const AnimKeyframe& key : clip.keyframes) {
        if ((float)key.tick <= tick) prev = &key;
        if ((float)key.tick >= tick) { next = &key; break; }
    }
    const float range = (float)(next->tick - prev->tick);
    const float t = meleeEase(range > 0.001f ? (tick - prev->tick) / range : 0.0f, prev->interp);
    const auto a = prev->parts.find(partName);
    const auto b = next->parts.find(partName);
    if (a == prev->parts.end() && b == next->parts.end()) return false;
    const AnimKeyframePart identity;
    const AnimKeyframePart& pa = a == prev->parts.end() ? identity : a->second;
    const AnimKeyframePart& pb = b == next->parts.end() ? identity : b->second;
    pose.translation = glm::mix(pa.translation, pb.translation, t);
    pose.rotation = AnimRotation::slerpShortest(pa.rotationQuat, pb.rotationQuat, t);
    pose.scale = glm::mix(pa.scale, pb.scale, t);
    return true;
}

static std::string actionClipForPhase(const WeaponData::MeleeAction& action,
                                     const char* phase)
{
    if (!action.animationClipId.empty()) return action.animationClipId;
    if (std::string(phase) == "windup") return action.windup.clipId;
    if (std::string(phase) == "active") return action.active.clipId;
    return action.recovery.clipId;
}

static std::optional<glm::vec3> animatedLimbHitboxCenter(
    const std::string& avatarName, const glm::vec3& actorPosition, float actorYawDegrees,
    const WeaponData::MeleeAction& action, const char* phase, int phaseTick, int elapsed,
    const WeaponData::MeleeHitbox& hitbox)
{
    if (hitbox.attachmentMode != WeaponData::MeleeHitboxAttachmentMode::AnimatedLimb)
        return std::nullopt;
    const std::string clipId = actionClipForPhase(action, phase);
    if (clipId.empty()) return std::nullopt;
    const AvatarAnimationSet* set = AvatarAnimationRegistry::instance().getAnimationSet(avatarName);
    if (!set || !set->valid) return std::nullopt;
    const auto clipIt = set->clips.find(clipId);
    if (clipIt == set->clips.end()) return std::nullopt;
    float sampleTick = (float)elapsed;
    if (action.animationClipId.empty()) {
        const int phaseTicks = phase == std::string("windup") ? action.windup.ticks :
                               phase == std::string("active") ? action.active.ticks : action.recovery.ticks;
        sampleTick = AnimRotation::samplePhaseClipTick(phaseTick, phaseTicks,
                                                        (float)clipIt->second.durationTicks);
    }
    SampledPartPose pose;
    if (!sampleClipPart(clipIt->second, sampleTick, hitbox.attachedPart, pose))
        return std::nullopt;
    const auto part = std::find_if(gServerBodyTemplate.begin(), gServerBodyTemplate.end(),
        [&](const ServerPlayerBodyPartTemplate& candidate) { return candidate.name == hitbox.attachedPart; });
    if (part == gServerBodyTemplate.end()) return std::nullopt;

    glm::mat4 poseMatrix = glm::translate(glm::mat4(1.0f), pose.translation) *
        glm::mat4_cast(pose.rotation) * glm::scale(glm::mat4(1.0f), pose.scale);
    const glm::mat4 actorRoot = glm::translate(glm::mat4(1.0f), actorPosition) *
        glm::rotate(glm::mat4(1.0f), glm::radians(actorYawDegrees), glm::vec3(0, 0, 1));
    const glm::mat4 limbWorld = actorRoot * part->restTransform * poseMatrix;
    return glm::vec3(limbWorld * glm::vec4(hitbox.localOffset, 1.0f));
}

static glm::vec3 legacyCameraBasisHitboxCenter(const glm::vec3& position,
    const glm::vec3& forward, const WeaponData::MeleeHitbox& hitbox)
{
    const glm::vec3 up(0, 0, 1);
    glm::vec3 right = glm::cross(forward, up);
    if (glm::length(right) < 0.001f) right = {0, 1, 0};
    right = glm::normalize(right);
    glm::vec3 base = position + up * 1.2f;
    if (hitbox.attachedPart == "leftLeg" || hitbox.attachedPart == "rightLeg") base = position + up * 0.5f;
    else if (hitbox.attachedPart == "torso") base = position + up * 1.1f;
    else if (hitbox.attachedPart == "head") base = position + up * 1.7f;
    return base + forward * (0.6f + hitbox.localOffset.z) + right * hitbox.localOffset.x + up * hitbox.localOffset.y;
}

static void emitMeleeActionEvent(const ServerPlayer& attacker, const char* event,
                                 const char* reason, uint32_t tick,
                                 const std::string& actionId)
{
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Duel, StructuredLevel::Important,
        event, std::to_string(attacker.id), reason, tick,
        nlohmann::json{{"actor", attacker.id},
                       {"action", actionId},
                       {"revision", 0},
                       {"position", {attacker.pos.x, attacker.pos.y, attacker.pos.z}}});
}

static void applyActionImpulses(ServerPlayer& attacker,
                                const WeaponData::MeleeAction& action,
                                const char* phase, int phaseTick, uint32_t tick)
{
    const glm::vec3 up(0.0f, 0.0f, 1.0f);
    const glm::vec3 forward = attacker.meleeActionForward;
    glm::vec3 right = glm::cross(forward, up);
    if (glm::length(right) < 0.001f)
        right = glm::vec3(0.0f, 1.0f, 0.0f);
    right = glm::normalize(right);

    for (const WeaponData::MeleeImpulse& imp : action.impulses) {
        if (imp.phase != phase || imp.tick != phaseTick)
            continue;
        const glm::vec3 impulse = forward * imp.forward + right * imp.right + up * imp.up;
        if (glm::length(impulse) < 0.001f)
            continue;
        attacker.vel += impulse;
        recordServerMovementExternalImpulse(attacker, impulse);
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Duel, StructuredLevel::Important,
            "melee.impulse", std::to_string(attacker.id),
            "melee action applied a phase impulse", tick,
            nlohmann::json{{"actor", attacker.id},
                           {"action", action.id},
                           {"phase", phase},
                           {"phase_tick", phaseTick},
                           {"forward", imp.forward},
                           {"right", imp.right},
                           {"up", imp.up}});
    }
}

static void emitMeleeHitFeedback(
    SOCKET sock, std::unordered_map<uint32_t, ServerPlayer>& players,
    uint64_t& totalPacketsOut, uint32_t tick, uint32_t attackerId,
    uint32_t targetId, const glm::vec3& hitPosition,
    const glm::vec3& direction, const glm::vec3& normal,
    float percentAdded, const glm::vec3& impulse, bool shieldBreak);

static void updateMeleeShield(
    ServerPlayer& p, uint32_t tick, SOCKET sock,
    std::unordered_map<uint32_t, ServerPlayer>& players,
    uint64_t& totalPacketsOut)
{
    const MeleeRules& rules = meleeRules();
    if (p.meleeShieldBroken) {
        p.meleeShieldDurability = std::min(
            rules.shieldDurability,
            p.meleeShieldDurability + rules.shieldDurability / std::max(1.0f, rules.shieldBreakRecoveryTicks));
        if (p.meleeShieldDurability >= rules.shieldDurability) {
            p.meleeShieldBroken = false;
            p.meleeShieldDurability = rules.shieldDurability;
        }
        return;
    }
    if (p.input.shieldHeld) {
        p.meleeShieldDurability -= rules.shieldDurability / std::max(1.0f, rules.shieldDecayTicks);
        if ((tick % 30u) == 0u) {
            const float remaining = std::clamp(
                p.meleeShieldDurability / std::max(1.0f, rules.shieldDurability),
                0.0f, 1.0f);
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Duel, StructuredLevel::Important,
                "melee.shield_state", std::to_string(p.id),
                "held Melee shield durability sample", tick,
                nlohmann::json{{"actor", p.id},
                               {"durability", p.meleeShieldDurability},
                               {"remaining_fraction", remaining},
                               {"visual_alpha", MimitaMeleeRules::Config::instance().shieldAlpha(remaining)},
                               {"broken", p.meleeShieldBroken}});
        }
        if (p.meleeShieldDurability <= 0.0f) {
            p.meleeShieldDurability = 0.0f;
            p.meleeShieldBroken = true;
            const glm::vec3 up(0.0f, 0.0f, 1.0f);
            const glm::vec3 breakImpulse = up * rules.shieldBreakUpImpulse;
            p.vel += breakImpulse;
            recordServerMovementExternalImpulse(p, breakImpulse);
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Duel, StructuredLevel::Important,
                "melee.shield_break", std::to_string(p.id),
                "shield broke from natural depletion", tick,
                nlohmann::json{{"actor", p.id}});
            // Natural expiry has no attacker hit; still publish the configured
            // break impulse and the existing impact/sound presentation once.
            emitMeleeHitFeedback(sock, players, totalPacketsOut, tick, p.id,
                                 p.id, p.pos, up, up, 0.0f, breakImpulse, true);
        }
    } else if (p.meleeShieldDurability < rules.shieldDurability) {
        // Released before break: recover over the configured window.
        p.meleeShieldDurability = std::min(
            rules.shieldDurability,
            p.meleeShieldDurability + rules.shieldDurability / std::max(1.0f, rules.shieldReleaseRecoveryTicks));
    }
}

// Provisional inherited-velocity cap by attacker percent (20/40/80 @ 0/100/300,
// capped ~120 @999). Configurable later through config/melee.json.
static float meleeGrabVelocityCap(float attackerPercent)
{
    const float p = std::max(0.0f, attackerPercent);
    float cap;
    if (p <= 100.0f)
        cap = 20.0f + (40.0f - 20.0f) * (p / 100.0f);
    else if (p <= 300.0f)
        cap = 40.0f + (80.0f - 40.0f) * ((p - 100.0f) / 200.0f);
    else
        cap = 80.0f + (120.0f - 80.0f) * std::min(1.0f, (p - 300.0f) / 699.0f);
    return std::min(cap, 120.0f);
}

static void applyMeleeGrab(ServerPlayer& attacker, ServerPlayer& target,
                           const WeaponData::MeleeAction& action, uint32_t tick)
{
    const MeleeRules& rules = meleeRules();
    // A shielded target cannot be grabbed at any shield strength.
    if (target.input.shieldHeld && !target.meleeShieldBroken) {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Duel, StructuredLevel::Important,
            "melee.grab_blocked", std::to_string(attacker.id),
            "grab rejected: target shielded", tick,
            nlohmann::json{{"attacker", attacker.id}, {"target_actor", target.id}});
        return;
    }
    if (tick < target.meleeGrabImmuneUntilTick)
        return;

    attacker.meleeGrabVictimId = target.id;
    attacker.meleeGrabPullTicksLeft =
        std::max(action.grabPullTicks, rules.grabPullTicks);
    target.meleeGrabImmuneUntilTick = tick + rules.grabAntiChainTicks;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Duel, StructuredLevel::Important,
        "melee.grab_start", std::to_string(attacker.id),
        "grab connected; pulling target", tick,
        nlohmann::json{{"attacker", attacker.id},
                       {"target_actor", target.id},
                       {"pull_ticks", attacker.meleeGrabPullTicksLeft},
                       {"attacker_percent", attacker.meleePercent},
                       {"victim_immune_until", target.meleeGrabImmuneUntilTick}});
}

// Advances an in-progress grab pull: a bounded, controllable impulse toward the
// attacker (the victim keeps full control; collision resolution is the movement
// owner's).
static void tickMeleeGrabPull(ServerPlayer& attacker,
                              std::unordered_map<uint32_t, ServerPlayer>& players)
{
    if (attacker.meleeGrabPullTicksLeft <= 0 || attacker.meleeGrabVictimId == 0)
        return;
    auto vit = players.find(attacker.meleeGrabVictimId);
    if (vit == players.end() || vit->second.dead) {
        attacker.meleeGrabPullTicksLeft = 0;
        attacker.meleeGrabVictimId = 0;
        return;
    }
    ServerPlayer& victim = vit->second;
    glm::vec3 dir = attacker.pos - victim.pos;
    dir.z = 0.0f;
    if (glm::length(dir) < 0.001f)
        dir = attacker.meleeActionForward;
    else
        dir = glm::normalize(dir);
    const float cap = meleeGrabVelocityCap(attacker.meleePercent);
    const glm::vec3 pull = dir * (cap * 0.25f);
    victim.vel += pull;
    recordServerMovementExternalImpulse(victim, pull);
    --attacker.meleeGrabPullTicksLeft;
    if (attacker.meleeGrabPullTicksLeft <= 0)
        attacker.meleeGrabVictimId = 0;
}

static void emitMeleeHitFeedback(
    SOCKET sock, std::unordered_map<uint32_t, ServerPlayer>& players,
    uint64_t& totalPacketsOut, uint32_t tick, uint32_t attackerId,
    uint32_t targetId, const glm::vec3& hitPosition,
    const glm::vec3& direction, const glm::vec3& normal,
    float percentAdded, const glm::vec3& impulse, bool shieldBreak = false)
{
    const uint32_t eventId = nextReliableGameplayEventId();
    ShotEventPacket event{};
    event.header.type = PACKET_SHOT_EVENT;
    event.header.tick = tick;
    event.header.playerId = attackerId;
    event.eventId = eventId;
    event.eventSessionId = serverReliableEventSessionId();
    event.shotSerial = eventId;
    event.shooterPlayerId = attackerId;
    event.targetPlayerId = targetId;
    // Visual intensity only. Melee percent is authoritative in snapshots and
    // the accepted result event; this never changes HP.
    event.damage = std::clamp((int)std::lround(percentAdded), 1, 6);
    event.power = percentAdded;
    event.effectFlags = SHOT_EFFECT_ENTITY_IMPACT | SHOT_EFFECT_HIT_SOUND |
                        SHOT_EFFECT_MELEE_CONFIRM;
    event.impactType = SHOT_IMPACT_ENTITY;
    event.damageConfirmed = 0;
    event.lastServerTick = tick;
    event.originX = hitPosition.x - direction.x * 0.35f;
    event.originY = hitPosition.y - direction.y * 0.35f;
    event.originZ = hitPosition.z - direction.z * 0.35f;
    event.hitX = hitPosition.x;
    event.hitY = hitPosition.y;
    event.hitZ = hitPosition.z;
    event.dirX = direction.x;
    event.dirY = direction.y;
    event.dirZ = direction.z;
    event.normalX = normal.x;
    event.normalY = normal.y;
    event.normalZ = normal.z;
    event.knockX = impulse.x;
    event.knockY = impulse.y;
    event.knockZ = impulse.z;

    const ReliableGameplayEventQueueResult result = queueReliableGameplayEventToAll(
        sock, players, &event, sizeof(event), eventId,
        event.eventSessionId, totalPacketsOut);
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Duel, StructuredLevel::Important,
        "melee.feedback.queued", std::to_string(eventId),
        shieldBreak ? "confirmed Melee shield break feedback queued"
                    : "confirmed Melee impact feedback queued", tick,
        nlohmann::json{{"attacker", attackerId}, {"target", targetId},
                       {"percent_added", percentAdded},
                       {"impulse", {impulse.x, impulse.y, impulse.z}},
                       {"shield_break", shieldBreak},
                       {"result", (int)result}});
}

static bool meleeShieldActive(const ServerPlayer& target)
{
    return target.input.shieldHeld && !target.meleeShieldBroken &&
           target.meleeShieldDurability > 0.0f;
}

static void applyMeleeContactToPlayer(
    const MeleeAttackerView& attacker, ServerPlayer& target,
    const WeaponData::MeleeAction& action, uint32_t tick, SOCKET sock,
    std::unordered_map<uint32_t, ServerPlayer>& players,
    uint64_t& totalPacketsOut,
    const WeaponExecution::PhysicalContactHit& contact)
{
    const MeleeRules& rules = meleeRules();
    const glm::vec3 up(0.0f, 0.0f, 1.0f);
    const bool shielded = meleeShieldActive(target);
    const float remaining = shielded
        ? std::clamp(target.meleeShieldDurability /
                         std::max(1.0f, rules.shieldDurability), 0.0f, 1.0f)
        : 1.0f;
    MeleeContactResult result = computeMeleeContact(
        attacker, target.pos, target.meleePercent, action,
        targetExternalImpulseDecay(target), shielded, remaining);

    target.meleePercent += result.percentAdded;
    target.vel += result.impulse;
    recordServerMovementExternalImpulse(target, result.impulse);

    glm::vec3 presentationImpulse = result.impulse;
    bool brokeShield = false;
    if (shielded) {
        target.meleeShieldDurability -= result.shieldDamage;
        if (target.meleeShieldDurability <= 0.0f) {
            brokeShield = true;
            target.meleeShieldDurability = 0.0f;
            target.meleeShieldBroken = true;
            const glm::vec3 breakImpulse = up * rules.shieldBreakUpImpulse;
            target.vel += breakImpulse;
            recordServerMovementExternalImpulse(target, breakImpulse);
            presentationImpulse += breakImpulse;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Duel, StructuredLevel::Important,
                "melee.shield_break", std::to_string(target.id),
                "shield broke from an incoming hit", tick,
                nlohmann::json{{"attacker", attacker.id},
                               {"target_actor", target.id},
                               {"action", action.id},
                               {"shield_damage", result.shieldDamage}});
        } else {
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Duel, StructuredLevel::Important,
                "melee.shield_hit", std::to_string(target.id),
                "attack absorbed by shield", tick,
                nlohmann::json{{"attacker", attacker.id},
                               {"target_actor", target.id},
                               {"action", action.id},
                               {"multiplier", result.shieldMultiplier},
                               {"shield_damage", result.shieldDamage},
                               {"shield_remaining", target.meleeShieldDurability}});
        }
    }

    const float percentBefore = target.meleePercent - result.percentAdded;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Duel, StructuredLevel::Important,
        "melee.hit", std::to_string(attacker.id),
        attacker.isNpc
            ? "npc melee action confirmed a swept hitbox contact"
            : "melee action confirmed a swept hitbox contact", tick,
        nlohmann::json{{"attacker", attacker.id},
                       {"attacker_is_npc", attacker.isNpc},
                       {"target_actor", target.id},
                       {"target_is_npc", false},
                       {"action", action.id},
                       {"percent_before", percentBefore},
                       {"percent_added", result.percentAdded},
                       {"percent_after", target.meleePercent},
                       {"curve_distance_m", result.curveDistance},
                       {"impulse", {result.impulse.x, result.impulse.y, result.impulse.z}},
                       {"force", result.force},
                       {"shielded", shielded},
                       {"shield_multiplier", result.shieldMultiplier},
                       {"shield_break", brokeShield},
                       {"spawn_generation", target.spawnGeneration}});
    emitMeleeHitFeedback(sock, players, totalPacketsOut, tick, attacker.id,
                         target.id, contact.hitPosition, attacker.forward,
                         contact.normal, result.percentAdded,
                         presentationImpulse, brokeShield);
    Debug::log(Debug::Category::Duel,
        "[MELEE HIT] attacker=%u target=%u action=%s percent=%.1f->%.1f kb=%.2f shield=%d\n",
        attacker.id, target.id, action.id.c_str(),
        percentBefore, target.meleePercent, result.knockbackMagnitude,
        (int)shielded);
}

static void applyMeleeContactToNpc(
    const MeleeAttackerView& attacker, ServerNpc& target,
    NpcMeleeState& targetState, const WeaponData::MeleeAction& action,
    uint32_t tick, SOCKET sock,
    std::unordered_map<uint32_t, ServerPlayer>& players,
    uint64_t& totalPacketsOut,
    const WeaponExecution::PhysicalContactHit& contact)
{
    const float prePercent = targetState.percent;
    // NPCs do not own a shield in this first slice; every hit is unshielded.
    MeleeContactResult result = computeMeleeContact(
        attacker, target.pos, prePercent, action,
        targetExternalImpulseDecay(target), false, 1.0f);

    targetState.percent += result.percentAdded;
    target.meleePercent = targetState.percent;
    target.knockbackImpulse += result.impulse;

    StructuredLogger::instance().writeEvent(
        StructuredCategory::Duel, StructuredLevel::Important,
        "melee.hit", std::to_string(attacker.id),
        "melee action confirmed a swept hitbox contact (npc)", tick,
        nlohmann::json{{"attacker", attacker.id},
                       {"attacker_is_npc", attacker.isNpc},
                       {"target_actor", target.entityId},
                       {"target_is_npc", true},
                       {"action", action.id},
                       {"percent_before", prePercent},
                       {"percent_added", result.percentAdded},
                       {"percent_after", target.meleePercent},
                       {"curve_distance_m", result.curveDistance},
                       {"impulse", {result.impulse.x, result.impulse.y, result.impulse.z}},
                       {"force", result.force},
                       {"shielded", false}});
    emitMeleeHitFeedback(sock, players, totalPacketsOut, tick, attacker.id,
                         target.entityId, contact.hitPosition, attacker.forward,
                         contact.normal, result.percentAdded, result.impulse);
}

static void startNpcMeleeAction(ServerNpc& npc, NpcMeleeState& st,
                                const WeaponData::MeleeAction& action,
                                const glm::vec3& forward, uint32_t tick,
                                const std::string& behaviorProfileId,
                                float targetDistance, float attackRange,
                                int attackGapTicks, float moveWeight)
{
    const bool interrupted = st.active;
    const std::string interruptedActionId = interrupted ? st.actionId : std::string{};
    const uint32_t interruptedElapsedTicks = interrupted ? tick - st.startTick : 0;
    st.active = true;
    st.actionId = action.id;
    st.startTick = tick;
    st.forward = forward;
    st.modelYaw = glm::degrees(std::atan2(npc.aim.y, npc.aim.x));
    st.lastHitboxCenter = glm::vec3(0.0f);
    st.hitTargets.clear();
    g_meleeActionSnapshots[npc.entityId] = action;
    const int attackIntervalTicks = std::clamp(attackGapTicks, 1, 30);
    st.nextAttackTick = tick + (uint32_t)attackIntervalTicks;

    // Snapshot action state drives the shared remote animation and audio
    // presentation path. Replacing these fields immediately interrupts the
    // previous clip on clients when the new action snapshot arrives.
    npc.meleeActionActive = true;
    npc.meleeActionId = action.id;
    npc.meleeActionStartTick = tick;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Duel, StructuredLevel::Important,
        "melee.action.start", std::to_string(npc.entityId),
        "npc melee action started", tick,
        nlohmann::json{{"actor", npc.entityId},
                       {"actor_is_npc", true},
                       {"action", action.id},
                       {"behavior_profile", behaviorProfileId},
                       {"target_distance_meters", targetDistance},
                       {"attack_range_meters", attackRange},
                       {"attack_gap_ticks", attackIntervalTicks},
                       {"attack_interval_ticks", attackIntervalTicks},
                       {"interrupted_previous_action", interrupted},
                       {"interrupted_action", interruptedActionId},
                       {"interrupted_elapsed_ticks", interruptedElapsedTicks},
                       {"move_weight", moveWeight},
                       {"position", {npc.pos.x, npc.pos.y, npc.pos.z}}});
}

} // namespace

bool serverMeleeStartAction(ServerPlayer& attacker, const std::string& actionId,
                            uint32_t tick)
{
    const WeaponData::MeleeMoveSet* moveSet = equippedMeleeMoveSet(attacker);
    if (!moveSet)
        return false;
    const WeaponData::MeleeAction* action = moveSet->find(actionId);
    if (!action)
        return false;

    attacker.meleeActionActive = true;
    attacker.meleeActionId = action->id;
    attacker.meleeActionStartTick = tick;
    attacker.meleePhaseTick = 0;
    attacker.meleeActionForward = horizontalForward(attacker.input.camForward);
    attacker.meleeActionYaw = attacker.yaw;
    attacker.meleeLastHitboxCenter = glm::vec3(0.0f);
    attacker.meleeHitTargets.clear();
    if (attacker.meleeActionSerial == 0)
        attacker.meleeActionSerial = 1;
    else
        ++attacker.meleeActionSerial;

    // Snapshot the action data so an in-progress action keeps the revision it
    // started with across a hot reload.
    g_meleeActionSnapshots[attacker.id] = *action;

    emitMeleeActionEvent(attacker, "melee.action.start",
                         "melee action started", tick, action->id);
    if (!action->animationClipId.empty()) {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Duel, StructuredLevel::Important,
            "melee.action.timeline", std::to_string(attacker.id),
            "melee action uses its authored clip timeline", tick,
            nlohmann::json{{"actor", attacker.id}, {"action", action->id},
                           {"animation_clip_id", action->animationClipId},
                           {"duration_ticks", action->totalTicks()},
                           {"windup_ticks", action->windup.ticks},
                           {"active_ticks", action->active.ticks},
                           {"recovery_ticks", action->recovery.ticks},
                           {"weapon_config_timing_used", false}});
    }
    return true;
}

void serverWriteWeaponActionSnapshot(const std::string& definitionId, bool active,
    const std::string& actionId, uint32_t startTick, SnapshotEntity& out)
{
    out.equippedWeaponDefNetworkId = weaponDefNetworkIdFor(definitionId);
    out.weaponActionIndex = 255;
    out.weaponActionStartTick = startTick;
    const auto* set = WeaponData::MeleeMoveSetRegistry::instance().get(definitionId);
    if (!active || !set) return;
    for (size_t i = 0; i < set->actions.size() && i < 255; ++i)
        if (set->actions[i].id == actionId) { out.weaponActionIndex = (uint8_t)i; break; }
}

float serverMeleePercentFor(const ServerPlayer& player)
{
    return player.meleePercent;
}

void serverMeleeResetForSpawn(ServerPlayer& player)
{
    player.meleePercent = 0.0f;
    player.meleeActionActive = false;
    player.meleeActionId.clear();
    player.meleePhaseTick = 0;
    player.meleeHitTargets.clear();
    player.meleeLastHitboxCenter = glm::vec3(0.0f);
    player.meleeShieldBroken = false;
    player.meleeShieldDurability = meleeRules().shieldDurability;
    player.meleeGrabVictimId = 0;
    player.meleeGrabPullTicksLeft = 0;
    player.meleeGrabImmuneUntilTick = 0;
    g_meleeActionSnapshots.erase(player.id);
}

void serverMeleeResetNpcForSpawn(uint32_t npcId)
{
    g_npcMelee.erase(npcId);
    g_meleeActionSnapshots.erase(npcId);
}

void serverMeleeResetNpcForSpawn(ServerNpc& npc)
{
    serverMeleeResetNpcForSpawn(npc.entityId);
    npc.meleePercent = 0.0f;
    npc.meleeActionActive = false;
    npc.meleeActionId.clear();
    npc.meleeActionStartTick = 0;
    npc.meleeActionForward = glm::vec3(1.0f, 0.0f, 0.0f);
    npc.meleeActionYaw = 0.0f;
    npc.meleeHitTargets.clear();
    npc.meleeNextAttackTick = 0;
}

void tickServerWeaponAnimationActions(std::unordered_map<uint32_t, ServerPlayer>& players,
    std::unordered_map<uint32_t, ServerNpc>& npcs, uint32_t tick)
{
    // Equipped continuous-contact weapons reuse the same action data and clip
    // timeline. AI supplies action intent; these actions never add percent,
    // shields, attack hitboxes, or self movement.
    for (auto& entry : npcs) {
        ServerNpc& npc = entry.second;
        const auto* set = WeaponData::MeleeMoveSetRegistry::instance().get(npc.equippedWeaponId);
        if (!set || !set->animationOnly) continue;
        const WeaponDefinition* def = WeaponRegistry::instance().get(npc.equippedWeaponId);
        if (!def || npc.health <= 0) { npc.meleeActionActive = false; continue; }
        float nearest = std::numeric_limits<float>::max();
        auto consider = [&](bool self, int team, bool alive, const glm::vec3& position) {
            if (!alive || self) return;
            if (!serverFriendlyFireEnabled() && team >= 0 && npc.matchTeam >= 0 && team == npc.matchTeam) return;
            nearest = std::min(nearest, glm::length(position - npc.pos));
        };
        for (const auto& p : players) consider(false, p.second.matchTeam,
            !p.second.dead && p.second.spawnState == ServerPlayer::Active, p.second.pos);
        for (const auto& n : npcs) consider(n.first == npc.entityId, n.second.matchTeam, n.second.health > 0, n.second.pos);
        const float range = WeaponExecution::paramOr(*def, "lungeRange", 6.0f);
        const auto* action = set->find(npc.meleeActionId);
        if (npc.meleeActionActive && (!action ||
            (!action->holdWhileButton && tick - npc.meleeActionStartTick >= (uint32_t)action->totalTicks()) ||
            (action->holdWhileButton && (nearest > range || tick - npc.meleeActionStartTick >=
                (uint32_t)WeaponExecution::paramOr(*def, "npcGuardTicks", 60.0f))))) {
            npc.meleeActionActive = false;
            npc.meleeActionId.clear();
        }
        if (!npc.meleeActionActive && nearest <= range && tick >= npc.meleeNextAttackTick && !set->actions.empty()) {
            const auto& selected = set->actions[(npc.entityId + tick / 30u) % set->actions.size()];
            npc.meleeActionActive = true;
            npc.meleeActionId = selected.id;
            npc.meleeActionStartTick = tick;
            npc.meleeNextAttackTick = tick + selected.totalTicks() + 20u;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Weapons, StructuredLevel::Important,
                "weapon.action.start", std::to_string(npc.entityId), "NPC selected equipped weapon action", tick,
                {{"actor", npc.entityId}, {"actor_type", "npc"}, {"weapon", def->id},
                 {"action", selected.id}, {"clip", selected.animationClipId}, {"animation_only", true}});
        }
    }

}

void tickServerMeleeActions(SOCKET sock,
                            std::unordered_map<uint32_t, ServerPlayer>& players,
                            std::unordered_map<uint32_t, ServerNpc>& npcs,
                            uint32_t tick, uint64_t& totalPacketsOut)
{
    MimitaMeleeRules::Config::instance().pollReload();
    for (auto& entry : players)
    {
        ServerPlayer& attacker = entry.second;
        if (attacker.dead || attacker.spawnState != ServerPlayer::Active) {
            attacker.meleeActionActive = false;
            g_meleeActionSnapshots.erase(attacker.id);
            continue;
        }
        const WeaponData::MeleeMoveSet* moveSet = equippedMeleeMoveSet(attacker);
        if (attacker.meleeActionActive && (!moveSet || !moveSet->find(attacker.meleeActionId))) {
            attacker.meleeActionActive = false;
            attacker.meleeActionId.clear();
            g_meleeActionSnapshots.erase(attacker.id);
        }
        const bool animationOnly = moveSet && moveSet->animationOnly;
        if (!animationOnly && serverGamemodeState().matchMode != "melee")
            continue;

        // Shield decay/recovery and the grab pull run every Melee tick, whether
        // or not an attack action is currently active.
        if (!animationOnly) {
            updateMeleeShield(attacker, tick, sock, players, totalPacketsOut);
            tickMeleeGrabPull(attacker, players);
        }

        if (!attacker.meleeActionActive)
            continue;

        // Prefer the action snapshot taken at start (retains the starting
        // revision); fall back to the live move set.
        const WeaponData::MeleeAction* action = nullptr;
        auto snapIt = g_meleeActionSnapshots.find(attacker.id);
        if (snapIt != g_meleeActionSnapshots.end())
            action = &snapIt->second;
        else if (moveSet)
            action = moveSet->find(attacker.meleeActionId);
        if (!action) {
            attacker.meleeActionActive = false;
            g_meleeActionSnapshots.erase(attacker.id);
            continue;
        }

        const uint32_t elapsed = tick - attacker.meleeActionStartTick;
        const int windup = action->windup.ticks;
        const int active = action->active.ticks;
        const int recovery = action->recovery.ticks;
        const int total = action->totalTicks();

        if ((int)elapsed >= total && !(animationOnly && action->holdWhileButton)) {
            // action can point into g_meleeActionSnapshots. Copy its ID before
            // erasing that snapshot so the end event never reads a dangling
            // string reference.
            const std::string completedActionId = action->id;
            attacker.meleeActionActive = false;
            g_meleeActionSnapshots.erase(attacker.id);
            emitMeleeActionEvent(attacker, "melee.action.end",
                                 "melee action completed", tick, completedActionId);
            continue;
        }

        const char* phase = "windup";
        int phaseTick = (int)elapsed;
        if ((int)elapsed >= windup + active) {
            phase = "recovery";
            phaseTick = (int)elapsed - windup - active;
        } else if ((int)elapsed >= windup) {
            phase = "active";
            phaseTick = (int)elapsed - windup;
        }
        attacker.meleePhaseTick = phaseTick;

        // Per-action impulses fire on their authored phase/tick.
        if (animationOnly) continue;
        applyActionImpulses(attacker, *action, phase, phaseTick, tick);

        if (std::string(phase) != "active")
            continue;

        // Sweep each authored hitbox that is active this tick.
        for (const WeaponData::MeleeHitbox& authoredHitbox : action->hitboxes) {
            if (phaseTick < authoredHitbox.activeStartTick || phaseTick > authoredHitbox.activeEndTick)
                continue;
            const WeaponData::MeleeHitbox hitbox =
                WeaponData::resolveMeleeHitboxForTick(authoredHitbox, phaseTick);

            const auto sampledCenter = animatedLimbHitboxCenter(
                attacker.avatarName, attacker.pos, attacker.meleeActionYaw,
                *action, phase, phaseTick, (int)elapsed, hitbox);
            if (hitbox.attachmentMode == WeaponData::MeleeHitboxAttachmentMode::AnimatedLimb && !sampledCenter) {
                if (phaseTick == hitbox.activeStartTick)
                    StructuredLogger::instance().writeEvent(
                        StructuredCategory::Duel, StructuredLevel::Errors,
                        "melee.hitbox.pose_unavailable", std::to_string(attacker.id),
                        "animated limb hitbox skipped because its authored pose could not be sampled", tick,
                        nlohmann::json{{"actor", attacker.id}, {"action", action->id},
                                       {"clip", actionClipForPhase(*action, phase)},
                                       {"part", hitbox.attachedPart}, {"mode", "animated_limb"}});
                continue;
            }
            const glm::vec3 center = sampledCenter ? *sampledCenter :
                legacyCameraBasisHitboxCenter(attacker.pos, attacker.meleeActionForward, hitbox);
            if (phaseTick == hitbox.activeStartTick)
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Duel, StructuredLevel::Important,
                    "melee.hitbox.transform", std::to_string(attacker.id),
                    "authoritative hitbox center resolved", tick,
                    nlohmann::json{{"actor", attacker.id}, {"action", action->id},
                                   {"part", hitbox.attachedPart},
                                   {"mode", sampledCenter ? "animated_limb" : "camera_basis"},
                                   {"center", {center.x, center.y, center.z}},
                                   {"camera_basis_used_for_position", !sampledCenter}});
            WeaponExecution::PhysicalContactShape shape;
            shape.kind = WeaponExecution::PhysicalShapeKind::Sphere;
            shape.radius = std::max(0.01f, hitbox.size.x);
            shape.previousA = attacker.meleeLastHitboxCenter == glm::vec3(0.0f)
                ? center : attacker.meleeLastHitboxCenter;
            shape.previousB = shape.previousA;
            shape.currentA = center;
            shape.currentB = center;
            attacker.meleeLastHitboxCenter = center;

            for (auto& targetEntry : players) {
                ServerPlayer& target = targetEntry.second;
                if (target.id == attacker.id || target.dead ||
                    target.spawnState != ServerPlayer::Active)
                    continue;
                if (attacker.meleeHitTargets.count(target.id))
                    continue;
                if (serverMeleeActorProtected(target.id))
                    continue;

                WeaponExecution::PlayerTarget desc;
                desc.playerId = target.id;
                desc.spawnGeneration = target.spawnGeneration;
                desc.position = target.pos;
                desc.radius = PLAYER_RADIUS;
                desc.height = PLAYER_HEIGHT;
                desc.dead = target.dead;

                WeaponExecution::PhysicalContactHit hit;
                if (!WeaponExecution::testPhysicalContact(shape, desc, hit))
                    continue;
                attacker.meleeHitTargets.insert(target.id);
                if (action->isGrab)
                    applyMeleeGrab(attacker, target, *action, tick);
                else {
                    const MeleeAttackerView view{
                        attacker.id, false, attacker.pos, attacker.vel,
                        attacker.meleeActionForward};
                    applyMeleeContactToPlayer(view, target, *action, tick, sock,
                                              players, totalPacketsOut, hit);
                }
            }

            for (auto& targetEntry : npcs) {
                ServerNpc& target = targetEntry.second;
                if (target.entityId == attacker.id || target.health <= 0)
                    continue;
                if (attacker.meleeHitTargets.count(target.entityId))
                    continue;
                if (serverMeleeActorProtected(target.entityId))
                    continue;

                WeaponExecution::PlayerTarget desc;
                desc.playerId = target.entityId;
                desc.spawnGeneration = target.transformEpoch;
                desc.position = target.pos + glm::vec3(0.0f, 0.0f, 0.9f);
                desc.radius = 0.65f;
                desc.height = PLAYER_HEIGHT;
                desc.dead = target.health <= 0;

                WeaponExecution::PhysicalContactHit hit;
                if (!WeaponExecution::testPhysicalContact(shape, desc, hit))
                    continue;
                attacker.meleeHitTargets.insert(target.entityId);
                // NpcMeleeState owns the persistent value; ServerNpc mirrors it
                // for snapshots and presentation later in this tick.
                NpcMeleeState& targetState = g_npcMelee[target.entityId];
                const MeleeAttackerView view{
                    attacker.id, false, attacker.pos, attacker.vel,
                    attacker.meleeActionForward};
                applyMeleeContactToNpc(view, target, targetState, *action, tick,
                                       sock, players, totalPacketsOut, hit);
            }
        }
    }

    // ── Melee NPC fighters ──────────────────────────────────────────────
    // An NPC whose equipped weapon is a Melee move set fights with the same
    // data-driven actions, percent, and knockback as a player.
    tickServerWeaponAnimationActions(players, npcs, tick);

    if (serverGamemodeState().matchMode == "melee") {
        for (auto& nEntry : npcs) {
            ServerNpc& npc = nEntry.second;
            NpcMeleeState& st = g_npcMelee[npc.entityId];
            if (npc.health <= 0) {
                st.active = false;
                npc.meleeActionActive = false;
                npc.meleeActionId.clear();
                g_meleeActionSnapshots.erase(npc.entityId);
                g_npcMelee.erase(npc.entityId);
                continue;
            }
            npc.meleePercent = st.percent;
            if (serverMeleeActorProtected(npc.entityId))
                continue;

            const WeaponData::MeleeMoveSet* ms = nullptr;
            {
                const ActorSpawnProfile prof = serverResolveActorSpawnProfile(npc.entityId);
                if (!prof.startingWeapon.empty())
                    ms = WeaponData::MeleeMoveSetRegistry::instance().get(prof.startingWeapon);
            }

            if (st.active) {
                const WeaponData::MeleeAction* action = nullptr;
                auto sit = g_meleeActionSnapshots.find(npc.entityId);
                if (sit != g_meleeActionSnapshots.end())
                    action = &sit->second;
                else if (ms)
                    action = ms->find(st.actionId);
                if (!action) {
                    st.active = false;
                    npc.meleeActionActive = false;
                    npc.meleeActionId.clear();
                    g_meleeActionSnapshots.erase(npc.entityId);
                } else {
                    const uint32_t elapsed = tick - st.startTick;
                    const int windup = action->windup.ticks;
                    const int active = action->active.ticks;
                    const int total = action->totalTicks();
                    if ((int)elapsed >= total || (int)elapsed < 0) {
                        st.active = false;
                        npc.meleeActionActive = false;
                        npc.meleeActionId.clear();
                        g_meleeActionSnapshots.erase(npc.entityId);
                    } else if ((int)elapsed >= windup && (int)elapsed < windup + active) {
                        const int phaseTick = (int)elapsed - windup;
                        for (const WeaponData::MeleeHitbox& authoredHitbox : action->hitboxes) {
                            if (phaseTick < authoredHitbox.activeStartTick || phaseTick > authoredHitbox.activeEndTick)
                                continue;
                            const WeaponData::MeleeHitbox hitbox =
                                WeaponData::resolveMeleeHitboxForTick(authoredHitbox, phaseTick);
                            const auto sampledCenter = animatedLimbHitboxCenter(
                                npc.avatarName, npc.pos, st.modelYaw, *action, "active",
                                phaseTick, (int)elapsed, hitbox);
                            if (hitbox.attachmentMode == WeaponData::MeleeHitboxAttachmentMode::AnimatedLimb && !sampledCenter) {
                                if (phaseTick == hitbox.activeStartTick)
                                    StructuredLogger::instance().writeEvent(
                                        StructuredCategory::Duel, StructuredLevel::Errors,
                                        "melee.hitbox.pose_unavailable", std::to_string(npc.entityId),
                                        "NPC animated limb hitbox skipped because its authored pose could not be sampled", tick,
                                        nlohmann::json{{"actor", npc.entityId}, {"action", action->id},
                                                       {"clip", actionClipForPhase(*action, "active")},
                                                       {"part", hitbox.attachedPart}, {"mode", "animated_limb"}});
                                continue;
                            }
                            const glm::vec3 center = sampledCenter ? *sampledCenter :
                                legacyCameraBasisHitboxCenter(npc.pos, st.forward, hitbox);
                            WeaponExecution::PhysicalContactShape shape;
                            shape.kind = WeaponExecution::PhysicalShapeKind::Sphere;
                            shape.radius = std::max(0.01f, hitbox.size.x);
                            shape.previousA = st.lastHitboxCenter == glm::vec3(0.0f) ? center : st.lastHitboxCenter;
                            shape.previousB = center;
                            shape.currentA = center;
                            shape.currentB = center;
                            st.lastHitboxCenter = center;
                            for (auto& pEntry : players) {
                                ServerPlayer& target = pEntry.second;
                                if (target.dead || target.spawnState != ServerPlayer::Active)
                                    continue;
                                if (st.hitTargets.count(target.id))
                                    continue;
                                if (serverMeleeActorProtected(target.id))
                                    continue;
                                WeaponExecution::PlayerTarget desc;
                                desc.playerId = target.id;
                                desc.spawnGeneration = target.spawnGeneration;
                                desc.position = target.pos;
                                desc.radius = PLAYER_RADIUS;
                                desc.height = PLAYER_HEIGHT;
                                desc.dead = target.dead;
                                WeaponExecution::PhysicalContactHit hit;
                                const bool contactHits =
                                    WeaponExecution::testPhysicalContact(shape, desc, hit);
                                if (phaseTick == hitbox.activeStartTick) {
                                    StructuredLogger::instance().writeEvent(
                                        StructuredCategory::Duel, StructuredLevel::Important,
                                        "melee.npc.sweep", std::to_string(npc.entityId),
                                        "npc melee active-tick sweep", tick,
                                        nlohmann::json{{"npc", npc.entityId},
                                                       {"target", target.id},
                                                       {"center", {center.x, center.y, center.z}},
                                                       {"radius", shape.radius},
                                                       {"target_pos", {target.pos.x, target.pos.y, target.pos.z}},
                                                       {"dist", glm::length(center - target.pos)},
                                                       {"hit", contactHits}});
                                }
                                if (!contactHits)
                                    continue;
                                st.hitTargets.insert(target.id);
                                if (!action->isGrab) {
                                    const MeleeAttackerView view{
                                        npc.entityId, true, npc.pos, npc.vel,
                                        st.forward};
                                    applyMeleeContactToPlayer(view, target,
                                                              *action, tick, sock,
                                                              players,
                                                              totalPacketsOut, hit);
                                }
                            }
                        }
                    }
                }
            }

            // The configured cadence is start-to-start. If the previous move
            // is still running when the next decision is due, starting the new
            // action below replaces its snapshot and interrupts its animation.
            if (ms && tick >= st.nextAttackTick) {
                const ServerPlayer* target = nullptr;
                float best = 1e9f;
                for (const auto& pEntry : players) {
                    const ServerPlayer& p = pEntry.second;
                    if (p.dead || p.spawnState != ServerPlayer::Active)
                        continue;
                    const float d = glm::length(glm::vec2(p.pos.x - npc.pos.x, p.pos.y - npc.pos.y));
                    if (d < best) { best = d; target = &p; }
                }
                const ActorSpawnProfile actorProfile = serverResolveActorSpawnProfile(npc.entityId);
                const BehaviorProfileDefinition* behavior =
                    BehaviorProfileRegistry::instance().get(actorProfile.behaviorProfileId);
                const float attackRange = behavior && behavior->meleeAttackRangeMeters >= 0.0f
                    ? behavior->meleeAttackRangeMeters : 4.5f;
                const int attackGapMinTicks = behavior && behavior->meleeAttackGapMinTicks >= 1
                    ? behavior->meleeAttackGapMinTicks : 20;
                const int attackGapMaxTicks = behavior && behavior->meleeAttackGapMaxTicks >= 1
                    ? behavior->meleeAttackGapMaxTicks : attackGapMinTicks;
                if (target && best <= attackRange) {
                    const bool grounded = npc.onGround;
                    const std::string fallback = grounded ? "a_ground_punch" : "a_air_kick";
                    std::string actionId = fallback;
                    float selectedWeight = 0.0f;
                    if (behavior && !behavior->meleeMoveWeights.empty()) {
                        float totalWeight = 0.0f;
                        auto eligible = [&](const std::string& id) {
                            const auto* candidate = ms->find(id);
                            return candidate && (candidate->actorState == WeaponData::MeleeActorState::Any ||
                                (grounded && candidate->actorState == WeaponData::MeleeActorState::Grounded) ||
                                (!grounded && candidate->actorState == WeaponData::MeleeActorState::Airborne));
                        };
                        for (const auto& move : behavior->meleeMoveWeights)
                            if (eligible(move.first)) totalWeight += move.second;
                        if (totalWeight > 0.0f) {
                            const uint32_t bucket = (npc.entityId * 2654435761u + tick * 2246822519u)
                                % 100000u;
                            float choice = totalWeight * (float)bucket / 100000.0f;
                            for (const auto& move : behavior->meleeMoveWeights) {
                                if (!eligible(move.first)) continue;
                                choice -= move.second;
                                if (choice < 0.0f) {
                                    actionId = move.first;
                                    selectedWeight = move.second;
                                    break;
                                }
                            }
                        }
                    }
                    if (const WeaponData::MeleeAction* action = ms->find(actionId)) {
                        if (selectedWeight <= 0.0f && behavior) {
                            const auto weight = behavior->meleeMoveWeights.find(actionId);
                            if (weight != behavior->meleeMoveWeights.end())
                                selectedWeight = weight->second;
                        }
                        glm::vec3 f = target->pos - npc.pos;
                        f.z = 0.0f;
                        if (glm::length(f) < 0.001f) {
                            f = glm::vec3(npc.aim.x, npc.aim.y, 0.0f);
                        }
                        if (glm::length(f) < 0.001f)
                            f = glm::vec3(1.0f, 0.0f, 0.0f);
                        const uint32_t gapSpan = static_cast<uint32_t>(
                            attackGapMaxTicks - attackGapMinTicks + 1);
                        const uint32_t gapHash = npc.entityId * 2654435761u
                            + tick * 2246822519u;
                        const int attackGapTicks = attackGapMinTicks
                            + static_cast<int>(gapHash % gapSpan);
                        startNpcMeleeAction(npc, st, *action, glm::normalize(f), tick,
                            actorProfile.behaviorProfileId, best, attackRange,
                            attackGapTicks, selectedWeight);
                    }
                }
            }
        }
    }
}

} // namespace MimitaNet
