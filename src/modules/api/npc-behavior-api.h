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

// Requests the module-owned score for one goal using the same bounded facts as
// a decision. The host uses this for diagnostics; it never drives gameplay.
struct GoalScoreV1 {
    std::uint32_t version;
    std::uint32_t structSize;
    std::uint32_t actorId;
    std::uint32_t tick;
    std::uint32_t goal;
    DecisionV1 context;
    ScoreV1 outScore;
    std::uint32_t outGeneration;
};

// Bounded target candidates after host validity/team filtering. The host owns
// actor lookup and applies the returned ID; the module owns priority,
// stickiness, and switch hysteresis.
constexpr std::uint32_t MAX_TARGET_CANDIDATES = 64;

struct TargetCandidateV1 {
    std::uint32_t id;
    float position[3];
    std::int32_t health;
    std::int32_t maxHealth;
    float threat;
};

enum TargetReason : std::uint32_t {
    TARGET_NONE = 0,
    TARGET_SCORED = 1,
    TARGET_STICKY = 2,
    TARGET_INVALID = 3,
};

struct TargetSelectV1 {
    std::uint32_t version;
    std::uint32_t structSize;
    std::uint32_t actorId;
    std::uint32_t tick;
    std::uint32_t currentTargetId;
    std::uint32_t candidateCount;
    TargetCandidateV1 candidates[MAX_TARGET_CANDIDATES];
    float distanceBias;
    float lowHealthBias;
    float threatBias;
    float stickiness;
    float switchThreshold;
    std::uint32_t outTargetId;
    std::uint32_t outReason;
    std::uint32_t outGeneration;
};

// Evaluates the host's bounded attack facts. The host still validates weapon
// legality and applies firing through the authoritative weapon path.
struct AttackIntentV1 {
    std::uint32_t version;
    std::uint32_t structSize;
    std::uint32_t actorId;
    std::uint32_t tick;
    std::uint32_t flags;
    float targetDistance;
    float healthFraction;
    std::uint32_t outAttack;
    std::uint32_t outReason;
    std::uint32_t outGeneration;
};

enum AttackFlag : std::uint32_t {
    ATTACK_HAS_TARGET = 1u << 0,
    ATTACK_TARGET_VISIBLE = 1u << 1,
    ATTACK_LOS_CLEAR = 1u << 2,
    ATTACK_WEAPON_READY = 1u << 3,
    ATTACK_SAME_TEAM = 1u << 4,
    ATTACK_REACTION_READY = 1u << 5,
    ATTACK_COOLDOWN_READY = 1u << 6,
};

enum AttackReason : std::uint32_t {
    ATTACK_REASON_ELIGIBLE = 0,
    ATTACK_REASON_NO_TARGET = 1,
    ATTACK_REASON_LOS_BLOCKED = 2,
    ATTACK_REASON_WEAPON_NOT_READY = 3,
    ATTACK_REASON_SAME_TEAM = 4,
    ATTACK_REASON_REACTION = 5,
    ATTACK_REASON_COOLDOWN = 6,
};

// Host draws the deterministic roll so the module swap does not reset or
// duplicate the NPC's RNG stream. The module owns the threshold policy.
struct DashDecisionV1 {
    std::uint32_t version;
    std::uint32_t structSize;
    std::uint32_t actorId;
    std::uint32_t tick;
    std::uint32_t flags;
    float difficulty01;
    float distance;
    float healthFraction;
    float chanceScale;
    float roll;
    std::uint32_t outDash;
    std::uint32_t outGeneration;
};

enum DashFlag : std::uint32_t {
    DASH_HAS_TARGET = 1u << 0,
    DASH_TARGET_SEES_ME = 1u << 1,
    DASH_OBJECTIVE_RUSH = 1u << 2,
    DASH_OBJECTIVE_KNOWN = 1u << 3,
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

// Maps a chosen utility goal to its action. The host owns the actor and calls
// this only after a goal decision; the module owns the mapping policy.
struct ActionV1 {
    std::uint32_t version;
    std::uint32_t structSize;
    std::uint32_t actorId;
    std::uint32_t tick;
    std::uint32_t contextFlags;
    std::uint32_t goal;
    float targetDistance;
    std::uint32_t outAction;
    std::uint32_t outGeneration;
};

enum GrenadeFlag : std::uint32_t {
    GRENADE_HAS_TARGET = 1u << 0,
    GRENADE_TARGET_BEHIND_COVER = 1u << 1,
    GRENADE_SELF_IN_BLAST = 1u << 2,
    GRENADE_TRAJECTORY_BLOCKED = 1u << 3,
    GRENADE_DUPLICATE_THROW = 1u << 4,
    GRENADE_HAVE_GRENADE = 1u << 5,
    GRENADE_SITE_DEFENSE = 1u << 6,
    GRENADE_SITE_RETAKE = 1u << 7,
};

// Grenade throw decision. The host gathers squad/target facts; the module owns
// the tactical scoring and rejection reasons.
struct GrenadeDecisionV1 {
    std::uint32_t version;
    std::uint32_t structSize;
    std::uint32_t actorId;
    std::uint32_t tick;
    std::uint32_t flags;
    std::int32_t enemyGroupDensity;
    std::int32_t friendlyNearImpact;
    float threshold;
    float outScore;
    std::uint32_t outAllowed;
    std::uint32_t outGeneration;
};

// Runs the module-owned pure self-test so the host CLI does not retain a
// duplicate policy implementation. 0 = utility, 1 = grenade reasoning.
struct SelfTestV1 {
    std::uint32_t version;
    std::uint32_t structSize;
    std::uint32_t which;
    std::uint32_t outOk;
    std::uint32_t outGeneration;
    char outReport[512];
};

} // namespace mimita_npc_behavior

