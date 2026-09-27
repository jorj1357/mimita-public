#include "npc-navigation.h"
#include "npc.h"

#include <glm/gtc/constants.hpp>

static constexpr float COVER_CHECK_DIST = 4.0f;

#include "physics/movement/physics-collision.h"
#include "physics/physics-types.h"
#include "world/world.h"
#include "npc/npc-internal.h"
#include "npc/npc-difficulty-config.h"

namespace {

bool rayTriangleIntersect(glm::vec3 origin, glm::vec3 dir, const CollisionTriangle& tri, float maxT, float& outT)
{
    return NpcNavigation::rayTriangle(origin, dir, tri, maxT, outT);
}

static void gatherNear(const World& world, glm::vec3 pos, float radius, std::vector<int>& out) {
    AABB b;
    b.min = pos - glm::vec3(radius);
    b.max = pos + glm::vec3(radius);
    appendChunkTrianglesForAABB(world, b, 0.0f, out, "npcGatherNear");
}

static bool rayHitsAny(glm::vec3 origin, glm::vec3 dir, float maxDist,
                       const std::vector<int>& candidates, const World& world,
                       float* outHitDist = nullptr, glm::vec3* outHitNormal = nullptr) {
    float nearest = maxDist;
    bool hit = false;
    glm::vec3 nml{0.0f};
    for (int ti : candidates) {
        if (ti < 0 || ti >= (int)world.collisionMesh.triangles.size()) continue;
        float t;
        if (rayTriangleIntersect(origin, dir, world.collisionMesh.triangles[ti], maxDist, t)) {
            if (t < nearest) {
                nearest = t;
                const CollisionTriangle& tri = world.collisionMesh.triangles[ti];
                nml = glm::normalize(glm::cross(tri.b - tri.a, tri.c - tri.a));
                hit = true;
            }
        }
    }
    if (hit && outHitDist) *outHitDist = nearest;
    if (hit && outHitNormal) *outHitNormal = nml;
    return hit;
}

static bool hasGroundSupport(const Npc& npc, glm::vec3 moveDir,
                             const World& world, float probeDistance,
                             float probeDepth, const std::vector<int>& candidates)
{
    if (!npc.body.ground.onGround)
        return true; // Air movement may intentionally cross a gap.

    moveDir.z = 0.0f;
    const float len = glm::length(moveDir);
    if (len < 0.001f)
        return true;
    moveDir /= len;

    const Capsule capsule = npc.body.getCapsule();
    const glm::vec3 probePos = npc.body.pos + moveDir * probeDistance;
    const glm::vec3 origin = probePos + glm::vec3(0.0f, 0.0f, 1.0f);
    const glm::vec3 down(0.0f, 0.0f, -1.0f);
    float bestZ = -1e6f;

    for (int ti : candidates)
    {
        if (ti < 0 || ti >= (int)world.collisionMesh.triangles.size())
            continue;
        float hitT = 0.0f;
        if (rayTriangleIntersect(origin, down, world.collisionMesh.triangles[ti],
                                 probeDepth + 1.0f, hitT))
            bestZ = std::max(bestZ, origin.z - hitT);
    }

    // Keep the probe tied to the actor's real capsule width. A floor directly
    // under only the center point is not enough if the NPC would hang off an
    // edge with its body radius unsupported.
    if (bestZ <= -1e5f)
        return false;
    const float feetZ = npc.body.pos.z - (capsule.b.z - capsule.a.z) * 0.5f - capsule.r;
    return bestZ >= feetZ - probeDepth;
}

} // anonymous namespace

bool NpcNavigation::rayTriangle(const glm::vec3& origin, const glm::vec3& dir,
                                const CollisionTriangle& tri, float maxT, float& outT)
{
    glm::vec3 e1 = tri.b - tri.a;
    glm::vec3 e2 = tri.c - tri.a;
    glm::vec3 p = glm::cross(dir, e2);
    float det = glm::dot(e1, p);
    if (std::fabs(det) < 0.0001f) return false;
    float invDet = 1.0f / det;
    glm::vec3 tVec = origin - tri.a;
    float u = glm::dot(tVec, p) * invDet;
    if (u < 0.0f || u > 1.0f) return false;
    glm::vec3 q = glm::cross(tVec, e1);
    float v = glm::dot(dir, q) * invDet;
    if (v < 0.0f || u + v > 1.0f) return false;
    outT = glm::dot(e2, q) * invDet;
    return outT > 0.01f && outT < maxT;
}

