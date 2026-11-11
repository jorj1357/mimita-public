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

// Bounded actor snapshot + decision result for the hot NPC brain boundary.
// The module returns outUseModule=0 to keep the legacy host brain; setting it
// to 1 (a module-only edit) lets the module own state/move/attack decisions.
struct ActorStateV1
{
    std::uint32_t version;
    std::uint32_t structSize;
    std::uint32_t tick;
    std::uint32_t actorId;
    std::uint32_t team;
    std::uint32_t hasTarget;
    std::uint32_t targetVisible;
    std::uint32_t targetRemembered;
    std::uint32_t currentState;
    std::uint32_t trainingMode;
    std::uint32_t onGround;
    std::uint32_t weaponReady;
    std::uint32_t attackCooldownReady;
    float difficulty01;
    float healthFraction;
    float aggression;
    float preferredRange;
    float selfPos[3];
    float selfVel[3];
    float targetPos[3];
    float targetVel[3];
    float lastKnownPos[3];
    float targetDistance;
    float stateTimer;
    float nextDecisionTime;
    std::uint32_t outUseModule;
    std::uint32_t outState;
    float outMinTime;
    float outMaxTime;
    std::uint32_t outAttack;
    std::uint32_t outJump;
    std::uint32_t outDash;
    float outMoveDir[3];
    std::uint32_t outGeneration;
};

// State min/max decision timing. The host owns the NpcStateMachine and applies
// the result; the module owns the per-state timing policy.
struct StateTimingV1
{
    std::uint32_t version;
    std::uint32_t structSize;
    std::uint32_t tick;
    std::uint32_t state; // NpcState numeric (Idle=0 .. Patrol=13)
    float difficulty01;
    float outMinTime;
    float outMaxTime;
    std::uint32_t outGeneration;
};

