// 2026-10-06
// MiMITA-owned boundary around Recast/Detour. The external types stay out of
// gameplay and movement code; this backend only answers navigation queries.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

struct World;

struct NavigationAgentProfile
{
    float radius = 0.45f;
    float height = 1.8f;
    float stepHeight = 0.65f;
    float maxSlopeDegrees = 53.0f;
};

struct RecastNavigationResult
{
    bool available = false;
    bool success = false;
    std::uint64_t navmeshVersion = 0;
    std::string failure;
    std::vector<glm::vec3> points;
    float pathLength = 0.0f;
    int polygonCount = 0;
    double queryMilliseconds = 0.0;
    double buildMilliseconds = 0.0;

    // Build diagnostics. MiMITA is Z-up while Recast expects Y-up; the adapter
    // converts geometry into Recast space. These values prove the walkable
    // surface actually exists after conversion instead of trusting the bake.
    std::string coordinateSystem;
    int sourceTriangleCount = 0;
    int walkableTriangleCount = 0;
    int navMeshPolyCount = 0;
    int navMeshVertCount = 0;
    glm::vec3 recastBoundsMin{0.0f};
    glm::vec3 recastBoundsMax{0.0f};

    // Query diagnostics. These distinguish the first failing stage instead of
    // collapsing every failure into "path_not_found".
    bool startPolyFound = false;
    bool destPolyFound = false;
    std::uint64_t startPolyRef = 0;
    std::uint64_t destPolyRef = 0;
    glm::vec3 nearestStart{0.0f};
    glm::vec3 nearestDest{0.0f};
    // How far the requested point was projected onto the navmesh. A large
    // value means the goal was off the surface; the contract forbids hiding
    // that, so it is always reported.
    float startProjectionDistance = 0.0f;
    float destProjectionDistance = 0.0f;
};

class RecastNavigationBackend
{
public:
    static RecastNavigationBackend& instance();

    // Build/load the current world representation before gameplay begins.
    // This keeps the first expensive Recast bake out of a fixed simulation
    // tick. During the compare phase no route is authoritative.
    RecastNavigationResult prepare(const World& world,
                                   const NavigationAgentProfile& profile);

    RecastNavigationResult query(const World& world,
                                 const glm::vec3& start,
                                 const glm::vec3& destination,
                                 const NavigationAgentProfile& profile);

    void invalidate();

private:
    RecastNavigationBackend() = default;
    RecastNavigationBackend(const RecastNavigationBackend&) = delete;
    RecastNavigationBackend& operator=(const RecastNavigationBackend&) = delete;

    struct State;
    State* mState = nullptr;
};
