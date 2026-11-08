// 2026-11-08
/* purpose
* define the versioned plain-data boundary for NPC intent selection
* the module owns utility goal/action policy; the host owns actors and simulation
* this file DOES NOT expose Npc, glm, STL, physics, or collision ownership
*/
#pragma once

#include <cstdint>

#include "modules/api/module-abi.h"

namespace mimita_npc_behavior {

constexpr std::uint32_t ABI_VERSION = 2;

enum ContextFlag : std::uint32_t {
    HAS_VISIBLE_TARGET = 1u << 0,
    HAS_KNOWN_TARGET = 1u << 1,
    HAZARD_ACTIVE = 1u << 2,
    LOS_BLOCKED = 1u << 3,
    OBJECTIVE_KNOWN = 1u << 4,
    PLANTED = 1u << 5,
    IS_DEFUSER = 1u << 6,
    CAN_PLANT = 1u << 7,
    CAN_DEFUSE = 1u << 8,
    AT_OBJECTIVE = 1u << 9,
    ON_DEFENSE = 1u << 10,
    ENEMY_AREA_KNOWN = 1u << 11,
    FOCUS_KNOWN = 1u << 12,
    SQUAD_SLOT_KNOWN = 1u << 13,
    AT_SQUAD_SLOT = 1u << 14,
    WEAPON_READY = 1u << 15,
    BOMB_DROPPED = 1u << 16,
    IS_RECOVERER = 1u << 17,
    IS_CARRIER = 1u << 18,
    IS_TRAVEL_SLOT_KNOWN = 1u << 19,
    HAZARD_LOOK_AWAY_KNOWN = 1u << 20,
    SQUAD_APPROACH_KNOWN = 1u << 21,
    SWARM = 1u << 22,
    DOWN_DASH_OPPORTUNITY = 1u << 23,
    DASH_OPPORTUNITY = 1u << 24,
    PAYLOAD_OBJECTIVE = 1u << 25,
};

struct ScoreV1 {
    std::uint32_t kind;
    float relevance;
    float distance;
    float lineOfSight;
    float health;
    float enemyCount;
    float timeRemaining;
    float objectiveState;
    float weaponReadiness;
    float confidence;
    float teamInformation;
    float total;
};

struct DecisionV1 {
    std::uint32_t version;
    std::uint32_t structSize;
    std::uint32_t actorId;
    std::uint32_t tick;
    std::uint32_t contextFlags;
    std::int32_t enemyCount;
    std::int32_t teamAlive;
    std::int32_t team;
    float targetDistance;
    float targetConfidence;
    float healthFraction;
    float timeRemaining;
    float bombDefendRadius;
    float travelObjectiveBias;
    float travelHuntBias;
    float enemyAreaPosition[3];
    float plantedBombPosition[3];
    float objectivePosition[3];
    float travelSlot[3];
    float hazardPosition[3];
    float hazardRadius;
    std::int32_t hazardKind;
    float lookAwayFrom[3];
    float focusPosition[3];
    float squadSlot[3];
    float squadAnchor[3];
    float squadApproachDirection[3];
    std::int32_t squadApproachSector;
    float dt;
    float minGoalSeconds;
    float switchMargin;
    std::uint32_t currentGoal;
    std::uint32_t currentAction;
    float goalTimer;
    float actionCooldown;
    ScoreV1 currentScore;
    std::uint32_t outGoal;
    std::uint32_t outAction;
    float outGoalTimer;
    float outActionCooldown;
    ScoreV1 outCurrentScore;
    std::uint32_t outGeneration;
    float outDecisionMilliseconds;
    float downDashChance;
    std::uint32_t outDownDash;
    std::uint32_t outHoldPursuit;
    float dashChance;
    std::uint32_t outDash;
    float targetCartDistance;
};

// The host owns the cart and actor list. The module assigns a destination from
// bounded, immutable facts; the host validates the returned world position.
struct PayloadRoleV1 {
    std::uint32_t version;
    std::uint32_t structSize;
    std::uint32_t actorId;
    std::uint32_t tick;
    std::int32_t team;
    std::int32_t attackerTeam;
    std::int32_t defenderTeam;
    std::int32_t teamSlot;
    std::int32_t humanPushers;
    std::int32_t maxPushAttackers;
    float interactionTransform[16];
    float interactionSize[3];
    float cartPosition[3];
    float outPosition[3];
    std::uint32_t outRole; // 1=push, 2=attack cover, 3=contest, 4=defend
    std::uint32_t outGeneration;
};

} // namespace mimita_npc_behavior

