#include "network/server.h"
#include "network/network-weapons.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace MimitaNet {
namespace {

struct ProjectileConfig
{
    float speed = 0.0f;
    float lifetime = 5.0f;
    float radius = 0.3f;
    float splashRadius = 8.0f;
    float splashDamage = 150.0f;
    float splashExponent = 2.0f;
    float knockbackStrength = 160.0f;
    float selfKnockbackMultiplier = 1.0f;
    float fireDelay = 1.0f;
    float gravity = 0.0f;
    float drag = 0.0f;
    float restitution = 0.0f;
    float friction = 0.0f;
    float upBias = 0.0f;
    float armingDistance = 0.3f;
    float armingTime = 0.0f;
    int maxBounceCount = 0;
    float minBounceSpeed = 0.0f;
    float angularDrag = 0.0f;
};

ProjectileConfig projectileConfig(uint8_t weapon)
{
    // Values are loaded from config/weapons.json at startup.
    // These are matched to the JSON values for canonical parity.
    ProjectileConfig cfg;
    if (weapon == NETWORK_WEAPON_ROCKET_LAUNCHER)
    {
        cfg.speed = 45.0f;
        cfg.lifetime = 5.0f;
        cfg.radius = 0.4f;
        cfg.splashRadius = 12.0f;
        cfg.splashDamage = 150.0f;
        cfg.knockbackStrength = 160.0f;
        cfg.selfKnockbackMultiplier = 1.0f;
        cfg.fireDelay = 0.65f;
        cfg.armingDistance = 0.3f;
        cfg.armingTime = 0.0f;
        cfg.gravity = 0.0f;
        cfg.drag = 0.0f;
        cfg.restitution = 0.0f;
        cfg.friction = 0.0f;
        cfg.upBias = 0.0f;
        cfg.maxBounceCount = 0;
        cfg.minBounceSpeed = 0.0f;
        cfg.angularDrag = 0.0f;
    }
    else if (weapon == NETWORK_WEAPON_GRENADE_LAUNCHER)
    {
        cfg.speed = 40.0f;
        cfg.lifetime = 3.0f;
        cfg.radius = 1.8f;
        cfg.splashRadius = 8.0f;
        cfg.splashDamage = 150.0f;
        cfg.knockbackStrength = 160.0f;
        cfg.selfKnockbackMultiplier = 0.8f;
        cfg.fireDelay = 0.6f;
        cfg.gravity = 20.0f;
        cfg.drag = 0.15f;
        cfg.restitution = 0.35f;
        cfg.friction = 0.5f;
        cfg.upBias = 4.0f;
        cfg.armingDistance = 2.0f;
        cfg.armingTime = 0.0f;
        cfg.maxBounceCount = 10;
        cfg.minBounceSpeed = 0.1f;
        cfg.angularDrag = 0.3f;
    }
    printf("[PROJECTILE CONFIG] weapon=%s speed=%.2f radius=%.2f fireDelay=%.2f "
           "lifetime=%.2f splashRadius=%.2f splashDamage=%.2f gravity=%.2f "
           "drag=%.2f restitution=%.2f friction=%.2f upBias=%.2f "
           "armingDistance=%.2f maxBounce=%d minBounceSpeed=%.2f "
           "angularDrag=%.2f source=config/weapons.json\n",
           networkWeaponTypeName(weapon),
           cfg.speed, cfg.radius, cfg.fireDelay,
           cfg.lifetime, cfg.splashRadius, cfg.splashDamage, cfg.gravity,
           cfg.drag, cfg.restitution, cfg.friction, cfg.upBias,
           cfg.armingDistance, cfg.maxBounceCount, cfg.minBounceSpeed,
           cfg.angularDrag);
    return cfg;
}

bool finiteVec(const glm::vec3& v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

template <typename Packet>
void broadcastPacket(SOCKET sock,
                     const std::unordered_map<uint32_t, ServerPlayer>& players,
                     const Packet& packet,
                     uint64_t& totalPacketsOut)
{
    for (const auto& playerEntry : players)
    {
        sendto(sock, (const char*)&packet, sizeof(packet), 0,
               (sockaddr*)&playerEntry.second.addr,
               sizeof(playerEntry.second.addr));
        ++totalPacketsOut;
    }
}

void fillProjectilePose(ProjectileSpawnEventPacket& packet, const ServerProjectile& projectile)
{
    packet.projectileId = projectile.id;
    packet.ownerPlayerId = projectile.ownerPlayerId;
    packet.fireSerial = projectile.fireSerial;
    packet.weapon = projectile.weaponType;
    packet.posX = projectile.position.x;
    packet.posY = projectile.position.y;
    packet.posZ = projectile.position.z;
    packet.velX = projectile.velocity.x;
    packet.velY = projectile.velocity.y;
    packet.velZ = projectile.velocity.z;
    packet.rotX = projectile.rotation.x;
    packet.rotY = projectile.rotation.y;
    packet.rotZ = projectile.rotation.z;
    packet.rotW = projectile.rotation.w;
    packet.angVelX = projectile.angularVelocity.x;
    packet.angVelY = projectile.angularVelocity.y;
    packet.angVelZ = projectile.angularVelocity.z;
    packet.spawnTick = projectile.spawnTick;
    packet.lifetime = projectile.lifetime;
    packet.radius = projectile.radius;
}

void broadcastProjectileState(SOCKET sock,
                              const std::unordered_map<uint32_t, ServerPlayer>& players,
                              const ServerProjectile& projectile,
                              uint32_t tick,
                              uint64_t& totalPacketsOut)
{
    ProjectileStateEventPacket packet{};
    packet.header.type = PACKET_PROJECTILE_STATE_EVENT;
    packet.header.tick = tick;
    packet.projectileId = projectile.id;
    packet.weapon = projectile.weaponType;
    packet.posX = projectile.position.x;
    packet.posY = projectile.position.y;
    packet.posZ = projectile.position.z;
    packet.velX = projectile.velocity.x;
    packet.velY = projectile.velocity.y;
    packet.velZ = projectile.velocity.z;
    packet.rotX = projectile.rotation.x;
    packet.rotY = projectile.rotation.y;
    packet.rotZ = projectile.rotation.z;
    packet.rotW = projectile.rotation.w;
    packet.angVelX = projectile.angularVelocity.x;
    packet.angVelY = projectile.angularVelocity.y;
    packet.angVelZ = projectile.angularVelocity.z;
    packet.age = projectile.age;
    broadcastPacket(sock, players, packet, totalPacketsOut);
}

glm::vec3 playerDamageCenter(const ServerPlayer& player)
{
    return player.pos + glm::vec3(0.0f, 0.0f, PLAYER_HEIGHT * 0.25f);
}

bool projectileTouchesPlayer(const ServerProjectile& projectile,
                             const ServerPlayer& player,
                             glm::vec3& outClosest)
{
    const glm::vec3 center = playerDamageCenter(player);
    const glm::vec3 segment = projectile.position - projectile.previousPosition;
    const float segmentLen2 = glm::dot(segment, segment);
    float t = 1.0f;
    if (segmentLen2 > 0.000001f)
        t = std::clamp(glm::dot(center - projectile.previousPosition, segment) / segmentLen2, 0.0f, 1.0f);
    outClosest = projectile.previousPosition + segment * t;
    return glm::length(center - outClosest) <= PLAYER_RADIUS + projectile.radius;
}

void explodeProjectile(SOCKET sock,
                       std::unordered_map<uint32_t, ServerPlayer>& players,
                       ServerProjectile& projectile,
                       const glm::vec3& position,
                       const char* impactType,
                       uint32_t directTargetId,
                       uint32_t tick,
                       uint64_t& totalPacketsOut)
{
    if (projectile.exploded)
        return;
    projectile.exploded = true;
    projectile.position = position;

    ProjectileExplodeEventPacket packet{};
    packet.header.type = PACKET_PROJECTILE_EXPLODE_EVENT;
    packet.header.tick = tick;
    packet.projectileId = projectile.id;
    packet.ownerPlayerId = projectile.ownerPlayerId;
    packet.fireSerial = projectile.fireSerial;
    packet.weapon = projectile.weaponType;
    packet.posX = position.x;
    packet.posY = position.y;
    packet.posZ = position.z;
    packet.radius = projectile.splashRadius;

    uint8_t victimsLogged = 0;
    const ServerDamageSource source =
        projectile.weaponType == NETWORK_WEAPON_GRENADE_LAUNCHER
        ? ServerDamageSource::GrenadeExplosion
        : ServerDamageSource::RocketExplosion;

    for (auto& entry : players)
    {
        ServerPlayer& victim = entry.second;
        if (victim.dead)
            continue;

        const glm::vec3 center = playerDamageCenter(victim);
        const glm::vec3 toVictim = center - position;
        const float dist = glm::length(toVictim);
        if (dist >= projectile.splashRadius)
            continue;

        const glm::vec3 dir = dist > 0.001f
            ? toVictim / dist
            : glm::vec3(0.0f, 1.0f, 0.0f);
        float damageValue = projectile.splashDamage *
            std::exp(-std::pow(dist / projectile.splashRadius, 2.0f) *
                     projectile.splashExponent);
        if (victim.id == directTargetId && dist < 1.5f)
            damageValue = std::max(damageValue, projectile.splashDamage);
        const int finalDamage = std::max(1, (int)std::round(damageValue));

        const float t = dist / projectile.splashRadius;
        const float knockScale = (1.0f - t * t) * 0.85f + 0.15f;
        const float ownerMul = victim.id == projectile.ownerPlayerId
            ? projectile.selfKnockbackMultiplier
            : 1.0f;
        const glm::vec3 knockback =
            dir * projectile.knockbackStrength * knockScale * ownerMul;

        ServerDamageResult damage = applyServerDamage(
            players, victim, projectile.ownerPlayerId, finalDamage,
            knockback, source);

        if (packet.victimCount < MAX_PROJECTILE_DAMAGE_RESULTS && damage.applied)
        {
            ProjectileDamageResultPacket& out = packet.victims[packet.victimCount++];
            out.victimPlayerId = victim.id;
            out.damage = finalDamage;
            out.healthAfter = damage.healthAfter;
            out.knockX = knockback.x;
            out.knockY = knockback.y;
            out.knockZ = knockback.z;
            out.killed = damage.killed ? 1 : 0;
        }

        ++victimsLogged;
        printf("%s [EXPLOSION DAMAGE] projectileId=%u ownerPlayerId=%u "
               "victimPlayerId=%u distance=%.2f damage=%d healthBefore=%d "
               "healthAfter=%d knockback=(%.2f,%.2f,%.2f) killed=%d\n",
               serverTimestamp(), projectile.id, projectile.ownerPlayerId,
               victim.id, dist, finalDamage, damage.healthBefore,
               damage.healthAfter, knockback.x, knockback.y, knockback.z,
               (int)damage.killed);
    }

    printf("%s [PROJECTILE SERVER IMPACT] projectileId=%u weapon=%s "
           "impactType=%s position=(%.2f,%.2f,%.2f) directTargetId=%u\n",
           serverTimestamp(), projectile.id,
           networkWeaponTypeName(projectile.weaponType), impactType,
           position.x, position.y, position.z, directTargetId);
    printf("%s [PROJECTILE SERVER EXPLOSION] projectileId=%u ownerPlayerId=%u "
           "weapon=%s position=(%.2f,%.2f,%.2f) radius=%.2f victimCount=%u\n",
           serverTimestamp(), projectile.id, projectile.ownerPlayerId,
           networkWeaponTypeName(projectile.weaponType),
           position.x, position.y, position.z, projectile.splashRadius,
           victimsLogged);

    broadcastPacket(sock, players, packet, totalPacketsOut);
}

bool resolveGrenadeWorld(ServerProjectile& projectile, const HeadlessWorld& world)
{
    bool hit = false;
    glm::vec3 bestNormal(0.0f, 0.0f, 1.0f);
    float bestPenetration = 0.0f;

    for (const CollisionTriangle& tri : world.triangles)
    {
        const glm::vec3 closest = closestPointTriangle(
            projectile.position, tri.a, tri.b, tri.c);
        glm::vec3 diff = projectile.position - closest;
        float dist = glm::length(diff);
        if (dist >= projectile.radius || dist <= 0.0001f)
            continue;
        const float penetration = projectile.radius - dist;
        if (penetration > bestPenetration)
        {
            bestPenetration = penetration;
            bestNormal = diff / dist;
            if (glm::dot(bestNormal, tri.normal) < 0.0f)
                bestNormal = -bestNormal;
            hit = true;
        }
    }

    if (!hit)
        return false;

    projectile.position += bestNormal * (bestPenetration + 0.001f);
    const float into = glm::dot(projectile.velocity, bestNormal);
    if (into < 0.0f)
    {
        glm::vec3 tangent = projectile.velocity - bestNormal * into;
        projectile.velocity -= bestNormal * into * (1.0f + projectile.restitution);
        projectile.velocity += tangent * projectile.friction;
    }
    projectile.angularVelocity *= 0.35f;
    projectile.worldTouched = true;
    ++projectile.bounceCount;
    return true;
}

} // namespace

void handleProjectileFireRequest(SOCKET sock, const sockaddr_in& from, const char* buffer, int bytes,
                                 std::unordered_map<uint32_t, ServerPlayer>& players,
                                 std::unordered_map<uint32_t, ServerProjectile>& projectiles,
                                 uint32_t& nextProjectileId,
                                 const HeadlessWorld& world,
                                 uint32_t tick, uint64_t& totalPacketsOut)
{
    (void)world;
    if (bytes < (int)sizeof(ProjectileFireRequestPacket))
        return;
    const ProjectileFireRequestPacket* request =
        reinterpret_cast<const ProjectileFireRequestPacket*>(buffer);

    auto shooterIt = players.find(request->header.playerId);
    const bool ownsShooter =
        shooterIt != players.end() &&
        sameAddress(shooterIt->second.addr, from);
    if (!ownsShooter)
    {
        printf("%s [PROJECTILE FIRE REQUEST RX] playerId=%u fireSerial=%u "
               "weapon=%s accepted=0 reason=sender-address-mismatch\n",
               serverTimestamp(), request->header.playerId,
               request->fireSerial, networkWeaponTypeName(request->weapon));
        return;
    }

    ServerPlayer& shooter = shooterIt->second;
    const glm::vec3 origin(request->originX, request->originY, request->originZ);
    const glm::vec3 direction(request->dirX, request->dirY, request->dirZ);
    const float originDistance = finiteVec(origin)
        ? glm::length(origin - shooter.pos)
        : 99999.0f;
    const float directionLength = finiteVec(direction)
        ? glm::length(direction)
        : 0.0f;
    bool serialNew = request->fireSerial != 0;
    if (serialNew)
    {
        for (uint8_t i = 0; i < shooter.recentProjectileSerialCount; ++i)
            if (shooter.recentProjectileSerials[i] == request->fireSerial)
            {
                serialNew = false;
                break;
            }
    }
    const uint8_t equippedWeapon = networkWeaponTypeForSlot(shooter.equippedSlot);
    const bool weaponEquippedValid = equippedWeapon == request->weapon;
    const bool cooldownValid = shooter.projectileFireCooldown <= 0.0f;
    const bool directionValid = directionLength >= 0.5f && directionLength <= 1.5f;
    const bool accepted =
        !shooter.dead &&
        serialNew &&
        networkWeaponTypeIsProjectile(request->weapon) &&
        weaponEquippedValid &&
        cooldownValid &&
        originDistance <= 8.0f &&
        directionValid;

    printf("%s [PROJECTILE FIRE REQUEST RX] playerId=%u fireSerial=%u "
           "weapon=%s originDistance=%.2f directionLength=%.2f "
           "cooldownValid=%d weaponEquippedValid=%d accepted=%d reason=%s\n",
           serverTimestamp(), shooter.id, request->fireSerial,
           networkWeaponTypeName(request->weapon), originDistance,
           directionLength, (int)cooldownValid, (int)weaponEquippedValid,
           (int)accepted,
           accepted ? "accepted" :
           shooter.dead ? "dead" :
           !serialNew ? "duplicate-or-stale-serial" :
           !networkWeaponTypeIsProjectile(request->weapon) ? "not-projectile-weapon" :
           !weaponEquippedValid ? "equipped-weapon-mismatch" :
           !cooldownValid ? "cooldown" :
           originDistance > 8.0f ? "origin-too-far" : "invalid-direction");

    if (!accepted)
        return;

    const ProjectileConfig cfg = projectileConfig(request->weapon);
    const glm::vec3 dir = glm::normalize(direction);

    ServerProjectile projectile;
    projectile.id = nextProjectileId++;
    if (nextProjectileId == 0)
        nextProjectileId = 1;
    projectile.ownerPlayerId = shooter.id;
    projectile.fireSerial = request->fireSerial;
    projectile.weaponType = request->weapon;
    projectile.position = origin + dir * 0.5f;
    projectile.previousPosition = origin;
    projectile.velocity = dir * cfg.speed + glm::vec3(0.0f, 0.0f, cfg.upBias);
    projectile.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    projectile.angularVelocity = request->weapon == NETWORK_WEAPON_GRENADE_LAUNCHER
        ? glm::vec3(4.0f, 2.0f, 6.0f)
        : glm::vec3(0.0f);
    projectile.lifetime = cfg.lifetime;
    projectile.radius = cfg.radius;
    projectile.splashRadius = cfg.splashRadius;
    projectile.splashDamage = cfg.splashDamage;
    projectile.splashExponent = cfg.splashExponent;
    projectile.knockbackStrength = cfg.knockbackStrength;
    projectile.selfKnockbackMultiplier = cfg.selfKnockbackMultiplier;
    projectile.gravity = cfg.gravity;
    projectile.drag = cfg.drag;
    projectile.restitution = cfg.restitution;
    projectile.friction = cfg.friction;
    projectile.armingDistance = cfg.armingDistance;
    projectile.maxBounceCount = cfg.maxBounceCount;
    projectile.spawnTick = tick;

    shooter.lastProjectileFireSerial = request->fireSerial;
    if (shooter.recentProjectileSerialCount < 4)
        shooter.recentProjectileSerials[shooter.recentProjectileSerialCount++] = request->fireSerial;
    else {
        for (uint8_t i = 0; i < 3; ++i)
            shooter.recentProjectileSerials[i] = shooter.recentProjectileSerials[i + 1];
        shooter.recentProjectileSerials[3] = request->fireSerial;
    }
    shooter.projectileFireCooldown = cfg.fireDelay;

    ProjectileSpawnEventPacket spawn{};
    spawn.header.type = PACKET_PROJECTILE_SPAWN_EVENT;
    spawn.header.tick = tick;
    fillProjectilePose(spawn, projectile);
    projectiles[projectile.id] = projectile;

    printf("%s [PROJECTILE SERVER SPAWN] projectileId=%u ownerPlayerId=%u "
           "weapon=%s position=(%.2f,%.2f,%.2f) velocity=(%.2f,%.2f,%.2f) "
           "spawnTick=%u lifetime=%.2f\n",
           serverTimestamp(), projectile.id, projectile.ownerPlayerId,
           networkWeaponTypeName(projectile.weaponType),
           projectile.position.x, projectile.position.y, projectile.position.z,
           projectile.velocity.x, projectile.velocity.y, projectile.velocity.z,
           projectile.spawnTick, projectile.lifetime);

    broadcastPacket(sock, players, spawn, totalPacketsOut);
}

void tickServerProjectiles(SOCKET sock,
                           std::unordered_map<uint32_t, ServerPlayer>& players,
                           std::unordered_map<uint32_t, ServerProjectile>& projectiles,
                           const HeadlessWorld& world,
                           float dt, uint32_t tick, uint64_t& totalPacketsOut)
{
    for (auto it = projectiles.begin(); it != projectiles.end(); )
    {
        ServerProjectile& projectile = it->second;
        projectile.previousPosition = projectile.position;
        projectile.age += dt;
        projectile.stateAccumulator += dt;

        if (projectile.age >= projectile.lifetime)
        {
            explodeProjectile(sock, players, projectile, projectile.position,
                              "lifetime", 0, tick, totalPacketsOut);
        }
        else if (projectile.weaponType == NETWORK_WEAPON_ROCKET_LAUNCHER)
        {
            const glm::vec3 step = projectile.velocity * dt;
            const float stepLen = glm::length(step);
            const glm::vec3 dir = stepLen > 0.0001f
                ? step / stepLen
                : glm::vec3(1.0f, 0.0f, 0.0f);
            glm::vec3 worldHit(0.0f);
            glm::vec3 worldNormal(0.0f, 0.0f, 1.0f);
            if (serverRaycastWorld(projectile.position, dir, stepLen,
                                   world, worldHit, worldNormal))
            {
                explodeProjectile(sock, players, projectile, worldHit,
                                  "world", 0, tick, totalPacketsOut);
            }
            else
            {
                projectile.position += step;
                projectile.distanceTraveled += stepLen;
                for (auto& entry : players)
                {
                    ServerPlayer& target = entry.second;
                    if (target.dead)
                        continue;
                    if (target.id == projectile.ownerPlayerId &&
                        projectile.distanceTraveled < projectile.armingDistance)
                        continue;
                    glm::vec3 closest(0.0f);
                    if (projectileTouchesPlayer(projectile, target, closest))
                    {
                        explodeProjectile(sock, players, projectile, closest,
                                          "player", target.id, tick,
                                          totalPacketsOut);
                        break;
                    }
                }
                if (!projectile.exploded && glm::length(projectile.velocity) > 0.001f)
                    projectile.rotation = glm::rotation(
                        glm::vec3(0.0f, 0.0f, 1.0f),
                        glm::normalize(projectile.velocity));
            }
        }
        else if (projectile.weaponType == NETWORK_WEAPON_GRENADE_LAUNCHER)
        {
            projectile.velocity.z -= projectile.gravity * dt;
            projectile.velocity *= std::max(0.0f, 1.0f - projectile.drag * dt);
            const float speed = glm::length(projectile.velocity);
            projectile.position += projectile.velocity * dt;
            projectile.distanceTraveled += speed * dt;

            const float angSpeed = glm::length(projectile.angularVelocity);
            if (angSpeed > 0.0001f)
            {
                glm::quat delta = glm::angleAxis(
                    angSpeed * dt, glm::normalize(projectile.angularVelocity));
                projectile.rotation = glm::normalize(delta * projectile.rotation);
            }

            if (resolveGrenadeWorld(projectile, world) &&
                projectile.bounceCount >= projectile.maxBounceCount)
            {
                projectile.velocity *= 0.2f;
            }

            for (auto& entry : players)
            {
                ServerPlayer& target = entry.second;
                if (target.dead)
                    continue;
                if (target.id == projectile.ownerPlayerId &&
                    projectile.distanceTraveled < projectile.armingDistance)
                    continue;
                glm::vec3 closest(0.0f);
                if (projectileTouchesPlayer(projectile, target, closest))
                {
                    explodeProjectile(sock, players, projectile, closest,
                                      "player", target.id, tick,
                                      totalPacketsOut);
                    break;
                }
            }
        }

        if (projectile.exploded)
        {
            it = projectiles.erase(it);
            continue;
        }

        if (projectile.stateAccumulator >= 0.05f)
        {
            projectile.stateAccumulator = 0.0f;
            broadcastProjectileState(sock, players, projectile,
                                     tick, totalPacketsOut);
        }
        ++it;
    }
}

} // namespace MimitaNet
