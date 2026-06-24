// C:\important\quiet\n\mimita-priv-v7\src\physics\movement\physics-collision.cpp
// feb 10 2026
// Purpose:
// - Handle ALL solid world collisions
// - No slope logic
// - No audio
// - No input handling
// - Pure positional correction + grounded detection
//
// Exposes:
//   doCollisions(...)

/**
 * 6 6 2026
 * possible to split this file into other smaller bits?
 * but might not be necsary
 * idk i  jorj lke to have files be 100 lines or less but if its works its works
 */

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
#include "config/player-settings.h"
#include "perf/perf.h"
#include "physics/movement/physics-collision.h"
#include "physics/movement/physics-collision-shared.h"

// =====================================================
// DEBUG TOGGLE
// =====================================================
#define PHYS_LOG(...) Debug::logThrottled(Debug::Category::Collision, "physics-collision", DebugConfig::PRINT_INTERVAL, __VA_ARGS__)

CollisionTraceSnapshot gLastCollisionTrace;

void recoverInvalidPlayerCollisionState(Player& p, const glm::vec3& frameStart, const char* phase)
{
    if (isFiniteVec3(p.pos) && isFiniteVec3(p.vel) && isFiniteVec3(p.externalImpulse))
        return;

    glm::vec3 recovery = isFiniteVec3(frameStart) ? frameStart : p.respawnPosition;
    if (!isFiniteVec3(recovery))
        recovery = glm::vec3(0.0f);

    Debug::log(Debug::Category::Collision,
        "[COLLISION RECOVER] invalid state phase=%s pos=(%.3f %.3f %.3f) vel=(%.3f %.3f %.3f) external=(%.3f %.3f %.3f)\n",
        phase ? phase : "unknown",
        p.pos.x, p.pos.y, p.pos.z,
        p.vel.x, p.vel.y, p.vel.z,
        p.externalImpulse.x, p.externalImpulse.y, p.externalImpulse.z);

    p.pos = recovery;
    p.vel = glm::vec3(0.0f);
    p.externalImpulse = glm::vec3(0.0f);
    p.syncLegacyStateToLayers();
    p.updateModelWorldTransforms();
}

void appendUniqueTriangleIndices(std::vector<int>& dst, const std::vector<int>& src)
{
    for (int triIndex : src)
    {
        if (std::find(dst.begin(), dst.end(), triIndex) == dst.end())
            dst.push_back(triIndex);
    }
}

// CHANGED: Simplified collision contact. Walls slide instead of bounce, jun 6 2026
void applyCollisionContact(
    Player& p,
    bool& groundedThisFrame,
    const glm::vec3& normal,
    glm::vec3 point,
    float penetration,
    int triangleIndex,
    const char* label
) {
    if (DebugConfig::COLLISION_VERBOSE)
        Debug::log(Debug::Category::Collision,
                   "[CONTACT] label=%s tri=%d normal=(%.3f %.3f %.3f) penetration=%.4f\n",
                   label, triangleIndex, normal.x, normal.y, normal.z, penetration);

    const PlayerSettings& cfg = GetPlayerSettings();

    p.realWorldContactThisFrame = true;
    p.hasWorldContact = true;
    p.worldContactLostTimer = 0.033f; // 2 frames of hysteresis for consistent wall climb

    // Ground: slope is walkable — only if contact is at or below player's feet.
    // This prevents body/weapon contacts with upward-facing surfaces above the
    // player from falsely setting grounded.
    if (normal.z > MAX_WALKABLE_SLOPE_DOT)
    {
        Capsule cap = p.getCapsule();
        float feetZ = cap.a.z - cap.r;
        if (point.z <= feetZ + 0.15f)
        {
            PHYS_LOG("[GROUND SET] source=%s tri=%d normal=(%.3f %.3f %.3f) point=(%.3f %.3f %.3f) feetZ=%.3f dist=%.3f pos=(%.3f %.3f %.3f) reason=walkable_contact\n",
                label, triangleIndex, normal.x, normal.y, normal.z,
                point.x, point.y, point.z, feetZ, feetZ - point.z,
                p.pos.x, p.pos.y, p.pos.z);

            groundedThisFrame = true;
            applyTouchResets(p);

            if (p.vel.z < 0.0f)
                p.vel.z = 0.0f;

            if (p.vel.z > 0.0f)
                p.vel.z = 0.0f;

            if (p.externalImpulse.z > 0.0f)
                p.externalImpulse.z = 0.0f;

            DebugVis::recordGroundNormal(point, normal, label);
        }
        else
        {
            PHYS_LOG("[GROUND REJECT] source=%s tri=%d normal=(%.3f %.3f %.3f) point=(%.3f %.3f %.3f) feetZ=%.3f dist=%.3f pos=(%.3f %.3f %.3f) reason=contact_above_feet\n",
                label, triangleIndex, normal.x, normal.y, normal.z,
                point.x, point.y, point.z, feetZ, point.z - feetZ,
                p.pos.x, p.pos.y, p.pos.z);
            projectVelocityAgainstNormal(p, normal);
        }
    }
    // Steep slope: not ground, slide along surface
    else if (normal.z > 0.0f)
    {
        applyTouchResets(p);
        // Project velocity out of the slope, preserving tangential slide
        projectVelocityAgainstNormal(p, normal);
    }
    else if (normal.z < -MAX_WALKABLE_SLOPE_DOT)
    {
        applyTouchResets(p);
        if (p.vel.z > 0.0f)
            p.vel.z = 0.0f;
    }
    else
    {
        // Wall or ceiling: project velocity against normal
        projectVelocityAgainstNormal(p, normal);
        applyTouchResets(p);
    }

    DebugVis::recordContact(point, normal, penetration, triangleIndex, label);
}

