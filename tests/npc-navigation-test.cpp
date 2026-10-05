// 10 04 2026
/* purpose
* Focused, world-independent tests for automatic surface navigation:
* triangle classification (floor/ramp/too-steep/wall/ceiling) and the
* actor-preset navigation settings parser/defaults/clamps.
* Runs as a plain check() + exit-code harness (no gtest) and links only
* src/npc/npc-surface.cpp and src/npc/npc-navigation-settings.cpp.
* The live NpcNavigator route behavior is covered by the in-binary
* --npc-navigation-selftest; this file proves the pure contracts.
*/

#include "npc/npc-surface.h"
#include "npc/npc-navigation-settings.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

int gChecks = 0;

void check(bool condition, const char* message)
{
    ++gChecks;
    if (!condition) {
        std::printf("[npc-navigation-test] FAIL %s\n", message);
        std::exit(1);
    }
}

void testSurfaceClassification()
{
    const float dot = 0.80f;  // shared physics walkable-slope rule

    check(classifySurface(glm::vec3(0, 0, 1), dot) == NavSurfaceKind::Floor,
          "flat triangle is walkable floor");
    const glm::vec3 ramp(0.0f, std::sin(glm::radians(30.0f)), std::cos(glm::radians(30.0f)));
    check(classifySurface(ramp, dot) == NavSurfaceKind::Ramp,
          "ramp below the slope limit is walkable");
    const glm::vec3 steep(0.0f, std::sin(glm::radians(60.0f)), std::cos(glm::radians(60.0f)));
    check(classifySurface(steep, dot) == NavSurfaceKind::TooSteep,
          "surface above the slope limit is rejected");
    check(classifySurface(glm::vec3(1, 0, 0), dot) == NavSurfaceKind::TooSteep,
          "vertical wall is rejected as a walking surface");
    check(classifySurface(glm::vec3(0, 0, -1), dot) == NavSurfaceKind::Ceiling,
          "ceiling is rejected");

    check(isWalkableNormal(glm::vec3(0, 0, 1), dot), "flat normal is walkable");
    check(!isWalkableNormal(glm::vec3(0, 0, -1), dot), "downward normal is not walkable");
    check(!isWalkableNormal(glm::vec3(1, 0, 0), dot), "wall normal is not walkable");

    check(std::string(navSurfaceKindName(NavSurfaceKind::Ramp)) == "ramp", "kind name");
    check(std::string(navCapabilityName(NavCapability::Jump)) == "jump", "jump capability name");
    check(std::string(navCapabilityName(NavCapability::Teleport)) == "teleport",
          "teleport capability name");
}

void testSettingsDefaults()
{
    std::string error;
    NpcNavigationSettings s;
    const bool ok = parseNpcNavigationSettings(nlohmann::json::parse("{}"), s, error);
    check(ok, "empty navigation block parses");
    check(error.empty(), "no error on empty block");
    check(s.configured, "block is configured");
    check(s.mode == "automatic_surface_path", "default mode");
    check(s.maxWalkableSlopeDot == 0.0f, "slope dot is unset by default (shared rule)");
    check(s.automaticFromCollision, "automatic_from_collision defaults on");
    check(s.repathWhenBlocked, "repath_when_blocked defaults on");
    check(s.allowNavigationJumps, "navigation jumps default on");
    check(!s.allowWallJump, "wall jump defaults off");
    check(s.blockedBehavior == "turn_then_repath", "default blocked behavior");
}

void testSettingsCounterStrike()
{
    std::string error;
    NpcNavigationSettings s;
    const auto j = nlohmann::json::parse(R"({
        "mode": "automatic_surface_path",
        "max_walkable_slope_degrees": 45,
        "automatic_from_collision": true,
        "repath_when_blocked": true,
        "search_radius": 20.0,
        "allow_navigation_jumps": true,
        "allow_wall_jump": false,
        "blocked_behavior": "turn_then_repath"
    })");
    check(parseNpcNavigationSettings(j, s, error), "counter_strike navigation block parses");
    const float expected = std::cos(45.0f * 3.14159265358979323846f / 180.0f);
    check(std::fabs(s.maxWalkableSlopeDot - expected) < 1e-5f,
          "45 degrees converts to the walkable dot");
    check(std::fabs(s.searchRadius - 20.0f) < 1e-5f, "search_radius parsed");
    check(!s.allowWallJump, "allow_wall_jump false");
    check(s.blockedBehavior == "turn_then_repath", "blocked_behavior parsed");
}

void testSettingsRejectionAndClamp()
{
    std::string error;
    NpcNavigationSettings s;
    check(!parseNpcNavigationSettings(nlohmann::json::parse(R"({"mode":"global_graph"})"), s, error),
          "unknown mode is rejected");
    check(!parseNpcNavigationSettings(nlohmann::json::parse(R"({"blocked_behavior":"panic"})"), s, error),
          "unknown blocked_behavior is rejected");
    check(!parseNpcNavigationSettings(nlohmann::json::parse(R"({"allow_wall_jump":"yes"})"), s, error),
          "non-boolean allow_wall_jump is rejected");
    check(!parseNpcNavigationSettings(nlohmann::json::parse(R"("direct")"), s, error),
          "non-object navigation is rejected");

    check(parseNpcNavigationSettings(nlohmann::json::parse(R"({"search_radius":9999})"), s, error),
          "large search_radius parses");
    check(s.searchRadius == 20.0f, "search_radius clamps to 20");
    check(!s.warnings.empty(), "clamped search_radius reports a warning");
    check(parseNpcNavigationSettings(nlohmann::json::parse(R"({"search_radius":0.1})"), s, error),
          "tiny search_radius parses");
    check(s.searchRadius == 4.0f, "search_radius clamps to 4");
}

} // namespace

int main()
{
    testSurfaceClassification();
    testSettingsDefaults();
    testSettingsCounterStrike();
    testSettingsRejectionAndClamp();
    std::printf("[npc-navigation-test] PASS (%d checks)\n", gChecks);
    return 0;
}
