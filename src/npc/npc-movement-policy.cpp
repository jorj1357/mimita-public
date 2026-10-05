// 10 03 2026
/* purpose
* Implements the actor-preset NPC movement policy schema, validation, and the
* pure queries used by state selection and the movement hot path.
* Strict on enums/booleans (reject => caller keeps the last valid preset),
* clamping on numeric ranges. No file or NPC dependency.
*/

#include "npc/npc-movement-policy.h"

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
                      float& out, std::string& error)
{
    if (!j.contains(key)) return true;
    if (!j[key].is_number()) {
        error = std::string(key) + " must be a number";
        return false;
    }
    out = std::clamp(j[key].get<float>(), lo, hi);
    return true;
}

} // namespace

bool parseNpcMovementPolicy(const nlohmann::json& j, NpcMovementPolicy& out,
                            std::string& error)
{
    error.clear();
    if (!j.is_object()) {
        error = "npc_behavior must be an object";
        return false;
    }

    NpcMovementPolicy next;  // defaults; fill from JSON then commit atomically.
    next.configured = true;

    if (!readString(j, "travel_style", next.travelStyle,
                    {"forward", "wander", "circle", "strafe", "hold_position"}, error)) return false;
    if (!readString(j, "combat_style", next.combatStyle,
                    {"forward", "circle", "strafe", "stand"}, error)) return false;
    if (!readString(j, "retreat_style", next.retreatStyle,
                    {"never", "low_health"}, error)) return false;
    if (!readString(j, "jump_style", next.jumpStyle,
                    {"never", "obstacle_only", "navigation_only", "obstacle_or_navigation"}, error)) return false;
    if (!readString(j, "dash_style", next.dashStyle,
                    {"never", "attack", "escape", "navigation", "attack_or_navigation"}, error)) return false;
    if (!readString(j, "world_knowledge", next.worldKnowledge,
                    {"local_sensing", "full_map"}, error)) return false;
    if (!readString(j, "blocked_behavior", next.blockedBehavior,
                    {"turn", "repath", "turn_then_repath"}, error)) return false;
    if (!readString(j, "movement_executor", next.movementExecutor,
                    {"sandbox_shared", "surface_navigation", "direct"}, error)) return false;

    if (!readBool(j, "allow_circle", next.allowCircle, error)) return false;
    if (!readBool(j, "allow_random_walk", next.allowRandomWalk, error)) return false;
    if (!readBool(j, "allow_strafe", next.allowStrafe, error)) return false;
    if (!readBool(j, "allow_zigzag", next.allowZigZag, error)) return false;
    if (!readBool(j, "allow_hold_position", next.allowHoldPosition, error)) return false;

    if (!readClampedFloat(j, "retreat_health_fraction", 0.0f, 1.0f,
                          next.retreatHealthFraction, error)) return false;
    if (!readClampedFloat(j, "forward_patrol_distance", 1.0f, 999.0f,
                          next.forwardPatrolDistance, error)) return false;
    if (!readClampedFloat(j, "movement_noise", 0.0f, 1.0f, next.movementNoise, error)) return false;

    out = next;
    return true;
}

float npcHealthFraction(int currentHp, int maxHp)
{
    return maxHp > 0
        ? static_cast<float>(currentHp) / static_cast<float>(maxHp)
        : 1.0f;
}

bool npcLowHealth(int currentHp, int maxHp, float fraction)
{
    // Inclusive at the boundary so an exactly-fractional threshold (e.g.
    // 35/100 at 0.35) still counts as low, matching the acceptance examples.
    return npcHealthFraction(currentHp, maxHp) <= fraction;
}

float npcRetreatChance(int currentHp, int maxHp, const NpcMovementPolicy& policy)
{
    if (policy.retreatStyle != "low_health")
        return 0.0f;

    const float threshold = std::clamp(policy.retreatHealthFraction, 0.0f, 1.0f);
    if (threshold <= 0.0f)
        return 0.0f;

    const float health = npcHealthFraction(currentHp, maxHp);
    if (health >= threshold)
        return 0.0f;

    // Linear severity from 0 at the threshold to 1 at zero health.
    const float severity = (threshold - health) / threshold;
    float chance = severity;

    // Below ~60% of the threshold (about 21% health for the default 0.35),
    // retreat dominates so a badly hurt actor breaks off instead of pushing in.
    const float knee = threshold * 0.6f;
    if (health <= knee) {
        const float kneeDrop = (knee - health) / std::max(knee, 1e-4f);
        chance = std::max(chance, 0.75f + 0.25f * std::clamp(kneeDrop, 0.0f, 1.0f));
    }
    return std::clamp(chance, 0.0f, 1.0f);
}

bool npcPolicyAllowsMovement(const NpcMovementPolicy& policy, NpcPolicyMovement m)
{
    if (!policy.configured)
        return true;  // legacy: everything is selectable
    switch (m) {
        case NpcPolicyMovement::Circle:       return policy.allowCircle;
        case NpcPolicyMovement::Strafe:       return policy.allowStrafe;
        case NpcPolicyMovement::ZigZag:       return policy.allowZigZag;
        case NpcPolicyMovement::RandomWalk:   return policy.allowRandomWalk;
        case NpcPolicyMovement::HoldPosition: return policy.allowHoldPosition;
    }
    return true;
}

bool npcPolicyAllowsJump(const NpcMovementPolicy& policy, NpcJumpReason reason)
{
    if (!policy.configured)
        return true;
    if (policy.jumpStyle == "never")
        return false;
    if (policy.jumpStyle == "obstacle_only")
        return reason == NpcJumpReason::Obstacle;
    if (policy.jumpStyle == "navigation_only")
        return reason == NpcJumpReason::Navigation || reason == NpcJumpReason::Gap ||
               reason == NpcJumpReason::Climbable;
    // obstacle_or_navigation: any real obstacle/traversal reason.
    return reason == NpcJumpReason::Obstacle || reason == NpcJumpReason::Navigation ||
           reason == NpcJumpReason::Gap || reason == NpcJumpReason::Climbable;
}

bool npcPolicyAllowsDash(const NpcMovementPolicy& policy, NpcDashReason reason)
{
    if (!policy.configured)
        return true;
    if (policy.dashStyle == "never")
        return false;
    if (policy.dashStyle == "attack")
        return reason == NpcDashReason::Attack;
    if (policy.dashStyle == "escape")
        return reason == NpcDashReason::Escape;
    if (policy.dashStyle == "navigation")
        return reason == NpcDashReason::Navigation;
    // attack_or_navigation
    return reason == NpcDashReason::Attack || reason == NpcDashReason::Navigation;
}
