// 10 03 2026
/* purpose
* Focused tests for the actor-preset NPC movement policy: schema parsing and
* validation, percentage-health retreat, and the pure movement/jump/dash
* permission queries.
* Runs as a plain check() + exit-code harness (no gtest).
* Links only src/npc/npc-movement-policy.cpp (nlohmann/json is header-only).
* The real NpcSystem behavior is covered by the in-binary
* --npc-movement-policy-selftest; this file proves the pure policy contract.
*/

#include "npc/npc-movement-policy.h"

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
        std::printf("[npc-movement-policy-test] FAIL %s\n", message);
        std::exit(1);
    }
}

const char* kCounterStrikeBlock = R"json({
  "travel_style": "forward",
  "combat_style": "forward",
  "allow_circle": false,
  "allow_random_walk": false,
  "allow_strafe": false,
  "allow_zigzag": false,
  "allow_hold_position": false,
  "retreat_style": "low_health",
  "retreat_health_fraction": 0.35,
  "jump_style": "obstacle_or_navigation",
  "dash_style": "attack_or_navigation",
  "world_knowledge": "local_sensing",
  "blocked_behavior": "turn_then_repath",
  "movement_noise": 0.0
})json";

void testCounterStrikeParsing()
{
    std::string error;
    NpcMovementPolicy policy;
    const bool ok = parseNpcMovementPolicy(nlohmann::json::parse(kCounterStrikeBlock),
                                           policy, error);
    check(ok, "counter_strike npc_behavior parses");
    check(error.empty(), "counter_strike npc_behavior has no error");
    check(policy.configured, "policy is configured");
    check(policy.travelStyle == "forward", "travel_style = forward");
    check(policy.combatStyle == "forward", "combat_style = forward");
    check(!policy.allowCircle, "allow_circle = false");
    check(!policy.allowRandomWalk, "allow_random_walk = false");
    check(!policy.allowStrafe, "allow_strafe = false");
    check(!policy.allowZigZag, "allow_zigzag = false");
    check(!policy.allowHoldPosition, "allow_hold_position = false");
    check(policy.retreatStyle == "low_health", "retreat_style = low_health");
    check(std::fabs(policy.retreatHealthFraction - 0.35f) < 1e-5f,
          "retreat_health_fraction = 0.35");
    check(policy.jumpStyle == "obstacle_or_navigation",
          "jump_style = obstacle_or_navigation");
    check(policy.dashStyle == "attack_or_navigation",
          "dash_style = attack_or_navigation");
    check(policy.worldKnowledge == "local_sensing",
          "world_knowledge = local_sensing");
    check(policy.blockedBehavior == "turn_then_repath",
          "blocked_behavior = turn_then_repath");
    check(std::fabs(policy.movementNoise) < 1e-5f, "movement_noise = 0.0");
}

void testInvalidValues()
{
    // Unknown enum value must be rejected; the caller keeps the previous preset.
    {
        std::string error;
        NpcMovementPolicy policy;
        const bool ok = parseNpcMovementPolicy(
            nlohmann::json::parse(R"({"travel_style":"teleport"})"), policy, error);
        check(!ok, "unknown travel_style is rejected");
        check(!error.empty(), "unknown travel_style reports a reason");
    }
    // A present-but-wrong-typed boolean must be rejected, not defaulted.
    {
        std::string error;
        NpcMovementPolicy policy;
        const bool ok = parseNpcMovementPolicy(
            nlohmann::json::parse(R"({"allow_circle":"no"})"), policy, error);
        check(!ok, "non-boolean allow_circle is rejected");
    }
    // Unknown retreat/jump/dash/world/blocked strings are rejected too.
    struct Case { const char* json; const char* label; };
    const Case cases[] = {
        {R"({"retreat_style":"always"})", "unknown retreat_style is rejected"},
        {R"({"jump_style":"always"})", "unknown jump_style is rejected"},
        {R"({"dash_style":"always"})", "unknown dash_style is rejected"},
        {R"({"world_knowledge":"omniscient"})", "unknown world_knowledge is rejected"},
        {R"({"blocked_behavior":"panic"})", "unknown blocked_behavior is rejected"},
        {R"("forward")", "non-object npc_behavior is rejected"},
    };
    for (const Case& c : cases) {
        std::string error;
        NpcMovementPolicy policy;
        check(!parseNpcMovementPolicy(nlohmann::json::parse(c.json), policy, error), c.label);
    }
}

