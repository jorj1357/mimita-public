#include "physics/movement/physics-collision-shared.h"
#include "physics/movement/physics-collision-glb-sweep.h"
#include "physics/movement/physics-collision-glb-body.h"
#include "physics/movement/physics-collision-glb-safety.h"
#include "physics/movement/physics-collision-glb-sweep-slide.h"

#include <chrono>
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
    const glm::vec3& move,
    const char* caller
);

#define PHYS_LOG(...) Debug::logThrottled(Debug::Category::Collision, "physics-collision", DebugConfig::PRINT_INTERVAL, __VA_ARGS__)

struct CollisionFrameDiag {
    double sweepSlideMs = 0.0;
    double depenGatherMs = 0.0;
    double depenSolveMs = 0.0;
    double floorRecoveryMs = 0.0;
    double bodyWeaponMs = 0.0;
    double emergencyCheckMs = 0.0;
    double emergencySearchMs = 0.0;
    double stuckTrackMs = 0.0;
    double debugVisMs = 0.0;
    double totalMs = 0.0;
    int depenIterations = 0;
    int depenContacts = 0;
    int emergencyDirsSearched = 0;
    bool emergencyTriggered = false;
};

// Spike detection: warns if a stage takes too long.
#define CHECK_COLL_SPIKE(NAME, MS) \
    do { \
        if ((MS) > 20.0f) \
            Debug::warn(Debug::Category::Collision, \
                "\033[31m[SPIKE]\033[0m %s %.1fms (threshold=20ms)\n", (NAME), (MS)); \
        else if ((MS) > 10.0f) \
            Debug::warn(Debug::Category::Collision, \
                "\033[38;5;214m[SPIKE]\033[0m %s %.1fms (threshold=10ms)\n", (NAME), (MS)); \
        else if ((MS) > 5.0f) \
            Debug::warn(Debug::Category::Collision, \
                "\033[33m[SPIKE]\033[0m %s %.1fms (threshold=5ms)\n", (NAME), (MS)); \
    } while(0)

static void logFrameSummary(const CollisionFrameDiag& d)
{
    CHECK_COLL_SPIKE("SweepSlide", d.sweepSlideMs);
    CHECK_COLL_SPIKE("Depen", d.depenGatherMs + d.depenSolveMs);
    CHECK_COLL_SPIKE("FloorRecovery", d.floorRecoveryMs);
    CHECK_COLL_SPIKE("BodyWeapon", d.bodyWeaponMs);
    CHECK_COLL_SPIKE("Emergency", d.emergencyCheckMs + d.emergencySearchMs);
    CHECK_COLL_SPIKE("StuckTracking", d.stuckTrackMs);
    CHECK_COLL_SPIKE("DebugVis", d.debugVisMs);

    Debug::logThrottled(Debug::Category::Collision, "frame-summary", 1.0f,
        "[COLLISION FRAME]\n"
        "  SweepSlide ......... %.2f ms\n"
        "  Depen .............. %.2f ms (gather=%.2f solve=%.2f iters=%d contacts=%d)\n"
        "  FloorRecovery ...... %.2f ms\n"
        "  BodyWeapon ......... %.2f ms\n"
        "  Emergency .......... %.2f ms (search=%.2f dirs=%d triggered=%d)\n"
        "  StuckTracking ...... %.2f ms\n"
        "  DebugVis ........... %.2f ms\n"
        "  Total .............. %.2f ms\n",
        d.sweepSlideMs,
        d.depenGatherMs + d.depenSolveMs, d.depenGatherMs, d.depenSolveMs,
        d.depenIterations, d.depenContacts,
        d.floorRecoveryMs,
        d.bodyWeaponMs,
        d.emergencyCheckMs + d.emergencySearchMs, d.emergencySearchMs,
        d.emergencyDirsSearched, (int)d.emergencyTriggered,
        d.stuckTrackMs,
        d.debugVisMs,
        d.totalMs);
}

static void trackGrowth(Player& p, int candidates, int contacts, int bodySpheres)
{
    auto& d = p.collision;
    if (candidates > d.diagPrevCandidates) d.diagCandidateGrowthFrames++;
    else if (candidates < d.diagPrevCandidates) d.diagCandidateGrowthFrames = 0;
    d.diagPrevCandidates = candidates;

    if (contacts > d.diagPrevContacts) d.diagContactGrowthFrames++;
    else if (contacts < d.diagPrevContacts) d.diagContactGrowthFrames = 0;
    d.diagPrevContacts = contacts;

    if (bodySpheres > d.diagPrevBodySpheres) d.diagBodySphereGrowthFrames++;
    else if (bodySpheres < d.diagPrevBodySpheres) d.diagBodySphereGrowthFrames = 0;
    d.diagPrevBodySpheres = bodySpheres;

    if (d.diagCandidateGrowthFrames >= 10)
        Debug::warn(Debug::Category::Collision,
            "[GROWTH WARNING] candidates increasing for %d frames (current=%d prev=%d)\n",
            d.diagCandidateGrowthFrames, candidates, d.diagPrevCandidates);
    if (d.diagContactGrowthFrames >= 10)
        Debug::warn(Debug::Category::Collision,
            "[GROWTH WARNING] contacts increasing for %d frames (current=%d prev=%d)\n",
            d.diagContactGrowthFrames, contacts, d.diagPrevContacts);
    if (d.diagBodySphereGrowthFrames >= 10)
        Debug::warn(Debug::Category::Collision,
            "[GROWTH WARNING] bodySpheres increasing for %d frames (current=%d prev=%d)\n",
            d.diagBodySphereGrowthFrames, bodySpheres, d.diagPrevBodySpheres);
}

