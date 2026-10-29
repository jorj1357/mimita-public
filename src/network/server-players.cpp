// 08 31 2026, 17 14
/* purpose
* Owns authoritative server player names, spawning, collision, simulation, and snapshot entity state.
* Keeps server Player lifecycle, movement parity state, and weapon spawn inventory synchronized.
* Provides small helpers used by the dedicated and listen-server loops.
* Does NOT parse client packets, render players, or define network packet layouts.
* Does NOT own weapon definitions, projectile simulation, or client prediction.
* Does NOT let stale movement sequences survive spawn, respawn, teleport, or death.
*/

#include "network/server.h"
#include "network/actor-lifecycle.h"
#include "network/server-gamemode.h"
#include "network/community-server-config.h"
#include "network/network-weapons.h"
#include "gamemode/match-roles.h"
#include "map/map-authored-geometry.h"
#include "physics/movement/movement-conversion.h"
#include "physics/movement/movement-step.h"
#include "physics/movement/physics-collision.h"
#include "physics/ray-utils.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-runtime.h"
#include "combat/weapon-types.h"
#include "combat/melee-rules-config.h"
#include "entities/aimbody-config.h"
#include "entities/player-avatar-animations.h"
#include "config/networking-config.h"
#include "config/spawn-velocity-config.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>

namespace MimitaNet {
namespace {

float safeServerSizeScale(float sizeScale)
{
    return std::isfinite(sizeScale) && sizeScale > 0.0f ? sizeScale : 1.0f;
}

void syncServerMovementRuntime(ServerPlayer& player, bool movementEnabled)
{
    player.movement.lifecycle = MovementLifecycleIdentity{
        player.spawnGeneration,
        static_cast<uint32_t>(player.transformEpoch)};
    player.movement.movementEnabled =
        movementEnabled && !player.dead && player.spawnState == ServerPlayer::Active;
    player.movement.position = player.pos;
    if (!movementIsFinite(player.movement.externalImpulse))
        player.movement.externalImpulse = glm::vec3(0.0f);
    player.movement.baseVelocity = player.vel - player.movement.externalImpulse;
    player.movement.lastInputMoveAxes = movementClampUnitOrZero(player.input.wish);
    player.movement.yaw = player.yaw;
    player.movement.sizeScale = safeServerSizeScale(player.sizeScale);
    player.movement.ground.onGround = player.onGround;
    player.movement.ground.stableOnGround = player.onGround ||
        player.movement.ground.stableOnGround;
    player.movement.ground.hasWorldContact = player.onGround;
    player.movement.dash.dashAvailable = player.dashAvailable;
}

glm::vec3 resolveDeveloperFlightWorldMove(const HeadlessWorld& world,
                                          glm::vec3 start,
                                          glm::vec3 movement,
                                          glm::vec3& velocity)
{
    constexpr float kWallTolerance = 0.35f;
    glm::vec3 position = start;
    glm::vec3 remaining = movement;
    for (int contact = 0; contact < 3; ++contact)
    {
        const float distance = glm::length(remaining);
        if (distance <= 1e-5f)
            break;
        glm::vec3 normal(0.0f);
        float hitDistance = 0.0f;
        const glm::vec3 target = position + remaining;
        if (!crossesBlockingGeometry(&world, position, target,
                                     kWallTolerance, &normal, &hitDistance,
                                     true))
        {
            position = target;
            break;
        }

        const glm::vec3 direction = remaining / distance;
        const float advance = std::clamp(
            hitDistance - kWallTolerance, 0.0f, distance);
        position += direction * advance;
        remaining -= direction * advance;
        const float intoSurface = glm::dot(remaining, normal);
        if (intoSurface < 0.0f)
            remaining -= normal * intoSurface;
        const float velocityIntoSurface = glm::dot(velocity, normal);
        if (velocityIntoSurface < 0.0f)
            velocity -= normal * velocityIntoSurface;
    }
    return position;
}

// Packet-out counter for reliable spawn-sync delivery (stats only; the
// reliable-event queue increments it while enqueueing/sending).
uint64_t gSpawnSyncTotalPacketsOut = 0;

} // namespace

void copyName(char (&dst)[MAX_NAME_BYTES], const std::string& name)
{
    std::memset(dst, 0, sizeof(dst));
    std::strncpy(dst, name.c_str(), sizeof(dst) - 1);
}

std::string uniquePlayerName(
    const std::unordered_map<uint32_t, ServerPlayer>& players,
    const std::string& requested,
    uint32_t ownId)
{
    const std::string base = requested.empty() ? "player" + std::to_string(ownId) : requested;
    std::string candidate = base;
    int suffix = 2;
    for (;;)
    {
        bool used = false;
        for (const auto& kv : players)
        {
            if (kv.first != ownId && kv.second.name == candidate)
            {
                used = true;
                break;
            }
        }
        if (!used)
            return candidate;
        candidate = base + "(" + std::to_string(suffix++) + ")";
    }
}

glm::vec3 closestPointTriangle(glm::vec3 p, glm::vec3 a, glm::vec3 b, glm::vec3 c)
{
    glm::vec3 ab = b - a;
    glm::vec3 ac = c - a;
    glm::vec3 ap = p - a;
    float d1 = glm::dot(ab, ap);
    float d2 = glm::dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) return a;
    glm::vec3 bp = p - b;
    float d3 = glm::dot(ab, bp);
    float d4 = glm::dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) return b;
    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
    {
        float v = d1 / (d1 - d3);
        return a + v * ab;
    }
    glm::vec3 cp = p - c;
    float d5 = glm::dot(ab, cp);
    float d6 = glm::dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) return c;
    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 >= 0.0f)
    {
        float w = d2 / (d2 - d6);
        return a + w * ac;
    }
    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
    {
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return b + w * (c - b);
    }
    float denom = 1.0f / (va + vb + vc);
    float v = vb * denom;
    float w = vc * denom;
    return a + ab * v + ac * w;
}

void resolveWorldCollision(ServerPlayer& p, const HeadlessWorld& world)
{
    p.onGround = false;

    for (int pass = 0; pass < 3; ++pass)
    {
        glm::vec3 samples[3] = {
            p.pos + glm::vec3(0, 0, -PLAYER_HEIGHT * 0.5f + PLAYER_RADIUS),
            p.pos,
            p.pos + glm::vec3(0, 0, PLAYER_HEIGHT * 0.5f - PLAYER_RADIUS)
        };

        for (glm::vec3 sample : samples)
        {
            // Broadphase: gather only triangles near the sample point
            AABB queryBounds;
            queryBounds.min = sample - glm::vec3(PLAYER_RADIUS + 0.1f);
            queryBounds.max = sample + glm::vec3(PLAYER_RADIUS + 0.1f);
            thread_local std::vector<int> s_candidates;
            s_candidates.clear();
            gatherHeadlessTrianglesForAABB(world, queryBounds, PLAYER_RADIUS * 0.1f, s_candidates);

            for (int triIdx : s_candidates)
            {
                if (triIdx < 0 || triIdx >= (int)world.triangles.size())
                    continue;
                const CollisionTriangle& tri = world.triangles[triIdx];

                glm::vec3 cp = closestPointTriangle(sample, tri.a, tri.b, tri.c);
                glm::vec3 delta = sample - cp;
                float dist = glm::length(delta);
                if (dist >= PLAYER_RADIUS || dist < 0.00001f)
                    continue;

                glm::vec3 n = delta / dist;
                if (glm::dot(n, tri.normal) < 0.0f)
                    n = -n;
                float penetration = PLAYER_RADIUS - dist;
                p.pos += n * (penetration + 0.001f);
                float into = glm::dot(p.vel, n);
                if (into < 0.0f)
                    p.vel -= n * into;
                if (n.z > 0.35f)
                    p.onGround = true;
            }
        }
    }

    // Debug log when a real triangle collision resolves below the map bounds
    if (p.pos.z < world.boundsMin.z)
    {
        static uint64_t lastBelowBoundsCollisionLogMs = 0;
        uint64_t nowBc = nowMs();
        if (nowBc - lastBelowBoundsCollisionLogMs >= 1000)
        {
            printf("%s [SERVER BELOW-MAP COLLISION] playerId=%u "
                   "posZBefore=%.2f posZAfter=%.2f onGround=%d\n",
                   serverTimestamp(), p.id,
                   p.pos.z, p.pos.z, (int)p.onGround);
            lastBelowBoundsCollisionLogMs = nowBc;
        }
    }

    // Note: the world.boundsMin.z global floor clamp has been intentionally
    // removed.  world.boundsMin is map metadata, not collision geometry.
    // Players below bounds must continue falling naturally until the
    // void-death threshold.  Actual collision triangles handle platforms.

    // Approximate weapon-vs-world collision. The client already pushes its
    // animated weapon out of geometry; this keeps the authoritative server
    // position from accepting a player whose held weapon is embedded in a wall.
    // This pass is capsule/box approximation. The future triangle mode may
    // replace it with cached exact weapon triangles.
    if (p.dead || p.equippedWeaponId.empty()) return;
    const WeaponDefinition* weapon = WeaponRegistry::instance().get(p.equippedWeaponId);
    if (!weapon || weapon->id == "nothing") return;
    auto param = [&](const char* key, float fallback) {
        const auto it = weapon->customParams.find(key);
        return it != weapon->customParams.end() ? it->second : fallback;
    };
    const float radius = std::max(0.05f,
        param("contactBladeRadius", param("bladeRadius", 0.2f)));
    const float length = std::max(0.2f,
        param("bladeLength", param("contactBladeLength", 2.5f)));
    glm::vec3 dir = p.input.camForward;
    if (glm::length(dir) < 0.001f)
        dir = glm::vec3(std::cos(p.yaw), std::sin(p.yaw), 0.0f);
    dir = glm::normalize(dir);
    const glm::vec3 base = p.pos + dir * 0.2f;
    const glm::vec3 tip = p.pos + dir * length;
    thread_local std::vector<int> candidates;
    for (int sampleIndex = 0; sampleIndex <= 4; ++sampleIndex) {
        const glm::vec3 sample = glm::mix(base, tip, (float)sampleIndex / 4.0f);
        AABB queryBounds;
        queryBounds.min = sample - glm::vec3(radius + 0.05f);
        queryBounds.max = sample + glm::vec3(radius + 0.05f);
        candidates.clear();
        gatherHeadlessTrianglesForAABB(world, queryBounds, radius * 0.1f, candidates);
        for (int triIdx : candidates) {
            if (triIdx < 0 || triIdx >= (int)world.triangles.size()) continue;
            const CollisionTriangle& tri = world.triangles[triIdx];
            const glm::vec3 cp = closestPointTriangle(sample, tri.a, tri.b, tri.c);
            const glm::vec3 delta = sample - cp;
            const float dist = glm::length(delta);
            if (dist >= radius || dist < 0.00001f) continue;
            glm::vec3 n = delta / dist;
            if (glm::dot(n, tri.normal) < 0.0f) n = -n;
            p.pos += n * (radius - dist + 0.001f);
            const float into = glm::dot(p.vel, n);
            if (into < 0.0f) p.vel -= n * into;
        }
    }
}

static bool serverPointInsideWorld(const HeadlessWorld& world,
                                   const glm::vec3& point)
{
    const glm::vec3 direction =
        glm::normalize(glm::vec3(1.0f, 0.3713907f, 0.193f));
    int hits = 0;
    for (const CollisionTriangle& triangle : world.triangles)
    {
        float distance = 0.0f;
        if (rayTriangle(point, direction, triangle, distance) &&
            distance > 1.0e-4f)
            ++hits;
    }
    return (hits % 2) != 0;
}

