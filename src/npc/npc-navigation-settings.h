// 10 04 2026
/* purpose
* Declares the automatic surface-navigation settings owned by actor presets
* (config/actor-presets/*.json -> "navigation") and the pure parser.
* A preset that omits the block keeps the navigator's shared defaults; a preset
* that provides a value overrides the shared collision default.
* Does NOT path, build surfaces, or read files; callers pass a parsed JSON
* object in, matching the NPC movement-policy pattern.
*/
#pragma once

#include <string>

#include <nlohmann/json.hpp>

struct NpcNavigationSettings
{
    // True only when a preset actually declared a valid "navigation" block.
    bool configured = false;

    // "automatic_surface_path" = triangle-based route planner (default).
    // "direct" = simple legacy steering, no surface graph.
    std::string mode = "automatic_surface_path";

    // cos(max walkable slope). 0 means "unset: use the shared collision rule"
    // (NpcNavigation::kWalkableSlopeDot). Presets specify degrees instead.
    float maxWalkableSlopeDot = 0.0f;

    // Generate navigation from loaded map collision triangles.
    bool automaticFromCollision = true;

    // Recalculate when the current route is blocked.
    bool repathWhenBlocked = true;

    // Maximum local search distance around the NPC (meters, half-extent).
    float searchRadius = 20.0f;

    // Allow the planner to create legal navigation jump links.
    bool allowNavigationJumps = true;

    // Never jump directly into a vertical wall.
    bool allowWallJump = false;

    // "turn" | "repath" | "turn_then_repath"
    std::string blockedBehavior = "turn_then_repath";

    // Maximum rise walked up without a jump.
    float maxStepHeight = 0.65f;

    // How far ahead the navigator probes for a blocking wall (meters).
    float wallProbeDistance = 1.6f;

    // Non-fatal validation notes (e.g. a requested value was clamped). Empty
    // when every supplied value was in range. The owning loader logs these so
    // an impossible request such as search_radius=200 is never silent.
    std::string warnings;
};

// Parses and validates a "navigation" object. Unknown enum strings and
// wrong-typed values are rejected (returns false with a reason); numeric ranges
// are clamped. Absent keys keep the shared/default values.
bool parseNpcNavigationSettings(const nlohmann::json& j, NpcNavigationSettings& out,
                                std::string& error);

// World-independent selftest for parsing, defaults, and the degree->dot rule.
bool npcNavigationSettingsSelfTest(std::string& report);
