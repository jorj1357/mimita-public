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
#include "network/network-weapons.h"
#include "debug/structured-log.h"
#include "debug/debug-log.h"
#include "persistence/persistence-emit.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cfloat>
#include <cstdio>
#include <optional>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "combat/projectile-simulation.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-runtime.h"
#include "combat/weapon-types.h"
#include "physics/movement/movement-step.h"
#include "physics/movement/physics-collision-shared.h"

namespace MimitaNet {
namespace {

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
    packet.posX = position.x;
    packet.posY = position.y;
    packet.posZ = position.z;
    packet.radius = projectile.splashRadius;

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

    for (auto& entry : players)
    {
        ServerPlayer& victim = entry.second;
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
        float damageValue = projectile.splashDamage *
            std::exp(-std::pow(dist / projectile.splashRadius, 2.0f) *
                     projectile.splashExponent);
        if (victim.id == directTargetId && dist < 1.5f)
            damageValue = std::max(damageValue, projectile.splashDamage);
        const bool isSelfDamage = (victim.id == projectile.ownerPlayerId);
        if (isSelfDamage)
            damageValue *= std::max(0.0f, projectile.selfDamageMultiplier);
        const int finalDamage = std::max(1, (int)std::round(damageValue));

        const float t = dist / projectile.splashRadius;
        const float knockScale = (1.0f - t * t) * 0.85f + 0.15f;
        const float ownerMul = isSelfDamage
            ? projectile.selfKnockbackMultiplier
            : 1.0f;
        const glm::vec3 knockback =
            dir * projectile.knockbackStrength * knockScale * ownerMul;

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
        }
        queueServerDamageConfirmedEvent(
            sock, players, tick, totalPacketsOut, projectile.ownerPlayerId, victim,
            finalDamage, damage, center, dir, knockback, source,
            projectile.weaponType, projectile.fireSerial, projectile.id);

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
        float damageValue = projectile.splashDamage *
            std::exp(-std::pow(dist / projectile.splashRadius, 2.0f) *
                     projectile.splashExponent);
        const int finalDamage = std::max(1, (int)std::round(damageValue));

        const float t = dist / projectile.splashRadius;
        const float knockScale = (1.0f - t * t) * 0.85f + 0.15f;
        const glm::vec3 knockback =
            dir * projectile.knockbackStrength * knockScale;

