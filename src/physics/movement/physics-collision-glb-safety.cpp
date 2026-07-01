#include "physics/movement/physics-collision-glb-safety.h"
#include "physics/movement/physics-collision-shared.h"
#include "physics/movement/physics-collision-glb-sweep.h"
#include "physics/config.h"
#include "world/world.h"
#include "entities/player.h"
#include "debug/debug-log.h"
#include "debug/debug-visuals.h"

#include <cfloat>
#define LOG_COLLISION(K, ...) Debug::logThrottled(Debug::Category::Collision, K, 0.25f, __VA_ARGS__)
#include <cmath>
#include <cstdio>
#include <glm/glm.hpp>
#include <vector>

#define PHYS_LOG(...) Debug::logThrottled(Debug::Category::Collision, "physics-collision", DebugConfig::PRINT_INTERVAL, __VA_ARGS__)

static void applyPostSnapCorrection(Player& p, const World& world, bool& groundedThisFrame)
{
    constexpr float SURFACE_SLOP = 0.01f;
    constexpr float MAX_CORRECTION = 2.0f;
    p.updateModelWorldTransforms();
    Capsule psc = p.getCapsule();
    auto cands = gatherGLBTriangles(world, psc, glm::vec3(0.0f));
    auto contacts = collectCapsuleRecoveryContacts(world, psc, cands);
    if (contacts.empty()) return;
    glm::vec3 corr = solveBatchedCorrection(contacts, SURFACE_SLOP, nullptr, nullptr);
    float clen = glm::length(corr);
    if (clen > MAX_CORRECTION) corr *= MAX_CORRECTION / clen;
    p.pos += corr;
    if (DebugConfig::DEBUG_COLLISION_SYSTEM)
        Debug::log(Debug::Category::Collision, "[COLL]   postSnapCorrection=(%.4f,%.4f,%.4f) contacts=%zu\n", corr.x, corr.y, corr.z, contacts.size());
    DebugVis::recordDepenetration(p.pos - corr, corr, "post-snap-wall-fix");
    for (const auto& c : contacts)
        applyCollisionContact(p, groundedThisFrame, c.normal, c.point, c.penetration, c.triangleIndex, c.label);
}