void testNumericClamping()
{
    {
        std::string error;
        NpcMovementPolicy policy;
        check(parseNpcMovementPolicy(
                  nlohmann::json::parse(R"({"retreat_health_fraction":-0.5})"),
                  policy, error), "negative retreat fraction parses");
        check(policy.retreatHealthFraction == 0.0f, "negative retreat fraction clamps to 0");
    }
    {
        std::string error;
        NpcMovementPolicy policy;
        check(parseNpcMovementPolicy(
                  nlohmann::json::parse(R"({"retreat_health_fraction":1.5})"),
                  policy, error), "retreat fraction > 1 parses");
        check(policy.retreatHealthFraction == 1.0f, "retreat fraction > 1 clamps to 1");
    }
    {
        std::string error;
        NpcMovementPolicy policy;
        check(parseNpcMovementPolicy(nlohmann::json::parse(R"({"movement_noise":-2.0})"),
                                     policy, error), "negative noise parses");
        check(policy.movementNoise == 0.0f, "negative noise clamps to 0");
    }
    {
        std::string error;
        NpcMovementPolicy policy;
        check(!parseNpcMovementPolicy(nlohmann::json::parse(R"({"movement_noise":"loud"})"),
                                      policy, error), "non-numeric noise is rejected");
    }
}

void testPercentageHealth()
{
    // The same fraction must classify every maximum-health scale identically.
    check(npcLowHealth(35, 100, 0.35f), "100 max / 35 hp is low health");
    check(npcLowHealth(350, 1000, 0.35f), "1,000 max / 350 hp is low health");
    check(npcLowHealth(3500, 10000, 0.35f), "10,000 max / 3,500 hp is low health");
    check(npcLowHealth(350000, 1000000, 0.35f), "1,000,000 max / 350,000 hp is low health");
    check(!npcLowHealth(35, 100, 0.30f), "35% is not low at a 30% threshold");
    check(npcHealthFraction(0, 0) == 1.0f, "zero maximum health is treated as healthy");
    check(std::fabs(npcHealthFraction(250, 1000) - 0.25f) < 1e-6f,
          "health fraction is current/maximum");
}

void testRetreatChance()
{
    NpcMovementPolicy policy;
    policy.retreatStyle = "low_health";
    policy.retreatHealthFraction = 0.35f;
    check(npcRetreatChance(100, 100, policy) == 0.0f, "full health: no retreat chance");
    check(npcRetreatChance(35, 100, policy) == 0.0f, "at threshold: no retreat chance");
    // 20% (or lower) must be a much higher chance, matching the design note.
    check(npcRetreatChance(20, 100, policy) >= 0.75f, "20% health: high retreat chance");
    check(npcRetreatChance(10, 100, policy) >= 0.85f, "10% health: very high retreat chance");
    check(npcRetreatChance(0, 100, policy) == 1.0f, "0 health: certain retreat chance");
    // The chance is percentage-based.
    check(std::fabs(npcRetreatChance(200, 1000, policy) -
                    npcRetreatChance(2000, 10000, policy)) < 1e-6f,
          "retreat chance is the same percentage at every scale");
    policy.retreatStyle = "never";
    check(npcRetreatChance(1, 100, policy) == 0.0f, "retreat never: no chance");
}