// Gamemode/difficulty targeting policy. The module owns mode parsing and
// inclusion fallbacks; the host keeps candidate gathering and application.
struct TargetingPolicyV1
{
    std::uint32_t version;
    std::uint32_t structSize;
    std::uint32_t tick;
    std::uint32_t configured;
    char mode[24];
    std::uint32_t includePlayers;
    std::uint32_t includeNpcs;
    char difficultyTargetMode[24];
    std::uint32_t damageOtherNpcs;
    std::uint32_t teamMatch;
    std::uint32_t outConfigured;
    std::uint32_t outMode; // 0 player, 1 closest, 2 opposite_team
    std::uint32_t outIncludePlayers;
    std::uint32_t outIncludeNpcs;
    std::uint32_t outGeneration;
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

enum FireFlag : std::uint32_t {
    FIRE_HAS_TARGET = 1u << 0,
    FIRE_TARGET_ALIVE = 1u << 1,
    FIRE_TARGET_VISIBLE = 1u << 2,
    FIRE_LOS_CLEAR = 1u << 3,
    FIRE_COOLDOWN_READY = 1u << 4,
    FIRE_HAS_WEAPON = 1u << 5,
    FIRE_RANGE_BLOCKED = 1u << 6,
    FIRE_HAS_AMMO_OR_RESERVE = 1u << 7,
    FIRE_MAGAZINE_EMPTY = 1u << 8,
    FIRE_RELOADING = 1u << 9,
};

enum FireReason : std::uint32_t {
    FIRE_REASON_ALLOW = 0,
    FIRE_REASON_TARGET_DEAD = 1,
    FIRE_REASON_COOLDOWN = 2,
    FIRE_REASON_NO_WEAPON = 3,
    FIRE_REASON_RANGE = 4,
    FIRE_REASON_NO_AMMO = 5,
    FIRE_REASON_START_RELOAD = 6,
    FIRE_REASON_RELOADING = 7,
    FIRE_REASON_MAGAZINE_EMPTY = 8,
    FIRE_REASON_NO_LOS = 9,
};

// Detailed fire gate. The host owns the weapon runtime, reload mutation, ammo
// decrement, RNG, and shot execution; the module owns the gate order/reason.
struct FireGateV1
{
    std::uint32_t version;
    std::uint32_t structSize;
    std::uint32_t actorId;
    std::uint32_t tick;
    std::uint32_t flags;
    float targetDistance;
    float healthFraction;
    float attackCooldown;
    float reloadTime;
    float aggression;
    float fireCadenceMultiplier;
    std::uint32_t currentAmmo;
    std::uint32_t reserveAmmo;
    std::uint32_t magazineSize;
    std::uint32_t behaviorType;
    std::uint32_t outUseModule;
    std::uint32_t outAllow;
    std::uint32_t outStartReload;
    std::uint32_t outReason;
    std::uint32_t outGeneration;
};

enum AimFlag : std::uint32_t {
    AIM_HAS_TARGET = 1u << 0,
    AIM_PROJECTILE = 1u << 1,
    AIM_FORCE_HIT = 1u << 2,
};

// Aim point, projectile lead, and error-angle policy. The host draws the error
// cone RNG stream and applies weapon spread/hitscan/projectile execution.
struct AimPolicyV1
{
    std::uint32_t version;
    std::uint32_t structSize;
    std::uint32_t actorId;
    std::uint32_t tick;
    std::uint32_t flags;
    float selfPos[3];
    float selfFacing[3];
    float targetPos[3];
    float targetVel[3];
    float predictedTarget[3];
    float targetDistance;
    float projectileSpeed;
    float aimLeadSeconds;
    float difficulty01;
    float baseAimErrorDeg;
    float profileAimErrorDeg;
    float difficultyErrorScale;
    std::uint32_t forceHit;
    float outAimPoint[3];
    float outErrorDeg;
    std::uint32_t outUseModule;
    std::uint32_t outGeneration;
};

// Numeric values deliberately match combat/weapon-types.h.
enum WeaponBehaviorKind : std::uint32_t {
    WEAPON_BEHAVIOR_NONE = 0,
    WEAPON_BEHAVIOR_HITSCAN = 1,
    WEAPON_BEHAVIOR_PROJECTILE = 2,
    WEAPON_BEHAVIOR_GODBALL = 3,
    WEAPON_BEHAVIOR_MELEE = 4,
    WEAPON_BEHAVIOR_SWORDSWORD = 5,
    WEAPON_BEHAVIOR_ROCKET_LAUNCHER = 6,
    WEAPON_BEHAVIOR_GRENADE_LAUNCHER = 7,
    WEAPON_BEHAVIOR_HAFS = 8,
    WEAPON_BEHAVIOR_QUICK_HIT = 9,
    WEAPON_BEHAVIOR_SPY_KNIFE = 10,
};

constexpr std::uint32_t MAX_WEAPON_CANDIDATES = 12;

struct WeaponCandidateV1 {
    char id[32];
    std::uint32_t behaviorType;
    std::uint32_t magazineSize;
    std::uint32_t currentAmmo;
    std::uint32_t reserveAmmo;
    std::uint32_t isReloading;
    std::uint32_t attackCapable;
    std::uint32_t ammoReady;
    std::uint32_t usable;
    std::uint32_t visitShots;
    float damage;
    std::uint32_t pelletCount;
    float effectiveRange;
    float projectileSpeed;
};

enum WeaponChoiceMode : std::uint32_t {
    WEAPON_CHOICE_CYCLE = 0,
    WEAPON_CHOICE_SCORED = 1,
    WEAPON_CHOICE_LEGACY = 2,
    WEAPON_CHOICE_FORCE = 3,
};

enum WeaponChoiceFlag : std::uint32_t {
    WEAPON_CHOICE_HAS_TARGET = 1u << 0,
    WEAPON_CHOICE_CYCLE_EMPTY = 1u << 1,
    WEAPON_CHOICE_CURRENT_USABLE = 1u << 2,
};

enum WeaponChoiceReason : std::uint32_t {
    WEAPON_CHOICE_KEEP = 0,
    WEAPON_CHOICE_CYCLE_NEXT = 1,
    WEAPON_CHOICE_FIRST_CURRENT = 2,
    WEAPON_CHOICE_SCORED_BEST = 3,
    WEAPON_CHOICE_LEGACY_EMPTY = 4,
    WEAPON_CHOICE_LEGACY_CLOSE = 5,
    WEAPON_CHOICE_LEGACY_FAR = 6,
    WEAPON_CHOICE_LEGACY_MID = 7,
    WEAPON_CHOICE_REASON_FORCE = 8,
    WEAPON_CHOICE_NO_CANDIDATE = 9,
};

// The host owns the loadout, weapon runtimes, visit state, switch cooldown,
// RNG jitter/delay, and npcSwitchWeapon(). The module owns candidate
// eligibility/order and which candidate to keep or select.
struct WeaponChoiceV1
{
    std::uint32_t version;
    std::uint32_t structSize;
    std::uint32_t actorId;
    std::uint32_t tick;
    std::uint32_t mode;
    std::uint32_t flags;
    std::int32_t currentIndex;
    std::int32_t firstReadyIndex;
    std::int32_t forceIndex;
    std::int32_t cycleOrder[MAX_WEAPON_CANDIDATES];
    std::uint32_t cycleOrderCount;
    std::uint32_t currentVisitShotsRemaining;
    float targetDistance;
    float healthFraction;
    float rangeBias;
    float damageBias;
    float safetyBias;
    float switchThreshold;
    float closeSwitchDist;
    float farSwitchDist;
    std::uint32_t candidateCount;
    WeaponCandidateV1 candidates[MAX_WEAPON_CANDIDATES];
    std::int32_t outIndex;
    std::uint32_t outKeepCurrent;
    std::uint32_t outReason;
    std::uint32_t outUseModule;
    std::uint32_t outGeneration;
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

