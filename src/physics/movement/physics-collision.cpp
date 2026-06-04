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

static inline void clampVelocityAgainstNormal(Player& p, const glm::vec3& normal)
{
    glm::vec3 totalVel = p.vel + glm::vec3(p.dashVel, 0.0f);
    float into = glm::dot(totalVel, normal);
    if (into >= -0.0001f)
        return;

    glm::vec3 resolved = totalVel - normal * into;
    p.vel = resolved;
    p.dashVel = glm::vec2(0.0f);
}

static inline void applyCollisionContact(
    Player& p,
    bool& groundedThisFrame,
    const glm::vec3& normal,
    glm::vec3 point,
    float penetration,
    int triangleIndex,
    const char* label
) {
    clampVelocityAgainstNormal(p, normal);

    if (normal.z > 0.35f)
    {
        groundedThisFrame = true;
        applyTouchResets(p);
        if (p.vel.z < 0.0f)
            p.vel.z = 0.0f;
        DebugVis::recordGroundNormal(point, normal, label);
    }
    else if (normal.z < -0.35f)
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

        // if (normal.z > 0.5f)
        // set to 0.3f to make more things count as grounded? idk mar 7 2026
        if (normal.z > 0.3f)
            grounded = true;

        return true;
    }

    return false;
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
    p.freezeAvailable = true;
}

