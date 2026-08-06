// 07 21 2026, 17 25
/* purpose
* Runs the GLB capsule sweep/slide collision phase for local Player movement.
* Emits step movement contacts when step-up succeeds while preserving slide behavior.
* Records collision trace data for diagnostics and stress tests.
* Does NOT own movement reset formulas, networking, rendering effects, audio, or damage.
* Does NOT change projectile, weapon, death, respawn, ICE, or packet behavior.
* Does NOT replace body, safety, or block collision phases.
*/

#include "physics/movement/physics-collision-shared.h"
#include "physics/movement/physics-collision-glb-sweep.h"
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
#define LOG_COLLISION(K, ...) Debug::logThrottled(Debug::Category::Collision, K, 0.25f, __VA_ARGS__)

struct SweepSlideDiag {
    int initialCandidates = 0;
    int bodySampleCount = 0;
    int sweepIterations = 0;
    int totalHits = 0;
    int maxTOI = 0;
    int maxSlideContacts = 0;
    int stepUpAttempts = 0;
    int seamTransitions = 0;
    double broadphaseMs = 0.0;
    double bodyExtraMs = 0.0;
    double sweepMs = 0.0;
    double slideMs = 0.0;
    double stepUpMs = 0.0;
    double seamMs = 0.0;
};

