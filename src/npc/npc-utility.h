// 10 02 2026
/* purpose
* Utility goal/action selection for NPC brains. Scores a fixed set of reusable
* goals from perception, health, distance, objective, and team information,
* then applies hysteresis (minimum duration, switch margin, action cooldown) so
* the NPC does not thrash between goals.
* Produces an abstract goal/action; does NOT move, path, or fire.
* Does NOT replace the legacy NpcStateMachine, which remains the executor until
* equivalence is proven (see docs/specs/20261002plan.md Stage 8).
*/
#pragma once

#include <cstdint>
#include <string>

#include <glm/glm.hpp>

class Npc;

enum class UtilityGoalKind : uint8_t
{
    None = 0,
    KillTarget,        // engage a visible/known hostile
    Survive,           // low health / under fire
    HoldPosition,      // hold an angle at the current spot
    TakeCover,         // move to break line of sight
    MoveToObjective,   // advance toward an objective position
    DefendSite,        // hold a defensive position/area
    RotateToSite,      // travel to another site
    PlantObjective,    // plant at the objective
    DefuseObjective,   // defuse the objective
    RetakeSite         // retake a lost site
};

enum class UtilityActionKind : uint8_t
{
    None = 0,
    Approach,
    Retreat,
    HoldAngle,
    Peek,
    Flank,
    TakeCover,
    Reposition,
    Shoot,
    Reload,
    SwitchWeapon,
    ThrowAreaEffect,
    Interact
};

const char* utilityGoalName(UtilityGoalKind kind);
const char* utilityActionName(UtilityActionKind kind);

// Everything the scorer may consult. All optional; zero/empty means "unknown".
struct UtilityContext {
    bool hasVisibleTarget = false;
    bool hasKnownTarget = false;        // visible or remembered
    float targetDistance = 0.0f;
    float targetConfidence = 0.0f;
    float healthFraction = 1.0f;
    float timeRemaining = 0.0f;         // round time left, seconds
    int enemyCount = 0;
    int teamAlive = 0;
    bool weaponReady = true;            // loaded and not reloading
    bool loSBlocked = false;            // line of sight to target is blocked
    bool objectiveKnown = false;        // a mode objective exists
    glm::vec3 objectivePos{0.0f};
    bool atObjective = false;
    bool canPlant = false;              // carrier, in a valid site
    bool canDefuse = false;             // defender, at a planted objective
    bool onDefense = false;
};

// Per-goal score breakdown, kept for inspection and tests.
struct UtilityGoalScore {
    UtilityGoalKind kind = UtilityGoalKind::None;
    float relevance = 0.0f;
    float distance = 0.0f;
    float lineOfSight = 0.0f;
    float health = 0.0f;
    float enemyCount = 0.0f;
    float timeRemaining = 0.0f;
    float objectiveState = 0.0f;
    float weaponReadiness = 0.0f;
    float confidence = 0.0f;
    float teamInformation = 0.0f;
    float total = 0.0f;
};

// Hysteresis state carried per NPC between ticks.
struct UtilityState {
    UtilityGoalKind currentGoal = UtilityGoalKind::None;
    UtilityActionKind currentAction = UtilityActionKind::None;
    float goalTimer = 0.0f;             // time the current goal has been held
    float actionCooldown = 0.0f;        // block re-selecting the last action
    UtilityGoalScore currentScore;      // last chosen goal's score
};

// Score one goal from the context. Pure; no NPC state mutated.
UtilityGoalScore scoreUtilityGoal(UtilityGoalKind kind, const UtilityContext& ctx);

// Select the best goal with hysteresis. `minGoalSeconds` and `switchMargin`
// keep the current goal unless a challenger clearly wins; `dt` advances the
// goal timer and action cooldown. Deterministic for a given context + state.
UtilityGoalKind selectUtilityGoal(const UtilityContext& ctx,
                                  UtilityState& state,
                                  float dt,
                                  float minGoalSeconds = 0.6f,
                                  float switchMargin = 0.08f);

// Map a goal to the action the executor should run.
UtilityActionKind actionForGoal(UtilityGoalKind goal, const UtilityContext& ctx);

// World-independent selftest for scoring, hysteresis, and action mapping.
bool npcUtilitySelfTest(std::string& report);
