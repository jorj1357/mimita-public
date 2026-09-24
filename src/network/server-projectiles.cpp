// 08 31 2026, 17 14
/* purpose
* Authoritative server projectile spawn, simulation, damage, and replication.
* Validates projectile fire requests and owns server-created projectile state.
* Bridges server projectiles into the shared deterministic physics kernel.
* Does NOT implement client prediction, render interpolation, or input capture.
* Does NOT own weapon definitions, ammo data, or packet schema definitions.
* Does NOT create a separate local-play projectile simulation path.
*/

#include "network/server.h"
#include "live-code/live-journal.h"
#include "network/server-gamemode.h"
#include "network/network-weapons.h"
#include "debug/structured-log.h"
#include "debug/debug-log.h"
#include "persistence/persistence-emit.h"
#include "ecs/actor-entities.h"
#include "ecs/entity-registry.h"
#include "network/server-context.h"
#include "live-code/live-behavior.h"
#include "live-code/live-gameplay.h"
#include "network/server-damage-policy.h"
#include "hot-reload/generic-runtime.h"
#include "hot-reload/hot-projectile-splash.h"
#include "hot-reload/hot-projectile-cancel.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cfloat>
#include <cstdio>
#include <optional>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "combat/projectile-simulation.h"
#include "combat/area-effect.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-runtime.h"
#include "combat/weapon-types.h"
#include "physics/movement/movement-step.h"
#include "physics/movement/physics-collision-shared.h"

namespace MimitaNet {
namespace {

// Resolve the active generation's projectile splash policy through the one
// generic doorway. Never cached across a generation swap. Null when none.
static const GameProjectileSplashPolicyV1* hotProjectileSplashPolicy()
{
    void* callable = MimitaRuntime::GenericRuntime::instance().capability(
        GAME_CAP_PROJECTILE_SPLASH);
    if (!callable)
        return nullptr;
    auto lookup = reinterpret_cast<GameProjectileSplashLookupFn>(callable);
    return lookup ? lookup(nullptr) : nullptr;
}

// Resolve the active generation's projectile cancellation policy. Null when no
// hot provider is registered (the shared fallback runs).
static GameProjectileCancelFn hotProjectileCancelPolicy()
{
    void* callable = MimitaRuntime::GenericRuntime::instance().capability(
        GAME_CAP_PROJECTILE_CANCEL);
    return callable ? reinterpret_cast<GameProjectileCancelFn>(callable) : nullptr;
}

ServerProjectilePerfStats gProjectilePerf;

// ── ProjectileConfig and helpers (anonymous, file-local) ─────────────

bool projectileIsSleeping(const ServerProjectile& projectile)
{
    return projectile.worldTouched &&
        glm::length(projectile.velocity) <= 0.0001f &&
        glm::length(projectile.angularVelocity) <= 0.0001f;
}

} // anonymous namespace

ServerProjectilePerfStats consumeServerProjectilePerfStats()
{
    ServerProjectilePerfStats out = gProjectilePerf;
    const uint32_t active = gProjectilePerf.activeProjectiles;
    const uint32_t moving = gProjectilePerf.movingProjectiles;
    const uint32_t sleeping = gProjectilePerf.sleepingProjectiles;
    gProjectilePerf = ServerProjectilePerfStats{};
    gProjectilePerf.activeProjectiles = active;
    gProjectilePerf.movingProjectiles = moving;
    gProjectilePerf.sleepingProjectiles = sleeping;
    return out;
}

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

    // Expand by `expansion` for sub-cell selection so triangles in the overlap
    // filter's expanded zone are never missed.
    AABB subQuery = queryBounds;
    subQuery.min -= glm::vec3(expansion);
    subQuery.max += glm::vec3(expansion);

    auto visitTri = [&](int triIdx) {
        if (triIdx < 0 || triIdx >= (int)world.triangles.size())
            return;
        if (s_gen[triIdx] == s_curGen)
            return;
        s_gen[triIdx] = s_curGen;

        AABB tb = makeTriangleAABB(world.triangles[triIdx]);
        tb.min -= glm::vec3(expansion);
        tb.max += glm::vec3(expansion);
        if (overlaps(queryBounds, tb))
            out.push_back(triIdx);
    };

