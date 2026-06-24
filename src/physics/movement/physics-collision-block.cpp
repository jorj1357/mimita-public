#include "physics/movement/physics-collision-shared.h"

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

static inline AABB makeBlockAABB(const Block& b)
{
    glm::vec3 half = b.size * 0.5f;
    return { b.pos - half, b.pos + half };
}

static inline Capsule translatedCapsule(const Capsule& cap, const glm::vec3& delta)
{
    Capsule out = cap;
    out.a += delta;
    out.b += delta;
    return out;
}

// =====================================================
// Swept AABB
// move = displacement for THIS STEP, not raw velocity
// =====================================================

static bool sweptAABB(
    const AABB& moving,
    const glm::vec3& move,
    const AABB& block,
    float& hitTime,
    glm::vec3& hitNormal
)
{
    glm::vec3 invEntry;
    glm::vec3 invExit;

    if (move.x > 0.0f) {
        invEntry.x = block.min.x - moving.max.x;
        invExit.x  = block.max.x - moving.min.x;
    } else {
        invEntry.x = block.max.x - moving.min.x;
        invExit.x  = block.min.x - moving.max.x;
    }

    if (move.y > 0.0f) {
        invEntry.y = block.min.y - moving.max.y;
        invExit.y  = block.max.y - moving.min.y;
    } else {
        invEntry.y = block.max.y - moving.min.y;
        invExit.y  = block.min.y - moving.max.y;
    }

    if (move.z > 0.0f) {
        invEntry.z = block.min.z - moving.max.z;
        invExit.z  = block.max.z - moving.min.z;
    } else {
        invEntry.z = block.max.z - moving.min.z;
        invExit.z  = block.min.z - moving.max.z;
    }

    glm::vec3 entry;
    glm::vec3 exit;

    entry.x = (std::fabs(move.x) < ALMOST_ZERO) ? -INFINITY : invEntry.x / move.x;
    exit.x  = (std::fabs(move.x) < ALMOST_ZERO) ?  INFINITY : invExit.x  / move.x;

    entry.y = (std::fabs(move.y) < ALMOST_ZERO) ? -INFINITY : invEntry.y / move.y;
    exit.y  = (std::fabs(move.y) < ALMOST_ZERO) ?  INFINITY : invExit.y  / move.y;

    entry.z = (std::fabs(move.z) < ALMOST_ZERO) ? -INFINITY : invEntry.z / move.z;
    exit.z  = (std::fabs(move.z) < ALMOST_ZERO) ?  INFINITY : invExit.z  / move.z;

    float entryTime = std::max(std::max(entry.x, entry.y), entry.z);
    float exitTime  = std::min(std::min(exit.x,  exit.y),  exit.z);

    if (entryTime > exitTime || entryTime < 0.0f || entryTime > 1.0f)
        return false;

    hitTime = entryTime;

    if (entryTime == entry.x)
        hitNormal = { move.x > 0.0f ? -1.0f : 1.0f, 0.0f, 0.0f };
    else if (entryTime == entry.y)
        hitNormal = { 0.0f, move.y > 0.0f ? -1.0f : 1.0f, 0.0f };
    else
        hitNormal = { 0.0f, 0.0f, move.z > 0.0f ? -1.0f : 1.0f };

    return true;
}

// we are A CAPSULE NOW MAR 7 2026 NOT A BOX

bool capsuleVsBlock(
    const Capsule& cap,
    const AABB& block,
    glm::vec3& correction,
    bool& grounded
)
{
    glm::vec3 testPoints[2] = { cap.a, cap.b };

    for (int i = 0; i < 2; i++)
    {
        glm::vec3 p = testPoints[i];

        glm::vec3 closest;

        closest.x = glm::clamp(p.x, block.min.x, block.max.x);
        closest.y = glm::clamp(p.y, block.min.y, block.max.y);
        closest.z = glm::clamp(p.z, block.min.z, block.max.z);

        glm::vec3 delta = p - closest;

        float dist2 = glm::dot(delta, delta);
        float r = cap.r;

        if (dist2 > r*r)
            continue;

        float dist = sqrtf(dist2);

        glm::vec3 normal;

        if (dist > 0.00001f)
            normal = delta / dist;
        else
            normal = {0,0,1};

        constexpr float PUSH_OUT_MARGIN = 0.002f;
        float penetration = r - dist;

        correction = normal * (penetration + PUSH_OUT_MARGIN);

        if (normal.z >= MAX_WALKABLE_SLOPE_DOT)
            grounded = true;

        return true;
    }

    return false;
}

