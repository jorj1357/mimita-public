// 07 21 2026, 21 15
/* purpose
* Owns authoritative server ticking for generic physical-contact weapons.
* Simulates Godball and Swordsword as config-driven contact shapes with episode batching.
* Applies damage, knockback, movement contact history, and confirmed damage events server-side.
* Does NOT receive weapon-specific contact packets, trust client damage, or render weapon effects.
* Does NOT migrate rocket, grenade, or other projectile weapon architecture.
* Does NOT run local-only client prediction or single-player collision presentation.
*/

#include "network/server.h"
#include "network/network-weapons.h"
#include "combat/weapon-execution.h"
#include "combat/weapon-registry.h"
#include "debug/debug-log.h"
#include "physics/movement/movement-step.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace MimitaNet {
namespace {

static constexpr uint8_t CONTACT_CONFIRM_BATCH = 4;

static const WeaponDefinition* equippedWeaponDefinition(const ServerPlayer& player)
{
    for (const std::string& id : player.ownedWeaponIds)
    {
        const WeaponDefinition* def = WeaponRegistry::instance().get(id);
        if (def && def->slot == player.equippedSlot)
            return def;
    }
    return nullptr;
}

static glm::vec3 safeForward(const ServerPlayer& player)
{
    glm::vec3 forward = player.input.camForward;
    if (!std::isfinite(forward.x) || !std::isfinite(forward.y) ||
        !std::isfinite(forward.z) || glm::length(forward) < 0.001f)
        return glm::vec3(1.0f, 0.0f, 0.0f);
    forward = glm::normalize(forward);
    forward.z = 0.0f;
    if (glm::length(forward) < 0.001f)
        return glm::vec3(1.0f, 0.0f, 0.0f);
    return glm::normalize(forward);
}

static float positiveParam(const WeaponDefinition& def, const char* key, float fallback)
{
    const float value = WeaponExecution::paramOr(def, key, fallback);
    return std::max(0.0f, value);
}

static uint32_t intervalTicks(float seconds)
{
    if (seconds <= 0.0f)
        return 1;
    return std::max<uint32_t>(1, (uint32_t)std::ceil(seconds * SERVER_TICK_RATE));
}

static void clearPhysicalRuntime(ServerPlayer& player)
{
    player.hasLastPhysicalWeaponShape = false;
    player.lastPhysicalWeaponDefNetworkId = 0;
    player.physicalContactEpisodes.clear();
}

static bool isSwordActive(const SwordswordState& state)
{
    return state.state == SwordswordState::AttackState::SlashActive ||
           state.state == SwordswordState::AttackState::LungeActive;
}

static bool isSwordLunge(const SwordswordState& state)
{
    return state.state == SwordswordState::AttackState::LungeActive;
}

static void advanceSwordState(ServerPlayer& player, const WeaponDefinition& def, float dt)
{
    if (player.meleeCooldownTimer > 0.0f)
        player.meleeCooldownTimer = std::max(0.0f, player.meleeCooldownTimer - dt);

    SwordswordState& ss = player.swordswordState;
    const float slashWindup = std::max(0.001f, positiveParam(def, "slashWindupTime", 0.08f));
    const float slashActive = std::max(0.001f, positiveParam(def, "slashActiveTime", 0.15f));
    const float slashRecover = std::max(0.001f, positiveParam(def, "slashRecoverTime", 0.10f));
    const float lungeWindup = std::max(0.001f, positiveParam(def, "lungeWindupTime", 0.10f));
    const float lungeActive = std::max(0.001f, positiveParam(def, "lungeActiveTime", 0.20f));
    const float lungeRecover = std::max(0.001f, positiveParam(def, "lungeRecoverTime", 0.12f));

    switch (ss.state)
    {
    case SwordswordState::AttackState::SlashWindup:
        ss.stateTimer += dt;
        ss.animTimer = ss.stateTimer / slashWindup;
        if (ss.stateTimer >= slashWindup)
        {
            ss.state = SwordswordState::AttackState::SlashActive;
            ss.stateTimer = 0.0f;
        }
        break;
    case SwordswordState::AttackState::SlashActive:
        ss.stateTimer += dt;
        ss.animTimer = ss.stateTimer / slashActive;
        if (ss.stateTimer >= slashActive)
        {
            ss.state = SwordswordState::AttackState::SlashRecover;
            ss.stateTimer = 0.0f;
        }
        break;
    case SwordswordState::AttackState::SlashRecover:
        ss.stateTimer += dt;
        ss.animTimer = ss.stateTimer / slashRecover;
        if (ss.stateTimer >= slashRecover)
        {
            ss = SwordswordState{};
            player.meleeCooldownTimer = 0.0f;
        }
        break;
    case SwordswordState::AttackState::LungeWindup:
        ss.stateTimer += dt;
        ss.animTimer = ss.stateTimer / lungeWindup;
        if (ss.stateTimer >= lungeWindup)
        {
            ss.state = SwordswordState::AttackState::LungeActive;
            ss.stateTimer = 0.0f;
        }
        break;
    case SwordswordState::AttackState::LungeActive:
        ss.stateTimer += dt;
        ss.animTimer = ss.stateTimer / lungeActive;
        if (ss.stateTimer >= lungeActive)
        {
            ss.state = SwordswordState::AttackState::LungeRecover;
            ss.stateTimer = 0.0f;
        }
        break;
    case SwordswordState::AttackState::LungeRecover:
        ss.stateTimer += dt;
        ss.animTimer = ss.stateTimer / lungeRecover;
        if (ss.stateTimer >= lungeRecover)
        {
            ss = SwordswordState{};
            player.meleeCooldownTimer = 0.0f;
        }
        break;
    case SwordswordState::AttackState::Idle:
    default:
        break;
    }
}

static bool buildPhysicalShape(ServerPlayer& attacker,
                               const WeaponDefinition& def,
                               uint16_t weaponDefNetworkId,
                               float dt,
                               WeaponExecution::PhysicalContactShape& outShape,
                               bool& outSwordLunge)
{
    (void)dt;
    outSwordLunge = false;
    const glm::vec3 forward = safeForward(attacker);

    if (def.behaviorType == WeaponBehaviorType::Godball)
    {
        const float radius = std::max(0.05f,
            WeaponExecution::paramOr(def, "ballRadius", std::max(0.1f, def.projectileRadius)));
        const float ropeLength = std::max(0.5f,
            WeaponExecution::paramOr(def, "ropeLength", 2.5f));
        const glm::vec3 center = attacker.pos + glm::vec3(0.0f, 0.0f, 0.9f) +
            forward * ropeLength;
        outShape.kind = WeaponExecution::PhysicalShapeKind::Sphere;
        outShape.currentA = center;
        outShape.currentB = center;
        outShape.radius = radius;
    }
    else if (def.behaviorType == WeaponBehaviorType::Swordsword ||
             def.behaviorType == WeaponBehaviorType::Melee ||
             def.behaviorType == WeaponBehaviorType::Hafs)
    {
        advanceSwordState(attacker, def, dt);
        if (!isSwordActive(attacker.swordswordState))
            return false;

        const float bladeLength = std::max(0.1f,
            WeaponExecution::paramOr(def, "bladeLength", 4.0f));
        const float bladeRadius = std::max(0.05f,
            WeaponExecution::paramOr(def, "bladeRadius", 0.35f));
        const glm::vec3 grip = attacker.pos + glm::vec3(0.0f, 0.0f, 0.8f) -
            forward * 0.5f;
        const glm::vec3 tip = grip + forward * bladeLength;
        outShape.kind = WeaponExecution::PhysicalShapeKind::Capsule;
        outShape.currentA = grip;
        outShape.currentB = tip;
        outShape.radius = bladeRadius;
        outSwordLunge = isSwordLunge(attacker.swordswordState);
    }
    else if (def.behaviorType == WeaponBehaviorType::QuickHit)
    {
        QuickHitState& qh = attacker.quickHitState;
        if (!qh.active || qh.activeTicksRemaining == 0)
            return false;

        qh.activeTicksRemaining--;

        const float capsuleRadius = std::max(0.05f,
            WeaponExecution::paramOr(def, "hitboxRadius", 0.22f));
        const float capsuleLength = std::max(0.1f,
            WeaponExecution::paramOr(def, "hitboxLength", 0.85f));

        // Approximate right arm position: shoulder height + forward extension
        const glm::vec3 shoulderOffset(0.0f, 0.0f, 1.2f);
        const glm::vec3 armCenter = attacker.pos + shoulderOffset + forward * 0.6f;
        const glm::vec3 armTip = armCenter + forward * capsuleLength;

        outShape.kind = WeaponExecution::PhysicalShapeKind::Capsule;
        outShape.currentA = armCenter;
        outShape.currentB = armTip;
        outShape.radius = capsuleRadius;
    }
    else
    {
        return false;
    }

    if (attacker.hasLastPhysicalWeaponShape &&
        attacker.lastPhysicalWeaponDefNetworkId == weaponDefNetworkId)
    {
        outShape.previousA = attacker.lastPhysicalWeaponShape.currentA;
        outShape.previousB = attacker.lastPhysicalWeaponShape.currentB;
    }
    else
    {
        outShape.previousA = outShape.currentA;
        outShape.previousB = outShape.currentB;
    }

    return true;
}

static void flushEpisode(SOCKET sock,
                         std::unordered_map<uint32_t, ServerPlayer>& players,
                         ServerPlayer& attacker,
                         const WeaponDefinition& def,
                         WeaponExecution::PhysicalContactEpisode& episode,
                         bool ending,
                         uint32_t tick,
                         uint64_t& totalPacketsOut)
{
    if (!WeaponExecution::episodeShouldConfirm(episode, ending, CONTACT_CONFIRM_BATCH))
        return;

    auto targetIt = players.find(episode.targetPlayerId);
    if (targetIt == players.end())
        return;

    ServerDamageResult confirmed;
    confirmed.applied = true;
    confirmed.killed = episode.pendingKilled;
    confirmed.healthBefore = episode.pendingHealthBefore;
    confirmed.healthAfter = episode.pendingHealthAfter;
    queueServerDamageConfirmedEvent(
        sock, players, tick, totalPacketsOut, attacker.id, targetIt->second,
        episode.pendingConfirmationDamage, confirmed,
        episode.lastHitPosition, episode.lastNormal, episode.accumulatedKnockback,
        ServerDamageSource::PhysicalContact,
        networkWeaponTypeForDefinition(def), episode.contactSerial);

    episode.pendingConfirmationDamage = 0;
    episode.pendingHealthBefore = 0;
    episode.pendingHealthAfter = 0;
    episode.pendingKilled = false;
    episode.samplesSinceConfirmation = 0;
    episode.accumulatedKnockback = glm::vec3(0.0f);
}

static void flushAndClearEpisodes(SOCKET sock,
                                  std::unordered_map<uint32_t, ServerPlayer>& players,
                                  ServerPlayer& attacker,
                                  const WeaponDefinition* def,
                                  uint32_t tick,
                                  uint64_t& totalPacketsOut)
{
    if (def)
    {
        for (auto& entry : attacker.physicalContactEpisodes)
            flushEpisode(sock, players, attacker, *def, entry.second, true, tick, totalPacketsOut);
    }
    attacker.physicalContactEpisodes.clear();
}

static int physicalContactDamage(const WeaponDefinition& def,
                                 const WeaponExecution::PhysicalContactShape& shape,
                                 bool swordLunge,
                                 float dt)
{
    if (def.behaviorType == WeaponBehaviorType::Godball)
    {
        const float base = WeaponExecution::paramOr(def, "baseDamagePerTick",
            std::max(1.0f, def.damage));
        const float speedFactor = WeaponExecution::paramOr(def, "speedDamageFactor", 0.5f);
        const float maxDamage = WeaponExecution::paramOr(def, "maxDamageCap", 200.0f);
        const float speed = WeaponExecution::physicalShapeTravelDistance(shape) /
            std::max(dt, 0.0001f);
        return std::clamp((int)std::round(base + speed * speedFactor),
            1, (int)std::max(1.0f, maxDamage));
    }

    if (def.behaviorType == WeaponBehaviorType::QuickHit)
    {
        // Force-based damage from capsule velocity
        const float travelDist = WeaponExecution::physicalShapeTravelDistance(shape);
        const float speed = travelDist / std::max(dt, 0.0001f);

        // Directness: how aligned capsule velocity is with the contact normal
        // Use shape direction as velocity proxy (previous->current)
        glm::vec3 shapeDir = shape.currentB - shape.currentA;
        float shapeLen = glm::length(shapeDir);
        float directness = 1.0f;
        if (shapeLen > 0.001f) {
            shapeDir /= shapeLen;
            // Use the hit normal if available, otherwise assume head-on
            directness = 0.8f;
        }

        float rawForce = speed * directness;
        float forceScale = WeaponExecution::paramOr(def, "forceDamageScale", 1.0f);
        float forceExp = WeaponExecution::paramOr(def, "forceDamageExponent", 1.35f);
        float minDmg = WeaponExecution::paramOr(def, "minDamage", 1.0f);
        float maxDmg = WeaponExecution::paramOr(def, "maxDamage", 100.0f);

        float damage = minDmg + std::pow(rawForce * forceScale, forceExp);
        return std::clamp((int)std::round(damage),
            (int)std::max(1.0f, minDmg), (int)std::max(1.0f, maxDmg));
    }

    const char* baseKey = swordLunge ? "lungeBaseDamage" : "slashBaseDamage";
    const char* compatKey = swordLunge ? "lungeDamage" : "slashDamage";
    const float fallback = swordLunge ? 18.0f : 10.0f;
    const float base = WeaponExecution::paramOr(def, baseKey,
        WeaponExecution::paramOr(def, compatKey, fallback));
    return std::clamp((int)std::round(base), 1, 500);
}

static glm::vec3 physicalContactKnockback(const WeaponDefinition& def,
                                          const WeaponExecution::PhysicalContactHit& hit,
                                          int damage,
                                          bool swordLunge)
{
    glm::vec3 normal = hit.normal;
    if (glm::length(normal) < 0.001f)
        normal = glm::vec3(0.0f, 0.0f, 1.0f);
    normal.z = std::max(normal.z, 0.15f);
    normal = glm::normalize(normal);

    if (def.behaviorType == WeaponBehaviorType::QuickHit)
    {
        float forceKbScale = WeaponExecution::paramOr(def, "forceKnockbackScale", 1.0f);
        float maxKb = WeaponExecution::paramOr(def, "maxKnockback", 100.0f);
        float minKb = WeaponExecution::paramOr(def, "minKnockback", 0.0f);
        float strength = std::clamp((float)damage * forceKbScale, minKb, maxKb);
        return normal * strength;
    }

    float strength = std::max(1.0f, damage * 0.75f);
    if (def.behaviorType != WeaponBehaviorType::Godball)
    {
        const char* key = swordLunge ? "lungeKnockback" : "slashKnockback";
        strength = WeaponExecution::paramOr(def, key, strength);
    }
    return normal * strength;
}

static void recordWeaponMovementContact(ServerPlayer& attacker,
                                        ServerPlayer& target,
                                        const WeaponExecution::PhysicalContactEpisode& episode,
                                        const WeaponExecution::PhysicalContactHit& hit,
                                        uint32_t tick,
                                        float strength)
{
    MovementLifecycleIdentity lifecycle{
        target.spawnGeneration,
        static_cast<uint32_t>(target.transformEpoch)};
    MovementContact contact = makeEntityMovementContact(
        MovementContactKind::Weapon,
        attacker.id == target.id ? MovementContactSource::OwnWeapon
                                 : MovementContactSource::EnemyWeapon,
        attacker.id,
        episode.contactSerial,
        0,
        tick,
        lifecycle,
        hit.hitPosition,
        hit.normal,
        strength,
        true);
    target.movement.contactHistory.recordStable(contact);
}

static void applyPhysicalContactHit(SOCKET sock,
                                    std::unordered_map<uint32_t, ServerPlayer>& players,
                                    ServerPlayer& attacker,
                                    ServerPlayer& target,
                                    const WeaponDefinition& def,
                                    const WeaponExecution::PhysicalContactShape& shape,
                                    const WeaponExecution::PhysicalContactHit& hit,
                                    bool swordLunge,
                                    uint32_t tick,
                                    float dt,
                                    uint64_t& totalPacketsOut)
{
    const uint16_t defNetId = weaponDefNetworkIdFor(def.id);
    auto& episode = attacker.physicalContactEpisodes[target.id];
    if (!episode.active ||
        episode.targetSpawnGeneration != target.spawnGeneration)
    {
        episode = WeaponExecution::PhysicalContactEpisode{};
        episode.active = true;
        episode.targetPlayerId = target.id;
        episode.targetSpawnGeneration = target.spawnGeneration;
        episode.contactSerial = attacker.nextPhysicalContactSerial++;
        if (attacker.nextPhysicalContactSerial == 0)
            attacker.nextPhysicalContactSerial = 1;
        episode.firstTick = tick;
    }

    const float damageInterval = def.behaviorType == WeaponBehaviorType::Godball
        ? WeaponExecution::paramOr(def, "damageTickInterval", 0.15f)
        : WeaponExecution::paramOr(def, "damageTickInterval", 0.05f);
    const uint32_t minDeltaTicks = intervalTicks(damageInterval);
    if (episode.lastSampleTick != 0 &&
        tick - episode.lastSampleTick < minDeltaTicks)
        return;

    episode.lastSampleTick = tick;
    const int damage = physicalContactDamage(def, shape, swordLunge, dt);
    const glm::vec3 knockback = physicalContactKnockback(def, hit, damage, swordLunge);
    ServerDamageResult result = applyServerDamage(
        players, target, attacker.id, damage, knockback,
        ServerDamageSource::PhysicalContact);
    if (!result.applied)
        return;

    recordWeaponMovementContact(attacker, target, episode, hit, tick, glm::length(knockback));

    episode.accumulatedDamage += damage;
    episode.pendingConfirmationDamage += damage;
    if (episode.samplesSinceConfirmation == 0)
        episode.pendingHealthBefore = result.healthBefore;
    episode.pendingHealthAfter = result.healthAfter;
    episode.pendingKilled = episode.pendingKilled || result.killed;
    episode.samplesSinceConfirmation++;
    episode.accumulatedKnockback += knockback;
    episode.lastHitPosition = hit.hitPosition;
    episode.lastNormal = hit.normal;

    Debug::log(Debug::Category::Weapons,
        "[PHYSICAL CONTACT DAMAGE] attacker=%u target=%u weapon=%s defNetId=%u "
        "serial=%u damage=%d pending=%d samples=%u killed=%d\n",
        attacker.id, target.id, def.id.c_str(), defNetId, episode.contactSerial,
        damage, episode.pendingConfirmationDamage,
        (unsigned)episode.samplesSinceConfirmation, (int)result.killed);

    flushEpisode(sock, players, attacker, def, episode, result.killed, tick, totalPacketsOut);
}

} // namespace