        npc.health -= finalDamage;
        npc.knockbackImpulse += knockback;
        const bool killed = npc.health <= 0;
        if (killed)
        {
            npc.health = 0;
            auto attacker = players.find(projectile.ownerPlayerId);
            if (attacker != players.end())
                attacker->second.health = serverMaxHp();
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

ServerProjectileAttackResult handleGenericProjectileAttack(
    SOCKET sock,
    std::unordered_map<uint32_t, ServerPlayer>& players,
    std::unordered_map<uint32_t, ServerNpc>& npcs,
    std::unordered_map<uint32_t, ServerProjectile>& projectiles,
    uint32_t& nextProjectileId,
    ServerPlayer& shooter,
    const WeaponDefinition& definition,
    uint32_t requestId,
    const glm::vec3& origin,
    const glm::vec3& direction,
    uint32_t clientSimulationTick,
    uint32_t tick,
    uint64_t& totalPacketsOut)
{
    ServerProjectileAttackResult result;

    auto rtIt = shooter.weaponRuntimes.find(definition.id);
    if (rtIt != shooter.weaponRuntimes.end())
    {
        result.magazineAmmo = rtIt->second.magazineAmmo;
        result.reserveAmmo = rtIt->second.reserveAmmo;
        result.nextAllowedFireTick = rtIt->second.nextAllowedFireTick;
        result.stateRevision = rtIt->second.stateRevision;
    }

    const uint8_t networkWeapon = networkWeaponTypeForDefinition(definition);
    const float originDistance = finiteVec(origin)
        ? glm::length(origin - shooter.pos)
        : 99999.0f;
    const float directionLength = finiteVec(direction)
        ? glm::length(direction)
        : 0.0f;

    if (shooter.dead)
    {
        result.reason = 2;
        return result;
    }
    if (requestId == 0)
    {
        result.reason = 8;
        return result;
    }
    if (definition.executionType != WeaponExecutionType::Projectile ||
        !networkWeaponTypeIsProjectile(networkWeapon))
    {
        result.reason = 7;
        return result;
    }
    if (rtIt == shooter.weaponRuntimes.end() || !rtIt->second.initialized)
    {
        result.reason = 7;
        return result;
    }
    if (!finiteVec(origin) || originDistance > 12.0f ||
        directionLength < 0.5f || directionLength > 1.5f)
    {
        result.reason = 5;
        return result;
    }

    ServerPlayer::ServerWeaponRuntime& runtime = rtIt->second;
    // Ammo is client-authoritative: the client owns its clip and never gets
    // rejected for ammo. The server-side counter stays informational.
    const bool consumesAmmo = definition.magazineSize > 0;

    constexpr uint64_t COOLDOWN_GRACE_TICKS = 2;
    if (tick + COOLDOWN_GRACE_TICKS < runtime.nextAllowedFireTick)
    {
        result.reason = 1;
        result.magazineAmmo = runtime.magazineAmmo;
        result.reserveAmmo = runtime.reserveAmmo;
        result.nextAllowedFireTick = runtime.nextAllowedFireTick;
        result.stateRevision = runtime.stateRevision;
        return result;
    }

    auto cfgOpt = projectileConfigFromDefinition(definition, networkWeapon);
    if (!cfgOpt)
    {
        result.reason = 7;
        result.magazineAmmo = runtime.magazineAmmo;
        result.reserveAmmo = runtime.reserveAmmo;
        result.nextAllowedFireTick = runtime.nextAllowedFireTick;
        result.stateRevision = runtime.stateRevision;
        return result;
    }
    const ProjectileConfig& cfg = *cfgOpt;
    const glm::vec3 dir = glm::normalize(direction);

    ServerProjectile projectile;
    projectile.id = nextProjectileId++;
    if (nextProjectileId == 0)
        nextProjectileId = 1;
    projectile.ownerPlayerId = shooter.id;
    projectile.fireSerial = requestId;
    projectile.weaponType = networkWeapon;
    projectile.position = origin;
    projectile.previousPosition = origin;
    projectile.velocity = dir * cfg.speed + glm::vec3(0.0f, 0.0f, cfg.upBias);
    projectile.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    if (networkWeapon == NETWORK_WEAPON_GRENADE_LAUNCHER)
    {
        glm::vec3 forward = glm::length(dir) > 0.0001f ? dir : glm::vec3(1.0f, 0.0f, 0.0f);
        glm::vec3 refUp = std::fabs(forward.z) < 0.99f
            ? glm::vec3(0.0f, 0.0f, 1.0f)
            : glm::vec3(1.0f, 0.0f, 0.0f);
        glm::vec3 right = glm::normalize(glm::cross(forward, refUp));
        projectile.angularVelocity = right * cfg.angularSpeed;
    }
    projectile.lifetime = cfg.lifetime;
    projectile.radius = cfg.radius;
    projectile.splashRadius = cfg.splashRadius;
    projectile.splashDamage = cfg.splashDamage;
    projectile.splashExponent = cfg.splashExponent;
    projectile.knockbackStrength = cfg.knockbackStrength;
    projectile.selfKnockbackMultiplier = cfg.selfKnockbackMultiplier;
    projectile.selfDamageMultiplier = cfg.selfDamageMultiplier;
    projectile.gravity = cfg.gravity;
    projectile.drag = cfg.drag;
    projectile.restitution = cfg.restitution;
    projectile.friction = cfg.friction;
    projectile.armingDistance = cfg.armingDistance;
    projectile.armingTime = cfg.armingTime;
    projectile.minBounceSpeed = cfg.minBounceSpeed;
    projectile.angularDrag = cfg.angularDrag;
    projectile.maxBounceCount = cfg.maxBounceCount;
    auto cp = [&](const char* key, float fallback) -> float {
        auto it = definition.customParams.find(key);
        return it != definition.customParams.end() ? it->second : fallback;
    };
    projectile.explodeOnPlayerImpact = cp("explodeOnPlayerImpact", 1.0f) > 0.0f;
    projectile.explodeOnWorldImpact = cp("explodeOnWorldImpact", 0.0f) > 0.0f;
    projectile.explodeOnLifetime = cp("explodeOnLifetime", 1.0f) > 0.0f;
    projectile.splashLineOfSight = cfg.splashLineOfSight;
    const uint32_t fireViewTick = estimateServerRewindTick(
        shooter, clientSimulationTick, tick);
    projectile.spawnTick = tick;
    projectile.fireViewTick = fireViewTick;
    projectile.simulationTick = fireViewTick > 0 ? fireViewTick : tick;

    Debug::logThrottled(Debug::Category::Weapons, "projectile-replay", 1.0,
        "[PROJECTILE REPLAY] playerId=%u requestId=%u receiveTick=%u "
        "clientFireTick=%u mappedFireTick=%u replayTicks=%u weapon=%s\n",
        shooter.id, requestId, tick, clientSimulationTick, fireViewTick,
        fireViewTick > 0 && fireViewTick < tick ? tick - fireViewTick : 0,
        networkWeaponTypeName(networkWeapon));

    if (consumesAmmo && runtime.magazineAmmo > 0)
        --runtime.magazineAmmo;
    runtime.nextAllowedFireTick = (uint64_t)tick +
        (uint64_t)std::ceil(cfg.fireDelay * SERVER_TICK_RATE);
    runtime.reloading = false;
    runtime.reloadCompleteTick = 0;
    ++runtime.stateRevision;

    shooter.lastProjectileFireSerial = requestId;
    shooter.nextProjectileFireTick = runtime.nextAllowedFireTick;
    shooter.projectileFireCooldown = cfg.fireDelay;

    ProjectileSpawnEventPacket spawn{};
    spawn.header.type = PACKET_PROJECTILE_SPAWN_EVENT;
    spawn.header.tick = tick;
    fillProjectilePose(spawn, projectile);
    projectiles[projectile.id] = projectile;
    // Broadcast the spawn to everyone EXCEPT the shooter — the shooter's client
    // already has its own instant predicted projectile and adopting a server copy
    // back is what caused the "two rockets" duplicate on badconn.
    broadcastPacket(sock, players, spawn, totalPacketsOut, projectile.ownerPlayerId);

    printf("%s [PROJECTILE GENERIC ACCEPT] playerId=%u requestId=%u "
           "projectileId=%u weapon=%s ammo=%d/%d nextAllowedTick=%llu "
           "position=(%.2f,%.2f,%.2f) velocity=(%.2f,%.2f,%.2f)\n",
           serverTimestamp(), shooter.id, requestId, projectile.id,
           networkWeaponTypeName(networkWeapon),
           runtime.magazineAmmo, runtime.reserveAmmo,
           (unsigned long long)runtime.nextAllowedFireTick,
           projectile.position.x, projectile.position.y, projectile.position.z,
           projectile.velocity.x, projectile.velocity.y, projectile.velocity.z);

    result.accepted = true;
    result.reason = 0;
    result.projectileId = projectile.id;
    result.magazineAmmo = runtime.magazineAmmo;
    result.reserveAmmo = runtime.reserveAmmo;
    result.nextAllowedFireTick = runtime.nextAllowedFireTick;
    result.stateRevision = runtime.stateRevision;
    return result;
}

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

#if 0
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
                rt.reserveAmmo = initialReserveAmmoForDefinition(*def);
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
    projectile.selfDamageMultiplier = cfg.selfDamageMultiplier;
    projectile.gravity = cfg.gravity;
    projectile.drag = cfg.drag;
    projectile.restitution = cfg.restitution;
    projectile.friction = cfg.friction;
    projectile.armingDistance = cfg.armingDistance;
    projectile.armingTime = cfg.armingTime;
    projectile.minBounceSpeed = cfg.minBounceSpeed;
    projectile.angularDrag = cfg.angularDrag;
    projectile.maxBounceCount = cfg.maxBounceCount;
    // Generic explosion-trigger policy captured at spawn from weapon definition
    if (wepId) {
        const WeaponDefinition* spawnDef = WeaponRegistry::instance().get(std::string(wepId));
        if (spawnDef) {
            auto spawnCp = [&](const char* key, float fb) -> float {
                auto it = spawnDef->customParams.find(key);
                return it != spawnDef->customParams.end() ? it->second : fb;
            };
            projectile.explodeOnPlayerImpact = spawnCp("explodeOnPlayerImpact", 1.0f) > 0.0f;
            projectile.explodeOnWorldImpact = spawnCp("explodeOnWorldImpact", 0.0f) > 0.0f;
            projectile.explodeOnLifetime = spawnCp("explodeOnLifetime", 1.0f) > 0.0f;
        }
    }
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
#endif
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
            projectile.weaponType == NETWORK_WEAPON_GRENADE_LAUNCHER;

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

                if (step.type == ProjectileCollisionType::LifetimeExpired && projectile.explodeOnLifetime)
                {
                    explodeProjectile(sock, world, players, npcs, projectile, projectile.position,
                                      "lifetime", 0, tick, stepTick, totalPacketsOut);
                }
                else if (step.type == ProjectileCollisionType::PlayerImpact && projectile.explodeOnPlayerImpact)
                {
                    explodeProjectile(sock, world, players, npcs, projectile, step.hitPosition,
                                      "player", step.hitPlayerId, tick, stepTick,
                                      totalPacketsOut);
                }
                else if (step.type == ProjectileCollisionType::WorldImpact && projectile.explodeOnWorldImpact)
                {
                    explodeProjectile(sock, world, players, npcs, projectile, step.hitPosition,
                                      "world", 0, tick, stepTick, totalPacketsOut);
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
}

void cancelDeadNpcProjectiles(
    SOCKET sock,
    std::unordered_map<uint32_t, ServerPlayer>& players,
    const std::unordered_map<uint32_t, ServerNpc>& npcs,
    std::unordered_map<uint32_t, ServerProjectile>& projectiles,
    uint32_t tick,
    uint64_t& totalPacketsOut)
{
    for (auto it = projectiles.begin(); it != projectiles.end(); )
    {
        const ServerProjectile& projectile = it->second;
        if (projectile.ownerNpcId == 0)
        {
            ++it;
            continue;
        }

        const auto npcIt = npcs.find(projectile.ownerNpcId);
        if (npcIt != npcs.end() && npcIt->second.health > 0)
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
        it = projectiles.erase(it);
    }
}

} // namespace MimitaNet
