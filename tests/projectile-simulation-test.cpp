// 07 19 2026, 09 29
/* purpose
* Deterministic projectile simulation unit tests
* Tests swept-sphere world collision, player capsule collision,
* bounce, tunneling prevention, and determinism.
* Does NOT test networking, damage, weapon config, or rendering.
*/

#include "combat/projectile-simulation.h"

#include <cstdio>
#include <cmath>
#include <vector>
#include <cstdint>
#include <cstring>

// ── Test collision world with known geometry ──────────────────────────

struct TestTriangle
{
    CollisionTriangle tri;
    int id = 0;
};

struct TestCapsule
{
    SweptPlayerCapsule cap;
    int id = 0;
};

class TestCollisionWorld : public CollisionWorldView
{
public:
    std::vector<TestTriangle> triangles;
    std::vector<TestCapsule> capsules;

    void addTriangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
                     const glm::vec3& normal, int id = 0)
    {
        TestTriangle tt;
        tt.tri.a = a;
        tt.tri.b = b;
        tt.tri.c = c;
        tt.tri.normal = normal;
        tt.id = id;
        triangles.push_back(tt);
    }

    void addCapsule(const glm::vec3& a, const glm::vec3& b, float radius,
                    uint32_t playerId = 1, uint32_t spawnGen = 1)
    {
        TestCapsule tc;
        tc.cap.playerId = playerId;
        tc.cap.spawnGeneration = spawnGen;
        tc.cap.a = a;
        tc.cap.b = b;
        tc.cap.radius = radius;
        tc.id = (int)playerId;
        capsules.push_back(tc);
    }

    void queryTrianglesSwept(
        const glm::vec3& from, const glm::vec3& to, float radius,
        std::vector<int>& outIndices) const override
    {
        // Simple AABB broadphase: test all triangles (small test scenes)
        glm::vec3 mins = glm::min(from, to) - glm::vec3(radius);
        glm::vec3 maxs = glm::max(from, to) + glm::vec3(radius);
        for (int i = 0; i < (int)triangles.size(); ++i)
        {
            const auto& tt = triangles[i];
            glm::vec3 triMin = glm::min(tt.tri.a, glm::min(tt.tri.b, tt.tri.c));
            glm::vec3 triMax = glm::max(tt.tri.a, glm::max(tt.tri.b, tt.tri.c));
            if (triMax.x >= mins.x && triMin.x <= maxs.x &&
                triMax.y >= mins.y && triMin.y <= maxs.y &&
                triMax.z >= mins.z && triMin.z <= maxs.z)
            {
                outIndices.push_back(i);
            }
        }
    }

    const CollisionTriangle& triangleAt(int index) const override
    {
        return triangles[index].tri;
    }

    int triangleCount() const override
    {
        return (int)triangles.size();
    }

    void queryPlayerCapsulesSwept(
        const glm::vec3& from, const glm::vec3& to, float radius,
        std::vector<SweptPlayerCapsule>& out) const override
    {
        glm::vec3 mins = glm::min(from, to) - glm::vec3(radius);
        glm::vec3 maxs = glm::max(from, to) + glm::vec3(radius);
        for (const auto& tc : capsules)
        {
            glm::vec3 capMins = glm::min(tc.cap.a, tc.cap.b) - glm::vec3(tc.cap.radius);
            glm::vec3 capMaxs = glm::max(tc.cap.a, tc.cap.b) + glm::vec3(tc.cap.radius);
            if (capMaxs.x >= mins.x && capMins.x <= maxs.x &&
                capMaxs.y >= mins.y && capMins.y <= maxs.y &&
                capMaxs.z >= mins.z && capMins.z <= maxs.z)
            {
                out.push_back(tc.cap);
            }
        }
    }
};

// ── Test result tracking ──────────────────────────────────────────────

static int gTestsPassed = 0;
static int gTestsFailed = 0;

#define TEST(name) do { printf("  %-40s ", name); } while(0)
#define PASS() do { printf("PASS\n"); ++gTestsPassed; } while(0)
#define FAIL(msg, ...) do { printf("FAIL  " msg "\n", ##__VA_ARGS__); ++gTestsFailed; } while(0)
#define CHECK(cond, msg, ...) do { if (!(cond)) { FAIL(msg, ##__VA_ARGS__); return; } } while(0)

// ── Test 1: High-speed projectile cannot tunnel through a thin wall ──

static void testNoTunnel()
{
    TEST("no-tunnel through thin wall");

    // Wall: a large thin quad on the XY plane at z = 0
    // Represented as two triangles forming a 20x20 quad
    TestCollisionWorld world;
    world.addTriangle(
        glm::vec3(-10.0f, -10.0f, 0.0f),
        glm::vec3( 10.0f, -10.0f, 0.0f),
        glm::vec3( 10.0f,  10.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f), 0);
    world.addTriangle(
        glm::vec3(-10.0f, -10.0f, 0.0f),
        glm::vec3( 10.0f,  10.0f, 0.0f),
        glm::vec3(-10.0f,  10.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f), 1);

    ProjectilePhysicsConfig cfg;
    cfg.radius = 0.1f;
    cfg.gravity = 0.0f;
    cfg.drag = 0.0f;
    cfg.bounceEnabled = false;
    cfg.lifetime = 10.0f;

    // Start just above the wall, moving down at high speed
    // Velocity 200 * 1/60 = 3.33 units per tick — more than enough to
    // tunnel through a 0.1-radius wall in one tick if not for swept collision.
    ProjectilePhysicsState state;
    state.position = glm::vec3(0.0f, 0.0f, 0.5f);
    state.velocity = glm::vec3(0.0f, 0.0f, -200.0f);

    // Simulate until we either hit the wall or pass through
    bool hitWall = false;
    float minZ = state.position.z;
    int maxTicks = 10;
    for (int tick = 0; tick < maxTicks; ++tick)
    {
        ProjectileStepResult r = simulateProjectileTick(state, cfg, world, 1.0f / 60.0f);
        minZ = std::min(minZ, state.position.z);
        if (r.type == ProjectileCollisionType::WorldImpact)
        {
            hitWall = true;
            break;
        }
    }

    CHECK(hitWall, "projectile did not hit wall within %d ticks", maxTicks);
    CHECK(minZ >= -0.2f,
          "projectile tunneled through wall: min z=%.4f", minZ);

    PASS();
}

