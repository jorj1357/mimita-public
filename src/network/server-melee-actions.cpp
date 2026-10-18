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
#include "combat/weapon-registry.h"
#include "combat/weapon-melee-config.h"
#include "combat/weapon-execution.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "physics/movement/movement-step.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <string>

#include <nlohmann/json.hpp>

namespace MimitaNet {
namespace {

// Provisional, JSON-editable Melee rules from config/melee.json (shield, grab,
// contact force caps). Loaded once; a missing/invalid file keeps the defaults.
struct MeleeRules {
    float shieldDurability = 50.0f;
    float shieldDecayTicks = 300.0f;
    float shieldBreakRecoveryTicks = 120.0f;
    float shieldReleaseRecoveryTicks = 90.0f;
    float shieldBreakUpImpulse = 5.0f;
    float grabRadius = 1.0f;
    int grabPullTicks = 30;
    uint32_t grabAntiChainTicks = 300;
    float forceMaxImpulse = 120.0f;
};

MeleeRules loadMeleeRules()
{
    MeleeRules r;
    std::ifstream file("config/melee.json");
    if (!file.is_open())
        return r;
    try {
        nlohmann::json j;
        file >> j;
        if (j.contains("shield") && j["shield"].is_object()) {
            const auto& s = j["shield"];
            r.shieldDurability = s.value("max_durability", r.shieldDurability);
            r.shieldDecayTicks = s.value("decay_ticks", r.shieldDecayTicks);
            r.shieldBreakRecoveryTicks = s.value("break_recovery_ticks", r.shieldBreakRecoveryTicks);
            r.shieldReleaseRecoveryTicks = s.value("release_recovery_ticks", r.shieldReleaseRecoveryTicks);
            r.shieldBreakUpImpulse = s.value("break_upward_impulse", r.shieldBreakUpImpulse);
        }
        if (j.contains("grab") && j["grab"].is_object()) {
            const auto& g = j["grab"];
            r.grabRadius = g.value("radius", r.grabRadius);
            r.grabPullTicks = std::max(1, g.value("pull_ticks", r.grabPullTicks));
            r.grabAntiChainTicks = (uint32_t)std::max(0, g.value("anti_chain_ticks", (int)r.grabAntiChainTicks));
        }
        if (j.contains("force") && j["force"].is_object())
            r.forceMaxImpulse = j["force"].value("max_impulse", r.forceMaxImpulse);
    } catch (...) {
        // keep defaults
    }
    return r;
}

const MeleeRules& meleeRules()
{
    static const MeleeRules r = loadMeleeRules();
    return r;
}

static constexpr float kMeleeReferenceSpeed = 20.0f;

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
    std::unordered_set<uint32_t> hitTargets;
    float percent = 0.0f;
    uint32_t nextAttackTick = 0;
};
std::unordered_map<uint32_t, NpcMeleeState> g_npcMelee;

static const WeaponData::MeleeMoveSet* equippedMeleeMoveSet(const ServerPlayer& player)
{
    for (const std::string& id : player.ownedWeaponIds)
    {
        const WeaponDefinition* def = WeaponRegistry::instance().get(id);
        if (def && def->slot == player.equippedSlot && serverCommunityWeaponAllowed(player, def->id))
            return WeaponData::MeleeMoveSetRegistry::instance().get(def->id);
    }
    return nullptr;
}

static glm::vec3 horizontalForward(const glm::vec3& raw)
{
    glm::vec3 forward(raw.x, raw.y, 0.0f);
    if (!std::isfinite(forward.x) || !std::isfinite(forward.y) ||
        glm::length(forward) < 0.001f)
        return glm::vec3(1.0f, 0.0f, 0.0f);
    return glm::normalize(forward);
}

// Approximate the world position of a hitbox anchored to a named rig part.
// The server does not simulate the animated part transforms, so the part base
// is approximated from the actor root plus a per-part shoulder/foot/hip offset,
// then extended along the captured attack basis by the hitbox local offset.
static glm::vec3 meleeHitboxCenter(const ServerPlayer& attacker,
                                   const WeaponData::MeleeHitbox& hitbox)
{
    const glm::vec3 up(0.0f, 0.0f, 1.0f);
    const glm::vec3 forward = attacker.meleeActionForward;
    glm::vec3 right = glm::cross(forward, up);
    if (glm::length(right) < 0.001f)
        right = glm::vec3(0.0f, 1.0f, 0.0f);
    right = glm::normalize(right);

    const std::string& part = hitbox.attachedPart;
    glm::vec3 base = attacker.pos + up * 1.2f;  // default shoulder height
    if (part == "leftLeg" || part == "rightLeg")
        base = attacker.pos + up * 0.5f;
    else if (part == "torso")
        base = attacker.pos + up * 1.1f;
    else if (part == "head")
        base = attacker.pos + up * 1.7f;

    // local_offset is expressed in the captured attack basis: x=right, z=forward,
    // y=up (the authored Melee convention).
    return base
        + forward * (0.6f + hitbox.localOffset.z)
        + right * hitbox.localOffset.x
        + up * hitbox.localOffset.y;
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

static void updateMeleeShield(ServerPlayer& p, uint32_t tick)
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
    const glm::vec3& direction, const glm::vec3& normal, float percentAdded)
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
    // The common impact renderer uses this as visual intensity only. Melee
    // percent remains authoritative in actor snapshots and never changes HP.
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