// Returns true when a player capsule centered at `pos` does not penetrate any
// world triangle. Uses the same sample-based capsule check as
// resolveWorldCollision, so a spawn spot passes iff the real collision solver
// would leave the player standing there instead of wedging it.
static bool serverCapsuleFits(const HeadlessWorld& world, const glm::vec3& pos)
{
    glm::vec3 samples[3] = {
        pos + glm::vec3(0.0f, 0.0f, -PLAYER_HEIGHT * 0.5f + PLAYER_RADIUS),
        pos,
        pos + glm::vec3(0.0f, 0.0f, PLAYER_HEIGHT * 0.5f - PLAYER_RADIUS)
    };
    for (glm::vec3 sample : samples)
    {
        if (serverPointInsideWorld(world, sample))
            return false;
        AABB queryBounds;
        queryBounds.min = sample - glm::vec3(PLAYER_RADIUS + 0.1f);
        queryBounds.max = sample + glm::vec3(PLAYER_RADIUS + 0.1f);
        thread_local std::vector<int> s_candidates;
        s_candidates.clear();
        gatherHeadlessTrianglesForAABB(world, queryBounds, PLAYER_RADIUS * 0.1f, s_candidates);
        for (int triIdx : s_candidates)
        {
            if (triIdx < 0 || triIdx >= (int)world.triangles.size())
                continue;
            const CollisionTriangle& tri = world.triangles[triIdx];
            glm::vec3 cp = closestPointTriangle(sample, tri.a, tri.b, tri.c);
            float dist = glm::length(sample - cp);
            if (dist < PLAYER_RADIUS && dist >= 0.00001f)
                return false;
        }
    }
    return true;
}

// Resolves a candidate into a standable spot: raycast down to the nearest
// surface and verify the ground-snapped standing capsule fits. Authored spawn
// points are commonly placed at floor level, so the horizontal capsule check is
// done at the snapped standing center, not at the raw point. Returns the
// ground-snapped standing center in outStanding.
static bool serverValidateStandingSpot(
    const HeadlessWorld& world, const glm::vec3& cand,
    glm::vec3& outStanding,
    const std::chrono::steady_clock::time_point* deadline = nullptr)
{
    const float maxDist =
        std::max(200.0f, (world.boundsMax.z - world.boundsMin.z) + 64.0f);

    glm::vec3 upHit;
    glm::vec3 upNormal;
    const bool hasCeiling =
        serverRaycastWorld(cand + glm::vec3(0.0f, 0.0f, 0.1f),
                           glm::vec3(0.0f, 0.0f, 1.0f), 64.0f, world,
                           upHit, upNormal);
    const float ceilingZ = hasCeiling ? upHit.z : cand.z + 64.0f;

    const float lifts[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f,
                           3.0f, 4.0f, 6.0f, 8.0f, 12.0f, 16.0f};
    const float rings[] = {0.0f, 0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f};
    for (float ring : rings)
    {
        const int directions = ring <= 0.0f ? 1 : 8;
        for (int directionIndex = 0; directionIndex < directions; ++directionIndex)
        {
            const float angle = directions == 1
                ? 0.0f : (float)directionIndex * 0.7853981634f;
            const glm::vec3 offset(std::cos(angle) * ring,
                                   std::sin(angle) * ring, 0.0f);
            for (float lift : lifts)
            {
                if (deadline && std::chrono::steady_clock::now() >= *deadline)
                    return false;
                if (lift > 0.0f && cand.z + lift >= ceilingZ - 0.05f)
                    break;
                const glm::vec3 candidate = cand + offset +
                    glm::vec3(0.0f, 0.0f, lift);
                if (serverPointInsideWorld(world, candidate))
                    continue;

                glm::vec3 hit;
                glm::vec3 normal;
                if (!serverRaycastWorld(
                        candidate + glm::vec3(0.0f, 0.0f, 0.5f),
                        glm::vec3(0.0f, 0.0f, -1.0f), maxDist, world,
                        hit, normal))
                    continue;

                const glm::vec3 standing(
                    candidate.x, candidate.y,
                    hit.z + PLAYER_HEIGHT * 0.5f + 0.02f);
                if (standing.z + PLAYER_HEIGHT * 0.5f >= ceilingZ - 0.05f)
                    continue;
                if (serverPointInsideWorld(world, standing))
                    continue;
                if (!serverCapsuleFits(world, standing))
                    continue;

                outStanding = standing;
                return true;
            }
        }
    }
    return false;
}

// Derives a validated standing spawn by probing the collision world on a grid
// when the map has no authored spawn points (or none are safe). Returns the
// probe nearest to `hint` in the horizontal plane.
static bool serverProbeSafeStanding(const HeadlessWorld& world, const glm::vec3& hint,
                                    glm::vec3& outPos, float& outYaw)
{
    const glm::vec3 mn = world.boundsMin;
    const glm::vec3 mx = world.boundsMax;
    if (!(mx.x > mn.x) || !(mx.y > mn.y) || !(mx.z > mn.z))
        return false;

    // A join must not scan the whole collision world. Probe a bounded local
    // area around the hint and stop at a small time budget; the caller has a
    // validated fallback path if this cannot find a spot quickly.
    constexpr float kProbeRadius = 32.0f;
    constexpr float kProbeStep = 4.0f;
    constexpr auto kProbeBudget = std::chrono::milliseconds(250);
    const auto probeDeadline = std::chrono::steady_clock::now() + kProbeBudget;
    const float x0 = std::max(mn.x, hint.x - kProbeRadius);
    const float x1 = std::min(mx.x, hint.x + kProbeRadius);
    const float y0 = std::max(mn.y, hint.y - kProbeRadius);
    const float y1 = std::min(mx.y, hint.y + kProbeRadius);
    const float maxDrop = (mx.z - mn.z) + 16.0f;
    float bestDistSq = std::numeric_limits<float>::max();
    bool found = false;

    for (float x = x0; x <= x1; x += kProbeStep)
    {
        for (float y = y0; y <= y1; y += kProbeStep)
        {
            if (std::chrono::steady_clock::now() >= probeDeadline)
                return found;
            const glm::vec3 origin(x, y, std::max(mn.z, mx.z - 0.5f));
            glm::vec3 hit, nrm;
            if (!serverRaycastWorld(origin, glm::vec3(0.0f, 0.0f, -1.0f), maxDrop, world, hit, nrm))
                continue;
            const glm::vec3 cand(x, y, hit.z + PLAYER_HEIGHT * 0.5f);
            glm::vec3 standing;
            if (!serverValidateStandingSpot(world, cand, standing, &probeDeadline))
                continue;
            const float dx = standing.x - hint.x;
            const float dy = standing.y - hint.y;
            const float d = dx * dx + dy * dy;
            if (d < bestDistSq)
            {
                bestDistSq = d;
                outPos = standing;
                outYaw = 0.0f;
                found = true;
            }
        }
    }
    return found;
}

bool serverPickSafeSpawn(const HeadlessWorld* world, uint32_t playerId,
                         glm::vec3& outPos, float& outYaw, size_t* outIndex)
{
    if (outIndex)
        *outIndex = 0;

    const std::string playerKey = std::to_string(playerId);
    const auto emit = [&](const char* source, long idx, bool validated) {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Network, StructuredLevel::Important,
            "spawn.server-selected", playerKey,
            "authoritative spawn selection", 0,
            {{"source", source},
             {"spawn_index", idx},
             {"position", {outPos.x, outPos.y, outPos.z}},
             {"yaw_deg", glm::degrees(outYaw)},
             {"spawn_points", (long)(world ? world->spawnPoints.size() : 0)},
             {"validated", validated}},
            __FILE__, __LINE__, __FUNCTION__);
    };

    // Host spawn override: the host explicitly chose this spot — trust it.
    const ServerGameOverrides& ov = serverGameOverrides();
    if (ov.spawnOverrideEnabled)
    {
        outPos = ov.spawnOverridePosition;
        if (world && !world->spawnPoints.empty())
            outYaw = world->spawnPoints[(playerId - 1) % world->spawnPoints.size()].yaw;
        else
            outYaw = 0.0f;
        emit("host-override", -1, false);
        return true;
    }

    if (world && !world->spawnPoints.empty())
    {
        const std::vector<ServerSpawnPoint>& points = world->spawnPoints;
        const size_t defaultIdx = (playerId - 1) % points.size();

        for (size_t k = 0; k < points.size(); ++k)
        {
            const size_t idx = (defaultIdx + k) % points.size();

            // The player must be able to STAND after spawning: snap to ground
            // and verify the capsule fits there. This rejects spawns that drop
            // the player into a narrow shaft/spire where the capsule cannot
            // move (the "stuck at spawn" bug).
            glm::vec3 standing;
            if (!serverValidateStandingSpot(*world, points[idx].position, standing))
                continue;

            outPos = standing;  // ground-snapped so the capsule is not embedded
            outYaw = points[idx].yaw;
            if (outIndex)
                *outIndex = idx;
            emit("authored", (long)idx, true);
            return true;
        }
        // No authored point validated. Search the collision surface near the
        // authored anchor instead of returning a point that may be embedded in
        // a ramp or wall.
        glm::vec3 probePos;
        float probeYaw = 0.0f;
        if (serverProbeSafeStanding(*world, points[defaultIdx].position,
                                    probePos, probeYaw))
        {
            outPos = probePos;
            outYaw = probeYaw;
            if (outIndex)
                *outIndex = (size_t)-1;
            emit("derived-authored-probe", (long)defaultIdx, true);
            return true;
        }

        // Join reliability fallback: the authored point is the map author's
        // explicit spawn. If validation/probing cannot improve it within the
        // bounded budget, admit the player there instead of making the mode
        // wait forever for an active client. The event marks it unvalidated so
        // the unsafe-spawn condition remains visible in the journal.
        outPos = points[defaultIdx].position;
        outYaw = points[defaultIdx].yaw;
        if (outIndex)
            *outIndex = defaultIdx;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Network, StructuredLevel::Errors,
            "spawn.authored-unvalidated", playerKey,
            "authored spawn used after bounded validation/probe failed", 0,
            {{"spawn_index", (long)defaultIdx},
             {"position", {outPos.x, outPos.y, outPos.z}},
             {"spawn_points", (long)points.size()},
             {"validation", "unvalidated-fallback"}},
            __FILE__, __LINE__, __FUNCTION__);
        return true;
    }

    // Derive a validated standing position from collision geometry. This covers
    // maps authored with no spawn nodes and maps whose authored points are all
    // embedded, so the policy still has a safe answer.
    if (world)
    {
        const glm::vec3 hint = world->spawnPoints.empty()
            ? glm::vec3((world->boundsMin.x + world->boundsMax.x) * 0.5f,
                        (world->boundsMin.y + world->boundsMax.y) * 0.5f, 0.0f)
            : world->spawnPoints[0].position;
        glm::vec3 probePos;
        float probeYaw = 0.0f;
        if (serverProbeSafeStanding(*world, hint, probePos, probeYaw))
        {
            outPos = probePos;
            outYaw = probeYaw;
            if (outIndex)
                *outIndex = (size_t)-1;  // derived, not an authored index
            emit("derived-probe", -1, true);
            return true;
        }

        // Last resort: the historical default, but only if it validates.
        const glm::vec3 fallback(1.0f + (float)(playerId - 1) * 1.5f, 5.0f, 30.0f);
        glm::vec3 standing;
        if (serverValidateStandingSpot(*world, fallback, standing))
        {
            outPos = fallback;
            outYaw = 0.0f;
            if (outIndex)
                *outIndex = 0;
            emit("default-validated", -1, true);
            return true;
        }
    }

    // No safe spawn exists. Refuse visibly; the caller must not place the actor
    // at a stale or out-of-world coordinate.
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Network, StructuredLevel::Errors,
        "spawn.server-no-safe-spawn", playerKey,
        "no spawn point or derived standing position passed validation", 0,
        {{"spawn_points", (long)(world ? world->spawnPoints.size() : 0)},
         {"has_world", (bool)(world != nullptr)}},
        __FILE__, __LINE__, __FUNCTION__);
    Debug::error(Debug::Category::Networking,
        "[SPAWN SAFETY] playerId=%u no safe spawn found (spawnPoints=%zu) — refusing spawn\n",
        playerId, world ? world->spawnPoints.size() : 0);
    return false;
}