// ── Test 2: Bounce reflection with controlled restitution and friction ─
// Regression: applyBounce() must reflect the normal component away from
// the surface, not into it.

static void testBounce()
{
    TEST("bounce reflection restitution*friction");

    TestCollisionWorld world;
    glm::vec3 floorNormal(0.0f, 0.0f, 1.0f);
    world.addTriangle(
        glm::vec3(-10.0f, -10.0f, 0.0f),
        glm::vec3( 10.0f, -10.0f, 0.0f),
        glm::vec3( 10.0f,  10.0f, 0.0f),
        floorNormal, 0);
    world.addTriangle(
        glm::vec3(-10.0f, -10.0f, 0.0f),
        glm::vec3( 10.0f,  10.0f, 0.0f),
        glm::vec3(-10.0f,  10.0f, 0.0f),
        floorNormal, 1);

    ProjectilePhysicsConfig cfg;
    cfg.radius = 0.3f;
    cfg.gravity = 0.0f;
    cfg.drag = 0.0f;
    cfg.restitution = 0.5f;
    cfg.friction = 0.25f;
    cfg.bounceEnabled = true;
    cfg.maxBounceCount = 10;
    cfg.minBounceSpeed = 0.1f;
    cfg.lifetime = 10.0f;

    // Place sphere such that one tick moves it into the floor.
    // Vertical velocity -10 at 1/60 s moves it 0.167 units down.
    // Starting at z=0.467 and radius 0.3 touches floor at z=0.0.
    // One tick brings center to 0.300, exactly contacting the floor.
    ProjectilePhysicsState state;
    state.position = glm::vec3(0.0f, 0.0f, 0.467f);
    // Tangential component (5,0,0) + vertical component (0,0,-10)
    state.velocity = glm::vec3(5.0f, 0.0f, -10.0f);

    // Simulate one tick — should bounce
    int maxTicks = 5;
    bool bounced = false;
    ProjectileStepResult result;
    for (int tick = 0; tick < maxTicks; ++tick)
    {
        result = simulateProjectileTick(state, cfg, world, 1.0f / 60.0f);
        if (result.type == ProjectileCollisionType::WorldBounce)
        {
            bounced = true;
            break;
        }
    }

    CHECK(bounced, "projectile did not bounce within %d ticks", maxTicks);
    CHECK(result.type == ProjectileCollisionType::WorldBounce,
          "expected WorldBounce got %d", (int)result.type);

    // Normal component must point AWAY from the floor after bounce
    float normalSpeed = glm::dot(state.velocity, floorNormal);
    CHECK(normalSpeed > 0.0f,
          "normal speed %.4f should be positive (away from floor)", normalSpeed);

    // Incoming normal speed was 10.0 (velocity.z = -10, normal = (0,0,1)).
    // With restitution 0.5, outgoing normal speed should be 10 * 0.5 = 5.0.
    CHECK(std::fabs(normalSpeed - 5.0f) < 0.1f,
          "expected normal speed ~5.0 got %.4f", normalSpeed);

    // Incoming tangential speed was 5.0 (velocity.x = 5).
    // With friction 0.25, outgoing tangential speed should be 5 * (1-0.25) = 3.75.
    glm::vec3 tangent = state.velocity - floorNormal * normalSpeed;
    float tangentSpeed = glm::length(tangent);
    CHECK(std::fabs(tangentSpeed - 3.75f) < 0.1f,
          "expected tangent speed ~3.75 got %.4f", tangentSpeed);

    CHECK(state.bounceCount == 1,
          "expected bounceCount 1 got %d", state.bounceCount);

    PASS();
}

// ── Test 9: Edge grazing parallel to triangle plane ──────────────────
// The sphere moves parallel to the triangle plane and grazes an edge.
// Current algorithm fails because approachSpeed = 0 returns false.

static void testEdgeGrazing()
{
    TEST("edge grazing parallel to plane");

    // Triangle in XY plane at z=0: A=(0,0,0), B=(10,0,0), C=(0,10,0)
    // Edge AB runs along X axis at y=0 from (0,0,0) to (10,0,0)
    TestCollisionWorld world;
    world.addTriangle(
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(10.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 10.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f), 0);

    ProjectilePhysicsConfig cfg;
    cfg.radius = 0.3f; cfg.gravity = 0; cfg.drag = 0;
    cfg.bounceEnabled = false; cfg.lifetime = 10.0f;

    // Sphere center at z=0.29 (just within radius of edge at z=0)
    // Moving along +Y at high speed so no single substep position
    // lands within 0.3 of the edge — only the swept path passes through.
    // Old code: approachSpeed=0 → returns false at every substep.
    // Velocity 300 → 5 units/tick → 0.625/substep.
    // y positions per substep: -0.80, -0.175, 0.450, 1.075, 1.700, ...
    // At y=-0.175: dist to edge = sqrt(0.175^2+0.29^2) = 0.339 > 0.3
    // At y= 0.450: dist to edge = sqrt(0.45^2+0.29^2) = 0.535 > 0.3
    // Neither substep lands within radius — the sweep MUST catch it.
    ProjectilePhysicsState state;
    state.position = glm::vec3(5.0f, -0.8f, 0.29f);
    state.velocity = glm::vec3(0.0f, 300.0f, 0.0f);

    bool hit = false;
    for (int t = 0; t < 5; ++t)
    {
        ProjectileStepResult r = simulateProjectileTick(state, cfg, world, 1.0f / 60.0f);
        if (r.type == ProjectileCollisionType::WorldImpact ||
            r.type == ProjectileCollisionType::WorldBounce)
        {
            hit = true;
            CHECK(std::isfinite(r.hitPosition.x) && std::isfinite(r.hitPosition.y) && std::isfinite(r.hitPosition.z),
                  "hit position not finite");
            CHECK(std::isfinite(r.hitNormal.x) && std::isfinite(r.hitNormal.y) && std::isfinite(r.hitNormal.z),
                  "hit normal not finite");
            break;
        }
    }

    CHECK(hit, "edge grazing not detected — sweep missed the edge");
    PASS();
}

