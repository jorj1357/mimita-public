#include "physics/movement/physics-collision-shared.h"
#include "physics/movement/physics-collision-glb-sweep.h"
#include "physics/movement/physics-collision-glb-body.h"
#include "physics/movement/physics-collision-glb-safety.h"
#include "physics/movement/physics-collision-glb-sweep-slide.h"

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

extern std::vector<int> gatherGLBTrianglesForSphere(
    const World& world,
    glm::vec3 center,
    float radius,
    const glm::vec3& move
);

#define PHYS_LOG(...) Debug::logThrottled(Debug::Category::Collision, "physics-collision", DebugConfig::PRINT_INTERVAL, __VA_ARGS__)

void doGLBTriangleCollisions(
    Player& p,
    const World& world,
    bool& groundedThisFrame,
    float dt
) {
    constexpr float SURFACE_SLOP = 0.01f;
    constexpr float MAX_CORRECTION = 2.0f;

    glm::vec3 totalMove = (p.vel + p.externalImpulse) * dt;
    // Clamp per-substep downward movement to prevent tunneling.
    const float maxZStep = PLAYER_RADIUS;
    if (totalMove.z < -maxZStep)
        totalMove.z = -maxZStep;
    glm::vec3 remainingMove;
    CollisionTraceSnapshot trace;
    std::vector<int> candidates;

    doGLBSweepSlide(p, world, groundedThisFrame, dt, totalMove, remainingMove, trace, candidates);

    if (DebugConfig::DEBUG_COLLISION_SYSTEM)
        printf("[COLL] after sweepSlide: pos=(%.2f,%.2f,%.2f) gnd=%d remaining=(%.4f,%.4f,%.4f) vel=(%.2f,%.2f,%.2f)\n",
               p.pos.x, p.pos.y, p.pos.z, (int)groundedThisFrame,
               remainingMove.x, remainingMove.y, remainingMove.z,
               p.vel.x, p.vel.y, p.vel.z);

    Capsule cap = p.getCapsule();
    p.updateModelWorldTransforms();
    cap = p.getCapsule();
    candidates = gatherGLBTriangles(world, cap, glm::vec3(0.0f));

    float maxPenetrationSeen = 0.0f;

    for (int depenIter = 0; depenIter < 4; ++depenIter)
    {
        p.updateModelWorldTransforms();
        cap = p.getCapsule();
        candidates = gatherGLBTriangles(world, cap, glm::vec3(0.0f));

        std::vector<RecoveryContact> contacts = collectCapsuleRecoveryContacts(
            world, cap, candidates
        );
        trace.maxRecoveryContacts = std::max(trace.maxRecoveryContacts, (int)contacts.size());

        if (contacts.empty())
            break;

        float iterMaxPen = 0.0f;
        glm::vec3 correction = solveBatchedCorrection(contacts, SURFACE_SLOP, &iterMaxPen, nullptr, totalMove, p.pos);
        maxPenetrationSeen = std::max(maxPenetrationSeen, iterMaxPen);
        trace.maxPenetration = std::max(trace.maxPenetration, iterMaxPen);

        float corrLen = glm::length(correction);
        if (corrLen > MAX_CORRECTION)
            correction *= MAX_CORRECTION / corrLen;

        p.pos += correction;

        if (DebugConfig::DEBUG_COLLISION_SYSTEM)
            printf("[COLL]   depen iter=%d contacts=%zu maxPen=%.4f correction=(%.4f,%.4f,%.4f) pos=(%.2f,%.2f,%.2f)\n",
                   depenIter, contacts.size(), iterMaxPen,
                   correction.x, correction.y, correction.z,
                   p.pos.x, p.pos.y, p.pos.z);

        for (const RecoveryContact& c : contacts)
        {
            applyCollisionContact(
                p, groundedThisFrame,
                c.normal, c.point, c.penetration,
                c.triangleIndex, c.label
            );
        }

        DebugVis::recordDepenetration(p.pos - correction, correction, "glb-batched-depen");

        PHYS_LOG(
            "[PHYS][GLB DEPEN] iter=%d contacts=%zu maxPen=%.4f correction=(%.4f %.4f %.4f)\n",
            depenIter, contacts.size(), iterMaxPen,
            correction.x, correction.y, correction.z
        );

        if (glm::dot(correction, correction) < 0.0000001f)
            break;
    }

    {
        p.updateModelWorldTransforms();
        glm::vec3 curMove = remainingMove;
        if (glm::length(curMove) > 0.001f)
        {
            Capsule resweepCap = p.getCapsule();
            std::vector<int> resweepCandidates = gatherGLBTriangles(world, resweepCap, curMove);
            SweepHit resweepHit;
            resweepHit.time = 1.0f;
            for (int triIndex : resweepCandidates)
            {
                const CollisionTriangle& tri = world.collisionMesh.triangles[triIndex];
                SweepHit hit;
                if (capsuleTriangleSweep(resweepCap, curMove, tri, triIndex, hit) &&
                    !rejectBelowTopFaceContact(resweepCap, tri, hit.normal, hit.point, triIndex, "post-depen-resweep") &&
                    hit.time < resweepHit.time)
                    resweepHit = hit;
            }

            if (resweepHit.hit && resweepHit.time < 1.0f)
            {
                trace.resweepHits++;
                glm::vec3 resweepStep = curMove * resweepHit.time;
                p.pos += resweepStep;
                p.pos += resweepHit.normal * SURFACE_SLOP;
                p.updateModelWorldTransforms();

                glm::vec3 afterStep = curMove - resweepStep;
                float into = glm::dot(afterStep, resweepHit.normal);
                if (into < 0.0f)
                    afterStep -= resweepHit.normal * into;
                p.pos += afterStep;
                p.updateModelWorldTransforms();

                applyCollisionContact(
                    p, groundedThisFrame,
                    resweepHit.normal, resweepHit.point, SURFACE_SLOP,
                    resweepHit.triangleIndex, "post-depen-resweep");
            }
            else
            {
                p.pos += curMove;
                p.updateModelWorldTransforms();
            }
            remainingMove = glm::vec3(0.0f);
        }
    }

    if (DebugConfig::DEBUG_COLLISION_SYSTEM)
        printf("[COLL] before safety: pos=(%.2f,%.2f,%.2f) gnd=%d vel.z=%.3f\n",
               p.pos.x, p.pos.y, p.pos.z, (int)groundedThisFrame, p.vel.z);

    doGroundSnap(p, world, groundedThisFrame);

    if (DebugConfig::DEBUG_COLLISION_SYSTEM)
        printf("[COLL] after groundSnap: pos=(%.2f,%.2f,%.2f) gnd=%d vel.z=%.3f\n",
               p.pos.x, p.pos.y, p.pos.z, (int)groundedThisFrame, p.vel.z);

    doFloorRecovery(p, world, groundedThisFrame);
    doBodyWeaponCollisionPhase(p, world, groundedThisFrame);

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
            p.collision.stuckFrames++;
            if (p.collision.stuckFrames >= 3)
            {
                PHYS_LOG("[PHYS][EMERGENCY] Deep penetration %.4f for %d frames. Searching escape.\n",
                         worstPen, p.collision.stuckFrames);

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
                    p.collision.stuckFrames = 0;
                }
            }
        }
        else
        {
            p.collision.stuckFrames = 0;
        }
    }

    p.updateModelWorldTransforms();
    cap = p.getCapsule();
    candidates = gatherGLBTriangles(world, cap, glm::vec3(0.0f));

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
        float moveLen = glm::length(totalMove);
        float posDelta = glm::length(p.pos - lastStuckPos);
        if (moveLen > 0.01f && posDelta < 0.005f)
        {
            stuckFrames++;
            if (stuckFrames >= 3 && stuckLogTimer >= 0.5f) {
                Capsule dcap = p.getCapsule();
                std::vector<int> dcandidates = gatherGLBTriangles(world, dcap, glm::vec3(0.0f));
                printf("[COLLISION STUCK] frames=%d move=%.4f delta=%.4f pos=(%.2f %.2f %.2f) vel=(%.2f %.2f %.2f) candidates=%zu gnd=%d\n",
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

    if (DebugConfig::DEBUG_COLLISION_SYSTEM) {
        glm::vec3 beforeRot = p.pos;
        doRotationSafetyPass(p, world, groundedThisFrame, trace);
        glm::vec3 rotDelta = p.pos - beforeRot;
        if (glm::length(rotDelta) > 0.001f)
            printf("[COLL] rotationSafety moved by (%.4f,%.4f,%.4f)\n", rotDelta.x, rotDelta.y, rotDelta.z);
    } else {
        doRotationSafetyPass(p, world, groundedThisFrame, trace);
    }

    if (DebugConfig::DEBUG_COLLISION_SYSTEM) {
        glm::vec3 beforeFinal = p.pos;
        doFinalSafetyPass(p, world, trace);
        glm::vec3 finalDelta = p.pos - beforeFinal;
        if (glm::length(finalDelta) > 0.001f)
            printf("[COLL] finalSafety moved by (%.4f,%.4f,%.4f)\n", finalDelta.x, finalDelta.y, finalDelta.z);
    } else {
        doFinalSafetyPass(p, world, trace);
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

    if (DebugConfig::DEBUG_COLLISION_SYSTEM)
    {
        Capsule debugCap = p.getCapsule();
        std::vector<int> debugCandidates = gatherGLBTriangles(world, debugCap, glm::vec3(0.0f));
        std::vector<glm::vec3> debugSamples = collectPlayerBodyCollisionSamples(p);
        std::vector<RecoveryContact> debugContacts = collectGLBRecoveryContacts(
            world, debugCap, debugSamples, debugCandidates, BODY_SAMPLE_RADIUS
        );
        for (const RecoveryContact& c : debugContacts)
        {
            DebugVis::recordContact(c.point, c.normal, c.penetration, c.triangleIndex, c.label);
        }
    }

    trace.finalPos = p.pos;
    gLastCollisionTrace = trace;

    if (DebugConfig::DEBUG_COLLISION_TRACE)
    {
        Debug::logThrottled(Debug::Category::Collision, "collision-trace",
            DebugConfig::PRINT_INTERVAL,
            "[COLLISION TRACE] start=(%.2f %.2f %.2f) final=(%.2f %.2f %.2f) move=(%.3f %.3f %.3f) candidates=%d/%d sweeps=%d hits=%d toiMax=%d slide=%d recovery=%d final=%d safety=%d maxPen=%.4f features(f/e/v)=%d/%d/%d resweep=%d emergency=%d\n",
            trace.startPos.x, trace.startPos.y, trace.startPos.z,
            trace.finalPos.x, trace.finalPos.y, trace.finalPos.z,
            trace.inputMove.x, trace.inputMove.y, trace.inputMove.z,
            trace.initialCandidates, trace.maxCandidates,
            trace.sweepIterations, trace.sweepHits, trace.maxSimultaneousTOI,
            trace.maxSlideContacts, trace.maxRecoveryContacts,
            trace.finalContacts, trace.finalSafetyContacts,
            trace.maxPenetration,
            trace.faceHits, trace.edgeHits, trace.vertexHits,
            trace.resweepHits, (int)trace.emergencyEscaped);
    }
}