// ── Initial inventory ────────────────────────────────────────────────
// Community servers derive the inventory from the effective weapon set. The
// role's weapon_set (resolved by the caller) wins; otherwise the gamemode's
// community set applies. Set 5 is the explicit opt-in for restricted weapons.
static void getInitialInventory(std::vector<std::string>& out, int setId)
{
    out.clear();
    const bool community = serverGamemodeState().enabled;
    for (const auto& kv : WeaponRegistry::instance().all())
    {
        if (!community) {
            if (!kv.second.restricted) out.push_back(kv.first);
            continue;
        }
        if (setId <= 0) {
            // Legacy path: no explicit set, use the gamemode's allowed filter.
            const bool includeRestricted =
                serverGamemodeState().communityWeaponSetId == 5;
            if ((includeRestricted || !kv.second.restricted) &&
                serverCommunityWeaponAllowed(kv.first))
                out.push_back(kv.first);
            continue;
        }
        CommunityServerConfig& config = CommunityServerConfig::instance();
        if (config.weaponSets().empty()) config.load();
        const bool includeRestricted = setId == 5;
        if ((includeRestricted || !kv.second.restricted) &&
            config.weaponAllowed(setId, kv.first))
            out.push_back(kv.first);
    }
}

// ── Centralized spawn/respawn reset ──────────────────────────────────
void resetPlayerForSpawn(ServerPlayer& player, bool isInitialSpawn)
{
    // Health and death. A self-only healthme override applies to this player
    // only; the host-wide healthall override remains the stronger global one.
    player.dead = false;
    // Melee: reset the percent meter and any in-progress action on spawn.
    serverMeleeResetForSpawn(player);
    const ActorSpawnProfile profile = serverResolveActorSpawnProfile(player.id);
    const int overrideHp = serverGameOverrides().maxHpOverride;
    const int maxHp = overrideHp > 0 ? overrideHp
                   : (player.healthOverrideEnabled ? player.healthOverrideValue
                   : (profile.health > 0 ? profile.health : 100));
    player.maxHealth = maxHp;
    player.health = maxHp;
    if (!profile.avatarName.empty())
        player.avatarName = profile.avatarName;
    // The active mode's allow-list is authoritative for players as well as
    // NPCs. Never let a rejected/manual avatar survive a spawn.
    if (profile.hasRole && !player.avatarName.empty() &&
        !isAvatarAllowedByPolicy(player.avatarName, profile.allowedAvatars, profile.deniedAvatars)) {
        std::string fallback = profile.avatarName;
        if (fallback.empty() || !isAvatarAllowedByPolicy(
                fallback, profile.allowedAvatars, profile.deniedAvatars)) {
            fallback = profile.allowedAvatars.empty() ? std::string() : profile.allowedAvatars.front();
        }
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Avatar, StructuredLevel::Important,
            "player.avatar.policy_fallback", player.avatarName,
            "spawn replaced avatar outside the active mode policy",
            (uint32_t)player.spawnGeneration,
            nlohmann::json{{"player_id", player.id},
                           {"rejected", player.avatarName},
                           {"selected", fallback},
                           {"role", profile.roleId}},
            __FILE__, __LINE__, __FUNCTION__);
        player.avatarName = fallback;
    }
    // Per-avatar movement capability (roll/fly/crawl) from the avatar rig.
    if (!player.avatarName.empty()) {
        const AvatarRig* rig =
            AvatarAnimationRegistry::instance().getRig(player.avatarName);
        if (rig) player.movementCap = rig->movement;
    }
    // Role movement identity persists for this life; used by the shared kernel.
    player.movementProfileId = profile.movementPreset;
    player.actorPresetId = profile.actorPresetId;
    serverMarkActorPresetApplied(player.id, profile.actorPresetId);
    player.actorCollisionPreviousPos = player.pos;
    player.hasActorCollisionPreviousPos = true;
    player.actorCollisionSupportKey = 0;
    player.actorCollisionSupportVelocity = glm::vec3(0.0f);
    player.actorCollisionChanged = false;

    // Increment spawn generation (never decremented, never reset)
    ++player.spawnGeneration;

    // Community matches own the active loadout. Rebuild it on every life so a
    // restricted GUI-selected set cannot be bypassed by a prior unrestricted
    // inventory. Offline/non-community spawns retain their existing policy.
    if (isInitialSpawn || serverGamemodeState().enabled)
    {
        // Role weapon set wins over the gamemode's community set for this life.
        if (serverGamemodeState().enabled)
            player.weaponSetId = profile.weaponSetId > 0
                ? profile.weaponSetId
                : serverGamemodeState().communityWeaponSetId;
        else
            player.weaponSetId = 0;
        player.ownedWeaponIds.clear();
        getInitialInventory(player.ownedWeaponIds, player.weaponSetId);
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Weapons, StructuredLevel::Important,
            "actor.loadout.resolved", std::to_string(player.id),
            isInitialSpawn ? "initial spawn loadout resolved" : "respawn loadout resolved",
            (uint32_t)player.spawnGeneration,
            nlohmann::json{{"actor", player.id}, {"kind", "player"},
                           {"weapon_set_id", player.weaponSetId},
                           {"weapon_count", player.ownedWeaponIds.size()},
                           {"weapons", player.ownedWeaponIds}},
            __FILE__, __LINE__, __FUNCTION__);
        // A declared disaster owns the per-actor loadout for this life. The
        // assignment is deterministic from the replicated round seed, so this
        // is a data lookup, not a mode-name branch.
        if (const std::string* disasterWeapon =
                MimitaGamemode::disasterWeaponForActor(serverGamemodeState().disaster, player.id)) {
            player.ownedWeaponIds.clear();
            player.ownedWeaponIds.push_back(*disasterWeapon);
        }
        player.weaponRuntimes.clear();
    }

    // Reset weapon runtimes for every owned weapon
    for (const std::string& wepId : player.ownedWeaponIds)
    {
        const WeaponDefinition* def = WeaponRegistry::instance().get(wepId);
        if (!def)
            continue;

        ServerPlayer::ServerWeaponRuntime rt;
        rt.magazineAmmo = def->magazineSize;
        rt.reserveAmmo = initialReserveAmmoForDefinition(*def);
        // Role-owned starting ammo overrides (Zombie Tower loadout contract).
        auto ammoIt = profile.weaponAmmo.find(wepId);
        if (ammoIt != profile.weaponAmmo.end()) {
            if (ammoIt->second.first >= 0) rt.magazineAmmo = ammoIt->second.first;
            if (ammoIt->second.second >= 0) rt.reserveAmmo = ammoIt->second.second;
        }
        rt.nextAllowedFireTick = 0;  // cooldown cleared — new life may fire immediately
        rt.reloading = false;
        rt.reloadCompleteTick = 0;
        rt.stateRevision = 0;
        rt.initialized = true;
        player.weaponRuntimes[wepId] = rt;
    }

    // The server owns the equipped item identity for this life. Preserve and
    // reapply the previous stable item when it is still owned and allowed by
    // the new life/loadout; otherwise fall back to the role's documented
    // starting weapon. The native slot is derived from the chosen stable id
    // for replication/HUD and must not itself decide equip or attack validity.
    const std::string previousEquipped = player.equippedWeaponId;
    player.equippedWeaponId.clear();
    player.equippedSlot = 0;
    auto applyEquippedById = [&](const std::string& weaponId) -> bool {
        if (weaponId.empty())
            return false;
        const WeaponDefinition* def = WeaponRegistry::instance().get(weaponId);
        const auto runtime = player.weaponRuntimes.find(weaponId);
        if (!def || runtime == player.weaponRuntimes.end() ||
            !runtime->second.initialized)
            return false;
        if (player.weaponSetId > 0 &&
            !serverCommunityWeaponAllowed(player, def->id))
            return false;
        player.equippedWeaponId = def->id;
        player.equippedSlot = def->slot;
        return true;
    };
    // A new game starts with empty hands. Preserve the previous stable item
    // only across respawns; do not apply a role starting weapon on the first
    // spawn. NPC loadouts still use their own startingWeapon path.
    if (!isInitialSpawn) {
        (void)applyEquippedById(previousEquipped);
        (void)applyEquippedById(profile.startingWeapon);
    }
    if (!isInitialSpawn && player.equippedWeaponId.empty() &&
        !profile.startingWeapon.empty()) {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Weapons, StructuredLevel::Errors,
            "weapon.spawn.starting_weapon_invalid", std::to_string(player.id),
            "role-starting-weapon-not-in-authoritative-loadout", 0,
            nlohmann::json{{"weapon", profile.startingWeapon},
                           {"previous_equipped", previousEquipped},
                           {"weapon_set", player.weaponSetId},
                           {"spawn_generation", player.spawnGeneration}});
    }

    // Clear transient combat/reload state
    player.projectileFireCooldown = 0.0f;
    player.nextProjectileFireTick = 0;
    player.lastShotSerial = 0;
    player.lastProjectileFireSerial = 0;
    player.meleeNextAllowedTick = 0;
    player.meleeClaimedTargetTicks.clear();
    player.meleeClaimedTargetTickOrder.clear();
    player.godballActive = false;
    player.godballX = player.godballY = player.godballZ = 0.0f;
    player.godballVx = player.godballVy = player.godballVz = 0.0f;
    player.hasLastPhysicalWeaponShape = false;
    player.lastPhysicalWeaponDefNetworkId = 0;
    player.nextPhysicalContactSerial = 1;
    player.physicalContactEpisodes.clear();
    player.input = {};
    player.inputStateFlags = 0;
    player.attackQueued = false;
    player.clientStateUpdated = false;
    player.dashAvailable = true;
    player.onGround = false;
    resetServerMovementForAuthoritativeLifecycle(
        player, makeCurrentRuntimeMovementConfig());

    // Hard-reset broadcast interpolation so a fresh life starts at the spawn
    // position instead of lerping from the previous life.
    player.hasBroadcastTransform = false;
    player.hasInterpSegment = false;
    player.interpFromPos = player.pos;
    player.interpToPos = player.pos;
    player.interpToTick = 0;
    player.interpDurationTicks = 2;
    player.interpSegmentStartTick = 0;
    player.hasSimBroadcastPos = false;
    player.simBroadcastPos = player.pos;

    printf("[SERVER SPAWN RESET] playerId=%u spawnGeneration=%u ownedWeapons=%zu"
           " isInitialSpawn=%d maxHealth=%d weaponSet=%d role=%s\n",
           player.id, player.spawnGeneration, player.ownedWeaponIds.size(),
           (int)isInitialSpawn, player.maxHealth, player.weaponSetId,
           profile.hasRole ? profile.roleId.c_str() : "none");
}