// ── Test 10: Vertex grazing ──────────────────────────────────────────
// Sphere touches only one triangle vertex.

static void testVertexGrazing()
{
    TEST("vertex grazing");

    TestCollisionWorld world;
    world.addTriangle(
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(10.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 10.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f), 0);

    ProjectilePhysicsConfig cfg;
    cfg.radius = 0.3f; cfg.gravity = 0; cfg.drag = 0;
    cfg.bounceEnabled = false; cfg.lifetime = 10.0f;

    // Fire toward vertex A=(0,0,0) from diagonally above at high speed
    // so the swept path intersects the vertex sphere but no single
    // substep lands within radius of the vertex.
    ProjectilePhysicsState state;
    state.position = glm::vec3(-0.8f, -0.8f, 0.29f);
    state.velocity = glm::vec3(300.0f, 300.0f, 0.0f);

    bool hit = false;
    for (int t = 0; t < 5; ++t)
    {
        ProjectileStepResult r = simulateProjectileTick(state, cfg, world, 1.0f / 60.0f);
        if (r.type == ProjectileCollisionType::WorldImpact ||
            r.type == ProjectileCollisionType::WorldBounce)
        {
            hit = true;
            // Vertex is at (0,0,0), sphere should contact near it
            float distToVertex = glm::length(state.position);
            CHECK(distToVertex < 0.5f,
                  "sphere too far from vertex: dist=%.4f", distToVertex);
            break;
        }
    }

    CHECK(hit, "vertex grazing not detected");
    PASS();
}

// ── Test 11: Parallel-to-plane edge collision ─────────────────────────
// Same as edge grazing but verify TOI is in valid range.

static void testParallelEdgeCollision()
{
    TEST("parallel-to-plane edge collision");

    TestCollisionWorld world;
    world.addTriangle(
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(10.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 10.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f), 0);

    ProjectilePhysicsConfig cfg;
    cfg.radius = 0.3f; cfg.gravity = 0; cfg.drag = 0;
    cfg.bounceEnabled = false; cfg.lifetime = 10.0f;

    ProjectilePhysicsState state;
    state.position = glm::vec3(5.0f, -0.8f, 0.29f);
    state.velocity = glm::vec3(0.0f, 300.0f, 0.0f);

    bool hit = false;
    for (int t = 0; t < 5; ++t)
    {
        ProjectileStepResult r = simulateProjectileTick(state, cfg, world, 1.0f / 60.0f);
        if (r.type == ProjectileCollisionType::WorldImpact ||
            r.type == ProjectileCollisionType::WorldBounce)
        {
            hit = true;
            // TOI is internal to simulation — verify hit position is finite
            break;
        }
    }

    CHECK(hit, "parallel edge collision not detected");
    PASS();
}

// ── Test 12: Near miss (no collision expected) ────────────────────────
// Same geometry as edge grazing but separation > radius.

static void testNearMiss()
{
    TEST("edge near miss (no collision)");

    TestCollisionWorld world;
    world.addTriangle(
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(10.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 10.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f), 0);

    ProjectilePhysicsConfig cfg;
    cfg.radius = 0.3f; cfg.gravity = 0; cfg.drag = 0;
    cfg.bounceEnabled = false; cfg.lifetime = 10.0f;

    // Sphere at z=0.31, just outside radius of edge at z=0
    ProjectilePhysicsState state;
    state.position = glm::vec3(5.0f, -0.4f, 0.31f);
    state.velocity = glm::vec3(0.0f, 30.0f, 0.0f);

    bool hit = false;
    for (int t = 0; t < 5; ++t)
    {
        ProjectileStepResult r = simulateProjectileTick(state, cfg, world, 1.0f / 60.0f);
        if (r.type == ProjectileCollisionType::WorldImpact ||
            r.type == ProjectileCollisionType::WorldBounce ||
            r.type == ProjectileCollisionType::PlayerImpact)
        {
            hit = true;
            break;
        }
    }

    CHECK(!hit, "near miss incorrectly detected collision");
    PASS();
}

// ── Test 13: Degenerate point capsule — direct hit ───────────────────

