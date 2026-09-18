// 09 17 2026
/* purpose
* Deterministic candidate self-test for the collision package. Runs against
* synthetic geometry before the generation is allowed to activate, so a broken
* broadphase or narrowphase is rejected and the previous generation keeps
* running.
* Tests: flat floor contact, wall response, slope narrowphase, shared union
* candidate set without duplicates, large-triangle indexing, cache rebuild after
* a map change, and invalid geometry safety.
* Does NOT own runtime collision or movement.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/packages/collision/collision-abi.h"
#include "hot-reload/packages/collision/collision-world.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace HotCollisionPackage {

namespace {

using glm::vec3;

bool fail(char* message, std::uint32_t cap, const char* text)
{
    if (message && cap) {
        std::uint32_t i = 0;
        for (; text[i] != '\0' && i + 1 < cap; ++i)
            message[i] = text[i];
        message[i] = '\0';
    }
    return false;
}

vec3 V(float x, float y, float z) { return vec3(x, y, z); }

void installQuad(const vec3& a, const vec3& b, const vec3& c, const vec3& d)
{
    WorldTri tris[2] = {{a, b, c}, {a, c, d}};
    installWorld(tris, 2);
}

bool finite3(const float v[3])
{
    return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
}

CollisionSolveV1 makeCapsule(std::uint64_t entity, std::uint64_t tick,
                             const vec3& pos, const vec3& vel, float radius,
                             float halfHeight)
{
    CollisionSolveV1 q{};
    q.entityId = entity;
    q.tick = tick;
    q.dt = 1.0f / 60.0f;
    q.sizeScale = 1.0f;
    q.mask = COLLISION_MASK_WORLD;
    q.colliderCount = 1;
    CollisionColliderV1& c = q.colliders[0];
    c.partId = COLLISION_PART_CAPSULE;
    c.shape = COLLISION_SHAPE_CAPSULE;
    c.policyId = COLLISION_POLICY_CAPSULE;
    c.radius = radius;
    c.halfHeight = halfHeight;
    for (int k = 0; k < 3; ++k) {
        c.position[k] = k == 0 ? pos.x : (k == 1 ? pos.y : pos.z);
        q.position[k] = c.position[k];
        q.velocity[k] = k == 0 ? vel.x : (k == 1 ? vel.y : vel.z);
    }
    return q;
}

// Advance a multi-tick test: the root and its collider positions must follow the
// package result together so each collider's local offset stays correct.
void carryOver(CollisionSolveV1& q)
{
    for (int k = 0; k < 3; ++k) {
        q.position[k] = q.outPosition[k];
        q.velocity[k] = q.outVelocity[k];
    }
    for (std::uint32_t i = 0; i < q.colliderCount; ++i) {
        for (int k = 0; k < 3; ++k)
            q.colliders[i].position[k] = q.outPosition[k];
    }
}

} // namespace

bool collisionPackageSelfTest(char* message, std::uint32_t cap)
{
    // Start from a clean runtime state so no earlier test leaves hysteresis or
    // bounce cooldown memory that would change a later test's result.
    collisionResetRuntimeState();

    // 1. Capsule resting on / impacting a flat floor contacts and grounds.
    installQuad(V(-20, -20, 0), V(20, -20, 0), V(20, 20, 0), V(-20, 20, 0));
    {
        CollisionSolveV1 q = makeCapsule(101, 10, V(0, 0, 0.49f), V(0, 0, -5),
                                         0.4f, 0.5f);
        collisionSolve(nullptr, &q);
        if (!q.handled || !q.grounded || !q.worldContact)
            return fail(message, cap, "capsule floor: no world contact");
        if (!(q.outPosition[2] > 0.3f && q.outPosition[2] < 0.9f))
            return fail(message, cap, "capsule floor: bad resolved height");
        if (!finite3(q.outPosition) || !finite3(q.outVelocity))
            return fail(message, cap, "capsule floor: non-finite output");
    }

    // 2. Moving into a wall responds along the wall normal and keeps the actor
    //    on the near side.
    installQuad(V(0, -20, -20), V(0, 20, -20), V(0, 20, 20), V(0, -20, 20));
    {
        CollisionSolveV1 q = makeCapsule(102, 20, V(0.39f, 0, 0), V(-5, 0, 0),
                                         0.4f, 0.5f);
        collisionSolve(nullptr, &q);
        if (!q.handled || !q.worldContact)
            return fail(message, cap, "wall: no contact");
        if (!(q.outPosition[0] > 0.3f))
            return fail(message, cap, "wall: passed through surface");
        if (!(q.outVelocity[0] >= -0.01f))
            return fail(message, cap, "wall: normal velocity not removed");
    }

    // 3. Slope narrowphase returns a tilted normal.
    {
        WorldTri slope[1] = {{V(0, 0, 0), V(10, 0, 0), V(0, 10, 10)}};
        installWorld(slope, 1);
        std::vector<std::uint32_t> candidates;
        gatherCandidates(V(-5, -5, -5), V(15, 15, 15), candidates);
        if (candidates.size() != 1)
            return fail(message, cap, "slope: triangle not gathered");
        SphereHit hits[4];
        const int hc = gatherSphereHits(V(3.33f, 3.12f, 3.54f), 0.5f, 0.02f,
                                        candidates, hits, 4);
        if (hc <= 0 || hits[0].penetration <= 0.0f)
            return fail(message, cap, "slope: no penetration found");
        if (std::abs(hits[0].normal.z) < 0.1f ||
            std::abs(hits[0].normal.y) < 0.1f)
            return fail(message, cap, "slope: normal is not tilted");
    }

    // 4. One shared union gather is local and has no duplicate triangles.
    {
        std::vector<WorldTri> many;
        for (int x = -20; x <= 20; ++x) {
            for (int y = -20; y <= 20; ++y) {
                const float cx = (float)x * 10.0f;
                const float cy = (float)y * 10.0f;
                many.push_back({V(cx - 0.5f, cy - 0.5f, 0), V(cx + 0.5f, cy - 0.5f, 0),
                                V(cx + 0.5f, cy + 0.5f, 0)});
            }
        }
        installWorld(many.data(), (std::uint32_t)many.size());
        std::vector<std::uint32_t> candidates;
        gatherCandidates(V(-1, -1, -1), V(1, 1, 1), candidates);
        if (candidates.empty() || candidates.size() >= many.size())
            return fail(message, cap, "union gather: not local");
        std::vector<std::uint32_t> sorted = candidates;
        std::sort(sorted.begin(), sorted.end());
        if (std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end())
            return fail(message, cap, "union gather: duplicate candidates");
    }

    // 5. Large triangles are still found via the coarse/always index.
    {
        WorldTri huge[1] = {{V(-1000, -1000, 0), V(1000, -1000, 0), V(0, 1000, 0)}};
        installWorld(huge, 1);
        std::vector<std::uint32_t> candidates;
        gatherCandidates(V(-1, -1, -1), V(1, 1, 1), candidates);
        if (candidates.size() != 1 || candidates[0] != 0)
            return fail(message, cap, "large triangle not indexed");
    }

    // 6. Cache rebuilds after a map change.
    {
        WorldTri origin[1] = {{V(-1, -1, 0), V(1, -1, 0), V(0, 1, 0)}};
        installWorld(origin, 1);
        std::vector<std::uint32_t> candidates;
        gatherCandidates(V(-2, -2, -2), V(2, 2, 2), candidates);
        if (candidates.size() != 1)
            return fail(message, cap, "cache: initial map missing");
        WorldTri far[1] = {{V(999, 999, 0), V(1001, 999, 0), V(1000, 1001, 0)}};
        installWorld(far, 1);
        gatherCandidates(V(-2, -2, -2), V(2, 2, 2), candidates);
        if (!candidates.empty())
            return fail(message, cap, "cache: stale geometry after rebuild");
    }

    // 7. Invalid geometry does not crash or produce non-finite output.
    {
        const float nan = std::nanf("");
        WorldTri bad[2] = {
            {V(nan, 0, 0), V(1, 0, 0), V(0, 1, 0)},
            {V(0, 0, 0), V(0, 0, 0), V(0, 0, 0)}};
        installWorld(bad, 2);
        std::vector<std::uint32_t> candidates;
        gatherCandidates(V(-1, -1, -1), V(1, 1, 1), candidates);
        CollisionSolveV1 q = makeCapsule(103, 30, V(0, 0, 0.5f), V(0, 0, -1),
                                         0.4f, 0.5f);
        collisionSolve(nullptr, &q);
        if (!finite3(q.outPosition) || !finite3(q.outVelocity))
            return fail(message, cap, "invalid geometry: non-finite output");
    }

    // 8. A fast fall must be caught by the swept-AABB gather, not tunnel. The
    //    actor starts high and falls at high speed; it must land within a few
    //    ticks and must never end up below the floor.
    {
        installQuad(V(-50, -50, 0), V(50, -50, 0), V(50, 50, 0), V(-50, 50, 0));
        CollisionSolveV1 q = makeCapsule(105, 50, V(0, 0, 5.0f), V(0, 0, -240.0f),
                                         0.4f, 0.5f);
        q.dt = 1.0f / 60.0f;
        bool landed = false;
        for (int i = 0; i < 12; ++i) {
            q.tick = 50ull + (std::uint64_t)i;
            if (i > 0)
                carryOver(q);
            collisionSolve(nullptr, &q);
            if (!q.handled)
                return fail(message, cap, "fast fall: solve declined");
            if (!finite3(q.outPosition) || !finite3(q.outVelocity))
                return fail(message, cap, "fast fall: non-finite output");
            if (q.outPosition[2] < 0.2f && !q.grounded)
                return fail(message, cap, "fast fall: tunneled through floor");
            if (q.grounded && q.outVelocity[2] >= -0.01f) {
                landed = true;
                break;
            }
        }
        if (!landed) {
            char detail[MIMITA_GAME_SELFTEST_MESSAGE];
            std::snprintf(detail, sizeof(detail),
                          "fast fall: no landing z=%.3f vz=%.2f g=%u wc=%u c=%u",
                          q.outPosition[2], q.outVelocity[2], q.grounded,
                          q.worldContact, q.contactCount);
            return fail(message, cap, detail);
        }
    }

    // 9. No world bound: the solve must decline instead of reporting a silent
    //    uncollided success (the old fall-through bug).
    {
        WorldTri none[1] = {{V(0, 0, 0), V(0, 0, 0), V(0, 0, 0)}};
        installWorld(none, 0);
        CollisionSolveV1 q = makeCapsule(106, 60, V(0, 0, 5.0f), V(0, 0, -10.0f),
                                         0.4f, 0.5f);
        collisionSolve(nullptr, &q);
        if (q.handled)
            return fail(message, cap, "no world: solver claimed handled");
    }

    // 10. Multiple body parts share one candidate set and resolve together.
    {
        installQuad(V(-20, -20, 0), V(20, -20, 0), V(20, 20, 0), V(-20, 20, 0));
        CollisionSolveV1 q = makeCapsule(104, 40, V(0, 0, 0.15f), V(0, 0, -3),
                                         0.4f, 0.5f);
        q.colliderCount = 4;
        const std::uint32_t parts[4] = {COLLISION_PART_HEAD, COLLISION_PART_TORSO,
                                        COLLISION_PART_LEFT_LEG,
                                        COLLISION_PART_RIGHT_LEG};
        for (int i = 1; i < 4; ++i) {
            CollisionColliderV1& c = q.colliders[i];
            c.partId = parts[i];
            c.shape = COLLISION_SHAPE_SPHERE;
            c.policyId = COLLISION_POLICY_BODY;
            c.radius = 0.2f;
            c.position[0] = 0.0f;
            c.position[1] = 0.0f;
            c.position[2] = 0.15f + (i - 1) * 0.18f;
        }
        collisionSolve(nullptr, &q);
        if (!q.handled || q.contactCount == 0)
            return fail(message, cap, "multi-part: no shared contacts");
        if (!finite3(q.outPosition))
            return fail(message, cap, "multi-part: non-finite output");
    }

    // 11. A resting capsule must stay grounded across repeated solves, with no
    //     flicker. The capsule torso centre is at halfHeight (0.9) so the bottom
    //     sample sits exactly at the floor: the tolerance must keep it grounded.
    {
        installQuad(V(-20, -20, 0), V(20, -20, 0), V(20, 20, 0), V(-20, 20, 0));
        CollisionSolveV1 q = makeCapsule(107, 70, V(0, 0, 0.9f), V(0, 0, 0),
                                         0.4f, 0.9f);
        for (int i = 0; i < 12; ++i) {
            q.tick = 70ull + (std::uint64_t)i;
            if (i > 0)
                carryOver(q);
            collisionSolve(nullptr, &q);
            if (!q.handled)
                return fail(message, cap, "resting: solve declined");
            if (!q.grounded || !q.worldContact)
                return fail(message, cap, "resting: grounding flickered off");
            if (!(q.outPosition[2] > 0.85f))
                return fail(message, cap, "resting: sank through floor");
        }
    }

    return true;
}

} // namespace HotCollisionPackage

#endif // MIMITA_GAME_DLL