// ── Complete authoritative spawn and notify client ────────────────────
// Called from every spawn path: initial spawn, auto-respawn, instant-respawn.
// Sends PlayerRespawnedPacket with authoritative generation and inventory.
void completeAuthoritativeSpawn(SOCKET sock, ServerPlayer& player, bool isInitialSpawn)
{
    resetPlayerForSpawn(player, isInitialSpawn);
    player.spawnState = ServerPlayer::AwaitingSpawnAck;
    resetServerMovementForAuthoritativeLifecycle(
        player, makeCurrentRuntimeMovementConfig());

    ActorSpawnEvent lifecycleEvent;
    lifecycleEvent.entityId = player.id;
    lifecycleEvent.actorKind = ActorKind::Player;
    lifecycleEvent.reason = isInitialSpawn
        ? ActorSpawnReason::InitialJoin : ActorSpawnReason::Respawn;
    lifecycleEvent.spawnGeneration = player.spawnGeneration;
    lifecycleEvent.transformEpoch = player.transformEpoch;
    lifecycleEvent.position = player.pos;
    lifecycleEvent.lookDirection = glm::vec3(std::cos(player.yaw),
                                             std::sin(player.yaw), 0.0f);
    lifecycleEvent = finalizeActorSpawn(lifecycleEvent, player.name.c_str());
    player.yaw = std::atan2(lifecycleEvent.lookDirection.y,
                            lifecycleEvent.lookDirection.x);
    player.vel = lifecycleEvent.velocity;
    resetServerMovementForAuthoritativeLifecycle(
        player, makeCurrentRuntimeMovementConfig());

    PlayerRespawnedPacket spawnSync{};
    spawnSync.header.type = PACKET_PLAYER_RESPAWNED;
    spawnSync.header.tick = 0;
    spawnSync.header.playerId = player.id;
    // Reliable-event id/session stamped by the reliable transport so the
    // client can dedup retransmitted copies; the packet itself is queued
    // through the reliable-event system (same as attack/shot events), so a
    // dropped spawn sync under packet loss is re-sent until acknowledged.
    spawnSync.eventId = nextReliableGameplayEventId();
    spawnSync.eventSessionId = 0;  // per-player session stamped by the queue
    spawnSync.spawnGeneration = player.spawnGeneration;
    spawnSync.transformEpoch = player.transformEpoch;
    spawnSync.posX = player.pos.x;
    spawnSync.posY = player.pos.y;
    spawnSync.posZ = player.pos.z;
    spawnSync.velX = player.vel.x;
    spawnSync.velY = player.vel.y;
    spawnSync.velZ = player.vel.z;
    spawnSync.health = player.health;
    spawnSync.communityWeaponSetId = serverGamemodeState().enabled
        ? static_cast<uint8_t>(std::clamp(player.weaponSetId, 0, 255)) : 0;
    if (!player.equippedWeaponId.empty() &&
        player.weaponRuntimes.find(player.equippedWeaponId) != player.weaponRuntimes.end()) {
        spawnSync.equippedWeaponDefNetworkId =
            weaponDefNetworkIdFor(player.equippedWeaponId);
    } else {
        for (const auto& weapon : WeaponRegistry::instance().all()) {
            if (weapon.second.slot == player.equippedSlot &&
                player.weaponRuntimes.find(weapon.first) != player.weaponRuntimes.end()) {
                spawnSync.equippedWeaponDefNetworkId =
                    weaponDefNetworkIdFor(weapon.first);
                break;
            }
        }
    }
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Weapons, StructuredLevel::Important,
        "weapon.spawn.equipped_published", std::to_string(player.id),
        "authoritative-equipped-identity-for-life",
        static_cast<uint32_t>(player.spawnGeneration),
        nlohmann::json{{"spawn_generation", player.spawnGeneration},
                       {"weapon_id", player.equippedWeaponId},
                       {"native_slot", player.equippedSlot},
                       {"weapon_def_network_id", spawnSync.equippedWeaponDefNetworkId},
                       {"initial_spawn", isInitialSpawn},
                       {"weapon_set", player.weaponSetId}});
    Debug::log(Debug::Category::Duel,
        "[DuelPacketSend] type=PlayerRespawnedPacket reliable=1 player=%u spawnGeneration=%u epoch=%u pos=(%.3f,%.3f,%.3f) velocity=(%.3f,%.3f,%.3f)\n",
        player.id, spawnSync.spawnGeneration, spawnSync.transformEpoch,
        player.pos.x, player.pos.y, player.pos.z,
        player.vel.x, player.vel.y, player.vel.z);
    spawnSync.weaponCount = 0;
    for (const auto& wkv : player.weaponRuntimes)
    {
        if (spawnSync.weaponCount >= 16) break;
        uint16_t wid = weaponDefNetworkIdFor(wkv.first);
        if (wid == 0) continue;
        auto& ws = spawnSync.weapons[spawnSync.weaponCount++];
        ws.weaponDefNetworkId = wid;
        ws.magazineAmmo = wkv.second.magazineAmmo;
        ws.reserveAmmo = wkv.second.reserveAmmo;
        ws.nextAllowedFireTick = wkv.second.nextAllowedFireTick;
        ws.stateRevision = wkv.second.stateRevision;
        ws.reloading = wkv.second.reloading ? 1 : 0;
    }

    // Reliable delivery to this player only: retransmitted by the reliable
    // event system until the client ACKs the event (or TTL/attempts expire).
    ReliableGameplayEventQueueResult qr = queueReliableGameplayEventToPlayer(
        sock, player, &spawnSync, sizeof(spawnSync),
        spawnSync.eventId, reliableGameplayEventSessionForPlayer(player),
        gSpawnSyncTotalPacketsOut);
    const char* queueResult = qr == ReliableGameplayEventQueueResult::Queued
        ? "queued"
        : (qr == ReliableGameplayEventQueueResult::ConnectionUnavailable
            ? "connection_unavailable" : "backlog_saturated");
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Network, StructuredLevel::Important,
        "server.actor.spawn_published", std::to_string(player.id),
        "authoritative player spawn packet queued for client",
        serverGamemodeState().currentServerTick,
        nlohmann::json{{"actor_id", player.id},
                       {"initial_spawn", isInitialSpawn},
                       {"spawn_generation", player.spawnGeneration},
                       {"transform_epoch", player.transformEpoch},
                       {"reliable_event_id", spawnSync.eventId},
                       {"queue_result", queueResult},
                       {"health", player.health},
                       {"dead", player.dead},
                       {"spawn_state", static_cast<int>(player.spawnState)},
                       {"avatar", player.avatarName},
                       {"position", {player.pos.x, player.pos.y, player.pos.z}},
                       {"velocity", {player.vel.x, player.vel.y, player.vel.z}}},
        __FILE__, __LINE__, __FUNCTION__);

    DBG(Network, "SPAWN TX CREATE id=%u spawnGen=%u epoch=%u reason=%s health=%d ownedWeapons=%zu",
        player.id, player.spawnGeneration, player.transformEpoch,
        isInitialSpawn ? "initial" : "respawn", player.health, player.ownedWeaponIds.size());
}

// ── Advance all reload timers — called once per server tick ─────────
void tickWeaponRuntimes(std::unordered_map<uint32_t, ServerPlayer>& players, uint32_t currentTick)
{
    for (auto& kv : players)
    {
        ServerPlayer& p = kv.second;
        for (auto& rtKv : p.weaponRuntimes)
        {
            ServerPlayer::ServerWeaponRuntime& rt = rtKv.second;
            if (!rt.initialized || !rt.reloading)
                continue;
            if (currentTick >= rt.reloadCompleteTick)
            {
                const WeaponDefinition* def = WeaponRegistry::instance().get(rtKv.first);
                if (!def)
                {
                    rt.reloading = false;
                    continue;
                }

                bool oneAtATime = false;
                auto cit = def->customParams.find("reloadOneAtATime");
                if (cit != def->customParams.end()) oneAtATime = (cit->second != 0.0f);

                if (oneAtATime)
                {
                    // Load +1 shell, schedule next if more needed
                    if (rt.magazineAmmo < def->magazineSize && rt.reserveAmmo > 0)
                    {
                        rt.magazineAmmo++;
                        rt.reserveAmmo--;
                        rt.stateRevision++;
                    }
                    if (rt.magazineAmmo < def->magazineSize && rt.reserveAmmo > 0)
                    {
                        float reloadInterval = def->customParams.count("reloadTimePerShell")
                            ? def->customParams.at("reloadTimePerShell") : (def->reloadTime > 0 ? def->reloadTime : 0.55f);
                        rt.reloadCompleteTick = currentTick + (uint32_t)std::ceil(reloadInterval * 60.0f);
                    }
                    else
                    {
                        rt.reloading = false;
                    }
                }
                else
                {
                    // Bulk reload — fill all missing shells at once
                    int needed = def->magazineSize - rt.magazineAmmo;
                    int loaded = std::min(needed, rt.reserveAmmo);
                    rt.magazineAmmo += loaded;
                    rt.reserveAmmo -= loaded;
                    rt.stateRevision++;
                    rt.reloading = false;
                }
            }
        }
    }
}

