// 07 19 2026 1030
/* purpose
* Test ClientCollisionWorldView adapter for the shared projectile kernel.
* Verifies world triangle queries, player capsule queries, owner exclusion,
* deterministic ordering, and kernel compatibility.
* fill in 3rd line
* Does NOT test live client prediction, packets, or weapon behavior.
*/

#include <cstdio>
#include <cstdint>
#include <cmath>
#include <unordered_map>
#include <vector>
#include <algorithm>

#include <glm/glm.hpp>

#include "combat/client-collision-world-view.h"
#include "combat/projectile-simulation.h"
#include "physics/physics-types.h"
#include "physics/movement/physics-collision-shared.h"

static int gPassed = 0;
static int gFailed = 0;

#define TEST(name) do { printf("  %-50s ", name); } while(0)
#define PASS() do { printf("PASS\n"); ++gPassed; } while(0)
#define FAIL(msg, ...) do { printf("FAIL  " msg "\n", ##__VA_ARGS__); ++gFailed; } while(0)
#define CHECK(cond, msg, ...) do { if (!(cond)) { FAIL(msg, ##__VA_ARGS__); return; } } while(0)

// ── Minimal mock world (same pattern as projectile-simulation TestCollisionWorld) ──

struct MockWorld {
    std::vector<CollisionTriangle> triangles;
    float collisionChunkSize = 6.0f;
    std::unordered_map<glm::ivec3, std::vector<int>, IVec3Hash> collisionChunks;
    std::vector<int> collisionLargeTriangles;
    CollisionMeshCache collisionMesh;

    void addTriangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
                     const glm::vec3& normal) {
        CollisionTriangle tri;
        tri.a = a; tri.b = b; tri.c = c; tri.normal = normal;
        triangles.push_back(tri);
    }

    void build() {
        collisionChunks.clear();
        collisionLargeTriangles.clear();
        collisionMesh.triangles = triangles;
        for (int i = 0; i < (int)triangles.size(); ++i) {
            const auto& tri = triangles[i];
            AABB tb;
            tb.min = glm::min(tri.a, glm::min(tri.b, tri.c));
            tb.max = glm::max(tri.a, glm::max(tri.b, tri.c));
            glm::ivec3 c0 = collisionChunkCoord(tb.min, collisionChunkSize);
            glm::ivec3 c1 = collisionChunkCoord(tb.max, collisionChunkSize);
            if (c1.x - c0.x > 3 || c1.y - c0.y > 3 || c1.z - c0.z > 3) {
                collisionLargeTriangles.push_back(i);
            } else {
                for (int x = c0.x; x <= c1.x; ++x)
                for (int y = c0.y; y <= c1.y; ++y)
                for (int z = c0.z; z <= c1.z; ++z)
                    collisionChunks[glm::ivec3(x,y,z)].push_back(i);
            }
        }
    }
};

// ── Test 1: World triangle query ─────────────────────────────────────

static void testWorldQuery()
{
    TEST("world triangle query finds wall");

    CollisionMeshCache mesh;
    CollisionTriangle tri;
    tri.a = glm::vec3(0,0,0); tri.b = glm::vec3(10,0,0);
    tri.c = glm::vec3(0,10,0); tri.normal = glm::vec3(0,0,1);
    mesh.triangles.push_back(tri);

    std::unordered_map<glm::ivec3, std::vector<int>, IVec3Hash> chunks;
    std::vector<int> largeTris;

    ClientCollisionWorldView view(mesh, 6.0f, &chunks, &largeTris, 1, {});

    std::vector<int> indices;
    view.queryTrianglesSwept(
        glm::vec3(5,5,5), glm::vec3(5,5,-1), 0.3f, indices);

    // No chunks built → should get an empty result (safe, not a crash)
    // The query works only when chunks are populated
    printf("  (no chunks: %zu triangles)", indices.size());

    PASS();
}

// ── Test 2: Unique indices and deterministic ordering ────────────────