float NpcNavigation::groundHeightAt(const World& world, const glm::vec3& pos, float searchDist, float radius)
{
    constexpr float NOT_FOUND = -1e6f;
    if (searchDist <= 0.0f)
        return NOT_FOUND;

    static thread_local std::vector<int> candidates;
    candidates.clear();
    AABB b;
    b.min = pos - glm::vec3(radius, radius, searchDist);
    b.max = pos + glm::vec3(radius, radius, 1.0f);
    appendChunkTrianglesForAABB(world, b, 0.0f, candidates, "npcGroundHeight");

    const glm::vec3 origin = pos + glm::vec3(0.0f, 0.0f, 1.0f);
    const glm::vec3 down(0.0f, 0.0f, -1.0f);
    float bestZ = NOT_FOUND;
    for (int ti : candidates)
    {
        if (ti < 0 || ti >= (int)world.collisionMesh.triangles.size())
            continue;
        float t;
        if (rayTriangleIntersect(origin, down, world.collisionMesh.triangles[ti], searchDist, t))
        {
            const float z = origin.z - t;
            if (z > bestZ)
                bestZ = z;
        }
    }
    return bestZ;
}

glm::vec3 NpcNavigation::wallAvoidDirection(const Npc& npc, glm::vec3 desiredDir, const World& world, const std::vector<int>& candidates)
{
    if (glm::length(desiredDir) < 0.001f)
        return desiredDir;

    desiredDir.z = 0.0f;
    float len = glm::length(desiredDir);
    if (len < 0.001f) return desiredDir;
    desiredDir /= len;

    glm::vec3 origin = npc.body.pos;
    origin.z += 0.5f;

    const auto& cfg = NpcDifficultyConfig::instance().settings();
    if (!cfg.wallAvoidanceEnabled)
        return desiredDir;

    const float checkDist = cfg.wallCastDistance;
    const bool forwardBlocked = rayHitsAny(origin, desiredDir, checkDist, candidates, world);
    const bool forwardUnsupported = cfg.wallGroundSupportRequired &&
        !hasGroundSupport(npc, desiredDir, world, checkDist,
                          cfg.wallGroundProbeDepth, candidates);
    if (!forwardBlocked && !forwardUnsupported)
        return desiredDir;

    // Search the closest useful escape direction first. This keeps the NPC
    // moving naturally around a wall, but also allows it to back up when the
    // forward, left, and right directions are all blocked.
    const glm::vec3 left{-desiredDir.y, desiredDir.x, 0.0f};
    const glm::vec3 right = -left;
    const glm::vec3 candidatesByPreference[] = {
        left, right,
        glm::normalize(left - desiredDir),
        glm::normalize(right - desiredDir),
        -desiredDir,
        glm::normalize(-desiredDir + left),
        glm::normalize(-desiredDir + right)
    };

    glm::vec3 bestDir = -desiredDir;
    float bestClearance = -1.0f;
    for (const glm::vec3& altDir : candidatesByPreference)
    {
        if (glm::length(altDir) < 0.001f)
            continue;
        float hitDistance = cfg.wallSearchDistance;
        const bool blocked = rayHitsAny(origin, altDir, cfg.wallSearchDistance,
                                        candidates, world, &hitDistance);
        const bool unsupported = cfg.wallGroundSupportRequired &&
            !hasGroundSupport(npc, altDir, world, cfg.wallSearchDistance,
                              cfg.wallGroundProbeDepth, candidates);
        if (!blocked && !unsupported)
            return glm::normalize(altDir);
        if (!unsupported && hitDistance > bestClearance)
        {
            bestClearance = hitDistance;
            bestDir = glm::normalize(altDir);
        }
    }

    // Every nearby direction is partly blocked. Move toward the direction
    // with the most room so the shared collision solver can make progress;
    // the normal stuck/repath logic will keep retrying on later ticks.
    return bestDir;
}