void testMovementPermissions()
{
    NpcMovementPolicy policy;
    policy.configured = true;
    check(!npcPolicyAllowsMovement(policy, NpcPolicyMovement::Circle), "circle disallowed");
    check(!npcPolicyAllowsMovement(policy, NpcPolicyMovement::Strafe), "strafe disallowed");
    check(!npcPolicyAllowsMovement(policy, NpcPolicyMovement::ZigZag), "zigzag disallowed");
    check(!npcPolicyAllowsMovement(policy, NpcPolicyMovement::RandomWalk), "randomwalk disallowed");
    check(!npcPolicyAllowsMovement(policy, NpcPolicyMovement::HoldPosition), "hold disallowed");
    policy.allowCircle = true;
    policy.allowStrafe = true;
    policy.allowZigZag = true;
    policy.allowRandomWalk = true;
    policy.allowHoldPosition = true;
    check(npcPolicyAllowsMovement(policy, NpcPolicyMovement::Circle), "circle re-enabled");
    check(npcPolicyAllowsMovement(policy, NpcPolicyMovement::HoldPosition), "hold re-enabled");
    // An unconfigured policy never restricts (legacy brain).
    NpcMovementPolicy legacy;
    check(npcPolicyAllowsMovement(legacy, NpcPolicyMovement::RandomWalk),
          "legacy policy allows random walk");
}

void testJumpPermissions()
{
    NpcMovementPolicy policy;
    policy.configured = true;
    policy.jumpStyle = "obstacle_or_navigation";
    check(npcPolicyAllowsJump(policy, NpcJumpReason::Obstacle), "obstacle jump allowed");
    check(npcPolicyAllowsJump(policy, NpcJumpReason::Navigation), "navigation jump allowed");
    check(npcPolicyAllowsJump(policy, NpcJumpReason::Gap), "gap jump allowed");
    check(npcPolicyAllowsJump(policy, NpcJumpReason::Climbable), "climb jump allowed");
    check(!npcPolicyAllowsJump(policy, NpcJumpReason::None), "jump with no reason is refused");

    policy.jumpStyle = "obstacle_only";
    check(npcPolicyAllowsJump(policy, NpcJumpReason::Obstacle), "obstacle-only allows obstacle");
    check(!npcPolicyAllowsJump(policy, NpcJumpReason::Navigation), "obstacle-only refuses navigation");

    policy.jumpStyle = "navigation_only";
    check(!npcPolicyAllowsJump(policy, NpcJumpReason::Obstacle), "navigation-only refuses obstacle");
    check(npcPolicyAllowsJump(policy, NpcJumpReason::Navigation), "navigation-only allows navigation");

    policy.jumpStyle = "never";
    check(!npcPolicyAllowsJump(policy, NpcJumpReason::Obstacle), "never refuses all jumps");
}

void testDashPermissions()
{
    NpcMovementPolicy policy;
    policy.configured = true;
    policy.dashStyle = "attack_or_navigation";
    check(npcPolicyAllowsDash(policy, NpcDashReason::Attack), "attack dash allowed");
    check(npcPolicyAllowsDash(policy, NpcDashReason::Navigation), "navigation dash allowed");
    check(!npcPolicyAllowsDash(policy, NpcDashReason::Escape), "escape dash refused by attack_or_navigation");
    check(!npcPolicyAllowsDash(policy, NpcDashReason::None), "dash with no reason is refused");

    policy.dashStyle = "escape";
    check(npcPolicyAllowsDash(policy, NpcDashReason::Escape), "escape style allows escape");
    check(!npcPolicyAllowsDash(policy, NpcDashReason::Attack), "escape style refuses attack");

    policy.dashStyle = "never";
    check(!npcPolicyAllowsDash(policy, NpcDashReason::Attack), "never refuses all dashes");
}

} // namespace

int main()
{
    testCounterStrikeParsing();
    testInvalidValues();
    testNumericClamping();
    testPercentageHealth();
    testRetreatChance();
    testMovementPermissions();
    testJumpPermissions();
    testDashPermissions();
    std::printf("[npc-movement-policy-test] PASS (%d checks)\n", gChecks);
    return 0;
}