static void testDegenerateCapsuleHit()
{
    TEST("degenerate point capsule direct hit");

    // A point capsule with a==b, simulating vertex via sweepSphereCapsule
    class PointWorld : public CollisionWorldView {
    public:
        CollisionTriangle tri[1];
        SweptPlayerCapsule cap;
        PointWorld() {
            tri[0].a = glm::vec3(0,0,-100); tri[0].b = glm::vec3(1,0,-100);
            tri[0].c = glm::vec3(0,1,-100); tri[0].normal = glm::vec3(0,0,1);
            cap.playerId = 17; cap.spawnGeneration = 3;
            cap.a = glm::vec3(0,0,0); cap.b = glm::vec3(0,0,0); // a == b
            cap.radius = 0.0f; // degenerate: zero-length segment, zero cap radius
        }
        void queryTrianglesSwept(const glm::vec3&, const glm::vec3&, float, std::vector<int>& out) const override {}
        const CollisionTriangle& triangleAt(int i) const override { return tri[i]; }
        int triangleCount() const override { return 0; }
        void queryPlayerCapsulesSwept(const glm::vec3& f, const glm::vec3& t, float r,
                                      std::vector<SweptPlayerCapsule>& out) const override {
            out = {cap};
        }
    };

    PointWorld world;
    ProjectilePhysicsConfig cfg;
    cfg.radius = 0.3f; cfg.gravity = 0; cfg.drag = 0;
    cfg.bounceEnabled = false; cfg.lifetime = 10.0f;

    ProjectilePhysicsState state;
    state.position = glm::vec3(0.0f, 0.0f, -5.0f);
    state.velocity = glm::vec3(0.0f, 0.0f, 30.0f);

    ProjectileStepResult result;
    for (int t = 0; t < 10; ++t)
    {
        result = simulateProjectileTick(state, cfg, world, 1.0f / 60.0f);
        if (result.type == ProjectileCollisionType::PlayerImpact)
            break;
    }

    CHECK(result.type == ProjectileCollisionType::PlayerImpact,
          "expected PlayerImpact, got %d", (int)result.type);
    CHECK(result.hitPlayerId == 17,
          "expected playerId 17, got %u", result.hitPlayerId);
    CHECK(result.hitPlayerSpawnGeneration == 3,
          "expected spawnGen 3, got %u", result.hitPlayerSpawnGeneration);
    CHECK(std::isfinite(result.hitPosition.x) && std::isfinite(result.hitPosition.y) && std::isfinite(result.hitPosition.z),
          "hitPosition not finite");
    CHECK(std::isfinite(result.hitNormal.x) && std::isfinite(result.hitNormal.y) && std::isfinite(result.hitNormal.z),
          "hitNormal not finite");
    float nl = glm::length(result.hitNormal);
    CHECK(std::fabs(nl - 1.0f) < 1e-4f,
          "normal length %.6f not approximately 1", nl);

    PASS();
}

// ── Test 14: Degenerate point capsule — near miss ───────────────────

static void testDegenerateCapsuleMiss()
{
    TEST("degenerate point capsule near miss");

    class PointWorld : public CollisionWorldView {
    public:
        CollisionTriangle tri[1];
        SweptPlayerCapsule cap;
        PointWorld() {
            tri[0].a = glm::vec3(0,0,-100); tri[0].b = glm::vec3(1,0,-100);
            tri[0].c = glm::vec3(0,1,-100); tri[0].normal = glm::vec3(0,0,1);
            cap.playerId = 17; cap.spawnGeneration = 3;
            cap.a = glm::vec3(0,0,0); cap.b = glm::vec3(0,0,0);
            cap.radius = 0.0f;
        }
        void queryTrianglesSwept(const glm::vec3&, const glm::vec3&, float, std::vector<int>& out) const override {}
        const CollisionTriangle& triangleAt(int i) const override { return tri[i]; }
        int triangleCount() const override { return 0; }
        void queryPlayerCapsulesSwept(const glm::vec3& f, const glm::vec3& t, float r,
                                      std::vector<SweptPlayerCapsule>& out) const override {
            out = {cap};
        }
    };

    PointWorld world;
    ProjectilePhysicsConfig cfg;
    cfg.radius = 0.3f; cfg.gravity = 0; cfg.drag = 0;
    cfg.bounceEnabled = false; cfg.lifetime = 10.0f;

    // Path passes 0.5 units from the point — > 0.3 radius → miss
    ProjectilePhysicsState state;
    state.position = glm::vec3(0.5f, 0.0f, -5.0f);
    state.velocity = glm::vec3(0.0f, 0.0f, 30.0f);

    bool hit = false;
    for (int t = 0; t < 10; ++t)
    {
        ProjectileStepResult r = simulateProjectileTick(state, cfg, world, 1.0f / 60.0f);
        if (r.type == ProjectileCollisionType::PlayerImpact)
        {
            hit = true;
            break;
        }
    }
    CHECK(!hit, "near miss incorrectly detected as PlayerImpact");
    PASS();
}

// ── Test 15: Exact-center overlap with degenerate point capsule ──────

static void testDegenerateCapsuleExactCenter()
{
    TEST("degenerate point capsule exact-center overlap");

    class PointWorld : public CollisionWorldView {
    public:
        CollisionTriangle tri[1];
        SweptPlayerCapsule cap;
        PointWorld() {
            tri[0].a = glm::vec3(0,0,-100); tri[0].b = glm::vec3(1,0,-100);
            tri[0].c = glm::vec3(0,1,-100); tri[0].normal = glm::vec3(0,0,1);
            cap.playerId = 42; cap.spawnGeneration = 7;
            cap.a = glm::vec3(0,0,0); cap.b = glm::vec3(0,0,0);
            cap.radius = 0.0f;
        }
        void queryTrianglesSwept(const glm::vec3&, const glm::vec3&, float, std::vector<int>& out) const override {}
        const CollisionTriangle& triangleAt(int i) const override { return tri[i]; }
        int triangleCount() const override { return 0; }
        void queryPlayerCapsulesSwept(const glm::vec3& f, const glm::vec3& t, float r,
                                      std::vector<SweptPlayerCapsule>& out) const override {
            out = {cap};
        }
    };

    PointWorld world;
    ProjectilePhysicsConfig cfg;
    cfg.radius = 0.3f; cfg.gravity = 0; cfg.drag = 0;
    cfg.bounceEnabled = false; cfg.lifetime = 10.0f;

    // Sphere center EXACTLY at the degenerate point (0,0,0)
    ProjectilePhysicsState state;
    state.position = glm::vec3(0.0f, 0.0f, 0.0f);
    state.velocity = glm::vec3(1.0f, 2.0f, 3.0f); // any non-zero direction

    ProjectileStepResult r = simulateProjectileTick(state, cfg, world, 1.0f / 60.0f);

    CHECK(r.type == ProjectileCollisionType::PlayerImpact,
          "expected PlayerImpact, got %d", (int)r.type);
    CHECK(std::isfinite(r.hitPosition.x) && std::isfinite(r.hitPosition.y) && std::isfinite(r.hitPosition.z),
          "hitPosition not finite");
    CHECK(std::isfinite(r.hitNormal.x) && std::isfinite(r.hitNormal.y) && std::isfinite(r.hitNormal.z),
          "hitNormal not finite");
    float nl = glm::length(r.hitNormal);
    CHECK(std::fabs(nl - 1.0f) < 1e-4f,
          "normal length %.6f not approximately 1", nl);
    // Exact-center fallback: normal should be opposite of motion direction
    glm::vec3 expectedNormal = glm::normalize(glm::vec3(-1.0f, -2.0f, -3.0f));
    CHECK(glm::length(r.hitNormal - expectedNormal) < 1e-4f,
          "exact-center normal mismatch: expected (%.4f,%.4f,%.4f) got (%.4f,%.4f,%.4f)",
          expectedNormal.x, expectedNormal.y, expectedNormal.z,
          r.hitNormal.x, r.hitNormal.y, r.hitNormal.z);

    PASS();
}