glm::vec3 NpcNavigation::wallAvoidDirection(const Npc& npc, glm::vec3 desiredDir, const World& world)
{
    static thread_local std::vector<int> candidates;
    candidates.clear();
    const auto& cfg = NpcDifficultyConfig::instance().settings();
    gatherNear(world, npc.body.pos + glm::vec3(0.0f, 0.0f, 0.5f),
               std::max(2.5f, cfg.wallSearchDistance + 0.5f), candidates);
    return wallAvoidDirection(npc, desiredDir, world, candidates);
}

bool NpcNavigation::isStuck(const Npc& npc)
{
    bool tryingMove = glm::length(npc.lastMoveInput) > 0.1f;
    glm::vec3 moved = npc.body.pos - npc.previousPosition;
    float moveSpeed = glm::length(moved);
    bool groundedAndSlow = npc.body.ground.onGround && moveSpeed < 0.02f;
    return tryingMove && groundedAndSlow;
}

glm::vec3 NpcNavigation::unstuckDirection(const Npc& npc, unsigned int& rng, const World& world, const std::vector<int>& candidates)
{
    glm::vec3 origin = npc.body.pos;
    origin.z += 0.5f;

    struct DirScore {
        glm::vec3 dir;
        float maxDist;
    };
    DirScore best{{1.0f, 0.0f, 0.0f}, 0.0f};

    for (int i = 0; i < 8; ++i)
    {
        float angle = random01(rng) * glm::two_pi<float>();
        glm::vec3 testDir{std::cos(angle), std::sin(angle), 0.0f};

        float closest = 10.0f;
        for (int ti : candidates)
        {
            if (ti < 0 || ti >= (int)world.collisionMesh.triangles.size()) continue;
            float t;
            if (rayTriangleIntersect(origin, testDir, world.collisionMesh.triangles[ti], 10.0f, t))
                closest = std::min(closest, t);
        }
        if (closest > best.maxDist)
        {
            best.dir = testDir;
            best.maxDist = closest;
        }
    }

    return best.dir;
}

glm::vec3 NpcNavigation::unstuckDirection(const Npc& npc, unsigned int& rng, const World& world)
{
    static thread_local std::vector<int> candidates;
    candidates.clear();
    gatherNear(world, npc.body.pos + glm::vec3(0.0f, 0.0f, 0.5f), 10.0f, candidates);
    return unstuckDirection(npc, rng, world, candidates);
}

bool NpcNavigation::isClimbableWall(const Npc& npc, glm::vec3 moveDir, const World& world, glm::vec3& outWallNormal, const std::vector<int>& candidates)
{
    if (glm::length(moveDir) < 0.001f)
        return false;

    glm::vec3 origin = npc.body.pos;
    origin.z += 0.5f;

    glm::vec3 checkDir = glm::normalize(glm::vec3(moveDir.x, moveDir.y, 0.0f));
    float checkDist = 1.2f;

    // Raise origin for upper-body check (wall climb needs a wall at chest height)
    origin.z += 0.6f;

    float hitDist;
    glm::vec3 hitNormal;
    if (rayHitsAny(origin, checkDir, checkDist, candidates, world, &hitDist, &hitNormal))
    {
        // The wall must be mostly vertical (normal dot up ≈ 0)
        if (std::fabs(hitNormal.z) < 0.3f && hitDist < checkDist * 0.9f)
        {
            outWallNormal = glm::normalize(glm::vec3(hitNormal.x, hitNormal.y, 0.0f));
            return true;
        }
    }

    return false;
}

bool NpcNavigation::isClimbableWall(const Npc& npc, glm::vec3 moveDir, const World& world, glm::vec3& outWallNormal)
{
    glm::vec3 origin = npc.body.pos;
    origin.z += 1.1f;
    static thread_local std::vector<int> candidates;
    candidates.clear();
    gatherNear(world, origin, 2.2f, candidates);
    return isClimbableWall(npc, moveDir, world, outWallNormal, candidates);
}

