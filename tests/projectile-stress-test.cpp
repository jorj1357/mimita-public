// 07 19 2026, 11 05
/* purpose
* Focused projectile kernel stress benchmark for the 60 Hz server budget.
* Derives representative projectile load from current player cap and projectile config.
* Reports timing distributions, candidate counts, and sleeping grenade behavior.
* Does NOT open sockets, render, send packets, or test client reconciliation.
* Does NOT tune weapon gameplay values or alter projectile physics behavior.
* Does NOT promise scaling beyond the reported configured supported bound.
*/

#include "combat/projectile-simulation.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace {

constexpr int kMaxPlayers = 32;
constexpr float kServerDt = 1.0f / 60.0f;
constexpr float kTickBudgetMs = 16.6667f;
constexpr float kRocketFireDelay = 0.65f;
constexpr float kRocketLifetime = 5.0f;
constexpr float kGrenadeFireDelay = 0.60f;
constexpr float kGrenadeLifetime = 3.0f;

struct BenchResult
{
    const char* name = "";
    int projectiles = 0;
    double avgMs = 0.0;
    double p95Ms = 0.0;
    double maxMs = 0.0;
    uint64_t triangleQueries = 0;
    uint64_t triangleCandidates = 0;
    uint32_t triangleMax = 0;
    uint64_t capsuleCandidates = 0;
    uint32_t capsuleMax = 0;
};

class StressWorld final : public CollisionWorldView
{
public:
    std::vector<CollisionTriangle> triangles;
    std::vector<SweptPlayerCapsule> capsules;

    struct CellKey
    {
        int x = 0;
        int y = 0;
        int z = 0;

        bool operator==(const CellKey& other) const
        {
            return x == other.x && y == other.y && z == other.z;
        }
    };

    struct CellHash
    {
        size_t operator()(const CellKey& key) const
        {
            uint32_t h = 2166136261u;
            h = (h ^ (uint32_t)key.x) * 16777619u;
            h = (h ^ (uint32_t)key.y) * 16777619u;
            h = (h ^ (uint32_t)key.z) * 16777619u;
            return h;
        }
    };

    std::unordered_map<CellKey, std::vector<int>, CellHash> cells;
    mutable std::vector<uint32_t> seen;
    mutable uint32_t generation = 0;

    StressWorld()
    {
        for (int x = -10; x < 10; ++x)
        for (int y = -10; y < 10; ++y)
        {
            const float x0 = (float)x * 2.0f;
            const float y0 = (float)y * 2.0f;
            const float x1 = x0 + 2.0f;
            const float y1 = y0 + 2.0f;
            addTri({x0, y0, 0.0f}, {x1, y0, 0.0f}, {x1, y1, 0.0f});
            addTri({x0, y0, 0.0f}, {x1, y1, 0.0f}, {x0, y1, 0.0f});
        }

        buildGrid();

        for (int i = 0; i < kMaxPlayers; ++i)
        {
            SweptPlayerCapsule cap;
            cap.playerId = (uint32_t)(i + 1);
            cap.spawnGeneration = 1;
            cap.a = glm::vec3(-8.0f + (float)i, 4.0f, 0.65f);
            cap.b = glm::vec3(-8.0f + (float)i, 4.0f, 2.85f);
            cap.radius = 0.65f;
            capsules.push_back(cap);
        }
    }

