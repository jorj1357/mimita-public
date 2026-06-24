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

// =====================================================
// DEBUG TOGGLE
// =====================================================
#define PHYS_LOG(...) Debug::logThrottled(Debug::Category::Collision, "physics-collision", DebugConfig::PRINT_INTERVAL, __VA_ARGS__)

std::vector<int> gatherGLBTriangles(
    const World& world,
    const Capsule& cap,
    const glm::vec3& move
) {
    std::vector<int> out;
    AABB sweepBounds = makeSweptCapsuleAABB(cap, move);
    appendChunkTrianglesForAABB(world, sweepBounds, cap.r, out);
    return out;
}

static std::vector<int> gatherGLBTrianglesForSphere(
    const World& world,
    glm::vec3 center,
    float radius,
    const glm::vec3& move
) {
    std::vector<int> out;
    AABB sweepBounds;
    sweepBounds.min = glm::min(center, center + move) - glm::vec3(radius);
    sweepBounds.max = glm::max(center, center + move) + glm::vec3(radius);
    appendChunkTrianglesForAABB(world, sweepBounds, radius, out);
    return out;
}

void doGLBTriangleCollisions(
    Player& p,
    const World& world,
    bool& groundedThisFrame,
    float dt
) {
    constexpr float SURFACE_SLOP = 0.01f;
    constexpr float MAX_CORRECTION = 2.0f;

    glm::vec3 totalMove = (p.vel + p.externalImpulse) * dt;
    p.updateModelWorldTransforms();
    Capsule cap = p.getCapsule();

    std::vector<int> candidates = gatherGLBTriangles(world, cap, totalMove);
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

    CollisionTraceSnapshot trace;
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

    glm::vec3 remainingMove = totalMove;
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

    {
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

    {
        p.updateModelWorldTransforms();
        recomputeWeaponCapsule(p);
        std::vector<BodyWeaponSphere> bwSpheres = collectBodyWeaponSpheres(p);

        if (!bwSpheres.empty())
        {
            AABB bwBounds;
            bool boundsSet = false;
            for (const auto& bs : bwSpheres)
            {
                glm::vec3 expand(bs.radius + 0.5f);
                if (!boundsSet) {
                    bwBounds.min = bs.center - expand;
                    bwBounds.max = bs.center + expand;
                    boundsSet = true;
                } else {
                    bwBounds.min = glm::min(bwBounds.min, bs.center - expand);
                    bwBounds.max = glm::max(bwBounds.max, bs.center + expand);
                }
            }

            std::vector<int> bwCandidates;
            if (boundsSet)
                appendChunkTrianglesForAABB(world, bwBounds, 0.5f, bwCandidates);

            std::vector<RecoveryContact> bwContacts =
                collectBodyWeaponContacts(p, world, bwCandidates, bwSpheres);

            if (!bwContacts.empty())
            {
                std::vector<RecoveryContact> pushContacts;
                for (const auto& c : bwContacts)
                {
                    if (c.normal.z <= MAX_WALKABLE_SLOPE_DOT)
                        pushContacts.push_back(c);
                }

                if (!pushContacts.empty())
                {
                    PHYS_LOG("[PHYS][BODY-WEAPON] %zu contacts (%zu push) at final pos\n",
                             bwContacts.size(), pushContacts.size());

                    Capsule bwRootCap = p.getCapsule();
                    std::vector<int> bwRootCandidates = gatherGLBTriangles(world, bwRootCap, glm::vec3(0.0f));
                    std::vector<RecoveryContact> bwRootContacts =
                        collectCapsuleRecoveryContacts(world, bwRootCap, bwRootCandidates);

                    std::vector<RecoveryContact> allContacts;
                    allContacts.insert(allContacts.end(), bwRootContacts.begin(), bwRootContacts.end());
                    allContacts.insert(allContacts.end(), pushContacts.begin(), pushContacts.end());

                    glm::vec3 combinedCorrection = solveBatchedCorrection(allContacts, 0.01f);
                    float combLen = glm::length(combinedCorrection);
                    if (combLen > 0.001f)
                    {
                        constexpr float MAX_BW_CORRECTION = 0.5f;
                        if (combLen > MAX_BW_CORRECTION)
                            combinedCorrection *= MAX_BW_CORRECTION / combLen;

                        p.pos += combinedCorrection;
                        DebugVis::recordDepenetration(p.pos - combinedCorrection, combinedCorrection, "body-weapon-unified");

                        for (const RecoveryContact& pc : pushContacts)
                        {
                            if (pc.normal.z <= MAX_WALKABLE_SLOPE_DOT)
                                projectVelocityAgainstNormal(p, pc.normal);
                        }
                    }
                }
            }
        }
    }

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