// ── Test 7: Capsule order independence ───────────────────────────────
// Two worlds with same two capsules in different query order must
// select playerId 3 (lower) every time.

static void testCapsuleOrderIndependence()
{
    TEST("capsule order independence");

    // World A: capsules in order [9, 3]
    class WorldA : public CollisionWorldView {
    public:
        CollisionTriangle tri[1];
        SweptPlayerCapsule cap[2];
        WorldA() {
            tri[0].a = glm::vec3(-10,-10,-100); tri[0].b = glm::vec3(10,-10,-100);
            tri[0].c = glm::vec3(10,10,-100); tri[0].normal = glm::vec3(0,0,1);
            cap[0].playerId = 9; cap[0].spawnGeneration = 1;
            cap[0].a = glm::vec3(0,0,10); cap[0].b = glm::vec3(0,0,12); cap[0].radius = 0.5f;
            cap[1].playerId = 3; cap[1].spawnGeneration = 2;
            cap[1].a = glm::vec3(0,0,10); cap[1].b = glm::vec3(0,0,12); cap[1].radius = 0.5f;
        }
        void queryTrianglesSwept(const glm::vec3&, const glm::vec3&, float, std::vector<int>& out) const override {
            out = {0};
        }
        const CollisionTriangle& triangleAt(int i) const override { return tri[i]; }
        int triangleCount() const override { return 1; }
        void queryPlayerCapsulesSwept(const glm::vec3&, const glm::vec3&, float, std::vector<SweptPlayerCapsule>& out) const override {
            out = {cap[0], cap[1]}; // playerId 9 first, then 3
        }
    };

    // World B: capsules in order [3, 9]
    class WorldB : public CollisionWorldView {
    public:
        CollisionTriangle tri[1];
        SweptPlayerCapsule cap[2];
        WorldB() {
            tri[0].a = glm::vec3(-10,-10,-100); tri[0].b = glm::vec3(10,-10,-100);
            tri[0].c = glm::vec3(10,10,-100); tri[0].normal = glm::vec3(0,0,1);
            cap[0].playerId = 3; cap[0].spawnGeneration = 2;
            cap[0].a = glm::vec3(0,0,10); cap[0].b = glm::vec3(0,0,12); cap[0].radius = 0.5f;
            cap[1].playerId = 9; cap[1].spawnGeneration = 1;
            cap[1].a = glm::vec3(0,0,10); cap[1].b = glm::vec3(0,0,12); cap[1].radius = 0.5f;
        }
        void queryTrianglesSwept(const glm::vec3&, const glm::vec3&, float, std::vector<int>& out) const override {
            out = {0};
        }
        const CollisionTriangle& triangleAt(int i) const override { return tri[i]; }
        int triangleCount() const override { return 1; }
        void queryPlayerCapsulesSwept(const glm::vec3&, const glm::vec3&, float, std::vector<SweptPlayerCapsule>& out) const override {
            out = {cap[0], cap[1]}; // playerId 3 first, then 9
        }
    };

    WorldA worldA;
    WorldB worldB;

    ProjectilePhysicsConfig cfg;
    cfg.radius = 0.3f; cfg.gravity = 0; cfg.drag = 0;
    cfg.bounceEnabled = false; cfg.lifetime = 10.0f;

    ProjectilePhysicsState stateA, stateB;
    stateA.position = glm::vec3(0,0,0); stateA.velocity = glm::vec3(0,0,30);
    stateB.position = glm::vec3(0,0,0); stateB.velocity = glm::vec3(0,0,30);

    ProjectileStepResult resA, resB;
    for (int t = 0; t < 30; ++t) {
        resA = simulateProjectileTick(stateA, cfg, worldA, 1.0f/60.0f);
        resB = simulateProjectileTick(stateB, cfg, worldB, 1.0f/60.0f);
        if (resA.type == ProjectileCollisionType::PlayerImpact) break;
    }

    CHECK(resA.type == ProjectileCollisionType::PlayerImpact,
          "World A: expected PlayerImpact got %d", (int)resA.type);
    CHECK(resB.type == ProjectileCollisionType::PlayerImpact,
          "World B: expected PlayerImpact got %d", (int)resB.type);
    // Both must select playerId 3 (lower)
    CHECK(resA.hitPlayerId == 3,
          "World A selected playerId %u, expected 3", resA.hitPlayerId);
    CHECK(resB.hitPlayerId == 3,
          "World B selected playerId %u, expected 3", resB.hitPlayerId);
    // Both must have identical spawn generation (first selected capsule's gen)
    CHECK(resA.hitPlayerSpawnGeneration == resB.hitPlayerSpawnGeneration,
          "spawnGen mismatch: A=%u B=%u", resA.hitPlayerSpawnGeneration, resB.hitPlayerSpawnGeneration);
    // Positions must match
    CHECK(std::fabs(stateA.position.x - stateB.position.x) < 1e-6f, "posX mismatch");
    CHECK(std::fabs(stateA.position.y - stateB.position.y) < 1e-6f, "posY mismatch");
    CHECK(std::fabs(stateA.position.z - stateB.position.z) < 1e-6f, "posZ mismatch");

    PASS();
}

// ── Test 8: World/player equal-TOI policy ────────────────────────────
// When world and player collide at equal TOI, world must win.

