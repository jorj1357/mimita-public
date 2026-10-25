#include "spawn-utils.h"
#include "world/world.h"
#include "entities/player.h"
#include "npc/npc.h"
#include "game/spawn-override.h"
#include "physics/ray-utils.h"
#include "physics/movement/physics-collision.h"
#include "debug/debug-log.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <limits>

static FILE* gSpawnDebugFile = nullptr;
static void spawnDebugOpen()
{
    if (!gSpawnDebugFile)
        gSpawnDebugFile = fopen("logs/map_spawn_debug.txt", "a");
}
static void spawnDebugClose()
{
    if (gSpawnDebugFile) {
        fclose(gSpawnDebugFile);
        gSpawnDebugFile = nullptr;
    }
}
#define SPAWNLOG(...) do { spawnDebugOpen(); if (gSpawnDebugFile) { fprintf(gSpawnDebugFile, __VA_ARGS__); fflush(gSpawnDebugFile); } } while(0)


namespace {

constexpr float kSpawnRayOriginLift = 0.5f;
constexpr float kSpawnLandingClearance = 0.02f;

glm::vec3 closestPointOnTriangle(const glm::vec3& p,
                                 const glm::vec3& a,
                                 const glm::vec3& b,
                                 const glm::vec3& c)
{
    const glm::vec3 ab = b - a;
    const glm::vec3 ac = c - a;
    const glm::vec3 ap = p - a;
    const float d1 = glm::dot(ab, ap);
    const float d2 = glm::dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f)
        return a;
    const glm::vec3 bp = p - b;
    const float d3 = glm::dot(ab, bp);
    const float d4 = glm::dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3)
        return b;
    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
        return a + ab * (d1 / (d1 - d3));
    const glm::vec3 cp = p - c;
    const float d5 = glm::dot(ab, cp);
    const float d6 = glm::dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6)
        return c;
    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
        return a + ac * (d2 / (d2 - d6));
    const float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
        return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
    const float denom = 1.0f / (va + vb + vc);
    return a + ab * (vb * denom) + ac * (vc * denom);
}

bool capsuleFitsWorld(const World& world,
                      const glm::vec3& center,
                      float radius,
                      float height)
{
    const std::array<glm::vec3, 3> samples = {
        center + glm::vec3(0.0f, 0.0f, -height * 0.5f + radius),
        center,
        center + glm::vec3(0.0f, 0.0f, height * 0.5f - radius)};
    for (const glm::vec3& sample : samples)
    {
        if (worldPointInsideSolid(world, sample))
            return false;
        for (const CollisionTriangle& triangle : world.collisionMesh.triangles)
        {
            const glm::vec3 closest =
                closestPointOnTriangle(sample, triangle.a, triangle.b, triangle.c);
            const float distance = glm::length(sample - closest);
            if (distance < radius && distance > 1.0e-5f)
                return false;
        }
    }
    return true;
}

bool raycastDownWorld(const World& world,
                      const glm::vec3& origin,
                      float maxDistance,
                      float& outZ)
{
    const glm::vec3 direction(0.0f, 0.0f, -1.0f);
    float nearest = maxDistance;
    bool found = false;
    for (const CollisionTriangle& triangle : world.collisionMesh.triangles)
    {
        float distance = 0.0f;
        if (rayTriangle(origin, direction, triangle, distance) &&
            distance < nearest)
        {
            nearest = distance;
            found = true;
        }
    }
    if (found)
        outZ = origin.z - nearest;
    return found;
}

bool raycastUpWorld(const World& world,
                    const glm::vec3& origin,
                    float maxDistance,
                    float& outDistance)
{
    const glm::vec3 direction(0.0f, 0.0f, 1.0f);
    float nearest = maxDistance;
    bool found = false;
    for (const CollisionTriangle& triangle : world.collisionMesh.triangles)
    {
        float distance = 0.0f;
        if (rayTriangle(origin, direction, triangle, distance) &&
            distance < nearest)
        {
            nearest = distance;
            found = true;
        }
    }
    if (found)
        outDistance = nearest;
    return found;
}

bool resolveCandidate(const World& world,
                      const glm::vec3& candidate,
                      float radius,
                      float height,
                      float maxDrop,
                      glm::vec3& outStanding)
{
    if (worldPointInsideSolid(world, candidate))
        return false;
    float groundZ = 0.0f;
    if (!raycastDownWorld(world, candidate + glm::vec3(0.0f, 0.0f, kSpawnRayOriginLift),
                          maxDrop, groundZ))
        return false;
    const glm::vec3 standing(candidate.x, candidate.y,
                             groundZ + height * 0.5f + kSpawnLandingClearance);
    if (worldPointInsideSolid(world, standing))
        return false;
    float headroom = 0.0f;
    if (raycastUpWorld(world, standing, height * 0.5f + 0.1f, headroom) &&
        headroom < height * 0.5f + 0.05f)
        return false;
    if (!capsuleFitsWorld(world, standing, radius, height))
        return false;
    outStanding = standing;
    return true;
}

} // namespace

bool worldPointInsideSolid(const World& world, const glm::vec3& point)
{
    const glm::vec3 direction = glm::normalize(glm::vec3(1.0f, 0.3713907f, 0.193f));
    int hits = 0;
    for (const CollisionTriangle& triangle : world.collisionMesh.triangles)
    {
        float distance = 0.0f;
        if (rayTriangle(point, direction, triangle, distance) && distance > 1.0e-4f)
            ++hits;
    }
    return (hits % 2) != 0;
}

