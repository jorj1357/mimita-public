#include <cstdio>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include <glm/glm.hpp>
#include "physics/config.h"
#include "world/world.h"
#include "entities/player.h"
#include "map/map-loader-collision.h"
#include "physics/movement/physics-collision.h"
#include "physics/movement/physics-collision-shared.h"
#include "debug/debug-log.h"

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

struct CollisionStressResult
{
    std::string summary;
    float maxPenetration = 0.0f;
    glm::vec3 finalPos{0.0f};
    bool grounded = false;
    bool finite = true;
    bool emergency = false;
};

static CollisionStressResult runCollisionStressCase(const std::string& caseName)
{
    World world;
    addStressFloor(world);

    float speed = 60.0f;
    std::string selected = caseName.empty() ? "wedge5" : caseName;
    glm::vec3 startPos(-4.0f, 0.0f, PLAYER_HEIGHT * 0.5f + 0.05f);
    glm::vec3 startVel(speed, 0.0f, 0.0f);

    if (selected == "floor")
    {
        speed = 18.0f;
        startVel = glm::vec3(speed, 0.0f, 0.0f);
    }
    else if (selected == "floor_drop")
    {
        startPos = glm::vec3(0.0f, 0.0f, 8.0f);
        startVel = glm::vec3(0.0f, 0.0f, -180.0f);
    }
    else if (selected == "corner")
    {
        addStressQuad(world,
            {1.0f, -5.0f, 0.0f},
            {1.0f, -5.0f, 4.0f},
            {1.0f,  5.0f, 4.0f},
            {1.0f,  5.0f, 0.0f});
        addStressQuad(world,
            {-5.0f, 1.0f, 0.0f},
            { 5.0f, 1.0f, 0.0f},
            { 5.0f, 1.0f, 4.0f},
            {-5.0f, 1.0f, 4.0f});
        startPos = glm::vec3(-4.0f, -4.0f, PLAYER_HEIGHT * 0.5f + 0.05f);
        startVel = glm::normalize(glm::vec3(1.0f, 1.0f, 0.0f)) * 60.0f;
    }
    else if (selected == "cone")
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

    Player testPlayer(false);
    testPlayer.pos = startPos;
    testPlayer.vel = startVel;
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

    CollisionStressResult result;
    result.maxPenetration = maxPen;
    result.finalPos = testPlayer.pos;
    result.grounded = grounded;
    result.finite = isFiniteVec3(testPlayer.pos) && isFiniteVec3(testPlayer.vel);
    result.emergency = gLastCollisionTrace.emergencyEscaped;

    char buf[1024];
    std::snprintf(buf, sizeof(buf),
        "[COLLISION STRESS] case=%s ticks=%d final=(%.2f %.2f %.2f) vel=(%.2f %.2f %.2f) contacts=%zu maxPen=%.4f grounded=%d %s",
        selected.c_str(), TICKS,
        testPlayer.pos.x, testPlayer.pos.y, testPlayer.pos.z,
        testPlayer.vel.x, testPlayer.vel.y, testPlayer.vel.z,
        contacts.size(), maxPen, (int)grounded,
        collisionLastTraceSummary().c_str());
    result.summary = std::string(buf);
    return result;
}

std::string collisionStressRun(const std::string& caseName)
{
    return runCollisionStressCase(caseName).summary;
}

bool collisionStressSelfTest(std::string* outSummary)
{
    const char* cases[] = {
        "floor",
        "floor_drop",
        "corner",
        "wedge1",
        "wedge5",
        "wedge10",
        "wedge20",
        "cone",
        "dash"
    };

    bool ok = true;
    std::string summary;
    for (const char* c : cases)
    {
        CollisionStressResult result = runCollisionStressCase(c);
        const bool caseOk =
            result.finite &&
            result.finalPos.z > -0.05f &&
            result.maxPenetration <= 0.04f &&
            !result.emergency;
        ok = ok && caseOk;

        summary += caseOk ? "PASS " : "FAIL ";
        summary += result.summary;
        summary += "\n";
    }

    if (outSummary)
        *outSummary = summary;
    return ok;
}