void simulatePlayer(ServerPlayer& p, const HeadlessWorld& world)
{
    p.actorCollisionPreviousPos = p.pos;
    p.hasActorCollisionPreviousPos = true;
    p.actorCollisionChanged = false;

    if (p.teleportInvulnerabilityTicks > 0)
        --p.teleportInvulnerabilityTicks;

    // Apply input yaw BEFORE any non-dead early return.
    // Orientation comes from current input and must update every frame,
    // even when clientStateUpdated causes an early return.
    p.yaw = AimBodyConfig::instance().smoothAngle(
        p.yaw, p.input.yaw, SERVER_DT);
    if (AimBodyConfig::instance().smoothMode())
        p.input.yaw = p.yaw;

    {
        static uint64_t lastLookApplyLogMs = 0;
        uint64_t nowLookApply = nowMs();
        if (nowLookApply - lastLookApplyLogMs >= 1000)
        {
            printf("[LOOK SERVER APPLY] playerId=%u inputYaw=%.2f oldYaw=%.2f newYaw=%.2f "
                   "clientStateUpdated=%d\n",
                   p.id, p.input.yaw, p.yaw, p.input.yaw, (int)p.clientStateUpdated);
            lastLookApplyLogMs = nowLookApply;
        }
    }

    // Countdown freeze: during the 3-2-1 of a round mode the human stands
    // still (can still look) and is released exactly at GO. Warmup is exempt.
    if (serverGamemodeState().roundCountdownFreeze && !serverGamemodeState().warmup)
    {
        p.input.wish = glm::vec2(0.0f);
        p.input.jumpHeld = false;
        p.input.dashPressed = false;
        p.input.downDashPressed = false;
    }

    // Downed actors cannot move or attack; they wait for a teammate revive.
    if (p.downed)
    {
        p.input.wish = glm::vec2(0.0f);
        p.input.jumpHeld = false;
        p.input.dashPressed = false;
        p.input.downDashPressed = false;
        p.input.attackPressed = false;
        p.vel = glm::vec3(0.0f);
        p.movement.externalImpulse = glm::vec3(0.0f);
        syncServerMovementRuntime(p, false);
        return;
    }

    if (p.dead)
    {
        p.vel = glm::vec3(0.0f);
        p.movement.externalImpulse = glm::vec3(0.0f);
        syncServerMovementRuntime(p, false);
        p.projectileFireCooldown = std::max(0.0f, p.projectileFireCooldown - SERVER_DT);

        // One-life / no-respawn mode: the actor remains dead for the round
        // (Spectating in the match state) and never revives here, even if a
        // client requests an instant respawn.
        if (!serverPlayerRespawnsEnabled(p.id))
        {
            p.instantRespawnRequested = false;
            return;
        }

        // Instant respawn request from client Space press
        if (p.instantRespawnRequested)
        {
            p.instantRespawnRequested = false;
            p.respawnSeconds = serverMatchRespawnSeconds();
            printf("%s [SERVER RESPAWN INSTANT] playerId=%u trigger=instantRespawnRequested\n",
                   serverTimestamp(), p.id);
        }
        const float respawnSecondsBeforeTick = p.respawnSeconds;
        p.respawnSeconds -= SERVER_DT;
        if (p.respawnSeconds <= 0.0f)
        {
            glm::vec3 respawnPos;
            float respawnYaw = 0.0f;
            if (p.hasDuelSpawnPos)
            {
                // Duel mode: fixed team spawn, full reset handled below.
                respawnPos = p.duelSpawnPos;
            }
            else if (!serverPickSafeSpawn(&world, p.id, respawnPos, respawnYaw))
            {
                // No safe spawn: keep the player waiting rather than placing
                // them at an unvalidated coordinate; retry shortly.
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Network, StructuredLevel::Errors,
                    "spawn.server-refused", std::to_string(p.id),
                    "respawn refused: no safe spawn", 0,
                    {{"spawn_points", (long)world.spawnPoints.size()}},
                    __FILE__, __LINE__, __FUNCTION__);
                p.respawnSeconds = 1.0f;
                return;
            }
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Network, StructuredLevel::Important,
                "server.actor.respawn_timer_expired", std::to_string(p.id),
                "respawn delay elapsed; server selected a safe spawn",
                serverGamemodeState().currentServerTick,
                nlohmann::json{{"actor_id", p.id},
                               {"spawn_generation", p.spawnGeneration},
                               {"respawn_timer_before_tick", respawnSecondsBeforeTick},
                               {"simulation_dt_seconds", SERVER_DT},
                               {"wave_mode", serverGamemodeState().npcWaves},
                               {"wave_lives_remaining", serverGamemodeState().waveLivesRemaining},
                               {"position", {respawnPos.x, respawnPos.y, respawnPos.z}},
                               {"yaw_degrees", respawnYaw}},
                __FILE__, __LINE__, __FUNCTION__);
            // The spawn yaw is the actor's valid look direction for this new
            // life. Compute the impulse after choosing it, not from the old
            // yaw left over from the previous life.
            p.yaw = respawnYaw;
            const glm::vec3 spawnVel = SpawnVelocityConfig::instance().enabled()
                ? SpawnVelocityConfig::instance().computeSpawnImpulse(respawnYaw)
                : glm::vec3(0.0f);
            beginAuthoritativeTransform(p, respawnPos, spawnVel, respawnYaw, "respawn");
            p.actorCollisionPreviousPos = p.pos;
            p.hasActorCollisionPreviousPos = true;
            // resetPlayerForSpawn is called by completeAuthoritativeSpawn
            // which is triggered by justRespawned flag in the server pump.
            p.justRespawned = true;  // signal caller to send spawn sync
            // Melee/Payload: grant the mode's spawn protection on respawn.
            {
                const ServerGamemodeState& mode = serverGamemodeState();
                const uint32_t protection = mode.payloadMode
                    ? (uint32_t)std::max(0, mode.payloadSpawnProtectionTicks) : 60u;
                serverMeleeNoteSpawn(p.id, protection);
            }
            p.input = {};
            p.attackQueued = false;
            p.dashAvailable = true;
            p.onGround = false;
            p.respawnSeconds = 0.0f;
            syncServerMovementRuntime(p, false);
            printf("%s [SERVER RESPAWN] playerId=%u position=(%.2f,%.2f,%.2f) epoch=%u spawnpoints=%zu spawnGeneration=%u health=%d\n",
                   serverTimestamp(), p.id, p.pos.x, p.pos.y, p.pos.z, (unsigned)p.transformEpoch,
                   world.spawnPoints.size(), p.spawnGeneration, p.health);
        }
        return;
    }

    // If the lifecycle/transform handshake is not complete, freeze movement.
    // The transform-ack gate can only wedge when the client has NOT yet proven
    // it knows the current life (see the implicit lifecycle resume in
    // validateClientMovementReport); once a matching-lifecycle report is
    // accepted the server takes its position directly and simulation resumes,
    // even if the SpawnAck is still in flight under bad packet loss.
    {
        // Backstop: never let the transform-ack gate wedge a player forever.
        // The lifecycle checks already reject stale-life reports, so a
        // timed-out ack is safe to clear.
        if (p.awaitingAuthoritativeTransformAck &&
            p.authoritativeTransformAssignedMs != 0 &&
            nowMs() - p.authoritativeTransformAssignedMs >=
                (uint64_t)kAuthoritativeTransformAckTimeoutMs)
        {
            p.awaitingAuthoritativeTransformAck = false;
            Debug::warn(Debug::Category::Networking,
                "[SERVER TRANSFORM ACK TIMEOUT] playerId=%u epoch=%u elapsedMs=%llu "
                "— ack gate cleared, movement resumed\n",
                p.id, (unsigned)p.transformEpoch,
                (unsigned long long)(nowMs() - p.authoritativeTransformAssignedMs));
        }
        if (p.spawnState != ServerPlayer::Active ||
            (p.awaitingAuthoritativeTransformAck && !p.hasAcceptedClientTransform))
        {
            // When awaiting a spawn ack the velocity is the authoritative
            // spawn impulse delivered by completeAuthoritativeSpawn.  Zeroing
            // it here would destroy the spawn velocity before the client
            // ever applies it.  Only clear velocity for non-spawn freezes
            // (e.g. stale transform ack on an already-active player).
            if (p.spawnState != ServerPlayer::AwaitingSpawnAck)
                p.vel = glm::vec3(0.0f);
            p.movement.externalImpulse = glm::vec3(0.0f);
            p.clientStateUpdated = false;
            syncServerMovementRuntime(
                p, p.spawnState == ServerPlayer::Active);
            return;
        }
    }

    p.projectileFireCooldown = std::max(0.0f, p.projectileFireCooldown - SERVER_DT);

    // Developer flight is server-authoritative: the client supplies only
    // direction buttons; this branch owns speed and position.
    if (p.flyEnabled)
    {
        // The client already sends camera-relative world X/Y wish movement.
        // Applying the camera basis here again would rotate it twice.
        glm::vec3 direction(p.input.wish.x, p.input.wish.y, 0.0f);
        if (p.input.flyUp) direction.z += 1.0f;
        if (p.input.flyDown) direction.z -= 1.0f;
        if (glm::length(direction) > 1.0f)
            direction = glm::normalize(direction);
        const float speed = movementDeveloperFlightSpeed(
            p.flySpeedMultiplier, p.input.flyFast);
        p.vel = direction * speed;
        p.pos = resolveDeveloperFlightWorldMove(
            world, p.pos, p.vel * SERVER_DT, p.vel);
        p.onGround = false;
        p.movement.externalImpulse = glm::vec3(0.0f);
        p.clientStateUpdated = false;
        syncServerMovementRuntime(p, true);
        return;
    }

    // ── Ragdoll / physical body: the owning client is authoritative ─────
    // Only an ACTUAL ragdoll (the G-toggle body) lets the client own its root
    // without normal movement validation. The always-on physical/hybrid aim
    // body reports a replicated pose too, but its root is driven by the normal
    // movement controller, so it must go through the validated movement path.
    // Treating hybrid/physical as a root-authoritative ragdoll previously let a
    // high-speed client report a position straight through world geometry and
    // the server accepted it, producing a permanent position disagreement.
    const bool ragdollRootAuthoritative =
        p.hasRagdollPose && !p.ragdollHistory.empty() &&
        p.ragdollHistory.back().pose.mode == RAGDOLL_NET_RAGDOLL;
    if (ragdollRootAuthoritative)
    {
        const RagdollReplicationPose& pose = p.ragdollHistory.back().pose;
        glm::vec3 target = pose.rootPosition;
        const bool finite = std::isfinite(target.x) &&
                            std::isfinite(target.y) &&
                            std::isfinite(target.z);
        if (pose.active && pose.count > 0 && finite)
        {
            static constexpr float kMaxRagdollStepMeters = 4.0f;
            const glm::vec3 before = p.pos;
            glm::vec3 delta = target - p.pos;
            const float dist = glm::length(delta);
            if (dist > kMaxRagdollStepMeters && dist > 1e-5f)
                target = p.pos + delta * (kMaxRagdollStepMeters / dist);

            // Swept geometry guard: even a real ragdoll root may not cross
            // blocking world geometry (walls). Clamp to the previous accepted
            // root when the reported move would pass through it.
            if (MimitaNet::crossesBlockingGeometry(&world, before, target, 0.35f))
                target = before;

            p.pos = target;

            if (std::isfinite(pose.rootYaw))
                p.yaw = pose.rootYaw;
            // Non-imaginary velocity for interpolation/extrapolation of the
            // root between low-rate poses.
            p.vel = (p.pos - before) / std::max(SERVER_DT, 1e-4f);
            p.onGround = false;
            p.movement.externalImpulse = glm::vec3(0.0f);
            p.clientStateUpdated = false;
            // Keep the accepted-client state fresh so leaving ragdoll does not
            // trip the post-gap correction (which would snap the client back).
            p.lastAcceptedClientPosition = p.pos;
            p.lastAcceptedClientVelocity = p.vel;
            p.hasAcceptedClientTransform = true;
            if (pose.sourceTick != 0)
                p.movementValidation.lastAcceptedClientTick = pose.sourceTick;
            syncServerMovementRuntime(p, true);

            static uint64_t lastRagdollClampLogMs = 0;
            if (dist > kMaxRagdollStepMeters && nowMs() - lastRagdollClampLogMs >= 1000)
            {
                lastRagdollClampLogMs = nowMs();
                Debug::warn(Debug::Category::Networking,
                    "[SERVER RAGDOLL CLAMP] playerId=%u dist=%.2f limit=%.2f "
                    "accepted=(%.2f,%.2f,%.2f)\n",
                    p.id, dist, kMaxRagdollStepMeters,
                    p.pos.x, p.pos.y, p.pos.z);
            }
            return;
        }
    }

    // ── Server-side movement simulation from input commands (spec) ─────
    // The server simulates movement using the SAME kernel as the client.
    // Input commands are replayed in CHRONOLOGICAL order (lowest sequence
    // first), so the simulated path follows the client's exact intended
    // trajectory. Consuming the newest command first instead would replay a
    // burst of commands in reverse order, kinking the simulated position and
    // making the broadcast stream jittery (visible as jitter to other clients).
    {
        // Find the oldest valid input command (FIFO replay). Redundant command
        // slots can insert out of ring order, so scan all slots for the lowest
        // sequence instead of relying on ring position.
        MovementCommand cmd;
        bool hasInput = false;
        uint32_t lowestSeq = 0;
        int bestIndex = -1;
        for (int i = 0; i < ServerPlayer::INPUT_COMMAND_BUFFER_SIZE; ++i)
        {
            const auto& entry = p.inputCommandBuffer[i];
            if (!entry.valid)
                continue;
            if (bestIndex < 0 || entry.command.sequence < lowestSeq)
            {
                lowestSeq = entry.command.sequence;
                bestIndex = i;
            }
        }
        if (bestIndex >= 0)
        {
            auto& entry = p.inputCommandBuffer[bestIndex];
            cmd = entry.command;
            hasInput = true;
            p.lastProcessedInputCommandSequence = cmd.sequence;
            entry.valid = false;
        }
        if (!hasInput)
        {
            cmd = movementCommandFromServerInput(
                p.input, p.lastMovementSequence,
                MovementLifecycleIdentity{p.spawnGeneration, p.transformEpoch});
        }

        // Re-resolve the avatar movement capability when the avatar changes
        // mid-life (e.g. switching from a roll avatar to a walk avatar).
        if (p.movementCapAvatar != p.avatarName)
        {
            p.movementCapAvatar = p.avatarName;
            const AvatarRig* rig =
                AvatarAnimationRegistry::instance().getRig(p.avatarName);
            p.movementCap = rig ? rig->movement : AvatarMovementCap{};
            // Resolved-capability evidence (spec 7.1/Slice 2): the server sees
            // the same mode the client resolved, with no avatar-name gate.
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Avatar, StructuredLevel::Important,
                "griever.capability.resolved", p.avatarName,
                "server resolved avatar movement capability",
                (uint32_t)p.spawnGeneration,
                {{"avatar", p.avatarName},
                 {"mode", p.movementCap.mode},
                 {"force", p.movementCap.force},
                 {"mass", p.movementCap.mass},
                 {"gravity_scale", p.movementCap.gravityScale},
                 {"restitution", p.movementCap.restitution},
                 {"radius", p.movementCap.radius},
                 {"height", p.movementCap.height}},
                __FILE__, __LINE__, __FUNCTION__);
        }

        // Roll mode is client-authoritative: the owning client simulates the
        // force-driven body and reports its transform; the movement validation
        // accepts it (see movement-validation.cpp). The server keeps that
        // accepted transform instead of re-simulating, so a rolling avatar is
        // responsive. It still clamps to blocking geometry as a safety guard.
        if (p.movementCap.mode == "roll")
        {
            p.clientStateUpdated = false;
            syncServerMovementRuntime(p, true);
            return;
        }

        // Role movement preset (if any) drives the shared kernel for this
        // actor; otherwise the global movement config applies.
        const MovementConfig* roleMove =
            RoleMovementCache::instance().get(p.movementProfileId);
        const MovementConfig cfg = roleMove
            ? applyRuntimeMovementTuning(*roleMove)
            : makeCurrentRuntimeMovementConfig();
        MovementState state = movementStateFromServerPlayer(p);

        // Phase 1: Pre-collision movement (gravity, walk, jump, dash)
        applyPreCollisionBasicMovement(state, cmd, cfg, SERVER_DT);
        MovementStepEvents preEvents;
        applySpecialMovementPreCollision(state, cmd, cfg, SERVER_DT, preEvents);
        applyMovementStateToServerPlayer(state, p);

        // Phase 2: World collision resolve
        resolveWorldCollision(p, world);

        // Phase 3: Build collision feedback and run post-collision movement
        state = movementStateFromServerPlayer(p);
        MovementCollisionFeedback collision;
        collision.onGround = p.onGround;
        collision.hasWorldContact = p.onGround;
        collision.realWorldContactThisFrame = p.onGround;
        collision.groundNormal = {0.0f, 0.0f, 1.0f};
        collision.simulationTick = cmd.clientSimulationTick;

        MovementStepResult stepResult = applyPostCollisionMovementWithSpecials(
            state, cmd, cfg, collision, SERVER_DT, preEvents);
        applyMovementStateToServerPlayer(stepResult.state, p);
        p.clientStateUpdated = false;

        {
            static uint64_t lastDivergenceLogMs = 0;
            uint64_t nowDiv = nowMs();
            const float simDivergence = glm::length(p.pos - p.lastAcceptedClientPosition);
            if (simDivergence > 2.0f && nowDiv - lastDivergenceLogMs >= 1000)
            {
                lastDivergenceLogMs = nowDiv;
                Debug::warn(Debug::Category::Networking,
                    "[SERVER SIM DIVERGENCE] playerId=%u simPos=(%.1f,%.1f,%.1f) "
                    "acceptedPos=(%.1f,%.1f,%.1f) distance=%.1f seq=%u tick=%llu\n",
                    p.id,
                    p.pos.x, p.pos.y, p.pos.z,
                    p.lastAcceptedClientPosition.x,
                    p.lastAcceptedClientPosition.y,
                    p.lastAcceptedClientPosition.z,
                    simDivergence, cmd.sequence,
                    (unsigned long long)cmd.clientSimulationTick);
            }
        }

        if (DebugConfig::DEBUG_MOVEMENT_SIM)
        {
            Debug::log(Debug::Category::Networking,
                "[SERVER MOVE SIM] playerId=%u seq=%u tick=%llu "
                "pos=(%.2f,%.2f,%.2f) vel=(%.2f,%.2f,%.2f) ground=%d\n",
                p.id, cmd.sequence,
                (unsigned long long)cmd.clientSimulationTick,
                p.pos.x, p.pos.y, p.pos.z,
                p.vel.x, p.vel.y, p.vel.z,
                (int)p.onGround);
        }
    }
    syncServerMovementRuntime(p, true);
}