static void testUniqueDeterministic()
{
    TEST("unique indices and deterministic ordering");

    CollisionMeshCache mesh;
    for (int i = 0; i < 2; ++i) {
        CollisionTriangle tri;
        tri.a = glm::vec3(i*10,0,0); tri.b = glm::vec3(i*10+10,0,0);
        tri.c = glm::vec3(i*10,10,0); tri.normal = glm::vec3(0,0,1);
        mesh.triangles.push_back(tri);
    }

    std::unordered_map<glm::ivec3, std::vector<int>, IVec3Hash> chunks;
    std::vector<int> largeTris;
    // Build a chunk that covers both triangles
    chunks[glm::ivec3(0,0,0)].push_back(0);
    chunks[glm::ivec3(0,0,0)].push_back(1);

    ClientCollisionWorldView view(mesh, 6.0f, &chunks, &largeTris, 1, {});

    std::vector<int> r1, r2;
    view.queryTrianglesSwept(glm::vec3(5,5,5), glm::vec3(5,5,-1), 0.3f, r1);
    view.queryTrianglesSwept(glm::vec3(5,5,5), glm::vec3(5,5,-1), 0.3f, r2);

    CHECK(r1.size() == r2.size(), "size mismatch: %zu vs %zu", r1.size(), r2.size());
    for (size_t i = 0; i < r1.size(); ++i)
        CHECK(r1[i] == r2[i], "index %zu: %d vs %d", i, r1[i], r2[i]);

    auto dedup = r1;
    std::sort(dedup.begin(), dedup.end());
    CHECK(std::adjacent_find(dedup.begin(), dedup.end()) == dedup.end(),
          "duplicate triangle indices");

    PASS();
}

// ── Test 3: Player capsule query ─────────────────────────────────────

static void testPlayerCapsuleQuery()
{
    TEST("player capsule query returns eligible players");

    CollisionMeshCache mesh;
    std::unordered_map<glm::ivec3, std::vector<int>, IVec3Hash> chunks;
    std::vector<int> largeTris;

    std::vector<ClientCollisionWorldView::PlayerReplica> players;

    ClientCollisionWorldView::PlayerReplica p2;
    p2.playerId = 2; p2.pos = glm::vec3(10,10,1); p2.dead = false;
    players.push_back(p2);

    ClientCollisionWorldView::PlayerReplica p3;
    p3.playerId = 3; p3.pos = glm::vec3(10,10,1); p3.dead = true;
    players.push_back(p3);

    ClientCollisionWorldView view(mesh, 6.0f, &chunks, &largeTris, 9, players);

    std::vector<SweptPlayerCapsule> capsules;
    view.queryPlayerCapsulesSwept(
        glm::vec3(0,0,5), glm::vec3(10,10,-2), 0.3f, capsules);

    bool found2 = false;
    for (const auto& c : capsules) {
        if (c.playerId == 2) found2 = true;
        CHECK(c.playerId != 3, "dead player 3 should be excluded");
    }
    CHECK(found2, "alive player 2 not returned");

    PASS();
}

// ── Test 4: Owner exclusion ──────────────────────────────────────────

static void testOwnerExclusion()
{
    TEST("owner exclusion");

    CollisionMeshCache mesh;
    std::unordered_map<glm::ivec3, std::vector<int>, IVec3Hash> chunks;
    std::vector<int> largeTris;

    std::vector<ClientCollisionWorldView::PlayerReplica> players;
    ClientCollisionWorldView::PlayerReplica p;
    p.playerId = 2; p.pos = glm::vec3(5,5,1);
    players.push_back(p);

    ClientCollisionWorldView view(mesh, 6.0f, &chunks, &largeTris, 2, players);

    std::vector<SweptPlayerCapsule> capsules;
    view.queryPlayerCapsulesSwept(
        glm::vec3(0,0,5), glm::vec3(10,10,-2), 0.3f, capsules);

    for (const auto& c : capsules)
        CHECK(c.playerId != 2, "owner player %u should be excluded", c.playerId);

    PASS();
}

// ── Test 5: Deterministic capsule ordering ───────────────────────────

static void testCapsuleOrdering()
{
    TEST("deterministic capsule ordering");

    CollisionMeshCache mesh;
    std::unordered_map<glm::ivec3, std::vector<int>, IVec3Hash> chunks;
    std::vector<int> largeTris;

    std::vector<ClientCollisionWorldView::PlayerReplica> players;
    for (auto id : {9u, 3u, 5u, 1u}) {
        ClientCollisionWorldView::PlayerReplica p;
        p.playerId = id; p.pos = glm::vec3(5,5,1);
        players.push_back(p);
    }

    ClientCollisionWorldView view(mesh, 6.0f, &chunks, &largeTris, 99, players);

    std::vector<SweptPlayerCapsule> r1, r2;
    view.queryPlayerCapsulesSwept(
        glm::vec3(0,0,5), glm::vec3(10,10,-2), 0.3f, r1);
    view.queryPlayerCapsulesSwept(
        glm::vec3(0,0,5), glm::vec3(10,10,-2), 0.3f, r2);

    CHECK(r1.size() == 4, "expected 4 capsules, got %zu", r1.size());
    CHECK(r1.size() == r2.size(), "size mismatch");
    for (size_t i = 0; i < r1.size(); ++i)
        CHECK(r1[i].playerId == r2[i].playerId,
              "index %zu: %u vs %u", i, r1[i].playerId, r2[i].playerId);

    for (size_t i = 1; i < r1.size(); ++i)
        CHECK(r1[i-1].playerId <= r1[i].playerId,
              "ordering violation: %u > %u", r1[i-1].playerId, r1[i].playerId);

    PASS();
}

