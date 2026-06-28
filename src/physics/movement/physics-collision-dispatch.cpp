#include <algorithm>
#include <cstdio>
#include <vector>
#include <cmath>
#include <limits>
#include <string>
#include <unordered_set>
#include <glm/glm.hpp>

#include "physics/config.h"
#include "world/world.h"
#include "entities/player.h"
#include "config.h"
#include "debug/debug-log.h"
#include "debug/debug-visuals.h"
#include "perf/perf.h"
#include "config/player-settings.h"
#include "perf/perf.h"
#include "physics/movement/physics-collision.h"
#include "physics/movement/physics-collision-shared.h"

#define PHYS_LOG(...) Debug::logThrottled(Debug::Category::Collision, "physics-collision", DebugConfig::PRINT_INTERVAL, __VA_ARGS__)

extern CollisionTraceSnapshot gLastCollisionTrace;

static inline AABB makeBlockAABB(const Block& b)
{
    glm::vec3 half = b.size * 0.5f;
    return { b.pos - half, b.pos + half };
}

std::vector<RecoveryContact> collectBlockContactsForCapsule(
    const Capsule& cap,
    const std::vector<Block*>& nearbyBlocks
);

bool findBlockFallbackEscape(
    const Capsule& cap,
    const std::vector<Block*>& nearbyBlocks,
    const std::vector<RecoveryContact>& contacts,
    const glm::vec3& weightedNormal,
    glm::vec3& outCorrection
);

extern std::vector<int> gatherGLBTriangles(const World& world, const Capsule& cap, const glm::vec3& move);

