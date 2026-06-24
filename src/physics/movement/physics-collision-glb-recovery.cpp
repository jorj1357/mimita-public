#include "physics/movement/physics-collision-shared.h"
#include "physics/movement/physics-collision-glb-sweep.h"

#include <cstdio>
#include <cmath>
#include <vector>
#include <string>
#include <unordered_set>
#include <glm/glm.hpp>
#include "physics/config.h"
#include "world/world.h"
#include "entities/player.h"
#include "config.h"
#include "debug/debug-log.h"
#include "debug/debug-visuals.h"
#include "config/player-settings.h"
#include "perf/perf.h"
#include "physics/movement/physics-collision.h"

#define PHYS_LOG(...) Debug::logThrottled(Debug::Category::Collision, "physics-collision", DebugConfig::PRINT_INTERVAL, __VA_ARGS__)

extern std::vector<int> gatherGLBTriangles(const World& world, const Capsule& cap, const glm::vec3& move);

void glbPhaseGroundSnap(Player& p, const World& world, bool& groundedThisFrame,
    float dt, CollisionTraceSnapshot& trace)
{
    (void)dt;
    constexpr float SURFACE_SLOP = 0.01f;
    constexpr float MAX_CORRECTION = 2.0f;
    constexpr float GROUND_SNAP_DISTANCE = 0.25f;
    constexpr float MAX_UPWARD_VEL_FOR_SNAP = 1.0f;

    if (p.vel.z <= MAX_UPWARD_VEL_FOR_SNAP)
    {
        Capsule checkCap = p.getCapsule();
        float feetZ = checkCap.a.z - checkCap.r;

        std::vector<int> groundCandidates = gatherGLBTriangles(world, checkCap, {0, 0, -GROUND_SNAP_DISTANCE});
        float bestGroundZ = -FLT_MAX;

        for (int triIndex : groundCandidates)
        {
            const CollisionTriangle& tri = world.collisionMesh.triangles[triIndex];
            if (tri.normal.z < MAX_WALKABLE_SLOPE_DOT)
            {
                PHYS_LOG("[GROUND SNAP REJECT] tri=%d reason=non_walkable_normal normal=(%.3f %.3f %.3f) feetZ=%.3f pos=(%.3f %.3f %.3f)\n",
                    triIndex, tri.normal.x, tri.normal.y, tri.normal.z, feetZ,
                    p.pos.x, p.pos.y, p.pos.z);
                continue;
            }

            glm::vec3 capCenter(checkCap.a.x, checkCap.a.y, 0.0f);
            float planeDist = glm::dot(capCenter - tri.a, tri.normal);
            glm::vec3 proj = capCenter - tri.normal * planeDist;
            if (!pointInTriangle(proj, tri))
            {
                glm::vec3 nearest = closestPointOnTriangle(capCenter, tri.a, tri.b, tri.c);
                float dist2 = glm::dot(nearest - capCenter, nearest - capCenter);
                if (dist2 < (PLAYER_RADIUS * 0.9f) * (PLAYER_RADIUS * 0.9f))
                {
                    if (nearest.z < feetZ && nearest.z > bestGroundZ)
                        bestGroundZ = nearest.z;
                }
                else
                {
                    PHYS_LOG("[GROUND SNAP REJECT] tri=%d reason=outside_capsule_radius nearestDist=%.3f capRadius=%.3f feetZ=%.3f pos=(%.3f %.3f %.3f)\n",
                        triIndex, sqrtf(dist2), PLAYER_RADIUS * 0.9f, feetZ,
                        p.pos.x, p.pos.y, p.pos.z);
                }
                continue;
            }

            {
                if (proj.z < feetZ && proj.z > bestGroundZ)
                    bestGroundZ = proj.z;
            }
        }

        if (bestGroundZ > -FLT_MAX)
        {
            float distToGround = feetZ - bestGroundZ;
            if (distToGround > 0.0f && distToGround < GROUND_SNAP_DISTANCE)
            {
                float snapAmount = distToGround;
                p.pos.z -= snapAmount;

                if (p.vel.z < 0.0f)
                    p.vel.z = 0.0f;

                PHYS_LOG("[PHYS][GROUND SNAP] snapped %.4f to ground at %.2f (dist=%.4f)\n", snapAmount, bestGroundZ, distToGround);
                if (DebugConfig::DEBUG_PHYSICS)
                    Debug::log(Debug::Category::Physics, "[SNAP] distToGround=%.4f snap=%.4f\n",
                        distToGround, snapAmount);

                {
                    p.updateModelWorldTransforms();
                    Capsule postSnapCap = p.getCapsule();
                    std::vector<int> postSnapCandidates = gatherGLBTriangles(world, postSnapCap, glm::vec3(0.0f));
                    std::vector<RecoveryContact> postSnapContacts = collectCapsuleRecoveryContacts(
                        world, postSnapCap, postSnapCandidates
                    );

                    if (!postSnapContacts.empty())
                    {
                        glm::vec3 snapCorrection = solveBatchedCorrection(postSnapContacts, SURFACE_SLOP, nullptr, nullptr);
                        float snapCorrLen = glm::length(snapCorrection);
                        if (snapCorrLen > MAX_CORRECTION)
                            snapCorrection *= MAX_CORRECTION / snapCorrLen;
                        p.pos += snapCorrection;
                        DebugVis::recordDepenetration(p.pos - snapCorrection, snapCorrection, "post-snap-wall-fix");

                        for (const RecoveryContact& c : postSnapContacts)
                        {
                            applyCollisionContact(
                                p, groundedThisFrame,
                                c.normal, c.point, c.penetration,
                                c.triangleIndex, c.label
                            );
                        }
                    }
                }
            }
        }
    }
}

