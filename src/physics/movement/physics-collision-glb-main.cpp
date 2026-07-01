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
    const float maxZStep = PLAYER_RADIUS;
    if (totalMove.z < -maxZStep)
        totalMove.z = -maxZStep;
    glm::vec3 remainingMove;
    CollisionTraceSnapshot trace;
    std::vector<int> candidates;
    candidates.reserve(512);

    // ── 1. Sweep + slide ─────────────────────────────────
    doGLBSweepSlide(p, world, groundedThisFrame, dt, totalMove, remainingMove, trace, candidates);

    // ── 2. Apply leftover remaining move ──────────────────
    // If the sweep-slide didn't consume all movement (5-iteration limit, t=0 contacts),
    // apply the rest directly so the player isn't robbed of momentum.
    
    // 7 1 2026 testing colision fixes idk waht happened bro 
    
    // if (glm::length(remainingMove) > 0.001f) {
    //     if (DebugConfig::DEBUG_COLLISION_SYSTEM)
    //         Debug::log(Debug::Category::Collision, "[COLL] applying remainingMove=(%.4f,%.4f,%.4f)\n",
    //                remainingMove.x, remainingMove.y, remainingMove.z);
    //     p.pos += remainingMove;
    //     p.updateModelWorldTransforms();
    //     remainingMove = glm::vec3(0.0f);
    // }

    // ── 3. Batched depenetration ─────────────────────────
    // Cache the candidate gather: the capsule barely moves during depen iterations,
    // so we gather once and reuse. This eliminates 3 of 4 gatherGLBTriangles calls.
    {
        p.updateModelWorldTransforms();
        Capsule cap = p.getCapsule();
        candidates = gatherGLBTriangles(world, cap, glm::vec3(0.0f));

        for (int depenIter = 0; depenIter < 4; ++depenIter)
        {
            std::vector<RecoveryContact> contacts = collectCapsuleRecoveryContacts(
                world, cap, candidates
            );
            trace.maxRecoveryContacts = std::max(trace.maxRecoveryContacts, (int)contacts.size());

            if (contacts.empty())
                break;

            float iterMaxPen = 0.0f;
            glm::vec3 correction = solveBatchedCorrection(contacts, SURFACE_SLOP, &iterMaxPen, nullptr, totalMove, p.pos);

            float corrLen = glm::length(correction);
            if (corrLen > MAX_CORRECTION)
                correction *= MAX_CORRECTION / corrLen;

            p.pos += correction;
            PHYS_LOG(
                "[DEPEN TEST] iter=%d contacts=%zu maxPen=%.4f correction=(%.4f %.4f %.4f) move=(%.4f %.4f %.4f) pos=(%.2f %.2f %.2f)\n",
                depenIter,
                contacts.size(),
                iterMaxPen,
                correction.x, correction.y, correction.z,
                totalMove.x, totalMove.y, totalMove.z,
                p.pos.x, p.pos.y, p.pos.z
            );

            for (const RecoveryContact& c : contacts) {
                PHYS_LOG(
                    "[CONTACT TEST] tri=%d normal=(%.3f %.3f %.3f) pen=%.4f point=(%.2f %.2f %.2f) label=%s\n",
                    c.triangleIndex,
                    c.normal.x, c.normal.y, c.normal.z,
                    c.penetration,
                    c.point.x, c.point.y, c.point.z,
                    c.label ? c.label : "?"
                );
            }
            cap = p.getCapsule();  // update capsule for next iter's contact collection

            if (DebugConfig::DEBUG_COLLISION_SYSTEM)
                Debug::log(Debug::Category::Collision, "[COLL]   depen iter=%d contacts=%zu maxPen=%.4f correction=(%.4f,%.4f,%.4f)\n",
                       depenIter, contacts.size(), iterMaxPen,
                       correction.x, correction.y, correction.z);

            for (const RecoveryContact& c : contacts)
                applyCollisionContact(p, groundedThisFrame, c.normal, c.point, c.penetration, c.triangleIndex, c.label);

            DebugVis::recordDepenetration(p.pos - correction, correction, "glb-batched-depen");

            if (glm::dot(correction, correction) < 0.0000001f)
                break;
        }
    }

    // ── 4. Ground snap + floor recovery ──────────────────
    // testing if this even does atnthing 7 1 2026 
    // doGroundSnap(p, world, groundedThisFrame);
    doFloorRecovery(p, world, groundedThisFrame);

    // ── 5. Body + weapon collision ───────────────────────
    doBodyWeaponCollisionPhase(p, world, groundedThisFrame);

    // ── 6. Emergency stuck escape ────────────────────────
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
                PHYS_LOG("[PHYS][EMERGENCY] Deep penetration %.4f for %d frames.\n",
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

    // ── 7. Stuck position tracking (logging only) ────────
    {
        static int sf = 0;
        static glm::vec3 lastPos(0.0f);
        static float logTimer = 0.0f;
        logTimer += dt;
        float moveLen = glm::length(totalMove);
        float posDelta = glm::length(p.pos - lastPos);
        if (moveLen > 0.01f && posDelta < 0.005f)
        {
            sf++;
            if (sf >= 3 && logTimer >= 0.5f) {
                Capsule dcap = p.getCapsule();
                std::vector<int> dc = gatherGLBTriangles(world, dcap, glm::vec3(0.0f));
                PHYS_LOG(
                        "[COLLISION STUCK] frames=%d move=%.4f delta=%.4f pos=(%.2f %.2f %.2f) vel=(%.2f %.2f %.2f) candidates=%zu gnd=%d\n",
                        sf, moveLen, posDelta,
                        p.pos.x, p.pos.y, p.pos.z,
                        p.vel.x, p.vel.y, p.vel.z,
                        dc.size(), (int)groundedThisFrame
                    );
                for (int ci = 0; ci < (int)dc.size() && ci < 5; ++ci) {
                    const CollisionTriangle& tri = world.collisionMesh.triangles[dc[ci]];
                    PHYS_LOG(
                        "  tri=%d normal=(%.3f %.3f %.3f)%s%s\n",
                        dc[ci],
                        tri.normal.x, tri.normal.y, tri.normal.z,
                        tri.normal.z >= MAX_WALKABLE_SLOPE_DOT ? " [WALKABLE]" : "",
                        tri.normal.z <= 0.0f ? " [BACKFACE]" : ""
                    );
                }
                logTimer = 0.0f;
            }
        } else {
            sf = 0;
        }
        lastPos = p.pos;
    }

    // ── Debug visualization ──────────────────────────────
    if (DebugConfig::DEBUG_COLLISION_SYSTEM)
    {
        Capsule debugCap = p.getCapsule();
        std::vector<int> debugCandidates = gatherGLBTriangles(world, debugCap, glm::vec3(0.0f));
        std::vector<glm::vec3> debugSamples = collectPlayerBodyCollisionSamples(p);
        std::vector<RecoveryContact> debugContacts = collectGLBRecoveryContacts(
            world, debugCap, debugSamples, debugCandidates, BODY_SAMPLE_RADIUS
        );
        for (const RecoveryContact& c : debugContacts)
            DebugVis::recordContact(c.point, c.normal, c.penetration, c.triangleIndex, c.label);
    }

    trace.finalPos = p.pos;
    gLastCollisionTrace = trace;

    if (DebugConfig::DEBUG_COLLISION_TRACE)
        Debug::logThrottled(Debug::Category::Collision, "collision-trace",
            DebugConfig::PRINT_INTERVAL,
            "[COLLISION TRACE] start=(%.2f %.2f %.2f) final=(%.2f %.2f %.2f) move=(%.3f %.3f %.3f) candidates=%d/%d sweeps=%d hits=%d toiMax=%d slide=%d recovery=%d maxPen=%.4f features(f/e/v)=%d/%d/%d emergency=%d\n",
            trace.startPos.x, trace.startPos.y, trace.startPos.z,
            trace.finalPos.x, trace.finalPos.y, trace.finalPos.z,
            trace.inputMove.x, trace.inputMove.y, trace.inputMove.z,
            trace.initialCandidates, trace.maxCandidates,
            trace.sweepIterations, trace.sweepHits, trace.maxSimultaneousTOI,
            trace.maxSlideContacts, trace.maxRecoveryContacts,
            trace.maxPenetration,
            trace.faceHits, trace.edgeHits, trace.vertexHits,
            (int)trace.emergencyEscaped);
}
