// 10 04 2026
// Pure navigation-surface classification and capability naming.
#include "npc/npc-surface.h"

#include <cmath>

namespace {
// A normal at least this upward is treated as flat floor rather than a ramp.
constexpr float kFloorNormalZ = 0.995f;
} // namespace

const char* navSurfaceKindName(NavSurfaceKind kind)
{
    switch (kind) {
        case NavSurfaceKind::Floor:    return "floor";
        case NavSurfaceKind::Ramp:     return "ramp";
        case NavSurfaceKind::Step:     return "step";
        case NavSurfaceKind::TooSteep: return "too_steep";
        case NavSurfaceKind::Ceiling:  return "ceiling";
    }
    return "floor";
}

const char* navCapabilityName(NavCapability capability)
{
    switch (capability) {
        case NavCapability::Walk:     return "walk";
        case NavCapability::Jump:     return "jump";
        case NavCapability::Drop:     return "drop";
        case NavCapability::Crawl:    return "crawl";
        case NavCapability::Fly:      return "fly";
        case NavCapability::Roll:     return "roll";
        case NavCapability::Teleport: return "teleport";
    }
    return "walk";
}

bool isWalkableNormal(const glm::vec3& normal, float walkableDot)
{
    return normal.z >= walkableDot;
}

NavSurfaceKind classifySurface(const glm::vec3& normal, float walkableDot)
{
    if (normal.z < -0.5f)
        return NavSurfaceKind::Ceiling;
    if (normal.z < walkableDot)
        return NavSurfaceKind::TooSteep;
    if (normal.z >= kFloorNormalZ)
        return NavSurfaceKind::Floor;
    return NavSurfaceKind::Ramp;
}

bool npcSurfaceSelfTest(std::string& report)
{
    bool ok = true;
    auto fail = [&](const std::string& why) { ok = false; report += "FAIL: " + why + "\n"; };

    const float walkableDot = 0.80f;  // shared physics rule

    // Flat floor.
    if (classifySurface(glm::vec3(0.0f, 0.0f, 1.0f), walkableDot) != NavSurfaceKind::Floor)
        fail("flat upward normal should be Floor");
    // Ramp below the limit (45 degrees -> dot ~0.707 would be too steep; use 30 deg).
    const glm::vec3 ramp(0.0f, std::sin(glm::radians(30.0f)), std::cos(glm::radians(30.0f)));
    if (classifySurface(ramp, walkableDot) != NavSurfaceKind::Ramp)
        fail("30-degree ramp should be Ramp");
    // Surface above the slope limit.
    const glm::vec3 steep(0.0f, std::sin(glm::radians(60.0f)), std::cos(glm::radians(60.0f)));
    if (classifySurface(steep, walkableDot) != NavSurfaceKind::TooSteep)
        fail("60-degree face should be TooSteep");
    // Vertical wall.
    if (classifySurface(glm::vec3(1.0f, 0.0f, 0.0f), walkableDot) != NavSurfaceKind::TooSteep)
        fail("vertical wall should be TooSteep");
    // Ceiling.
    if (classifySurface(glm::vec3(0.0f, 0.0f, -1.0f), walkableDot) != NavSurfaceKind::Ceiling)
        fail("downward normal should be Ceiling");

    if (!isWalkableNormal(glm::vec3(0.0f, 0.0f, 1.0f), walkableDot))
        fail("flat surface should be walkable");
    if (isWalkableNormal(glm::vec3(0.0f, 0.0f, -1.0f), walkableDot))
        fail("ceiling should not be walkable");

    if (std::string(navCapabilityName(NavCapability::Jump)) != "jump")
        fail("capability name mismatch");

    report += ok ? "PASS\n" : "FAIL\n";
    return ok;
}