void pushPositionHistory(ServerPlayer& p, uint32_t tick)
{
    const auto& netCfg = NetworkingConfig::instance().data();
    // Record the position actually broadcast to clients (the smoothed
    // interpolated one), so hit rewind reads exactly what attackers saw.
    // In server_sim broadcast mode that is the authoritative simulation pos,
    // optionally smoothed via server_sim_smooth_ticks.
    const bool serverSimBroadcast =
        netCfg.remotePlayers.broadcastSource == "server_sim";

    // Server-sim broadcast smoothing: ease the broadcast position toward the
    // authoritative sim position so the snapshot stream is clean (removes the
    // tiny per-tick resting jitter from the collision solver). Real movement
    // passes through with ~server_sim_smooth_ticks of easing (negligible lag).
    const uint32_t smoothTicks = netCfg.remotePlayers.serverSimSmoothTicks;
    glm::vec3 simBroadcast = p.pos;
    if (serverSimBroadcast)
    {
        if (!p.hasSimBroadcastPos)
        {
            p.simBroadcastPos = p.pos;
            p.hasSimBroadcastPos = true;
        }
        if (smoothTicks > 0)
        {
            const float k = 1.0f / (float)(smoothTicks + 1);
            p.simBroadcastPos += (p.pos - p.simBroadcastPos) * k;
        }
        else
        {
            p.simBroadcastPos = p.pos;
        }
        simBroadcast = p.simBroadcastPos;
    }

    const glm::vec3 histPos = serverSimBroadcast ? simBroadcast
        : (p.hasBroadcastTransform ? p.broadcastPosition : p.pos);
    const glm::vec3 histVel = serverSimBroadcast
        ? (p.hasAcceptedClientTransform ? p.lastAcceptedClientVelocity : p.vel)
        : (p.hasBroadcastTransform ? p.broadcastVelocity : p.vel);
    p.posHistory.push_back({histPos, histVel, p.yaw, tick});
    const std::size_t historyLimit = NetworkingConfig::instance()
        .data().bufferLimits.serverPositionHistoryTicks;
    while (p.posHistory.size() > historyLimit)
        p.posHistory.pop_front();
}

bool getPositionAtTick(const ServerPlayer& p, uint32_t targetTick, glm::vec3& outPos)
{
    if (p.posHistory.empty())
        return false;
    if (targetTick >= p.posHistory.back().tick)
    {
        outPos = p.posHistory.back().pos;
        return true;
    }
    if (targetTick <= p.posHistory.front().tick)
    {
        outPos = p.posHistory.front().pos;
        return true;
    }
    for (int i = (int)p.posHistory.size() - 1; i > 0; --i)
    {
        if (p.posHistory[i].tick == targetTick)
        {
            outPos = p.posHistory[i].pos;
            return true;
        }
        if (p.posHistory[i].tick < targetTick)
        {
            const auto& a = p.posHistory[i];
            const auto& b = p.posHistory[i + 1];
            float frac = float(targetTick - a.tick) / float(b.tick - a.tick);
            outPos = glm::mix(a.pos, b.pos, frac);
            return true;
        }
    }
    outPos = p.posHistory.front().pos;
    return true;
}

// Like getPositionAtTick but also returns the broadcast yaw at that tick so
// body-part hitboxes can be reconstructed with the correct facing (the yaw the
// attacker saw, not the target's current yaw).
bool getPlayerPoseAtTick(const ServerPlayer& p, uint32_t targetTick,
                         glm::vec3& outPos, float& outYaw)
{
    if (p.posHistory.empty())
        return false;
    const auto& back = p.posHistory.back();
    if (targetTick >= back.tick)
    {
        outPos = back.pos;
        outYaw = back.yaw;
        return true;
    }
    const auto& front = p.posHistory.front();
    if (targetTick <= front.tick)
    {
        outPos = front.pos;
        outYaw = front.yaw;
        return true;
    }
    int lo = 0;
    int hi = (int)p.posHistory.size() - 1;
    while (lo < hi - 1)
    {
        int mid = (lo + hi) / 2;
        if (p.posHistory[mid].tick <= targetTick)
            lo = mid;
        else
            hi = mid;
    }
    const auto& a = p.posHistory[lo];
    const auto& b = p.posHistory[lo + 1];
    const float frac = (b.tick > a.tick)
        ? float(targetTick - a.tick) / float(b.tick - a.tick)
        : 0.0f;
    outPos = glm::mix(a.pos, b.pos, frac);
    outYaw = a.yaw + (b.yaw - a.yaw) * frac;
    return true;
}

bool getPlayerRagdollPoseAtTick(const ServerPlayer& p, uint32_t targetTick,
                                RagdollReplicationPose& out)
{
    if (p.ragdollHistory.empty())
        return false;

    const auto& back = p.ragdollHistory.back();
    if (targetTick >= back.tick) {
        out = back.pose;
        return out.active;
    }
    const auto& front = p.ragdollHistory.front();
    if (targetTick <= front.tick) {
        out = front.pose;
        return out.active;
    }

    int lo = 0;
    int hi = (int)p.ragdollHistory.size() - 1;
    while (lo < hi - 1) {
        const int mid = (lo + hi) / 2;
        if (p.ragdollHistory[mid].tick <= targetTick)
            lo = mid;
        else
            hi = mid;
    }
    const auto& a = p.ragdollHistory[lo];
    const auto& b = p.ragdollHistory[lo + 1];
    if (!a.pose.active && !b.pose.active)
        return false;
    const float frac = (b.tick > a.tick)
        ? float(targetTick - a.tick) / float(b.tick - a.tick)
        : 0.0f;
    out = interpolateReplicatedPose(a.pose, b.pose, frac);
    return out.active;
}