// Capsule vs Capsule collision - for NPC vs Player
static bool capsuleVsCapsule(
    const Capsule& capA,
    const Capsule& capB,
    glm::vec3& correction,
    bool& groundedA
)
{
    float aMinZ = std::min(capA.a.z, capA.b.z) - capA.r;
    float aMaxZ = std::max(capA.a.z, capA.b.z) + capA.r;
    float bMinZ = std::min(capB.a.z, capB.b.z) - capB.r;
    float bMaxZ = std::max(capB.a.z, capB.b.z) + capB.r;
    if (aMaxZ <= bMinZ || bMaxZ <= aMinZ)
        return false;

    glm::vec2 centerA = glm::vec2(capA.a.x + capA.b.x, capA.a.y + capA.b.y) * 0.5f;
    glm::vec2 centerB = glm::vec2(capB.a.x + capB.b.x, capB.a.y + capB.b.y) * 0.5f;
    glm::vec2 delta = centerA - centerB;
    float dist = glm::length(delta);
    float combinedR = capA.r + capB.r;
    if (dist >= combinedR)
        return false;

    glm::vec2 planarNormal =
        dist > 0.00001f
        ? delta / dist
        : glm::vec2(1.0f, 0.0f);

    constexpr float PUSH_OUT_MARGIN = 0.002f;
    float penetration = combinedR - dist;
    correction = glm::vec3(planarNormal * (penetration + PUSH_OUT_MARGIN), 0.0f);
    groundedA = false;

    return true;
}

static bool sphereAABBContact(
    glm::vec3 center,
    float radius,
    const AABB& block,
    RecoveryContact& out,
    const Block* sourceBlock,
    const char* label
) {
    glm::vec3 closest;
    closest.x = glm::clamp(center.x, block.min.x, block.max.x);
    closest.y = glm::clamp(center.y, block.min.y, block.max.y);
    closest.z = glm::clamp(center.z, block.min.z, block.max.z);

    glm::vec3 delta = center - closest;
    float dist2 = glm::dot(delta, delta);

    if (dist2 > radius * radius)
        return false;

    glm::vec3 normal(0.0f, 0.0f, 1.0f);
    float penetration = 0.0f;

    if (dist2 > 0.00000001f)
    {
        float dist = sqrtf(dist2);
        normal = delta / dist;
        penetration = radius - dist;
    }
    else
    {
        float dNegX = center.x - block.min.x;
        float dPosX = block.max.x - center.x;
        float dNegY = center.y - block.min.y;
        float dPosY = block.max.y - center.y;
        float dNegZ = center.z - block.min.z;
        float dPosZ = block.max.z - center.z;

        float best = dNegX;
        normal = {-1.0f, 0.0f, 0.0f};
        if (dPosX < best) { best = dPosX; normal = { 1.0f, 0.0f, 0.0f}; }
        if (dNegY < best) { best = dNegY; normal = {0.0f, -1.0f, 0.0f}; }
        if (dPosY < best) { best = dPosY; normal = {0.0f,  1.0f, 0.0f}; }
        if (dNegZ < best) { best = dNegZ; normal = {0.0f, 0.0f, -1.0f}; }
        if (dPosZ < best) { best = dPosZ; normal = {0.0f, 0.0f,  1.0f}; }

        penetration = radius + std::max(best, 0.0f);
        closest = center - normal * std::max(best, 0.0f);
    }

    out.normal = normal;
    out.point = closest;
    out.penetration = std::max(penetration, 0.0f);
    out.block = sourceBlock;
    out.label = label;
    return true;
}

bool rejectBelowBlockTopContact(
    const Capsule& cap,
    const AABB& block,
    const RecoveryContact& contact)
{
    if (contact.normal.z <= MAX_WALKABLE_SLOPE_DOT)
        return false;

    const float feetZ = cap.a.z - cap.r;
    const float tolerance = std::max(
        GetPlayerSettings().collisionSeamTolerance,
        COLLISION_SKIN + 0.005f);
    if (feetZ + tolerance >= block.max.z)
        return false;

    Debug::logThrottled(Debug::Category::Collision, "seam-filter-block-below-top",
        DebugConfig::PRINT_INTERVAL,
        "[SEAM FILTER] ignored top contact reason=below_top_face phase=block-recovery contactZ=%.3f feetZ=%.3f triMaxZ=%.3f\n",
        contact.point.z, feetZ, block.max.z);
    return true;
}

// this is for capsule stuff but idk whre to put it mar 7 2026
bool capsuleSweep(
    const Capsule& cap,
    const glm::vec3& move,
    const AABB& block,
    float& hitTime,
    glm::vec3& hitNormal
)
{
    AABB expanded;
    expanded.min = block.min - glm::vec3(cap.r);
    expanded.max = block.max + glm::vec3(cap.r);

    glm::vec3 center = (cap.a + cap.b) * 0.5f;

    AABB pointBox;
    pointBox.min = center;
    pointBox.max = center;

    return sweptAABB(pointBox, move, expanded, hitTime, hitNormal);
}

