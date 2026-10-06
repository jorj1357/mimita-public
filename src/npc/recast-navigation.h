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
};

class RecastNavigationBackend
{
public:
    static RecastNavigationBackend& instance();

    // Build/load the current world representation before gameplay begins.
    // This keeps the first expensive Recast bake out of a fixed simulation
    // tick. The returned query fields are diagnostic; no route is authoritative
    // during the compare phase.
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
