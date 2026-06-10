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
#include <unordered_set>
#include <glm/glm.hpp>

#include "physics/config.h"
#include "world/world.h"
#include "entities/player.h"
#include "debug/debug-log.h"
#include "debug/debug-visuals.h"
#include "config/player-settings.h"
#include "devtools/terminal.h"

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

static bool sweepSphereTriangle(
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

struct RecoveryContact
{
    glm::vec3 normal{0.0f, 0.0f, 1.0f};
    glm::vec3 point{0.0f};
    float penetration = 0.0f;
    int triangleIndex = -1;
    const Block* block = nullptr;
    const char* label = "recovery";
};

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

    glm::vec3 incoming = p.vel + p.externalImpulse;
    float speed = glm::length(incoming);
    float into = glm::dot(incoming, normal);
    const PlayerSettings& cfg = GetPlayerSettings();

    // Wall-like normal: project velocity
    bool isWall = std::fabs(normal.z) < 0.45f;
    if (isWall)
    {
        projectVelocityAgainstNormal(p, normal);
    }
    
    // Ground: slope is walkable
    if (normal.z >= MAX_WALKABLE_SLOPE_DOT)
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
    // Steep slope: not ground, but project velocity along tangent for sliding
    else if (normal.z > 0.0f)
    {
        applyTouchResets(p);
        // Project velocity along slope tangent to allow sliding/surfing
        glm::vec3 tangent = glm::normalize(glm::cross(glm::cross(normal, glm::vec3(0,0,1)), normal));
        float tangentSpeed = glm::dot(p.vel, tangent);
        p.vel = tangent * tangentSpeed;
        float tangentImpulse = glm::dot(p.externalImpulse, tangent);
        p.externalImpulse = tangent * tangentImpulse;
    }
    else if (normal.z < -MAX_WALKABLE_SLOPE_DOT)
    {
        if (p.vel.z > 0.0f)
            p.vel.z = 0.0f;
    }
    else
    {
        applyTouchResets(p);
    }

    DebugVis::recordContact(point, normal, penetration, triangleIndex, label);
}

// =====================================================
// AABB helpers (TEMP until capsule collisions v3)
// =====================================================

struct AABB {
    glm::vec3 min;
    glm::vec3 max;
};

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

static inline glm::ivec3 collisionChunkCoord(const glm::vec3& p, float size)
{
    return glm::ivec3(
        (int)std::floor(p.x / size),
        (int)std::floor(p.y / size),
        (int)std::floor(p.z / size)
    );
}