void doGLBSweepSlide(
    Player& p,
    const World& world,
    bool& groundedThisFrame,
    float dt,
    const glm::vec3& totalMove,
    glm::vec3& remainingMove,
    CollisionTraceSnapshot& trace,
    std::vector<int>& candidates
)
{
    SweepSlideDiag diag;
    auto tSweepStart = std::chrono::steady_clock::now();
    constexpr float SURFACE_SLOP = 0.01f;
    constexpr float MAX_CORRECTION = 2.0f;

    p.updateModelWorldTransforms();
    Capsule cap = p.getCapsule();

    {
        auto t0 = std::chrono::steady_clock::now();
        candidates = gatherGLBTriangles(world, cap, totalMove, "Player_Capsule_SweepInitial");
        auto t1 = std::chrono::steady_clock::now();
        diag.broadphaseMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
    }
    diag.initialCandidates = (int)candidates.size();

    std::vector<glm::vec3> bodySamples = collectPlayerBodyCollisionSamples(p);
    diag.bodySampleCount = (int)bodySamples.size();

    {
        auto t0 = std::chrono::steady_clock::now();
        std::vector<glm::vec3> bodyDeltas(bodySamples.size(), glm::vec3(0.0f));
        for (size_t si = 0; si < bodySamples.size() && si < p.previousBodySamplePositions.size(); ++si)
            bodyDeltas[si] = bodySamples[si] - p.previousBodySamplePositions[si];

        // One gather from the union swept AABB of every body sample instead of
        // one gather per sample. The samples overlap heavily, so this avoids ~60
        // redundant sub-grid queries and the O(n²) per-sample dedup.
        AABB bodyUnion;
        bool bodyUnionValid = false;
        for (size_t si = 0; si < bodySamples.size(); ++si)
        {
            glm::vec3 sample = bodySamples[si];
            glm::vec3 sampleMove = totalMove + bodyDeltas[si];
            AABB sb;
            sb.min = glm::min(sample, sample + sampleMove) - glm::vec3(BODY_SAMPLE_RADIUS);
            sb.max = glm::max(sample, sample + sampleMove) + glm::vec3(BODY_SAMPLE_RADIUS);
            if (!bodyUnionValid) { bodyUnion = sb; bodyUnionValid = true; }
            else {
                bodyUnion.min = glm::min(bodyUnion.min, sb.min);
                bodyUnion.max = glm::max(bodyUnion.max, sb.max);
            }
        }

        // Clamp the union to the root capsule region. A player body can only span
        // a few units; if body samples scatter (garbage transforms), the raw union
        // would cover the whole map and make the gather scan tens of thousands of
        // cells. Bound it to keep every frame fast.
        if (bodyUnionValid)
        {
            constexpr float MAX_BODY_EXTENT = 8.0f;
            Capsule rootCapClamp = p.getCapsule();
            AABB maxBounds{
                glm::min(rootCapClamp.a, rootCapClamp.b) - glm::vec3(MAX_BODY_EXTENT),
                glm::max(rootCapClamp.a, rootCapClamp.b) + glm::vec3(MAX_BODY_EXTENT)
            };
            AABB rawUnion = bodyUnion;
            bodyUnion.min = glm::max(bodyUnion.min, maxBounds.min);
            bodyUnion.max = glm::min(bodyUnion.max, maxBounds.max);
            const bool stillValid =
                bodyUnion.min.x <= bodyUnion.max.x &&
                bodyUnion.min.y <= bodyUnion.max.y &&
                bodyUnion.min.z <= bodyUnion.max.z;
            if (stillValid)
            {
                std::vector<int> bodyUnionCandidates;
                appendChunkTrianglesForAABB(world, bodyUnion, BODY_SAMPLE_RADIUS,
                                            bodyUnionCandidates, "bodySampleUnion");
                appendUniqueTriangleIndices(candidates, bodyUnionCandidates);
            }
            if (rawUnion.min.x < maxBounds.min.x || rawUnion.max.x > maxBounds.max.x ||
                rawUnion.min.y < maxBounds.min.y || rawUnion.max.y > maxBounds.max.y ||
                rawUnion.min.z < maxBounds.min.z || rawUnion.max.z > maxBounds.max.z)
            {
                Debug::logThrottled(Debug::Category::Collision, "body-sample-spread", 1.0f,
                    "[BODY SAMPLE SPREAD] body samples exceeded %d units "
                    "(rawUnion=(%.1f %.1f %.1f)-(%.1f %.1f %.1f)); clamped\n",
                    (int)MAX_BODY_EXTENT,
                    rawUnion.min.x, rawUnion.min.y, rawUnion.min.z,
                    rawUnion.max.x, rawUnion.max.y, rawUnion.max.z);
            }
        }
        auto t1 = std::chrono::steady_clock::now();
        diag.bodyExtraMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
    }

    p.previousBodySamplePositions = bodySamples;

    trace.startPos = p.pos;
    trace.inputMove = totalMove;
    trace.initialCandidates = (int)candidates.size();
    trace.maxCandidates = trace.initialCandidates;

    remainingMove = totalMove;

    std::vector<int> preMoveCandidates = candidates;

    for (int iter = 0; iter < 5; ++iter)
    {
        auto tIterStart = std::chrono::steady_clock::now();
        SweepHit earliest;
        earliest.time = 1.0f;
        std::vector<SweepHit> toiHits;
        toiHits.reserve(4);
        constexpr float TOI_EPSILON = 0.001f;

        cap = p.getCapsule();
        {
            auto t0 = std::chrono::steady_clock::now();
            char iterTag[64];
            std::snprintf(iterTag, sizeof(iterTag), "Player_Capsule_SweepIter_%d", iter);
            candidates = gatherGLBTriangles(world, cap, remainingMove, iterTag);
            auto t1 = std::chrono::steady_clock::now();
            diag.sweepMs += std::chrono::duration<float, std::milli>(t1 - t0).count();
        }

        glm::vec3 stepMove = remainingMove * earliest.time;

        LOG_COLLISION(
            "sweep_step",
            "[SWEEP STEP] iter=%d hit=%d t=%.4f stepMove=(%.4f %.4f %.4f) remainingBefore=(%.4f %.4f %.4f) posBefore=(%.2f %.2f %.2f)",
            iter,
            (int)earliest.hit,
            earliest.time,
            stepMove.x, stepMove.y, stepMove.z,
            remainingMove.x, remainingMove.y, remainingMove.z,
            p.pos.x, p.pos.y, p.pos.z
        );

        p.pos += stepMove;
        p.updateModelWorldTransforms();
        cap = p.getCapsule();

        if (!earliest.hit)
        {
            if (DebugConfig::DEBUG_COLLISION_SYSTEM)
                Debug::log(Debug::Category::Collision, "[COLL]   no hit remaining=0 iter=%d", iter);
            LOG_COLLISION(
                "slope_hit",
                "[SLOPE HIT] tri=%d normal=(%.3f %.3f %.3f) type=%s t=%.4f point=(%.2f %.2f %.2f) feetZ=%.3f remaining=(%.4f %.4f %.4f)",
                earliest.triangleIndex,
                earliest.normal.x, earliest.normal.y, earliest.normal.z,
                earliest.normal.z >= MAX_WALKABLE_SLOPE_DOT ? "WALKABLE" :
                earliest.normal.z <= 0.0f ? "BACKFACE" :
                std::fabs(earliest.normal.z) < 0.2f ? "WALL" : "SLOPE",
                earliest.time,
                earliest.point.x, earliest.point.y, earliest.point.z,
                cap.a.z - cap.r,
                remainingMove.x, remainingMove.y, remainingMove.z
            );
            remainingMove = glm::vec3(0.0f);
            break;
        }

        if (DebugConfig::DEBUG_COLLISION_SYSTEM)
            Debug::log(Debug::Category::Collision, "[COLL]   HIT tri=%d normal=(%.3f,%.3f,%.3f) point=(%.2f,%.2f,%.2f) t=%.3f toi=%zu feature=%s",
                       earliest.triangleIndex, earliest.normal.x, earliest.normal.y, earliest.normal.z,
                       earliest.point.x, earliest.point.y, earliest.point.z, earliest.time, toiHits.size(),
                       earliest.normal.z >= MAX_WALKABLE_SLOPE_DOT ? "WALKABLE" :
                       earliest.normal.z <= 0.0f ? "BACKFACE" :
                       std::fabs(earliest.normal.z) < 0.2f ? "WALL" : "SLOPE");

        trace.sweepHits++;
        trace.maxSimultaneousTOI = std::max(trace.maxSimultaneousTOI, (int)toiHits.size());
        for (const SweepHit& hit : toiHits)
        {
            if (hit.triangleIndex < 0 || hit.triangleIndex >= (int)world.collisionMesh.triangles.size())
                continue;

            const char* feature = triangleFeatureLabel(world.collisionMesh.triangles[hit.triangleIndex], hit.point);
            if (feature[0] == 'f')
                trace.faceHits++;
            else if (feature[0] == 'e')
                trace.edgeHits++;
            else if (feature[0] == 'v')
                trace.vertexHits++;
        }

        {
            float feetZ = cap.a.z - cap.r;
            bool stepWall = std::fabs(earliest.normal.z) < 0.2f;
            if (DebugConfig::DEBUG_COLLISION_SYSTEM)
                Debug::log(Debug::Category::Collision, "[COLL]   stepUpCheck: stepWall=%d feetZ=%.3f hitPointZ=%.3f stepH=%.3f maxStep=%.3f",
                           (int)stepWall, feetZ, earliest.point.z, earliest.point.z - feetZ, MAX_STEP_HEIGHT);
        }

        if (std::fabs(earliest.normal.z) < 0.2f)
        {
            float feetZ = cap.a.z - cap.r;

            float stepTopZ = earliest.point.z;
            float stepHeight = stepTopZ - feetZ;

            if (stepHeight > 0.0f &&
                stepHeight <= MAX_STEP_HEIGHT)
            {
                glm::vec3 originalPos = p.pos;

                int consistentSamples = 0;
                int totalSamples = 0;
                for (int s = 0; s < 5; s++) {
                    float t = (float)s / 4.0f;
                    glm::vec3 samplePos = cap.a + (cap.b - cap.a) * t;
                    float sampleFeetZ = samplePos.z - cap.r;
                    for (int triIndex : candidates) {
                        const CollisionTriangle& tri =
                            world.collisionMesh.triangles[triIndex];
                        float triZ = std::max({tri.a.z, tri.b.z, tri.c.z});
                        if (sampleFeetZ < triZ && triZ - sampleFeetZ <= MAX_STEP_HEIGHT) {
                            consistentSamples++;
                            break;
                        }
                    }
                    totalSamples++;
                }
                bool consistentStep = (consistentSamples >= 3);

                if (DebugConfig::DEBUG_COLLISION_SYSTEM)
                    Debug::log(Debug::Category::Collision, "[COLL]   stepUp: consistent=%d/%d stepH=%.3f feetZ=%.3f",
                               consistentSamples, totalSamples, stepHeight, feetZ);

                if (!consistentStep) {
                    if (DebugConfig::DEBUG_COLLISION_SYSTEM)
                        Debug::log(Debug::Category::Collision, "[COLL]   stepUp FAILED consistency restore pos=(%.2f,%.2f,%.2f)",
                                   originalPos.x, originalPos.y, originalPos.z);
                    p.pos = originalPos;
                    p.updateModelWorldTransforms();
                    continue;
                }

                p.pos.z += stepHeight + 0.01f;

                p.updateModelWorldTransforms();

                Capsule stepCap = p.getCapsule();

                bool blocked = false;

                for (int triIndex : candidates)
                {
                    const CollisionTriangle& tri =
                        world.collisionMesh.triangles[triIndex];

                    Contact c;

                    if (capsuleTriangleContact(
                        stepCap,
                        tri,
                        triIndex,
                        c))
                    {
                        if (c.normal.z < 0.5f)
                        // 7 1 2026 colisions borken fixes testing again 
                        // if (c.normal.z < 1.1f)
                        {
                            blocked = true;
                            break;
                        }
                    }
                }

                if (DebugConfig::DEBUG_COLLISION_SYSTEM)
                    Debug::log(Debug::Category::Collision, "[COLL]   stepUp: blocked=%d", (int)blocked);

                if (!blocked)
                {
                    Capsule checkCap = stepCap;
                    checkCap.a.z -= 0.3f;
                    checkCap.b.z -= 0.3f;
                    bool hasFloor = false;
                    for (int triIndex : candidates) {
                        const CollisionTriangle& tri =
                            world.collisionMesh.triangles[triIndex];
                        Contact fc;
                        if (capsuleTriangleContact(checkCap, tri, triIndex, fc)) {
                            if (fc.normal.z >= MAX_WALKABLE_SLOPE_DOT) {
                                hasFloor = true;
                                break;
                            }
                        }
                    }

                    if (DebugConfig::DEBUG_COLLISION_SYSTEM)
                        Debug::log(Debug::Category::Collision, "[COLL]   stepUp: hasFloor=%d\n", (int)hasFloor);

                    if (!hasFloor) {
                        p.pos = originalPos;
                        p.updateModelWorldTransforms();
                        continue;
                    }

                    groundedThisFrame = true;
                    appendPlayerMovementContact(
                        p,
                        MovementContactKind::Step,
                        earliest.normal,
                        earliest.point,
                        SURFACE_SLOP,
                        earliest.triangleIndex);

                    if (p.vel.z < 0.0f)
                        p.vel.z = 0.0f;

                    remainingMove -= stepMove;

                    LOG_COLLISION("step_success", "[STEP] succeeded stepH=%.3f remaining=(%.4f,%.4f,%.4f)",
                                  stepHeight, remainingMove.x, remainingMove.y, remainingMove.z);
                    continue;
                }

                LOG_COLLISION("step_blocked", "[STEP] blocked stepH=%.3f reason=no_landing_or_blocked",
                              stepHeight);
                p.pos = originalPos;
                p.updateModelWorldTransforms();
            }
        }
        // SEAM TRANSITION: when the sweep hits a non-walkable surface (edge, backface,
        // or wall at a mesh seam), check for a walkable surface ahead and project
        // movement against THAT surface instead of the blocking edge.
        {
            float feetZ = cap.a.z - cap.r;
            float horizMove = glm::length(glm::vec2(remainingMove.x, remainingMove.y));
            if (earliest.normal.z < MAX_WALKABLE_SLOPE_DOT && horizMove > 0.01f &&
                earliest.point.z > feetZ - MAX_STEP_HEIGHT)
            {
                glm::vec3 moveDir = glm::normalize(glm::vec3(remainingMove.x, remainingMove.y, 0.0f));
                if (glm::length(moveDir) < 0.001f) moveDir = glm::vec3(0.0f, 1.0f, 0.0f);
                int bestTri = -1;
                float bestScore = -1.0f;
                int walkableCandidates = 0, totalChecked = 0;

                // Scan current iteration's sweep candidates first (fresh, with remainingMove)
                for (int idx : candidates) {
                    const auto& tri = world.collisionMesh.triangles[idx];
                    ++totalChecked;
                    if (tri.normal.z < MAX_WALKABLE_SLOPE_DOT) continue;
                    ++walkableCandidates;
                    float score = -glm::dot(moveDir, tri.normal);
                    if (score > bestScore) { bestScore = score; bestTri = idx; }
                }

                // Fallback: scan pre-move candidates if current set had no walkable
                if (bestTri < 0) {
                    for (int idx : preMoveCandidates) {
                        const auto& tri = world.collisionMesh.triangles[idx];
                        ++totalChecked;
                        if (tri.normal.z < MAX_WALKABLE_SLOPE_DOT) continue;
                        ++walkableCandidates;
                        float score = -glm::dot(moveDir, tri.normal);
                        if (score > bestScore) { bestScore = score; bestTri = idx; }
                    }
                }

                if (bestTri >= 0 && bestScore > 0.0f) {
                    const auto& walkTri = world.collisionMesh.triangles[bestTri];
                    remainingMove -= stepMove;
                    float vn = glm::dot(remainingMove, walkTri.normal);
                    if (vn < 0.0f)
                        remainingMove -= walkTri.normal * vn;
                    p.pos.z += 0.01f;
                    p.updateModelWorldTransforms();
                    LOG_COLLISION("seam", "[SEAM] edgeTri=%d edgeN=(%.3f,%.3f,%.3f) walkTri=%d walkN=(%.3f,%.3f,%.3f)"
                                  " score=%.3f remaining=(%.4f,%.4f,%.4f) walkableCand=%d totalCand=%d",
                                  earliest.triangleIndex,
                                  earliest.normal.x, earliest.normal.y, earliest.normal.z,
                                  bestTri, walkTri.normal.x, walkTri.normal.y, walkTri.normal.z,
                                  bestScore, remainingMove.x, remainingMove.y, remainingMove.z,
                                  walkableCandidates, totalChecked);
                    if (glm::dot(remainingMove, remainingMove) < 0.000001f) break;
                    continue;
                } else {
                    LOG_COLLISION("seam_fail", "[SEAM_FAIL] edgeTri=%d edgeN=(%.3f,%.3f,%.3f)"
                                  " walkableCand=%d totalCand=%d bestScore=%.3f horizMove=%.3f"
                                  " feetZ=%.3f hitPointZ=%.3f",
                                  earliest.triangleIndex,
                                  earliest.normal.x, earliest.normal.y, earliest.normal.z,
                                  walkableCandidates, totalChecked, bestScore,
                                  horizMove, feetZ, earliest.point.z);
                }
            }
        }

        glm::vec3 depen = earliest.normal;

        // Only zero depenetration Z for near-horizontal normals (walls).
        // Moderate slopes (z >= 0.2) keep upward push — without this the
        // player gets pushed horizontally into the slope instead of riding up it.
        if (depen.z > 0.0f && depen.z < 0.2f)
            depen.z = 0.0f;

        if (DebugConfig::DEBUG_COLLISION_SYSTEM)
            Debug::log(Debug::Category::Collision, "[COLL]   normalSlide: preSlideDepen=(%.3f,%.3f,%.3f) stepMove=(%.4f,%.4f,%.4f)\n",
                   depen.x, depen.y, depen.z, stepMove.x, stepMove.y, stepMove.z);

        if (glm::length(depen) > 0.0001f)
            depen = glm::normalize(depen);

        p.pos += depen * SURFACE_SLOP;
        for (const SweepHit& hit : toiHits)
        {
            if (hit.triangleIndex < 0 || hit.triangleIndex >= (int)world.collisionMesh.triangles.size())
                continue;
            DebugVis::recordHit(hit.point, hit.normal, hit.triangleIndex, hit.colliderName.c_str());
            DebugVis::recordTriangle(world.collisionMesh.triangles[hit.triangleIndex], hit.triangleIndex, "sweep-hit-triangle");
        }
        {
            glm::vec3 beforeSlide = remainingMove;
            remainingMove -= stepMove;
            for (const SweepHit& hit : toiHits) {
                float vn = glm::dot(remainingMove, hit.normal);
                if (vn < 0.0f)
                    remainingMove -= hit.normal * vn;
            }
            LOG_COLLISION("slide_proj", "[SLIDE] before=(%.4f,%.4f,%.4f) after=(%.4f,%.4f,%.4f) stepMove=(%.4f,%.4f,%.4f) hitNormal=(%.3f,%.3f,%.3f)",
                          beforeSlide.x, beforeSlide.y, beforeSlide.z,
                          remainingMove.x, remainingMove.y, remainingMove.z,
                          stepMove.x, stepMove.y, stepMove.z,
                          earliest.normal.x, earliest.normal.y, earliest.normal.z);
        }

        {
            Capsule slideCap = p.getCapsule();
            std::vector<int> slideCandidates = gatherGLBTriangles(world, slideCap, glm::vec3(0.0f), "Player_Capsule_SlideContacts");
            std::vector<RecoveryContact> slideContacts = collectCapsuleRecoveryContacts(
                world, slideCap, slideCandidates);
            trace.maxSlideContacts = std::max(trace.maxSlideContacts, (int)slideContacts.size());

            glm::vec3 before8Pass = remainingMove;
            constexpr int SLIDE_SOLVER_PASSES = 8;
            for (int slidePass = 0; slidePass < SLIDE_SOLVER_PASSES; ++slidePass)
            {
                for (const RecoveryContact& sc : slideContacts)
                {
                    float vn = glm::dot(remainingMove, sc.normal);
                    if (vn < 0.0f)
                        remainingMove -= sc.normal * vn;
                }
            }
            glm::vec3 after8Pass = remainingMove;
            glm::vec3 slideDelta = after8Pass - before8Pass;
            if (glm::length(slideDelta) > 0.001f) {
                LOG_COLLISION("slide8", "[SLIDE] 8-pass: before=(%.4f,%.4f,%.4f) after=(%.4f,%.4f,%.4f) contacts=%zu",
                              before8Pass.x, before8Pass.y, before8Pass.z,
                              after8Pass.x, after8Pass.y, after8Pass.z, slideContacts.size());
            }
        }

        for (const SweepHit& hit : toiHits)
        {
            applyCollisionContact(
                p,
                groundedThisFrame,
                hit.normal,
                hit.point,
                SURFACE_SLOP,
                hit.triangleIndex,
                hit.colliderName.c_str()
            );
        }

        PHYS_LOG(
            "[PHYS][GLB HIT] tri=%d t=%.3f toi=%zu normal=(%.2f %.2f %.2f) point=(%.2f %.2f %.2f)\n",
            earliest.triangleIndex,
            earliest.time,
            toiHits.size(),
            earliest.normal.x,
            earliest.normal.y,
            earliest.normal.z,
            earliest.point.x,
            earliest.point.y,
            earliest.point.z
        );

        LOG_COLLISION("iter_end", "[SWEEP] iter=%d end: pos=(%.2f,%.2f,%.2f) remaining=(%.4f,%.4f,%.4f) gnd=%d",
                      iter, p.pos.x, p.pos.y, p.pos.z,
                      remainingMove.x, remainingMove.y, remainingMove.z,
                      (int)groundedThisFrame);

        if (glm::dot(remainingMove, remainingMove) < 0.000001f)
            break;
    }

    auto tSweepEnd = std::chrono::steady_clock::now();
    float sweepTotalMs = std::chrono::duration<float, std::milli>(tSweepEnd - tSweepStart).count();
    Debug::logThrottled(Debug::Category::Collision, "sweep_diag_summary", 1.0f,
        "[SWEEP DIAG] totalMs=%.2f initialCand=%d bodySamples=%d iters=%d hits=%d maxTOI=%d "
        "stepUp=%d seam=%d broadphaseMs=%.2f bodyExtraMs=%.2f sweepMs=%.2f\n",
        sweepTotalMs, diag.initialCandidates, diag.bodySampleCount,
        trace.sweepIterations, trace.sweepHits, trace.maxSimultaneousTOI,
        diag.stepUpAttempts, diag.seamTransitions,
        diag.broadphaseMs, diag.bodyExtraMs, diag.sweepMs);
}