void doGroundSnap(Player& p, const World& world, bool& groundedThisFrame)
{
    constexpr float SURFACE_SLOP = 0.01f;
    constexpr float MAX_CORRECTION = 2.0f;
    constexpr float GROUND_SNAP_DISTANCE = 0.25f;
    constexpr float MAX_UPWARD_VEL_FOR_SNAP = 1.0f;

    if (DebugConfig::DEBUG_COLLISION_SYSTEM)
        Debug::log(Debug::Category::Collision, "[COLL] doGroundSnap enter vel.z=%.3f maxUp=%.3f\n", p.vel.z, MAX_UPWARD_VEL_FOR_SNAP);

    if (p.vel.z <= MAX_UPWARD_VEL_FOR_SNAP)
    {
        Capsule checkCap = p.getCapsule();
        float feetZ = checkCap.a.z - checkCap.r;

        std::vector<int> groundCandidates = gatherGLBTriangles(world, checkCap, {0, 0, -GROUND_SNAP_DISTANCE});

        if (DebugConfig::DEBUG_COLLISION_SYSTEM)
            Debug::log(Debug::Category::Collision, "[COLL]   groundCandidates=%zu feetZ=%.3f\n", groundCandidates.size(), feetZ);

        float bestGroundZ = -FLT_MAX;
        int bestTri = -1;
        int nRejected = 0;

        for (int triIndex : groundCandidates)
        {
            const CollisionTriangle& tri = world.collisionMesh.triangles[triIndex];
            if (tri.normal.z < MAX_WALKABLE_SLOPE_DOT)
            PHYS_LOG(
                "[SNAP TRI] tri=%d normal=(%.2f %.2f %.2f)",
                triIndex,
                tri.normal.x,
                tri.normal.y,
                tri.normal.z
            );
            {
                ++nRejected;
                if (DebugConfig::DEBUG_COLLISION_SYSTEM && nRejected <= 3)
                    Debug::log(Debug::Category::Collision, "[COLL]   snapReject tri=%d normal=(%.3f,%.3f,%.3f)%s\n",
                           triIndex, tri.normal.x, tri.normal.y, tri.normal.z,
                           tri.normal.z <= 0.0f ? " [BACKFACE]" : "");
                continue;
            }

            glm::vec3 capCenter(checkCap.a.x, checkCap.a.y, 0.0f);
            float planeDist = glm::dot(capCenter - tri.a, tri.normal);
            glm::vec3 proj = capCenter - tri.normal * planeDist;
            PHYS_LOG(
                "[SNAP PROJ] tri=%d planeDist=%.3f proj=(%.2f %.2f %.2f) feet=(%.2f %.2f %.2f)",
                triIndex,
                planeDist,
                proj.x, proj.y, proj.z,
                checkCap.a.x,
                checkCap.a.y,
                feetZ
            );
            if (!pointInTriangle(proj, tri))
            {
                glm::vec3 nearest = closestPointOnTriangle(capCenter, tri.a, tri.b, tri.c);
                float dist2 = glm::dot(nearest - capCenter, nearest - capCenter);
                if (dist2 < (PLAYER_RADIUS * 0.9f) * (PLAYER_RADIUS * 0.9f))
                {
                    if (nearest.z < feetZ && nearest.z > bestGroundZ) {
                        bestGroundZ = nearest.z;
                        bestTri = triIndex;
                    }
                }
                else
                {
                    if (DebugConfig::DEBUG_COLLISION_SYSTEM)
                        Debug::log(Debug::Category::Collision, "[COLL]   snapReject tri=%d reason=outsideRadius dist=%.3f\n",
                               triIndex, sqrtf(dist2));
                }
                continue;
            }

            if (proj.z < feetZ && proj.z > bestGroundZ) {
                bestGroundZ = proj.z;
                bestTri = triIndex;
            }
        }

        if (DebugConfig::DEBUG_COLLISION_SYSTEM)
            Debug::log(Debug::Category::Collision, "[COLL]   snapResult: bestTri=%d bestZ=%.3f feetZ=%.3f dist=%.3f rejected=%d\n",
                   bestTri, bestGroundZ, feetZ, feetZ - bestGroundZ, nRejected);

    LOG_COLLISION("snap_result", "[GROUND SNAP] feetZ=%.3f bestZ=%.3f dist=%.3f totalCand=%zu bestTri=%d",
                  feetZ, bestGroundZ, feetZ - bestGroundZ, groundCandidates.size(), bestTri);

    if (bestGroundZ > -FLT_MAX)
    {
        float distToGround = feetZ - bestGroundZ;
        if (distToGround > 0.0f && distToGround < GROUND_SNAP_DISTANCE)
        {
            float snapAmount = distToGround;
            p.pos.z -= snapAmount;

            if (p.vel.z < 0.0f)
                p.vel.z = 0.0f;

                if (DebugConfig::DEBUG_COLLISION_SYSTEM)
                    Debug::log(Debug::Category::Collision, "[COLL]   SNAPPED down %.4f to groundZ=%.3f (was feetZ=%.3f) tri=%d\n",
                           snapAmount, bestGroundZ, feetZ, bestTri);

                applyPostSnapCorrection(p, world, groundedThisFrame);
            }
        }
    }
}

void doFloorRecovery(Player& p, const World& world, bool& groundedThisFrame)
{
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
            PHYS_LOG(
                "[SNAP TEST] tri=%d a=(%.2f %.2f %.2f) b=(%.2f %.2f %.2f) c=(%.2f %.2f %.2f)",
                triIndex,
                tri.a.x, tri.a.y, tri.a.z,
                tri.b.x, tri.b.y, tri.b.z,
                tri.c.x, tri.c.y, tri.c.z
            );
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
                groundedThisFrame = true;
                PHYS_LOG("[PHYS][FLOOR RECOVERY] feet was %.3f below floor (tri=%d closestZ=%.3f), lifted %.4f\n",
                         bestFloorZ - feetZ, bestFloorTri, bestFloorZ, lift);
            }
        }
    }
}

