// 10 04 2026
/* purpose
* Pure, world-independent target-selection policy for NPCs.
* A gamemode may declare an "npc_targeting" block (mode + player/NPC inclusion)
* so team modes choose enemies by team relation instead of the global
* npc-difficulty targetMode. Hostility is a single generic predicate so a future
* team-relation table (RED hostile BLUE/GREEN, etc.) can replace it without
* changing selection code.
* Does NOT query the world, read config files, or apply damage.
*/
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

// "player" = human-priority legacy; "closest" = nearest valid hostile;
// "opposite_team" = any live actor on a hostile team.
enum class NpcTargetingMode : uint8_t
{
    Player = 0,
    Closest,
    OppositeTeam,
};

struct NpcTargetingPolicy
{
    // True only when a gamemode declared a valid "npc_targeting" block. When
    // false, callers fall back to the legacy npc-difficulty targetMode.
    bool configured = false;
    NpcTargetingMode mode = NpcTargetingMode::Closest;
    bool includePlayers = true;
    bool includeNpcs = true;
};

NpcTargetingMode npcTargetingModeFromString(const std::string& value, bool& ok);
const char* npcTargetingModeName(NpcTargetingMode mode);

// Hostility under the policy. Closest/OppositeTeam require both teams valid and
// different. Player keeps the legacy free-for-all rule (either team < 0 counts
// as hostile) so non-team modes are unchanged.
bool npcTargetingIsHostile(const NpcTargetingPolicy& policy,
                           int myTeam, int candidateTeam);

bool npcTargetingIncludesPlayers(const NpcTargetingPolicy& policy);
bool npcTargetingIncludesNpcs(const NpcTargetingPolicy& policy);

// One candidate actor for pure selection/testing. `isNpc` distinguishes humans.
struct NpcTargetCandidate
{
    uint32_t id = 0;
    int team = -1;
    bool isNpc = false;
    bool dead = false;
    glm::vec3 pos{0.0f};
};

// Nearest valid hostile candidate under the policy. Player mode prefers a
// hostile human and only falls back to an NPC when no human is available.
// Returns 0 when no valid target exists.
uint32_t selectNpcTargetId(const NpcTargetingPolicy& policy, int myTeam,
                           const std::vector<NpcTargetCandidate>& candidates,
                           uint32_t currentTargetId = 0);

// World-independent selftest for the policy predicate and selection matrix.
bool npcTargetingSelfTest(std::string& report);