// Verifies the collision sub-grid broadphase returns exactly the same triangle
// set as the previous whole-chunk iteration (via brute-force overlap scan), and
// that a small query near a dense cluster touches a small fraction of triangles.
bool collisionSubGridSelfTest(std::string* outSummary)
{
    std::string summary;
    bool ok = true;
    auto check = [&](bool cond, const char* name) {
        summary += cond ? "  PASS: " : "  FAIL: ";
        summary += name;
        summary += "\n";
        if (!cond) ok = false;
    };

    // Dense cluster of tiny triangles packed into one 6-unit chunk (~9600 tris).
    World world;
    world.collisionChunkSize = 6.0f;
    constexpr int GRID = 40;
    for (int x = 0; x < GRID; ++x)
    for (int y = 0; y < GRID; ++y)
    for (int z = 0; z < 3; ++z)
    {
        glm::vec3 p0(x * 0.1f, y * 0.1f, z * 1.0f);
        CollisionTriangle tri;
        tri.a = p0;
        tri.b = glm::vec3(p0.x + 0.1f, p0.y, p0.z);
        tri.c = glm::vec3(p0.x, p0.y + 0.1f, p0.z);
        tri.normal = glm::vec3(0.0f, 0.0f, 1.0f);
        world.collisionMesh.triangles.push_back(tri);
    }

    buildCollisionChunks(world, nullptr);

    check(!world.collisionChunks.empty(), "chunks built");
    check(!world.collisionSubGrids.empty(), "subgrids built");
    check(!world.collisionMesh.triangles.empty(), "dense triangles present");

    // Brute-force reference using the same overlap filter as the broadphase.
    auto bruteForce = [&](const AABB& q, float expansion) {
        std::vector<int> out;
        for (int i = 0; i < (int)world.collisionMesh.triangles.size(); ++i) {
            AABB tb = makeTriangleAABB(world.collisionMesh.triangles[i]);
            tb.min -= glm::vec3(expansion);
            tb.max += glm::vec3(expansion);
            if (overlaps(q, tb))
                out.push_back(i);
        }
        return out;
    };

    const float expansion = 0.1f;
    const AABB queries[] = {
        AABB{{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
        AABB{{-0.5f, -0.5f, -0.5f}, {2.0f, 2.0f, 2.0f}},
        AABB{{100.0f, 100.0f, 100.0f}, {101.0f, 101.0f, 101.0f}},
        AABB{{-1.0f, -1.0f, -1.0f}, {5.0f, 5.0f, 4.0f}}
    };

    for (int i = 0; i < 4; ++i) {
        std::vector<int> expected = bruteForce(queries[i], expansion);
        std::vector<int> got;
        appendChunkTrianglesForAABB(world, queries[i], expansion, got, "subgridSelftest");
        std::sort(expected.begin(), expected.end());
        std::sort(got.begin(), got.end());
        bool sameSet = (expected == got);
        char name[128];
        std::snprintf(name, sizeof(name),
            "query%d candidate set matches brute-force (%zu vs %zu)",
            i + 1, got.size(), expected.size());
        check(sameSet, name);
    }

    {
        std::vector<int> small;
        appendChunkTrianglesForAABB(world, queries[0], expansion, small, "subgridSelftestSmall");
        const float ratio = (float)small.size() / (float)world.collisionMesh.triangles.size();
        char name[128];
        std::snprintf(name, sizeof(name),
            "small query returns %zu/%zu tris (%.2f%% of world)",
            small.size(), world.collisionMesh.triangles.size(), ratio * 100.0f);
        check(!small.empty() && ratio < 0.5f, name);
    }

    if (outSummary)
        *outSummary = summary;
    return ok;
}