static void addCandidateDirection(std::vector<glm::vec3>& dirs, glm::vec3 dir)
{
    float len2 = glm::dot(dir, dir);
    if (len2 < 0.000001f)
        return;

    dir /= sqrtf(len2);
    for (glm::vec3 existing : dirs)
        if (glm::dot(existing, dir) > 0.98f)
            return;

    dirs.push_back(dir);
}

static bool capsuleHasBlockContactsAfterMove(
    const Capsule& cap,
    const std::vector<Block*>& nearbyBlocks,
    const glm::vec3& correction
) {
    return !collectBlockContactsForCapsule(translatedCapsule(cap, correction), nearbyBlocks).empty();
}

std::vector<RecoveryContact> collectBlockContactsForCapsule(
    const Capsule& cap,
    const std::vector<Block*>& nearbyBlocks
) {
    std::vector<RecoveryContact> contacts;
    constexpr int SAMPLE_COUNT = 5;

    for (Block* b : nearbyBlocks)
    {
        if (!b || b->isSlope)
            continue;

        AABB ba = makeBlockAABB(*b);
        for (int i = 0; i < SAMPLE_COUNT; ++i)
        {
            float t = (SAMPLE_COUNT == 1) ? 0.0f : (float)i / (float)(SAMPLE_COUNT - 1);
            glm::vec3 sample = cap.a + (cap.b - cap.a) * t;
            RecoveryContact c;
            if (sphereAABBContact(sample, cap.r, ba, c, b, "block-overlap") &&
                !rejectBelowBlockTopContact(cap, ba, c))
                contacts.push_back(c);
        }
    }

    return contacts;
}

bool findBlockFallbackEscape(
    const Capsule& cap,
    const std::vector<Block*>& nearbyBlocks,
    const std::vector<RecoveryContact>& contacts,
    const glm::vec3& weightedNormal,
    glm::vec3& outCorrection
) {
    std::vector<glm::vec3> dirs;
    addCandidateDirection(dirs, weightedNormal);
    addCandidateDirection(dirs, { 1.0f, 0.0f, 0.0f});
    addCandidateDirection(dirs, {-1.0f, 0.0f, 0.0f});
    addCandidateDirection(dirs, {0.0f,  1.0f, 0.0f});
    addCandidateDirection(dirs, {0.0f, -1.0f, 0.0f});
    addCandidateDirection(dirs, {0.0f, 0.0f,  1.0f});
    addCandidateDirection(dirs, {0.0f, 0.0f, -1.0f});

    for (const RecoveryContact& c : contacts)
        addCandidateDirection(dirs, c.normal);

    float maxBlockExtent = 1.0f;
    for (Block* b : nearbyBlocks)
        if (b && !b->isSlope)
            maxBlockExtent = std::max(maxBlockExtent, std::max(std::max(b->size.x, b->size.y), b->size.z));

    const float maxEscape = maxBlockExtent + PLAYER_HEIGHT + PLAYER_RADIUS * 4.0f;
    bool found = false;
    float bestLen = std::numeric_limits<float>::max();
    glm::vec3 best(0.0f);

    for (glm::vec3 dir : dirs)
    {
        float low = 0.0f;
        float high = PLAYER_RADIUS * 0.25f + 0.01f;
        while (high <= maxEscape && capsuleHasBlockContactsAfterMove(cap, nearbyBlocks, dir * high))
            high *= 2.0f;

        if (high > maxEscape)
            continue;

        for (int i = 0; i < 12; ++i)
        {
            float mid = (low + high) * 0.5f;
            if (capsuleHasBlockContactsAfterMove(cap, nearbyBlocks, dir * mid))
                low = mid;
            else
                high = mid;
        }

        if (high < bestLen)
        {
            bestLen = high;
            best = dir * (high + 0.002f);
            found = true;
        }
    }

    if (!found)
        return false;

    outCorrection = best;
    return true;
}

// Resolve collision between two capsules (e.g., player vs NPC)
bool resolveCapsuleVsCapsule(
    Player& a,
    Player& b,
    bool& groundedA,
    bool& groundedB
)
{
    Capsule capA = a.getCapsule();
    Capsule capB = b.getCapsule();

    glm::vec3 correctionA;
    bool grounded = false;
    if (!capsuleVsCapsule(capA, capB, correctionA, grounded))
        return false;

    // Split correction between both bodies (mass-based or 50/50)
    // For now, push each away from each other equally
    glm::vec3 correctionB = -correctionA;

    constexpr float SPLIT_RATIO = 0.5f;
    a.pos += correctionA * SPLIT_RATIO;
    b.pos += correctionB * SPLIT_RATIO;

    groundedA = grounded;
    groundedB = grounded;

    // Clamp velocities against collision normal
    glm::vec3 normal = glm::normalize(correctionA);
    clampVelocityAgainstNormal(a, normal);
    clampVelocityAgainstNormal(b, -normal);

    return true;
}