glm::vec3 NpcNavigation::findCoverDirection(const Npc& npc, glm::vec3 threatPos, const World& world)
{
    glm::vec3 fromNpc = threatPos - npc.body.pos;
    float threatDist = glm::length(fromNpc);
    if (threatDist < 1.0f) return glm::vec3(0.0f);
    glm::vec3 toThreat = fromNpc / threatDist;

    // Test directions: perpendicular (left/right) and backward
    glm::vec3 perpL(-toThreat.y, toThreat.x, 0.0f);
    glm::vec3 perpR(toThreat.y, -toThreat.x, 0.0f);
    glm::vec3 back(-toThreat.x, -toThreat.y, 0.0f);

    glm::vec3 testDirs[] = {perpL, perpR, back, perpL * 0.7f + back * 0.3f, perpR * 0.7f + back * 0.3f};

    glm::vec3 origin = npc.body.pos;
    origin.z += 0.8f;

    // Gather nearby triangles only — cover test positions are within 4m of NPC.
    // There is no need to include the distant threat position in the query.
    static thread_local std::vector<int> allCandidates;
    allCandidates.clear();
    gatherNear(world, origin, COVER_CHECK_DIST + 4.0f, allCandidates);

    for (const auto& dir : testDirs)
    {
        glm::vec3 testPos = npc.body.pos + dir * COVER_CHECK_DIST;
        testPos.z += 0.8f;

        // Check if this position has LOS blocked to the threat
        glm::vec3 toThreatFromCover = threatPos - testPos;
        float coverToThreat = glm::length(toThreatFromCover);
        if (coverToThreat < 1.0f) continue;
        toThreatFromCover /= coverToThreat;

        for (int ti : allCandidates)
        {
            if (ti < 0 || ti >= (int)world.collisionMesh.triangles.size()) continue;
            const CollisionTriangle& tri = world.collisionMesh.triangles[ti];
            glm::vec3 e1 = tri.b - tri.a;
            glm::vec3 e2 = tri.c - tri.a;
            glm::vec3 pVec = glm::cross(toThreatFromCover, e2);
            float det = glm::dot(e1, pVec);
            if (std::fabs(det) < 0.0001f) continue;
            float invDet = 1.0f / det;
            glm::vec3 tVec = testPos - tri.a;
            float u = glm::dot(tVec, pVec) * invDet;
            if (u < 0.0f || u > 1.0f) continue;
            glm::vec3 qVec = glm::cross(tVec, e1);
            float v = glm::dot(toThreatFromCover, qVec) * invDet;
            if (v < 0.0f || u + v > 1.0f) continue;
            float t = glm::dot(e2, qVec) * invDet;
            // If a triangle blocks LOS near the cover position
            if (t > 0.1f && t < coverToThreat - 1.0f)
            {
                // Verify the direction is walkable (no wall right in front)
                if (!obstacleInDirection(npc, dir, 1.5f, world, allCandidates))
                    return glm::normalize(dir);
            }
        }
    }

    return glm::vec3(0.0f);
}

bool NpcNavigation::obstacleInDirection(const Npc& npc, glm::vec3 dir, float checkDist, const World& world, const std::vector<int>& candidates)
{
    if (glm::length(dir) < 0.001f)
        return false;

    glm::vec3 origin = npc.body.pos;
    origin.z += 0.5f;
    glm::vec3 checkDir = glm::normalize(glm::vec3(dir.x, dir.y, 0.0f));

    return rayHitsAny(origin, checkDir, checkDist, candidates, world);
}

bool NpcNavigation::obstacleInDirection(const Npc& npc, glm::vec3 dir, float checkDist, const World& world)
{
    static thread_local std::vector<int> candidates;
    candidates.clear();
    gatherNear(world, npc.body.pos + glm::vec3(0.0f, 0.0f, 0.5f), checkDist + 1.0f, candidates);
    return obstacleInDirection(npc, dir, checkDist, world, candidates);
}
