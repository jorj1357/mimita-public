#include "network/multiplayer-context.h"
#include "network/network-weapons.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "audio/audio.h"
#include "camera.h"
#include "combat/projectile-render.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-runtime.h"
#include "combat/weapon-types.h"
#include "config/weapon-hitfx-config.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"
#include "physics/physics-types.h"
#include "world/world.h"

namespace MimitaNet {
namespace {

bool finiteVec(const glm::vec3& v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

float projectileGravity(uint8_t weapon)
{
    return weapon == NETWORK_WEAPON_GRENADE_LAUNCHER ? 20.0f : 0.0f;
}

float projectileDrag(uint8_t weapon)
{
    return weapon == NETWORK_WEAPON_GRENADE_LAUNCHER ? 0.15f : 0.0f;
}

const WeaponDefinition* projectileDefinition(uint8_t weapon)
{
    const char* id = weapon == NETWORK_WEAPON_GRENADE_LAUNCHER
        ? "grenade_launcher"
        : "rocket_launcher";
    return WeaponRegistry::instance().get(id);
}

float cp(const WeaponDefinition* def, const char* key, float fallback)
{
    if (!def)
        return fallback;
    auto it = def->customParams.find(key);
    return it != def->customParams.end() ? it->second : fallback;
}

ProjectileVisualConfig projectileVisualConfig(uint8_t weapon)
{
    const bool isRocket = weapon == NETWORK_WEAPON_ROCKET_LAUNCHER;
    const WeaponDefinition* def = projectileDefinition(weapon);
    ProjectileVisualConfig cfg;
    cfg.texturePath = isRocket ? "assets/textureshq/colorful2.png" : "assets/textureshq/meat1.png";
    cfg.length = cp(def, "projectileVisualLength", isRocket ? 1.5f : 1.8f);
    cfg.radius = cp(def, "projectileVisualRadius", isRocket ? 0.18f : 0.28f);
    cfg.scale = glm::vec3(
        cp(def, "projectileVisualScaleX", 1.0f),
        cp(def, "projectileVisualScaleY", 1.0f),
        cp(def, "projectileVisualScaleZ", 1.0f));
    cfg.rotationOffsetDegrees = glm::vec3(
        cp(def, "projectileVisualRotationOffsetX", 0.0f),
        cp(def, "projectileVisualRotationOffsetY", 0.0f),
        cp(def, "projectileVisualRotationOffsetZ", 0.0f));
    cfg.textureTiling = glm::vec2(
        cp(def, "projectileVisualTextureTilingU", 1.0f),
        cp(def, "projectileVisualTextureTilingV", 1.0f));
    cfg.fillAlpha = cp(def, "projectileFillAlpha", 1.0f);
    cfg.outlineEnabled = cp(def, "projectileOutlineEnabled", 1.0f) > 0.0f;
    cfg.outlineColor = glm::vec3(
        cp(def, "projectileOutlineColorR", 1.0f),
        cp(def, "projectileOutlineColorG", 0.8f),
        cp(def, "projectileOutlineColorB", 0.2f));
    cfg.outlineAlpha = cp(def, "projectileOutlineAlpha", 0.4f);
    cfg.outlineScale = cp(def, "projectileOutlineScale", 1.15f);
    cfg.glowEnabled = cp(def, "projectileGlowEnabled", 1.0f) > 0.0f;
    cfg.glowColor = glm::vec3(
        cp(def, "projectileGlowColorR", 1.0f),
        cp(def, "projectileGlowColorG", 0.6f),
        cp(def, "projectileGlowColorB", 0.0f));
    cfg.glowAlpha = cp(def, "projectileGlowAlpha", 0.15f);
    cfg.glowRadiusMultiplier = cp(def, "projectileGlowRadiusMultiplier", 3.0f);
    return cfg;
}

void spawnProjectileTrail(NetworkProjectile& projectile, float dt)
{
    const bool rocket = projectile.weaponType == NETWORK_WEAPON_ROCKET_LAUNCHER;
    const float rate = rocket ? 30.0f : 18.0f;
    projectile.smokeAccumulator += rate * dt;
    while (projectile.smokeAccumulator >= 1.0f)
    {
        projectile.smokeAccumulator -= 1.0f;
        EffectPart part;
        // Trails use render position for smoothness
        part.position = projectile.renderPosition;
        part.velocity = rocket
            ? projectile.renderVelocity * -0.08f
            : projectile.renderVelocity * 0.10f;
        part.lifetime = 0.0f;
        part.maxLifetime = rocket ? 0.8f : 0.25f;
        part.scale = rocket ? 0.18f : 0.04f;
        part.endScale = rocket ? 0.6f : 0.01f;
        part.color = rocket
            ? glm::vec3(0.55f, 0.55f, 0.55f)
            : glm::vec3(1.0f, 0.65f, 0.15f);
        part.alpha = rocket ? 0.55f : 0.9f;
        part.gravity = rocket ? 0.0f : 8.0f;
        part.affectedByGravity = !rocket;
        part.replayType = rocket ? "net_rocket_trail" : "net_grenade_spark";
        EffectPartSystem::instance().spawn(part);
    }
}

} // namespace

uint32_t mpSendProjectileFireRequest(
    MultiplayerContext& ctx,
    uint8_t weapon,
    const glm::vec3& origin,
    const glm::vec3& direction)
{
    if (!ctx.active || !ctx.localPlayerId ||
        !networkWeaponTypeIsProjectile(weapon) ||
        !finiteVec(origin) || !finiteVec(direction) ||
        glm::length(direction) <= 0.001f)
        return 0;

    ProjectileFireRequestPacket packet{};
    packet.header.type = PACKET_PROJECTILE_FIRE_REQUEST;
    packet.header.tick = ctx.tick;
    packet.header.playerId = ctx.localPlayerId;
    packet.fireSerial = ctx.nextLocalProjectileFireSerial++;
    if (ctx.nextLocalProjectileFireSerial == 0)
        ctx.nextLocalProjectileFireSerial = 1;
    packet.lastServerTick = ctx.latestServerTick;
    packet.weapon = weapon;
    const glm::vec3 dir = glm::normalize(direction);
    packet.originX = origin.x;
    packet.originY = origin.y;
    packet.originZ = origin.z;
    packet.dirX = dir.x;
    packet.dirY = dir.y;
    packet.dirZ = dir.z;
    mpSendPacket(ctx, &packet, sizeof(packet));

    // Track pending fire request for retransmission
    MultiplayerContext::PendingFireRequest pfr;
    pfr.fireSerial = packet.fireSerial;
    pfr.weapon = weapon;
    pfr.origin = origin;
    pfr.direction = dir;
    pfr.firstSentMs = nowMs();
    pfr.lastSentMs = nowMs();
    pfr.attempts = 1;
    ctx.pendingFireRequests[packet.fireSerial] = pfr;

    printf("[PROJECTILE FIRE REQUEST SEND] localPlayerId=%u localFireSerial=%u "
           "weapon=%s origin=(%.2f,%.2f,%.2f) direction=(%.3f,%.3f,%.3f) "
           "velocity=(server-derived) clientTick=%u\n",
           ctx.localPlayerId, packet.fireSerial,
           networkWeaponTypeName(weapon),
           origin.x, origin.y, origin.z,
           dir.x, dir.y, dir.z, ctx.tick);
    return packet.fireSerial;
}

uint32_t mpSendMeleeHitRequest(
    MultiplayerContext& ctx,
    uint32_t targetPlayerId,
    int damage,
    uint8_t weapon,
    uint8_t attackType,
    const glm::vec3& hit,
    const glm::vec3& normal,
    const glm::vec3& knockback,
    float weaponSpeed)
{
    if (!ctx.active || !ctx.localPlayerId || targetPlayerId == 0 ||
        weapon != NETWORK_WEAPON_SWORDSWORD)
        return 0;

    MeleeHitRequestPacket packet{};
    packet.header.type = PACKET_MELEE_HIT_REQUEST;
    packet.header.tick = ctx.tick;
    packet.header.playerId = ctx.localPlayerId;
    packet.attackSerial = ctx.nextLocalMeleeAttackSerial++;
    if (ctx.nextLocalMeleeAttackSerial == 0)
        ctx.nextLocalMeleeAttackSerial = 1;
    packet.lastServerTick = ctx.latestServerTick;
    packet.targetPlayerId = targetPlayerId;
    packet.damage = std::clamp(damage, 1, 200);
    packet.weapon = weapon;
    packet.attackType = attackType;
    packet.hitX = hit.x;
    packet.hitY = hit.y;
    packet.hitZ = hit.z;
    packet.normalX = normal.x;
    packet.normalY = normal.y;
    packet.normalZ = normal.z;
    packet.knockX = knockback.x;
    packet.knockY = knockback.y;
    packet.knockZ = knockback.z;
    packet.weaponSpeed = weaponSpeed;
    mpSendPacket(ctx, &packet, sizeof(packet));
    printf("[SWORD HIT REQUEST SEND] attackerId=%u targetId=%u attackSerial=%u "
           "damage=%d hit=(%.2f,%.2f,%.2f) knockback=(%.2f,%.2f,%.2f)\n",
           ctx.localPlayerId, targetPlayerId, packet.attackSerial,
           packet.damage, hit.x, hit.y, hit.z,
           knockback.x, knockback.y, knockback.z);
    return packet.attackSerial;
}

void mpProcessProjectileSpawnEventPacket(MultiplayerContext& ctx, const ProjectileSpawnEventPacket* event)
{
    // Mark pending fire request as acknowledged
    if (event->fireSerial != 0)
        ctx.pendingFireRequests.erase(event->fireSerial);

    // Rocket launcher: local simulation handles it — suppress server interpolation
    if (event->ownerPlayerId == ctx.localPlayerId &&
        event->weapon == NETWORK_WEAPON_ROCKET_LAUNCHER)
    {
        ctx.predictedProjectileIds.insert(event->projectileId);
        printf("[PROJECTILE PREDICTED] projectileId=%u fireSerial=%u weapon=%s "
               "pos=(%.2f,%.2f,%.2f) — suppressed server interpolation\n",
               event->projectileId, event->fireSerial,
               networkWeaponTypeName(event->weapon),
               event->posX, event->posY, event->posZ);
        return;
    }

    NetworkProjectile& projectile = ctx.networkProjectiles[event->projectileId];

    // Grenade launcher: local projectile uses prediction physics
    if (event->ownerPlayerId == ctx.localPlayerId)
        projectile.predicted = true;
    projectile.projectileId = event->projectileId;
    projectile.ownerPlayerId = event->ownerPlayerId;
    projectile.fireSerial = event->fireSerial;
    projectile.weaponType = event->weapon;
    projectile.position = {event->posX, event->posY, event->posZ};
    projectile.previousPosition = projectile.position;
    projectile.velocity = {event->velX, event->velY, event->velZ};
    projectile.rotation = glm::quat(event->rotW, event->rotX, event->rotY, event->rotZ);
    projectile.angularVelocity = {event->angVelX, event->angVelY, event->angVelZ};
    projectile.age = 0.0f;
    projectile.lifetime = event->lifetime;
    projectile.radius = event->radius;
    projectile.predicted = false;
    projectile.exploded = false;

    // Initialize render state directly from spawn (first state = immediate render)
    projectile.renderPosition = projectile.position;
    projectile.renderVelocity = projectile.velocity;
    projectile.renderRotation = projectile.rotation;
    projectile.renderAngularVelocity = projectile.angularVelocity;

    // Initialize interpolation state
    projectile.prevStateTick = event->spawnTick;
    projectile.prevStatePos = projectile.position;
    projectile.prevStateVel = projectile.velocity;
    projectile.prevStateRot = projectile.rotation;

    projectile.targetStateTick = event->spawnTick;
    projectile.targetStatePos = projectile.position;
    projectile.targetStateVel = projectile.velocity;
    projectile.targetStateRot = projectile.rotation;

    projectile.latestAcceptedTick = event->spawnTick;
    projectile.lastTargetReceivedMs = nowMs();
    projectile.hasTargetState = true;

    printf("[PROJECTILE CLIENT SPAWN] projectileId=%u ownerPlayerId=%u "
           "weapon=%s position=(%.2f,%.2f,%.2f) velocity=(%.2f,%.2f,%.2f) "
           "spawnTick=%u\n",
           projectile.projectileId, projectile.ownerPlayerId,
           networkWeaponTypeName(projectile.weaponType),
           projectile.position.x, projectile.position.y, projectile.position.z,
           projectile.velocity.x, projectile.velocity.y, projectile.velocity.z,
           event->spawnTick);
}

void mpProcessProjectileStateEventPacket(MultiplayerContext& ctx, const ProjectileStateEventPacket* event)
{
    // Skip state updates for locally-predicted projectiles
    if (ctx.predictedProjectileIds.count(event->projectileId))
        return;

    auto it = ctx.networkProjectiles.find(event->projectileId);
    if (it == ctx.networkProjectiles.end())
    {
        // Lost spawn event — recover from state update
        NetworkProjectile& projectile = ctx.networkProjectiles[event->projectileId];
        projectile.projectileId = event->projectileId;
        projectile.ownerPlayerId = 0;
        projectile.weaponType = event->weapon;
        projectile.position = {event->posX, event->posY, event->posZ};
        projectile.previousPosition = projectile.position;
        projectile.velocity = {event->velX, event->velY, event->velZ};
        projectile.rotation = glm::quat(event->rotW, event->rotX, event->rotY, event->rotZ);
        projectile.angularVelocity = {event->angVelX, event->angVelY, event->angVelZ};
        projectile.age = event->age;
        projectile.lifetime = 0.0f;
        projectile.radius = 0.0f;
        projectile.predicted = false;
        projectile.exploded = false;
        // Initialize render state for recovery
        projectile.renderPosition = projectile.position;
        projectile.renderVelocity = projectile.velocity;
        projectile.renderRotation = projectile.rotation;
        projectile.prevStateTick = event->header.tick;
        projectile.prevStatePos = projectile.position;
        projectile.prevStateVel = projectile.velocity;
        projectile.prevStateRot = projectile.rotation;
        projectile.targetStateTick = event->header.tick;
        projectile.targetStatePos = projectile.position;
        projectile.targetStateVel = projectile.velocity;
        projectile.targetStateRot = projectile.rotation;
        projectile.latestAcceptedTick = event->header.tick;
        projectile.lastTargetReceivedMs = nowMs();
        projectile.hasTargetState = true;
        printf("[PROJECTILE STATE RECOVER] projectileId=%u weapon=%s "
               "position=(%.2f,%.2f,%.2f) age=%.2f\n",
               event->projectileId, networkWeaponTypeName(event->weapon),
               event->posX, event->posY, event->posZ, event->age);
        return;
    }
    NetworkProjectile& projectile = it->second;

    // ── Stale state rejection ──────────────────────────────────────
    // Reject states older than our latest accepted state.
    const uint32_t newTick = event->header.tick;
    if (newTick <= projectile.latestAcceptedTick)
    {
        printf("[PROJECTILE STATE RX] projectileId=%u serverTick=%u "
               "latestAcceptedTick=%u accepted=0 reason=stale-or-duplicate\n",
               event->projectileId, newTick, projectile.latestAcceptedTick);
        return;
    }

    // ── Move current target to previous, set new target ────────────
    projectile.prevStateTick = projectile.targetStateTick;
    projectile.prevStatePos = projectile.targetStatePos;
    projectile.prevStateVel = projectile.targetStateVel;
    projectile.prevStateRot = projectile.targetStateRot;

    projectile.targetStateTick = newTick;
    projectile.targetStatePos = {event->posX, event->posY, event->posZ};
    projectile.targetStateVel = {event->velX, event->velY, event->velZ};
    projectile.targetStateRot = glm::quat(event->rotW, event->rotX, event->rotY, event->rotZ);

    // Also update raw authoritative state
    projectile.position = projectile.targetStatePos;
    projectile.velocity = projectile.targetStateVel;
    projectile.rotation = projectile.targetStateRot;
    projectile.angularVelocity = {event->angVelX, event->angVelY, event->angVelZ};
    projectile.age = event->age;

    projectile.latestAcceptedTick = newTick;
    projectile.lastTargetReceivedMs = nowMs();
    projectile.hasTargetState = true;

    printf("[PROJECTILE STATE RX] projectileId=%u serverTick=%u "
           "latestAcceptedTick=%u accepted=1 pos=(%.2f,%.2f,%.2f) vel=(%.2f,%.2f,%.2f) age=%.2f\n",
           event->projectileId, newTick, projectile.latestAcceptedTick,
           event->posX, event->posY, event->posZ,
           event->velX, event->velY, event->velZ,
           event->age);
}

void mpProcessProjectileExplodeEventPacket(MultiplayerContext& ctx, const ProjectileExplodeEventPacket* event)
{
    // Predicted projectile: reconcile health but skip visuals/knockback (already applied locally)
    if (ctx.predictedProjectileIds.erase(event->projectileId))
    {
        for (uint8_t i = 0; i < event->victimCount && i < MAX_PROJECTILE_DAMAGE_RESULTS; ++i)
        {
            const ProjectileDamageResultPacket& victim = event->victims[i];
            if (victim.victimPlayerId == ctx.localPlayerId)
            {
                ctx.localServerHealth = victim.healthAfter;
                printf("[PROJECTILE PREDICTED EXPLODE] projectileId=%u victim=local "
                       "healthAfter=%d (reconciled, no knockback)\n",
                       event->projectileId, victim.healthAfter);
            }
        }
        printf("[PROJECTILE PREDICTED EXPLODE] projectileId=%u weapon=%s — suppressed dupe effects\n",
               event->projectileId, networkWeaponTypeName(event->weapon));
        return;
    }

    const glm::vec3 position(event->posX, event->posY, event->posZ);
    const char* weaponName = networkWeaponTypeName(event->weapon);
    bool removedVisual = ctx.networkProjectiles.erase(event->projectileId) > 0;

    playWorldSound(
        event->weapon == NETWORK_WEAPON_GRENADE_LAUNCHER
            ? "grenadelauncher/grenadelauncherexplode"
            : "rocketlauncher/rocketlauncherexplode",
        position, 1.0f, 1.0f, 50.0f);
    EffectPartSystem::instance().spawnMuzzleFlash(position, std::string(weaponName) + "_explosion");
    EffectPartSystem::instance().spawnWorldDebris(position, glm::vec3(0.0f, 0.0f, 1.0f), 3.0f);
    // Explosion smoke burst — config-driven burst for rockets and grenades
    {
        const std::string weaponId = weaponName;
        const auto& expCfg = WeaponHitFxConfig::instance().explosionBurstFor(weaponId);
        if (expCfg.smoke.enabled)
        {
            for (int i = 0; i < expCfg.smoke.count; ++i)
            {
                EffectPart part;
                part.position = position + glm::vec3(
                    ((float)rand() / RAND_MAX - 0.5f) * expCfg.smoke.spread,
                    ((float)rand() / RAND_MAX - 0.5f) * expCfg.smoke.spread,
                    ((float)rand() / RAND_MAX - 0.5f) * expCfg.smoke.spread);
                part.velocity = glm::vec3(
                    ((float)rand() / RAND_MAX - 0.5f) * expCfg.smoke.speed,
                    ((float)rand() / RAND_MAX - 0.5f) * expCfg.smoke.speed,
                    (float)rand() / RAND_MAX * expCfg.smoke.speed * 0.5f + expCfg.smoke.upwardBias);
                part.lifetime = 0.0f;
                part.maxLifetime = expCfg.smoke.lifetime + (float)rand() / RAND_MAX * expCfg.smoke.lifetime * 0.3f;
                part.scale = expCfg.smoke.size + (float)rand() / RAND_MAX * expCfg.smoke.size * 0.5f;
                part.endScale = expCfg.smoke.endSize + (float)rand() / RAND_MAX * expCfg.smoke.endSize * 0.5f;
                part.color = expCfg.smoke.color;
                part.alpha = expCfg.smoke.alpha;
                part.gravity = 1.0f;
                part.affectedByGravity = true;
                part.billboardText = false;
                part.replayType = std::string(weaponName) + "_explosion_smoke";
                EffectPartSystem::instance().spawn(part);
            }
        }
    }

    // ── Config-driven explosion sphere ─────────────────────────────────
    {
        const std::string weaponId = weaponName;
        const auto& expCfg = WeaponHitFxConfig::instance().explosionBurstFor(weaponId);
        if (expCfg.sphere.enabled)
        {
            EffectPart sphere;
            sphere.position = position;
            sphere.maxLifetime = (float)expCfg.sphere.lifetimeTicks / 60.0f;
            sphere.scale = expCfg.sphere.startRadius;
            sphere.endScale = expCfg.sphere.endRadius;
            sphere.color = expCfg.sphere.startColor * expCfg.sphere.brightnessStart;
            sphere.alpha = expCfg.sphere.alphaStart;
            sphere.billboardText = false;
            sphere.replayType = std::string(weaponName) + "_explosion_sphere";
            EffectPartSystem::instance().spawn(sphere);
        }
    }

    HitEvent hit;
    hit.position = position;
    hit.normal = glm::vec3(0.0f, 0.0f, 1.0f);
    hit.hitWorld = true;
    hit.damage = 0;
    hit.attacker = "player_" + std::to_string(event->ownerPlayerId);
    hit.weaponSource = weaponName;
    HitEffects::onHit(hit);

    for (uint8_t i = 0; i < event->victimCount && i < MAX_PROJECTILE_DAMAGE_RESULTS; ++i)
    {
        const ProjectileDamageResultPacket& victim = event->victims[i];
        if (victim.victimPlayerId == ctx.localPlayerId)
        {
            ctx.localServerHealth = victim.healthAfter;
            ctx.pendingKnockback += glm::vec3(
                victim.knockX, victim.knockY, victim.knockZ);
            ctx.pendingKnockbackSource = weaponName;
            printf("[NET KNOCKBACK APPLY] projectileId=%u victim=local "
                   "impulse=(%.2f,%.2f,%.2f) source=%s\n",
                   event->projectileId,
                   victim.knockX, victim.knockY, victim.knockZ,
                   weaponName);
        }
        else
        {
            auto remote = ctx.remotePlayers.find(victim.victimPlayerId);
            if (remote != ctx.remotePlayers.end())
            {
                remote->second.currentHp = victim.healthAfter;
                remote->second.dead = victim.healthAfter <= 0;
                remote->second.externalImpulse += glm::vec3(
                    victim.knockX, victim.knockY, victim.knockZ);
            }
            auto interpolation = ctx.remotePlayerInterpolation.find(victim.victimPlayerId);
            if (interpolation != ctx.remotePlayerInterpolation.end())
                interpolation->second.target.health = victim.healthAfter;
        }
        printf("[NET PROJECTILE DAMAGE RECV] projectileId=%u victimPlayerId=%u "
               "damage=%d healthAfter=%d killed=%d\n",
               event->projectileId, victim.victimPlayerId,
               victim.damage, victim.healthAfter, (int)victim.killed);
    }

    printf("[PROJECTILE CLIENT EXPLODE] projectileId=%u weapon=%s "
           "position=(%.2f,%.2f,%.2f) serverTick=%u removedVisual=%d\n",
           event->projectileId, weaponName,
           position.x, position.y, position.z, event->header.tick,
           (int)removedVisual);
}

void mpProcessProjectileFireResultPacket(MultiplayerContext& ctx, const ProjectileFireResultPacket* event)
{
    auto it = ctx.pendingFireRequests.find(event->fireSerial);
    if (event->accepted)
    {
        if (it != ctx.pendingFireRequests.end())
        {
            it->second.acknowledged = true;
            printf("[PROJECTILE FIRE RESULT] fireSerial=%u projectileId=%u accepted=1\n",
                   event->fireSerial, event->projectileId);
        }
    }
    else
    {
        printf("[PROJECTILE FIRE RESULT] fireSerial=%u accepted=0 reason=%d\n",
               event->fireSerial, (int)event->reason);
    }
}

void mpProcessProjectileDespawnEventPacket(MultiplayerContext& ctx, const ProjectileDespawnEventPacket* event)
{
    ctx.networkProjectiles.erase(event->projectileId);
}

void mpProcessMeleeHitEventPacket(MultiplayerContext& ctx, const MeleeHitEventPacket* event)
{
    // Animation-only event (targetPlayerId=0): create sword state but skip VFX
    if (event->targetPlayerId != 0)
    {
        NetworkShotEvent out;
        out.shotSerial = event->attackSerial;
        out.shooterPlayerId = event->attackerPlayerId;
        out.targetPlayerId = event->targetPlayerId;
        out.damage = event->damage;
        out.targetHealth = event->targetHealth;
        out.effectFlags =
            SHOT_EFFECT_ENTITY_IMPACT |
            SHOT_EFFECT_BLOOD |
            SHOT_EFFECT_HIT_SOUND |
            SHOT_EFFECT_WEAPON_TRIGGER;
        out.weapon = event->weapon;
        out.impactType = SHOT_IMPACT_ENTITY;
        out.killed = event->killed != 0;
        out.damageConfirmed = event->damageConfirmed != 0;
        out.hit = {event->hitX, event->hitY, event->hitZ};
        out.normal = {event->normalX, event->normalY, event->normalZ};
        out.knockback = {event->knockX, event->knockY, event->knockZ};
        out.direction = glm::length(out.knockback) > 0.001f
            ? glm::normalize(out.knockback)
            : -out.normal;
        out.origin = out.hit - out.direction;
        ctx.shotEvents.push_back(out);
    }

    // Initialize remote sword state for animation reconstruction
    if (event->attackerPlayerId == 0 || event->attackerPlayerId == ctx.localPlayerId)
        return;
    auto playerIt = ctx.remotePlayers.find(event->attackerPlayerId);
    if (playerIt == ctx.remotePlayers.end())
        return;

    Player& attacker = playerIt->second;
    auto rtIt = attacker.weaponRuntimes.find("swordsword");
    if (rtIt == attacker.weaponRuntimes.end())
    {
        // Ensure runtime exists
        const WeaponDefinition* def = WeaponRegistry::instance().get("swordsword");
        if (!def) return;
        WeaponRuntime& rt = attacker.weaponRuntimes["swordsword"];
        rt = WeaponRuntime{};
        WeaponRuntimeHelper::initRuntime(rt, *def);
    }

    // Set pose state for immediate visual feedback
    WeaponRuntime& rt = attacker.weaponRuntimes["swordsword"];
    rt.shootEffectTimer = 0.18f;
    if (event->attackType == 1)
        rt.customFloats["swordPoseState"] = 1.0f;
    else if (event->attackType == 2)
        rt.customFloats["swordPoseState"] = 2.0f;

    // Create/replace sword state for lifecycle tracking
    SwordswordState& ss = ctx.remoteSwordStates[event->attackerPlayerId];
    ss = SwordswordState{};
    if (event->attackType == 1)
        ss.state = SwordswordState::AttackState::SlashWindup;
    else
        ss.state = SwordswordState::AttackState::LungeWindup;
    ss.stateTimer = 0.0f;
    ss.animTimer = 0.0f;
}

void mpUpdateRemoteSwordStates(MultiplayerContext& ctx, float dt)
{
    const WeaponDefinition* def = WeaponRegistry::instance().get("swordsword");
    if (!def) return;

    float slashWindup  = 0.08f; float slashActive  = 0.15f; float slashRecover = 0.10f;
    float lungeWindup  = 0.10f; float lungeActive  = 0.20f; float lungeRecover = 0.12f;

    auto it = ctx.remoteSwordStates.begin();
    while (it != ctx.remoteSwordStates.end())
    {
        uint32_t attackerId = it->first;
        SwordswordState& ss = it->second;
        Player* attacker = nullptr;
        {
            auto pi = ctx.remotePlayers.find(attackerId);
            if (pi != ctx.remotePlayers.end())
                attacker = &pi->second;
        }

        if (ss.state == SwordswordState::AttackState::Idle)
        {
            it = ctx.remoteSwordStates.erase(it);
            continue;
        }

        // Advance state
        if (ss.state == SwordswordState::AttackState::SlashWindup) {
            ss.stateTimer += dt;
            ss.animTimer = ss.stateTimer / slashWindup;
            if (ss.stateTimer >= slashWindup) {
                ss.state = SwordswordState::AttackState::SlashActive;
                ss.stateTimer = 0.0f;
            }
        } else if (ss.state == SwordswordState::AttackState::SlashActive) {
            ss.stateTimer += dt;
            ss.animTimer = ss.stateTimer / slashActive;
            if (ss.stateTimer >= slashActive) {
                ss.state = SwordswordState::AttackState::SlashRecover;
                ss.stateTimer = 0.0f;
            }
        } else if (ss.state == SwordswordState::AttackState::SlashRecover) {
            ss.stateTimer += dt;
            ss.animTimer = ss.stateTimer / slashRecover;
            if (ss.stateTimer >= slashRecover) {
                ss.state = SwordswordState::AttackState::Idle;
                if (attacker) {
                    auto rtIt = attacker->weaponRuntimes.find("swordsword");
                    if (rtIt != attacker->weaponRuntimes.end())
                        rtIt->second.customFloats["swordPoseState"] = 0.0f;
                }
                it = ctx.remoteSwordStates.erase(it);
                continue;
            }
        } else if (ss.state == SwordswordState::AttackState::LungeWindup) {
            ss.stateTimer += dt;
            ss.animTimer = ss.stateTimer / lungeWindup;
            if (ss.stateTimer >= lungeWindup) {
                ss.state = SwordswordState::AttackState::LungeActive;
                ss.stateTimer = 0.0f;
            }
        } else if (ss.state == SwordswordState::AttackState::LungeActive) {
            ss.stateTimer += dt;
            ss.animTimer = ss.stateTimer / lungeActive;
            if (ss.stateTimer >= lungeActive) {
                ss.state = SwordswordState::AttackState::LungeRecover;
                ss.stateTimer = 0.0f;
            }
        } else if (ss.state == SwordswordState::AttackState::LungeRecover) {
            ss.stateTimer += dt;
            ss.animTimer = ss.stateTimer / lungeRecover;
            if (ss.stateTimer >= lungeRecover) {
                ss.state = SwordswordState::AttackState::Idle;
                if (attacker) {
                    auto rtIt = attacker->weaponRuntimes.find("swordsword");
                    if (rtIt != attacker->weaponRuntimes.end())
                        rtIt->second.customFloats["swordPoseState"] = 0.0f;
                }
                it = ctx.remoteSwordStates.erase(it);
                continue;
            }
        }

        // Update swordPoseState on remote player's runtime
        if (attacker)
        {
            auto rtIt = attacker->weaponRuntimes.find("swordsword");
            if (rtIt != attacker->weaponRuntimes.end())
            {
                rtIt->second.shootEffectTimer = 0.18f;
                if (ss.state == SwordswordState::AttackState::SlashWindup ||
                    ss.state == SwordswordState::AttackState::LungeWindup)
                    rtIt->second.customFloats["swordPoseState"] = 0.0f;
                else if (ss.state == SwordswordState::AttackState::SlashActive)
                    rtIt->second.customFloats["swordPoseState"] = 1.0f;
                else if (ss.state == SwordswordState::AttackState::LungeActive)
                    rtIt->second.customFloats["swordPoseState"] = 2.0f;
                else if (ss.state == SwordswordState::AttackState::SlashRecover ||
                         ss.state == SwordswordState::AttackState::LungeRecover)
                    rtIt->second.customFloats["swordPoseState"] = 0.0f;
            }
        }

        ++it;
    }
}

static glm::vec3 closestPtOnTri(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
{
    glm::vec3 ab = b - a, ac = c - a, ap = p - a;
    float d1 = glm::dot(ab, ap), d2 = glm::dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) return a;
    glm::vec3 bp = p - b;
    float d3 = glm::dot(ab, bp), d4 = glm::dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) return b;
    glm::vec3 cp = p - c;
    float d5 = glm::dot(ab, cp), d6 = glm::dot(ac, cp);
    if (d5 >= 0.0f && d6 >= 0.0f) return c;
    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
        return a + (d1 / (d1 - d3)) * ab;
    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
        return a + (d2 / (d2 - d6)) * ac;
    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
        return b + ((d4 - d3) / ((d4 - d3) + (d5 - d6))) * (c - b);
    float denom = va + vb + vc;
    return denom == 0.0f ? a : (va * a + vb * b + vc * c) / denom;
}

static bool resolveClientGrenadeWorld(NetworkProjectile& projectile, const World& world, const WeaponDefinition* def)
{
    float restitution = 0.35f, friction = 0.5f;
    if (def)
    {
        restitution = cp(def, "bounceRestitution", 0.35f);
        friction = cp(def, "bounceFriction", 0.5f);
    }
    bool hit = false;
    glm::vec3 bestNormal(0.0f, 0.0f, 1.0f);
    float bestPenetration = 0.0f;
    for (const CollisionTriangle& tri : world.collisionMesh.triangles)
    {
        glm::vec3 closest = closestPtOnTri(projectile.position, tri.a, tri.b, tri.c);
        glm::vec3 diff = projectile.position - closest;
        float dist = glm::length(diff);
        if (dist >= projectile.radius || dist <= 0.0001f)
            continue;
        float penetration = projectile.radius - dist;
        if (penetration > bestPenetration)
        {
            bestPenetration = penetration;
            bestNormal = diff / dist;
            if (glm::dot(bestNormal, tri.normal) < 0.0f)
                bestNormal = -bestNormal;
            hit = true;
        }
    }
    if (!hit) return false;
    projectile.position += bestNormal * (bestPenetration + 0.001f);
    float into = glm::dot(projectile.velocity, bestNormal);
    if (into < 0.0f)
    {
        glm::vec3 tangent = projectile.velocity - bestNormal * into;
        projectile.velocity -= bestNormal * into * (1.0f + restitution);
        projectile.velocity += tangent * friction;
    }
    projectile.angularVelocity *= 0.35f;
    return true;
}

void mpUpdateNetworkProjectiles(MultiplayerContext& ctx, float dt, const World& world)
{
    // ── Retransmit unacknowledged fire requests ─────────────────────
    const uint64_t now = nowMs();
    for (auto it = ctx.pendingFireRequests.begin(); it != ctx.pendingFireRequests.end(); )
    {
        MultiplayerContext::PendingFireRequest& pfr = it->second;
        if (pfr.acknowledged)
        {
            it = ctx.pendingFireRequests.erase(it);
            continue;
        }
        // Retry every 100ms, up to 10 attempts
        if (now - pfr.lastSentMs >= 100 && pfr.attempts < 10)
        {
            ProjectileFireRequestPacket packet{};
            packet.header.type = PACKET_PROJECTILE_FIRE_REQUEST;
            packet.header.tick = ctx.tick;
            packet.header.playerId = ctx.localPlayerId;
            packet.fireSerial = pfr.fireSerial;
            packet.lastServerTick = ctx.latestServerTick;
            packet.weapon = pfr.weapon;
            packet.originX = pfr.origin.x; packet.originY = pfr.origin.y; packet.originZ = pfr.origin.z;
            packet.dirX = pfr.direction.x; packet.dirY = pfr.direction.y; packet.dirZ = pfr.direction.z;
            mpSendPacket(ctx, &packet, sizeof(packet));
            pfr.lastSentMs = now;
            pfr.attempts++;
            printf("[PROJECTILE FIRE RETRY] fireSerial=%u attempt=%d\n",
                   pfr.fireSerial, pfr.attempts);
        }
        // Timeout after 3 seconds
        if (now - pfr.firstSentMs > 3000)
        {
            printf("[PROJECTILE FIRE TIMEOUT] fireSerial=%u dropped\n", pfr.fireSerial);
            it = ctx.pendingFireRequests.erase(it);
        }
        else
        {
            ++it;
        }
    }

    for (auto it = ctx.networkProjectiles.begin(); it != ctx.networkProjectiles.end(); )
    {
        NetworkProjectile& projectile = it->second;
        projectile.previousPosition = projectile.position;
        projectile.age += dt;

        if (projectile.lifetime > 0.0f && projectile.age > projectile.lifetime + 1.0f)
        {
            it = ctx.networkProjectiles.erase(it);
            continue;
        }

        // ── Predicted projectile: local physics simulation ──────────
        if (projectile.predicted)
        {
            const WeaponDefinition* def = projectileDefinition(projectile.weaponType);
            const float grav = projectileGravity(projectile.weaponType);
            const float drag = projectileDrag(projectile.weaponType);
            const float angDrag = def ? cp(def, "angularDrag", 0.3f) : 0.0f;

            // Gravity
            if (grav > 0.0f)
                projectile.velocity.z -= grav * dt;

            // Drag
            if (drag > 0.0f)
                projectile.velocity *= std::max(0.0f, 1.0f - drag * dt);

            // Angular velocity
            float angSpeed = glm::length(projectile.angularVelocity);
            if (angDrag > 0.0f && angSpeed > 0.0f)
                projectile.angularVelocity *= std::max(0.0f, 1.0f - angDrag * dt);
            if (angSpeed > 0.0001f)
            {
                float newAngSpeed = glm::length(projectile.angularVelocity);
                if (newAngSpeed > 0.0001f)
                {
                    glm::quat delta = glm::angleAxis(newAngSpeed * dt, glm::normalize(projectile.angularVelocity));
                    projectile.rotation = glm::normalize(delta * projectile.rotation);
                }
            }

            // Move
            glm::vec3 step = projectile.velocity * dt;
            projectile.position += step;

            // World collision (grenade bouncing)
            if (projectile.weaponType == NETWORK_WEAPON_GRENADE_LAUNCHER)
                resolveClientGrenadeWorld(projectile, world, def);

            // Blend toward server state if available
            if (projectile.hasTargetState)
            {
                constexpr float BLEND_RATE = 0.15f;
                projectile.position = glm::mix(projectile.position, projectile.targetStatePos, BLEND_RATE);
                projectile.velocity = glm::mix(projectile.velocity, projectile.targetStateVel, BLEND_RATE);
            }

            // Render from predicted position
            projectile.renderPosition = projectile.position;
            projectile.renderVelocity = projectile.velocity;
            projectile.renderRotation = projectile.rotation;
        }
        else if (projectile.hasTargetState)
        {
            // Normal interpolation or extrapolation
            constexpr float SERVER_TICK_INTERVAL = 1.0f / 60.0f; // 60 Hz server
            constexpr float EXTRAPOLATION_LIMIT = 0.15f; // 150ms max extrapolation

            const uint64_t nowMsVal = nowMs();
            const float elapsedSinceTargetMs = (float)(nowMsVal - projectile.lastTargetReceivedMs);

            if (elapsedSinceTargetMs < 200.0f)
            {
                // Normal interpolation between previous and target
                const float timeBetweenStates = (float)(projectile.targetStateTick - projectile.prevStateTick) * SERVER_TICK_INTERVAL;
                const float timeSincePrev = elapsedSinceTargetMs / 1000.0f;
                const float alpha = timeBetweenStates > 0.001f
                    ? std::clamp(timeSincePrev / timeBetweenStates, 0.0f, 1.0f)
                    : 1.0f;

                projectile.renderPosition = glm::mix(projectile.prevStatePos, projectile.targetStatePos, alpha);
                projectile.renderVelocity = glm::mix(projectile.prevStateVel, projectile.targetStateVel, alpha);
                projectile.renderRotation = glm::normalize(glm::slerp(projectile.prevStateRot, projectile.targetStateRot, alpha));
            }
            else
            {
                // Extrapolation: continue from target state with its velocity
                const float extrapSec = std::min((elapsedSinceTargetMs - 200.0f) / 1000.0f, EXTRAPOLATION_LIMIT);
                projectile.renderPosition = projectile.targetStatePos + projectile.targetStateVel * extrapSec;
                projectile.renderVelocity = projectile.targetStateVel;
                projectile.renderRotation = projectile.targetStateRot;
            }
        }
        else
        {
            // Fallback: just use authoritative position directly
            projectile.renderPosition = projectile.position;
            projectile.renderVelocity = projectile.velocity;
        }

        // Rotation from render velocity
        const float renderSpeed = glm::length(projectile.renderVelocity);
        if (renderSpeed > 0.001f)
        {
            projectile.renderRotation = glm::rotation(
                glm::vec3(0.0f, 0.0f, 1.0f),
                glm::normalize(projectile.renderVelocity));
        }

        spawnProjectileTrail(projectile, dt);
        ++it;
    }
}

void mpRenderNetworkProjectiles(const MultiplayerContext& ctx, const Camera& camera)
{
    for (const auto& entry : ctx.networkProjectiles)
    {
        const NetworkProjectile& projectile = entry.second;
        if (projectile.exploded)
            continue;
        ProjectileVisualConfig cfg = projectileVisualConfig(projectile.weaponType);
        renderProjectile(camera, projectile.renderPosition, projectile.renderRotation, cfg);
    }
}

} // namespace MimitaNet
