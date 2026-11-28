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
#include "physics/movement/physics-scratch.h"
#include "physics/physical-entity.h"
#include "config/collision-config.h"
#include "debug/structured-log.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cmath>
#include <limits>
#include <vector>
#include <string>
#include <unordered_set>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
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

// TODO-DELETE: doGLBSweepSlide — superseded by the actor-triangle solver.
// Owner that replaces it: src/physics/movement/actor-triangle-solver.cpp
// (solveActorTriangleCollision). The triangle owner sweeps the actor's real body
// and weapon triangles from the safe pose to the desired pose, so the root
// capsule sweep/slide is no longer the gameplay collision authority for players.
// DO NOT DELETE until human gameplay testing proves the triangle path is at least
// as good as this in Dust 3 Siberia, Trainkinda, and Chain of Judgement, AND one
// of these is decided:
//   (a) step-up is reimplemented in the triangle owner (this function is the only
//       place that lifts the actor over MAX_STEP_HEIGHT ledges), or
//   (b) step-up is intentionally dropped by a spec decision.
// Consolidation target: solveActorTriangleCollision (+ a new triangle step-up).
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
    auto& scratch = physicsScratch();
    auto tSweepStart = std::chrono::steady_clock::now();
    const glm::vec3 velocityAtStart = p.vel;
    constexpr float SURFACE_SLOP = 0.01f;
    constexpr float MAX_CORRECTION = 2.0f;

    p.updateModelWorldTransforms();
    Capsule cap = p.getCapsule();
    uint32_t supportId = 0;
    glm::vec3 supportVelocity(0.0f);

    {
        auto t0 = std::chrono::steady_clock::now();
        gatherGLBTriangles(candidates, world, cap, totalMove, "Player_Capsule_SweepInitial");
        auto t1 = std::chrono::steady_clock::now();
        diag.broadphaseMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
    }
    diag.initialCandidates = (int)candidates.size();

    auto& bodySamples = scratch.vec3s;
    collectPlayerBodyCollisionSamples(p, bodySamples);
    diag.bodySampleCount = (int)bodySamples.size();

    {
        auto t0 = std::chrono::steady_clock::now();
        auto& bodyDeltas = scratch.vec3s2;
        bodyDeltas.assign(bodySamples.size(), glm::vec3(0.0f));
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
                auto& bodyUnionCandidates = scratch.ints3;
                bodyUnionCandidates.clear();
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
    float remainingTickSeconds = dt;
    int bounceDisplacementContacts = 0;
    glm::vec3 bounceDisplacement(0.0f);

    auto& preMoveCandidates = scratch.ints2;
    preMoveCandidates.assign(candidates.begin(), candidates.end());
    auto& toiHits = scratch.sweepHits;
    toiHits.clear();
    if (toiHits.capacity() < 4)
        toiHits.reserve(4);

    for (int iter = 0; iter < 5; ++iter)
    {
        trace.sweepIterations = iter + 1;
        toiHits.clear();
        auto tIterStart = std::chrono::steady_clock::now();
        SweepHit earliest;
        earliest.time = 1.0f;
        constexpr float TOI_EPSILON = 0.001f;

        cap = p.getCapsule();
        {
            auto t0 = std::chrono::steady_clock::now();
            char iterTag[64];
            std::snprintf(iterTag, sizeof(iterTag), "Player_Capsule_SweepIter_%d", iter);
            gatherGLBTriangles(candidates, world, cap, remainingMove, iterTag);

            // The legacy Arcade path bypasses the actor-triangle solver, so
            // this capsule sweep is its only pre-move wall/floor guard. Keep
            // every earliest-time contact: a corner needs both planes to
            // constrain the remaining move, while later contacts belong to a
            // subsequent iteration.
            for (const int triIndex : candidates)
            {
                if (triIndex < 0 ||
                    triIndex >= static_cast<int>(world.collisionMesh.triangles.size()))
                    continue;

                SweepHit hit;
                if (!capsuleTriangleSweep(
                        cap, remainingMove,
                        world.collisionMesh.triangles[triIndex], triIndex, hit) ||
                    !std::isfinite(hit.time) || hit.time < 0.0f || hit.time > 1.0f)
                    continue;

                // Arcade bypasses the actor-triangle solver, so route its
                // capsule TOI through the same cached smooth-seam policy.
                // Otherwise triangle edge normals can snag the capsule and
                // feed a false direction into both slide and bounce response.
                if (CollisionConfig::instance().arcadeCollision())
                {
                    glm::vec3 seamNormal(0.0f);
                    if (smoothSeamNormalAt(world, triIndex, hit.point, seamNormal))
                    {
                        if (glm::dot(seamNormal, hit.normal) < 0.0f)
                            seamNormal = -seamNormal;
                        if (glm::dot(remainingMove, seamNormal) < -ALMOST_ZERO)
                            hit.normal = seamNormal;
                    }
                }

                if (hit.time < earliest.time - TOI_EPSILON)
                {
                    earliest = hit;
                    toiHits.clear();
                    toiHits.push_back(hit);
                }
                else if (std::fabs(hit.time - earliest.time) <= TOI_EPSILON)
                {
                    const bool duplicate = std::any_of(
                        toiHits.begin(), toiHits.end(),
                        [&hit](const SweepHit& existing) {
                            return existing.entityId == hit.entityId &&
                                   existing.triangleIndex == hit.triangleIndex;
                        });
                    if (!duplicate)
                        toiHits.push_back(hit);
                }
            }

            // Arcade keeps root-capsule versus world collision, but dynamic
            // physical boxes (including the Payload cart) are also solid. Use
            // the entity owner's cached world triangles and sweep in relative
            // motion so a slow cart cannot tunnel through a stationary actor.
            if (CollisionConfig::instance().arcadeCollision())
            {
                const glm::vec3 startMin = glm::min(cap.a, cap.b);
                const glm::vec3 startMax = glm::max(cap.a, cap.b);
                const glm::vec3 endMin = glm::min(cap.a + remainingMove,
                                                  cap.b + remainingMove);
                const glm::vec3 endMax = glm::max(cap.a + remainingMove,
                                                  cap.b + remainingMove);
                const AABB sweepBounds{
                    glm::min(startMin, endMin) - glm::vec3(cap.r),
                    glm::max(startMax, endMax) + glm::vec3(cap.r)};
                const auto& entities = PhysicalEntitySystem::instance().entities();
                for (const PhysicalEntity& entity : entities)
                {
                    if (!entity.collidesWithActors ||
                        entity.shape != PhysicalEntityShape::Box ||
                        entity.localTriangles.empty())
                        continue;

                    const glm::vec3 half = glm::max(entity.halfExtents,
                                                    glm::vec3(0.0f));
                    AABB entityBounds;
                    entityBounds.min = glm::vec3(
                        std::numeric_limits<float>::max());
                    entityBounds.max = glm::vec3(
                        std::numeric_limits<float>::lowest());
                    for (int corner = 0; corner < 8; ++corner)
                    {
                        const glm::vec3 local(
                            (corner & 1) ? half.x : -half.x,
                            (corner & 2) ? half.y : -half.y,
                            (corner & 4) ? half.z : -half.z);
                        const glm::vec3 point = glm::vec3(
                            entity.transform * glm::vec4(local, 1.0f));
                        entityBounds.min = glm::min(entityBounds.min, point);
                        entityBounds.max = glm::max(entityBounds.max, point);
                    }
                    if (glm::any(glm::lessThan(sweepBounds.max,
                                               entityBounds.min)) ||
                        glm::any(glm::greaterThan(sweepBounds.min,
                                                  entityBounds.max)))
                        continue;

                    const EntitySurfaceCacheView surface =
                        cachedEntitySurface(entity);
                    if (!surface.meshCache)
                        continue;
                    const glm::vec3 relativeMove = remainingMove -
                        entity.velocity * remainingTickSeconds;
                    for (size_t triangleIndex = 0;
                         triangleIndex < surface.meshCache->triangles.size();
                         ++triangleIndex)
                    {
                        SweepHit hit;
                        if (!capsuleTriangleSweep(
                                cap, relativeMove,
                                surface.meshCache->triangles[triangleIndex],
                                static_cast<int>(triangleIndex), hit) ||
                            !std::isfinite(hit.time) || hit.time < 0.0f ||
                            hit.time > 1.0f)
                            continue;
                        hit.entityId = entity.id;
                        hit.surfaceVelocity = entity.velocity;
                        hit.colliderName = "physical_entity";
                        if (hit.time < earliest.time - TOI_EPSILON)
                        {
                            earliest = hit;
                            toiHits.clear();
                            toiHits.push_back(hit);
                        }
                        else if (std::fabs(hit.time - earliest.time) <=
                                 TOI_EPSILON)
                        {
                            const bool duplicate = std::any_of(
                                toiHits.begin(), toiHits.end(),
                                [&hit](const SweepHit& existing) {
                                    return existing.entityId == hit.entityId &&
                                           existing.triangleIndex ==
                                               hit.triangleIndex;
                                });
                            if (!duplicate)
                                toiHits.push_back(hit);
                        }
                    }
                }
            }
            auto t1 = std::chrono::steady_clock::now();
            diag.sweepMs += std::chrono::duration<float, std::milli>(t1 - t0).count();
        }

        glm::vec3 stepMove = remainingMove * earliest.time;
        const float nextRemainingSeconds = remainingTickSeconds *
            std::clamp(1.0f - earliest.time, 0.0f, 1.0f);

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
        for (size_t hitIndex = 0; hitIndex < toiHits.size(); ++hitIndex)
        {
            const SweepHit& hit = toiHits[hitIndex];
            if (hit.triangleIndex < 0 ||
                (hit.entityId == 0 &&
                 hit.triangleIndex >= (int)world.collisionMesh.triangles.size()))
                continue;

            // A real swept capsule impact must reach the configured bounce
            // response here; waiting for post-move depenetration only projects
            // velocity on a clean TOI stop. Fold coplanar seam triangles into
            // one impact, while retaining distinct corner planes.
            bool duplicateSurfaceNormal = false;
            for (size_t previousIndex = 0; previousIndex < hitIndex; ++previousIndex)
            {
                if (glm::dot(toiHits[previousIndex].normal, hit.normal) > 0.9995f)
                {
                    duplicateSurfaceNormal = true;
                    break;
                }
            }
            if (!duplicateSurfaceNormal)
            {
                const bool groundedSupport =
                    (p.ground.onGround || p.ground.stableOnGround || groundedThisFrame) &&
                    hit.normal.z > 0.995f;
                if (hit.entityId != 0)
                {
                    glm::vec3* velocities[] = {&p.vel, &p.externalImpulse};
                    for (glm::vec3* velocity : velocities)
                    {
                        const float into = glm::dot(
                            *velocity - hit.surfaceVelocity, hit.normal);
                        if (into < 0.0f)
                            *velocity -= hit.normal * into;
                    }
                }
                else if (groundedSupport)
                    projectVelocityAgainstNormal(p, hit.normal);
                else
                    respondVelocityAgainstNormal(
                        p, hit.normal, glm::vec3(0.0f), false, 0.0f,
                        "root_capsule", hit.triangleIndex);
            }

            if (hit.entityId != 0)
                continue;
            const char* feature = triangleFeatureLabel(world.collisionMesh.triangles[hit.triangleIndex], hit.point);
            if (feature[0] == 'f')
                trace.faceHits++;
            else if (feature[0] == 'e')
                trace.edgeHits++;
            else if (feature[0] == 'v')
                trace.vertexHits++;
        }

        if (earliest.entityId == 0)
        {
            float feetZ = cap.a.z - cap.r;
            bool stepWall = std::fabs(earliest.normal.z) < 0.2f;
            if (DebugConfig::DEBUG_COLLISION_SYSTEM)
                Debug::log(Debug::Category::Collision, "[COLL]   stepUpCheck: stepWall=%d feetZ=%.3f hitPointZ=%.3f stepH=%.3f maxStep=%.3f",
                           (int)stepWall, feetZ, earliest.point.z, earliest.point.z - feetZ, MAX_STEP_HEIGHT);
        }

        if (earliest.entityId == 0 &&
            std::fabs(earliest.normal.z) < 0.2f)
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
                    LOG_COLLISION("step_blocked", "[STEP] fallback to slide stepH=%.3f reason=inconsistent_samples",
                                  stepHeight);
                }
                else
                {
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
                            LOG_COLLISION("step_blocked", "[STEP] fallback to slide stepH=%.3f reason=no_landing",
                                          stepHeight);
                            p.pos = originalPos;
                            p.updateModelWorldTransforms();
                        }
                        else
                        {
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
                            remainingTickSeconds = nextRemainingSeconds;

                            LOG_COLLISION("step_success", "[STEP] succeeded stepH=%.3f remaining=(%.4f,%.4f,%.4f)",
                                          stepHeight, remainingMove.x, remainingMove.y, remainingMove.z);
                            continue;
                        }
                    }
                    else
                    {
                        LOG_COLLISION("step_blocked", "[STEP] fallback to slide stepH=%.3f reason=blocked",
                                      stepHeight);
                        p.pos = originalPos;
                        p.updateModelWorldTransforms();
                    }
                }
            }
        }
        // SEAM TRANSITION: when the sweep hits a non-walkable surface (edge, backface,
        // or wall at a mesh seam), check for a walkable surface ahead and project
        // movement against THAT surface instead of the blocking edge.
        if (earliest.entityId == 0)
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
                    remainingTickSeconds = nextRemainingSeconds;
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
                if (hit.entityId == 0 && hit.triangleIndex >= 0 &&
                    hit.triangleIndex <
                        static_cast<int>(world.collisionMesh.triangles.size()))
                    DebugVis::recordTriangle(
                        world.collisionMesh.triangles[hit.triangleIndex],
                        hit.triangleIndex, "sweep-hit-triangle");
        }
        {
            glm::vec3 beforeSlide = remainingMove;
            remainingMove -= stepMove;
            for (const SweepHit& hit : toiHits) {
                const glm::vec3 surfaceRemainder =
                    hit.surfaceVelocity * nextRemainingSeconds;
                glm::vec3 relativeRemainder =
                    remainingMove - surfaceRemainder;
                float vn = glm::dot(relativeRemainder, hit.normal);
                if (vn >= 0.0f)
                    continue;

                if (hit.entityId != 0)
                {
                    relativeRemainder -= hit.normal * vn;
                    remainingMove = relativeRemainder + surfaceRemainder;
                    continue;
                }

                const bool groundedSupport =
                    (p.ground.onGround || p.ground.stableOnGround || groundedThisFrame) &&
                    hit.normal.z > 0.995f;
                const float outwardSpeed = glm::dot(
                    p.vel + p.externalImpulse, hit.normal);
                if (!groundedSupport && outwardSpeed > 0.001f &&
                    nextRemainingSeconds > 0.0f)
                {
                    // Keep the tangential part of the swept remainder, but
                    // carry the configured root-capsule rebound through the
                    // unused fraction of this fixed tick. The next sweep still
                    // clips it against any corner plane before moving.
                    const float reboundDistance =
                        outwardSpeed * nextRemainingSeconds;
                    remainingMove += hit.normal * (reboundDistance - vn);
                    bounceDisplacement += hit.normal * reboundDistance;
                    ++bounceDisplacementContacts;
                }
                else
                {
                    remainingMove -= hit.normal * vn;
                }
            }
            remainingTickSeconds = nextRemainingSeconds;
            LOG_COLLISION("slide_proj", "[SLIDE] before=(%.4f,%.4f,%.4f) after=(%.4f,%.4f,%.4f) stepMove=(%.4f,%.4f,%.4f) hitNormal=(%.3f,%.3f,%.3f)",
                          beforeSlide.x, beforeSlide.y, beforeSlide.z,
                          remainingMove.x, remainingMove.y, remainingMove.z,
                          stepMove.x, stepMove.y, stepMove.z,
                          earliest.normal.x, earliest.normal.y, earliest.normal.z);
        }

        {
            Capsule slideCap = p.getCapsule();
            auto& slideCandidates = scratch.ints3;
            gatherGLBTriangles(slideCandidates, world, slideCap,
                               glm::vec3(0.0f), "Player_Capsule_SlideContacts");
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
            if (hit.entityId == 0)
            {
                applyCollisionContact(
                    p, groundedThisFrame, hit.normal, hit.point,
                    SURFACE_SLOP, hit.triangleIndex,
                    hit.colliderName.c_str());
            }
            else
            {
                glm::vec3* velocities[] = {&p.vel, &p.externalImpulse};
                for (glm::vec3* velocity : velocities)
                {
                    const float into = glm::dot(
                        *velocity - hit.surfaceVelocity, hit.normal);
                    if (into < 0.0f)
                        *velocity -= hit.normal * into;
                }
                if (hit.normal.z > MAX_WALKABLE_SLOPE_DOT &&
                    hit.point.z <= p.getCapsule().a.z - p.getCapsule().r + 0.15f)
                {
                    groundedThisFrame = true;
                    p.ground.onGround = true;
                    p.ground.stableOnGround = true;
                    p.ground.hasWorldContact = true;
                    p.ground.realWorldContactThisFrame = true;
                    p.ground.worldContactLostTimer = 0.033f;
                    supportId = hit.entityId;
                    supportVelocity = hit.surfaceVelocity;
                    appendPlayerMovementContact(
                        p, MovementContactKind::Ground, hit.normal, hit.point,
                        SURFACE_SLOP, hit.triangleIndex,
                        MovementContactSource::MovingStructure);
                }
            }

            static thread_local auto sLastEntityContactLog =
                std::chrono::steady_clock::time_point{};
            const auto entityContactNow = std::chrono::steady_clock::now();
            if (hit.entityId != 0 &&
                entityContactNow - sLastEntityContactLog >=
                    std::chrono::seconds(1))
            {
                sLastEntityContactLog = entityContactNow;
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Collision, StructuredLevel::Important,
                    "collision.arcade_entity.sweep", std::to_string(hit.entityId),
                    "Arcade root capsule swept against moving physical box",
                    static_cast<uint32_t>(p.movementSimulationTick),
                    {{"actor", p.username}, {"entity_id", hit.entityId},
                     {"time_of_impact", hit.time},
                     {"contact_point", {hit.point.x, hit.point.y, hit.point.z}},
                     {"contact_normal", {hit.normal.x, hit.normal.y, hit.normal.z}},
                     {"surface_velocity", {hit.surfaceVelocity.x,
                                             hit.surfaceVelocity.y,
                                             hit.surfaceVelocity.z}},
                     {"position", {p.pos.x, p.pos.y, p.pos.z}}},
                    __FILE__, __LINE__, __FUNCTION__);
            }
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

    // Moving support carries the actor by transforming the supported position
    // each tick (the sweep is already relative to the platform). Do not fold
    // the platform velocity into the actor's own velocity when support ends:
    // standing on a moving Payload cart, or stepping off it, must not launch the
    // actor or grant free speed.
    p.collision.supportEntityId = supportId;
    p.collision.supportVelocity = supportVelocity;

    auto tSweepEnd = std::chrono::steady_clock::now();
    float sweepTotalMs = std::chrono::duration<float, std::milli>(tSweepEnd - tSweepStart).count();
    Debug::logThrottled(Debug::Category::Collision, "sweep_diag_summary", 1.0f,
        "[SWEEP DIAG] totalMs=%.2f initialCand=%d bodySamples=%d iters=%d hits=%d maxTOI=%d "
        "stepUp=%d seam=%d broadphaseMs=%.2f bodyExtraMs=%.2f sweepMs=%.2f\n",
        sweepTotalMs, diag.initialCandidates, diag.bodySampleCount,
        trace.sweepIterations, trace.sweepHits, trace.maxSimultaneousTOI,
        diag.stepUpAttempts, diag.seamTransitions,
        diag.broadphaseMs, diag.bodyExtraMs, diag.sweepMs);

    // A bounded structured sample ties the fixed-tick input to the capsule
    // solver's actual contacts and resulting motion. It is the runtime evidence
    // for the Arcade path; verbose per-triangle tracing remains opt-in.
    static thread_local auto sLastCapsuleSummary =
        std::chrono::steady_clock::time_point{};
    const auto now = std::chrono::steady_clock::now();
    if ((trace.sweepHits > 0 || trace.maxSlideContacts > 0) &&
        now - sLastCapsuleSummary >= std::chrono::seconds(1))
    {
        sLastCapsuleSummary = now;
        trace.finalPos = p.pos;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Collision, StructuredLevel::Important,
            "collision.capsule-world.solve", p.username,
            "fixed-tick root capsule sweep and slide result",
            static_cast<uint32_t>(p.movementSimulationTick),
            nlohmann::json{
                {"arcade_mode", CollisionConfig::instance().arcadeCollision()},
                {"world_shape", "root_capsule"},
                {"start_position", {trace.startPos.x, trace.startPos.y,
                                      trace.startPos.z}},
                {"final_position", {p.pos.x, p.pos.y, p.pos.z}},
                {"input_move", {totalMove.x, totalMove.y, totalMove.z}},
                {"velocity_before", {velocityAtStart.x, velocityAtStart.y,
                                      velocityAtStart.z}},
                {"velocity_after", {p.vel.x, p.vel.y, p.vel.z}},
                {"bounce_displacement_contacts", bounceDisplacementContacts},
                {"bounce_displacement", {bounceDisplacement.x,
                                           bounceDisplacement.y,
                                           bounceDisplacement.z}},
                {"candidate_triangles", trace.maxCandidates},
                {"sweep_iterations", trace.sweepIterations},
                {"sweep_hits", trace.sweepHits},
                {"simultaneous_toi_contacts", trace.maxSimultaneousTOI},
                {"slide_contacts", trace.maxSlideContacts},
                {"face_hits", trace.faceHits},
                {"edge_hits", trace.edgeHits},
                {"vertex_hits", trace.vertexHits},
                {"solve_ms", sweepTotalMs}}
            , __FILE__, __LINE__, __FUNCTION__);
    }

    // The legacy no-progress event was emitted only by the actor-triangle
    // solver, which Arcade deliberately bypasses. Reuse the actor's existing
    // collision counters here so the same stall contract covers the capsule
    // backend and the Dust2 regression route cannot pass vacuously.
    if (CollisionConfig::instance().arcadeCollision())
    {
        CollisionState& collision = p.collision;
        const uint64_t tick = p.movementSimulationTick;
        const float requestedH =
            glm::length(glm::vec2(totalMove.x, totalMove.y));
        const float achievedH = glm::length(glm::vec2(
            p.pos.x - trace.startPos.x, p.pos.y - trace.startPos.y));
        const float horizontalSpeed = glm::length(glm::vec2(p.vel.x, p.vel.y));
        const bool wantsMove = glm::length(p.inputWishMove) > 0.1f;
        const bool supported = groundedThisFrame || p.ground.onGround;
        const bool hasWorldContact = trace.sweepHits > 0 ||
                                     trace.maxRecoveryContacts > 0;

        if (tick != collision.progressStallLastTick)
        {
            collision.progressStallLastTick = tick;
            if (wantsMove && supported && hasWorldContact &&
                horizontalSpeed < 0.5f && requestedH > 0.01f &&
                achievedH < 0.002f)
            {
                if (collision.progressStallTicks == 0)
                {
                    collision.progressStallStartTick = tick;
                    collision.progressStallAnchor = trace.startPos;
                }
                ++collision.progressStallTicks;
            }
            else if (collision.progressStallTicks > 0)
            {
                if (collision.progressStallTicks >= 10)
                {
                    StructuredLogger::instance().writeEvent(
                        StructuredCategory::Collision, StructuredLevel::Important,
                        "collision.progress.stall.end", "arcade-capsule",
                        "Arcade capsule resumed horizontal progress",
                        static_cast<uint32_t>(tick),
                        {{"actor", p.username},
                         {"stalled_ticks", collision.progressStallTicks},
                         {"stall_start_tick", collision.progressStallStartTick},
                         {"stall_anchor", {collision.progressStallAnchor.x,
                                            collision.progressStallAnchor.y,
                                            collision.progressStallAnchor.z}},
                         {"position", {p.pos.x, p.pos.y, p.pos.z}},
                         {"velocity", {p.vel.x, p.vel.y, p.vel.z}}},
                        __FILE__, __LINE__, __FUNCTION__);
                }
                collision.progressStallTicks = 0;
            }
        }

        if (collision.progressStallTicks >= 10 &&
            (collision.progressStallTicks == 10 ||
             tick - collision.progressStallLastLogTick >= 60))
        {
            collision.progressStallLastLogTick = tick;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Collision, StructuredLevel::Important,
                "collision.progress.stall", "arcade-capsule",
                "input held but Arcade root capsule made no horizontal progress",
                static_cast<uint32_t>(tick),
                {{"actor", p.username},
                 {"equipped_weapon", p.equippedWeaponId},
                 {"arcade_mode", true},
                 {"world_shape", "root_capsule"},
                 {"stalled_ticks", collision.progressStallTicks},
                 {"stall_start_tick", collision.progressStallStartTick},
                 {"stall_anchor", {collision.progressStallAnchor.x,
                                    collision.progressStallAnchor.y,
                                    collision.progressStallAnchor.z}},
                 {"position", {p.pos.x, p.pos.y, p.pos.z}},
                 {"velocity", {p.vel.x, p.vel.y, p.vel.z}},
                 {"wish", {p.inputWishMove.x, p.inputWishMove.y}},
                 {"requested_horizontal_m", requestedH},
                 {"achieved_horizontal_m", achievedH},
                 {"grounded", supported},
                 {"contact_count", trace.maxRecoveryContacts},
                 {"sweep_hits", trace.sweepHits},
                 {"simultaneous_toi_contacts", trace.maxSimultaneousTOI},
                 {"slide_contacts", trace.maxSlideContacts},
                 {"likely_corner_stop", trace.maxSimultaneousTOI >= 2 ||
                                            trace.maxSlideContacts >= 2}},
                __FILE__, __LINE__, __FUNCTION__);
        }

        const glm::vec3 velocityBefore = velocityAtStart + p.externalImpulse;
        const glm::vec3 velocityAfter = p.vel + p.externalImpulse;
        const float speedBefore = glm::length(velocityBefore);
        const float speedAfter = glm::length(velocityAfter);
        const float horizontalSpeedBefore = glm::length(
            glm::vec2(velocityBefore.x, velocityBefore.y));
        const bool speedCollapsed = hasWorldContact &&
            !p.freeze.freezeActive && speedBefore > 1.0f &&
            speedAfter < speedBefore * 0.1f;
        if (!speedCollapsed)
        {
            collision.speedCollapseEpisodeActive = false;
        }
        else if (!collision.speedCollapseEpisodeActive ||
                 tick - collision.speedCollapseLastLogTick >= 30)
        {
            collision.speedCollapseEpisodeActive = true;
            collision.speedCollapseLastLogTick = tick;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Collision, StructuredLevel::Important,
                "collision.speed_collapse", "arcade-capsule",
                "Arcade capsule speed dropped by more than 90 percent in one fixed tick",
                static_cast<uint32_t>(tick),
                {{"actor", p.username},
                 {"position", {p.pos.x, p.pos.y, p.pos.z}},
                 {"velocity_before", {velocityBefore.x, velocityBefore.y,
                                       velocityBefore.z}},
                 {"velocity_after", {velocityAfter.x, velocityAfter.y,
                                      velocityAfter.z}},
                 {"speed_before", speedBefore},
                 {"speed_after", speedAfter},
                 {"horizontal_speed_before", horizontalSpeedBefore},
                 {"horizontal_speed_after", glm::length(glm::vec2(
                      velocityAfter.x, velocityAfter.y))},
                 {"wish", {p.inputWishMove.x, p.inputWishMove.y}},
                 {"grounded", supported},
                 {"likely_corner_stop", trace.maxSimultaneousTOI >= 2 ||
                                            trace.maxSlideContacts >= 2},
                 {"simultaneous_toi_contacts", trace.maxSimultaneousTOI},
                 {"slide_contacts", trace.maxSlideContacts},
                 {"sweep_hits", trace.sweepHits},
                 {"recovery_contacts", trace.maxRecoveryContacts}},
                __FILE__, __LINE__, __FUNCTION__);
        }
    }
}