static inline AABB makeBlockAABB(const Block& b)
{
    glm::vec3 half = b.size * 0.5f;
    return { b.pos - half, b.pos + half };
}

void appendChunkTrianglesForAABB(
    const World& world,
    const AABB& queryBounds,
    float expansion,
    std::vector<int>& out
) {
    if (world.collisionChunks.empty() || world.collisionChunkSize <= 0.001f)
    {
        for (int i = 0; i < (int)world.collisionMesh.triangles.size(); ++i)
        {
            AABB triBounds = makeTriangleAABB(world.collisionMesh.triangles[i]);
            triBounds.min -= glm::vec3(expansion);
            triBounds.max += glm::vec3(expansion);
            if (overlaps(queryBounds, triBounds))
                out.push_back(i);
        }
        return;
    }

    glm::ivec3 c0 = collisionChunkCoord(queryBounds.min, world.collisionChunkSize);
    glm::ivec3 c1 = collisionChunkCoord(queryBounds.max, world.collisionChunkSize);
    std::unordered_set<int> seen;

    for (int x = c0.x; x <= c1.x; ++x)
    for (int y = c0.y; y <= c1.y; ++y)
    for (int z = c0.z; z <= c1.z; ++z)
    {
        auto it = world.collisionChunks.find(glm::ivec3(x, y, z));
        if (it == world.collisionChunks.end())
            continue;

        for (int triIndex : it->second)
        {
            if (triIndex < 0 || triIndex >= (int)world.collisionMesh.triangles.size())
                continue;
            if (!seen.insert(triIndex).second)
                continue;

            AABB triBounds = makeTriangleAABB(world.collisionMesh.triangles[triIndex]);
            triBounds.min -= glm::vec3(expansion);
            triBounds.max += glm::vec3(expansion);
            if (overlaps(queryBounds, triBounds))
                out.push_back(triIndex);
        }
    }
}

