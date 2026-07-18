#include "network/server.h"
#include "network/network-weapons.h"
#include "debug/structured-log.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "combat/weapon-registry.h"
#include "combat/weapon-types.h"

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
    float angularSpeed = 6.0f;
};

ProjectileConfig projectileConfig(uint8_t weapon)
{
    ProjectileConfig cfg;

    const char* weaponId = nullptr;
    if (weapon == NETWORK_WEAPON_ROCKET_LAUNCHER)
        weaponId = "rocket_launcher";
    else if (weapon == NETWORK_WEAPON_GRENADE_LAUNCHER)
        weaponId = "grenade_launcher";
    else
        return cfg;

    const WeaponDefinition* def = WeaponRegistry::instance().get(weaponId);
    if (!def)
    {
        printf("[PROJECTILE CONFIG] weapon=%s NOT FOUND in registry (id=%s) — using fallback defaults\n",
               networkWeaponTypeName(weapon), weaponId);
        return cfg;
    }

    cfg.speed = def->projectileSpeed > 0.0f ? def->projectileSpeed : 40.0f;
    cfg.lifetime = def->projectileLifetime > 0.0f ? def->projectileLifetime : 5.0f;
    cfg.radius = def->projectileRadius > 0.0f ? def->projectileRadius : 0.3f;
    cfg.fireDelay = def->fireDelay > 0.0f ? def->fireDelay : 1.0f;

    auto cp = [&](const char* key, float fallback) -> float {
        auto it = def->customParams.find(key);
        return it != def->customParams.end() ? it->second : fallback;
    };

    cfg.splashRadius = cp("splashRadius", 8.0f);
    cfg.splashDamage = cp("rocketDirectDamage", 150.0f);
    cfg.splashExponent = cp("splashExponent", 2.0f);
    cfg.knockbackStrength = cp("knockbackStrength", 160.0f);
    cfg.selfKnockbackMultiplier = cp("selfKnockbackMultiplier", 1.0f);
    cfg.gravity = cp("gravity", 0.0f);
    cfg.drag = cp("drag", 0.0f);
    cfg.restitution = cp("bounceRestitution", 0.0f);
    cfg.friction = cp("bounceFriction", 0.0f);
    cfg.upBias = cp("upBias", 0.0f);
    cfg.armingDistance = cp("armingDistance", 0.3f);
    cfg.armingTime = cp("armingTime", 0.0f);
    cfg.maxBounceCount = (int)cp("maxBounceCount", 0.0f);
    cfg.minBounceSpeed = cp("minBounceSpeed", 0.0f);
    cfg.angularDrag = cp("angularDrag", 0.0f);
    cfg.angularSpeed = cp("angSpeed", 6.0f);

    printf("[PROJECTILE CONFIG] weapon=%s speed=%.2f radius=%.2f fireDelay=%.2f "
           "lifetime=%.2f splashRadius=%.2f splashDamage=%.2f gravity=%.2f "
           "drag=%.2f restitution=%.2f friction=%.2f upBias=%.2f "
           "armingDistance=%.2f maxBounce=%d minBounceSpeed=%.2f "
           "angularDrag=%.2f source=weapon-registry\n",
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
        if (playerEntry.second.transport)
        {
            playerEntry.second.transport->send(&packet, sizeof(packet));
        }
        else
        {
            sendto(sock, (const char*)&packet, sizeof(packet), 0,
                   (sockaddr*)&playerEntry.second.addr,
                   sizeof(playerEntry.second.addr));
        }
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
    {
        auto& _log = ::StructuredLogger::instance();
        if (_log.shouldLog(::StructuredCategory::GrenadeLauncher, ::StructuredLevel::Important)) {
            ::StructuredLogger::Entry e;
            e.category = ::StructuredCategory::GrenadeLauncher;
            e.level = ::StructuredLevel::Important;
            e.eventId = "GRENADE_SERVER_EXPLODE";
            e.correlationId = "GRENADE_P" + std::to_string(projectile.ownerPlayerId)
                + "_F" + std::to_string(projectile.fireSerial)
                + "_J" + std::to_string(projectile.id);
            e.reason = impactType;
            char buf[512]; std::snprintf(buf, sizeof(buf),
                "position=(%.2f,%.2f,%.2f) age=%.2f splashRadius=%.2f splashDamage=%.1f "
                "victimCount=%u bounceCount=%d distanceTraveled=%.1f",
                position.x, position.y, position.z, projectile.age,
                projectile.splashRadius, projectile.splashDamage,
                victimsLogged, projectile.bounceCount, projectile.distanceTraveled);
            e.message = buf;
            _log.write(e);
        }
    }

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
            // Use the separation direction (center→closestPoint, points toward sphere center from surface)
            // Do NOT flip based on triangle winding — the closest-point direction is already correct.
            bestNormal = diff / dist;
            hit = true;
        }
    }

    if (!hit)
        return false;

    // Depenetrate
    projectile.position += bestNormal * (bestPenetration + 0.001f);

    const float into = glm::dot(projectile.velocity, bestNormal);

    // Log every overlap (inward or outward)
    {
        auto& _lg = ::StructuredLogger::instance();
        if (_lg.shouldLog(::StructuredCategory::GrenadeLauncher, ::StructuredLevel::Verbose)) {
            ::StructuredLogger::Entry e;
            e.category = ::StructuredCategory::GrenadeLauncher;
            e.level = ::StructuredLevel::Verbose;
            e.eventId = into < 0.0f ? "" : "GRENADE_SERVER_CONTACT_OVERLAP";
            if (e.eventId.empty()) {
                // Impact is logged separately below; skip dup
            } else {
                e.correlationId = "GRENADE_P" + std::to_string(projectile.ownerPlayerId)
                    + "_F" + std::to_string(projectile.fireSerial)
                    + "_J" + std::to_string(projectile.id);
                e.reason = "overlap-only";
                char b[512]; std::snprintf(b, sizeof(b),
                    "into=%.3f penetration=%.3f normal=(%.3f,%.3f,%.3f) "
                    "vel=(%.2f,%.2f,%.2f) speed=%.2f bounceCount=%d",
                    into, bestPenetration, bestNormal.x, bestNormal.y, bestNormal.z,
                    projectile.velocity.x, projectile.velocity.y, projectile.velocity.z,
                    glm::length(projectile.velocity), projectile.bounceCount);
                e.message = b;
                _lg.write(e);
            }
        }
    }

    if (into < 0.0f)
    {
        // Stable bounce: separate normal and tangential components
        glm::vec3 tangent = projectile.velocity - bestNormal * into;
        float tangentRetention = std::clamp(1.0f - projectile.friction, 0.0f, 1.0f);
        glm::vec3 velBefore = projectile.velocity;
        float speedBefore = glm::length(velBefore);
        projectile.velocity = tangent * tangentRetention - bestNormal * into * projectile.restitution;
        float speedAfter = glm::length(projectile.velocity);

        projectile.angularVelocity *= 0.35f;
        projectile.worldTouched = true;
        ++projectile.bounceCount;

        {
            auto& _lg = ::StructuredLogger::instance();
            if (_lg.shouldLog(::StructuredCategory::GrenadeLauncher, ::StructuredLevel::Verbose)) {
                ::StructuredLogger::Entry e;
                e.category = ::StructuredCategory::GrenadeLauncher;
                e.level = ::StructuredLevel::Verbose;
                e.eventId = "GRENADE_SERVER_CONTACT_IMPACT";
                e.correlationId = "GRENADE_P" + std::to_string(projectile.ownerPlayerId)
                    + "_F" + std::to_string(projectile.fireSerial)
                    + "_J" + std::to_string(projectile.id);
                e.reason = "impact";
                char b[512]; std::snprintf(b, sizeof(b),
                    "bounceCount=%d speedBefore=%.3f speedAfter=%.3f "
                    "restitution=%.2f friction=%.2f tangentRetention=%.2f "
                    "into=%.3f normal=(%.3f,%.3f,%.3f)",
                    projectile.bounceCount, speedBefore, speedAfter,
                    projectile.restitution, projectile.friction, tangentRetention,
                    into, bestNormal.x, bestNormal.y, bestNormal.z);
                e.message = b;
                _lg.write(e);
            }
        }
    }
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
        uint8_t checkCount = std::min(shooter.recentProjectileSerialCount, (uint8_t)4);
        for (uint8_t i = 0; i < checkCount; ++i)
            if (shooter.recentProjectileSerials[i] == request->fireSerial)
            {
                serialNew = false;
                break;
            }
    }
    const uint8_t equippedWeapon = networkWeaponTypeForSlot(shooter.equippedSlot);
    const bool weaponEquippedValid = equippedWeapon == request->weapon;
    const bool cooldownValid = shooter.projectileFireCooldown <= 0.05f; // 50ms grace for network jitter
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
    {
        // Send rejection result for every structurally valid request from a known player
        ProjectileFireResultPacket reject{};
        reject.header.type = PACKET_PROJECTILE_FIRE_RESULT;
        reject.header.tick = tick;
        reject.header.playerId = shooter.id;
        reject.fireSerial = request->fireSerial;
        reject.weapon = request->weapon;
        reject.accepted = 0;
        reject.cooldownRemaining = shooter.projectileFireCooldown;

        if (shooter.dead) reject.reason = PROJECTILE_FIRE_DEAD;
        else if (!serialNew) reject.reason = PROJECTILE_FIRE_ALREADY_ACCEPTED;
        else if (!networkWeaponTypeIsProjectile(request->weapon)) reject.reason = PROJECTILE_FIRE_CONFIG_MISSING;
        else if (!weaponEquippedValid) reject.reason = PROJECTILE_FIRE_WEAPON_MISMATCH;
        else if (!cooldownValid) reject.reason = PROJECTILE_FIRE_COOLDOWN;
        else if (originDistance > 8.0f) reject.reason = PROJECTILE_FIRE_ORIGIN_INVALID;
        else reject.reason = PROJECTILE_FIRE_DIRECTION_INVALID;

        for (const auto& kv : players)
        {
            if (kv.first == shooter.id)
            {
                if (kv.second.transport)
                    kv.second.transport->send(&reject, sizeof(reject));
                else
                    sendto(sock, (const char*)&reject, sizeof(reject), 0,
                           (sockaddr*)&kv.second.addr, sizeof(kv.second.addr));
            }
        }
        {
            auto& _log = ::StructuredLogger::instance();
            if (_log.shouldLog(::StructuredCategory::GrenadeLauncher, ::StructuredLevel::Important)) {
                ::StructuredLogger::Entry e;
                e.category = ::StructuredCategory::GrenadeLauncher;
                e.level = ::StructuredLevel::Important;
                e.eventId = "GRENADE_SERVER_REQUEST";
                e.correlationId = "GRENADE_P" + std::to_string(shooter.id)
                    + "_F" + std::to_string(request->fireSerial) + "_J0";
                e.reason = "rejected";
                char buf[512]; std::snprintf(buf, sizeof(buf),
                    "playerId=%u fireSerial=%u weapon=%s serialNew=%d coordValid=%d weaponOk=%d "
                    "dirLen=%.2f originDist=%.2f cooldown=%.3f",
                    shooter.id, request->fireSerial, networkWeaponTypeName(request->weapon),
                    (int)serialNew, (int)cooldownValid, (int)weaponEquippedValid,
                    directionLength, originDistance, shooter.projectileFireCooldown);
                e.message = buf;
                _log.write(e);
            }
        }
        return;
    }

    const ProjectileConfig cfg = projectileConfig(request->weapon);
    const glm::vec3 dir = glm::normalize(direction);

    ServerProjectile projectile;
    projectile.id = nextProjectileId++;
    if (nextProjectileId == 0)
        nextProjectileId = 1;
    projectile.ownerPlayerId = shooter.id;
    projectile.fireSerial = request->fireSerial;
    projectile.weaponType = request->weapon;
    // Match client spawn position exactly (no 0.5f forward offset)
    projectile.position = origin;
    projectile.previousPosition = origin;
    projectile.velocity = dir * cfg.speed + glm::vec3(0.0f, 0.0f, cfg.upBias);
    projectile.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    // Deterministic angular velocity for grenades (no rand())
    if (request->weapon == NETWORK_WEAPON_GRENADE_LAUNCHER)
    {
        glm::vec3 forward = glm::length(dir) > 0.0001f ? dir : glm::vec3(1.0f, 0.0f, 0.0f);
        glm::vec3 refUp = std::fabs(forward.z) < 0.99f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
        glm::vec3 right = glm::normalize(glm::cross(forward, refUp));
        projectile.angularVelocity = right * cfg.angularSpeed;
    }
    else
    {
        projectile.angularVelocity = glm::vec3(0.0f);
    }
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
    projectile.armingTime = cfg.armingTime;
    projectile.minBounceSpeed = cfg.minBounceSpeed;
    projectile.angularDrag = cfg.angularDrag;
    projectile.maxBounceCount = cfg.maxBounceCount;
    projectile.spawnTick = tick;

    shooter.lastProjectileFireSerial = request->fireSerial;
    shooter.recentProjectileSerials[shooter.recentProjectileSerialCount % 4] = request->fireSerial;
    if (shooter.recentProjectileSerialCount < 255)
        shooter.recentProjectileSerialCount++;
    shooter.projectileFireCooldown = cfg.fireDelay;

    ProjectileSpawnEventPacket spawn{};
    spawn.header.type = PACKET_PROJECTILE_SPAWN_EVENT;
    spawn.header.tick = tick;
    fillProjectilePose(spawn, projectile);
    projectiles[projectile.id] = projectile;

    {
        auto& _lg = ::StructuredLogger::instance();
        if (_lg.shouldLog(::StructuredCategory::GrenadeLauncher, ::StructuredLevel::Important)) {
            ::StructuredLogger::Entry e;
            e.category = ::StructuredCategory::GrenadeLauncher;
            e.level = ::StructuredLevel::Important;
            e.eventId = "GRENADE_SERVER_REQUEST";
            e.correlationId = "GRENADE_P" + std::to_string(shooter.id)
                + "_F" + std::to_string(request->fireSerial)
                + "_J" + std::to_string(projectile.id);
            e.reason = "accepted";
            char b[512]; std::snprintf(b, sizeof(b),
                "projectileId=%u playerId=%u fireSerial=%u weapon=%s "
                "spawnPos=(%.2f,%.2f,%.2f) spawnVel=(%.2f,%.2f,%.2f) "
                "lifetime=%.1f radius=%.2f speed=%.1f gravity=%.1f drag=%.2f "
                "restitution=%.2f friction=%.2f upBias=%.1f "
                "angSpeed=%.1f angDrag=%.2f maxBounce=%d minBounceSpeed=%.1f armingDist=%.1f",
                projectile.id, shooter.id, request->fireSerial,
                networkWeaponTypeName(request->weapon),
                projectile.position.x, projectile.position.y, projectile.position.z,
                projectile.velocity.x, projectile.velocity.y, projectile.velocity.z,
                projectile.lifetime, projectile.radius, cfg.speed, projectile.gravity,
                projectile.drag, projectile.restitution, projectile.friction,
                cfg.upBias, cfg.angularSpeed, projectile.angularDrag,
                projectile.maxBounceCount, projectile.minBounceSpeed,
                projectile.armingDistance);
            e.message = b;
            _lg.write(e);
        }
    }

    {
        auto& _lg = ::StructuredLogger::instance();
        if (_lg.shouldLog(::StructuredCategory::GrenadeLauncher, ::StructuredLevel::Important)) {
            ::StructuredLogger::Entry e;
            e.category = ::StructuredCategory::GrenadeLauncher;
            e.level = ::StructuredLevel::Important;
            e.eventId = "GRENADE_SERVER_SPAWN";
            e.correlationId = "GRENADE_P" + std::to_string(shooter.id)
                + "_F" + std::to_string(request->fireSerial)
                + "_J" + std::to_string(projectile.id);
            e.reason = "spawn";
            char b[512]; std::snprintf(b, sizeof(b),
                "projectileId=%u playerId=%u fireSerial=%u weapon=%s "
                "pos=(%.2f,%.2f,%.2f) vel=(%.2f,%.2f,%.2f) speed=%.1f "
                "angVel=(%.2f,%.2f,%.2f) radius=%.2f gravity=%.1f drag=%.2f "
                "angDrag=%.2f restitution=%.2f friction=%.2f "
                "maxBounce=%d lifetime=%.1f armingDist=%.1f armingTime=%.1f "
                "upBias=%.1f minBounceSpeed=%.1f",
                projectile.id, shooter.id, request->fireSerial,
                networkWeaponTypeName(request->weapon),
                projectile.position.x, projectile.position.y, projectile.position.z,
                projectile.velocity.x, projectile.velocity.y, projectile.velocity.z,
                glm::length(projectile.velocity),
                projectile.angularVelocity.x, projectile.angularVelocity.y, projectile.angularVelocity.z,
                projectile.radius, projectile.gravity, projectile.drag,
                projectile.angularDrag, projectile.restitution, projectile.friction,
                projectile.maxBounceCount, projectile.lifetime,
                projectile.armingDistance, projectile.armingTime,
                cfg.upBias, projectile.minBounceSpeed);
            e.message = b;
            _lg.write(e);
        }
    }

    printf("%s [PROJECTILE SERVER SPAWN] projectileId=%u ownerPlayerId=%u "
           "weapon=%s position=(%.2f,%.2f,%.2f) velocity=(%.2f,%.2f,%.2f) "
           "spawnTick=%u lifetime=%.2f\n",
           serverTimestamp(), projectile.id, projectile.ownerPlayerId,
           networkWeaponTypeName(projectile.weaponType),
           projectile.position.x, projectile.position.y, projectile.position.z,
           projectile.velocity.x, projectile.velocity.y, projectile.velocity.z,
           projectile.spawnTick, projectile.lifetime);

    broadcastPacket(sock, players, spawn, totalPacketsOut);

    // Send fire result to the shooter
    ProjectileFireResultPacket result{};
    result.header.type = PACKET_PROJECTILE_FIRE_RESULT;
    result.header.tick = tick;
    result.header.playerId = shooter.id;
    result.fireSerial = request->fireSerial;
    result.projectileId = projectile.id;
    result.weapon = request->weapon;
    result.accepted = 1;
    result.reason = PROJECTILE_FIRE_ACCEPTED;
    result.cooldownRemaining = shooter.projectileFireCooldown;
    for (const auto& kv : players)
    {
        if (kv.first == shooter.id)
        {
            if (kv.second.transport)
                kv.second.transport->send(&result, sizeof(result));
            else
                sendto(sock, (const char*)&result, sizeof(result), 0,
                       (sockaddr*)&kv.second.addr, sizeof(kv.second.addr));
        }
    }
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
            // Adaptive substeps to prevent tunneling with small collision radius
            float speed = glm::length(projectile.velocity);
            float maxStep = std::max(projectile.radius * 0.5f, 0.1f);
            int steps = std::min((int)std::ceil(speed * dt / maxStep) + 1, 8);
            float subDt = dt / (float)steps;

            for (int s = 0; s < steps && !projectile.exploded; ++s)
            {
                projectile.velocity.z -= projectile.gravity * subDt;
                projectile.velocity *= std::max(0.0f, 1.0f - projectile.drag * subDt);
                glm::vec3 step = projectile.velocity * subDt;
                projectile.position += step;
                projectile.distanceTraveled += glm::length(step);

                // Apply angular drag to angular velocity only (never affects linear)
                float angSpeed = glm::length(projectile.angularVelocity);
                if (projectile.angularDrag > 0.0f && angSpeed > 0.0f)
                {
                    projectile.angularVelocity *= std::max(0.0f, 1.0f - projectile.angularDrag * subDt);
                }
                if (angSpeed > 0.0001f)
                {
                    float newAngSpeed = glm::length(projectile.angularVelocity);
                    if (newAngSpeed > 0.0001f)
                    {
                        glm::quat delta = glm::angleAxis(
                            newAngSpeed * subDt, glm::normalize(projectile.angularVelocity));
                        projectile.rotation = glm::normalize(delta * projectile.rotation);
                    }
                }

                // World collision per substep
                if (resolveGrenadeWorld(projectile, world))
                {
                    static int settleCallCount = 0;
                    if (projectile.bounceCount >= projectile.maxBounceCount &&
                        projectile.velocity != glm::vec3(0.0f))
                    {
                        // After max bounces: kill restitution, kill tangent, let it settle
                        float speedBefore = glm::length(projectile.velocity);
                        glm::vec3 velBefore = projectile.velocity;
                        float lateralSpeedBefore = glm::length(glm::vec3(velBefore.x, velBefore.y, 0.0f));
                        float intoComp = glm::dot(projectile.velocity, glm::vec3(0.0f, 0.0f, 1.0f));

                        if (speedBefore > 0.1f)
                        {
                            glm::vec3 lateral = projectile.velocity;
                            lateral.z = 0.0f;
                            projectile.velocity -= lateral * 0.95f;
                            if (projectile.velocity.z < -20.0f)
                                projectile.velocity.z = -20.0f;
                        }
                        else
                        {
                            projectile.velocity = glm::vec3(0.0f);
                            projectile.angularVelocity = glm::vec3(0.0f);
                        }

                        ++settleCallCount;
                        {
                            auto& _lg = ::StructuredLogger::instance();
                            if (_lg.shouldLog(::StructuredCategory::GrenadeLauncher, ::StructuredLevel::Verbose)) {
                                ::StructuredLogger::Entry e;
                                e.category = ::StructuredCategory::GrenadeLauncher;
                                e.level = ::StructuredLevel::Verbose;
                                e.eventId = "GRENADE_SERVER_SETTLE";
                                e.correlationId = "GRENADE_P" + std::to_string(projectile.ownerPlayerId)
                                    + "_F" + std::to_string(projectile.fireSerial)
                                    + "_J" + std::to_string(projectile.id);
                                e.reason = "max-bounce-settle";
                                char b[512]; std::snprintf(b, sizeof(b),
                                    "tick=%u bounceCount=%d maxBounce=%d "
                                    "speedBefore=%.2f speedAfter=%.2f "
                                    "lateralBefore=%.2f lateralAfter=%.2f "
                                    "downVelBefore=%.2f downVelAfter=%.2f "
                                    "settleCount=%d",
                                    tick, projectile.bounceCount, projectile.maxBounceCount,
                                    speedBefore, glm::length(projectile.velocity),
                                    lateralSpeedBefore,
                                    glm::length(glm::vec3(projectile.velocity.x, projectile.velocity.y, 0.0f)),
                                    intoComp, projectile.velocity.z,
                                    settleCallCount);
                                e.message = b;
                                _lg.write(e);
                            }
                        }
                    }
                }

                // Player collision per substep (skip owner during grace period)
                for (auto& entry : players)
                {
                    ServerPlayer& target = entry.second;
                    if (target.dead)
                        continue;
                    if (target.id == projectile.ownerPlayerId &&
                        (projectile.distanceTraveled < projectile.armingDistance))
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
        }

        // Rate-limited tick log
        if (projectile.weaponType == NETWORK_WEAPON_GRENADE_LAUNCHER && (tick % 15) == 0)
        {
            auto& _lg = ::StructuredLogger::instance();
            if (_lg.shouldLog(::StructuredCategory::GrenadeLauncher, ::StructuredLevel::Verbose)) {
                ::StructuredLogger::Entry e;
                e.category = ::StructuredCategory::GrenadeLauncher;
                e.level = ::StructuredLevel::Verbose;
                e.eventId = "GRENADE_SERVER_TICK";
                e.correlationId = "GRENADE_P" + std::to_string(projectile.ownerPlayerId)
                    + "_F" + std::to_string(projectile.fireSerial)
                    + "_J" + std::to_string(projectile.id);
                e.reason = "sim";
                char b[512]; std::snprintf(b, sizeof(b),
                    "tick=%u age=%.2f pos=(%.2f,%.2f,%.2f) vel=(%.2f,%.2f,%.2f) speed=%.2f "
                    "angVel=(%.2f,%.2f,%.2f) bounceCount=%d distanceTraveled=%.1f",
                    tick, projectile.age,
                    projectile.position.x, projectile.position.y, projectile.position.z,
                    projectile.velocity.x, projectile.velocity.y, projectile.velocity.z,
                    glm::length(projectile.velocity),
                    projectile.angularVelocity.x, projectile.angularVelocity.y, projectile.angularVelocity.z,
                    projectile.bounceCount, projectile.distanceTraveled);
                e.message = b;
                _lg.write(e);
            }
        }

        if (projectile.exploded)
        {
            {
                auto& _lg = ::StructuredLogger::instance();
                if (_lg.shouldLog(::StructuredCategory::GrenadeLauncher, ::StructuredLevel::Verbose)) {
                    ::StructuredLogger::Entry e;
                    e.category = ::StructuredCategory::GrenadeLauncher;
                    e.level = ::StructuredLevel::Verbose;
                    e.eventId = "GRENADE_SERVER_REMOVE";
                    e.correlationId = "GRENADE_P" + std::to_string(projectile.ownerPlayerId)
                        + "_F" + std::to_string(projectile.fireSerial)
                        + "_J" + std::to_string(projectile.id);
                    e.reason = "exploded";
                    char b[256]; std::snprintf(b, sizeof(b),
                        "tick=%u age=%.2f pos=(%.2f,%.2f,%.2f) vel=(%.2f,%.2f,%.2f) bounceCount=%d",
                        tick, projectile.age,
                        projectile.position.x, projectile.position.y, projectile.position.z,
                        projectile.velocity.x, projectile.velocity.y, projectile.velocity.z,
                        projectile.bounceCount);
                    e.message = b;
                    _lg.write(e);
                }
            }
            it = projectiles.erase(it);
            continue;
        }

        if (projectile.stateAccumulator >= 0.05f)
        {
            projectile.stateAccumulator = 0.0f;
            broadcastProjectileState(sock, players, projectile,
                                     tick, totalPacketsOut);
            {
                auto& _lg = ::StructuredLogger::instance();
                if (_lg.shouldLog(::StructuredCategory::GrenadeLauncher, ::StructuredLevel::Verbose)) {
                    ::StructuredLogger::Entry e;
                    e.category = ::StructuredCategory::GrenadeLauncher;
                    e.level = ::StructuredLevel::Verbose;
                    e.eventId = "GRENADE_SERVER_STATE_SEND";
                    e.correlationId = "GRENADE_P" + std::to_string(projectile.ownerPlayerId)
                        + "_F" + std::to_string(projectile.fireSerial)
                        + "_J" + std::to_string(projectile.id);
                    e.reason = "broadcast";
                    char b[256]; std::snprintf(b, sizeof(b),
                        "tick=%u pos=(%.2f,%.2f,%.2f) vel=(%.2f,%.2f,%.2f) age=%.2f players=%zu",
                        tick, projectile.position.x, projectile.position.y, projectile.position.z,
                        projectile.velocity.x, projectile.velocity.y, projectile.velocity.z,
                        projectile.age, players.size());
                    e.message = b;
                    _lg.write(e);
                }
            }
        }
        ++it;
    }
}

} // namespace MimitaNet