static void appendChunkTrianglesForAABB(
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
// Overlap cleanup fallback
// =====================================================

static inline void resolveOverlap(
    const AABB& block,
    AABB& player,
    Player& p,
    bool& groundedOut,
    const Block& b
)
{
    float px1 = block.max.x - player.min.x;
    float px2 = player.max.x - block.min.x;
    float py1 = block.max.y - player.min.y;
    float py2 = player.max.y - block.min.y;
    float pz1 = block.max.z - player.min.z;
    float pz2 = player.max.z - block.min.z;

    float penX = std::min(px1, px2);
    float penY = std::min(py1, py2);
    float penZ = std::min(pz1, pz2);

    constexpr float SLOP = 0.001f;

    if (penX <= penY && penX <= penZ)
    {
        float centerPlayer = (player.min.x + player.max.x) * 0.5f;
        float centerBlock  = (block.min.x  + block.max.x)  * 0.5f;

        float push = (centerPlayer < centerBlock)
            ? -(penX + SLOP)
            :  ( penX + SLOP);

        p.pos.x += push;
        p.vel.x = 0.0f;

        PHYS_LOG(
            "[PHYS][COLLISION] X axis | push=%.4f | block=(%.2f %.2f %.2f)\n",
            push, b.pos.x, b.pos.y, b.pos.z
        );
    }
    else if (penY <= penZ)
    {
        float centerPlayer = (player.min.y + player.max.y) * 0.5f;
        float centerBlock  = (block.min.y  + block.max.y)  * 0.5f;

        float push = (centerPlayer < centerBlock)
            ? -(penY + SLOP)
            :  ( penY + SLOP);

        p.pos.y += push;
        p.vel.y = 0.0f;

        PHYS_LOG(
            "[PHYS][COLLISION] Y axis | push=%.4f | block=(%.2f %.2f %.2f)\n",
            push, b.pos.x, b.pos.y, b.pos.z
        );
    }
    else
    {
        float centerPlayer = (player.min.z + player.max.z) * 0.5f;
        float centerBlock  = (block.min.z  + block.max.z)  * 0.5f;

        float push = (centerPlayer < centerBlock)
            ? -(penZ + SLOP)
            :  ( penZ + SLOP);

        p.pos.z += push;

        if (push > 0.0f)
        {
            groundedOut = true;

            if (p.vel.z < 0.0f)
                p.vel.z = 0.0f;

            PHYS_LOG(
                "[PHYS][GROUND] Grounded on block=(%.2f %.2f %.2f)\n",
                b.pos.x, b.pos.y, b.pos.z
            );
        }
        else
        {
            if (p.vel.z > 0.0f)
                p.vel.z = 0.0f;
        }

        PHYS_LOG(
            "[PHYS][COLLISION] Z axis | push=%.4f | block=(%.2f %.2f %.2f)\n",
            push, b.pos.x, b.pos.y, b.pos.z
        );
    }

    player = makePlayerAABB(p);
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
            if (sphereAABBContact(sample, cap.r, ba, c, b, "block-overlap"))
                contacts.push_back(c);
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

static glm::vec3 solveBatchedCorrection(
    const std::vector<RecoveryContact>& contacts,
    float slop,
    float* outMaxPenetration = nullptr,
    glm::vec3* outWeightedNormal = nullptr,
    glm::vec3 intendedMove = glm::vec3(0.0f),
    glm::vec3 debugPosition = glm::vec3(0.0f)
) {
    const PlayerSettings& cfg = GetPlayerSettings();
    std::vector<RecoveryContact> manifold;
    std::vector<RecoveryContact> discarded;
    for (const RecoveryContact& contact : contacts) {
        RecoveryContact merged = contact;
        bool found = false;
        for (RecoveryContact& existing : manifold) {
            float alignment = glm::dot(existing.normal, contact.normal);
            if (alignment >= 1.0f - cfg.collisionSeamTolerance) {
                float totalWeight = std::max(existing.penetration, slop) + std::max(contact.penetration, slop);
                existing.normal = glm::normalize(existing.normal * std::max(existing.penetration, slop)
                                               + contact.normal * std::max(contact.penetration, slop));
                existing.point = (existing.point + contact.point) * 0.5f;
                existing.penetration = std::max(existing.penetration, contact.penetration);
                (void)totalWeight;
                found = true;
                break;
            }
        }
        if (!found)
            manifold.push_back(merged);
    }

    if (glm::dot(intendedMove, intendedMove) > 0.000001f && manifold.size() > 1) {
        glm::vec3 moveDir = glm::normalize(intendedMove);
        size_t preferred = 0;
        float preferredScore = -std::numeric_limits<float>::max();
        for (size_t i = 0; i < manifold.size(); ++i) {
            float blocks = std::max(0.0f, -glm::dot(moveDir, manifold[i].normal));
            float score = manifold[i].penetration - blocks * cfg.collisionMovementBias;
            if (score > preferredScore) {
                preferredScore = score;
                preferred = i;
            }
        }
        std::vector<RecoveryContact> filtered;
        for (size_t i = 0; i < manifold.size(); ++i) {
            bool shallowSeam = i != preferred &&
                manifold[i].penetration <= cfg.collisionSeamTolerance &&
                std::fabs(manifold[i].normal.z) < 0.45f;
            if (shallowSeam)
                discarded.push_back(manifold[i]);
            else
                filtered.push_back(manifold[i]);
        }
        manifold.swap(filtered);

        static int seamLogCooldown = 0;
        seamLogCooldown = std::max(0, seamLogCooldown - 1);
        if (!discarded.empty() && seamLogCooldown == 0) {
            const RecoveryContact& chosen = manifold.front();
            char line[512];
            snprintf(line, sizeof(line),
                     "[COLLISION SEAM] pos=(%.3f %.3f %.3f) move=(%.3f %.3f %.3f) chosen=(%.3f %.3f %.3f) discarded=%zu",
                     debugPosition.x, debugPosition.y, debugPosition.z,
                     intendedMove.x, intendedMove.y, intendedMove.z,
                     chosen.normal.x, chosen.normal.y, chosen.normal.z, discarded.size());
            Terminal::instance().addLog(line);
            for (const RecoveryContact& c : manifold) {
                snprintf(line, sizeof(line), "  contact normal=(%.3f %.3f %.3f) penetration=%.4f",
                         c.normal.x, c.normal.y, c.normal.z, c.penetration);
                Terminal::instance().addLog(line);
            }
            for (const RecoveryContact& c : discarded) {
                snprintf(line, sizeof(line), "  discarded normal=(%.3f %.3f %.3f) penetration=%.4f",
                         c.normal.x, c.normal.y, c.normal.z, c.penetration);
                Terminal::instance().addLog(line);
            }
            seamLogCooldown = 30;
        }
    }

    glm::vec3 correction(0.0f);
    glm::vec3 weightedNormal(0.0f);
    float maxPenetration = 0.0f;

    for (const RecoveryContact& c : manifold)
    {
        maxPenetration = std::max(maxPenetration, c.penetration);
        weightedNormal += c.normal * std::max(c.penetration + slop, slop);
    }

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

static void applyRecoveryContacts(
    Player& p,
    bool& groundedThisFrame,
    const std::vector<RecoveryContact>& contacts,
    const glm::vec3& correction,
    const char* debugLabel
) {
    for (const RecoveryContact& c : contacts)
    {
        applyCollisionContact(
            p,
            groundedThisFrame,
            c.normal,
            c.point,
            c.penetration,
            c.triangleIndex,
            c.label
        );
    }

    glm::vec3 before = p.pos;
    p.pos += correction;
    DebugVis::recordDepenetration(before, correction, debugLabel);
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
}

static void doGLBTriangleCollisions(
    Player& p,
    const World& world,
    bool& groundedThisFrame,
    float dt
) {
    constexpr float SURFACE_SLOP = 0.002f;
    constexpr float MAX_CORRECTION = 2.0f;

    // CHANGED: No dashVel — dash is now in vel, jun 6 2026
    glm::vec3 totalMove = (p.vel + p.externalImpulse) * dt;
    p.updateModelWorldTransforms();
    Capsule cap = p.getCapsule();

    std::vector<int> candidates = gatherGLBTriangles(world, cap, totalMove);
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
        for (int triIndex : sampleCandidates)
            if (std::find(candidates.begin(), candidates.end(), triIndex) == candidates.end())
                candidates.push_back(triIndex);
    }

    // Save current samples as previous for next frame
    p.previousBodySamplePositions = bodySamples;

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

        cap = p.getCapsule();
        DebugVis::recordMovement(p.pos, remainingMove, "glb-substep-move");
        DebugVis::recordSweep(cap.a, cap.a + remainingMove, "capsule-bottom");
        DebugVis::recordSweep((cap.a + cap.b) * 0.5f, (cap.a + cap.b) * 0.5f + remainingMove, "capsule-mid");
        DebugVis::recordSweep(cap.b, cap.b + remainingMove, "capsule-top");
        for (int triIndex : candidates)
        {
            const CollisionTriangle& tri = world.collisionMesh.triangles[triIndex];
            SweepHit hit;
            if (capsuleTriangleSweep(cap, remainingMove, tri, triIndex, hit) && hit.time < earliest.time)
                earliest = hit;
        }

        for (size_t si = 0; si < bodySamples.size(); ++si)
        {
            glm::vec3 sample = bodySamples[si];
            // Include limb animation delta in the sweep every iteration
            glm::vec3 animDelta = (si < bodyDeltas.size()) ? bodyDeltas[si] : glm::vec3(0.0f);
            glm::vec3 sampleMove = remainingMove + animDelta;
            for (int triIndex : candidates)
            {
                float t = 1.0f;
                glm::vec3 n(0.0f);
                glm::vec3 point(0.0f);
                if (sweepSphereTriangle(sample, sampleMove, BODY_SAMPLE_RADIUS, world.collisionMesh.triangles[triIndex], t, n, point) && t < earliest.time)
                {
                    earliest.hit = true;
                    earliest.time = t;
                    earliest.normal = n;
                    earliest.point = point;
                    earliest.triangleIndex = triIndex;
                    earliest.colliderName = "player_body_part";
                }
            }
        }

        glm::vec3 stepMove = remainingMove * earliest.time;
        p.pos += stepMove;
        p.updateModelWorldTransforms();
        bodySamples = collectPlayerBodyCollisionSamples(p);
        cap = p.getCapsule();

        if (!earliest.hit)
            break;

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
                    groundedThisFrame = true;

                    if (p.vel.z < 0.0f)
                        p.vel.z = 0.0f;

                    PHYS_LOG(
                        "[GLB STEP] stepped up %.3f\n",
                        stepHeight
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
        DebugVis::recordHit(earliest.point, earliest.normal, earliest.triangleIndex, earliest.colliderName.c_str());
        DebugVis::recordTriangle(world.collisionMesh.triangles[earliest.triangleIndex], earliest.triangleIndex, "sweep-hit-triangle");
        remainingMove -= stepMove;

        float vn = glm::dot(remainingMove, earliest.normal);
        if (vn < 0.0f)
            remainingMove -= earliest.normal * vn;

        applyCollisionContact(
            p,
            groundedThisFrame,
            earliest.normal,
            earliest.point,
            SURFACE_SLOP,
            earliest.triangleIndex,
            earliest.colliderName.c_str()
        );

        PHYS_LOG(
            "[PHYS][GLB HIT] tri=%d t=%.3f normal=(%.2f %.2f %.2f) point=(%.2f %.2f %.2f)\n",
            earliest.triangleIndex,
            earliest.time,
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
    bodySamples = collectPlayerBodyCollisionSamples(p);
    candidates = gatherGLBTriangles(world, cap, glm::vec3(0.0f));

    float maxPenetrationSeen = 0.0f;

    for (int depenIter = 0; depenIter < 4; ++depenIter)
    {
        p.updateModelWorldTransforms();
        cap = p.getCapsule();
        bodySamples = collectPlayerBodyCollisionSamples(p);
        candidates = gatherGLBTriangles(world, cap, glm::vec3(0.0f));

        std::vector<RecoveryContact> contacts = collectGLBRecoveryContacts(
            world, cap, bodySamples, candidates, BODY_SAMPLE_RADIUS
        );

        if (contacts.empty())
            break;

        // Use batched correction solver instead of sequential pushes
        // This iteratively solves all contact constraints simultaneously,
        // preventing contradictory normals from fighting each other
        float iterMaxPen = 0.0f;
        glm::vec3 correction = solveBatchedCorrection(contacts, SURFACE_SLOP, &iterMaxPen, nullptr, totalMove, p.pos);
        maxPenetrationSeen = std::max(maxPenetrationSeen, iterMaxPen);

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

    // Ground snap: if player is very close to ground and not jumping up, snap to ground
    // This prevents hovering and flickering grounded state
    // IMPORTANT: After snapping, re-check for wall penetration
    {
        constexpr float GROUND_SNAP_DISTANCE = 0.08f;
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
                
                // Check if triangle is horizontally aligned with player
                float triMinX = std::min({tri.a.x, tri.b.x, tri.c.x});
                float triMaxX = std::max({tri.a.x, tri.b.x, tri.c.x});
                float triMinY = std::min({tri.a.y, tri.b.y, tri.c.y});
                float triMaxY = std::max({tri.a.y, tri.b.y, tri.c.y});
                
                if (checkCap.a.x + checkCap.r >= triMinX && checkCap.a.x - checkCap.r <= triMaxX &&
                    checkCap.a.y + checkCap.r >= triMinY && checkCap.a.y - checkCap.r <= triMaxY)
                {
                    // Triangle is roughly under player - check Z
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
                    
                    PHYS_LOG("[PHYS][GROUND SNAP] snapped %.4f to ground at %.2f\n", snapAmount, bestGroundZ);

                    // SAFETY: After ground snap, re-check for wall penetration
                    // Snapping upward can push player into adjacent walls
                    {
                        p.updateModelWorldTransforms();
                        Capsule postSnapCap = p.getCapsule();
                        std::vector<int> postSnapCandidates = gatherGLBTriangles(world, postSnapCap, glm::vec3(0.0f));
                        std::vector<glm::vec3> postSnapSamples = collectPlayerBodyCollisionSamples(p);
                        std::vector<RecoveryContact> postSnapContacts = collectGLBRecoveryContacts(
                            world, postSnapCap, postSnapSamples, postSnapCandidates, BODY_SAMPLE_RADIUS
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
        std::vector<RecoveryContact> stuckContacts = collectGLBRecoveryContacts(
            world, stuckCheckCap, collectPlayerBodyCollisionSamples(p), stuckCandidates, BODY_SAMPLE_RADIUS
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

    // CHANGED: Phase 3 velocity projection — no dashVel, skip floor normals, jun 6 2026
    p.updateModelWorldTransforms();
    cap = p.getCapsule();
    candidates = gatherGLBTriangles(world, cap, glm::vec3(0.0f));
    bodySamples = collectPlayerBodyCollisionSamples(p);

    std::vector<RecoveryContact> finalContacts = collectGLBRecoveryContacts(
        world, cap, bodySamples, candidates, BODY_SAMPLE_RADIUS
    );

    for (const RecoveryContact& c : finalContacts)
    {
        // Only project against wall-like normals — floors should never cancel horizontal velocity
        if (std::fabs(c.normal.z) > 0.35f)
            continue;
        // projectVelocityAgainstNormal(p, c.normal);
        glm::vec3 wallNormal = c.normal;

        // walls only
        if (std::fabs(wallNormal.z) < 0.45f)
        {
            wallNormal.z = 0.0f;

            if (glm::length(wallNormal) > 0.0001f)
                wallNormal = glm::normalize(wallNormal);

            projectVelocityAgainstNormal(p, wallNormal);
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
}

// =====================================================
// PUBLIC ENTRY
// =====================================================

void doCollisions(
    Player& p,
    const World& world,
    bool& groundedThisFrame,
    float dt
)
{
    p.collisionBounceCooldown = std::max(0.0f, p.collisionBounceCooldown - dt);
    if (!world.collisionMesh.empty())
    {
        doGLBTriangleCollisions(p, world, groundedThisFrame, dt);
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
        constexpr float GROUND_SNAP_DISTANCE = 0.08f;
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

    // CHANGED: Project velocity against block contacts — skip floor normals, no dashVel, jun 6 2026
    cap = p.getCapsule();
    std::vector<RecoveryContact> blockContacts = collectBlockContactsForCapsule(cap, nearbyBlocks);
    for (const RecoveryContact& c : blockContacts)
    {
        if (std::fabs(c.normal.z) > 0.35f)
            continue;
        // projectVelocityAgainstNormal(p, c.normal);
        glm::vec3 wallNormal = c.normal;

        // walls only
        if (std::fabs(wallNormal.z) < 0.45f)
        {
            wallNormal.z = 0.0f;

            if (glm::length(wallNormal) > 0.0001f)
                wallNormal = glm::normalize(wallNormal);

            projectVelocityAgainstNormal(p, wallNormal);
        }
    }
}

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

static bool sweepSphereTriangle(
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
    glm::vec3 n = dist > 0.00001f ? delta / dist : tri.normal;
    if (glm::dot(n, tri.normal) < 0.0f)
        n = -n;

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
    constexpr int NUM_SAMPLES = 5;
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
        if (sweepSphereTriangle(sample, move, cap.r, tri, t, n, p) && t < bestT)
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
    constexpr int NUM_SAMPLES = 5;
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
        if (sphereTriangleContact(sample, cap.r, tri, c))
        {
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
        if (!collider.samplePoints.empty())
        {
            for (glm::vec3 point : collider.samplePoints)
                samples.push_back(glm::vec3(xform * glm::vec4(point, 1.0f)));
        }
        else
        {
            for (const CollisionTriangle& tri : collider.triangles)
            {
                samples.push_back(glm::vec3(xform * glm::vec4(tri.a, 1.0f)));
                samples.push_back(glm::vec3(xform * glm::vec4(tri.b, 1.0f)));
                samples.push_back(glm::vec3(xform * glm::vec4(tri.c, 1.0f)));
            }
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
