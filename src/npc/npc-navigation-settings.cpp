// 10 04 2026
/* purpose
* Implements the actor-preset navigation settings schema, validation, and the
* degree -> walkable-slope-dot conversion. Strict on enum strings and booleans
* (reject => caller keeps the last valid preset), clamping on numeric ranges.
* No file, world, or NPC dependency.
*/

#include "npc/npc-navigation-settings.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace {

bool inSet(const std::string& value, std::initializer_list<const char*> allowed)
{
    for (const char* candidate : allowed)
        if (value == candidate) return true;
    return false;
}

bool readString(const nlohmann::json& j, const char* key, std::string& out,
                std::initializer_list<const char*> allowed, std::string& error)
{
    if (!j.contains(key)) return true;
    if (!j[key].is_string()) {
        error = std::string(key) + " must be a string";
        return false;
    }
    const std::string value = j[key].get<std::string>();
    if (!inSet(value, allowed)) {
        error = std::string(key) + " has unknown value \"" + value + "\"";
        return false;
    }
    out = value;
    return true;
}

bool readBool(const nlohmann::json& j, const char* key, bool& out, std::string& error)
{
    if (!j.contains(key)) return true;
    if (!j[key].is_boolean()) {
        error = std::string(key) + " must be a boolean";
        return false;
    }
    out = j[key].get<bool>();
    return true;
}

bool readClampedFloat(const nlohmann::json& j, const char* key, float lo, float hi,
                      float& out, std::string& error, std::string& warnings)
{
    if (!j.contains(key)) return true;
    if (!j[key].is_number()) {
        error = std::string(key) + " must be a number";
        return false;
    }
    const float requested = j[key].get<float>();
    const float clamped = std::clamp(requested, lo, hi);
    if (std::fabs(clamped - requested) > 1e-6f) {
        if (!warnings.empty()) warnings += "; ";
        warnings += std::string(key) + "=" + std::to_string(requested) +
                    " out of range [" + std::to_string(lo) + "," +
                    std::to_string(hi) + "] clamped to " + std::to_string(clamped);
    }
    out = clamped;
    return true;
}

} // namespace

bool parseNpcNavigationSettings(const nlohmann::json& j, NpcNavigationSettings& out,
                                std::string& error)
{
    error.clear();
    if (!j.is_object()) {
        error = "navigation must be an object";
        return false;
    }

    NpcNavigationSettings next;  // defaults; fill from JSON then commit atomically.
    next.configured = true;

    if (!readString(j, "mode", next.mode,
                    {"automatic_surface_path", "direct"}, error)) return false;
    if (!readString(j, "blocked_behavior", next.blockedBehavior,
                    {"turn", "repath", "turn_then_repath"}, error)) return false;

    if (!readBool(j, "automatic_from_collision", next.automaticFromCollision, error)) return false;
    if (!readBool(j, "repath_when_blocked", next.repathWhenBlocked, error)) return false;
    if (!readBool(j, "allow_navigation_jumps", next.allowNavigationJumps, error)) return false;
    if (!readBool(j, "allow_wall_jump", next.allowWallJump, error)) return false;

    // search_radius is the rolling local planner's half-extent. The navigator
    // clamps its own window to [6,20] (see npc-navigator.cpp halfExtent), so 20
    // is the largest value that has any effect; larger requests are clamped and
    // reported through `warnings` rather than silently accepted.
    if (!readClampedFloat(j, "search_radius", 4.0f, 20.0f,
                          next.searchRadius, error, next.warnings)) return false;
    if (!readClampedFloat(j, "max_step_height", 0.0f, 2.0f,
                          next.maxStepHeight, error, next.warnings)) return false;
    if (!readClampedFloat(j, "wall_probe_distance", 0.5f, 6.0f,
                          next.wallProbeDistance, error, next.warnings)) return false;

    // Degrees -> walkable dot. The shared collision rule is used when omitted.
    if (j.contains("max_walkable_slope_degrees")) {
        if (!j["max_walkable_slope_degrees"].is_number()) {
            error = "max_walkable_slope_degrees must be a number";
            return false;
        }
        const float degrees = std::clamp(j["max_walkable_slope_degrees"].get<float>(), 0.0f, 89.0f);
        next.maxWalkableSlopeDot = std::cos(degrees * 3.14159265358979323846f / 180.0f);
    }

    out = next;
    return true;
}

bool npcNavigationSettingsSelfTest(std::string& report)
{
    bool ok = true;
    auto fail = [&](const std::string& why) { ok = false; report += "FAIL: " + why + "\n"; };

    // Defaults when a block provides only the required object.
    {
        std::string error;
        NpcNavigationSettings s;
        if (!parseNpcNavigationSettings(nlohmann::json::parse("{}"), s, error))
            fail("empty navigation block should parse");
        if (!s.configured) fail("parsed settings should be configured");
        if (s.mode != "automatic_surface_path") fail("default mode");
        if (s.maxWalkableSlopeDot != 0.0f) fail("default slope dot should be unset (0)");
        if (!s.allowNavigationJumps) fail("navigation jumps default on");
        if (s.allowWallJump) fail("wall jump default off");
        if (s.blockedBehavior != "turn_then_repath") fail("default blocked behavior");
    }

    // The Counter-Strike block, with degrees converted to a dot.
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
        if (!parseNpcNavigationSettings(j, s, error)) fail("counter_strike navigation block parses");
        const float expected = std::cos(45.0f * 3.14159265358979323846f / 180.0f);
        if (std::fabs(s.maxWalkableSlopeDot - expected) > 1e-5f)
            fail("45 degrees converts to the walkable dot");
        if (std::fabs(s.searchRadius - 20.0f) > 1e-5f) fail("search_radius parsed");
        if (s.allowWallJump) fail("allow_wall_jump false");
    }

    // Invalid enum and bad type are rejected.
    {
        std::string error;
        NpcNavigationSettings s;
        if (parseNpcNavigationSettings(nlohmann::json::parse(R"({"mode":"global_graph"})"), s, error))
            fail("unknown mode is rejected");
        if (parseNpcNavigationSettings(nlohmann::json::parse(R"({"allow_wall_jump":"yes"})"), s, error))
            fail("non-boolean allow_wall_jump is rejected");
        if (parseNpcNavigationSettings(nlohmann::json::parse(R"("direct")"), s, error))
            fail("non-object navigation is rejected");
    }

    // Numeric clamping.
    {
        std::string error;
        NpcNavigationSettings s;
        if (!parseNpcNavigationSettings(nlohmann::json::parse(R"({"search_radius":9999})"), s, error))
            fail("large search_radius parses");
        if (s.searchRadius != 20.0f) fail("search_radius clamps to 20");
        if (s.warnings.empty()) fail("clamped search_radius reports a warning");
        if (!parseNpcNavigationSettings(
                nlohmann::json::parse(R"({"max_walkable_slope_degrees":200})"), s, error))
            fail("large slope degrees parses");
        if (s.maxWalkableSlopeDot < -1e-5f) fail("clamped slope degrees stay valid");
    }

    report += ok ? "PASS\n" : "FAIL\n";
    return ok;
}
