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
#include "devtools/terminal.h"
#include "perf/perf.h"
#include "physics/movement/physics-collision.h"

// =====================================================
// DEBUG TOGGLE
// =====================================================
#define PHYS_LOG(...) Debug::logThrottled(Debug::Category::Collision, "physics-collision", DebugConfig::PRINT_INTERVAL, __VA_ARGS__)

static bool capsuleTriangleSweep(
    const Capsule& cap,
    const glm::vec3& move,
    const CollisionTriangle& tri,
    int triIndex,
    SweepHit& out
);

static bool capsuleTriangleContact(
    const Capsule& cap,
    const CollisionTriangle& tri,
    int triIndex,
    Contact& out
);

bool sweepSphereTriangle(
    glm::vec3 start,
    glm::vec3 move,
    float radius,
    const CollisionTriangle& tri,
    float& hitTime,
    glm::vec3& hitNormal,
    glm::vec3& hitPoint
);

static bool sphereTriangleContact(
    glm::vec3 center,
    float radius,
    const CollisionTriangle& tri,
    Contact& contact
);

static bool pointInTriangle(glm::vec3 p, const CollisionTriangle& tri);

bool sweepSphereEdge(
    glm::vec3 start,
    glm::vec3 move,
    float radius,
    glm::vec3 edgeA,
    glm::vec3 edgeB,
    float& hitTime,
    glm::vec3& hitNormal,
    glm::vec3& hitPoint
);

static std::vector<int> gatherGLBTriangles(
    const World& world,
    const Capsule& cap,
    const glm::vec3& move
);

static std::vector<int> gatherGLBTrianglesForSphere(
    const World& world,
    glm::vec3 center,
    float radius,
    const glm::vec3& move
);

static std::vector<glm::vec3> collectPlayerBodyCollisionSamples(Player& p);
static void applyTouchResets(Player& p);
static glm::vec3 closestPointOnTriangle(glm::vec3 p, glm::vec3 a, glm::vec3 b, glm::vec3 c);
static const char* triangleFeatureLabel(const CollisionTriangle& tri, const glm::vec3& point);

struct CollisionTraceSnapshot
{
    glm::vec3 startPos{0.0f};
    glm::vec3 finalPos{0.0f};
    glm::vec3 inputMove{0.0f};
    int initialCandidates = 0;
    int maxCandidates = 0;
    int sweepIterations = 0;
    int sweepHits = 0;
    int maxSimultaneousTOI = 0;
    int maxSlideContacts = 0;
    int maxRecoveryContacts = 0;
    int finalContacts = 0;
    int finalSafetyContacts = 0;
    int resweepHits = 0;
    int faceHits = 0;
    int edgeHits = 0;
    int vertexHits = 0;
    float maxPenetration = 0.0f;
    bool emergencyEscaped = false;
};

static CollisionTraceSnapshot gLastCollisionTrace;

static void appendUniqueTriangleIndices(std::vector<int>& dst, const std::vector<int>& src)
{
    for (int triIndex : src)
    {
        if (std::find(dst.begin(), dst.end(), triIndex) == dst.end())
            dst.push_back(triIndex);
    }
}

// Project each velocity contributor independently so wall-normal momentum
// stays removed without sacrificing tangent velocity.
static inline void projectVelocityAgainstNormal(
    Player& p,
    const glm::vec3& normal
)
{
        glm::vec3* velocities[] =
    {
        &p.vel,
        &p.externalImpulse
    };

    for (glm::vec3* v : velocities)
    {
        float into =
            glm::dot(*v, normal);

        // already moving away
        if (into >= 0.0f)
            continue;

        // remove ONLY component into wall
        *v -= normal * into;
    }
}

static inline void clampVelocityAgainstNormal(Player& p, const glm::vec3& normal)
{
    projectVelocityAgainstNormal(p, normal);
}