glm::vec3 resolveSafeStandingSpawn(const World& world, const glm::vec3& desired,
                                   float radius, float height)
{
    radius = std::max(0.05f, radius);
    height = std::max(0.2f, height);
    if (world.collisionMesh.triangles.empty())
        return desired;

    const glm::vec3 extent = world.collisionMesh.boundsMax - world.collisionMesh.boundsMin;
    const float maxDrop = std::max(200.0f, extent.z + 64.0f);
    float ceilingDistance = 0.0f;
    const bool hasCeiling = raycastUpWorld(world, desired, 64.0f, ceilingDistance);
    const float maxLift = hasCeiling ? std::max(0.0f, ceilingDistance - 0.1f) : 16.0f;
    glm::vec3 resolved;
    const float lifts[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f,
                           3.0f, 4.0f, 6.0f, 8.0f, 12.0f, 16.0f};
    for (float lift : lifts)
    {
        if (lift > maxLift)
            break;
        if (resolveCandidate(world, desired + glm::vec3(0.0f, 0.0f, lift),
                             radius, height, maxDrop, resolved))
        {
            if (lift > 0.0f)
                Debug::log(Debug::Category::General,
                    "[SPAWN SAFE] raised spawn by %.2f m to (%.2f %.2f %.2f)\\n",
                    lift, resolved.x, resolved.y, resolved.z);
            return resolved;
        }
    }

    static constexpr float kRingRadii[] = {0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f};
    for (float ring : kRingRadii)
    {
        for (int step = 0; step < 8; ++step)
        {
            const float angle = (float)step * 0.7853981634f;
            const glm::vec3 offset(std::cos(angle) * ring, std::sin(angle) * ring, 0.0f);
            for (float lift : lifts)
            {
                if (resolveCandidate(world, desired + offset + glm::vec3(0.0f, 0.0f, lift),
                                     radius, height, maxDrop, resolved))
                {
                    Debug::log(Debug::Category::General,
                        "[SPAWN SAFE] moved spawn by (%.2f %.2f %.2f) to (%.2f %.2f %.2f)\\n",
                        offset.x, offset.y, lift, resolved.x, resolved.y, resolved.z);
                    return resolved;
                }
            }
        }
    }

    Debug::log(Debug::Category::General,
        "[SPAWN SAFE] no validated standing spot near (%.2f %.2f %.2f); keeping raw spawn\\n",
        desired.x, desired.y, desired.z);
    return desired;
}

glm::vec3 getSpawnPosition(const World& world, int entityIndex) {
    glm::vec3 overridePos;
    if (tryGetSpawnOverride(overridePos)) {
        Debug::log(Debug::Category::General, "[SPAWN] override active position=(%.1f %.1f %.1f)\n",
                   overridePos.x, overridePos.y, overridePos.z);
        return overridePos;
    }
    if (!world.spawnPoints.empty()) {
        int idx = entityIndex % (int)world.spawnPoints.size();
        glm::vec3 pos = world.spawnPoints[idx].position;
        printf("[SPAWN] using spawnpoint %d/%zu position=(%.2f, %.2f, %.2f)\n",
               idx, world.spawnPoints.size(), pos.x, pos.y, pos.z);
        return pos;
    }
    printf("[SPAWN] no spawnpoints found — using fallback at (0, 0, 100)\n");
    return FALLBACK_SPAWN_POS;
}

glm::vec3 spawnNpcAtSafePosition(NpcSystem& npcs, uint32_t npcId, float difficulty,
                                  const World& world, int entityIndex) {
    bool explicitSpawn = world.spawnPoints.empty() ? false : true;
    glm::vec3 basePos = getSpawnPosition(world, entityIndex);

    glm::vec3 spawnPos = basePos;
    // Skip random offset when override is active — spawn exactly at override position
    glm::vec3 _ignored;
    if (!tryGetSpawnOverride(_ignored)) {
        spawnPos += glm::vec3(
            (float)(rand() % 40 - 20) * 0.5f,
            (float)(rand() % 40 - 20) * 0.5f,
            0.0f
        );
    }

    // Clamp Z to at least 1 unit above the map surface
    // The fallback position at (0,0,100) already ensures this
    if (spawnPos.z < 1.0f && world.spawnPoints.empty())
        spawnPos.z = FALLBACK_SPAWN_POS.z;

    npcs.spawnNpc(npcId, difficulty, spawnPos);

    int spawnIdx = world.spawnPoints.empty() ? -1 : (entityIndex % (int)world.spawnPoints.size());
    printf("[SPAWN NPC] id=%u difficulty=%.1f position=(%.2f, %.2f, %.2f) "
           "spawnpoint=%d totalNpcs=%zu\n",
           npcId, difficulty, spawnPos.x, spawnPos.y, spawnPos.z,
           spawnIdx, npcs.all().size());

    SPAWNLOG("NPC spawned id=%u difficulty=%.1f explicit=%s spawn_index=%d position=(%.2f %.2f %.2f)\n",
             npcId, difficulty, explicitSpawn ? "yes" : "no", spawnIdx,
             spawnPos.x, spawnPos.y, spawnPos.z);

    return spawnPos;
}

void logSpawnDiagnostics(const World& world, const Player& player, NpcSystem& npcs) {
    printf("[SPAWN DIAG] spawnpoints=%zu player.pos=(%.2f,%.2f,%.2f) player.dead=%d player.hp=%d\n",
           world.spawnPoints.size(),
           player.pos.x, player.pos.y, player.pos.z,
           (int)player.dead, player.currentHp);

    for (size_t i = 0; i < npcs.all().size(); ++i) {
        const Npc& n = npcs.all()[i];
        printf("[SPAWN DIAG] npc[%zu] id=%u pos=(%.2f,%.2f,%.2f) dead=%d hp=%d bombHolder=%d\n",
               i, n.id, n.body.pos.x, n.body.pos.y, n.body.pos.z,
               (int)n.body.dead, n.body.currentHp,
               (int)n.bombTagHasBomb);
    }
}