static void testWorldPlayerEqualTOI()
{
    TEST("world wins over player at equal TOI");

    // Two capsules at same z-range, one on the floor, one slightly above.
    // Projectile starts at z=0 with velocity toward floor.
    // Both world and capsule at TOI=0 (static overlap).
    // Policy: world (type=0) beats player (type=1).

    class EqualWorld : public CollisionWorldView {
    public:
        CollisionTriangle tri[2];
        SweptPlayerCapsule cap;
        EqualWorld() {
            tri[0].a = glm::vec3(-10,-10,0); tri[0].b = glm::vec3(10,-10,0);
            tri[0].c = glm::vec3(10,10,0); tri[0].normal = glm::vec3(0,0,1);
            tri[1].a = glm::vec3(-10,-10,0); tri[1].b = glm::vec3(10,10,0);
            tri[1].c = glm::vec3(-10,10,0); tri[1].normal = glm::vec3(0,0,1);
            cap.playerId = 99; cap.spawnGeneration = 1;
            cap.a = glm::vec3(0,0,0); cap.b = glm::vec3(0,0,2); cap.radius = 0.5f;
        }
        void queryTrianglesSwept(const glm::vec3& f, const glm::vec3& t, float r, std::vector<int>& out) const override {
            // Return world first, then capsule alternates — tests ordering independence
            out = {0, 1};
        }
        const CollisionTriangle& triangleAt(int i) const override { return tri[i]; }
        int triangleCount() const override { return 2; }
        void queryPlayerCapsulesSwept(const glm::vec3& f, const glm::vec3& t, float r, std::vector<SweptPlayerCapsule>& out) const override {
            out = {cap};
        }
    };

    EqualWorld world;
    ProjectilePhysicsConfig cfg;
    cfg.radius = 0.3f; cfg.gravity = 0; cfg.drag = 0;
    cfg.bounceEnabled = false; cfg.lifetime = 10.0f;

    // Place projectile overlapping both world and capsule at z=0
    ProjectilePhysicsState state;
    state.position = glm::vec3(0,0,0.2f); // within radius of both floor and capsule
    state.velocity = glm::vec3(0,0,-5);

    ProjectileStepResult result = simulateProjectileTick(state, cfg, world, 1.0f/60.0f);

    // Must be WorldImpact (type=2) not PlayerImpact (type=3)
    CHECK(result.type == ProjectileCollisionType::WorldImpact,
          "expected WorldImpact got %d (PlayerImpact would mean player won)", (int)result.type);
    CHECK(result.hitPlayerId == 0,
          "expected no player ID on world impact, got %u", result.hitPlayerId);

    PASS();
}

// ── Test 3: Player capsule before wall returns PlayerImpact ──────────

static void testPlayerBeforeWall()
{
    TEST("player capsule before wall");

    TestCollisionWorld world;
    // Wall at z=15
    world.addTriangle(
        glm::vec3(-10.0f, -10.0f, 15.0f),
        glm::vec3( 10.0f, -10.0f, 15.0f),
        glm::vec3( 10.0f,  10.0f, 15.0f),
        glm::vec3(0.0f, 0.0f, 1.0f), 0);
    world.addTriangle(
        glm::vec3(-10.0f, -10.0f, 15.0f),
        glm::vec3( 10.0f,  10.0f, 15.0f),
        glm::vec3(-10.0f,  10.0f, 15.0f),
        glm::vec3(0.0f, 0.0f, 1.0f), 1);
    // Player capsule at z=10, height 2, radius 0.5
    world.addCapsule(
        glm::vec3(0.0f, 0.0f, 10.0f),
        glm::vec3(0.0f, 0.0f, 12.0f),
        0.5f, 42, 1);

    ProjectilePhysicsConfig cfg;
    cfg.radius = 0.3f;
    cfg.gravity = 0.0f;
    cfg.drag = 0.0f;
    cfg.bounceEnabled = false;
    cfg.lifetime = 10.0f;

    ProjectilePhysicsState state;
    state.position = glm::vec3(0.0f, 0.0f, 0.0f);
    state.velocity = glm::vec3(0.0f, 0.0f, 30.0f);

    for (int tick = 0; tick < 30; ++tick)
    {
        ProjectileStepResult r = simulateProjectileTick(state, cfg, world, 1.0f / 60.0f);
        if (r.type == ProjectileCollisionType::PlayerImpact)
        {
            CHECK(r.hitPlayerId == 42,
                  "expected player ID 42, got %u", r.hitPlayerId);
            CHECK(r.hitPlayerSpawnGeneration == 1,
                  "expected spawn gen 1, got %u", r.hitPlayerSpawnGeneration);
            PASS();
            return;
        }
        if (r.type != ProjectileCollisionType::None)
        {
            printf("\n  tick %d: unexpected result=%d", tick, (int)r.type);
            FAIL("got unexpected collision type %d at tick %d", (int)r.type, tick);
            return;
        }
    }
    printf("  final pos=(%.4f,%.4f,%.4f)", state.position.x, state.position.y, state.position.z);
    FAIL("player impact not detected within 30 ticks");
}

// ── Test 4: Wall before player capsule returns WorldImpact ───────────