void beginServerBroadcastInterp(ServerPlayer& player, uint32_t serverTick)
{
    if (!player.hasAcceptedClientTransform)
        return;

    const auto& interpCfg = NetworkingConfig::instance().data().remotePlayers;

    // When smoothing is off the broadcast is simply the newest accepted report
    // — coherent with the animation state carried in the same snapshot.
    if (!interpCfg.serverSmoothing)
    {
        player.broadcastPosition = player.lastAcceptedClientPosition;
        player.broadcastVelocity = player.lastAcceptedClientVelocity;
        player.hasBroadcastTransform = true;
        player.hasInterpSegment = false;
        player.broadcastSamples.clear();
        player.broadcastSamplesSeeded = false;
        return;
    }

    // Lifecycle reset (respawn / teleport / map change): drop all history so
    // the broadcast snaps to the new life instead of smearing across it.
    if (!player.broadcastSamplesSeeded)
    {
        player.broadcastSamplesSeeded = true;
        player.broadcastSamplesSpawnGeneration = player.spawnGeneration;
        player.broadcastSamplesEpoch = player.transformEpoch;
    }
    else if (player.spawnGeneration != player.broadcastSamplesSpawnGeneration ||
             player.transformEpoch != player.broadcastSamplesEpoch)
    {
        player.broadcastSamples.clear();
        player.broadcastSamplesSpawnGeneration = player.spawnGeneration;
        player.broadcastSamplesEpoch = player.transformEpoch;
    }

    // Buffer the accepted report. Interpolation is keyed by the client's own
    // movement tick (uniform 60Hz), not by server arrival time, so reordered
    // and bursty arrivals render as uniform motion.
    const uint32_t acceptedClientTick =
        player.movementValidation.lastAcceptedClientTick;
    player.broadcastSamples.push_back({
        nowMs(), acceptedClientTick,
        player.lastAcceptedClientPosition, player.lastAcceptedClientVelocity});
    const std::size_t sampleLimit = NetworkingConfig::instance()
        .data().bufferLimits.serverBroadcastSampleLimit;
    while (player.broadcastSamples.size() > sampleLimit)
        player.broadcastSamples.pop_front();

    // Keep the fixed-window lerp fields fresh as the fallback path used when
    // serverBroadcastDelaySeconds == 0.
    const uint32_t newTick = acceptedClientTick;
    if (player.hasInterpSegment)
        player.interpFromPos = player.broadcastPosition;
    else
        player.interpFromPos = player.lastAcceptedClientPosition;
    player.interpDurationTicks = std::max(1u, interpCfg.serverSmoothingDurationTicks);
    player.interpToPos = player.lastAcceptedClientPosition;
    player.interpToTick = newTick;
    player.interpSegmentStartTick = serverTick;
    player.hasInterpSegment = true;
    player.broadcastVelocity = player.lastAcceptedClientVelocity;
    player.hasBroadcastTransform = true;
}

void updateServerBroadcastInterp(ServerPlayer& player, uint32_t serverTick)
{
    if (!player.hasAcceptedClientTransform)
        return;

    const auto& interpCfg = NetworkingConfig::instance().data().remotePlayers;

    if (!interpCfg.serverSmoothing)
    {
        player.broadcastPosition = player.lastAcceptedClientPosition;
        player.broadcastVelocity = player.lastAcceptedClientVelocity;
        player.hasBroadcastTransform = true;
        return;
    }

    // ── Client-tick broadcast rendering (primary path) ───────────────
    // Render the broadcast at `newestClientTick - delayTicks`, lerping between
    // the two accepted reports whose client movement ticks bracket that
    // instant. Client ticks are uniform 60Hz, so even reordered/bursty
    // arrivals produce uniform motion — no arrival-cluster "flash-then-hold".
    if (interpCfg.serverBroadcastDelaySeconds > 0.0 &&
        player.broadcastSamples.size() >= 2)
    {
        const uint32_t newestClientTick = player.broadcastSamples.back().clientTick;
        const double delayTicks =
            interpCfg.serverBroadcastDelaySeconds * (double)GAMEPLAY_SIMULATION_HZ;
        const double targetTick = (double)newestClientTick - delayTicks;

        // Optional speed clamp: cap how far the broadcast may move per tick so
        // a burst report cannot make the body lurch even through the buffer.
        // 0 = unlimited.
        const float maxDelta = interpCfg.serverBroadcastMaxSpeed > 0.0
            ? (float)(interpCfg.serverBroadcastMaxSpeed * (double)SERVER_DT)
            : 0.0f;
        const auto clampDelta = [&](const glm::vec3& target) {
            if (maxDelta <= 0.0f)
            {
                player.broadcastPosition = target;
                return;
            }
            const glm::vec3 delta = target - player.broadcastPosition;
            const float len = glm::length(delta);
            player.broadcastPosition = (len > maxDelta)
                ? player.broadcastPosition + delta * (maxDelta / len)
                : target;
        };

        const ServerPlayer::BroadcastSample* older = nullptr;
        const ServerPlayer::BroadcastSample* newer = nullptr;
        for (const auto& s : player.broadcastSamples)
        {
            if ((double)s.clientTick <= targetTick)
                older = &s;   // keep the latest one at/before targetTick
            else
            {
                newer = &s;   // first sample after targetTick
                break;
            }
        }

        if (older && newer)
        {
            double alpha = 1.0;
            if (newer->clientTick > older->clientTick)
            {
                alpha = (targetTick - (double)older->clientTick) /
                        (double)(newer->clientTick - older->clientTick);
                alpha = std::clamp(alpha, 0.0, 1.0);
            }
            clampDelta(older->position +
                (newer->position - older->position) * (float)alpha);
            player.broadcastVelocity = older->velocity +
                (newer->velocity - older->velocity) * (float)alpha;
            player.hasBroadcastTransform = true;
            return;
        }

        if (!older)
        {
            // targetTick older than everything buffered (fresh start): hold the
            // oldest sample until enough history accumulates.
            const auto& newest = player.broadcastSamples.front();
            clampDelta(newest.position);
            player.broadcastVelocity = newest.velocity;
            player.hasBroadcastTransform = true;
            return;
        }

        // Buffer ran dry (no fresh reports): extrapolate along the newest
        // velocity for a short wall-clock time, then hold.
        const auto& newest = player.broadcastSamples.back();
        const double dryTicks = targetTick - (double)newest.clientTick;
        const double dryMs = dryTicks / (double)GAMEPLAY_SIMULATION_HZ * 1000.0;
        if (interpCfg.serverBroadcastExtrapolationSeconds > 0.0 && dryMs > 0.0)
        {
            const double extrapMs = std::min(
                dryMs, interpCfg.serverBroadcastExtrapolationSeconds * 1000.0);
            clampDelta(newest.position +
                newest.velocity * (float)(extrapMs / 1000.0));
            player.broadcastVelocity = newest.velocity;
            player.hasBroadcastTransform = true;
            return;
        }
        clampDelta(newest.position);
        player.broadcastVelocity = newest.velocity;
        player.hasBroadcastTransform = true;
        return;
    }

    // ── Fallback: fixed-window lerp (serverBroadcastDelaySeconds == 0) ──
    if (!player.hasInterpSegment)
    {
        player.broadcastPosition = player.lastAcceptedClientPosition;
        player.broadcastVelocity = player.lastAcceptedClientVelocity;
        player.hasBroadcastTransform = true;
        return;
    }

    const uint32_t duration = std::max(1u, player.interpDurationTicks);
    const uint32_t elapsed = serverTick >= player.interpSegmentStartTick
        ? (serverTick - player.interpSegmentStartTick)
        : 0u;
    const float alpha = elapsed >= duration
        ? 1.0f
        : (float)elapsed / (float)duration;
    player.broadcastPosition = player.interpFromPos +
        (player.interpToPos - player.interpFromPos) * alpha;
    player.broadcastVelocity = player.lastAcceptedClientVelocity;
    player.hasBroadcastTransform = true;
}

uint32_t estimateServerRewindTick(const ServerPlayer& attacker,
                                  uint32_t clientSimulationTick,
                                  uint32_t serverTick)
{
    // clientSimulationTick carries the newest server tick the attacker had
    // rendered (the client stamps it from the local-player snapshot tick), so
    // it is already in the server tick domain. Their view is that tick minus
    // the remote interpolation buffer.
    // rewind_compensation_ms adds an extra tunable offset (in server ticks)
    // so hits land on the body the shooter actually rendered under jitter and
    // server broadcast smoothing. Positive = rewind further back.
    const int64_t compTicks = (int64_t)std::llround(
        NetworkingConfig::instance().data().remotePlayers
            .rewindCompensationSeconds * (double)GAMEPLAY_SIMULATION_HZ);
    // A non-zero clientSimulationTick is already the client's delayed rendered
    // world tick.  Do not subtract ping again: the received snapshot is already
    // old by the network transit time, and subtracting ping here double-counts
    // that delay and validates against a pose older than the shooter saw.
    const int64_t pingTicks = (int64_t)std::llround(
        (double)attacker.pingMs / 1000.0 * (double)GAMEPLAY_SIMULATION_HZ);
    int64_t rewind;
    if (clientSimulationTick != 0)
        rewind = (int64_t)clientSimulationTick - (int64_t)REWIND_INTERP_DELAY_TICKS - compTicks;
    else if (attacker.movementValidation.lastAcceptedClientTick != 0 &&
             attacker.lastAcceptedServerTick != 0)
        rewind = (int64_t)attacker.lastAcceptedServerTick -
                 (int64_t)REWIND_INTERP_DELAY_TICKS - compTicks - pingTicks;
    else
        rewind = (int64_t)serverTick - (int64_t)REWIND_INTERP_DELAY_TICKS - compTicks - pingTicks;

    // Hard ceiling: never rewind older than maxRewindTicks (the rewind history
    // window / worst-case latency + interpolation). A stale client fire tick
    // (clock drift, blackout) must not push the rewind seconds into the past,
    // which made every hit miss by validating against an ancient target pose.
    const uint32_t maxRewind = NetworkingConfig::instance().data()
        .remotePlayers.maxRewindTicks;
    if (maxRewind > 0)
    {
        const int64_t floor = (int64_t)serverTick - (int64_t)maxRewind;
        if (rewind < floor)
        {
            Debug::logThrottled(Debug::Category::Weapons, "attack-rewind-clamp", 1.0,
                "[ATTACK REWIND CLAMP] playerId=%u requestedRewind=%lld floor=%lld "
                "serverTick=%u maxRewindTicks=%u\n",
                attacker.id, (long long)rewind, (long long)floor,
                serverTick, maxRewind);
            rewind = floor;
        }
    }
    rewind = std::clamp<int64_t>(rewind, 0, (int64_t)serverTick);
    return (uint32_t)rewind;
}