    void queryTrianglesSwept(const glm::vec3& from,
                             const glm::vec3& to,
                             float radius,
                             std::vector<int>& outIndices) const override
    {
        const glm::vec3 mins = glm::min(from, to) - glm::vec3(radius);
        const glm::vec3 maxs = glm::max(from, to) + glm::vec3(radius);
        ++generation;
        if (generation == 0)
        {
            std::fill(seen.begin(), seen.end(), 0);
            generation = 1;
        }
        const CellKey c0 = cellFor(mins);
        const CellKey c1 = cellFor(maxs);
        for (int x = c0.x; x <= c1.x; ++x)
        for (int y = c0.y; y <= c1.y; ++y)
        for (int z = c0.z; z <= c1.z; ++z)
        {
            auto it = cells.find(CellKey{x, y, z});
            if (it == cells.end())
                continue;
            for (int triIndex : it->second)
            {
                if (seen[(size_t)triIndex] == generation)
                    continue;
                seen[(size_t)triIndex] = generation;
                const CollisionTriangle& tri = triangles[(size_t)triIndex];
                const glm::vec3 triMin = glm::min(tri.a, glm::min(tri.b, tri.c));
                const glm::vec3 triMax = glm::max(tri.a, glm::max(tri.b, tri.c));
                if (triMax.x >= mins.x && triMin.x <= maxs.x &&
                    triMax.y >= mins.y && triMin.y <= maxs.y &&
                    triMax.z >= mins.z && triMin.z <= maxs.z)
                {
                    outIndices.push_back(triIndex);
                }
            }
        }
        std::sort(outIndices.begin(), outIndices.end());
    }

    const CollisionTriangle& triangleAt(int index) const override
    {
        return triangles[(size_t)index];
    }

    int triangleCount() const override
    {
        return (int)triangles.size();
    }

    void queryPlayerCapsulesSwept(const glm::vec3& from,
                                  const glm::vec3& to,
                                  float radius,
                                  std::vector<SweptPlayerCapsule>& out) const override
    {
        const glm::vec3 mins = glm::min(from, to) - glm::vec3(radius);
        const glm::vec3 maxs = glm::max(from, to) + glm::vec3(radius);
        for (const SweptPlayerCapsule& cap : capsules)
        {
            const glm::vec3 capMin = glm::min(cap.a, cap.b) - glm::vec3(cap.radius);
            const glm::vec3 capMax = glm::max(cap.a, cap.b) + glm::vec3(cap.radius);
            if (capMax.x >= mins.x && capMin.x <= maxs.x &&
                capMax.y >= mins.y && capMin.y <= maxs.y &&
                capMax.z >= mins.z && capMin.z <= maxs.z)
            {
                out.push_back(cap);
            }
        }
    }

private:
    void addTri(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
    {
        CollisionTriangle tri;
        tri.a = a;
        tri.b = b;
        tri.c = c;
        tri.normal = glm::vec3(0.0f, 0.0f, 1.0f);
        triangles.push_back(tri);
    }

    CellKey cellFor(const glm::vec3& p) const
    {
        constexpr float kCellSize = 6.0f;
        return CellKey{
            (int)std::floor(p.x / kCellSize),
            (int)std::floor(p.y / kCellSize),
            (int)std::floor(p.z / kCellSize)
        };
    }

    void buildGrid()
    {
        for (int i = 0; i < (int)triangles.size(); ++i)
        {
            const CollisionTriangle& tri = triangles[(size_t)i];
            const glm::vec3 triMin = glm::min(tri.a, glm::min(tri.b, tri.c));
            const glm::vec3 triMax = glm::max(tri.a, glm::max(tri.b, tri.c));
            const CellKey c0 = cellFor(triMin);
            const CellKey c1 = cellFor(triMax);
            for (int x = c0.x; x <= c1.x; ++x)
            for (int y = c0.y; y <= c1.y; ++y)
            for (int z = c0.z; z <= c1.z; ++z)
                cells[CellKey{x, y, z}].push_back(i);
        }
        seen.assign(triangles.size(), 0);
    }
};

ProjectilePhysicsConfig grenadeConfig()
{
    ProjectilePhysicsConfig cfg;
    cfg.radius = 0.3f;
    cfg.lifetime = kGrenadeLifetime;
    cfg.gravity = 20.0f;
    cfg.drag = 0.15f;
    cfg.angularDrag = 0.3f;
    cfg.restitution = 0.35f;
    cfg.friction = 0.5f;
    cfg.maxBounceCount = 10;
    cfg.minBounceSpeed = 0.1f;
    cfg.bounceEnabled = true;
    return cfg;
}

std::vector<ProjectilePhysicsState> makeProjectiles(int count, bool sleeping)
{
    std::vector<ProjectilePhysicsState> out;
    out.reserve((size_t)count);
    for (int i = 0; i < count; ++i)
    {
        ProjectilePhysicsState state;
        state.position = glm::vec3(-9.0f + (float)(i % 18), -8.0f + (float)((i / 18) % 10), sleeping ? 0.31f : 5.0f);
        state.velocity = sleeping ? glm::vec3(0.0f) : glm::vec3(18.0f, 3.0f, 4.0f);
        state.angularVelocity = sleeping ? glm::vec3(0.0f) : glm::vec3(0.0f, 6.0f, 0.0f);
        state.sleeping = sleeping;
        out.push_back(state);
    }
    return out;
}

BenchResult runCase(const char* name, int projectileCount, bool sleeping)
{
    StressWorld world;
    ProjectilePhysicsConfig cfg = grenadeConfig();
    std::vector<ProjectilePhysicsState> projectiles = makeProjectiles(projectileCount, sleeping);
    std::vector<double> tickMs;
    tickMs.reserve(120);

    BenchResult result;
    result.name = name;
    result.projectiles = projectileCount;

    for (int tick = 0; tick < 120; ++tick)
    {
        const auto start = std::chrono::steady_clock::now();
        for (ProjectilePhysicsState& projectile : projectiles)
        {
            ProjectileStepResult step = simulateProjectileTick(projectile, cfg, world, kServerDt);
            result.triangleQueries += step.triangleQueryCount;
            result.triangleCandidates += step.triangleCandidateTotal;
            result.triangleMax = std::max(result.triangleMax, step.triangleCandidateMax);
            result.capsuleCandidates += step.playerCapsuleCandidateTotal;
            result.capsuleMax = std::max(result.capsuleMax, step.playerCapsuleCandidateMax);
        }
        const double ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - start).count();
        tickMs.push_back(ms);
        result.avgMs += ms;
        result.maxMs = std::max(result.maxMs, ms);
    }

