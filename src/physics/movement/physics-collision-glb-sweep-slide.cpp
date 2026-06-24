#include "physics/movement/physics-collision-shared.h"
#include "physics/movement/physics-collision-glb-sweep.h"
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
    constexpr float SURFACE_SLOP = 0.01f;
    constexpr float MAX_CORRECTION = 2.0f;

    p.updateModelWorldTransforms();
    Capsule cap = p.getCapsule();

    candidates = gatherGLBTriangles(world, cap, totalMove);
    std::vector<int> bodyCandidateExtras;
    std::vector<glm::vec3> bodySamples = collectPlayerBodyCollisionSamples(p);

    std::vector<glm::vec3> bodyDeltas(bodySamples.size(), glm::vec3(0.0f));
    for (size_t si = 0; si < bodySamples.size() && si < p.previousBodySamplePositions.size(); ++si)
        bodyDeltas[si] = bodySamples[si] - p.previousBodySamplePositions[si];

    for (size_t si = 0; si < bodySamples.size(); ++si)
    {
        glm::vec3 sample = bodySamples[si];
        glm::vec3 sampleMove = totalMove + bodyDeltas[si];
        std::vector<int> sampleCandidates = gatherGLBTrianglesForSphere(world, sample, BODY_SAMPLE_RADIUS, sampleMove);
        appendUniqueTriangleIndices(bodyCandidateExtras, sampleCandidates);
    }
    appendUniqueTriangleIndices(candidates, bodyCandidateExtras);

    p.previousBodySamplePositions = bodySamples;

    trace.startPos = p.pos;
    trace.inputMove = totalMove;
    trace.initialCandidates = (int)candidates.size();
    trace.maxCandidates = trace.initialCandidates;

    static int frameLog = 0;
    if ((frameLog++ % 60) == 0)
    {
        PHYS_LOG(
            "[PHYS][GLB] tris=%zu candidates=%zu bodySamples=%zu pos=(%.2f %.2f %.2f) move=(%.3f %.3f %.3f)\n",
            world.collisionMesh.triangles.size(),
            candidates.size(),
            bodySamples.size(),
            p.pos.x, p.pos.y, p.pos.z,
            totalMove.x, totalMove.y, totalMove.z
        );
    }

    remainingMove = totalMove;
    for (int iter = 0; iter < 5; ++iter)
    {
        SweepHit earliest;
        earliest.time = 1.0f;
        std::vector<SweepHit> toiHits;
        toiHits.reserve(4);
        constexpr float TOI_EPSILON = 0.001f;

        cap = p.getCapsule();
        candidates = gatherGLBTriangles(world, cap, remainingMove);
        appendUniqueTriangleIndices(candidates, bodyCandidateExtras);
        trace.maxCandidates = std::max(trace.maxCandidates, (int)candidates.size());
        trace.sweepIterations++;

        DebugVis::recordMovement(p.pos, remainingMove, "glb-substep-move");
        DebugVis::recordSweep(cap.a, cap.a + remainingMove, "capsule-bottom");
        DebugVis::recordSweep((cap.a + cap.b) * 0.5f, (cap.a + cap.b) * 0.5f + remainingMove, "capsule-mid");
        DebugVis::recordSweep(cap.b, cap.b + remainingMove, "capsule-top");
        for (int triIndex : candidates)
        {
            const CollisionTriangle& tri = world.collisionMesh.triangles[triIndex];
            SweepHit hit;
            if (!capsuleTriangleSweep(cap, remainingMove, tri, triIndex, hit))
                continue;
            if (rejectBelowTopFaceContact(cap, tri, hit.normal, hit.point, triIndex, "sweep"))
                continue;

            if (!earliest.hit || hit.time + TOI_EPSILON < earliest.time)
            {
                earliest = hit;
                toiHits.clear();
                toiHits.push_back(hit);
            }
            else if (hit.time <= earliest.time + TOI_EPSILON)
            {
                toiHits.push_back(hit);
            }
        }

        glm::vec3 stepMove = remainingMove * earliest.time;
        p.pos += stepMove;
        p.updateModelWorldTransforms();
        cap = p.getCapsule();

        if (!earliest.hit)
        {
            remainingMove = glm::vec3(0.0f);
            break;
        }

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

                if (!consistentStep) {
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
                        {
                            blocked = true;
                            break;
                        }
                    }
                }

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

                    if (!hasFloor) {
                        p.pos = originalPos;
                        p.updateModelWorldTransforms();
                        continue;
                    }

                    groundedThisFrame = true;
                    applyTouchResets(p);

                    if (p.vel.z < 0.0f)
                        p.vel.z = 0.0f;

                    remainingMove -= stepMove;

                    PHYS_LOG(
                        "[GLB STEP] stepped up %.3f remainingMove=(%.4f %.4f %.4f)\n",
                        stepHeight,
                        remainingMove.x, remainingMove.y, remainingMove.z
                    );

                    continue;
                }

                p.pos = originalPos;
                p.updateModelWorldTransforms();
            }
        }
        glm::vec3 depen = earliest.normal;

        if (depen.z > 0.0f && depen.z < 0.7f)
            depen.z = 0.0f;

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
        remainingMove -= stepMove;

        for (const SweepHit& hit : toiHits)
        {
            float vn = glm::dot(remainingMove, hit.normal);
            if (vn < 0.0f)
                remainingMove -= hit.normal * vn;
        }

        {
            Capsule slideCap = p.getCapsule();
            std::vector<int> slideCandidates = gatherGLBTriangles(world, slideCap, glm::vec3(0.0f));
            std::vector<RecoveryContact> slideContacts = collectCapsuleRecoveryContacts(
                world, slideCap, slideCandidates);
            trace.maxSlideContacts = std::max(trace.maxSlideContacts, (int)slideContacts.size());

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

        if (glm::dot(remainingMove, remainingMove) < 0.000001f)
            break;
    }
}