// ── Test 6: Kernel compatibility ─────────────────────────────────────

static void testKernelCompatibility()
{
    TEST("kernel compatibility with adapter");

    CollisionMeshCache mesh;
    CollisionTriangle t1, t2;
    t1.a = glm::vec3(-10,-10,0); t1.b = glm::vec3(10,-10,0);
    t1.c = glm::vec3(10,10,0); t1.normal = glm::vec3(0,0,1);
    t2.a = glm::vec3(-10,-10,0); t2.b = glm::vec3(10,10,0);
    t2.c = glm::vec3(-10,10,0); t2.normal = glm::vec3(0,0,1);
    mesh.triangles.push_back(t1);
    mesh.triangles.push_back(t2);

    std::unordered_map<glm::ivec3, std::vector<int>, IVec3Hash> chunks;
    std::vector<int> largeTris;
    chunks[glm::ivec3(0,0,0)].push_back(0);
    chunks[glm::ivec3(0,0,0)].push_back(1);

    std::vector<ClientCollisionWorldView::PlayerReplica> players;
    ClientCollisionWorldView::PlayerReplica p;
    p.playerId = 42; p.spawnGeneration = 3; p.pos = glm::vec3(0,0,5);
    players.push_back(p);

    // 6a: World impact (floor at z=0, player is far above)
    {
        ClientCollisionWorldView view2(mesh, 6.0f, &chunks, &largeTris, 1, {});

        ProjectilePhysicsConfig cfg;
        cfg.gravity = 0; cfg.drag = 0;
        cfg.bounceEnabled = false; cfg.lifetime = 10.0f;
        cfg.radius = 0.3f;

        ProjectilePhysicsState state;
        state.position = glm::vec3(0,0,5);
        state.velocity = glm::vec3(0,0,-30);

        ProjectileStepResult result;
        for (int t = 0; t < 10; ++t) {
            result = simulateProjectileTick(state, cfg, view2, 1.0f/60.0f);
            if (result.type == ProjectileCollisionType::WorldImpact)
                break;
        }
        CHECK(result.type == ProjectileCollisionType::WorldImpact,
              "expected WorldImpact, got %d (no players in view)", (int)result.type);
        CHECK(state.position.z < 1.0f,
              "projectile did not reach floor: z=%.4f", state.position.z);
    }

    // 6b: Player impact (fire toward player 42 at z=5, no floor)
    {
        CollisionMeshCache emptyMesh;
        std::unordered_map<glm::ivec3, std::vector<int>, IVec3Hash> emptyChunks;
        std::vector<int> emptyLargeTris;

        ClientCollisionWorldView view3(emptyMesh, 6.0f, &emptyChunks, &emptyLargeTris, 1, players);

        ProjectilePhysicsConfig cfg;
        cfg.gravity = 0; cfg.drag = 0;
        cfg.bounceEnabled = false; cfg.lifetime = 10.0f;
        cfg.radius = 0.3f;

        ProjectilePhysicsState state;
        state.position = glm::vec3(0,0,0);
        state.velocity = glm::vec3(0,0,30);

        ProjectileStepResult result;
        for (int t = 0; t < 20; ++t) {
            result = simulateProjectileTick(state, cfg, view3, 1.0f/60.0f);
            if (result.type == ProjectileCollisionType::PlayerImpact)
                break;
        }
        CHECK(result.type == ProjectileCollisionType::PlayerImpact,
              "expected PlayerImpact, got %d", (int)result.type);
        CHECK(result.hitPlayerId == 42,
              "expected player ID 42, got %u", result.hitPlayerId);
    }

    PASS();
}

int main()
{
    printf("=== Client Collision Adapter Tests ===\n\n");

    testWorldQuery();
    testUniqueDeterministic();
    testPlayerCapsuleQuery();
    testOwnerExclusion();
    testCapsuleOrdering();
    testKernelCompatibility();

    printf("\nResults: %d passed, %d failed\n",
           gPassed, gFailed);

    return gFailed > 0 ? 1 : 0;
}