// CHANGED: Simplified collision contact. Walls slide instead of bounce, jun 6 2026
static inline void applyCollisionContact(
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

    p.hasWorldContact = true;
    p.worldContactLostTimer = 0.033f; // 2 frames of hysteresis for consistent wall climb

    // Ground: slope is walkable
    if (normal.z > MAX_WALKABLE_SLOPE_DOT)
    {
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

// =====================================================
// AABB helpers (TEMP until capsule collisions v3)
// =====================================================

static inline AABB makeSweptCapsuleAABB(const Capsule& cap, const glm::vec3& move)
{
    glm::vec3 mn = glm::min(glm::min(cap.a, cap.b), glm::min(cap.a + move, cap.b + move));
    glm::vec3 mx = glm::max(glm::max(cap.a, cap.b), glm::max(cap.a + move, cap.b + move));
    return {mn - glm::vec3(cap.r), mx + glm::vec3(cap.r)};
}

static inline AABB makeTriangleAABB(const CollisionTriangle& tri)
{
    return {
        glm::min(glm::min(tri.a, tri.b), tri.c),
        glm::max(glm::max(tri.a, tri.b), tri.c)
    };
}

static inline AABB makePlayerAABB(const Player& p)
{
    glm::vec3 half(
        PLAYER_WIDTH  * 0.5f,
        PLAYER_DEPTH  * 0.5f,
        PLAYER_HEIGHT * 0.5f
    );

    return { p.pos - half, p.pos + half };
}

static inline AABB makeBlockAABB(const Block& b)
{
    glm::vec3 half = b.size * 0.5f;
    return { b.pos - half, b.pos + half };
}

static inline bool overlaps(const AABB& a, const AABB& b)
{
    return (a.min.x <= b.max.x && a.max.x >= b.min.x) &&
           (a.min.y <= b.max.y && a.max.y >= b.min.y) &&
           (a.min.z <= b.max.z && a.max.z >= b.min.z);
}

static bool rejectBelowTopFaceContact(
    const Capsule& cap,
    const CollisionTriangle& tri,
    const glm::vec3& normal,
    const glm::vec3& point,
    int triangleIndex,
    const char* phase)
{
    if (normal.z <= MAX_WALKABLE_SLOPE_DOT)
        return false;

    const float feetZ = cap.a.z - cap.r;
    const float tolerance = std::max(
        GetPlayerSettings().collisionSeamTolerance,
        COLLISION_SKIN + 0.005f);
    if (feetZ + tolerance >= point.z)
        return false;

    const float triMaxZ = std::max({tri.a.z, tri.b.z, tri.c.z});
    Debug::logThrottled(Debug::Category::Collision, "seam-filter-below-top",
        DebugConfig::PRINT_INTERVAL,
        "[SEAM FILTER] ignored top contact reason=below_top_face phase=%s tri=%d contactZ=%.3f feetZ=%.3f triMaxZ=%.3f\n",
        phase ? phase : "unknown", triangleIndex, point.z, feetZ, triMaxZ);
    return true;
}

static bool rejectBelowBlockTopContact(
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

static inline glm::ivec3 collisionChunkCoord(const glm::vec3& p, float size)
{
    return glm::ivec3(
        (int)std::floor(p.x / size),
        (int)std::floor(p.y / size),
        (int)std::floor(p.z / size)
    );
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

static bool capsuleVsBlock(
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

static inline Capsule translatedCapsule(const Capsule& cap, const glm::vec3& delta)
{
    Capsule out = cap;
    out.a += delta;
    out.b += delta;
    return out;
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

static std::vector<RecoveryContact> collectBlockContactsForCapsule(
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

static std::vector<RecoveryContact> collectCapsuleRecoveryContacts(
    const World& world,
    const Capsule& cap,
    const std::vector<int>& candidates
) {
    std::vector<RecoveryContact> contacts;
    for (int triIndex : candidates)
    {
        if (triIndex < 0 || triIndex >= (int)world.collisionMesh.triangles.size())
            continue;
        Contact contact;
        if (capsuleTriangleContact(cap, world.collisionMesh.triangles[triIndex], triIndex, contact))
        {
            const CollisionTriangle& tri = world.collisionMesh.triangles[triIndex];
            if (rejectBelowTopFaceContact(cap, tri, contact.normal, contact.point, triIndex, "recovery"))
                continue;
            contacts.push_back({
                contact.normal,
                contact.point,
                contact.penetration,
                contact.triangleIndex,
                nullptr,
                "capsule-triangle"
            });
        }
    }
    return contacts;
}

static std::vector<RecoveryContact> collectGLBRecoveryContacts(
    const World& world,
    const Capsule& cap,
    const std::vector<glm::vec3>& bodySamples,
    const std::vector<int>& candidates,
    float bodySampleRadius
) {
    std::vector<RecoveryContact> contacts;

    for (int triIndex : candidates)
    {
        if (triIndex < 0 || triIndex >= (int)world.collisionMesh.triangles.size())
            continue;

        Contact contact;
        if (capsuleTriangleContact(cap, world.collisionMesh.triangles[triIndex], triIndex, contact))
        {
            const CollisionTriangle& tri = world.collisionMesh.triangles[triIndex];
            if (rejectBelowTopFaceContact(cap, tri, contact.normal, contact.point, triIndex, "glb-recovery"))
                continue;
            contacts.push_back({
                contact.normal,
                contact.point,
                contact.penetration,
                contact.triangleIndex,
                nullptr,
                "capsule-triangle"
            });
        }
    }

    for (glm::vec3 sample : bodySamples)
    {
        for (int triIndex : candidates)
        {
            if (triIndex < 0 || triIndex >= (int)world.collisionMesh.triangles.size())
                continue;

            Contact contact;
            if (!sphereTriangleContact(sample, bodySampleRadius, world.collisionMesh.triangles[triIndex], contact))
                continue;

            contact.triangleIndex = triIndex;
            contacts.push_back({
                contact.normal,
                contact.point,
                contact.penetration,
                contact.triangleIndex,
                nullptr,
                "body-triangle"
            });
        }
    }

    return contacts;
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

static bool capsuleHasBlockContactsAfterMove(
    const Capsule& cap,
    const std::vector<Block*>& nearbyBlocks,
    const glm::vec3& correction
) {
    return !collectBlockContactsForCapsule(translatedCapsule(cap, correction), nearbyBlocks).empty();
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

static bool findBlockFallbackEscape(
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

// this is for capsule stuff but idk whre to put it mar 7 2026 
static bool capsuleSweep(
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

static void applyTouchResets(Player& p)
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

static void doGLBTriangleCollisions(
    Player& p,
    const World& world,
    bool& groundedThisFrame,
    float dt
) {
    constexpr float SURFACE_SLOP = 0.01f;
    constexpr float MAX_CORRECTION = 2.0f;

    // CHANGED: No dashVel — dash is now in vel, jun 6 2026
    glm::vec3 totalMove = (p.vel + p.externalImpulse) * dt;
    p.updateModelWorldTransforms();
    Capsule cap = p.getCapsule();

    std::vector<int> candidates = gatherGLBTriangles(world, cap, totalMove);
    std::vector<int> bodyCandidateExtras;
    std::vector<glm::vec3> bodySamples = collectPlayerBodyCollisionSamples(p);

    // Compute per-sample limb motion deltas for sweep collision
    // delta = current sample position - previous frame sample position
    // This captures animation-driven limb motion between frames
    std::vector<glm::vec3> bodyDeltas(bodySamples.size(), glm::vec3(0.0f));
    for (size_t si = 0; si < bodySamples.size() && si < p.previousBodySamplePositions.size(); ++si)
        bodyDeltas[si] = bodySamples[si] - p.previousBodySamplePositions[si];

    // Gather candidate triangles using the swept volume (current position + player move + limb delta)
    for (size_t si = 0; si < bodySamples.size(); ++si)
    {
        glm::vec3 sample = bodySamples[si];
        glm::vec3 sampleMove = totalMove + bodyDeltas[si];
        std::vector<int> sampleCandidates = gatherGLBTrianglesForSphere(world, sample, BODY_SAMPLE_RADIUS, sampleMove);
        appendUniqueTriangleIndices(bodyCandidateExtras, sampleCandidates);
    }
    appendUniqueTriangleIndices(candidates, bodyCandidateExtras);

    // Save current samples as previous for next frame
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

    // Phase 1: Sweep + slide movement (5 iterations)
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

        // p.pos += earliest.normal * SURFACE_SLOP;
        // =====================================================
        // GLB STEP UP
        // =====================================================

        if (std::fabs(earliest.normal.z) < 0.2f)
        {
            float feetZ = cap.a.z - cap.r;

            float stepTopZ = earliest.point.z;
            float stepHeight = stepTopZ - feetZ;

            if (stepHeight > 0.0f &&
                stepHeight <= MAX_STEP_HEIGHT)
            {
                glm::vec3 originalPos = p.pos;

                // Validate step-up: check that at least 3 of 5 capsule
                // sample points agree on the step height. This prevents
                // stepping on micro-edges or single-triangle spikes.
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

                // lift player upward
                p.pos.z += stepHeight + 0.01f;

                p.updateModelWorldTransforms();

                Capsule stepCap = p.getCapsule();

                bool blocked = false;

                // recheck nearby triangles for head collision
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
                        // ignore floor-like contacts
                        if (c.normal.z < 0.5f)
                        {
                            blocked = true;
                            break;
                        }
                    }
                }

                if (!blocked)
                {
                    // Floor validation: after lifting, verify there is
                    // walkable floor below by checking ground snap range.
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

                    if (p.vel.z < 0.0f)
                        p.vel.z = 0.0f;

                    // Reduce remaining move so the player does not
                    // accumulate extra distance across sweep iterations.
                    remainingMove -= stepMove;

                    PHYS_LOG(
                        "[GLB STEP] stepped up %.3f remainingMove=(%.4f %.4f %.4f)\n",
                        stepHeight,
                        remainingMove.x, remainingMove.y, remainingMove.z
                    );

                    continue;
                }

                // failed
                p.pos = originalPos;
                p.updateModelWorldTransforms();
            }
        }
        glm::vec3 depen = earliest.normal;

        // prevent wedges from pushing upward
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

    // Phase 2: Gather ALL contacts at final position, depenetrate iteratively
    // Uses batched manifold solving instead of sequential per-contact pushes
    // This correctly handles contradictory normals (corners, edges, seams)
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

        // Use batched correction solver instead of sequential pushes
        // This iteratively solves all contact constraints simultaneously,
        // preventing contradictory normals from fighting each other
        float iterMaxPen = 0.0f;
        glm::vec3 correction = solveBatchedCorrection(contacts, SURFACE_SLOP, &iterMaxPen, nullptr, totalMove, p.pos);
        maxPenetrationSeen = std::max(maxPenetrationSeen, iterMaxPen);
        trace.maxPenetration = std::max(trace.maxPenetration, iterMaxPen);

        // Clamp correction magnitude to prevent explosive escapes
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

    // Phase 2.5: Re-sweep only the leftover movement after depenetration.
    // This preserves movement feel while preventing the old full-frame resweep
    // over-advance failure in acute wedges and pockets.
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

    // Ground snap: if player is very close to ground and not jumping up, snap to ground
    // This prevents hovering and flickering grounded state
    // IMPORTANT: After snapping, re-check for wall penetration
    {
        constexpr float GROUND_SNAP_DISTANCE = 0.15f;
        constexpr float MAX_UPWARD_VEL_FOR_SNAP = 0.5f;
        
        if (p.vel.z <= MAX_UPWARD_VEL_FOR_SNAP)
        {
            Capsule checkCap = p.getCapsule();
            float feetZ = checkCap.a.z - checkCap.r;
            
            // Check for ground directly below within snap distance
            std::vector<int> groundCandidates = gatherGLBTriangles(world, checkCap, {0, 0, -GROUND_SNAP_DISTANCE});
            float bestGroundZ = -FLT_MAX;
            
            for (int triIndex : groundCandidates)
            {
                const CollisionTriangle& tri = world.collisionMesh.triangles[triIndex];
                if (tri.normal.z < MAX_WALKABLE_SLOPE_DOT) continue; // Only walkable surfaces
                
                // Project capsule center onto triangle plane and test actual triangle membership
                // instead of AABB overlap, which produces false positives for off-center triangles.
                glm::vec3 capCenter(checkCap.a.x, checkCap.a.y, 0.0f);
                float planeDist = glm::dot(capCenter - tri.a, tri.normal);
                glm::vec3 proj = capCenter - tri.normal * planeDist;
                if (!pointInTriangle(proj, tri))
                {
                    // Fallback for irregular meshes: capsule center may not project
                    // onto any single triangle interior. Check nearest point on
                    // triangle edge or vertex.
                    glm::vec3 nearest = closestPointOnTriangle(capCenter, tri.a, tri.b, tri.c);
                    float dist2 = glm::dot(nearest - capCenter, nearest - capCenter);
                    if (dist2 < (PLAYER_RADIUS * 0.9f) * (PLAYER_RADIUS * 0.9f))
                    {
                        if (nearest.z < feetZ && nearest.z > bestGroundZ)
                            bestGroundZ = nearest.z;
                    }
                    continue;
                }
                
                {
                    // Triangle is under player - check Z
                    float triCenterZ = (tri.a.z + tri.b.z + tri.c.z) / 3.0f;
                    if (triCenterZ < feetZ && triCenterZ > bestGroundZ)
                        bestGroundZ = triCenterZ;
                }
            }
            
            if (bestGroundZ > -FLT_MAX)
            {
                float distToGround = feetZ - bestGroundZ;
                if (distToGround > 0.0f && distToGround < GROUND_SNAP_DISTANCE)
                {
                    float snapAmount = distToGround;
                    p.pos.z -= snapAmount;
                    groundedThisFrame = true;
                    
                    if (p.vel.z < 0.0f)
                        p.vel.z = 0.0f;
                    
                    PHYS_LOG("[PHYS][GROUND SNAP] snapped %.4f to ground at %.2f (dist=%.4f)\n", snapAmount, bestGroundZ, distToGround);
                    if (DebugConfig::DEBUG_PHYSICS)
                        Debug::log(Debug::Category::Physics, "[SNAP] distToGround=%.4f snap=%.4f grounded=%d\n",
                            distToGround, snapAmount, (int)groundedThisFrame);

                    // SAFETY: After ground snap, re-check for wall penetration
                    // Snapping upward can push player into adjacent walls
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

    // Emergency stuck prevention:
    // If deep penetration persists after all depen + snap corrections,
    // search outward for a free position to prevent permanent trapping.
    {
        constexpr float STUCK_THRESHOLD = 0.05f;
        const float EMERGENCY_SEARCH_RADIUS = PLAYER_HEIGHT + PLAYER_RADIUS * 4.0f;

        // Only check capsule contacts (the primary hitbox)
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

                // Search outward in cardinal + diagonal directions for free space
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

                        // Check capsule at test position
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

                    // CHANGED: No dashVel, jun 6 2026
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

    // Final velocity projection: slide velocity along all contact surfaces
    // Only walkable ground is skipped so horizontal movement isn't cancelled.
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
        // Skip walkable ground — these should not cancel horizontal velocity
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

    // Stuck diagnostic: detect when player has movement input but position
    // barely changes for several frames (invisible wall / seam wedging).
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

    // Rotation safety pass: after all collision resolution, verify the
    // authoritative capsule is not penetrating world geometry due to
    // body/weapon/animation/rotation updates that happened during the frame.
    // This catches cases where non-authoritative changes (weapon pose,
    // arm rotation, body turning) could have pushed through walls.
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

    // Final safety net after sweep, depen, snap, stuck escape, and rotation
    // safety. Normal cases should be clean here; any remaining penetration
    // gets one batched correction instead of relying on emergency escape.
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

    // Overlapping geometry debug detection:
    // If candidate triangles have normals pointing in strongly divergent
    // directions while being close together, log a warning for the developer.
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
                    if (dotNormals < -0.5f) // Nearly opposing normals
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
                overlapWarnCooldown = 30; // Log at most every 30 frames
            }
        }
    }

    // Body-part collision runs in physicsMainUpdate_Internal after animation.
    // (not here — moved to avoid animation overwrite)

    // collision_debug: record additional contact visualization events
    // (actual drawing happens in drawDebugStuff with the camera)
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

void doCollisions(
    Player& p,
    const World& world,
    bool& groundedThisFrame,
    float dt
)
{
    Perf::ScopedTimer _colTimer("Collision");
    p.collisionBounceCooldown = std::max(0.0f, p.collisionBounceCooldown - dt);
    if (!world.collisionMesh.empty())
    {
        doGLBTriangleCollisions(p, world, groundedThisFrame, dt);

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

    // Ground snap for blocks: if player is very close to block top and not jumping up, snap to ground
    {
        constexpr float GROUND_SNAP_DISTANCE = 0.15f;
        constexpr float MAX_UPWARD_VEL_FOR_SNAP = 0.5f;
        
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
                    groundedThisFrame = true;
                    
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
}

// =====================================================
// BODY-PART COLLISION RESOLUTION
// =====================================================
// Called once per frame (not per substep) after the movement capsule
// collision resolves. Pushes body colliders back from world geometry
// and nudges the root capsule by 30% of the push magnitude.
// This prevents limbs visually passing through walls without making
// body collision the primary locomotion driver.

static glm::vec3 closestPointOnTriangle(glm::vec3 p, glm::vec3 a, glm::vec3 b, glm::vec3 c)
{
    glm::vec3 ab = b - a;
    glm::vec3 ac = c - a;
    glm::vec3 ap = p - a;

    float d1 = glm::dot(ab, ap);
    float d2 = glm::dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) return a;

    glm::vec3 bp = p - b;
    float d3 = glm::dot(ab, bp);
    float d4 = glm::dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) return b;

    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
    {
        float v = d1 / (d1 - d3);
        return a + ab * v;
    }

    glm::vec3 cp = p - c;
    float d5 = glm::dot(ab, cp);
    float d6 = glm::dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) return c;

    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
    {
        float w = d2 / (d2 - d6);
        return a + ac * w;
    }

    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
    {
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return b + (c - b) * w;
    }

    float denom = 1.0f / (va + vb + vc);
    float v = vb * denom;
    float w = vc * denom;
    return a + ab * v + ac * w;
}

static float pointSegmentDistanceSq(glm::vec3 p, glm::vec3 a, glm::vec3 b)
{
    glm::vec3 ab = b - a;
    float abLenSq = glm::dot(ab, ab);
    if (abLenSq <= 0.00000001f)
        return glm::dot(p - a, p - a);

    float t = glm::clamp(glm::dot(p - a, ab) / abLenSq, 0.0f, 1.0f);
    glm::vec3 closest = a + ab * t;
    return glm::dot(p - closest, p - closest);
}

static const char* triangleFeatureLabel(const CollisionTriangle& tri, const glm::vec3& point)
{
    constexpr float FEATURE_EPS = 0.025f;
    constexpr float FEATURE_EPS_SQ = FEATURE_EPS * FEATURE_EPS;

    if (glm::dot(point - tri.a, point - tri.a) <= FEATURE_EPS_SQ ||
        glm::dot(point - tri.b, point - tri.b) <= FEATURE_EPS_SQ ||
        glm::dot(point - tri.c, point - tri.c) <= FEATURE_EPS_SQ)
        return "vertex";

    if (pointSegmentDistanceSq(point, tri.a, tri.b) <= FEATURE_EPS_SQ ||
        pointSegmentDistanceSq(point, tri.b, tri.c) <= FEATURE_EPS_SQ ||
        pointSegmentDistanceSq(point, tri.c, tri.a) <= FEATURE_EPS_SQ)
        return "edge";

    return "face";
}

static bool pointInTriangle(glm::vec3 p, const CollisionTriangle& tri)
{
    glm::vec3 closest = closestPointOnTriangle(p, tri.a, tri.b, tri.c);
    glm::vec3 delta = closest - p;
    return glm::dot(delta, delta) < 0.0001f;
}

static bool sweepSpherePoint(
    glm::vec3 start,
    glm::vec3 move,
    float radius,
    glm::vec3 point,
    float& hitTime,
    glm::vec3& hitNormal,
    glm::vec3& hitPoint
) {
    float a = glm::dot(move, move);
    if (a < ALMOST_ZERO)
        return false;

    glm::vec3 rel = start - point;
    float b = 2.0f * glm::dot(rel, move);
    float c = glm::dot(rel, rel) - radius * radius;
    float disc = b * b - 4.0f * a * c;
    if (disc < 0.0f)
        return false;

    float t = (-b - sqrtf(disc)) / (2.0f * a);
    if (t < 0.0f || t > 1.0f)
        return false;

    glm::vec3 center = start + move * t;
    glm::vec3 n = center - point;
    if (glm::dot(n, n) < 0.000001f)
        n = -glm::normalize(move);
    else
        n = glm::normalize(n);

    hitTime = t;
    hitNormal = n;
    hitPoint = point;
    return true;
}

// Swept sphere vs line segment (edge).  Prevents the sphere from
// passing between triangle vertices through the edge opening.
bool sweepSphereEdge(
    glm::vec3 start,
    glm::vec3 move,
    float radius,
    glm::vec3 edgeA,
    glm::vec3 edgeB,
    float& hitTime,
    glm::vec3& hitNormal,
    glm::vec3& hitPoint
) {
    glm::vec3 edgeDir = edgeB - edgeA;
    float edgeLen = glm::length(edgeDir);
    if (edgeLen < 0.000001f)
        return false;
    edgeDir /= edgeLen;

    // Project sphere trajectory onto the plane perpendicular to the edge.
    // The perpendicular motion determines closest approach to the edge line.
    glm::vec3 rel = start - edgeA;
    float proj = glm::dot(rel, edgeDir);
    glm::vec3 relPerp = rel - edgeDir * proj;
    glm::vec3 movePerp = move - edgeDir * glm::dot(move, edgeDir);

    float a = glm::dot(movePerp, movePerp);
    if (a < ALMOST_ZERO)
        return false;

    float b = 2.0f * glm::dot(relPerp, movePerp);
    float c = glm::dot(relPerp, relPerp) - radius * radius;

    float disc = b * b - 4.0f * a * c;
    if (disc < 0.0f)
        return false;

    float t = (-b - sqrtf(disc)) / (2.0f * a);
    if (t < 0.0f || t > 1.0f)
        return false;

    // Verify contact point is within the edge segment at hit time
    glm::vec3 centerAtT = start + move * t;
    glm::vec3 relAtT = centerAtT - edgeA;
    float projAtT = glm::dot(relAtT, edgeDir);
    if (projAtT < 0.0f || projAtT > edgeLen)
        return false;

    glm::vec3 closestOnEdge = edgeA + edgeDir * projAtT;
    glm::vec3 normal = centerAtT - closestOnEdge;
    float dist = glm::length(normal);
    if (dist < 0.000001f)
        return false;
    normal /= dist;

    hitTime = t;
    hitNormal = normal;
    hitPoint = closestOnEdge;
    return true;
}

bool sweepSphereTriangle(
    glm::vec3 start,
    glm::vec3 move,
    float radius,
    const CollisionTriangle& tri,
    float& hitTime,
    glm::vec3& hitNormal,
    glm::vec3& hitPoint
) {
    float bestT = 1.0f;
    glm::vec3 bestN(0.0f);
    glm::vec3 bestP(0.0f);
    bool hit = false;

    glm::vec3 n = tri.normal;
    float dist = glm::dot(start - tri.a, n);
    if (dist < 0.0f)
    {
        n = -n;
        dist = -dist;
    }

    float denom = glm::dot(move, n);
    if (denom < -ALMOST_ZERO)
    {
        float t = (radius - dist) / denom;
        if (t >= 0.0f && t <= 1.0f)
        {
            glm::vec3 centerAtHit = start + move * t;
            glm::vec3 planePoint = centerAtHit - n * radius;
            if (pointInTriangle(planePoint, tri))
            {
                bestT = t;
                bestN = n;
                bestP = planePoint;
                hit = true;
            }
        }
    }

    // Edge sweeps: prevent sphere from passing between vertices through the edge opening
    {
        glm::vec3 edgePairs[3][2] = {{tri.a, tri.b}, {tri.b, tri.c}, {tri.c, tri.a}};
        for (auto& ep : edgePairs)
        {
            float t = 1.0f;
            glm::vec3 en(0.0f);
            glm::vec3 epPt(0.0f);
            if (sweepSphereEdge(start, move, radius, ep[0], ep[1], t, en, epPt) && t < bestT)
            {
                bestT = t;
                bestN = en;
                bestP = epPt;
                hit = true;
            }
        }
    }

    // Vertex sweeps
    glm::vec3 pts[3] = {tri.a, tri.b, tri.c};
    for (glm::vec3 pt : pts)
    {
        float t = 1.0f;
        glm::vec3 pn(0.0f);
        glm::vec3 pp(0.0f);
        if (sweepSpherePoint(start, move, radius, pt, t, pn, pp) && t < bestT)
        {
            bestT = t;
            bestN = pn;
            bestP = pp;
            hit = true;
        }
    }

    if (!hit)
        return false;

    hitTime = bestT;
    hitNormal = bestN;
    hitPoint = bestP;
    return true;
}

static bool sphereTriangleContact(
    glm::vec3 center,
    float radius,
    const CollisionTriangle& tri,
    Contact& contact
) {
    glm::vec3 closest = closestPointOnTriangle(center, tri.a, tri.b, tri.c);
    glm::vec3 delta = center - closest;
    float dist2 = glm::dot(delta, delta);
    if (dist2 > radius * radius)
        return false;

    float dist = sqrtf(std::max(dist2, 0.0f));
    glm::vec3 n;
    if (dist > 0.00001f) {
        n = delta / dist;
        // Edge protection: if closest point is on an edge (not interior),
        // blend the computed normal toward the face normal for stability.
        float barycentricSum = 0.0f;
        glm::vec3 edgeDirs[3] = {tri.b - tri.a, tri.c - tri.b, tri.a - tri.c};
        for (int ei = 0; ei < 3; ++ei) {
            float edgeLen = glm::length(edgeDirs[ei]);
            if (edgeLen < 0.0001f) continue;
            glm::vec3 edgeDir = edgeDirs[ei] / edgeLen;
            glm::vec3 toClosest = closest - (ei == 0 ? tri.a : (ei == 1 ? tri.b : tri.c));
            float alongEdge = glm::dot(toClosest, edgeDir);
            if (alongEdge >= 0.0f && alongEdge <= edgeLen)
                barycentricSum += 1.0f;
        }
        // If closest point is on an edge (only 2 of 3 barycentric coords non-zero),
        // blend toward face normal to prevent unreliable edge normals
        if (barycentricSum < 2.5f) {
            float blend = 0.3f;
            n = glm::normalize(n + tri.normal * blend);
        }
    } else {
        float side = glm::dot(center - tri.a, tri.normal);
        n = (side >= 0.0f) ? tri.normal : -tri.normal;
    }

    contact.point = closest;
    contact.normal = n;
    contact.penetration = radius - dist;
    return true;
}

static bool capsuleTriangleSweep(
    const Capsule& cap,
    const glm::vec3& move,
    const CollisionTriangle& tri,
    int triIndex,
    SweepHit& out
) {
    constexpr int NUM_SAMPLES = 7;
    glm::vec3 samples[NUM_SAMPLES];
    for (int i = 0; i < NUM_SAMPLES; ++i) {
        float t = (float)i / (float)(NUM_SAMPLES - 1);
        samples[i] = cap.a + (cap.b - cap.a) * t;
    }
    bool hit = false;
    float bestT = 1.0f;
    glm::vec3 bestN(0.0f);
    glm::vec3 bestP(0.0f);

    for (glm::vec3 sample : samples)
    {
        float t = 1.0f;
        glm::vec3 n(0.0f);
        glm::vec3 p(0.0f);
        float skinRadius = cap.r;
        if (sweepSphereTriangle(sample, move, skinRadius, tri, t, n, p) && t < bestT)
        {
            bestT = t;
            bestN = n;
            bestP = p;
            hit = true;
        }
    }

    if (!hit)
        return false;

    out.hit = true;
    out.time = bestT;
    out.normal = bestN;
    out.point = bestP;
    out.triangleIndex = triIndex;
    out.colliderName = "world_glb";
    return true;
}

static bool capsuleTriangleContact(
    const Capsule& cap,
    const CollisionTriangle& tri,
    int triIndex,
    Contact& out
) {
    constexpr int NUM_SAMPLES = 7;
    glm::vec3 samples[NUM_SAMPLES];
    for (int i = 0; i < NUM_SAMPLES; ++i) {
        float t = (float)i / (float)(NUM_SAMPLES - 1);
        samples[i] = cap.a + (cap.b - cap.a) * t;
    }
    bool hit = false;
    Contact best;

    for (glm::vec3 sample : samples)
    {
        Contact c;
        float skinRadius = cap.r + COLLISION_SKIN;
        if (sphereTriangleContact(sample, skinRadius, tri, c))
        {
            // Clamp penetration to real radius (remove skin contribution)
            c.penetration = std::max(0.0f, c.penetration - COLLISION_SKIN);
            if (!hit || c.penetration > best.penetration)
            {
                best = c;
                hit = true;
            }
        }
    }

    if (!hit)
        return false;

    best.triangleIndex = triIndex;
    out = best;
    return true;
}

static std::vector<int> gatherGLBTriangles(const World& world, const Capsule& cap, const glm::vec3& move)
{
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

static std::vector<glm::vec3> collectPlayerBodyCollisionSamples(Player& p)
{
    std::vector<glm::vec3> samples;

    for (const Collider& collider : p.bodyColliders)
    {
        auto it = std::find_if(p.nodes.begin(), p.nodes.end(), [&](const TransformNode& node) {
            return node.name == collider.name;
        });
        if (it == p.nodes.end())
            continue;

        const glm::mat4& xform = it->worldTransform;

        // Compute capsule axis from collider bounds, then sample along it.
        // This replaces the old per-vertex sampling that caused limb snagging
        // on world geometry corners.
        glm::vec3 localCenter = (collider.localMin + collider.localMax) * 0.5f;
        glm::vec3 localExtents = (collider.localMax - collider.localMin) * 0.5f;
        float axisLen = glm::length(localExtents);
        glm::vec3 axisDir(0.0f, 0.0f, 1.0f);
        if (axisLen > 0.001f)
            axisDir = localExtents / axisLen;

        glm::vec3 worldCenter = glm::vec3(xform * glm::vec4(localCenter, 1.0f));
        float radius = std::min(localExtents.x, localExtents.y) * 1.5f;
        // Scale radius up slightly to fill gaps between old triangle vertices
        radius = std::max(radius, BODY_SAMPLE_RADIUS);
        radius = std::min(radius, 0.35f); // cap to avoid excessive size

        glm::vec3 worldA = worldCenter - glm::vec3(xform * glm::vec4(axisDir * axisLen, 0.0f));
        glm::vec3 worldB = worldCenter + glm::vec3(xform * glm::vec4(axisDir * axisLen, 0.0f));
        if (glm::length(worldB - worldA) < 0.001f) {
            // Nearly spherical part: emit a single sample
            samples.push_back(worldCenter);
            continue;
        }

        // Sample 3 spheres along the capsule axis for smooth sliding
        for (int i = 0; i < 3; i++) {
            float t = (float)i / 2.0f;
            glm::vec3 pt = worldA + (worldB - worldA) * t;
            samples.push_back(pt);
        }
    }

    return samples;
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

static void addStressTriangle(World& world, glm::vec3 a, glm::vec3 b, glm::vec3 c)
{
    glm::vec3 n = glm::cross(b - a, c - a);
    if (glm::length(n) < 0.000001f)
        return;

    CollisionTriangle tri;
    tri.a = a;
    tri.b = b;
    tri.c = c;
    tri.normal = glm::normalize(n);
    world.collisionMesh.triangles.push_back(tri);
}

static void addStressQuad(World& world, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d)
{
    addStressTriangle(world, a, b, c);
    addStressTriangle(world, a, c, d);
}

static void addStressFloor(World& world)
{
    addStressQuad(world,
        {-8.0f, -8.0f, 0.0f},
        { 8.0f, -8.0f, 0.0f},
        { 8.0f,  8.0f, 0.0f},
        {-8.0f,  8.0f, 0.0f});
}

static void addStressWedge(World& world, float degrees)
{
    const float halfRad = glm::radians(std::max(0.5f, degrees) * 0.5f);
    const float backX = -5.0f;
    const float apexX = 4.0f;
    const float width = std::max(0.08f, std::tan(halfRad) * (apexX - backX));
    const float topZ = 4.0f;

    addStressQuad(world,
        {apexX, 0.0f, 0.0f},
        {backX, width, 0.0f},
        {backX, width, topZ},
        {apexX, 0.0f, topZ});
    addStressQuad(world,
        {backX, -width, 0.0f},
        {apexX,  0.0f, 0.0f},
        {apexX,  0.0f, topZ},
        {backX, -width, topZ});
}

static void addStressCone(World& world)
{
    constexpr int SIDES = 16;
    const float radius = 1.2f;
    const float height = 3.0f;
    glm::vec3 tip(1.5f, 0.0f, height);
    glm::vec3 center(1.5f, 0.0f, 0.0f);

    for (int i = 0; i < SIDES; ++i)
    {
        float a0 = (float)i / (float)SIDES * 6.2831853f;
        float a1 = (float)(i + 1) / (float)SIDES * 6.2831853f;
        glm::vec3 p0(center.x + std::cos(a0) * radius, center.y + std::sin(a0) * radius, 0.0f);
        glm::vec3 p1(center.x + std::cos(a1) * radius, center.y + std::sin(a1) * radius, 0.0f);
        addStressTriangle(world, p0, p1, tip);
    }
}

std::string collisionStressRun(const std::string& caseName)
{
    World world;
    addStressFloor(world);

    float speed = 60.0f;
    std::string selected = caseName.empty() ? "wedge5" : caseName;
    if (selected == "cone")
    {
        addStressCone(world);
    }
    else
    {
        float angle = 5.0f;
        if (selected == "wedge1") angle = 1.0f;
        else if (selected == "wedge10") angle = 10.0f;
        else if (selected == "wedge20") angle = 20.0f;
        else if (selected == "dash") { angle = 5.0f; speed = 120.0f; }
        addStressWedge(world, angle);
    }

    Player testPlayer;
    testPlayer.pos = glm::vec3(-4.0f, 0.0f, PLAYER_HEIGHT * 0.5f + 0.05f);
    testPlayer.vel = glm::vec3(speed, 0.0f, 0.0f);
    testPlayer.syncLegacyStateToLayers();

    bool grounded = false;
    constexpr int TICKS = 60;
    constexpr float DT = 1.0f / 60.0f;
    for (int i = 0; i < TICKS; ++i)
    {
        grounded = false;
        doCollisions(testPlayer, world, grounded, DT);
    }

    Capsule cap = testPlayer.getCapsule();
    std::vector<int> candidates = gatherGLBTriangles(world, cap, glm::vec3(0.0f));
    std::vector<RecoveryContact> contacts = collectCapsuleRecoveryContacts(world, cap, candidates);
    float maxPen = 0.0f;
    for (const RecoveryContact& c : contacts)
        maxPen = std::max(maxPen, c.penetration);

    char buf[1024];
    std::snprintf(buf, sizeof(buf),
        "[COLLISION STRESS] case=%s ticks=%d final=(%.2f %.2f %.2f) vel=(%.2f %.2f %.2f) contacts=%zu maxPen=%.4f grounded=%d %s",
        selected.c_str(), TICKS,
        testPlayer.pos.x, testPlayer.pos.y, testPlayer.pos.z,
        testPlayer.vel.x, testPlayer.vel.y, testPlayer.vel.z,
        contacts.size(), maxPen, (int)grounded,
        collisionLastTraceSummary().c_str());
    return std::string(buf);
}
