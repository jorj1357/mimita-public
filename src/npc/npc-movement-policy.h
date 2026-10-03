// 10 03 2026
/* purpose
* Declares the reusable, JSON-controlled NPC movement policy owned by actor
* presets (config/actor-presets/*.json -> "npc_behavior") and the pure helpers
* NPC state selection and the movement hot path use to honor it.
* A policy is generic: it describes how any actor moves and makes general
* movement decisions, not what the game objective is.
* No policy configured => empty/default policy => the legacy NPC brain runs
* unchanged (see NpcMovementPolicy::configured).
* Does NOT own combat tuning, movement physics, navigation, or collision.
* Does NOT read files; callers pass a parsed nlohmann::json object in.
*/
#pragma once

#include <string>

#include <nlohmann/json.hpp>

struct NpcMovementPolicy
{
    // True only when a preset actually declared a valid "npc_behavior" block.
    // When false, every query below returns the permissive/legacy answer.
    bool configured = false;

    // "forward" = keep moving toward the current desired direction.
    // Others (wander/circle/strafe/hold_position) are reserved for future
    // presets and currently fall back to the legacy scored selection.
    std::string travelStyle = "forward";

    // "forward" = keep moving while attacking.
    std::string combatStyle = "forward";

    // Explicit permissions. A false value prevents the matching movement style
    // from being selected by random scoring or legacy state logic.
    bool allowCircle = false;
    bool allowRandomWalk = false;
    bool allowStrafe = false;
    bool allowZigZag = false;
    bool allowHoldPosition = false;

    // "never" | "low_health"
    std::string retreatStyle = "low_health";
    // Fraction of maximum health in [0,1]; 0.35 => retreat pressure below 35%.
    float retreatHealthFraction = 0.35f;

    // "never" | "obstacle_only" | "navigation_only" | "obstacle_or_navigation"
    std::string jumpStyle = "obstacle_or_navigation";
    // "never" | "attack" | "escape" | "navigation" | "attack_or_navigation"
    std::string dashStyle = "attack_or_navigation";
    // "local_sensing" | "full_map" (local_sensing is the only implemented path)
    std::string worldKnowledge = "local_sensing";
    // "turn" | "repath" | "turn_then_repath"
    std::string blockedBehavior = "turn_then_repath";

    // 0.0 = no random directional noise.
    float movementNoise = 0.0f;
};

// Parses and validates an "npc_behavior" object. Unknown enum strings and
// wrong-typed booleans are rejected (returns false with a reason); numeric
// ranges are clamped, never clipped silently to an unrelated behavior.
bool parseNpcMovementPolicy(const nlohmann::json& j, NpcMovementPolicy& out,
                            std::string& error);

// current_health / maximum_health, safe when maximum_health <= 0 (returns 1).
float npcHealthFraction(int currentHp, int maxHp);

// True when the health fraction is at or below `fraction` (boundary-inclusive
// so an exactly-fractional value such as 35/100 at 0.35 is low).
bool npcLowHealth(int currentHp, int maxHp, float fraction);

// Probability in [0,1] that a "forward" actor picks Retreat on this decision.
// Zero at or above retreatHealthFraction; rises as health drops and dominates
// below ~60% of the threshold (about 20% health for the default 0.35), so
// retreat is emergent rather than a hard branch.
float npcRetreatChance(int currentHp, int maxHp, const NpcMovementPolicy& policy);

// Movement styles a policy can gate.
enum class NpcPolicyMovement
{
    Circle,
    Strafe,
    ZigZag,
    RandomWalk,
    HoldPosition,
};
bool npcPolicyAllowsMovement(const NpcMovementPolicy& policy, NpcPolicyMovement m);

// Explicit jump reasons. None means "no real obstacle/traversal".
enum class NpcJumpReason
{
    None,
    Obstacle,
    Navigation,
    Gap,
    Climbable,
};
bool npcPolicyAllowsJump(const NpcMovementPolicy& policy, NpcJumpReason reason);

// Explicit dash reasons.
enum class NpcDashReason
{
    None,
    Attack,
    Escape,
    Navigation,
};
bool npcPolicyAllowsDash(const NpcMovementPolicy& policy, NpcDashReason reason);