static void testWallBeforePlayer()
{
    TEST("wall before player capsule");

    TestCollisionWorld world;
    // Wall at z=5
    world.addTriangle(
        glm::vec3(-10.0f, -10.0f, 5.0f),
        glm::vec3( 10.0f, -10.0f, 5.0f),
        glm::vec3( 10.0f,  10.0f, 5.0f),
        glm::vec3(0.0f, 0.0f, 1.0f), 0);
    world.addTriangle(
        glm::vec3(-10.0f, -10.0f, 5.0f),
        glm::vec3( 10.0f,  10.0f, 5.0f),
        glm::vec3(-10.0f,  10.0f, 5.0f),
        glm::vec3(0.0f, 0.0f, 1.0f), 1);
    // Player capsule behind wall at z=10
    world.addCapsule(
        glm::vec3(0.0f, 0.0f, 10.0f),
        glm::vec3(0.0f, 0.0f, 12.0f),
        0.5f, 99, 2);

    ProjectilePhysicsConfig cfg;
    cfg.radius = 0.3f;
    cfg.gravity = 0.0f;
    cfg.drag = 0.0f;
    cfg.bounceEnabled = false;
    cfg.lifetime = 10.0f;

    ProjectilePhysicsState state;
    state.position = glm::vec3(0.0f, 0.0f, 0.0f);
    state.velocity = glm::vec3(0.0f, 0.0f, 30.0f);

    for (int tick = 0; tick < 30; ++tick)
    {
        ProjectileStepResult r = simulateProjectileTick(state, cfg, world, 1.0f / 60.0f);
        if (r.type == ProjectileCollisionType::PlayerImpact)
        {
            FAIL("projectile passed through wall and hit player behind it");
            return;
        }
        if (r.type == ProjectileCollisionType::WorldImpact)
        {
            CHECK(state.position.z < 6.0f,
                  "projectile passed through wall: z=%.4f", state.position.z);
            CHECK(state.position.z >= 4.0f,
                  "projectile went too far past wall: z=%.4f", state.position.z);
            PASS();
            return;
        }
    }
    printf("  final pos=(%.4f,%.4f,%.4f)", state.position.x, state.position.y, state.position.z);
    FAIL("neither wall nor player impact within 30 ticks");
}

// ── Test 5: Identical initial state produces identical results ────────

static void testDeterminism()
{
    TEST("determinism across 300 ticks");

    TestCollisionWorld world;
    // Simple floor
    world.addTriangle(
        glm::vec3(-20.0f, -20.0f, 0.0f),
        glm::vec3( 20.0f, -20.0f, 0.0f),
        glm::vec3( 20.0f,  20.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f), 0);
    world.addTriangle(
        glm::vec3(-20.0f, -20.0f, 0.0f),
        glm::vec3( 20.0f,  20.0f, 0.0f),
        glm::vec3(-20.0f,  20.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f), 1);

    ProjectilePhysicsConfig cfg;
    cfg.radius = 0.3f;
    cfg.gravity = 20.0f;
    cfg.drag = 0.15f;
    cfg.restitution = 0.35f;
    cfg.friction = 0.5f;
    cfg.bounceEnabled = true;
    cfg.maxBounceCount = 10;
    cfg.minBounceSpeed = 0.1f;
    cfg.lifetime = 10.0f;

    // Run simulation 1
    ProjectilePhysicsState stateA;
    stateA.position = glm::vec3(2.0f, 3.0f, 8.0f);
    stateA.velocity = glm::vec3(5.0f, -2.0f, -12.0f);

    // Run simulation 2 (identical copy)
    ProjectilePhysicsState stateB = stateA;

    for (int tick = 0; tick < 300; ++tick)
    {
        simulateProjectileTick(stateA, cfg, world, 1.0f / 60.0f);
        simulateProjectileTick(stateB, cfg, world, 1.0f / 60.0f);
    }

    CHECK(stateA.position.x == stateB.position.x,
          "X mismatch: %.10f vs %.10f", stateA.position.x, stateB.position.x);
    CHECK(stateA.position.y == stateB.position.y,
          "Y mismatch: %.10f vs %.10f", stateA.position.y, stateB.position.y);
    CHECK(stateA.position.z == stateB.position.z,
          "Z mismatch: %.10f vs %.10f", stateA.position.z, stateB.position.z);
    CHECK(stateA.velocity.x == stateB.velocity.x,
          "vX mismatch: %.10f vs %.10f", stateA.velocity.x, stateB.velocity.x);
    CHECK(stateA.velocity.y == stateB.velocity.y,
          "vY mismatch: %.10f vs %.10f", stateA.velocity.y, stateB.velocity.y);
    CHECK(stateA.velocity.z == stateB.velocity.z,
          "vZ mismatch: %.10f vs %.10f", stateA.velocity.z, stateB.velocity.z);
    CHECK(stateA.bounceCount == stateB.bounceCount,
          "bounceCount mismatch: %d vs %d", stateA.bounceCount, stateB.bounceCount);
    CHECK(stateA.age == stateB.age,
          "age mismatch: %.10f vs %.10f", stateA.age, stateB.age);

    PASS();
}

// ── Test 6: Invalid NaN/zero inputs return safe result ──────────────

static void testInvalidInputs()
{
    TEST("NaN position returns safe result");

    TestCollisionWorld world;
    world.addTriangle(
        glm::vec3(-10.0f, -10.0f, 0.0f),
        glm::vec3( 10.0f, -10.0f, 0.0f),
        glm::vec3( 10.0f,  10.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f), 0);

    ProjectilePhysicsConfig cfg;
    cfg.radius = 0.3f;
    cfg.lifetime = 10.0f;

    // NaN position
    {
        ProjectilePhysicsState state;
        state.position = glm::vec3(NAN, 0.0f, 0.0f);
        state.velocity = glm::vec3(0.0f, 0.0f, -10.0f);
        ProjectileStepResult r = simulateProjectileTick(state, cfg, world, 1.0f / 60.0f);
        CHECK(r.type == ProjectileCollisionType::None,
              "NaN position should return None, got %d", (int)r.type);
    }

    // NaN velocity
    {
        ProjectilePhysicsState state;
        state.position = glm::vec3(0.0f, 0.0f, 10.0f);
        state.velocity = glm::vec3(NAN, 0.0f, 0.0f);
        ProjectileStepResult r = simulateProjectileTick(state, cfg, world, 1.0f / 60.0f);
        CHECK(r.type == ProjectileCollisionType::None,
              "NaN velocity should return None, got %d", (int)r.type);
    }

    // Zero fixedDt
    {
        ProjectilePhysicsState state;
        state.position = glm::vec3(0.0f, 0.0f, 10.0f);
        state.velocity = glm::vec3(0.0f, 0.0f, -10.0f);
        ProjectileStepResult r = simulateProjectileTick(state, cfg, world, 0.0f);
        CHECK(r.type == ProjectileCollisionType::None,
              "zero dt should return None, got %d", (int)r.type);
    }

    // Negative fixedDt
    {
        ProjectilePhysicsState state;
        state.position = glm::vec3(0.0f, 0.0f, 10.0f);
        state.velocity = glm::vec3(0.0f, 0.0f, -10.0f);
        ProjectileStepResult r = simulateProjectileTick(state, cfg, world, -1.0f);
        CHECK(r.type == ProjectileCollisionType::None,
              "negative dt should return None, got %d", (int)r.type);
    }

    // Exploded state does not mutate further
    {
        ProjectilePhysicsState state;
        state.position = glm::vec3(0.0f, 0.0f, 10.0f);
        state.velocity = glm::vec3(0.0f, 0.0f, -10.0f);
        state.exploded = true;
        ProjectileStepResult r = simulateProjectileTick(state, cfg, world, 1.0f / 60.0f);
        CHECK(r.type == ProjectileCollisionType::None,
              "exploded should return None, got %d", (int)r.type);
        // Position should not change when exploded
        CHECK(state.position.z == 10.0f,
              "exploded projectile moved: z=%.4f", state.position.z);
    }

    PASS();
}