    for (int x = c0.x; x <= c1.x; ++x)
    for (int y = c0.y; y <= c1.y; ++y)
    for (int z = c0.z; z <= c1.z; ++z)
    {
        glm::ivec3 chunkCoord(x, y, z);
        auto it = world.collisionChunks.find(chunkCoord);
        if (it == world.collisionChunks.end())
            continue;

        auto subIt = world.collisionSubGrids.find(chunkCoord);
        if (subIt != world.collisionSubGrids.end() && subIt->second.subSize > 0.001f)
        {
            const float cs = world.collisionChunkSize;
            const float subSize = subIt->second.subSize;
            const int subdiv = std::max(1, (int)std::floor(cs / subSize + 0.5f));
            const glm::vec3 chunkMin((float)chunkCoord.x * cs, (float)chunkCoord.y * cs, (float)chunkCoord.z * cs);
            glm::ivec3 s0((int)std::floor((subQuery.min.x - chunkMin.x) / subSize),
                          (int)std::floor((subQuery.min.y - chunkMin.y) / subSize),
                          (int)std::floor((subQuery.min.z - chunkMin.z) / subSize));
            glm::ivec3 s1((int)std::floor((subQuery.max.x - chunkMin.x) / subSize),
                          (int)std::floor((subQuery.max.y - chunkMin.y) / subSize),
                          (int)std::floor((subQuery.max.z - chunkMin.z) / subSize));
            s0 = glm::clamp(s0, glm::ivec3(0), glm::ivec3(subdiv - 1));
            s1 = glm::clamp(s1, glm::ivec3(0), glm::ivec3(subdiv - 1));
            for (int sx = s0.x; sx <= s1.x; ++sx)
            for (int sy = s0.y; sy <= s1.y; ++sy)
            for (int sz = s0.z; sz <= s1.z; ++sz)
            {
                auto scIt = subIt->second.cells.find(glm::ivec3(sx, sy, sz));
                if (scIt == subIt->second.cells.end())
                    continue;
                for (int triIdx : scIt->second)
                    visitTri(triIdx);
            }
        }
        else
        {
            for (int triIdx : it->second)
                visitTri(triIdx);
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

// ── LEGACY (kept in place, not deleted; unreachable from the live server) ──
// The legacy kernel-container projectile policy below (ProjectileConfig,
// projectileConfig*, finiteVec, playerDamageCenter, ServerProjectileWorldView,
// explodeProjectile, tickServerProjectiles) has no live caller: the canonical
// authoritative projectile path is the hot `projectiles.60` system
// (src/hot-reload/modules/tools/hot-projectiles.cpp). Kept as a reference
// fallback per the no-deletion rule; policy here must not be extended.
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
    float selfDamageMultiplier = 1.0f;
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
    bool splashLineOfSight = true;
    float fullDamageRadius = 0.0f;
    float edgeDamage = 0.0f;
    float onExpireEffect = 0.0f; // 0=explosion, 1=smoke, 2=fire
    bool inheritOwnerVelocity = false;
};

std::optional<ProjectileConfig> projectileConfigFromDefinition(
    const WeaponDefinition& def,
    uint8_t weapon)
{
    ProjectileConfig cfg;
    cfg.speed = def.projectileSpeed > 0.0f ? def.projectileSpeed : 40.0f;
    cfg.lifetime = def.projectileLifetime > 0.0f ? def.projectileLifetime : 5.0f;
    cfg.radius = def.projectileRadius > 0.0f ? def.projectileRadius : 0.3f;
    cfg.fireDelay = def.fireDelay > 0.0f ? def.fireDelay : 1.0f;

    auto cp = [&](const char* key, float fallback) -> float {
        auto it = def.customParams.find(key);
        return it != def.customParams.end() ? it->second : fallback;
    };

    cfg.splashRadius = cp("splashRadius", 8.0f);
    cfg.splashDamage = cp("rocketDirectDamage", 150.0f);
    cfg.splashExponent = cp("splashExponent", 2.0f);
    cfg.knockbackStrength = cp("knockbackStrength", 160.0f);
    cfg.selfKnockbackMultiplier = cp("selfKnockbackMultiplier", 1.0f);
    cfg.selfDamageMultiplier = std::max(0.0f, cp("selfDamageMultiplier", 1.0f));
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
    cfg.splashLineOfSight = cp("splashLineOfSight", 1.0f) > 0.0f;
    cfg.fullDamageRadius = cp("full_damage_radius", 0.0f);
    cfg.edgeDamage = cp("edge_damage", 0.0f);
    cfg.onExpireEffect = cp("on_expire_effect", 0.0f);
    cfg.inheritOwnerVelocity = cp("inherit_owner_velocity", 0.0f) > 0.0f;
    // Thrown grenades author their fuse in server ticks; convert once to the
    // shared kernel's seconds so client prediction matches exactly at 60 Hz.
    const float fuseTicks = cp("fuse_ticks", 0.0f);
    if (fuseTicks > 0.0f)
        cfg.lifetime = fuseTicks / 60.0f;

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
           "angularDrag=%.2f source=weapon-definition\n",
           networkWeaponTypeName(weapon),
           cfg.speed, cfg.radius, cfg.fireDelay,
           cfg.lifetime, cfg.splashRadius, cfg.splashDamage, cfg.gravity,
           cfg.drag, cfg.restitution, cfg.friction, cfg.upBias,
           cfg.armingDistance, cfg.maxBounceCount, cfg.minBounceSpeed,
           cfg.angularDrag);
    return cfg;
}

std::optional<ProjectileConfig> projectileConfig(uint8_t weapon)
{
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

    return projectileConfigFromDefinition(*def, weapon);
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
                     uint64_t& totalPacketsOut,
                     uint32_t exceptPlayerId = 0)
{
    for (const auto& playerEntry : players)
    {
        if (exceptPlayerId != 0 && playerEntry.first == exceptPlayerId)
            continue; // skip the shooter so it never sees the server's copy of its own projectile
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
    packet.weaponDefNetworkId = projectile.weaponDefNetworkId;
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
    packet.weaponDefNetworkId = projectile.weaponDefNetworkId;
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
    packet.ownerPlayerId = projectile.ownerPlayerId;
    packet.fireSerial = projectile.fireSerial;
    gProjectilePerf.correctionPackets += players.size();
    gProjectilePerf.correctionBytes += players.size() * sizeof(packet);
    // Never send the server's state of a projectile back to its OWNER — the
    // shooter's client simulates its own rocket/grenade, so receiving the
    // server copy here is what caused the "two rockets" duplicate on badconn.
    broadcastPacket(sock, players, packet, totalPacketsOut, projectile.ownerPlayerId);
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
                              const std::unordered_map<uint32_t, ServerNpc>& npcs,
                              uint32_t ownerPlayerId,
                              uint32_t ownerNpcId,
                              bool skipOwner,
                              uint32_t targetTick = 0,
                              int ownerTeam = -1)
        : mWorld(world),
          mPlayers(players),
          mNpcs(npcs),
          mOwnerPlayerId(ownerPlayerId),
          mOwnerNpcId(ownerNpcId),
          mSkipOwner(skipOwner),
          mTargetTick(targetTick),
          mOwnerTeam(ownerTeam)
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
            // Team-based friendly fire filtering: skip friendly players
            if (mOwnerTeam >= 0 && player.matchTeam >= 0 && mOwnerTeam == player.matchTeam)
                continue;

            SweptPlayerCapsule cap;
            cap.playerId = player.id;
            cap.spawnGeneration = player.spawnGeneration;
            glm::vec3 targetPos = player.pos;
            if (mTargetTick != 0)
                getPositionAtTick(player, mTargetTick, targetPos);
            cap.a = targetPos + glm::vec3(0.0f, 0.0f,
                                           -PLAYER_HEIGHT * 0.5f + PLAYER_RADIUS);
            cap.b = targetPos + glm::vec3(0.0f, 0.0f,
                                           PLAYER_HEIGHT * 0.5f - PLAYER_RADIUS);
            cap.radius = PLAYER_RADIUS;

            AABB capsuleBounds;
            capsuleBounds.min = glm::min(cap.a, cap.b) - glm::vec3(cap.radius);
            capsuleBounds.max = glm::max(cap.a, cap.b) + glm::vec3(cap.radius);
            if (overlaps(projectileBounds, capsuleBounds))
                out.push_back(cap);
        }