void doRotationSafetyPass(Player& p, const World& world, bool& groundedThisFrame, CollisionTraceSnapshot& trace)
{
    constexpr float SURFACE_SLOP = 0.01f;
    constexpr float MAX_CORRECTION = 2.0f;

    {
        p.updateModelWorldTransforms();
        Capsule safetyCap = p.getCapsule();
        std::vector<int> safetyCandidates = gatherGLBTriangles(world, safetyCap, glm::vec3(0.0f));
        std::vector<RecoveryContact> safetyContacts = collectCapsuleRecoveryContacts(
            world, safetyCap, safetyCandidates
        );
        trace.finalSafetyContacts = std::max(trace.finalSafetyContacts, (int)safetyContacts.size());

        if (!safetyContacts.empty())
        {
            float maxPen = 0.0f;
            int srcTri = -1;
            glm::vec3 srcNormal(0.0f);
            for (const auto& c : safetyContacts)
            {
                if (c.penetration > maxPen)
                {
                    maxPen = c.penetration;
                    srcTri = c.triangleIndex;
                    srcNormal = c.normal;
                }
            }
            trace.maxPenetration = std::max(trace.maxPenetration, maxPen);

            PHYS_LOG(
                "[COLLISION] rotation-safety: capsule penetration=%.4f contacts=%zu triangleId=%d normal=(%.3f %.3f %.3f)\n",
                maxPen, safetyContacts.size(), srcTri,
                srcNormal.x, srcNormal.y, srcNormal.z
            );

            glm::vec3 safetyCorrection = solveBatchedCorrection(safetyContacts, SURFACE_SLOP);
            float corrLen = glm::length(safetyCorrection);
            if (corrLen > MAX_CORRECTION)
                safetyCorrection *= MAX_CORRECTION / corrLen;

            p.pos += safetyCorrection;
            DebugVis::recordDepenetration(p.pos - safetyCorrection, safetyCorrection, "rotation-safety");

            for (const RecoveryContact& c : safetyContacts)
            {
                if (c.normal.z <= MAX_WALKABLE_SLOPE_DOT)
                    projectVelocityAgainstNormal(p, c.normal);
                applyCollisionContact(p, groundedThisFrame, c.normal, c.point, c.penetration, c.triangleIndex, "rotation-safety");
            }
        }
    }
}

void doFinalSafetyPass(Player& p, const World& world, CollisionTraceSnapshot& trace)
{
    constexpr float SURFACE_SLOP = 0.01f;
    constexpr float MAX_CORRECTION = 2.0f;

    {
        p.updateModelWorldTransforms();
        Capsule finalCap = p.getCapsule();
        std::vector<int> finalCandidates = gatherGLBTriangles(world, finalCap, glm::vec3(0.0f));
        std::vector<RecoveryContact> finalSafetyContacts = collectCapsuleRecoveryContacts(
            world, finalCap, finalCandidates
        );
        trace.finalSafetyContacts = std::max(trace.finalSafetyContacts, (int)finalSafetyContacts.size());

        if (!finalSafetyContacts.empty())
        {
            float finalMaxPen = 0.0f;
            for (const RecoveryContact& fc : finalSafetyContacts)
                finalMaxPen = std::max(finalMaxPen, fc.penetration);
            trace.maxPenetration = std::max(trace.maxPenetration, finalMaxPen);

            if (finalMaxPen > COLLISION_SKIN * 0.5f)
            {
                PHYS_LOG(
                    "[COLLISION FINAL SAFETY] penetration=%.4f contacts=%zu pos=(%.2f %.2f %.2f)\n",
                    finalMaxPen, finalSafetyContacts.size(), p.pos.x, p.pos.y, p.pos.z);

                glm::vec3 finalCorrection = solveBatchedCorrection(finalSafetyContacts, SURFACE_SLOP);
                float finalCorrLen = glm::length(finalCorrection);
                if (finalCorrLen > MAX_CORRECTION)
                    finalCorrection *= MAX_CORRECTION / finalCorrLen;
                p.pos += finalCorrection;

                for (const RecoveryContact& fc : finalSafetyContacts)
                {
                    if (fc.normal.z <= MAX_WALKABLE_SLOPE_DOT)
                        projectVelocityAgainstNormal(p, fc.normal);
                }
            }
        }
    }
}