void tickServerPhysicalContactWeapons(SOCKET sock,
                                      std::unordered_map<uint32_t, ServerPlayer>& players,
                                      const HeadlessWorld& world,
                                      float dt, uint32_t tick,
                                      uint64_t& totalPacketsOut)
{
    (void)world;

    for (auto& attackerEntry : players)
    {
        ServerPlayer& attacker = attackerEntry.second;
        const WeaponDefinition* def = equippedWeaponDefinition(attacker);
        if (!def || def->executionType != WeaponExecutionType::PhysicalContact ||
            attacker.dead || attacker.spawnState != ServerPlayer::Active)
        {
            const WeaponDefinition* flushDef =
                def && def->executionType == WeaponExecutionType::PhysicalContact ? def : nullptr;
            flushAndClearEpisodes(sock, players, attacker, flushDef, tick, totalPacketsOut);
            clearPhysicalRuntime(attacker);
            continue;
        }

        const uint16_t defNetId = weaponDefNetworkIdFor(def->id);
        WeaponExecution::PhysicalContactShape shape;
        bool swordLunge = false;
        if (!buildPhysicalShape(attacker, *def, defNetId, dt, shape, swordLunge))
        {
            flushAndClearEpisodes(sock, players, attacker, def, tick, totalPacketsOut);
            attacker.hasLastPhysicalWeaponShape = false;
            attacker.lastPhysicalWeaponDefNetworkId = defNetId;
            continue;
        }

        std::unordered_set<uint32_t> touchingTargets;
        for (auto& targetEntry : players)
        {
            ServerPlayer& target = targetEntry.second;
            if (target.id == attacker.id || target.dead ||
                target.spawnState != ServerPlayer::Active)
                continue;

            WeaponExecution::PlayerTarget targetDesc;
            targetDesc.playerId = target.id;
            targetDesc.spawnGeneration = target.spawnGeneration;
            targetDesc.position = target.pos;
            targetDesc.radius = PLAYER_RADIUS;
            targetDesc.height = PLAYER_HEIGHT;
            targetDesc.dead = target.dead;

            WeaponExecution::PhysicalContactHit hit;
            if (!WeaponExecution::testPhysicalContact(shape, targetDesc, hit))
                continue;

            touchingTargets.insert(target.id);
            applyPhysicalContactHit(sock, players, attacker, target, *def, shape,
                                    hit, swordLunge, tick, dt, totalPacketsOut);
        }

        for (auto it = attacker.physicalContactEpisodes.begin();
             it != attacker.physicalContactEpisodes.end(); )
        {
            if (touchingTargets.count(it->first))
            {
                ++it;
                continue;
            }
            flushEpisode(sock, players, attacker, *def, it->second, true,
                         tick, totalPacketsOut);
            it = attacker.physicalContactEpisodes.erase(it);
        }

        attacker.lastPhysicalWeaponShape = shape;
        attacker.hasLastPhysicalWeaponShape = true;
        attacker.lastPhysicalWeaponDefNetworkId = defNetId;
    }
}

} // namespace MimitaNet