    result.avgMs /= (double)tickMs.size();
    std::sort(tickMs.begin(), tickMs.end());
    result.p95Ms = tickMs[(size_t)std::ceil((double)tickMs.size() * 0.95) - 1];
    return result;
}

bool report(const BenchResult& r)
{
    printf("%-28s projectiles=%4d avg=%.3fms p95=%.3fms max=%.3fms "
           "triQueries=%llu triCandidates=%llu triMax=%u capsuleCandidates=%llu capsuleMax=%u\n",
           r.name, r.projectiles, r.avgMs, r.p95Ms, r.maxMs,
           (unsigned long long)r.triangleQueries,
           (unsigned long long)r.triangleCandidates,
           r.triangleMax,
           (unsigned long long)r.capsuleCandidates,
           r.capsuleMax);
    return r.maxMs <= kTickBudgetMs;
}

} // namespace

int main()
{
    const int rocketPerPlayer = (int)std::ceil(kRocketLifetime / kRocketFireDelay);
    const int grenadePerPlayer = (int)std::ceil(kGrenadeLifetime / kGrenadeFireDelay);
    const int supportedProjectiles = kMaxPlayers * std::max(rocketPerPlayer, grenadePerPlayer);

    printf("=== Projectile Stress Benchmark ===\n");
    printf("derivedBound maxPlayers=%d rocketPerPlayer=%d grenadePerPlayer=%d supportedProjectiles=%d budget=%.4fms\n",
           kMaxPlayers, rocketPerPlayer, grenadePerPlayer,
           supportedProjectiles, kTickBudgetMs);

    bool ok = true;
    ok = report(runCase("zero baseline", 0, false)) && ok;
    ok = report(runCase("one projectile", 1, false)) && ok;
    ok = report(runCase("full-match moving", supportedProjectiles, false)) && ok;
    ok = report(runCase("full-match sleeping", supportedProjectiles, true)) && ok;

    if (!ok)
    {
        printf("FAIL projectile stress exceeded fixed 60 Hz tick budget\n");
        return 1;
    }

    printf("PASS projectile stress stayed within fixed 60 Hz tick budget\n");
    return 0;
}
