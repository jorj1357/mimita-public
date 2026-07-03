#include "physics/movement/physics-collision-glb-safety.h"
#include "physics/movement/physics-collision-shared.h"
#include "physics/movement/physics-collision-glb-sweep.h"
#include "physics/config.h"
#include "world/world.h"
#include "entities/player.h"
#include "debug/debug-log.h"
#include "debug/debug-visuals.h"

#include <chrono>
#include <cfloat>
#define LOG_COLLISION(K, ...) Debug::logThrottled(Debug::Category::Collision, K, 0.25f, __VA_ARGS__)
#include <cmath>
#include <cstdio>
#include <glm/glm.hpp>
#include <vector>

#define SAFETY_LOG(...) Debug::logThrottled(Debug::Category::Collision, "safety-pass", 1.0f, __VA_ARGS__)
#define PHYS_LOG(...) Debug::logThrottled(Debug::Category::Collision, "physics-collision", DebugConfig::PRINT_INTERVAL, __VA_ARGS__)

static void applyPostSnapCorrection(Player& p, const World& world, bool& groundedThisFrame)
{
    constexpr float SURFACE_SLOP = 0.01f;
    constexpr float MAX_CORRECTION = 2.0f;
    p.updateModelWorldTransforms();
    Capsule psc = p.getCapsule();
    auto cands = gatherGLBTriangles(world, psc, glm::vec3(0.0f), "Player_Capsule_GatherCheck");
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
    auto t0 = std::chrono::steady_clock::now();
    constexpr float SURFACE_SLOP = 0.01f;
    constexpr float MAX_CORRECTION = 2.0f;
    constexpr float GROUND_SNAP_DISTANCE = 0.25f;
    constexpr float MAX_UPWARD_VEL_FOR_SNAP = 1.0f;

    if (p.vel.z > MAX_UPWARD_VEL_FOR_SNAP) {
        SAFETY_LOG("[GROUND SNAP] skipped vel.z=%.3f > maxUp=%.3f\n", p.vel.z, MAX_UPWARD_VEL_FOR_SNAP);
        return;
    }

    Capsule checkCap = p.getCapsule();
    float feetZ = checkCap.a.z - checkCap.r;

    std::vector<int> groundCandidates = gatherGLBTriangles(world, checkCap, {0, 0, -GROUND_SNAP_DISTANCE}, "Player_Capsule_GroundSnap");

    float bestGroundZ = -FLT_MAX;
    int bestTri = -1;
    int nRejected = 0;

    for (int triIndex : groundCandidates)
    {
        const CollisionTriangle& tri = world.collisionMesh.triangles[triIndex];
        if (tri.normal.z < MAX_WALKABLE_SLOPE_DOT)
        {
            ++nRejected;
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
                if (nearest.z < feetZ && nearest.z > bestGroundZ) {
                    bestGroundZ = nearest.z;
                    bestTri = triIndex;
                }
            }
            continue;
        }

        if (proj.z < feetZ && proj.z > bestGroundZ) {
            bestGroundZ = proj.z;
            bestTri = triIndex;
        }
    }

    bool snapped = false;
    if (bestGroundZ > -FLT_MAX)
    {
        float distToGround = feetZ - bestGroundZ;
        if (distToGround > 0.0f && distToGround < GROUND_SNAP_DISTANCE)
        {
            float snapAmount = distToGround;
            p.pos.z -= snapAmount;
            if (p.vel.z < 0.0f)
                p.vel.z = 0.0f;
            applyPostSnapCorrection(p, world, groundedThisFrame);
            snapped = true;
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    float elapsedMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
    SAFETY_LOG(
        "[GROUND SNAP] candidates=%zu feetZ=%.3f bestZ=%.3f snapped=%d bestTri=%d rejected=%d elapsedMs=%.2f\n",
        groundCandidates.size(), feetZ, bestGroundZ, (int)snapped, bestTri, nRejected, elapsedMs);
}

void doFloorRecovery(Player& p, const World& world, bool& groundedThisFrame)
{
    auto t0 = std::chrono::steady_clock::now();
    {
        p.updateModelWorldTransforms();
        Capsule fCap = p.getCapsule();
        float feetZ = fCap.a.z - fCap.r;
        std::vector<int> fCandidates = gatherGLBTriangles(world, fCap, glm::vec3(0.0f), "Player_Capsule_FloorRecovery");

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

        float liftAmount = 0.0f;
        if (bestFloorZ > -FLT_MAX && feetZ < bestFloorZ - 0.01f)
        {
            float lift = bestFloorZ - feetZ + 0.005f;
            if (lift > 0.0f && lift < 0.5f)
            {
                p.pos.z += lift;
                p.externalImpulse.z = std::min(p.externalImpulse.z, 0.0f);
                p.vel.z = std::min(p.vel.z, 0.0f);
                groundedThisFrame = true;
                liftAmount = lift;
            }
        }

        auto t1 = std::chrono::steady_clock::now();
        float elapsedMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
        SAFETY_LOG(
            "[FLOOR RECOVERY] candidates=%zu feetZ=%.3f bestFloorZ=%.3f lift=%.4f tri=%d elapsedMs=%.2f\n",
            fCandidates.size(), feetZ, bestFloorZ, liftAmount, bestFloorTri, elapsedMs);
    }
}

void doRotationSafetyPass(Player& p, const World& world, bool& groundedThisFrame, CollisionTraceSnapshot& trace)
{
    auto t0 = std::chrono::steady_clock::now();
    constexpr float SURFACE_SLOP = 0.01f;
    constexpr float MAX_CORRECTION = 2.0f;

    {
        p.updateModelWorldTransforms();
        Capsule safetyCap = p.getCapsule();
        std::vector<int> safetyCandidates = gatherGLBTriangles(world, safetyCap, glm::vec3(0.0f), "Player_Capsule_RotationSafety");
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

        auto t1 = std::chrono::steady_clock::now();
        float elapsedMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
        SAFETY_LOG(
            "[ROTATION SAFETY] candidates=%zu contacts=%zu maxPen=%.4f elapsedMs=%.2f\n",
            safetyCandidates.size(), safetyContacts.size(), trace.maxPenetration, elapsedMs);
    }
}

void doFinalSafetyPass(Player& p, const World& world, CollisionTraceSnapshot& trace)
{
    auto t0 = std::chrono::steady_clock::now();
    constexpr float SURFACE_SLOP = 0.01f;
    constexpr float MAX_CORRECTION = 2.0f;

    {
        p.updateModelWorldTransforms();
        Capsule finalCap = p.getCapsule();
        std::vector<int> finalCandidates = gatherGLBTriangles(world, finalCap, glm::vec3(0.0f), "Player_Capsule_FinalSafety");
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

        auto t1 = std::chrono::steady_clock::now();
        float elapsedMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
        SAFETY_LOG(
            "[FINAL SAFETY] candidates=%zu contacts=%zu maxPen=%.4f elapsedMs=%.2f\n",
            finalCandidates.size(), finalSafetyContacts.size(), trace.maxPenetration, elapsedMs);
    }
}