void glbPhaseFloorRecovery(Player& p, const World& world)
{
    p.updateModelWorldTransforms();
    Capsule fCap = p.getCapsule();
    float feetZ = fCap.a.z - fCap.r;
    std::vector<int> fCandidates = gatherGLBTriangles(world, fCap, glm::vec3(0.0f));

    float bestFloorZ = -FLT_MAX;
    int bestFloorTri = -1;
    for (int triIndex : fCandidates)
    {
        const CollisionTriangle& tri = world.collisionMesh.triangles[triIndex];
        if (tri.normal.z < MAX_WALKABLE_SLOPE_DOT) continue;
        glm::vec3 footPoint(fCap.a.x, fCap.a.y, feetZ);
        glm::vec3 closest = closestPointOnTriangle(footPoint, tri.a, tri.b, tri.c);
        float horizDist2 = (closest.x - fCap.a.x) * (closest.x - fCap.a.x)
                         + (closest.y - fCap.a.y) * (closest.y - fCap.a.y);
        if (horizDist2 > fCap.r * fCap.r) continue;
        if (closest.z > bestFloorZ && closest.z < fCap.a.z + 1.0f)
        {
            bestFloorZ = closest.z;
            bestFloorTri = triIndex;
        }
    }

    if (bestFloorZ > -FLT_MAX && feetZ < bestFloorZ - 0.01f)
    {
        float lift = bestFloorZ - feetZ + 0.005f;
        if (lift > 0.0f && lift < 0.5f)
        {
            p.pos.z += lift;
            p.externalImpulse.z = std::min(p.externalImpulse.z, 0.0f);
            p.vel.z = std::min(p.vel.z, 0.0f);
            PHYS_LOG("[PHYS][FLOOR RECOVERY] feet was %.3f below floor (tri=%d closestZ=%.3f), lifted %.4f\n",
                     bestFloorZ - feetZ, bestFloorTri, bestFloorZ, lift);
        }
    }
}

void glbPhaseEmergencyStuck(Player& p, const World& world, float dt,
    CollisionTraceSnapshot& trace)
{
    constexpr float STUCK_THRESHOLD = 0.05f;
    const float EMERGENCY_SEARCH_RADIUS = PLAYER_HEIGHT + PLAYER_RADIUS * 4.0f;

    p.updateModelWorldTransforms();
    Capsule stuckCheckCap = p.getCapsule();
    std::vector<int> stuckCandidates = gatherGLBTriangles(world, stuckCheckCap, glm::vec3(0.0f));
    std::vector<RecoveryContact> stuckContacts = collectCapsuleRecoveryContacts(
        world, stuckCheckCap, stuckCandidates
    );

    float worstPen = 0.0f;
    for (const auto& c : stuckContacts)
        worstPen = std::max(worstPen, c.penetration);

    if (worstPen > STUCK_THRESHOLD)
    {
        p.collisionStuckFrames++;
        if (p.collisionStuckFrames >= 3)
        {
            PHYS_LOG("[PHYS][EMERGENCY] Deep penetration %.4f for %d frames. Searching escape.\n",
                     worstPen, p.collisionStuckFrames);

            const glm::vec3 searchDirs[] = {
                { 1, 0, 0}, {-1, 0, 0}, { 0, 1, 0}, { 0,-1, 0},
                { 0, 0, 1}, { 1, 1, 0}, { 1,-1, 0}, {-1, 1, 0}, {-1,-1, 0},
                { 1, 0, 1}, {-1, 0, 1}, { 0, 1, 1}, { 0,-1, 1}
            };

            glm::vec3 bestPos = p.pos;
            float bestPen = worstPen;
            bool foundFree = false;

            for (glm::vec3 dir : searchDirs)
            {
                if (glm::length(dir) > 0.001f)
                    dir = glm::normalize(dir);

                for (float dist = 0.05f; dist <= EMERGENCY_SEARCH_RADIUS; dist += 0.05f)
                {
                    glm::vec3 testPos = p.pos + dir * dist;
                    Player testP = p;
                    testP.pos = testPos;
                    Capsule testCap = testP.getCapsule();
                    std::vector<int> testCandidates = gatherGLBTriangles(world, testCap, glm::vec3(0.0f));

                    std::vector<RecoveryContact> testContacts;
                    {
                        std::vector<int> triCandidates = testCandidates;
                        std::vector<glm::vec3> emptySamples;
                        testContacts = collectGLBRecoveryContacts(
                            world, testCap, emptySamples, triCandidates, BODY_SAMPLE_RADIUS
                        );
                    }

                    float testPen = 0.0f;
                    for (const auto& tc : testContacts)
                        testPen = std::max(testPen, tc.penetration);

                    if (testPen < 0.01f)
                    {
                        bestPos = testPos;
                        bestPen = testPen;
                        foundFree = true;
                        break;
                    }
                    if (testPen < bestPen)
                    {
                        bestPos = testPos;
                        bestPen = testPen;
                    }
                }
                if (foundFree) break;
            }

            if (foundFree || bestPen < worstPen)
            {
                DebugVis::recordDepenetration(p.pos, bestPos - p.pos, "emergency-stuck-escape");
                PHYS_LOG("[PHYS][EMERGENCY] Escaped: pen %.4f -> %.4f, move=(%.4f %.4f %.4f)\n",
                         worstPen, bestPen,
                         bestPos.x - p.pos.x, bestPos.y - p.pos.y, bestPos.z - p.pos.z);
                p.pos = bestPos;
                trace.emergencyEscaped = true;

                p.vel = glm::vec3(0.0f);
                p.collisionStuckFrames = 0;
            }
        }
    }
    else
    {
        p.collisionStuckFrames = 0;
    }
}