glm::vec3 solveBatchedCorrection(
    const std::vector<RecoveryContact>& contacts,
    float slop,
    float* outMaxPenetration,
    glm::vec3* outWeightedNormal,
    glm::vec3 intendedMove,
    glm::vec3 debugPosition
) {
    const PlayerSettings& cfg = GetPlayerSettings();
    std::vector<RecoveryContact> manifold;
    for (const RecoveryContact& contact : contacts) {
        RecoveryContact merged = contact;
        bool found = false;
        for (RecoveryContact& existing : manifold) {
            float alignment = glm::dot(existing.normal, contact.normal);
            // Merge normals that are similar (seam merging: dot > 0.95)
            if (alignment >= 0.95f) {
                existing.normal = glm::normalize(existing.normal + contact.normal);
                existing.point = (existing.point + contact.point) * 0.5f;
                existing.penetration = std::max(existing.penetration, contact.penetration);
                found = true;
                break;
            }
        }
        if (!found)
            manifold.push_back(merged);
    }

    // Sort by penetration depth (deepest first)
    std::sort(manifold.begin(), manifold.end(),
        [](const RecoveryContact& a, const RecoveryContact& b) {
            return a.penetration > b.penetration;
        });

    // Seam filtering: prefer contacts opposing movement, discard shallow seams
    if (glm::dot(intendedMove, intendedMove) > 0.000001f && manifold.size() > 1) {
        glm::vec3 moveDir = glm::normalize(intendedMove);
        size_t preferred = 0;
        float preferredScore = -std::numeric_limits<float>::max();
        for (size_t i = 0; i < manifold.size(); ++i) {
            float blocks = std::max(0.0f, -glm::dot(moveDir, manifold[i].normal));
            float score = manifold[i].penetration + blocks * cfg.collisionMovementBias;
            if (score > preferredScore) {
                preferredScore = score;
                preferred = i;
            }
        }
        std::vector<RecoveryContact> filtered;
        for (size_t i = 0; i < manifold.size(); ++i) {
            bool shallowSeam = i != preferred &&
                manifold[i].penetration <= slop &&
                std::fabs(manifold[i].normal.z) < 0.45f;
            if (shallowSeam) {
                // Keep shallow seams as discarded list for debug
            } else {
                filtered.push_back(manifold[i]);
            }
        }
        if (!filtered.empty())
            manifold.swap(filtered);
    }

    glm::vec3 correction(0.0f);
    glm::vec3 weightedNormal(0.0f);
    float maxPenetration = 0.0f;

    for (const RecoveryContact& c : manifold)
    {
        maxPenetration = std::max(maxPenetration, c.penetration);
        weightedNormal += c.normal * std::max(c.penetration + slop, slop);
    }

    // Gauss-Seidel solver with deepest contacts processed first
    constexpr int SOLVER_PASSES = 6;
    constexpr float RELAXATION = 0.8f;
    for (int pass = 0; pass < SOLVER_PASSES; ++pass)
    {
        for (const RecoveryContact& c : manifold)
        {
            float required = c.penetration + slop;
            float satisfied = glm::dot(correction, c.normal);
            if (satisfied < required)
                correction += c.normal * (required - satisfied) * RELAXATION;
        }
    }

    // Clamp per-axis correction to prevent one contact from dominating
    constexpr float MAX_AXIS_CORRECTION = 0.5f;
    correction.x = glm::clamp(correction.x, -MAX_AXIS_CORRECTION, MAX_AXIS_CORRECTION);
    correction.y = glm::clamp(correction.y, -MAX_AXIS_CORRECTION, MAX_AXIS_CORRECTION);
    correction.z = glm::clamp(correction.z, -MAX_AXIS_CORRECTION, MAX_AXIS_CORRECTION);

    if (outMaxPenetration)
        *outMaxPenetration = maxPenetration;
    if (outWeightedNormal)
        *outWeightedNormal = weightedNormal;

    return correction;
}


void applyTouchResets(Player& p)
{
    p.airJumpsLeft = AIR_JUMPS_MAX;
    p.dashAvailable = true;
    p.groundReturnAvailable = true;
    p.downDashAvailable = true;
    p.freezeAvailable = true;
    if (DebugConfig::DEBUG_PHYSICS)
        Debug::logThrottled(Debug::Category::Physics, "touchreset", 0.25f,
            "[TOUCH RESET] airJumps=%d dash=%d groundReturn=%d freeze=%d\n",
            p.airJumpsLeft, (int)p.dashAvailable, (int)p.groundReturnAvailable, (int)p.freezeAvailable);
}