void doGLBTriangleCollisions(
    Player& p,
    const World& world,
    bool& groundedThisFrame,
    float dt
) {
    CollisionFrameDiag diag;
    auto tFrameStart = std::chrono::steady_clock::now();
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

    // ── 1. Body + weapon collision (before root capsule sweep) ──
    // Run first so weapon contacts push player position before the
    // root capsule sweep resolves. This prevents the weapon from
    // entering geometry — the player is pushed away at the weapon's
    // contact point before the capsule can move further into the wall.
    if (!isCurrentEntityNpc())
    {
        Perf::ScopedTimer _bw("WeaponCollisions");
        auto t0 = std::chrono::steady_clock::now();
        doBodyWeaponCollisionPhase(p, world, groundedThisFrame);
        auto t1 = std::chrono::steady_clock::now();
        diag.bodyWeaponMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
        // Recompute totalMove after body/weapon may have adjusted position
        totalMove = (p.vel + p.externalImpulse) * dt;
    }

    // ── 2. Sweep + slide ─────────────────────────────────
    {
        Perf::ScopedTimer _st("SweepSlide");
        auto t0 = std::chrono::steady_clock::now();
        doGLBSweepSlide(p, world, groundedThisFrame, dt, totalMove, remainingMove, trace, candidates);
        auto t1 = std::chrono::steady_clock::now();
        diag.sweepSlideMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
    }

    // ── 3. Batched depenetration ─────────────────────────
    {
        Perf::ScopedTimer _dt("Depenetration");
        auto t0 = std::chrono::steady_clock::now();
        p.updateModelWorldTransforms();
        Capsule cap = p.getCapsule();
        {
            auto tg0 = std::chrono::steady_clock::now();
            candidates = gatherGLBTriangles(world, cap, glm::vec3(0.0f), "Player_Capsule_Depen");
            auto tg1 = std::chrono::steady_clock::now();
            diag.depenGatherMs = std::chrono::duration<float, std::milli>(tg1 - tg0).count();
        }

        for (int depenIter = 0; depenIter < 4; ++depenIter)
        {
            std::vector<RecoveryContact> contacts = collectCapsuleRecoveryContacts(
                world, cap, candidates
            );
            trace.maxRecoveryContacts = std::max(trace.maxRecoveryContacts, (int)contacts.size());

            if (contacts.empty())
                break;
            diag.depenIterations++;
            diag.depenContacts = (int)contacts.size();

            float iterMaxPen = 0.0f;
            glm::vec3 correction = solveBatchedCorrection(contacts, SURFACE_SLOP, &iterMaxPen, nullptr, totalMove, p.pos);

            float corrLen = glm::length(correction);
            if (corrLen > MAX_CORRECTION)
                correction *= MAX_CORRECTION / corrLen;

            p.pos += correction;
            cap = p.getCapsule();

            for (const RecoveryContact& c : contacts)
                applyCollisionContact(p, groundedThisFrame, c.normal, c.point, c.penetration, c.triangleIndex, c.label);

            DebugVis::recordDepenetration(p.pos - correction, correction, "glb-batched-depen");

            if (glm::dot(correction, correction) < 0.0000001f)
                break;
        }
        auto t1 = std::chrono::steady_clock::now();
        diag.depenSolveMs = std::chrono::duration<float, std::milli>(t1 - t0).count() - diag.depenGatherMs;
    }

    // ── 4. Floor recovery ────────────────────────────────
    {
        Perf::ScopedTimer _fr("GroundDetection");
        auto t0 = std::chrono::steady_clock::now();
        doFloorRecovery(p, world, groundedThisFrame);
        auto t1 = std::chrono::steady_clock::now();
        diag.floorRecoveryMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
    }

    // ── 5. Emergency stuck escape ────────────────────────
    {
        Perf::ScopedTimer _es("CharVsWorld");
        auto t0 = std::chrono::steady_clock::now();
        constexpr float STUCK_THRESHOLD = 0.05f;
        const float EMERGENCY_SEARCH_RADIUS = PLAYER_HEIGHT + PLAYER_RADIUS * 4.0f;

        p.updateModelWorldTransforms();
        Capsule stuckCheckCap = p.getCapsule();
        std::vector<int> stuckCandidates = gatherGLBTriangles(world, stuckCheckCap, glm::vec3(0.0f), "Player_Capsule_EmergencyCheck");
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
                diag.emergencyTriggered = true;

                glm::vec3 bestPos = p.pos;
                float bestPen = worstPen;
                bool foundFree = false;
                int dirsSearched = 0;
                // Bound the worst case: 13 dirs × ~128 steps each would be ~1664
                // full collision gathers per frame for a stuck-embedded entity.
                // Directions are ordered UP/DOWN first so a player embedded in a
                // floor escapes within the first few dozen tests, well under the cap.
                constexpr int MAX_EMERGENCY_TESTS = 512;
                const glm::vec3 searchDirs[] = {
                    { 0, 0, 1}, { 0, 0,-1}, { 1, 0, 0}, {-1, 0, 0},
                    { 0, 1, 0}, { 0,-1, 0}, { 1, 1, 0}, { 1,-1, 0}, {-1, 1, 0}, {-1,-1, 0},
                    { 1, 0, 1}, {-1, 0, 1}, { 0, 1, 1}, { 0,-1, 1}
                };

                for (glm::vec3 dir : searchDirs)
                {
                    if (glm::length(dir) > 0.001f)
                        dir = glm::normalize(dir);

                    for (float dist = 0.05f; dist <= EMERGENCY_SEARCH_RADIUS; dist += 0.05f)
                    {
                        if (dirsSearched >= MAX_EMERGENCY_TESTS) break;
                        ++dirsSearched;
                        glm::vec3 testPos = p.pos + dir * dist;
                        Player testP = p;
                        testP.pos = testPos;
                        Capsule testCap = testP.getCapsule();
                        char searchTag[64];
                        std::snprintf(searchTag, sizeof(searchTag), "Player_Capsule_EmergencySearch_%d", dirsSearched);
                        std::vector<int> testCandidates = gatherGLBTriangles(world, testCap, glm::vec3(0.0f), searchTag);
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
                    if (dirsSearched >= MAX_EMERGENCY_TESTS || foundFree) break;
                }
                diag.emergencyDirsSearched = dirsSearched;

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
        auto t1 = std::chrono::steady_clock::now();
        diag.emergencyCheckMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
    }

    // ── 6. Stuck position tracking ───────────────────────
    {
        auto t0 = std::chrono::steady_clock::now();
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
                std::vector<int> dc = gatherGLBTriangles(world, dcap, glm::vec3(0.0f), "Player_Capsule_StuckTrack");
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
        auto t1 = std::chrono::steady_clock::now();
        diag.stuckTrackMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
    }

    // ── 7. Debug visualization ───────────────────────────
    {
        auto t0 = std::chrono::steady_clock::now();
        if (DebugConfig::DEBUG_COLLISION_SYSTEM)
        {
            Capsule debugCap = p.getCapsule();
            std::vector<int> debugCandidates = gatherGLBTriangles(world, debugCap, glm::vec3(0.0f), "Player_Capsule_DebugVis");
            std::vector<glm::vec3> debugSamples = collectPlayerBodyCollisionSamples(p);
            std::vector<RecoveryContact> debugContacts = collectGLBRecoveryContacts(
                world, debugCap, debugSamples, debugCandidates, BODY_SAMPLE_RADIUS
            );
            for (const RecoveryContact& c : debugContacts)
                DebugVis::recordContact(c.point, c.normal, c.penetration, c.triangleIndex, c.label);
        }
        auto t1 = std::chrono::steady_clock::now();
        diag.debugVisMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
    }

    trace.finalPos = p.pos;
    gLastCollisionTrace = trace;

    auto tFrameEnd = std::chrono::steady_clock::now();
    diag.totalMs = std::chrono::duration<float, std::milli>(tFrameEnd - tFrameStart).count();

    // ── Per-frame summary ────────────────────────────────
    logFrameSummary(diag);

    // ── Growth tracking ──────────────────────────────────
    int bodySphereCount = (int)collectPlayerBodyCollisionSamples(p).size();
    trackGrowth(p, trace.maxCandidates, trace.maxRecoveryContacts, bodySphereCount);

    // ── Trend detection: detect if any stage time grows for 20+ consecutive frames ──
    {
        static double prevSweepMs = 0, prevBodyMs = 0, prevDepenMs = 0;
        static int sweepGrowthFrames = 0, bodyGrowthFrames = 0, depenGrowthFrames = 0;

        auto checkTrend = [&](const char* name, double current, double& prev, int& frames) {
            if (current > prev + 0.1) frames++;
            else if (current < prev - 0.1) frames = 0;
            if (frames >= 20)
                Debug::log(Debug::Category::Collision,
                    "[TREND WARNING] %s increasing for %d frames (now=%.2fms, prev=%.2fms)\n",
                    name, frames, current, prev);
            prev = current;
        };

        checkTrend("SweepSlide", diag.sweepSlideMs, prevSweepMs, sweepGrowthFrames);
        checkTrend("BodyWeapon", diag.bodyWeaponMs, prevBodyMs, bodyGrowthFrames);
        checkTrend("Depen", diag.depenGatherMs + diag.depenSolveMs, prevDepenMs, depenGrowthFrames);
    }

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
