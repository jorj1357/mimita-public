#include "npc-navigation.h"
#include "npc.h"

#include <glm/gtc/constants.hpp>

static constexpr float COVER_CHECK_DIST = 4.0f;

#include "physics/movement/physics-collision.h"
#include "physics/physics-types.h"
#include "world/world.h"
#include "npc/npc-internal.h"

namespace {

bool rayTriangleIntersect(glm::vec3 origin, glm::vec3 dir, const CollisionTriangle& tri, float maxT, float& outT)
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

} // anonymous namespace

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

    float checkDist = 1.5f;
    float stepAngle = glm::pi<float>() / 6.0f;

    if (rayHitsAny(origin, desiredDir, checkDist, candidates, world))
    {
        for (int side = 0; side < 4; ++side)
        {
            float angle = stepAngle * (side + 1);
            for (float sign : {1.0f, -1.0f})
            {
                glm::vec3 altDir = desiredDir;
                float c = std::cos(angle * sign);
                float s = std::sin(angle * sign);
                altDir = {altDir.x * c - altDir.y * s, altDir.x * s + altDir.y * c, 0.0f};

                if (!rayHitsAny(origin, altDir, checkDist, candidates, world))
                    return altDir;
            }
        }
        glm::vec3 perp{-desiredDir.y, desiredDir.x, 0.0f};
        if (!rayHitsAny(origin, perp, checkDist, candidates, world))
            return perp;
        return -perp;
    }

    return desiredDir;
}

glm::vec3 NpcNavigation::wallAvoidDirection(const Npc& npc, glm::vec3 desiredDir, const World& world)
{
    std::vector<int> candidates;
    gatherNear(world, npc.body.pos + glm::vec3(0.0f, 0.0f, 0.5f), 2.5f, candidates);
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
    std::vector<int> candidates;
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
    std::vector<int> candidates;
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
    std::vector<int> allCandidates;
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
    std::vector<int> candidates;
    gatherNear(world, npc.body.pos + glm::vec3(0.0f, 0.0f, 0.5f), checkDist + 1.0f, candidates);
    return obstacleInDirection(npc, dir, checkDist, world, candidates);
}