    const ReliableGameplayEventQueueResult result = queueReliableGameplayEventToAll(
        sock, players, &event, sizeof(event), eventId,
        event.eventSessionId, totalPacketsOut);
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Duel, StructuredLevel::Important,
        "melee.feedback.queued", std::to_string(eventId),
        "confirmed Melee impact feedback queued", tick,
        nlohmann::json{{"attacker", attackerId}, {"target", targetId},
                       {"percent_added", percentAdded},
                       {"result", (int)result}});
}

static void applyMeleeHit(ServerPlayer& attacker, ServerPlayer& target,
                          const WeaponData::MeleeAction& action,
                          uint32_t tick, SOCKET sock,
                          std::unordered_map<uint32_t, ServerPlayer>& players,
                          uint64_t& totalPacketsOut,
                          const WeaponExecution::PhysicalContactHit& contact)
{
    const MeleeRules& rules = meleeRules();
    const glm::vec3 up(0.0f, 0.0f, 1.0f);
    const glm::vec3 forward = attacker.meleeActionForward;

    glm::vec3 dir = target.pos - attacker.pos;
    dir.z = 0.0f;
    if (glm::length(dir) < 0.001f)
        dir = forward;
    dir = glm::normalize(dir);

    const float speed = glm::length(glm::vec3(attacker.vel.x, attacker.vel.y, 0.0f));
    const float speedFactor = std::clamp(speed / kMeleeReferenceSpeed, 0.0f, 1.0f);
    const float directness = std::clamp(glm::dot(dir, forward), 0.0f, 1.0f);
    // Slow/glancing -> weak; fast/direct -> strong.
    const float force = std::clamp(0.35f + 0.65f * directness * (0.5f + 0.5f * speedFactor),
                                   0.0f, 1.5f);
    const float percentScale = 1.0f + std::max(0.0f, target.meleePercent) / 100.0f;

    // Melee shield: a held, unbroken shield scales received percent/knockback by
    // (1 - remainingFraction) measured before this hit, and absorbs the move's
    // shield damage. Excess shield damage does not spill into fighter percent.
    const bool shielded = target.input.shieldHeld && !target.meleeShieldBroken &&
                          target.meleeShieldDurability > 0.0f;
    float multiplier = 1.0f;
    if (shielded) {
        const float remaining = std::clamp(
            target.meleeShieldDurability / std::max(1.0f, rules.shieldDurability), 0.0f, 1.0f);
        multiplier = 1.0f - remaining;
    }

    const float percentAdd = action.basePercent * force * percentScale * multiplier;
    float kbMagnitude = action.baseKnockback * action.knockbackForceScale * force * percentScale * multiplier;
    kbMagnitude = std::min(kbMagnitude, rules.forceMaxImpulse);
    glm::vec3 impulse = dir * kbMagnitude +
        up * (kbMagnitude * action.knockbackVerticalFraction);

    target.meleePercent += percentAdd;
    target.vel += impulse;
    recordServerMovementExternalImpulse(target, impulse);

    if (shielded) {
        const float shieldDamage = action.hasShieldDamage ? action.shieldDamage : action.basePercent;
        target.meleeShieldDurability -= shieldDamage;
        if (target.meleeShieldDurability <= 0.0f) {
            target.meleeShieldDurability = 0.0f;
            target.meleeShieldBroken = true;
            const glm::vec3 breakImpulse = up * rules.shieldBreakUpImpulse;
            target.vel += breakImpulse;
            recordServerMovementExternalImpulse(target, breakImpulse);
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Duel, StructuredLevel::Important,
                "melee.shield_break", std::to_string(target.id),
                "shield broke from an incoming hit", tick,
                nlohmann::json{{"attacker", attacker.id},
                               {"target_actor", target.id},
                               {"action", action.id},
                               {"shield_damage", shieldDamage}});
        } else {
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Duel, StructuredLevel::Important,
                "melee.shield_hit", std::to_string(target.id),
                "attack absorbed by shield", tick,
                nlohmann::json{{"attacker", attacker.id},
                               {"target_actor", target.id},
                               {"action", action.id},
                               {"multiplier", multiplier},
                               {"shield_damage", shieldDamage},
                               {"shield_remaining", target.meleeShieldDurability}});
        }
    }

    StructuredLogger::instance().writeEvent(
        StructuredCategory::Duel, StructuredLevel::Important,
        "melee.hit", std::to_string(attacker.id),
        "melee action confirmed a swept hitbox contact", tick,
        nlohmann::json{{"attacker", attacker.id},
                       {"target_actor", target.id},
                       {"action", action.id},
                       {"percent_added", percentAdd},
                       {"percent_after", target.meleePercent},
                       {"impulse", {impulse.x, impulse.y, impulse.z}},
                       {"speed", speed},
                       {"directness", directness},
                       {"force", force},
                       {"shielded", shielded},
                       {"spawn_generation", target.spawnGeneration}});
    emitMeleeHitFeedback(sock, players, totalPacketsOut, tick, attacker.id,
                         target.id, contact.hitPosition, dir, contact.normal,
                         percentAdd);
    Debug::log(Debug::Category::Duel,
        "[MELEE HIT] attacker=%u target=%u action=%s percent=%.1f->%.1f kb=%.2f shield=%d\n",
        attacker.id, target.id, action.id.c_str(),
        target.meleePercent - percentAdd, target.meleePercent, kbMagnitude, (int)shielded);
}