        for (const auto& entry : mNpcs)
        {
            const ServerNpc& npc = entry.second;
            if (npc.health <= 0)
                continue;
            if (mSkipOwner && npc.entityId == mOwnerNpcId)
                continue;

            SweptPlayerCapsule cap;
            cap.playerId = npc.entityId;
            cap.spawnGeneration = 0;
            glm::vec3 targetPos = npc.pos;
            if (mTargetTick != 0)
                getNpcPositionAtTick(npc, mTargetTick, targetPos);
            cap.a = targetPos + glm::vec3(0.0f, 0.0f,
                                        -PLAYER_HEIGHT * 0.5f + PLAYER_RADIUS);
            cap.b = targetPos + glm::vec3(0.0f, 0.0f,
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
    const std::unordered_map<uint32_t, ServerNpc>& mNpcs;
    uint32_t mOwnerPlayerId = 0;
    uint32_t mOwnerNpcId = 0;
    bool mSkipOwner = false;
    uint32_t mTargetTick = 0;
    int mOwnerTeam = -1;  // -1 = no team, 0 = red, 1 = blue
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
    state.sleeping = projectileIsSleeping(projectile);
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

// Splash line-of-sight: the blast reaches the victim unless a non-floor surface
// (wall/column/cover) lies between the explosion center and the victim's nearest
// body part. Floors/ceilings (|normal.z| > 0.7) never block, so a grenade on the
// ground still splashes someone standing next to it. Uses the shared kernel
// helpers from projectile-simulation.h (identical rule on the client).
bool splashHasLineOfSight(const HeadlessWorld& world,
                          const glm::vec3& explosionPos,
                          const glm::vec3& victimPoint)
{
    const glm::vec3 delta = victimPoint - explosionPos;
    const float maxDist = glm::length(delta);
    if (maxDist < 0.75f)
        return true;
    const glm::vec3 dir = delta / maxDist;

    AABB rayBounds;
    rayBounds.min = glm::min(explosionPos, victimPoint);
    rayBounds.max = glm::max(explosionPos, victimPoint);
    std::vector<int> candidates;
    gatherHeadlessTrianglesForAABB(world, rayBounds, 0.05f, candidates);

    return !splashRayBlockedByWall(explosionPos, dir, maxDist, candidates, world.triangles);
}

// Reconstruct the victim's REAL body-part boxes (head/arms/legs/torso) at their
// current pose — same template + yaw math the server's hit-rewind uses, never a
// capsule. Writes the nearest body-part point to the blast into outPoint.
// Falls back to the torso center when no template is available.
bool splashNearestPlayerBodyPoint(const ServerPlayer& victim,
                                  const glm::vec3& blast,
                                  glm::vec3& outPoint,
                                  const glm::vec3* historicalPos = nullptr,
                                  const float* historicalYaw = nullptr)
{
    SplashBodyPartBox boxes[8];
    int count = 0;
    if (const auto* tpl = standardPlayerBodyTemplate())
    {
        const glm::vec3 basePos = historicalPos ? *historicalPos : victim.pos;
        const float yaw = historicalYaw ? *historicalYaw : victim.yaw;
        const float c = std::cos(yaw);
        const float s = std::sin(yaw);
        for (const auto& t : *tpl)
        {
            if (count >= 8)
                break;
            const glm::vec3 off(t.offset.x * c - t.offset.y * s,
                                t.offset.x * s + t.offset.y * c,
                                t.offset.z);
            boxes[count].center = basePos + off;
            boxes[count].half = t.half;
            ++count;
        }
    }
    if (count == 0)
    {
        outPoint = (historicalPos ? *historicalPos : victim.pos) +
            glm::vec3(0.0f, 0.0f, PLAYER_HEIGHT * 0.25f);
        return true;
    }
    return splashNearestBodyPartPoint(blast, boxes, count, outPoint);
}

bool splashNearestNpcBodyPoint(const ServerNpc& npc,
                               const glm::vec3& blast,
                               glm::vec3& outPoint,
                               uint32_t historicalTick = 0)
{
    SplashBodyPartBox boxes[8];
    int count = 0;
    if (historicalTick != 0)
    {
        const ServerNpcBodyPartSample* parts = nullptr;
        uint8_t partCount = 0;
        if (getNpcBodyPartsAtTick(npc, historicalTick, &parts, &partCount))
        {
            SplashBodyPartBox boxes[8];
            const int count = std::min<int>(partCount, 8);
            for (int i = 0; i < count; ++i)
            {
                boxes[i].center = {parts[i].cx, parts[i].cy, parts[i].cz};
                boxes[i].half = {parts[i].hx, parts[i].hy, parts[i].hz};
            }
            if (count > 0)
                return splashNearestBodyPartPoint(blast, boxes, count, outPoint);
        }
    }
    for (uint8_t i = 0; i < npc.bodyPartCount && i < npc.bodyParts.size(); ++i)
    {
        const ServerNpcBodyPartSample& s = npc.bodyParts[i];
        boxes[count].center = {s.cx, s.cy, s.cz};
        boxes[count].half = {s.hx, s.hy, s.hz};
        ++count;
    }
    if (count == 0)
    {
        outPoint = npc.pos + glm::vec3(0.0f, 0.0f, PLAYER_HEIGHT * 0.25f);
        return true;
    }
    return splashNearestBodyPartPoint(blast, boxes, count, outPoint);
}

void explodeProjectile(SOCKET sock,
                       const HeadlessWorld& world,
                       std::unordered_map<uint32_t, ServerPlayer>& players,
                       std::unordered_map<uint32_t, ServerNpc>& npcs,
                       ServerProjectile& projectile,
                       const glm::vec3& position,
                       const char* impactType,
                       uint32_t directTargetId,
                       uint32_t tick,
                       uint32_t targetTick,
                       uint64_t& totalPacketsOut)
{
    if (projectile.exploded)
        return;
    projectile.exploded = true;
    projectile.position = position;

    ProjectileExplodeEventPacket packet{};
    packet.header.type = PACKET_PROJECTILE_EXPLODE_EVENT;
    packet.header.tick = tick;
    packet.eventId = nextReliableGameplayEventId();
    packet.projectileId = projectile.id;
    packet.ownerPlayerId = projectile.ownerPlayerId;
    packet.fireSerial = projectile.fireSerial;
    packet.weapon = projectile.weaponType;
    packet.weaponDefNetworkId = projectile.weaponDefNetworkId;
    packet.posX = position.x;
    packet.posY = position.y;
    packet.posZ = position.z;
    packet.radius = projectile.splashRadius;

    // Resolve the exact weapon definition so thrown smoke/fire grenades spawn
    // their area-effect volume instead of a splash explosion. Frag/rocket keep
    // the splash path. The explode packet is broadcast for every case so every
    // client can spawn the same visual deterministically from weaponDefNetworkId.
    const WeaponDefinition* explodeDef = nullptr;
    if (const std::string* defId = weaponIdForDefNetworkId(projectile.weaponDefNetworkId))
        explodeDef = WeaponRegistry::instance().get(*defId);
    float onExpireEffect = 0.0f;
    if (explodeDef)
    {
        auto it = explodeDef->customParams.find("on_expire_effect");
        if (it != explodeDef->customParams.end())
            onExpireEffect = it->second;
    }
    if (explodeDef && onExpireEffect > 0.5f)
    {
        const AreaEffectType effect =
            onExpireEffect >= 1.5f ? AreaEffectType::Fire : AreaEffectType::Smoke;
        const AreaEffectParams params =
            areaEffectParamsFromDefinition(*explodeDef, effect);
        if (effect == AreaEffectType::Smoke)
            AreaEffectSystem::instance().spawnSmoke(position, params, (int)tick,
                                                    projectile.ownerPlayerId);
        else
            AreaEffectSystem::instance().spawnFire(position, params, (int)tick,
                                                   projectile.ownerPlayerId);
    }

    // Falloff curves are hot (net.projectile-splash).
    auto splashDamageAt = [&](float dist) -> float {
        GameSplashFalloffV1 request{};
        request.structSize = sizeof(GameSplashFalloffV1);
        request.distance = dist;
        request.fullDamageRadius = projectile.fullDamageRadius;
        request.splashRadius = projectile.splashRadius;
        request.splashDamage = projectile.splashDamage;
        request.edgeDamage = projectile.edgeDamage;
        request.splashExponent = projectile.splashExponent;
        const GameProjectileSplashPolicyV1* policy = hotProjectileSplashPolicy();
        if (policy && policy->damage)
            policy->damage(nullptr, &request);
        else
            HotProjectileSplashImpl::damage(request);
        return request.outDamage;
    };
    auto splashKnockScaleAt = [&](float dist, float damageValue) -> float {
        GameSplashFalloffV1 request{};
        request.structSize = sizeof(GameSplashFalloffV1);
        request.distance = dist;
        request.fullDamageRadius = projectile.fullDamageRadius;
        request.splashRadius = projectile.splashRadius;
        request.splashDamage = projectile.splashDamage;
        request.damageValue = damageValue;
        const GameProjectileSplashPolicyV1* policy = hotProjectileSplashPolicy();
        if (policy && policy->knockScale)
            policy->knockScale(nullptr, &request);
        else
            HotProjectileSplashImpl::knockScale(request);
        return request.outKnockScale;
    };

    if (directTargetId != 0)
    {
        auto directIt = players.find(directTargetId);
        if (directIt != players.end() && !directIt->second.dead)
        {
            ServerPlayer& target = directIt->second;
            MovementLifecycleIdentity lifecycle{
                target.spawnGeneration,
                static_cast<uint32_t>(target.transformEpoch)};
            const glm::vec3 normal = glm::length(target.pos - position) > 0.001f
                ? glm::normalize(target.pos - position)
                : glm::vec3(0.0f, 0.0f, 1.0f);
            MovementContact contact = makeProjectileMovementContact(
                projectile.id, packet.eventId, tick, lifecycle,
                position, normal);
            target.movement.contactHistory.recordStable(contact);
        }
    }

    uint8_t victimsLogged = 0;
    const ServerDamageSource source =
        projectile.weaponType == NETWORK_WEAPON_GRENADE_LAUNCHER
        ? ServerDamageSource::GrenadeExplosion
        : ServerDamageSource::RocketExplosion;

    // The authoritative owner: a player or an NPC. NPC-fired rockets carry
    // ownerPlayerId=0 and ownerNpcId set, so all attribution must use this
    // resolved owner rather than ownerPlayerId alone.
    const uint32_t ownerId = projectile.ownerNpcId != 0
        ? projectile.ownerNpcId : projectile.ownerPlayerId;
    const uint8_t ownerKind = projectile.ownerNpcId != 0
        ? ENTITY_NPC : ENTITY_PLAYER;

    for (auto& entry : players)
    {
        ServerPlayer& victim = entry.second;
        if (!projectile.splashEnabled)
            break;
        if (victim.dead)
            continue;
        // Team-based friendly fire filtering: skip friendly players in explosion splash
        {
            auto ownerIt = players.find(projectile.ownerPlayerId);
            if (ownerIt != players.end() && ownerIt->second.matchTeam >= 0 &&
                victim.matchTeam >= 0 && ownerIt->second.matchTeam == victim.matchTeam &&
                victim.id != projectile.ownerPlayerId)
                continue;
        }

        glm::vec3 historicalPos = victim.pos;
        float historicalYaw = victim.yaw;
        if (targetTick != 0)
            getPlayerPoseAtTick(victim, targetTick, historicalPos, historicalYaw);
        const glm::vec3 center = historicalPos + glm::vec3(0.0f, 0.0f, PLAYER_HEIGHT * 0.25f);
        const glm::vec3 toVictim = center - position;
        const float dist = glm::length(toVictim);
        if (dist >= projectile.splashRadius)
            continue;

        if (projectile.splashLineOfSight)
        {
            glm::vec3 target;
            splashNearestPlayerBodyPoint(victim, position, target,
                                         &historicalPos, &historicalYaw);
            if (!splashHasLineOfSight(world, position, target))
                continue; // wall/cover between blast and the victim's nearest body part → no hit
        }

        const glm::vec3 dir = dist > 0.001f
            ? toVictim / dist
            : glm::vec3(0.0f, 1.0f, 0.0f);
        float damageValue = splashDamageAt(dist);
        if (victim.id == directTargetId && dist < 1.5f)
            damageValue = std::max(damageValue, projectile.splashDamage);
        const bool isSelfDamage = (victim.id == projectile.ownerPlayerId);
        if (isSelfDamage)
            damageValue *= std::max(0.0f, projectile.selfDamageMultiplier);
        int finalDamage = std::max(1, (int)std::round(damageValue));

        const float knockScale = splashKnockScaleAt(dist, damageValue);
        const float ownerMul = isSelfDamage
            ? projectile.selfKnockbackMultiplier
            : 1.0f;
        glm::vec3 knockback =
            dir * projectile.knockbackStrength * knockScale * ownerMul;

        // Generic authoritative damage policy: the hot behavior owns the final
        // value. Falls back to the JSON-derived base if no behavior handles it.
        {
            ServerDamagePolicyInput policyInput{};
            policyInput.source = GAME_DAMAGE_SOURCE_EXPLOSION;
            policyInput.attackerEntity = Ecs::raw(ownerKind == ENTITY_NPC
                ? Ecs::ensure(EntityRealm::Server, EntityDomain::Npc, ownerId)
                : Ecs::ensure(EntityRealm::Server, EntityDomain::Player, ownerId));
            policyInput.victimEntity = Ecs::raw(
                Ecs::ensure(EntityRealm::Server, EntityDomain::Player, victim.id));
            policyInput.projectileEntity = Ecs::raw(EntityRegistry::instance().find(
                EntityRealm::Server, EntityDomain::Projectile, projectile.id));
            policyInput.weaponNetworkId = projectile.weaponDefNetworkId;
            policyInput.victimIsNpc = 0;
            policyInput.distance = dist;
            policyInput.tick = tick;
            finalDamage = serverResolveDamagePolicy(policyInput, finalDamage, knockback);
        }

        printf("%s [SELF_DAMAGE] ownerId=%u victimId=%u isSelf=%d "
               "baseDmg=%.1f mul=%.2f finalDmg=%d\n",
               serverTimestamp(), projectile.ownerPlayerId, victim.id,
               (int)isSelfDamage, projectile.splashDamage,
               isSelfDamage ? projectile.selfDamageMultiplier : 1.0f,
               finalDamage);

        ServerDamageResult damage = applyServerDamage(
            players, victim, projectile.ownerPlayerId, finalDamage,
            knockback, source);
        if (damage.applied)
        {
            MovementLifecycleIdentity lifecycle{
                victim.spawnGeneration,
                static_cast<uint32_t>(victim.transformEpoch)};
            MovementContact contact = makeExplosionMovementContact(
                packet.eventId, projectile.ownerPlayerId, tick,
                lifecycle, position, glm::length(knockback));
            victim.movement.contactHistory.recordStable(contact);
            // NPC-owned splash counts as recent NPC damage so a later ownerless
            // or self-inflicted killing blow is still attributed to the NPC.
            if (projectile.ownerNpcId != 0)
            {
                victim.lastNpcDamageSourceId = projectile.ownerNpcId;
                victim.lastNpcDamageTick = tick;
            }
        }
        queueServerDamageConfirmedEvent(
            sock, players, tick, totalPacketsOut, projectile.ownerPlayerId, victim,
            finalDamage, damage, center, dir, knockback, source,
            projectile.weaponType, projectile.fireSerial, projectile.id,
            projectile.ownerNpcId, std::string());

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
            out.targetSpawnGeneration = victim.spawnGeneration;
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

    for (auto& npcEntry : npcs)
    {
        ServerNpc& npc = npcEntry.second;
        if (!projectile.splashEnabled)
            break;
        if (npc.health <= 0)
            continue;
        // Skip the NPC that fired this projectile (no self-damage)
        if (projectile.ownerNpcId != 0 && npc.entityId == projectile.ownerNpcId)
            continue;

        glm::vec3 historicalNpcPos = npc.pos;
        float historicalNpcYaw = npc.yaw;
        if (targetTick != 0)
            getNpcPoseAtTick(npc, targetTick, historicalNpcPos, historicalNpcYaw);
        const glm::vec3 npcCenter = historicalNpcPos +
            glm::vec3(0.0f, 0.0f, PLAYER_HEIGHT * 0.25f);
        const glm::vec3 toNpc = npcCenter - position;
        const float dist = glm::length(toNpc);
        if (dist >= projectile.splashRadius)
            continue;

        if (projectile.splashLineOfSight)
        {
            glm::vec3 target;
            splashNearestNpcBodyPoint(npc, position, target, targetTick);
            if (!splashHasLineOfSight(world, position, target))
                continue; // wall/cover between blast and the NPC's nearest body part → no hit
        }

        const glm::vec3 dir = dist > 0.001f
            ? toNpc / dist
            : glm::vec3(0.0f, 1.0f, 0.0f);
        float damageValue = splashDamageAt(dist);
        int finalDamage = std::max(1, (int)std::round(damageValue));

        const float knockScale = splashKnockScaleAt(dist, damageValue);
        glm::vec3 knockback =
            dir * projectile.knockbackStrength * knockScale;

        // Generic authoritative damage policy (NPC victim).
        {
            ServerDamagePolicyInput policyInput{};
            policyInput.source = GAME_DAMAGE_SOURCE_EXPLOSION;
            policyInput.attackerEntity = Ecs::raw(ownerKind == ENTITY_NPC
                ? Ecs::ensure(EntityRealm::Server, EntityDomain::Npc, ownerId)
                : Ecs::ensure(EntityRealm::Server, EntityDomain::Player, ownerId));
            policyInput.victimEntity = Ecs::raw(
                Ecs::ensure(EntityRealm::Server, EntityDomain::Npc, npc.entityId));
            policyInput.projectileEntity = Ecs::raw(EntityRegistry::instance().find(
                EntityRealm::Server, EntityDomain::Projectile, projectile.id));
            policyInput.weaponNetworkId = projectile.weaponDefNetworkId;
            policyInput.victimIsNpc = 1;
            policyInput.distance = dist;
            policyInput.tick = tick;
            finalDamage = serverResolveDamagePolicy(policyInput, finalDamage, knockback);
        }

        npc.health -= finalDamage;
        npc.knockbackImpulse += knockback;
        const bool killed = npc.health <= 0;
        if (killed)
        {
            npc.health = 0;
            const char* killWeaponId = networkWeaponTypeName(projectile.weaponType);
            if (const std::string* defId = weaponIdForDefNetworkId(projectile.weaponDefNetworkId))
                killWeaponId = defId->c_str();
            std::string killWeaponDisplay = killWeaponId;
            if (const WeaponDefinition* wd = WeaponRegistry::instance().get(killWeaponId))
                if (!wd->displayName.empty()) killWeaponDisplay = wd->displayName;
            glm::vec3 killerPos = position;
            auto killerIt = players.find(ownerId);
            if (killerIt != players.end()) killerPos = killerIt->second.pos;
            serverGamemodeRecordKill(sock, players, &npcs,
                ownerId, ownerKind,
                npc.entityId, ENTITY_NPC,
                killWeaponId, killWeaponDisplay, projectile.fireSerial,
                killerPos, npc.pos, tick, totalPacketsOut);
        }

        broadcastNpcDamageEvent(
            sock, players, tick, totalPacketsOut, projectile.ownerPlayerId,
            npc, finalDamage, killed,
            position, npcCenter, dir, -dir, projectile.weaponType);

        printf("%s [EXPLOSION NPC DAMAGE] projectileId=%u ownerPlayerId=%u "
               "npcId=%u distance=%.2f damage=%d healthAfter=%d killed=%d\n",
               serverTimestamp(), projectile.id, projectile.ownerPlayerId,
               npc.entityId, dist, finalDamage, npc.health, (int)killed);
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

    queueReliableGameplayEventToAll(sock, players, &packet, sizeof(packet),
                                    packet.eventId, packet.eventSessionId,
                                    totalPacketsOut);
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

    ProjectileFireResultPacket reject{};
    reject.header.type = PACKET_PROJECTILE_FIRE_RESULT;
    reject.header.tick = tick;
    reject.header.playerId = request->header.playerId;
    reject.fireSerial = request->fireSerial;
    reject.weapon = request->weapon;
    reject.accepted = 0;
    reject.reason = PROJECTILE_FIRE_CONFIG_MISSING;
    auto legacyShooter = players.find(request->header.playerId);
    if (legacyShooter != players.end() && sameAddress(legacyShooter->second.addr, from))
        serverSendToPlayer(sock, legacyShooter->second, &reject, sizeof(reject));
    printf("%s [PROJECTILE FIRE REQUEST RX] playerId=%u fireSerial=%u "
           "weapon=%s accepted=0 reason=legacy-direct-packet-disabled\n",
           serverTimestamp(), request->header.playerId, request->fireSerial,
           networkWeaponTypeName(request->weapon));
    return;

}

void tickServerProjectiles(SOCKET sock,
                           std::unordered_map<uint32_t, ServerPlayer>& players,
                           std::unordered_map<uint32_t, ServerNpc>& npcs,
                           std::unordered_map<uint32_t, ServerProjectile>& projectiles,
                           const HeadlessWorld& world,
                           float /*dt*/, uint32_t tick, uint64_t& totalPacketsOut)
{
    constexpr float tickDt = GAMEPLAY_FIXED_DT;
    uint32_t activeCount = 0;
    uint32_t movingCount = 0;
    uint32_t sleepingCount = 0;

    for (auto it = projectiles.begin(); it != projectiles.end(); )
    {
        ServerProjectile& projectile = it->second;
        ++activeCount;
        if (projectileIsSleeping(projectile))
            ++sleepingCount;
        else
            ++movingCount;

        const bool sharedProjectile =
            projectile.weaponType == NETWORK_WEAPON_ROCKET_LAUNCHER ||
            projectile.weaponType == NETWORK_WEAPON_GRENADE_LAUNCHER ||
            projectile.genericMotion;

        if (sharedProjectile)
        {
            const uint32_t firstStepTick = projectile.simulationTick != 0
                ? projectile.simulationTick + 1 : tick;
            for (uint32_t stepTick = firstStepTick;
                 stepTick <= tick && !projectile.exploded; ++stepTick)
            {
                projectile.previousPosition = projectile.position;
                projectile.stateAccumulator += tickDt;
                const int previousBounceCount = projectile.bounceCount;
                ProjectilePhysicsState state = makePhysicsState(projectile);
                ProjectilePhysicsConfig config = makePhysicsConfig(projectile);
                // Look up owner's team for friendly fire filtering
                int ownerTeam = -1;
                auto ownerIt = players.find(projectile.ownerPlayerId);
                if (ownerIt != players.end())
                    ownerTeam = ownerIt->second.matchTeam;
                ServerProjectileWorldView physicsWorld(
                    world, players, npcs, projectile.ownerPlayerId,
                    projectile.ownerNpcId,
                    projectile.distanceTraveled < projectile.armingDistance,
                    stepTick, ownerTeam);

                auto simStart = std::chrono::steady_clock::now();
                ProjectileStepResult step =
                    simulateProjectileTick(state, config, physicsWorld, tickDt);
                gProjectilePerf.projectileSimUs += (uint64_t)std::chrono::duration<double, std::micro>(
                    std::chrono::steady_clock::now() - simStart).count();
                gProjectilePerf.triangleQueryCount += step.triangleQueryCount;
                gProjectilePerf.triangleCandidateTotal += step.triangleCandidateTotal;
                gProjectilePerf.triangleCandidateMax = std::max(
                    gProjectilePerf.triangleCandidateMax, step.triangleCandidateMax);
                gProjectilePerf.playerCapsuleCandidateTotal += step.playerCapsuleCandidateTotal;
                gProjectilePerf.playerCapsuleCandidateMax = std::max(
                    gProjectilePerf.playerCapsuleCandidateMax, step.playerCapsuleCandidateMax);
                applyPhysicsState(projectile, state);
                projectile.distanceTraveled += step.travelDistance;
                projectile.simulationTick = stepTick;
                if (state.sleeping || state.bounceCount > previousBounceCount ||
                    step.type == ProjectileCollisionType::WorldBounce ||
                    step.type == ProjectileCollisionType::WorldImpact)
                {
                    projectile.worldTouched = true;
                }

                if (step.type != ProjectileCollisionType::None)
                {
                auto& _lg = ::StructuredLogger::instance();
                if (_lg.shouldLog(::StructuredCategory::GrenadeLauncher, ::StructuredLevel::Important)) {
                    ::StructuredLogger::Entry e;
                    e.category = ::StructuredCategory::GrenadeLauncher;
                    e.level = ::StructuredLevel::Important;
                    e.eventId = "PROJECTILE_POLICY_EVENT";
                    e.correlationId = "PROJECTILE_" + std::to_string(projectile.id);
                    e.reason =
                        (step.type == ProjectileCollisionType::LifetimeExpired && projectile.explodeOnLifetime) ? "explode-lifetime" :
                        (step.type == ProjectileCollisionType::PlayerImpact && projectile.explodeOnPlayerImpact) ? "explode-player" :
                        (step.type == ProjectileCollisionType::WorldImpact && projectile.explodeOnWorldImpact) ? "explode-world" :
                        (step.type == ProjectileCollisionType::WorldBounce) ? "continue-bounce" :
                        "continue-event";
                    char b[256]; std::snprintf(b, sizeof(b),
                        "projectileId=%u weapon=%s stepType=%d hitPlayerId=%u age=%.2f bounceCount=%d",
                        projectile.id, networkWeaponTypeName(projectile.weaponType),
                        (int)step.type, step.hitPlayerId, projectile.age,
                        projectile.bounceCount);
                    e.message = b;
                    _lg.write(e);
                }
                }

                // Generic projectile impact policy: a hot behavior may own the
                // consequence. When handled, its outExplode decides; otherwise
                // the cold per-type flags below run (fallback).
                bool impactHandled = false;
                bool impactExplode = false;
                if (step.type != ProjectileCollisionType::None)
                {
                    ProjectileImpactPolicyV1 impact{};
                    impact.ownerId = projectile.ownerPlayerId != 0
                        ? projectile.ownerPlayerId : projectile.ownerNpcId;
                    impact.victimId = step.hitPlayerId;
                    impact.weaponNetworkId = projectile.weaponType;
                    impact.projectileTypeId = projectile.genericTypeId != 0
                        ? projectile.genericTypeId : projectile.weaponType;
                    if (impact.ownerId != 0)
                        impact.ownerEntity = static_cast<std::uint64_t>(Ecs::ensure(
                            EntityRealm::Server, EntityDomain::Player, impact.ownerId));
                    if (step.hitPlayerId != 0)
                        impact.victimEntity = static_cast<std::uint64_t>(Ecs::ensure(
                            EntityRealm::Server, EntityDomain::Player, step.hitPlayerId));
                    impact.hitKind =
                        step.type == ProjectileCollisionType::WorldImpact ? 1u :
                        step.type == ProjectileCollisionType::PlayerImpact ? 2u :
                        step.type == ProjectileCollisionType::LifetimeExpired ? 4u : 0u;
                    impact.position[0] = step.hitPosition.x;
                    impact.position[1] = step.hitPosition.y;
                    impact.position[2] = step.hitPosition.z;
                    impact.age = projectile.age;
                    impact.lifetime = projectile.lifetime;
                    impactHandled = LiveBehavior::dispatchProjectileImpact(impact, stepTick);
                    impactExplode = impact.outExplode != 0;
                }

                const bool coldExplode =
                    (step.type == ProjectileCollisionType::LifetimeExpired && projectile.explodeOnLifetime) ||
                    (step.type == ProjectileCollisionType::PlayerImpact && projectile.explodeOnPlayerImpact) ||
                    (step.type == ProjectileCollisionType::WorldImpact && projectile.explodeOnWorldImpact);
                const bool explodeNow = impactHandled ? impactExplode : coldExplode;

                if (explodeNow && step.type == ProjectileCollisionType::LifetimeExpired)
                {
                    explodeProjectile(sock, world, players, npcs, projectile, projectile.position,
                                      "lifetime", 0, tick, stepTick, totalPacketsOut);
                }
                else if (explodeNow && step.type == ProjectileCollisionType::PlayerImpact)
                {
                    explodeProjectile(sock, world, players, npcs, projectile, step.hitPosition,
                                      "player", step.hitPlayerId, tick, stepTick,
                                      totalPacketsOut);
                }
                else if (explodeNow && step.type == ProjectileCollisionType::WorldImpact)
                {
                    explodeProjectile(sock, world, players, npcs, projectile, step.hitPosition,
                                      "world", 0, tick, stepTick, totalPacketsOut);
                }
                else if (impactHandled && !impactExplode &&
                         step.type == ProjectileCollisionType::LifetimeExpired)
                {
                    // A hot policy chose not to explode at end of life; despawn.
                    projectile.exploded = true;
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
        }
        else
        {
            projectile.previousPosition = projectile.position;
            projectile.stateAccumulator += tickDt;
            projectile.age += tickDt;
            projectile.simulationTick = tick;
            if (projectile.age >= projectile.lifetime)
            {
                explodeProjectile(sock, world, players, npcs, projectile, projectile.position,
                                  "lifetime", 0, tick, tick,
                                  totalPacketsOut);
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
            Ecs::despawn(EntityRegistry::instance().find(
                EntityRealm::Server, EntityDomain::Projectile, projectile.id));
            it = projectiles.erase(it);
            continue;
        }

        const bool sleepingNow = projectileIsSleeping(projectile);
        const float stateInterval = sleepingNow ? 1.0f : 0.05f;
        if (projectile.stateAccumulator >= stateInterval)
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

    gProjectilePerf.activeProjectiles = activeCount;
    gProjectilePerf.movingProjectiles = movingCount;
    gProjectilePerf.sleepingProjectiles = sleepingCount;

    // ── Persistent area effects (smoke/fire) ─────────────────────────
    // The server owns fire damage; clients predict it locally and reconcile.
    AreaEffectSystem::instance().update((int)tick);
    std::vector<AreaEffectContactTarget> fireTargets;
    fireTargets.reserve(players.size() + npcs.size());
    for (const auto& entry : players)
    {
        const ServerPlayer& p = entry.second;
        if (p.dead)
            continue;
        AreaEffectContactTarget t;
        t.id = p.id;
        t.center = p.pos + glm::vec3(0.0f, 0.0f, PLAYER_HEIGHT * 0.5f);
        t.radius = PLAYER_RADIUS;
        t.height = PLAYER_HEIGHT;
        fireTargets.push_back(t);
    }
    for (const auto& entry : npcs)
    {
        const ServerNpc& n = entry.second;
        if (n.health <= 0)
            continue;
        AreaEffectContactTarget t;
        t.id = n.entityId;
        t.center = n.pos + glm::vec3(0.0f, 0.0f, PLAYER_HEIGHT * 0.5f);
        t.radius = PLAYER_RADIUS;
        t.height = PLAYER_HEIGHT;
        fireTargets.push_back(t);
    }
    std::vector<AreaEffectDamageEvent> fireDamage;
    AreaEffectSystem::instance().collectFireDamage(fireTargets, (int)tick, fireDamage);
    for (const AreaEffectDamageEvent& ev : fireDamage)
    {
        auto pIt = players.find(ev.targetId);
        if (pIt != players.end() && !pIt->second.dead)
        {
            ServerPlayer& victim = pIt->second;
            const ServerDamageResult dmg = applyServerDamage(
                players, victim, 0, ev.damage, glm::vec3(0.0f), ServerDamageSource::Fire);
            queueServerDamageConfirmedEvent(
                sock, players, tick, totalPacketsOut, 0, victim, ev.damage, dmg,
                victim.pos, glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f),
                ServerDamageSource::Fire, NETWORK_WEAPON_GRENADE_LAUNCHER, 0, 0, 0,
                std::string());
            continue;
        }
        auto nIt = npcs.find(ev.targetId);
        if (nIt != npcs.end() && nIt->second.health > 0)
        {
            ServerNpc& npc = nIt->second;
            npc.health -= ev.damage;
            const bool killed = npc.health <= 0;
            if (killed)
                npc.health = 0;
            broadcastNpcDamageEvent(
                sock, players, tick, totalPacketsOut, 0, npc, ev.damage, killed,
                npc.pos, npc.pos, glm::vec3(0.0f, 0.0f, 1.0f),
                glm::vec3(0.0f, 0.0f, -1.0f), NETWORK_WEAPON_GRENADE_LAUNCHER);
        }
    }
}

void cancelDeadNpcProjectiles(
    SOCKET sock,
    std::unordered_map<uint32_t, ServerPlayer>& players,
    const std::unordered_map<uint32_t, ServerNpc>& npcs,
    std::unordered_map<uint32_t, ServerProjectile>& projectiles,
    uint32_t tick,
    uint64_t& totalPacketsOut)
{
    std::uint32_t cancelled = 0;
    for (auto it = projectiles.begin(); it != projectiles.end(); )
    {
        const ServerProjectile& projectile = it->second;
        if (projectile.ownerNpcId == 0)
        {
            ++it;
            continue;
        }

        const auto npcIt = npcs.find(projectile.ownerNpcId);
        const bool ownerAlive = npcIt != npcs.end() && npcIt->second.health > 0;
        // The cancellation policy is hot (net.projectile-cancel); the cold path
        // owns the container, despawn, and transport.
        GameProjectileCancelV1 cancelRequest{};
        cancelRequest.structSize = sizeof(GameProjectileCancelV1);
        cancelRequest.version = GAME_PROJECTILE_CANCEL_VERSION;
        cancelRequest.projectileId = projectile.id;
        cancelRequest.ownerPlayerId = projectile.ownerPlayerId;
        cancelRequest.ownerNpcId = projectile.ownerNpcId;
        cancelRequest.ownerAlive = ownerAlive ? 1u : 0u;
        cancelRequest.ownerDead = ownerAlive ? 0u : 1u;
        cancelRequest.exploded = projectile.exploded ? 1u : 0u;
        GameProjectileCancelFn cancelPolicy = hotProjectileCancelPolicy();
        if (cancelPolicy)
            cancelPolicy(nullptr, &cancelRequest);
        else
            HotProjectileCancelImpl::evaluate(cancelRequest);
        if (!cancelRequest.outCancel)
        {
            ++it;
            continue;
        }

        ProjectileDespawnEventPacket event{};
        event.header.type = PACKET_PROJECTILE_DESPAWN_EVENT;
        event.header.tick = tick;
        event.eventId = nextReliableGameplayEventId();
        event.eventSessionId = serverReliableEventSessionId();
        event.projectileId = projectile.id;
        event.weapon = projectile.weaponType;
        event.reason = 2; // owner NPC died or was removed
        queueReliableGameplayEventToAll(
            sock, players, &event, sizeof(event), event.eventId,
            event.eventSessionId, totalPacketsOut);
        Debug::log(Debug::Category::Networking,
            "[NPC PROJECTILE CANCEL] projectileId=%u ownerNpcId=%u tick=%u reason=owner-dead",
            projectile.id, projectile.ownerNpcId, tick);
        Ecs::despawn(EntityRegistry::instance().find(
            EntityRealm::Server, EntityDomain::Projectile, projectile.id));
        it = projectiles.erase(it);
        ++cancelled;
    }
    if (cancelled > 0)
    {
        LiveEventJournal::Fields f;
        f.tick = tick;
        f.result = "owner_dead";
        f.extra = std::string("\"cancelled_count\":") + std::to_string(cancelled);
        LiveEventJournal::instance().record("server.npc_projectiles_cancelled", f);
    }
}

// Generic authoritative projectile spawn from a package spec. The kernel keeps
// id allocation, simulation, collision, and replication; hot code supplies only
// generic data and gets back a stable projectile entity id.
bool serverSpawnGenericProjectile(const GameProjectileSpawnSpecV1& spec,
                                  std::uint64_t* outEntity)
{
    ServerContextV1* context = activeServerContext();
    if (!context || !context->players || !context->projectiles ||
        !context->nextProjectileId || !context->tick || !context->totalPacketsOut)
        return false;
    auto& players =
        *static_cast<std::unordered_map<uint32_t, ServerPlayer>*>(context->players);
    auto& projectiles =
        *static_cast<std::unordered_map<uint32_t, ServerProjectile>*>(context->projectiles);
    const uint32_t tick = *context->tick;

    ServerProjectile projectile;
    projectile.id = (*context->nextProjectileId)++;
    if (*context->nextProjectileId == 0)
        *context->nextProjectileId = 1;
    projectile.ownerPlayerId = spec.ownerPlayerId;
    projectile.ownerNpcId = spec.ownerNpcId;
    projectile.weaponType = static_cast<std::uint8_t>(spec.weaponNetworkId);
    projectile.weaponDefNetworkId = static_cast<std::uint16_t>(spec.weaponNetworkId);
    projectile.position = glm::vec3(spec.position[0], spec.position[1], spec.position[2]);
    projectile.previousPosition = projectile.position;
    projectile.velocity = glm::vec3(spec.velocity[0], spec.velocity[1], spec.velocity[2]);
    projectile.radius = spec.radius;
    projectile.lifetime = spec.lifetime;
    projectile.gravity = spec.gravity;
    projectile.drag = spec.drag;
    projectile.restitution = spec.restitution;
    projectile.maxBounceCount = static_cast<int>(spec.maxBounceCount);
    projectile.explodeOnPlayerImpact = spec.explodeOnPlayerImpact != 0;
    projectile.explodeOnWorldImpact = spec.explodeOnWorldImpact != 0;
    projectile.explodeOnLifetime = spec.explodeOnLifetime != 0;
    projectile.splashRadius = spec.splashRadius;
    projectile.splashDamage = spec.splashDamage;
    projectile.splashExponent = spec.splashExponent > 0.0f ? spec.splashExponent : 2.0f;
    projectile.fullDamageRadius = spec.fullDamageRadius;
    projectile.edgeDamage = spec.edgeDamage;
    projectile.knockbackStrength = spec.knockbackStrength;
    projectile.selfDamageMultiplier = spec.selfDamageMultiplier > 0.0f
        ? spec.selfDamageMultiplier : 1.0f;
    projectile.splashEnabled = spec.splashEnabled != 0 && spec.splashRadius > 0.0f;
    projectile.genericMotion = true;
    projectile.genericTypeId = spec.typeId;
    projectile.spawnTick = tick;
    projectile.fireViewTick = tick;
    projectile.simulationTick = tick;
    projectiles[projectile.id] = projectile;

    EntityId ownerEntity = static_cast<EntityId>(spec.ownerEntity);
    if (ownerEntity == kInvalidEntityId && spec.ownerPlayerId != 0)
        ownerEntity = Ecs::ensure(EntityRealm::Server, EntityDomain::Player,
                                  spec.ownerPlayerId);
    const EntityId projectileEntity = Ecs::spawnRocket(
        EntityRealm::Server, projectile.id, ownerEntity, projectile.position,
        projectile.velocity, projectile.weaponDefNetworkId, projectile.fireSerial,
        projectile.lifetime, NetworkAuthority::Server);

    ProjectileSpawnEventPacket spawn{};
    spawn.header.type = PACKET_PROJECTILE_SPAWN_EVENT;
    spawn.header.tick = tick;
    fillProjectilePose(spawn, projectile);
    broadcastPacket(static_cast<SOCKET>(context->sock), players, spawn,
                    *context->totalPacketsOut, projectile.ownerPlayerId);

    if (outEntity)
        *outEntity = static_cast<std::uint64_t>(projectileEntity);
    Debug::log(Debug::Category::Weapons,
        "[GENERIC PROJECTILE SPAWN] id=%u type=%llu owner=%u pos=(%.2f,%.2f,%.2f)\n",
        projectile.id, (unsigned long long)spec.typeId, spec.ownerPlayerId,
        projectile.position.x, projectile.position.y, projectile.position.z);
    return true;
}

} // namespace MimitaNet