std::string collisionLastTraceSummary()
{
    const CollisionTraceSnapshot& trace = gLastCollisionTrace;
    char buf[768];
    std::snprintf(buf, sizeof(buf),
        "[COLLISION TRACE] start=(%.2f %.2f %.2f) final=(%.2f %.2f %.2f) move=(%.3f %.3f %.3f) candidates=%d/%d sweeps=%d hits=%d toiMax=%d slide=%d recovery=%d final=%d safety=%d maxPen=%.4f features(f/e/v)=%d/%d/%d resweep=%d emergency=%d",
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
    return std::string(buf);
}

std::string collisionStateSummary(const Player& p)
{
    const CollisionTraceSnapshot& tr = gLastCollisionTrace;
    char buf[1024];
    std::snprintf(buf, sizeof(buf),
        "[COLLISION STATE]\n"
        "  pos=(%.2f %.2f %.2f) vel=(%.2f %.2f %.2f)\n"
        "  onGround=%d stableOnGround=%d groundLostTimer=%.4f airborneTimer=%.4f\n"
        "  wasOnGround=%d didLand=%d\n"
        "  landingCooldown=%.3f worldContact=%.4f\n"
        "  finalPos=(%.2f %.2f %.2f) floorSnapDist=0.25f\n"
        "  frameCandidates=%d/%d sweepHits=%d recovery=%d final=%d maxPen=%.4f\n",
        p.pos.x, p.pos.y, p.pos.z,
        p.vel.x, p.vel.y, p.vel.z,
        (int)p.ground.onGround, (int)p.ground.stableOnGround, p.ground.groundLostTimer, p.ground.airborneTimer,
        (int)p.ground.wasOnGround, (int)p.ground.didLand,
        p.ground.landingCooldown, p.ground.worldContactLostTimer,
        tr.finalPos.x, tr.finalPos.y, tr.finalPos.z,
        tr.initialCandidates, tr.maxCandidates,
        tr.sweepHits, tr.maxRecoveryContacts, tr.finalContacts, tr.maxPenetration);
    return std::string(buf);
}

void doCollisions(
    Player& p,
    const World& world,
    bool& groundedThisFrame,
    float dt
)
{
    Perf::ScopedTimer _colTimer("Collision");
    const glm::vec3 frameStart = p.pos;
    recoverInvalidPlayerCollisionState(p, frameStart, "start");

    p.collision.bounceCooldown = std::max(0.0f, p.collision.bounceCooldown - dt);
    if (!world.collisionMesh.empty())
    {
        doGLBTriangleCollisions(p, world, groundedThisFrame, dt);
        recoverInvalidPlayerCollisionState(p, frameStart, "glb");

        if (DebugConfig::DEBUG_COLLISION_SYSTEM) {
            Capsule debugCap = p.getCapsule();
            std::vector<int> debugCands = gatherGLBTriangles(world, debugCap, glm::vec3(0.0f));
            std::vector<glm::vec3> debugSamps = collectPlayerBodyCollisionSamples(p);
            std::vector<RecoveryContact> reportContacts = collectGLBRecoveryContacts(
                world, debugCap, debugSamps, debugCands, BODY_SAMPLE_RADIUS);
            float maxPen = 0.0f;
            for (const auto& rc : reportContacts)
                maxPen = std::max(maxPen, rc.penetration);
            printf("[COLLISION] contacts=%zu penetration=%.4f\n",
                   reportContacts.size(), maxPen);
        }

        return;
    }

    glm::vec3 move = (p.vel + p.externalImpulse) * dt;

    Capsule cap = p.getCapsule();

    std::vector<Block*> nearbyBlocksA;
    std::vector<Sphere*> nearbySpheresA;
    std::vector<Block*> nearbyBlocksB;
    std::vector<Sphere*> nearbySpheresB;

    world.getNearby(p.pos,        nearbyBlocksA, nearbySpheresA);
    world.getNearby(p.pos + move, nearbyBlocksB, nearbySpheresB);

    std::vector<Block*> nearbyBlocks = nearbyBlocksA;
    for (Block* b : nearbyBlocksB)
    {
        if (std::find(nearbyBlocks.begin(), nearbyBlocks.end(), b) == nearbyBlocks.end())
            nearbyBlocks.push_back(b);
    }
    std::vector<Block*> uniqueNearbyBlocks;
    for (Block* b : nearbyBlocks)
    {
        if (std::find(uniqueNearbyBlocks.begin(), uniqueNearbyBlocks.end(), b) == uniqueNearbyBlocks.end())
            uniqueNearbyBlocks.push_back(b);
    }
    nearbyBlocks = uniqueNearbyBlocks;

    for (int i = 0; i < 4; i++)
    {
        float earliest = 1.0f;
        glm::vec3 hitNormal(0.0f);
        glm::vec3 sweepStart = p.pos;

        for (Block* b : nearbyBlocks)
        {
            if (!b || b->isSlope)
                continue;

            AABB ba = makeBlockAABB(*b);

            float t = 1.0f;
            glm::vec3 normal(0.0f);

            if (capsuleSweep(cap, move, ba, t, normal))
            {
                if (t < earliest)
                {
                    earliest = t;
                    hitNormal = normal;
                }
            }
        }

        DebugVis::recordMovement(p.pos, move, "block-substep-move");
        DebugVis::recordSweep(cap.a, cap.a + move, "block-capsule-bottom");
        DebugVis::recordSweep((cap.a + cap.b) * 0.5f, (cap.a + cap.b) * 0.5f + move, "block-capsule-mid");
        DebugVis::recordSweep(cap.b, cap.b + move, "block-capsule-top");
        glm::vec3 stepMove = move * earliest;
        p.pos += stepMove;
        cap = p.getCapsule();

        if (earliest >= 1.0f)
            break;

        move -= stepMove;
        DebugVis::recordHit(p.pos, hitNormal, -1, "block-sweep");

        if (std::fabs(hitNormal.z) < 0.2f)
        {
            p.ground.realWorldContactThisFrame = true;

            p.jump.airJumpsLeft = AIR_JUMPS_MAX;
            p.dash.dashAvailable = true;
            p.groundReturn.available = true;
            p.freeze.freezeAvailable = true;

            float feetZ = cap.a.z - cap.r;

            Block* hitBlock = nullptr;
            float bestT = earliest;

            for (Block* b : nearbyBlocks)
            {
                if (!b || b->isSlope)
                    continue;

                AABB ba = makeBlockAABB(*b);

                float t = 1.0f;
                glm::vec3 normal(0.0f);

                if (capsuleSweep(cap, move, ba, t, normal))
                {
                    if (t <= bestT + 0.0001f)
                    {
                        bestT = t;
                        hitBlock = b;
                    }
                }
            }

            if (hitBlock)
            {
                AABB hitAABB = makeBlockAABB(*hitBlock);
                float stepTopZ = hitAABB.max.z;
                float stepHeight = stepTopZ - feetZ;

                if (stepHeight > 0.0f && stepHeight <= MAX_STEP_HEIGHT)
                {
                    glm::vec3 originalPos = p.pos;

                    p.pos.z += stepHeight + 0.001f;

                    Capsule stepCap = p.getCapsule();

                    bool blocked = false;

                    for (Block* b : nearbyBlocks)
                    {
                        if (!b || b->isSlope)
                            continue;

                        AABB ba = makeBlockAABB(*b);

                        glm::vec3 corr;
                        bool g = false;

                        if (capsuleVsBlock(stepCap, ba, corr, g))
                        {
                            if (corr.z <= 0.0f || std::fabs(corr.x) > 0.001f || std::fabs(corr.y) > 0.001f)
                            {
                                blocked = true;
                                break;
                            }
                        }
                    }

                    if (!blocked)
                    {
                        PHYS_LOG("[STEP] stepped up %.3f\n", stepHeight);
                        cap = p.getCapsule();
                        groundedThisFrame = true;

                        if (p.vel.z < 0.0f)
                            p.vel.z = 0.0f;

                        continue;
                    }

                    p.pos = originalPos;
                    cap = p.getCapsule();
                }
            }
        }

        if (hitNormal.z > 0.0f)
        {
            groundedThisFrame = true;
            p.ground.realWorldContactThisFrame = true;

            if (p.vel.z <= 0.0f)
            {
                p.jump.airJumpsLeft = AIR_JUMPS_MAX;
                p.dash.dashAvailable = true;
                p.groundReturn.available = true;
                p.freeze.freezeAvailable = true;

                p.vel.z = 0.0f;
            }
            DebugVis::recordGroundNormal(p.pos, hitNormal, "block-ground");
        }
        else if (hitNormal.z < 0.0f)
        {
            if (p.vel.z > 0.0f)
                p.vel.z = 0.0f;
        }

        float vn = glm::dot(move, hitNormal);
        if (vn < 0.0f)
            move -= hitNormal * vn;

        clampVelocityAgainstNormal(p, hitNormal);

        glm::vec3 nudge = hitNormal * 0.002f;
        p.pos += nudge;
        DebugVis::recordDepenetration(sweepStart + stepMove, nudge, "block-sweep-margin");
    }

    constexpr float BLOCK_DEPEN_SLOP = 0.002f;
    constexpr float BLOCK_MAX_CORRECTION = 2.0f;
    constexpr int MAX_BLOCK_RECOVERY_ITERATIONS = 6;
    for (int recoverIter = 0; recoverIter < MAX_BLOCK_RECOVERY_ITERATIONS; ++recoverIter)
    {
        cap = p.getCapsule();
        std::vector<RecoveryContact> contacts = collectBlockContactsForCapsule(cap, nearbyBlocks);

        if (contacts.empty())
            break;

        glm::vec3 correction = solveBatchedCorrection(contacts, BLOCK_DEPEN_SLOP, nullptr, nullptr, move, p.pos);
        float corrLen = glm::length(correction);
        if (corrLen > BLOCK_MAX_CORRECTION)
            correction *= BLOCK_MAX_CORRECTION / corrLen;

        p.pos += correction;

        for (const RecoveryContact& c : contacts)
        {
            applyCollisionContact(
                p, groundedThisFrame,
                c.normal, c.point, c.penetration,
                c.triangleIndex, c.label
            );
        }

        DebugVis::recordDepenetration(p.pos - correction, correction, "block-batched-depen");

        PHYS_LOG(
            "[PHYS][BLOCK DEPEN] iter=%d contacts=%zu correction=(%.4f %.4f %.4f)\n",
            recoverIter, contacts.size(),
            correction.x, correction.y, correction.z
        );

        if (glm::dot(correction, correction) < 0.0000001f)
            break;
    }

    cap = p.getCapsule();
    {
        std::vector<RecoveryContact> remaining = collectBlockContactsForCapsule(cap, nearbyBlocks);
        if (!remaining.empty()) {
            glm::vec3 weightedNormal(0.0f);
            for (const RecoveryContact& c : remaining)
                weightedNormal += c.normal * std::max(c.penetration, BLOCK_DEPEN_SLOP);
            glm::vec3 escape(0.0f);
            if (findBlockFallbackEscape(cap, nearbyBlocks, remaining, weightedNormal, escape)) {
                glm::vec3 before = p.pos;
                p.pos += escape;
                p.vel = glm::vec3(0.0f);
                DebugVis::recordDepenetration(before, escape, "block-overlap-escape");
                PHYS_LOG("[PHYS][BLOCK ESCAPE] contacts=%zu correction=(%.4f %.4f %.4f)\n",
                         remaining.size(), escape.x, escape.y, escape.z);
            }
        }
    }

    {
        constexpr float GROUND_SNAP_DISTANCE = 0.25f;
        constexpr float MAX_UPWARD_VEL_FOR_SNAP = 1.0f;

        if (p.vel.z <= MAX_UPWARD_VEL_FOR_SNAP)
        {
            Capsule checkCap = p.getCapsule();
            float feetZ = checkCap.a.z - checkCap.r;

            float bestGroundZ = -FLT_MAX;
            for (Block* b : nearbyBlocks)
            {
                if (!b || b->isSlope) continue;

                AABB ba = makeBlockAABB(*b);
                float blockTopZ = ba.max.z;

                if (checkCap.a.x + checkCap.r >= ba.min.x && checkCap.a.x - checkCap.r <= ba.max.x &&
                    checkCap.a.y + checkCap.r >= ba.min.y && checkCap.a.y - checkCap.r <= ba.max.y)
                {
                    if (blockTopZ < feetZ && blockTopZ > bestGroundZ)
                        bestGroundZ = blockTopZ;
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

                    PHYS_LOG("[PHYS][GROUND SNAP] snapped %.4f to block ground at %.2f\n", snapAmount, bestGroundZ);

                    {
                        Capsule postSnapCap = p.getCapsule();
                        std::vector<RecoveryContact> snapContacts = collectBlockContactsForCapsule(postSnapCap, nearbyBlocks);
                        if (!snapContacts.empty())
                        {
                            glm::vec3 snapCorrection = solveBatchedCorrection(snapContacts, BLOCK_DEPEN_SLOP, nullptr, nullptr);
                            float snapCorrLen = glm::length(snapCorrection);
                            if (snapCorrLen > BLOCK_MAX_CORRECTION)
                                snapCorrection *= BLOCK_MAX_CORRECTION / snapCorrLen;
                            p.pos += snapCorrection;
                            DebugVis::recordDepenetration(p.pos - snapCorrection, snapCorrection, "block-post-snap-wall-fix");

                            for (const RecoveryContact& c : snapContacts)
                                applyCollisionContact(p, groundedThisFrame, c.normal, c.point, c.penetration, c.triangleIndex, c.label);
                        }
                    }
                }
            }
        }
    }

    cap = p.getCapsule();
    std::vector<RecoveryContact> blockContacts = collectBlockContactsForCapsule(cap, nearbyBlocks);
    for (const RecoveryContact& c : blockContacts)
    {
        if (c.normal.z > MAX_WALKABLE_SLOPE_DOT)
            continue;

        glm::vec3 beforeVel = p.vel;
        projectVelocityAgainstNormal(p, c.normal);

        if (DebugConfig::DEBUG_COLLISION_SYSTEM && glm::length(beforeVel - p.vel) > 0.01f) {
            printf("[COLLISION SLIDE] block before=(%.2f %.2f %.2f) normal=(%.2f %.2f %.2f) after=(%.2f %.2f %.2f)\n",
                beforeVel.x, beforeVel.y, beforeVel.z,
                c.normal.x, c.normal.y, c.normal.z,
                p.vel.x, p.vel.y, p.vel.z);
        }
    }

    if (DebugConfig::DEBUG_COLLISION_SYSTEM) {
        float maxPen = 0.0f;
        for (const auto& rc : blockContacts)
            maxPen = std::max(maxPen, rc.penetration);
        printf("[COLLISION] block contacts=%zu penetration=%.4f\n",
               blockContacts.size(), maxPen);
    }

    recoverInvalidPlayerCollisionState(p, frameStart, "blocks");
}
