// 07 19 2026, 09 29
/* purpose
* Authoritative server projectile spawn, simulation, damage, and replication.
* Validates projectile fire requests and owns server-created projectile state.
* Bridges server projectiles into the shared deterministic physics kernel.
* Does NOT implement client prediction, render interpolation, or input capture.
* Does NOT own weapon definitions, ammo data, or packet schema definitions.
* Does NOT create a separate local-play projectile simulation path.
*/

#include "network/server.h"
#include "network/network-weapons.h"
#include "debug/structured-log.h"

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstdio>
#include <optional>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "combat/projectile-simulation.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-types.h"
#include "physics/movement/physics-collision-shared.h"

namespace MimitaNet {
namespace {

// ── ProjectileConfig and helpers (anonymous, file-local) ─────────────

} // anonymous namespace

// ── Broadphase: gather candidate triangle indices intersecting an AABB ──
// (declared in server.h)
void gatherHeadlessTrianglesForAABB(
    const HeadlessWorld& world,
    const AABB& queryBounds,
    float expansion,
    std::vector<int>& out)
{
    if (world.collisionChunks.empty() || world.collisionChunkSize <= 0.001f)
    {
        // Fallback: scan all triangles (no acceleration structure)
        for (int i = 0; i < (int)world.triangles.size(); ++i)
        {
            AABB tb = makeTriangleAABB(world.triangles[i]);
            tb.min -= glm::vec3(expansion);
            tb.max += glm::vec3(expansion);
            if (overlaps(queryBounds, tb))
                out.push_back(i);
        }
        return;
    }

    glm::ivec3 c0 = collisionChunkCoord(queryBounds.min - glm::vec3(expansion), world.collisionChunkSize);
    glm::ivec3 c1 = collisionChunkCoord(queryBounds.max + glm::vec3(expansion), world.collisionChunkSize);

    // Clamp cell range
    constexpr int MAX_CELLS = 100;
    int64_t cellsX = (int64_t)c1.x - (int64_t)c0.x + 1;
    int64_t cellsY = (int64_t)c1.y - (int64_t)c0.y + 1;
    int64_t cellsZ = (int64_t)c1.z - (int64_t)c0.z + 1;
    if (cellsX > MAX_CELLS) { c1.x = c0.x + MAX_CELLS - 1; cellsX = MAX_CELLS; }
    if (cellsY > MAX_CELLS) { c1.y = c0.y + MAX_CELLS - 1; cellsY = MAX_CELLS; }
    if (cellsZ > MAX_CELLS) { c1.z = c0.z + MAX_CELLS - 1; cellsZ = MAX_CELLS; }

    // Thread-local dedup generation counter
    thread_local std::vector<uint32_t> s_gen;
    thread_local uint32_t s_curGen = 0;
    s_curGen++;
    if (s_curGen == 0) { s_gen.assign(world.triangles.size(), 0); s_curGen = 1; }
    if (s_gen.size() != world.triangles.size())
        s_gen.assign(world.triangles.size(), 0);

    for (int x = c0.x; x <= c1.x; ++x)
    for (int y = c0.y; y <= c1.y; ++y)
    for (int z = c0.z; z <= c1.z; ++z)
    {
        auto it = world.collisionChunks.find(glm::ivec3(x, y, z));
        if (it == world.collisionChunks.end())
            continue;

        for (int triIdx : it->second)
        {
            if (triIdx < 0 || triIdx >= (int)world.triangles.size())
                continue;
            if (s_gen[triIdx] == s_curGen)
                continue;
            s_gen[triIdx] = s_curGen;

            AABB tb = makeTriangleAABB(world.triangles[triIdx]);
            tb.min -= glm::vec3(expansion);
            tb.max += glm::vec3(expansion);
            if (overlaps(queryBounds, tb))
                out.push_back(triIdx);
        }
    }

    // Large triangles that exceeded per-chunk limit
    for (int triIdx : world.collisionLargeTriangles)
    {
        if (triIdx < 0 || triIdx >= (int)world.triangles.size())
            continue;
        if (s_gen[triIdx] == s_curGen)
            continue;
        s_gen[triIdx] = s_curGen;

        AABB tb = makeTriangleAABB(world.triangles[triIdx]);
        tb.min -= glm::vec3(expansion);
        tb.max += glm::vec3(expansion);
        if (overlaps(queryBounds, tb))
            out.push_back(triIdx);
    }
}

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

std::optional<ProjectileConfig> projectileConfig(uint8_t weapon)
{
    ProjectileConfig cfg;

    const char* weaponId = nullptr;
    if (weapon == NETWORK_WEAPON_ROCKET_LAUNCHER)
        weaponId = "rocket_launcher";
    else if (weapon == NETWORK_WEAPON_GRENADE_LAUNCHER)
        weaponId = "grenade_launcher";
    else
        return std::nullopt;

    const WeaponDefinition* def = WeaponRegistry::instance().get(weaponId);
    if (!def)
    {
        printf("[PROJECTILE CONFIG] weapon=%s NOT FOUND in registry (id=%s) — rejecting request\n",
               networkWeaponTypeName(weapon), weaponId);
        return std::nullopt;
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
    cfg.gravity = cp("gravity", 20.0f);
    cfg.drag = cp("drag", 0.15f);
    cfg.restitution = cp("bounceRestitution", 0.35f);
    cfg.friction = cp("bounceFriction", 0.5f);
    cfg.upBias = cp("upBias", 4.0f);
    cfg.armingDistance = cp("armingDistance", 2.0f);
    cfg.armingTime = cp("armingTime", 0.0f);
    cfg.maxBounceCount = (int)cp("maxBounceCount", 10.0f);
    cfg.minBounceSpeed = cp("minBounceSpeed", 0.1f);
    cfg.angularDrag = cp("angularDrag", 0.3f);
    cfg.angularSpeed = cp("angSpeed", 6.0f);

    // Validate essential config values; reject if critical fields are invalid
    bool valid = true;
    if (cfg.speed <= 0.0f || !std::isfinite(cfg.speed))
    { printf("[PROJECTILE CONFIG] weapon=%s INVALID speed=%.2f\n", networkWeaponTypeName(weapon), cfg.speed); valid = false; }
    if (cfg.radius <= 0.0f || !std::isfinite(cfg.radius))
    { printf("[PROJECTILE CONFIG] weapon=%s INVALID radius=%.2f\n", networkWeaponTypeName(weapon), cfg.radius); valid = false; }
    if (cfg.lifetime <= 0.0f || !std::isfinite(cfg.lifetime))
    { printf("[PROJECTILE CONFIG] weapon=%s INVALID lifetime=%.2f\n", networkWeaponTypeName(weapon), cfg.lifetime); valid = false; }
    if (!std::isfinite(cfg.gravity))
    { printf("[PROJECTILE CONFIG] weapon=%s INVALID gravity=%.2f\n", networkWeaponTypeName(weapon), cfg.gravity); valid = false; }
    if (!std::isfinite(cfg.drag))
    { printf("[PROJECTILE CONFIG] weapon=%s INVALID drag=%.2f\n", networkWeaponTypeName(weapon), cfg.drag); valid = false; }
    if (!std::isfinite(cfg.restitution))
    { printf("[PROJECTILE CONFIG] weapon=%s INVALID restitution=%.2f\n", networkWeaponTypeName(weapon), cfg.restitution); valid = false; }
    if (!std::isfinite(cfg.friction))
    { printf("[PROJECTILE CONFIG] weapon=%s INVALID friction=%.2f\n", networkWeaponTypeName(weapon), cfg.friction); valid = false; }
    if (!std::isfinite(cfg.splashRadius) || cfg.splashRadius <= 0.0f)
    { printf("[PROJECTILE CONFIG] weapon=%s INVALID splashRadius=%.2f\n", networkWeaponTypeName(weapon), cfg.splashRadius); valid = false; }

    if (!valid)
        return std::nullopt;

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

namespace {

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

class ServerProjectileWorldView final : public CollisionWorldView
{
public:
    ServerProjectileWorldView(const HeadlessWorld& world,
                              const std::unordered_map<uint32_t, ServerPlayer>& players,
                              uint32_t ownerPlayerId,
                              bool skipOwner)
        : mWorld(world),
          mPlayers(players),
          mOwnerPlayerId(ownerPlayerId),
          mSkipOwner(skipOwner)
    {
    }

    void queryTrianglesSwept(const glm::vec3& from,
                             const glm::vec3& to,
                             float radius,
                             std::vector<int>& outIndices) const override
    {
        AABB queryBounds;
        queryBounds.min = glm::min(from, to) - glm::vec3(radius);
        queryBounds.max = glm::max(from, to) + glm::vec3(radius);
        gatherHeadlessTrianglesForAABB(mWorld, queryBounds,
                                       radius * 0.1f, outIndices);
        std::sort(outIndices.begin(), outIndices.end());
        outIndices.erase(std::unique(outIndices.begin(), outIndices.end()),
                         outIndices.end());
    }

    const CollisionTriangle& triangleAt(int index) const override
    {
        return mWorld.triangles[(size_t)index];
    }

    int triangleCount() const override
    {
        return (int)mWorld.triangles.size();
    }

    void queryPlayerCapsulesSwept(const glm::vec3& from,
                                  const glm::vec3& to,
                                  float radius,
                                  std::vector<SweptPlayerCapsule>& out) const override
    {
        AABB projectileBounds;
        projectileBounds.min = glm::min(from, to) - glm::vec3(radius);
        projectileBounds.max = glm::max(from, to) + glm::vec3(radius);

        for (const auto& entry : mPlayers)
        {
            const ServerPlayer& player = entry.second;
            if (player.dead)
                continue;
            if (mSkipOwner && player.id == mOwnerPlayerId)
                continue;

            SweptPlayerCapsule cap;
            cap.playerId = player.id;
            cap.spawnGeneration = player.spawnGeneration;
            cap.a = player.pos + glm::vec3(0.0f, 0.0f,
                                           -PLAYER_HEIGHT * 0.5f + PLAYER_RADIUS);
            cap.b = player.pos + glm::vec3(0.0f, 0.0f,
                                           PLAYER_HEIGHT * 0.5f - PLAYER_RADIUS);
            cap.radius = PLAYER_RADIUS;

            AABB capsuleBounds;
            capsuleBounds.min = glm::min(cap.a, cap.b) - glm::vec3(cap.radius);
            capsuleBounds.max = glm::max(cap.a, cap.b) + glm::vec3(cap.radius);
            if (overlaps(projectileBounds, capsuleBounds))
                out.push_back(cap);
        }

        std::sort(out.begin(), out.end(),
                  [](const SweptPlayerCapsule& a, const SweptPlayerCapsule& b) {
                      if (a.playerId != b.playerId)
                          return a.playerId < b.playerId;
                      return a.spawnGeneration < b.spawnGeneration;
                  });
    }

private:
    const HeadlessWorld& mWorld;
    const std::unordered_map<uint32_t, ServerPlayer>& mPlayers;
    uint32_t mOwnerPlayerId = 0;
    bool mSkipOwner = false;
};

ProjectilePhysicsState makePhysicsState(const ServerProjectile& projectile)
{
    ProjectilePhysicsState state;
    state.position = projectile.position;
    state.velocity = projectile.velocity;
    state.rotation = projectile.rotation;
    state.angularVelocity = projectile.angularVelocity;
    state.age = projectile.age;
    state.bounceCount = projectile.bounceCount;
    state.exploded = projectile.exploded;
    state.sleeping =
        projectile.worldTouched &&
        glm::length(projectile.velocity) <= 0.0001f &&
        glm::length(projectile.angularVelocity) <= 0.0001f;
    return state;
}

ProjectilePhysicsConfig makePhysicsConfig(const ServerProjectile& projectile)
{
    ProjectilePhysicsConfig config;
    config.speed = glm::length(projectile.velocity);
    config.radius = projectile.radius;
    config.lifetime = projectile.lifetime;
    config.armingDistance = projectile.armingDistance;
    config.gravity = projectile.gravity;
    config.drag = projectile.drag;
    config.angularDrag = projectile.angularDrag;
    config.restitution = projectile.restitution;
    config.friction = projectile.friction;
    config.maxBounceCount = projectile.maxBounceCount;
    config.minBounceSpeed = projectile.minBounceSpeed;
    config.bounceEnabled = projectile.maxBounceCount > 0;
    return config;
}

void applyPhysicsState(ServerProjectile& projectile,
                       const ProjectilePhysicsState& state)
{
    projectile.position = state.position;
    projectile.velocity = state.velocity;
    projectile.rotation = state.rotation;
    projectile.angularVelocity = state.angularVelocity;
    projectile.age = state.age;
    projectile.bounceCount = state.bounceCount;
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

    // ── Cache lookup: if this fireSerial was already processed, resend ──
    for (uint8_t ci = 0; ci < ServerPlayer::MAX_CACHED_FIRE_RESULTS; ++ci)
    {
        const auto& cached = shooter.cachedFireResults[ci];
        if (!cached.valid || cached.fireSerial != request->fireSerial)
            continue;

        ProjectileFireResultPacket cachedResult{};
        cachedResult.header.type = PACKET_PROJECTILE_FIRE_RESULT;
        cachedResult.header.tick = tick;
        cachedResult.header.playerId = shooter.id;
        cachedResult.fireSerial = cached.fireSerial;
        cachedResult.projectileId = cached.projectileId;
        cachedResult.weapon = cached.weapon;
        cachedResult.accepted = cached.accepted ? 1 : 0;
        cachedResult.reason = cached.reason;
        cachedResult.cooldownRemaining = cached.cooldownRemaining;
        for (const auto& kv : players)
        {
            if (kv.first == shooter.id)
            {
                if (kv.second.transport) kv.second.transport->send(&cachedResult, sizeof(cachedResult));
                else sendto(sock, (const char*)&cachedResult, sizeof(cachedResult), 0,
                            (sockaddr*)&kv.second.addr, sizeof(kv.second.addr));
            }
        }
        // If accepted and the projectile still exists, resend the spawn to this client
        if (cached.accepted && cached.projectileId != 0)
        {
            auto projIt = projectiles.find(cached.projectileId);
            if (projIt != projectiles.end())
            {
                ProjectileSpawnEventPacket spawn{};
                spawn.header.type = PACKET_PROJECTILE_SPAWN_EVENT;
                spawn.header.tick = tick;
                fillProjectilePose(spawn, projIt->second);
                serverSendToPlayer(sock, shooter, &spawn, sizeof(spawn));
            }
        }
        return;
    }

    const glm::vec3 origin(request->originX, request->originY, request->originZ);
    const glm::vec3 direction(request->dirX, request->dirY, request->dirZ);
    const float originDistance = finiteVec(origin)
        ? glm::length(origin - shooter.pos)
        : 99999.0f;
    const float directionLength = finiteVec(direction)
        ? glm::length(direction)
        : 0.0f;
    const bool serialNew = request->fireSerial != 0;

    auto cacheResult = [&](bool accepted, uint32_t projId, uint8_t reason, float cooldown) {
        auto& slot = shooter.cachedFireResults[shooter.nextCachedFireResultSlot];
        slot.fireSerial = request->fireSerial;
        slot.accepted = accepted;
        slot.projectileId = projId;
        slot.weapon = request->weapon;
        slot.reason = reason;
        slot.cooldownRemaining = cooldown;
        slot.valid = true;
        shooter.nextCachedFireResultSlot = (shooter.nextCachedFireResultSlot + 1) % ServerPlayer::MAX_CACHED_FIRE_RESULTS;
    };

    // ── Authoritative weapon runtime ──────────────────────────────────
    // Lazy-init the runtime from WeaponDefinition on first use
    const char* wepId = (request->weapon == NETWORK_WEAPON_GRENADE_LAUNCHER) ? "grenade_launcher"
                      : (request->weapon == NETWORK_WEAPON_ROCKET_LAUNCHER) ? "rocket_launcher"
                      : nullptr;
    ServerPlayer::ServerWeaponRuntime* runtime = nullptr;
    if (wepId)
    {
        auto rtIt = shooter.weaponRuntimes.find(wepId);
        if (rtIt == shooter.weaponRuntimes.end())
        {
            // Initialize from WeaponDefinition
            const WeaponDefinition* def = WeaponRegistry::instance().get(wepId);
            if (def)
            {
                ServerPlayer::ServerWeaponRuntime rt;
                rt.magazineAmmo = def->magazineSize;
                rt.reserveAmmo = (int)def->customParams.count("reserveAmmo")
                    ? (int)def->customParams.at("reserveAmmo") : 1337;
                rt.nextAllowedFireTick = 0;
                rt.initialized = true;
                shooter.weaponRuntimes[wepId] = rt;
                rtIt = shooter.weaponRuntimes.find(wepId);
            }
        }
        if (rtIt != shooter.weaponRuntimes.end())
            runtime = &rtIt->second;
    }

    const bool hasAmmo = runtime ? runtime->magazineAmmo > 0 : true;

    // Tick-based cooldown validation (replaces float cooldown)
    constexpr uint64_t COOLDOWN_GRACE_TICKS = 2;
    const uint64_t currentTick = tick;
    const bool cooldownValid = currentTick + COOLDOWN_GRACE_TICKS >= shooter.nextProjectileFireTick;
    const uint64_t remainingCooldownTicks = shooter.nextProjectileFireTick > currentTick
        ? shooter.nextProjectileFireTick - currentTick : 0;

    const bool directionValid = directionLength >= 0.5f && directionLength <= 1.5f;
    const bool accepted =
        !shooter.dead &&
        serialNew &&
        networkWeaponTypeIsProjectile(request->weapon) &&
        hasAmmo &&
        cooldownValid &&
        originDistance <= 8.0f &&
        directionValid;

    int serverAmmo = runtime ? runtime->magazineAmmo : -1;
    printf("%s [PROJECTILE FIRE REQUEST RX] playerId=%u fireSerial=%u "
           "weapon=%s originDistance=%.2f directionLength=%.2f "
           "serverTick=%u nextAllowedTick=%llu remainingTicks=%llu "
           "hasAmmo=%d serverAmmo=%d "
           "cooldownValid=%d accepted=%d reason=%s\n",
           serverTimestamp(), shooter.id, request->fireSerial,
           networkWeaponTypeName(request->weapon), originDistance,
           directionLength, currentTick,
           (unsigned long long)shooter.nextProjectileFireTick,
           (unsigned long long)remainingCooldownTicks,
           (int)hasAmmo, serverAmmo,
           (int)cooldownValid,
           (int)accepted,
           accepted ? "accepted" :
           shooter.dead ? "dead" :
           !serialNew ? "zero-serial" :
           !hasAmmo ? "out-of-ammo" :
           !networkWeaponTypeIsProjectile(request->weapon) ? "not-projectile-weapon" :
           !cooldownValid ? "cooldown" :
           originDistance > 8.0f ? "origin-too-far" : "invalid-direction");

    if (!accepted)
    {
        ProjectileFireResultPacket reject{};
        reject.header.type = PACKET_PROJECTILE_FIRE_RESULT;
        reject.header.tick = tick;
        reject.header.playerId = shooter.id;
        reject.fireSerial = request->fireSerial;
        reject.weapon = request->weapon;
        reject.accepted = 0;
        reject.cooldownRemaining = (float)remainingCooldownTicks / 60.0f;

        if (shooter.dead) reject.reason = PROJECTILE_FIRE_DEAD;
        else if (!serialNew) reject.reason = PROJECTILE_FIRE_ALREADY_ACCEPTED;
        else if (!networkWeaponTypeIsProjectile(request->weapon)) reject.reason = PROJECTILE_FIRE_CONFIG_MISSING;
        else if (!hasAmmo) reject.reason = PROJECTILE_FIRE_DEAD; // reuse DEAD as out-of-ammo for now
        else if (!cooldownValid) reject.reason = PROJECTILE_FIRE_COOLDOWN;
        else if (originDistance > 8.0f) reject.reason = PROJECTILE_FIRE_ORIGIN_INVALID;
        else reject.reason = PROJECTILE_FIRE_DIRECTION_INVALID;

        cacheResult(false, 0, reject.reason, reject.cooldownRemaining);

        serverSendToPlayer(sock, shooter, &reject, sizeof(reject));
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
                    "playerId=%u fireSerial=%u weapon=%s serialNew=%d "
                    "dirLen=%.2f originDist=%.2f cooldownRemainingTicks=%llu",
                    shooter.id, request->fireSerial, networkWeaponTypeName(request->weapon),
                    (int)serialNew,
                    directionLength, originDistance,
                    (unsigned long long)remainingCooldownTicks);
                e.message = buf;
                _log.write(e);
            }
        }
        return;
    }

    auto cfgOpt = projectileConfig(request->weapon);
    if (!cfgOpt)
    {
        // Config missing or invalid — reject without consuming state
        ProjectileFireResultPacket reject{};
        reject.header.type = PACKET_PROJECTILE_FIRE_RESULT;
        reject.header.tick = tick;
        reject.header.playerId = shooter.id;
        reject.fireSerial = request->fireSerial;
        reject.weapon = request->weapon;
        reject.accepted = 0;
        reject.reason = PROJECTILE_FIRE_CONFIG_MISSING;
        reject.cooldownRemaining = shooter.projectileFireCooldown;
        cacheResult(false, 0, PROJECTILE_FIRE_CONFIG_MISSING, shooter.projectileFireCooldown);
        serverSendToPlayer(sock, shooter, &reject, sizeof(reject));
        return;
    }
    const ProjectileConfig& cfg = *cfgOpt;
    // Update equipped slot from weapon type to handle equip-input/fire-request race
    if (networkWeaponTypeIsProjectile(request->weapon))
    {
        int slotForWeapon = slotForNetworkWeaponType(request->weapon);
        if (slotForWeapon > 0)
            shooter.equippedSlot = slotForWeapon;
    }
    const glm::vec3 dir = glm::normalize(direction);

    ServerProjectile projectile;
    projectile.id = nextProjectileId++;
    if (nextProjectileId == 0)
        nextProjectileId = 1;
    projectile.ownerPlayerId = shooter.id;
    projectile.fireSerial = request->fireSerial;
    projectile.weaponType = request->weapon;
    projectile.position = origin;
    projectile.previousPosition = origin;
    projectile.velocity = dir * cfg.speed + glm::vec3(0.0f, 0.0f, cfg.upBias);
    projectile.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
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
    // Tick-based cooldown: ceil(fireDelay * 60) ticks
    uint32_t cooldownTicks = (uint32_t)std::ceil(cfg.fireDelay * 60.0f);
    shooter.nextProjectileFireTick = (uint64_t)tick + cooldownTicks;
    shooter.projectileFireCooldown = cfg.fireDelay; // kept for legacy snapshot serialization

    // Authoritative ammo consumption
    if (runtime)
    {
        if (runtime->magazineAmmo > 0)
            --runtime->magazineAmmo;
        printf("[SERVER WEAPON RUNTIME] playerId=%u weapon=%s magazineAmmo=%d/%d\n",
               shooter.id, wepId ? wepId : "?", runtime->magazineAmmo,
               runtime->magazineAmmo + (int)(runtime->reserveAmmo > 0));
    }

    ProjectileSpawnEventPacket spawn{};
    spawn.header.type = PACKET_PROJECTILE_SPAWN_EVENT;
    spawn.header.tick = tick;
    fillProjectilePose(spawn, projectile);
    projectiles[projectile.id] = projectile;

    // Cache the accepted result BEFORE sending so retries can be answered idempotently
    cacheResult(true, projectile.id, PROJECTILE_FIRE_ACCEPTED, shooter.projectileFireCooldown);

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
    serverSendToPlayer(sock, shooter, &result, sizeof(result));
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
        projectile.stateAccumulator += dt;

        const bool sharedProjectile =
            projectile.weaponType == NETWORK_WEAPON_ROCKET_LAUNCHER ||
            projectile.weaponType == NETWORK_WEAPON_GRENADE_LAUNCHER;

        if (sharedProjectile)
        {
            const int previousBounceCount = projectile.bounceCount;
            ProjectilePhysicsState state = makePhysicsState(projectile);
            ProjectilePhysicsConfig config = makePhysicsConfig(projectile);
            ServerProjectileWorldView physicsWorld(
                world, players, projectile.ownerPlayerId,
                projectile.distanceTraveled < projectile.armingDistance);

            ProjectileStepResult step =
                simulateProjectileTick(state, config, physicsWorld, dt);
            applyPhysicsState(projectile, state);
            projectile.distanceTraveled += step.travelDistance;
            if (state.sleeping || state.bounceCount > previousBounceCount ||
                step.type == ProjectileCollisionType::WorldBounce ||
                step.type == ProjectileCollisionType::WorldImpact)
            {
                projectile.worldTouched = true;
            }

            if (step.type == ProjectileCollisionType::LifetimeExpired)
            {
                explodeProjectile(sock, players, projectile, projectile.position,
                                  "lifetime", 0, tick, totalPacketsOut);
            }
            else if (step.type == ProjectileCollisionType::PlayerImpact)
            {
                explodeProjectile(sock, players, projectile, step.hitPosition,
                                  "player", step.hitPlayerId, tick,
                                  totalPacketsOut);
            }
            else if (step.type == ProjectileCollisionType::WorldImpact &&
                     projectile.weaponType == NETWORK_WEAPON_ROCKET_LAUNCHER)
            {
                explodeProjectile(sock, players, projectile, step.hitPosition,
                                  "world", 0, tick, totalPacketsOut);
            }

            if (!projectile.exploded &&
                projectile.weaponType == NETWORK_WEAPON_ROCKET_LAUNCHER &&
                glm::length(projectile.velocity) > 0.001f)
            {
                projectile.rotation = glm::rotation(
                    glm::vec3(0.0f, 0.0f, 1.0f),
                    glm::normalize(projectile.velocity));
            }
        }
        else
        {
            projectile.age += dt;
            if (projectile.age >= projectile.lifetime)
            {
                explodeProjectile(sock, players, projectile, projectile.position,
                                  "lifetime", 0, tick, totalPacketsOut);
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