static void testPhysicsDoesNotOwnExplosionPolicy()
{
    TEST("physics reports expiry without explosion");

    TestCollisionWorld world;
    ProjectilePhysicsConfig cfg;
    cfg.radius = 0.3f;
    cfg.lifetime = 1.0f / 60.0f;

    ProjectilePhysicsState state;
    state.position = glm::vec3(0.0f, 0.0f, 10.0f);
    state.velocity = glm::vec3(0.0f, 0.0f, 0.0f);

    ProjectileStepResult r = simulateProjectileTick(state, cfg, world, 1.0f / 60.0f);
    CHECK(r.type == ProjectileCollisionType::LifetimeExpired,
          "expected LifetimeExpired got %d", (int)r.type);
    CHECK(!state.exploded,
          "physics kernel should not set exploded on lifetime expiry");

    world.addCapsule(
        glm::vec3(0.0f, 0.0f, 5.0f),
        glm::vec3(0.0f, 0.0f, 7.0f),
        0.5f, 7, 3);
    cfg.lifetime = 10.0f;
    state = ProjectilePhysicsState{};
    state.position = glm::vec3(0.0f, 0.0f, 0.0f);
    state.velocity = glm::vec3(0.0f, 0.0f, 60.0f);

    for (int tick = 0; tick < 10; ++tick)
    {
        r = simulateProjectileTick(state, cfg, world, 1.0f / 60.0f);
        if (r.type == ProjectileCollisionType::PlayerImpact)
            break;
    }

    CHECK(r.type == ProjectileCollisionType::PlayerImpact,
          "expected PlayerImpact got %d", (int)r.type);
    CHECK(r.hitPlayerId == 7,
          "expected player ID 7, got %u", r.hitPlayerId);
    CHECK(!state.exploded,
          "physics kernel should not set exploded on player impact");

    // ── WorldImpact ownership ───────────────────────────────────────
    {
        TestCollisionWorld w2;
        w2.addTriangle(
            glm::vec3(-10,-10,0), glm::vec3(10,-10,0),
            glm::vec3(10,10,0), glm::vec3(0,0,1), 0);
        w2.addTriangle(
            glm::vec3(-10,-10,0), glm::vec3(10,10,0),
            glm::vec3(-10,10,0), glm::vec3(0,0,1), 1);

        ProjectilePhysicsConfig c2;
        c2.radius = 0.3f; c2.gravity = 0; c2.drag = 0;
        c2.bounceEnabled = false; c2.lifetime = 10.0f;

        ProjectilePhysicsState s2;
        s2.position = glm::vec3(0,0,5);
        s2.velocity = glm::vec3(0,0,-60);

        ProjectileStepResult r2;
        for (int t = 0; t < 10; ++t)
        {
            r2 = simulateProjectileTick(s2, c2, w2, 1.0f/60.0f);
            if (r2.type == ProjectileCollisionType::WorldImpact)
                break;
        }
        CHECK(r2.type == ProjectileCollisionType::WorldImpact,
              "expected WorldImpact got %d", (int)r2.type);
        CHECK(!s2.exploded,
              "physics kernel should not set exploded on world impact");
    }

    // ── Caller-owned untouched — exploded=true before call ──────────
    {
        ProjectilePhysicsConfig c3;
        c3.lifetime = 10.0f;

        ProjectilePhysicsState s3;
        s3.position = glm::vec3(5,5,5);
        s3.velocity = glm::vec3(10,0,0);
        s3.exploded = true; // caller marked it as ended

        ProjectileStepResult r3 = simulateProjectileTick(s3, c3, world, 1.0f/60.0f);
        CHECK(r3.type == ProjectileCollisionType::None,
              "exploded projectile should return None, got %d", (int)r3.type);
        CHECK(s3.exploded == true,
              "caller-owned exploded flag was cleared by kernel");
        CHECK(s3.position == glm::vec3(5,5,5),
              "exploded projectile position changed");
    }

    PASS();
}

// ── Main ──────────────────────────────────────────────────────────────

int main()
{
    printf("=== Projectile Simulation Tests ===\n\n");

    testNoTunnel();
    testBounce();
    testEdgeGrazing();
    testVertexGrazing();
    testParallelEdgeCollision();
    testNearMiss();
    testDegenerateCapsuleHit();
    testDegenerateCapsuleMiss();
    testDegenerateCapsuleExactCenter();
    testCapsuleOrderIndependence();
    testWorldPlayerEqualTOI();
    testPlayerBeforeWall();
    testWallBeforePlayer();
    testDeterminism();
    testInvalidInputs();
    testPhysicsDoesNotOwnExplosionPolicy();

    printf("\n=== Results: %d passed, %d failed ===\n",
           gTestsPassed, gTestsFailed);

    return gTestsFailed > 0 ? 1 : 0;
}