SnapshotEntity makePlayerEntity(const ServerPlayer& player)
{
    SnapshotEntity out{};
    out.networkEntityId = player.id;
    out.entityType = ENTITY_PLAYER;
    out.active = 1;
    out.ownerClientId = player.id;
    // server_sim broadcast source: stream the server's own 60Hz input-driven
    // simulation. Always smooth and uniform regardless of the player's network
    // quality (client reports arrive gappy under loss/jitter/reorder). The
    // broadcast position is the eased simBroadcastPos (see pushPositionHistory)
    // so the snapshot stream is clean for other clients' linear interpolation.
    // Velocity + grounded use the VALIDATED client report: the server's own
    // re-sim cannot detect ground contact for a resting client (collision
    // dimensions differ) so gravity accumulates a bogus negative vz each tick —
    // broadcasting that makes observers extrapolate bodies into the floor.
    const bool serverSimBroadcast =
        NetworkingConfig::instance().data().remotePlayers.broadcastSource ==
        "server_sim";
    if (serverSimBroadcast)
    {
        const glm::vec3 bp = player.hasSimBroadcastPos
            ? player.simBroadcastPos : player.pos;
        out.px = bp.x;
        out.py = bp.y;
        out.pz = bp.z;
        const glm::vec3 bv = player.hasAcceptedClientTransform
            ? player.lastAcceptedClientVelocity : player.vel;
        out.vx = bv.x;
        out.vy = bv.y;
        out.vz = bv.z;
    }
    else if (player.hasBroadcastTransform)
    {
        out.px = player.broadcastPosition.x;
        out.py = player.broadcastPosition.y;
        out.pz = player.broadcastPosition.z;
        out.vx = player.broadcastVelocity.x;
        out.vy = player.broadcastVelocity.y;
        out.vz = player.broadcastVelocity.z;
    }
    else if (player.hasAcceptedClientTransform)
    {
        out.px = player.lastAcceptedClientPosition.x;
        out.py = player.lastAcceptedClientPosition.y;
        out.pz = player.lastAcceptedClientPosition.z;
        out.vx = player.lastAcceptedClientVelocity.x;
        out.vy = player.lastAcceptedClientVelocity.y;
        out.vz = player.lastAcceptedClientVelocity.z;
    }
    else
    {
        out.px = player.pos.x; out.py = player.pos.y; out.pz = player.pos.z;
        out.vx = player.vel.x; out.vy = player.vel.y; out.vz = player.vel.z;
    }
    out.yaw = player.yaw;
    out.health = player.health;
    out.meleePercent = player.meleePercent;
    {
        const auto& rules = MimitaMeleeRules::Config::instance().rules();
        const float remaining = std::clamp(
            player.meleeShieldDurability / std::max(1.0f, rules.shieldDurability),
            0.0f, 1.0f);
        out.meleeShield = (uint8_t)std::clamp((int)std::lround(remaining * 255.0f), 0, 255);
    }
    out.onGround = (serverSimBroadcast ? player.lastAcceptedClientOnGround : player.onGround) ? 1 : 0;
    out.equippedSlot = (int16_t)player.equippedSlot;
    out.weaponState = player.weaponState;
    serverWriteWeaponActionSnapshot(player.equippedWeaponId, player.meleeActionActive,
        player.meleeActionId, player.meleeActionStartTick, out);
    out.transformEpoch = player.transformEpoch;
    out.aimX = player.input.camForward.x;
    out.aimY = player.input.camForward.y;
    out.aimZ = player.input.camForward.z;
    out.pingMs = player.pingMs;
    out.sizeScale = player.sizeScale;
    out.spawnGeneration = player.spawnGeneration;

    {
        static uint64_t lastLookSnapshotLogMs = 0;
        uint64_t nowLookSnap = nowMs();
        if (nowLookSnap - lastLookSnapshotLogMs >= 1000)
        {
            printf("[LOOK SNAPSHOT BUILD] playerId=%u yaw=%.2f aim=(%.2f,%.2f,%.2f) "
                   "pos=(%.2f,%.2f,%.2f)\n",
                   player.id, player.yaw,
                   player.input.camForward.x, player.input.camForward.y, player.input.camForward.z,
                   player.pos.x, player.pos.y, player.pos.z);
            lastLookSnapshotLogMs = nowLookSnap;
        }
    }

    // Build snapshot state flags from client input state + server authoritative state.
    // Use client-validated visual flags for cosmetic animation replication
    // so that walking animation works even when server onGround is temporarily wrong.
    {
        constexpr uint16_t CLIENT_VISUAL_FLAGS =
            NET_STATE_WALKING | NET_STATE_JUMPING |
            NET_STATE_DASHING | NET_STATE_DOWN_DASHING |
            NET_STATE_FREEZING | NET_STATE_ATTACKING |
            NET_STATE_SHIELDING | NET_STATE_GRABBING |
            NET_STATE_CROUCHING | NET_STATE_REVIVING;

        uint16_t flags = player.inputStateFlags & CLIENT_VISUAL_FLAGS;

        // Server retains authority over ON_GROUND (in server_sim broadcast the
        // server's own ground detection is broken for resting clients, so fall
        // back to the validated client-reported grounded flag).
        const bool broadcastOnGround = serverSimBroadcast
            ? player.lastAcceptedClientOnGround : player.onGround;
        if (broadcastOnGround)
            flags |= NET_STATE_ON_GROUND;
        else
            flags &= ~NET_STATE_ON_GROUND;

        if (player.positionCorrectionPending)
            flags |= NET_STATE_POSITION_CORRECTED;
        if (player.actorCollisionChanged)
            flags |= NET_STATE_ACTOR_COLLISION;
        out.stateFlags = flags;

        static uint64_t lastWalkBuildLogMs = 0;
        uint64_t nowWalkBuild = nowMs();
        if (nowWalkBuild - lastWalkBuildLogMs >= 1000)
        {
            printf("[WALK SERVER BUILD] playerId=%u inputStateFlags=0x%04x "
                   "snapshotStateFlags=0x%04x walkingInputBit=%d "
                   "serverOnGround=%d planarSpeed=%.2f\n",
                   player.id, (unsigned)player.inputStateFlags,
                   (unsigned)out.stateFlags,
                   (int)((out.stateFlags & NET_STATE_WALKING) != 0),
                   (int)player.onGround,
                   glm::length(glm::vec2(player.vel.x, player.vel.y)));
            lastWalkBuildLogMs = nowWalkBuild;
        }
    }
    // Emit presentation serials (replicated from client, pass through unchanged).
    // These are the canonical event-ordered serials for remote VFX/anim trigger.
    out.dashSerial = player.lastPresentationDashSerial;
    out.groundJumpSerial = player.lastPresentationGroundJumpSerial;
    out.airJumpSerial = player.lastPresentationAirJumpSerial;
    out.downDashSerial = player.lastPresentationDownDashSerial;
    out.directionChangeSerial = player.lastPresentationDirectionChangeSerial;
    out.equipSerial = player.lastEquipSerial;
    out.freezeSerial = player.lastPresentationFreezeSerial;

    if (player.health <= 0)
    {
        const uint16_t stateFlagsBeforeDeathSanitize = out.stateFlags;
        const uint32_t dashSerialBeforeDeathSanitize = out.dashSerial;
        const uint32_t groundJumpSerialBeforeDeathSanitize = out.groundJumpSerial;
        const uint32_t airJumpSerialBeforeDeathSanitize = out.airJumpSerial;
        const uint32_t downDashSerialBeforeDeathSanitize = out.downDashSerial;
        const uint32_t directionChangeSerialBeforeDeathSanitize = out.directionChangeSerial;
        const uint32_t freezeSerialBeforeDeathSanitize = out.freezeSerial;

        // Dead actors must never replicate live presentation state. The
        // corpse presentation is a separate client-side object.
        out.stateFlags = 0;
        out.weaponState = 0;
        out.dashSerial = 0;
        out.groundJumpSerial = 0;
        out.airJumpSerial = 0;
        out.downDashSerial = 0;
        out.directionChangeSerial = 0;
        out.freezeSerial = 0;

        static uint64_t lastDeadSnapshotDiagnosticMs = 0;
        const uint64_t nowDeadSnapshot = nowMs();
        if (nowDeadSnapshot - lastDeadSnapshotDiagnosticMs >= 1000)
        {
            lastDeadSnapshotDiagnosticMs = nowDeadSnapshot;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Network, StructuredLevel::Important,
                "server.dead.snapshot.sanitized", std::to_string(player.id),
                "dead player snapshot presentation state cleared",
                0,
                { {"actor_id", player.id}, {"health", player.health},
                  {"state_flags_before", stateFlagsBeforeDeathSanitize},
                  {"dash_serial_before", dashSerialBeforeDeathSanitize},
                  {"ground_jump_serial_before", groundJumpSerialBeforeDeathSanitize},
                  {"air_jump_serial_before", airJumpSerialBeforeDeathSanitize},
                  {"down_dash_serial_before", downDashSerialBeforeDeathSanitize},
                  {"direction_change_serial_before", directionChangeSerialBeforeDeathSanitize},
                  {"freeze_serial_before", freezeSerialBeforeDeathSanitize},
                  {"state_flags_after", out.stateFlags},
                  {"dash_serial_after", out.dashSerial},
                  {"ground_jump_serial_after", out.groundJumpSerial},
                  {"air_jump_serial_after", out.airJumpSerial},
                  {"down_dash_serial_after", out.downDashSerial},
                  {"direction_change_serial_after", out.directionChangeSerial},
                  {"freeze_serial_after", out.freezeSerial},
                  {"spawn_generation", player.spawnGeneration} },
                __FILE__, __LINE__, __FUNCTION__);
        }
    }
    copyName(out.displayName, player.name);
    std::memset(out.avatarName, 0, sizeof(out.avatarName));
    std::strncpy(out.avatarName, player.avatarName.c_str(), sizeof(out.avatarName) - 1);
    MimitaVip::copyAppearanceToBytes(
        player.vipAppearance, out.vipTier, out.vipStyleKind,
        out.vipColorR, out.vipColorG, out.vipColorB, out.vipFlags);
    out.vipStyleEpoch = (uint8_t)std::min<uint32_t>(player.vipStyleEpoch, 255);
    if (player.godballActive && player.health > 0) {
        out.stateFlags |= NET_STATE_GODBALL_ACTIVE;
    }
    out.godballX = player.godballX;
    out.godballY = player.godballY;
    out.godballZ = player.godballZ;
    out.godballVx = player.godballVx;
    out.godballVy = player.godballVy;
    out.godballVz = player.godballVz;
    return out;
}


bool serverSpawnSafetySelfTest(std::string& report)
{
    bool ok = true;
    HeadlessWorld world;
    if (!loadHeadlessWorld("assets/maps/tdmtest3.glb", world))
    {
        report += "FAIL: tdmtest3 headless collision did not load\n";
        return false;
    }
    applyAuthoredMapGeometry(world, "tdmtest3");

    for (const ServerSpawnPoint& raw : world.spawnPoints)
    {
        glm::vec3 standing;
        if (serverValidateStandingSpot(world, raw.position, standing))
        {
            if (serverPointInsideWorld(world, standing))
            {
                ok = false;
                report += "FAIL: validated spawn is inside a solid\n";
            }
            if (!serverCapsuleFits(world, standing))
            {
                ok = false;
                report += "FAIL: validated spawn capsule does not fit\n";
            }
        }
    }

    for (uint32_t playerId = 1; playerId <= 2; ++playerId)
    {
        glm::vec3 position(0.0f);
        float yaw = 0.0f;
        size_t index = 0;
        if (!serverPickSafeSpawn(&world, playerId, position, yaw, &index))
        {
            ok = false;
            report += "FAIL: no safe spawn for player " + std::to_string(playerId) + "\n";
            continue;
        }
        if (serverPointInsideWorld(world, position))
        {
            ok = false;
            report += "FAIL: selected spawn is inside a solid for player " +
                      std::to_string(playerId) + "\n";
        }
        if (!serverCapsuleFits(world, position))
        {
            ok = false;
            report += "FAIL: selected spawn capsule does not fit for player " +
                      std::to_string(playerId) + "\n";
        }
        report += "player=" + std::to_string(playerId) +
                  " position=(" + std::to_string(position.x) + "," +
                  std::to_string(position.y) + "," +
                  std::to_string(position.z) + ") index=" +
                  std::to_string(index) + "\n";
    }

    report += ok ? "PASS\n" : "FAIL\n";
    return ok;
}

} // namespace MimitaNet
