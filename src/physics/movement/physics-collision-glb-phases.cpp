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

void glbPhaseFinalSafety(Player& p, const World& world, bool& groundedThisFrame,
    float dt, CollisionTraceSnapshot& trace)
{
    constexpr float SURFACE_SLOP = 0.01f;
    constexpr float MAX_CORRECTION = 2.0f;
    constexpr float MAX_WALKABLE_SLOPE_DOT = 0.7f;
    constexpr float COLLISION_SKIN = 0.01f;

    p.updateModelWorldTransforms();
    Capsule cap = p.getCapsule();
    std::vector<int> candidates = gatherGLBTriangles(world, cap, glm::vec3(0.0f));

    std::vector<RecoveryContact> finalContacts = collectCapsuleRecoveryContacts(
        world, cap, candidates
    );
    trace.finalContacts = (int)finalContacts.size();
    for (const RecoveryContact& c : finalContacts)
        trace.maxPenetration = std::max(trace.maxPenetration, c.penetration);

    for (const RecoveryContact& c : finalContacts)
    {
        if (c.normal.z > MAX_WALKABLE_SLOPE_DOT)
            continue;

        glm::vec3 beforeVel = p.vel;
        projectVelocityAgainstNormal(p, c.normal);

        if (DebugConfig::DEBUG_COLLISION_SYSTEM && glm::length(beforeVel - p.vel) > 0.01f) {
            Debug::log(Debug::Category::Collision,
                "[COLLISION] triangleId=%d penetration=%.4f normal=(%.3f %.3f %.3f) contact=(%.3f %.3f %.3f)\n",
                c.triangleIndex, c.penetration,
                c.normal.x, c.normal.y, c.normal.z,
                c.point.x, c.point.y, c.point.z);
        }
    }

    {
        static int stuckFrames = 0;
        static glm::vec3 lastStuckPos(0.0f);
        static float stuckLogTimer = 0.0f;
        stuckLogTimer += dt;
        float moveLen = glm::length(p.vel * dt + p.externalImpulse * dt);
        float posDelta = glm::length(p.pos - lastStuckPos);
        if (moveLen > 0.01f && posDelta < 0.005f)
        {
            stuckFrames++;
            if (stuckFrames >= 3 && stuckLogTimer >= 0.5f) {
                Capsule dcap = p.getCapsule();
                std::vector<int> dcandidates = gatherGLBTriangles(world, dcap, glm::vec3(0.0f));
                printf("[COLLISION STUCK] frames=%d move=%.4f delta=%.4f pos=(%.2f %.2f %.2f) vel=(%.2f %.2f %.2f) candidates=%zu grounded=%d\n",
                       stuckFrames, moveLen, posDelta,
                       p.pos.x, p.pos.y, p.pos.z,
                       p.vel.x, p.vel.y, p.vel.z,
                       dcandidates.size(), (int)groundedThisFrame);
                for (int ci = 0; ci < (int)dcandidates.size() && ci < 5; ++ci) {
                    const CollisionTriangle& tri = world.collisionMesh.triangles[dcandidates[ci]];
                    printf("  tri=%d normal=(%.3f %.3f %.3f) a=(%.2f %.2f %.2f) b=(%.2f %.2f %.2f) c=(%.2f %.2f %.2f)\n",
                           dcandidates[ci], tri.normal.x, tri.normal.y, tri.normal.z,
                           tri.a.x, tri.a.y, tri.a.z,
                           tri.b.x, tri.b.y, tri.b.z,
                           tri.c.x, tri.c.y, tri.c.z);
                }
                stuckLogTimer = 0.0f;
            }
        } else {
            stuckFrames = 0;
        }
        lastStuckPos = p.pos;
    }

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

    {
        static int overlapWarnCooldown = 0;
        overlapWarnCooldown--;
        if (candidates.size() >= 3 && overlapWarnCooldown <= 0)
        {
            glm::vec3 avgPos = p.pos;
            int opposingPairs = 0;
            for (size_t i = 0; i < candidates.size() && i < 20; ++i)
            {
                for (size_t j = i + 1; j < candidates.size() && j < 20; ++j)
                {
                    const CollisionTriangle& ti = world.collisionMesh.triangles[candidates[i]];
                    const CollisionTriangle& tj = world.collisionMesh.triangles[candidates[j]];
                    float dotNormals = glm::dot(ti.normal, tj.normal);
                    if (dotNormals < -0.5f)
                    {
                        opposingPairs++;
                        if (opposingPairs <= 3)
                        {
                            glm::vec3 tiCenter = (ti.a + ti.b + ti.c) / 3.0f;
                            glm::vec3 tjCenter = (tj.a + tj.b + tj.c) / 3.0f;
                            float triDist = glm::distance(tiCenter, tjCenter);
                            if (triDist < PLAYER_RADIUS * 2.0f)
                            {
                                DebugVis::recordTriangle(ti, candidates[i], "overlap-warn-tri-A");
                                DebugVis::recordTriangle(tj, candidates[j], "overlap-warn-tri-B");
                            }
                        }
                    }
                }
            }
            if (opposingPairs > 0)
            {
                PHYS_LOG("[PHYS][OVERLAP WARN] %d opposing normal pairs near player. pos=(%.2f %.2f %.2f)\n",
                         opposingPairs, p.pos.x, p.pos.y, p.pos.z);
                overlapWarnCooldown = 30;
            }
        }
    }
}