static void doGLBTriangleCollisions(
    Player& p,
    const World& world,
    bool& groundedThisFrame,
    float dt
) {
    glm::vec3 move = (p.vel + glm::vec3(p.dashVel, 0.0f)) * dt;
    p.updateModelWorldTransforms();
    Capsule cap = p.getCapsule();
    std::vector<int> candidates = gatherGLBTriangles(world, cap, move);
    std::vector<glm::vec3> bodySamples = collectPlayerBodyCollisionSamples(p);
    constexpr float BODY_SAMPLE_RADIUS = 0.045f;
    for (glm::vec3 sample : bodySamples)
    {
        std::vector<int> sampleCandidates = gatherGLBTrianglesForSphere(world, sample, BODY_SAMPLE_RADIUS, move);
        for (int triIndex : sampleCandidates)
            if (std::find(candidates.begin(), candidates.end(), triIndex) == candidates.end())
                candidates.push_back(triIndex);
    }

    static int frameLog = 0;
    if ((frameLog++ % 60) == 0)
    {
        PHYS_LOG(
            "[PHYS][GLB] tris=%zu candidates=%zu bodySamples=%zu pos=(%.2f %.2f %.2f) move=(%.3f %.3f %.3f)\n",
            world.collisionMesh.triangles.size(),
            candidates.size(),
            bodySamples.size(),
            p.pos.x, p.pos.y, p.pos.z,
            move.x, move.y, move.z
        );
    }

    constexpr float SURFACE_SLOP = 0.002f;

    for (int iter = 0; iter < 5; ++iter)
    {
        SweepHit earliest;
        earliest.time = 1.0f;

        cap = p.getCapsule();
        DebugVis::recordMovement(p.pos, move, "glb-substep-move");
        DebugVis::recordSweep(cap.a, cap.a + move, "capsule-bottom");
        DebugVis::recordSweep((cap.a + cap.b) * 0.5f, (cap.a + cap.b) * 0.5f + move, "capsule-mid");
        DebugVis::recordSweep(cap.b, cap.b + move, "capsule-top");
        for (int triIndex : candidates)
        {
            const CollisionTriangle& tri = world.collisionMesh.triangles[triIndex];
            SweepHit hit;
            if (capsuleTriangleSweep(cap, move, tri, triIndex, hit) && hit.time < earliest.time)
                earliest = hit;
        }

        for (glm::vec3 sample : bodySamples)
        {
            for (int triIndex : candidates)
            {
                float t = 1.0f;
                glm::vec3 n(0.0f);
                glm::vec3 point(0.0f);
                if (sweepSphereTriangle(sample, move, BODY_SAMPLE_RADIUS, world.collisionMesh.triangles[triIndex], t, n, point) && t < earliest.time)
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

        glm::vec3 stepMove = move * earliest.time;
        p.pos += stepMove;
        p.updateModelWorldTransforms();
        bodySamples = collectPlayerBodyCollisionSamples(p);
        cap = p.getCapsule();

        if (!earliest.hit)
            break;

        p.pos += earliest.normal * SURFACE_SLOP;
        DebugVis::recordHit(earliest.point, earliest.normal, earliest.triangleIndex, earliest.colliderName.c_str());
        DebugVis::recordTriangle(world.collisionMesh.triangles[earliest.triangleIndex], earliest.triangleIndex, "sweep-hit-triangle");
        move -= stepMove;

        float vn = glm::dot(move, earliest.normal);
        if (vn < 0.0f)
            move -= earliest.normal * vn;

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

        if (glm::dot(move, move) < 0.000001f)
            break;
    }

    cap = p.getCapsule();
    p.updateModelWorldTransforms();
    bodySamples = collectPlayerBodyCollisionSamples(p);
    candidates = gatherGLBTriangles(world, cap, glm::vec3(0.0f));
    for (int cleanupIter = 0; cleanupIter < 8; ++cleanupIter)
    {
        bool moved = false;
        p.updateModelWorldTransforms();
        cap = p.getCapsule();
        bodySamples = collectPlayerBodyCollisionSamples(p);
        candidates = gatherGLBTriangles(world, cap, glm::vec3(0.0f));
        for (int triIndex : candidates)
        {
            Contact contact;
            if (!capsuleTriangleContact(cap, world.collisionMesh.triangles[triIndex], triIndex, contact))
                continue;

            glm::vec3 correction = contact.normal * (contact.penetration + SURFACE_SLOP);
            p.pos += correction;
            DebugVis::recordDepenetration(p.pos - correction, correction, "capsule-triangle");
            moved = true;
            applyCollisionContact(
                p,
                groundedThisFrame,
                contact.normal,
                contact.point,
                contact.penetration,
                contact.triangleIndex,
                "capsule-triangle"
            );

            PHYS_LOG(
                "[PHYS][GLB CONTACT] tri=%d pen=%.4f normal=(%.2f %.2f %.2f)\n",
                contact.triangleIndex,
                contact.penetration,
                contact.normal.x,
                contact.normal.y,
                contact.normal.z
            );

            cap = p.getCapsule();
        }

        bool restartBodySamplePass = false;
        for (glm::vec3 sample : bodySamples)
        {
            for (int triIndex : candidates)
            {
                Contact contact;
                if (!sphereTriangleContact(sample, BODY_SAMPLE_RADIUS, world.collisionMesh.triangles[triIndex], contact))
                    continue;

                contact.triangleIndex = triIndex;
                glm::vec3 correction = contact.normal * (contact.penetration + SURFACE_SLOP);
                p.pos += correction;
                DebugVis::recordDepenetration(p.pos - correction, correction, "body-triangle");
                moved = true;
                restartBodySamplePass = true;
                applyCollisionContact(
                    p,
                    groundedThisFrame,
                    contact.normal,
                    contact.point,
                    contact.penetration,
                    contact.triangleIndex,
                    "body-triangle"
                );

                PHYS_LOG(
                    "[PHYS][BODY CONTACT] tri=%d pen=%.4f normal=(%.2f %.2f %.2f)\n",
                    contact.triangleIndex,
                    contact.penetration,
                    contact.normal.x,
                    contact.normal.y,
                    contact.normal.z
                );
                break;
            }

            if (restartBodySamplePass)
                break;
        }

        if (!moved)
            break;
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
    if (!world.collisionMesh.empty())
    {
        doGLBTriangleCollisions(p, world, groundedThisFrame, dt);
        return;
    }

    // do not set grounded here
    // so that we can actually jump 
    // groundedThisFrame = false;

    // glm::vec3 move = p.vel * dt;

    // this kind includes dash movement, for sweeps 
    glm::vec3 move = (p.vel + glm::vec3(p.dashVel,0.0f)) * dt;

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

    // fallback overlap cleanup for tiny penetrations / resting contact
    // dont call capsule cap 
    // just put cap mar 7 2026 
    cap = p.getCapsule();

    for (int recoverIter = 0; recoverIter < 8; ++recoverIter)
    {
        bool moved = false;
        cap = p.getCapsule();

        for (Block* b : nearbyBlocks)
        {
            if (!b || b->isSlope)
                continue;

            AABB ba = makeBlockAABB(*b);

            glm::vec3 correction;
            bool grounded = false;

            if (capsuleVsBlock(cap, ba, correction, grounded))
            {
                glm::vec3 before = p.pos;
                glm::vec3 normal = glm::length(correction) > 0.000001f ? glm::normalize(correction) : glm::vec3(0,0,1);
                p.pos += correction;
                moved = true;

                DebugVis::recordDepenetration(before, correction, "block-overlap");
                DebugVis::recordContact(before + correction, normal, glm::length(correction), -1, "block-overlap");

                if (grounded)
                {
                    groundedThisFrame = true;

                    // --------------------------------------------------
                    // TOUCH OBJECT RESET (overlap correction)
                    // --------------------------------------------------

                    p.airJumpsLeft = AIR_JUMPS_MAX;
                    p.dashAvailable = true;

                    // future abilities
                    p.groundReturnAvailable = true;
                    p.freezeAvailable = true;

                    if (p.vel.z < 0)
                        p.vel.z = 0;
                    DebugVis::recordGroundNormal(before + correction, normal, "block-overlap-ground");
                }

                clampVelocityAgainstNormal(p, normal);
                cap = p.getCapsule();
            }
        }

        if (!moved)
            break;
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
    glm::vec3 samples[3] = {cap.a, (cap.a + cap.b) * 0.5f, cap.b};
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
    glm::vec3 samples[3] = {cap.a, (cap.a + cap.b) * 0.5f, cap.b};
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