// ── NPC Melee fighter helpers ─────────────────────────────────────────
static glm::vec3 meleeNpcHitboxCenter(const ServerNpc& npc, const glm::vec3& forward,
                                      const WeaponData::MeleeHitbox& hitbox)
{
    const glm::vec3 up(0.0f, 0.0f, 1.0f);
    glm::vec3 right = glm::cross(forward, up);
    if (glm::length(right) < 0.001f)
        right = glm::vec3(0.0f, 1.0f, 0.0f);
    right = glm::normalize(right);
    const std::string& part = hitbox.attachedPart;
    glm::vec3 base = npc.pos + up * 1.2f;
    if (part == "leftLeg" || part == "rightLeg")
        base = npc.pos + up * 0.5f;
    else if (part == "torso")
        base = npc.pos + up * 1.1f;
    else if (part == "head")
        base = npc.pos + up * 1.7f;
    return base + forward * (0.6f + hitbox.localOffset.z) +
           right * hitbox.localOffset.x + up * hitbox.localOffset.y;
}

static void applyMeleeNpcHit(ServerNpc& attacker, const glm::vec3& forward, ServerPlayer& target,
                             const WeaponData::MeleeAction& action, uint32_t tick,
                             SOCKET sock,
                             std::unordered_map<uint32_t, ServerPlayer>& players,
                             uint64_t& totalPacketsOut,
                             const WeaponExecution::PhysicalContactHit& contact)
{
    const MeleeRules& rules = meleeRules();
    const glm::vec3 up(0.0f, 0.0f, 1.0f);
    glm::vec3 dir = target.pos - attacker.pos;
    dir.z = 0.0f;
    if (glm::length(dir) < 0.001f)
        dir = forward;
    dir = glm::normalize(dir);

    const float speed = glm::length(glm::vec3(attacker.vel.x, attacker.vel.y, 0.0f));
    const float speedFactor = std::clamp(speed / kMeleeReferenceSpeed, 0.0f, 1.0f);
    const float directness = std::clamp(glm::dot(dir, forward), 0.0f, 1.0f);
    const float force = std::clamp(0.35f + 0.65f * directness * (0.5f + 0.5f * speedFactor),
                                   0.0f, 1.5f);
    const float percentScale = 1.0f + std::max(0.0f, target.meleePercent) / 100.0f;

    const bool shielded = target.input.shieldHeld && !target.meleeShieldBroken &&
                          target.meleeShieldDurability > 0.0f;
    float multiplier = 1.0f;
    if (shielded) {
        const float remaining = std::clamp(
            target.meleeShieldDurability / std::max(1.0f, rules.shieldDurability), 0.0f, 1.0f);
        multiplier = 1.0f - remaining;
    }

    const float percentAdd = action.basePercent * force * percentScale * multiplier;
    float kbMagnitude = action.baseKnockback * action.knockbackForceScale * force * percentScale * multiplier;
    kbMagnitude = std::min(kbMagnitude, rules.forceMaxImpulse);
    const glm::vec3 impulse = dir * kbMagnitude +
        up * (kbMagnitude * action.knockbackVerticalFraction);

    target.meleePercent += percentAdd;
    target.vel += impulse;
    recordServerMovementExternalImpulse(target, impulse);

    if (shielded) {
        const float shieldDamage = action.hasShieldDamage ? action.shieldDamage : action.basePercent;
        target.meleeShieldDurability -= shieldDamage;
        if (target.meleeShieldDurability <= 0.0f) {
            target.meleeShieldDurability = 0.0f;
            target.meleeShieldBroken = true;
            const glm::vec3 breakImpulse = up * rules.shieldBreakUpImpulse;
            target.vel += breakImpulse;
            recordServerMovementExternalImpulse(target, breakImpulse);
        }
    }

    StructuredLogger::instance().writeEvent(
        StructuredCategory::Duel, StructuredLevel::Important,
        "melee.hit", std::to_string(attacker.entityId),
        "npc melee action confirmed a swept hitbox contact", tick,
        nlohmann::json{{"attacker", attacker.entityId},
                       {"attacker_is_npc", true},
                       {"target_actor", target.id},
                       {"action", action.id},
                       {"percent_added", percentAdd},
                       {"percent_after", target.meleePercent},
                       {"impulse", {impulse.x, impulse.y, impulse.z}},
                       {"force", force},
                       {"shielded", shielded}});
    emitMeleeHitFeedback(sock, players, totalPacketsOut, tick, attacker.entityId,
                         target.id, contact.hitPosition, dir, contact.normal,
                         percentAdd);
}