// =====================================================
// PUBLIC ENTRY
// =====================================================

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
        "  wasOnGround=%d didLand=%d jumpConsumed=%d\n"
        "  landingCooldown=%.3f worldContact=%.4f\n"
        "  finalPos=(%.2f %.2f %.2f) floorSnapDist=0.25f\n"
        "  frameCandidates=%d/%d sweepHits=%d recovery=%d final=%d maxPen=%.4f\n",
        p.pos.x, p.pos.y, p.pos.z,
        p.vel.x, p.vel.y, p.vel.z,
        (int)p.onGround, (int)p.stableOnGround, p.groundLostTimer, p.airborneTimer,
        (int)p.wasOnGround, (int)p.didLand, (int)p.jumpConsumed,
        p.landingCooldown, p.worldContactLostTimer,
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

    p.collisionBounceCooldown = std::max(0.0f, p.collisionBounceCooldown - dt);
    if (!world.collisionMesh.empty())
    {
        doGLBTriangleCollisions(p, world, groundedThisFrame, dt);
        recoverInvalidPlayerCollisionState(p, frameStart, "glb");

        if (DebugConfig::DEBUG_COLLISION_SYSTEM) {
            // Report contact count and max penetration for the frame
            Capsule debugCap = p.getCapsule();
            std::vector<int> debugCands = gatherGLBTriangles(world, debugCap, glm::vec3(0.0f));
            std::vector<glm::vec3> debugSamps = collectPlayerBodyCollisionSamples(p);
            std::vector<RecoveryContact> reportContacts = collectGLBRecoveryContacts(
                world, debugCap, debugSamps, debugCands, BODY_SAMPLE_RADIUS);
            float maxPen = 0.0f;
            for (const auto& rc : reportContacts)
                maxPen = std::max(maxPen, rc.penetration);
            // TODO(debug): migrate to Debug::log(Debug::Category::Collision)
            printf("[COLLISION] contacts=%zu penetration=%.4f\n",
                   reportContacts.size(), maxPen);
        }

        return;
    }

    // do not set grounded here
    // so that we can actually jump 
    // groundedThisFrame = false;

    // glm::vec3 move = p.vel * dt;

    // CHANGED: No dashVel, jun 6 2026
    glm::vec3 move = (p.vel + p.externalImpulse) * dt;

    Capsule cap = p.getCapsule();

    // gather blocks near start and end of this move
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

    // sweep + slide iterations
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

        // move to first hit point (or full move if none)
        DebugVis::recordMovement(p.pos, move, "block-substep-move");
        DebugVis::recordSweep(cap.a, cap.a + move, "block-capsule-bottom");
        DebugVis::recordSweep((cap.a + cap.b) * 0.5f, (cap.a + cap.b) * 0.5f + move, "block-capsule-mid");
        DebugVis::recordSweep(cap.b, cap.b + move, "block-capsule-top");
        glm::vec3 stepMove = move * earliest;
        p.pos += stepMove;
        cap = p.getCapsule();
        
        // if nothing hit, done
        if (earliest >= 1.0f)
            break;

        // remaining movement after reaching hit point
        move -= stepMove;
        DebugVis::recordHit(p.pos, hitNormal, -1, "block-sweep");

        // idk if we put this here or where wherever idk mar 7 2026 
        // ==========================
        // REAL STEP-UP TEST
        // only climb if obstacle top is close enough to feet
        // and there is room above

        // ==========================
        // --------------------------------------------------
        // TOUCH OBJECT RESET
        // touching any solid surface restores abilities
        // --------------------------------------------------
        if (std::fabs(hitNormal.z) < 0.2f)
        {
            p.realWorldContactThisFrame = true;

            // reset jump (wall contact)
            p.airJumpsLeft = AIR_JUMPS_MAX;

            // reset dash
            p.dashAvailable = true;

            // future abilities (enable later)
            p.groundReturnAvailable = true;
            p.freezeAvailable = true;

            // continue normal step up logic 
            float feetZ = cap.a.z - cap.r;

            Block* hitBlock = nullptr;
            float bestT = earliest;

            // find which block we actually hit
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

                // only step if obstacle top is above feet but not too high
                if (stepHeight > 0.0f && stepHeight <= MAX_STEP_HEIGHT)
                {
                    glm::vec3 originalPos = p.pos;

                    // move feet to just above block top
                    p.pos.z += stepHeight + 0.001f;

                    Capsule stepCap = p.getCapsule();

                    bool blocked = false;

                    // check headroom / room after stepping
                    for (Block* b : nearbyBlocks)
                    {
                        if (!b || b->isSlope)
                            continue;

                        AABB ba = makeBlockAABB(*b);

                        glm::vec3 corr;
                        bool g = false;

                        if (capsuleVsBlock(stepCap, ba, corr, g))
                        {
                            // ignore the step surface itself if we're merely standing on it
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

        // end step up logic

        // grounding / ceiling handling
        if (hitNormal.z > 0.0f)
        {
            groundedThisFrame = true;
            p.realWorldContactThisFrame = true;

            if (p.vel.z <= 0.0f)
            {
                // --------------------------------------------------
                // TOUCH OBJECT RESET (ground contact)
                // --------------------------------------------------

                p.airJumpsLeft = AIR_JUMPS_MAX;
                p.dashAvailable = true;

                // future abilities
                p.groundReturnAvailable = true;
                p.freezeAvailable = true;

                p.vel.z = 0.0f;
            }
            DebugVis::recordGroundNormal(p.pos, hitNormal, "block-ground");
        }
        else if (hitNormal.z < 0.0f)
        {
            if (p.vel.z > 0.0f)
                p.vel.z = 0.0f;
        }

        // slide remaining move along the surface
        float vn = glm::dot(move, hitNormal);
        if (vn < 0.0f)
            move -= hitNormal * vn;

        clampVelocityAgainstNormal(p, hitNormal);

        // tiny nudge out of the surface to avoid re-hitting the exact same plane
        glm::vec3 nudge = hitNormal * 0.002f;
        p.pos += nudge;
        DebugVis::recordDepenetration(sweepStart + stepMove, nudge, "block-sweep-margin");
    }

    // Iterative depenetration for overlap recovery (batched solver)
    constexpr float BLOCK_DEPEN_SLOP = 0.002f;
    constexpr float BLOCK_MAX_CORRECTION = 2.0f;
    constexpr int MAX_BLOCK_RECOVERY_ITERATIONS = 6;
    for (int recoverIter = 0; recoverIter < MAX_BLOCK_RECOVERY_ITERATIONS; ++recoverIter)
    {
        cap = p.getCapsule();
        std::vector<RecoveryContact> contacts = collectBlockContactsForCapsule(cap, nearbyBlocks);

        if (contacts.empty())
            break;

        // Use batched correction instead of sequential pushes
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
                // CHANGED: No dashVel, jun 6 2026
                p.vel = glm::vec3(0.0f);
                DebugVis::recordDepenetration(before, escape, "block-overlap-escape");
                PHYS_LOG("[PHYS][BLOCK ESCAPE] contacts=%zu correction=(%.4f %.4f %.4f)\n",
                         remaining.size(), escape.x, escape.y, escape.z);
            }
        }
    }

    // Ground snap for blocks: if player is very close to block top and not jumping
    // upward, snap to ground. Same approach as GLB snap — the velocity check only
    // blocks snapping while jumping upward.
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
                
                // Check horizontal overlap
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
                    // Ground snap moves the player down but does NOT set grounded
                    // or call applyTouchResets — only actual collision contact
                    // may prove the capsule touched real world geometry.
                    
                    if (p.vel.z < 0.0f)
                        p.vel.z = 0.0f;
                    
                    PHYS_LOG("[PHYS][GROUND SNAP] snapped %.4f to block ground at %.2f\n", snapAmount, bestGroundZ);

                    // SAFETY: After ground snap, re-check for wall penetration
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

    // Final velocity projection against all block contacts (skip walkable ground only)
    cap = p.getCapsule();
    std::vector<RecoveryContact> blockContacts = collectBlockContactsForCapsule(cap, nearbyBlocks);
    for (const RecoveryContact& c : blockContacts)
    {
        if (c.normal.z > MAX_WALKABLE_SLOPE_DOT)
            continue;

        glm::vec3 beforeVel = p.vel;
        projectVelocityAgainstNormal(p, c.normal);

        if (DebugConfig::DEBUG_COLLISION_SYSTEM && glm::length(beforeVel - p.vel) > 0.01f) {
            // TODO(debug): migrate to Debug::log(Debug::Category::Collision)
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

// =====================================================
// BODY-PART COLLISION RESOLUTION
// =====================================================
// Called once per frame (not per substep) after the movement capsule
// collision resolves. Pushes body colliders back from world geometry
// and nudges the root capsule by 30% of the push magnitude.
// This prevents limbs visually passing through walls without making
// body collision the primary locomotion driver.




