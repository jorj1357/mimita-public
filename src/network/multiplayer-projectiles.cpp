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
#include "combat/weapon-types.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"

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
        part.position = projectile.position;
        part.velocity = rocket
            ? projectile.velocity * -0.08f
            : projectile.velocity * 0.10f;
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
    NetworkProjectile& projectile = ctx.networkProjectiles[event->projectileId];
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
    auto it = ctx.networkProjectiles.find(event->projectileId);
    if (it == ctx.networkProjectiles.end())
        return;
    NetworkProjectile& projectile = it->second;
    projectile.position = {event->posX, event->posY, event->posZ};
    projectile.velocity = {event->velX, event->velY, event->velZ};
    projectile.rotation = glm::quat(event->rotW, event->rotX, event->rotY, event->rotZ);
    projectile.angularVelocity = {event->angVelX, event->angVelY, event->angVelZ};
    projectile.age = event->age;
}

void mpProcessProjectileExplodeEventPacket(MultiplayerContext& ctx, const ProjectileExplodeEventPacket* event)
{
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

void mpProcessProjectileDespawnEventPacket(MultiplayerContext& ctx, const ProjectileDespawnEventPacket* event)
{
    ctx.networkProjectiles.erase(event->projectileId);
}

void mpProcessMeleeHitEventPacket(MultiplayerContext& ctx, const MeleeHitEventPacket* event)
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

void mpUpdateNetworkProjectiles(MultiplayerContext& ctx, float dt)
{
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
        projectile.velocity.z -= projectileGravity(projectile.weaponType) * dt;
        projectile.velocity *= std::max(0.0f, 1.0f - projectileDrag(projectile.weaponType) * dt);
        projectile.position += projectile.velocity * dt;
        const float angSpeed = glm::length(projectile.angularVelocity);
        if (angSpeed > 0.0001f)
        {
            glm::quat delta = glm::angleAxis(
                angSpeed * dt, glm::normalize(projectile.angularVelocity));
            projectile.rotation = glm::normalize(delta * projectile.rotation);
        }
        else if (glm::length(projectile.velocity) > 0.001f)
        {
            projectile.rotation = glm::rotation(
                glm::vec3(0.0f, 0.0f, 1.0f),
                glm::normalize(projectile.velocity));
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
        renderProjectile(camera, projectile.position, projectile.rotation, cfg);
    }
}

} // namespace MimitaNet