static void startNpcMeleeAction(ServerNpc& npc, NpcMeleeState& st,
                                const WeaponData::MeleeAction& action,
                                const glm::vec3& forward, uint32_t tick)
{
    st.active = true;
    st.actionId = action.id;
    st.startTick = tick;
    st.forward = forward;
    st.hitTargets.clear();
    g_meleeActionSnapshots[npc.entityId] = action;
    const int total = action.totalTicks();
    st.nextAttackTick = tick + (uint32_t)std::max(1, total) + 20u;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Duel, StructuredLevel::Important,
        "melee.action.start", std::to_string(npc.entityId),
        "npc melee action started", tick,
        nlohmann::json{{"actor", npc.entityId},
                       {"actor_is_npc", true},
                       {"action", action.id},
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

void tickServerMeleeActions(SOCKET sock,
                            std::unordered_map<uint32_t, ServerPlayer>& players,
                            std::unordered_map<uint32_t, ServerNpc>& npcs,
                            uint32_t tick, uint64_t& totalPacketsOut)
{
    for (auto& entry : players)
    {
        ServerPlayer& attacker = entry.second;
        if (attacker.dead || attacker.spawnState != ServerPlayer::Active) {
            attacker.meleeActionActive = false;
            g_meleeActionSnapshots.erase(attacker.id);
            continue;
        }
        if (serverGamemodeState().matchMode != "melee")
            continue;

        // Shield decay/recovery and the grab pull run every Melee tick, whether
        // or not an attack action is currently active.
        updateMeleeShield(attacker, tick);
        tickMeleeGrabPull(attacker, players);

        if (!attacker.meleeActionActive)
            continue;

        const WeaponData::MeleeMoveSet* moveSet = equippedMeleeMoveSet(attacker);
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

        if ((int)elapsed >= total) {
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
        applyActionImpulses(attacker, *action, phase, phaseTick, tick);

        if (std::string(phase) != "active")
            continue;

        // Sweep each authored hitbox that is active this tick.
        for (const WeaponData::MeleeHitbox& authoredHitbox : action->hitboxes) {
            if (phaseTick < authoredHitbox.activeStartTick || phaseTick > authoredHitbox.activeEndTick)
                continue;
            const WeaponData::MeleeHitbox hitbox =
                WeaponData::resolveMeleeHitboxForTick(authoredHitbox, phaseTick);

            const glm::vec3 center = meleeHitboxCenter(attacker, hitbox);
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
                else
                    applyMeleeHit(attacker, target, *action, tick, sock, players,
                                  totalPacketsOut, hit);
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
                // NPCs use the same Melee percent and knockback result policy
                // as player targets. NPC decision-making is the only actor-kind
                // difference; confirmed contact has one result path.
                const glm::vec3 forward = attacker.meleeActionForward;
                glm::vec3 dir = target.pos - attacker.pos;
                dir.z = 0.0f;
                if (glm::length(dir) < 0.001f) dir = forward;
                dir = glm::normalize(dir);
                const float speed = glm::length(glm::vec3(attacker.vel.x, attacker.vel.y, 0.0f));
                const float speedFactor = std::clamp(speed / kMeleeReferenceSpeed, 0.0f, 1.0f);
                const float directness = std::clamp(glm::dot(dir, forward), 0.0f, 1.0f);
                const float force = std::clamp(0.35f + 0.65f * directness * (0.5f + 0.5f * speedFactor),
                                               0.0f, 1.5f);
                // NpcMeleeState owns the persistent value; ServerNpc mirrors it
                // for snapshots and presentation later in this tick.
                NpcMeleeState& targetState = g_npcMelee[target.entityId];
                const float percentScale = 1.0f + std::max(0.0f, targetState.percent) / 100.0f;
                const float percentAdd = action->basePercent * force * percentScale;
                const float kb = action->baseKnockback * action->knockbackForceScale * force * percentScale;
                targetState.percent += percentAdd;
                target.meleePercent = targetState.percent;
                target.knockbackImpulse += dir * kb + glm::vec3(0.0f, 0.0f, kb * action->knockbackVerticalFraction);
                emitMeleeHitFeedback(sock, players, totalPacketsOut, tick,
                                     attacker.id, target.entityId,
                                     hit.hitPosition, dir, hit.normal, percentAdd);
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Duel, StructuredLevel::Important,
                    "melee.hit", std::to_string(attacker.id),
                    "melee action confirmed a swept hitbox contact (npc)", tick,
                    nlohmann::json{{"attacker", attacker.id},
                                   {"target_actor", target.entityId},
                                   {"action", action->id},
                                   {"target_is_npc", true},
                                   {"percent_added", percentAdd},
                                   {"percent_after", target.meleePercent},
                                   {"impulse", {dir.x * kb, dir.y * kb, kb * action->knockbackVerticalFraction}},
                                   {"force", force}});
            }
        }
    }

    // ── Melee NPC fighters ──────────────────────────────────────────────
    // An NPC whose equipped weapon is a Melee move set fights with the same
    // data-driven actions, percent, and knockback as a player.
    if (serverGamemodeState().matchMode == "melee") {
        for (auto& nEntry : npcs) {
            ServerNpc& npc = nEntry.second;
            NpcMeleeState& st = g_npcMelee[npc.entityId];
            if (npc.health <= 0) {
                st.active = false;
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
                    g_meleeActionSnapshots.erase(npc.entityId);
                } else {
                    const uint32_t elapsed = tick - st.startTick;
                    const int windup = action->windup.ticks;
                    const int active = action->active.ticks;
                    const int total = action->totalTicks();
                    if ((int)elapsed >= total || (int)elapsed < 0) {
                        st.active = false;
                        g_meleeActionSnapshots.erase(npc.entityId);
                    } else if ((int)elapsed >= windup && (int)elapsed < windup + active) {
                        const int phaseTick = (int)elapsed - windup;
                        for (const WeaponData::MeleeHitbox& authoredHitbox : action->hitboxes) {
                            if (phaseTick < authoredHitbox.activeStartTick || phaseTick > authoredHitbox.activeEndTick)
                                continue;
                            const WeaponData::MeleeHitbox hitbox =
                                WeaponData::resolveMeleeHitboxForTick(authoredHitbox, phaseTick);
                            const glm::vec3 center = meleeNpcHitboxCenter(npc, st.forward, hitbox);
                            WeaponExecution::PhysicalContactShape shape;
                            shape.kind = WeaponExecution::PhysicalShapeKind::Sphere;
                            shape.radius = std::max(0.01f, hitbox.size.x);
                            shape.previousA = center;
                            shape.previousB = center;
                            shape.currentA = center;
                            shape.currentB = center;
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
                                if (!action->isGrab)
                                    applyMeleeNpcHit(npc, st.forward, target, *action,
                                                     tick, sock, players,
                                                     totalPacketsOut, hit);
                            }
                        }
                    }
                }
            }

            // Decide a new action when idle: chase the nearest player and swing.
            if (ms && !st.active && tick >= st.nextAttackTick) {
                const ServerPlayer* target = nullptr;
                float best = 1e9f;
                for (const auto& pEntry : players) {
                    const ServerPlayer& p = pEntry.second;
                    if (p.dead || p.spawnState != ServerPlayer::Active)
                        continue;
                    const float d = glm::length(glm::vec2(p.pos.x - npc.pos.x, p.pos.y - npc.pos.y));
                    if (d < best) { best = d; target = &p; }
                }
                if (target && best < 4.5f) {
                    const bool grounded = npc.onGround;
                    std::string actionId = grounded ? "a_ground_punch" : "a_air_kick";
                    if (((npc.entityId + tick / 30u) % 5u) == 0u)
                        actionId = grounded ? "b_falcon_punch" : "b_air_forward_dash";
                    if (const WeaponData::MeleeAction* action = ms->find(actionId)) {
                        glm::vec3 f = target->pos - npc.pos;
                        f.z = 0.0f;
                        if (glm::length(f) < 0.001f) {
                            f = glm::vec3(npc.aim.x, npc.aim.y, 0.0f);
                        }
                        if (glm::length(f) < 0.001f)
                            f = glm::vec3(1.0f, 0.0f, 0.0f);
                        startNpcMeleeAction(npc, st, *action, glm::normalize(f), tick);
                    }
                }
            }
        }
    }
}

} // namespace MimitaNet
