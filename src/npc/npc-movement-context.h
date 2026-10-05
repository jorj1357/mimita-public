// 10 04 2026
/* purpose
* Defines the typed boundary between mode-specific target/objective decisions
* and the one shared NPC movement executor.
* A mode (Counter-Strike, Sandbox, future Arena Fighter / Zombie Guard) fills a
* context with the target it selected, the objective position it resolved, and
* an optional goal override; the shared NpcSystem::updateOneNpc owns all
* steering, wall avoidance, pathing, stuck recovery, and physics.
* Also defines the reusable movement-executor selector so an actor preset can
* pick the shared executor without any mode-specific branch.
* Does NOT path, steer, read config, or apply physics.
*/
#pragma once

#include <cstdint>
#include <string>

#include <glm/glm.hpp>

#include "npc/npc-goal.h"
#include "npc/npc-utility.h"

struct Player;

// Which shared movement executor an actor runs. "sandbox_shared" is the default
// and makes every actor behave exactly like Sandbox.
enum class NpcMovementExecutor : uint8_t
{
    SandboxShared = 0,   // shared executor; optional preset navigation is honored
    SurfaceNavigation,   // shared executor honoring the preset navigation block
    Direct,              // shared executor, direct steering (no surface graph)
};

const char* npcMovementExecutorName(NpcMovementExecutor executor);

// Strict parse. Returns false and leaves `out` untouched for an unknown value.
bool npcMovementExecutorFromString(const std::string& value, NpcMovementExecutor& out);

// Everything a mode decides before the shared movement executor runs.
struct NpcMovementContext
{
    // Mode-selected target. Used by perception and combat; never selected here.
    Player* target = nullptr;
    bool hasTarget = false;
    uint32_t targetActorId = 0;
    bool targetIsNpc = false;

    // Mode-selected objective. Objective-aware callers (TeamBrain) fill these;
    // Sandbox leaves them default. Applied only when `hasObjective` is true, so
    // the existing direct utility-context producer keeps working unchanged.
    UtilityContext objective;
    bool hasObjective = false;

    // Optional explicit goal from mode code. Objectives own the goal, never the
    // movement executor; when false the shared makeNavGoal() decides.
    bool hasGoalOverride = false;
    NpcGoal goalOverride;

    bool valid() const { return target != nullptr; }
};

// World-independent selftest for executor naming/parsing and the default.
bool npcMovementExecutorSelfTest(std::string& report);
