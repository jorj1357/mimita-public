// 09 11 2026
/* purpose
* Define and load reusable NPC behavior profiles from config/behavior-profiles.json.
* A profile is combat and pursuit tuning (aim error, reaction delay, fire cadence,
* aggression, preferred range, and remembered-target behavior) referenced by role
* definitions through behavior_profile.
* Humans have no NPC behavior profile; this is consumed by NPC combat/decisions.
* Does NOT assign profiles, simulate actors, or own combat state.
* Does NOT fail hard on bad JSON - keeps the last valid data and logs an error.
*/
#pragma once

#include <filesystem>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

// One named behavior profile. Missing numeric fields are left at their
// "no override" sentinel so callers fall back to existing defaults.
struct BehaviorProfileDefinition
{
    std::string id;
    float aimErrorDeg = -1.0f;           // <0 = no override
    float reactionDelay = -1.0f;         // <0 = no override
    float fireCadenceMultiplier = 1.0f;  // >1 = faster, <1 = slower
    float aggression = -1.0f;            // <0 = no override
    float preferredRange = -1.0f;        // <0 = no override
    // Target selection weights (<0 = no override).
    float targetStickiness = -1.0f;      // bonus for keeping the current target
    float targetSwitchThreshold = -1.0f; // required score margin to switch
    float lowHealthTargetBias = -1.0f;   // preference for vulnerable targets
    float threatBias = -1.0f;            // preference for dangerous targets
    float distanceTargetBias = -1.0f;    // preference for closer targets
    // Weapon selection weights (<0 = no override).
    float weaponRangeBias = -1.0f;       // fit between weapon range and distance
    float weaponDamageBias = -1.0f;      // preference for high damage
    float weaponSafetyBias = -1.0f;      // preference for safer/ranged weapons
    float weaponSwitchThreshold = -1.0f; // required score margin to switch
    // Pursuit after line of sight is lost. String values are documented in JSON.
    std::string pursuitMode = "last_known_position";
    bool continueThroughCover = true;
    bool pursueLastKnownPosition = true;
    std::string afterReachingLastKnown = "look_around";
    int pursuitSearchTicks = 600;      // fixed 60 Hz ticks after reaching memory
    // Target information through cover. String values are documented in JSON.
    std::string informationMode = "memory";
    int radarDelayTicks = 0;
    float radarErrorMeters = 0.0f;
    std::string radarMemoryMode = "normal";
    int radarMemoryTicks = 1800;
    int rememberedPathPoints = 12;
    bool continuePredictedPath = false;

    // ── Movement replanning + commitment (owned here, consumed by the shared
    // NpcNavigator). -1 on the two timers = "no override" so old profiles keep
    // the compiled compatibility defaults (0.9 s / 2.5 m). The commitment
    // fields carry concrete defaults, so a profile that omits the nested
    // `movement_commitment` object still gets a valid committed pursuit.
    float repathIntervalSeconds = -1.0f;      // <0 = no override
    float goalMoveThresholdMeters = -1.0f;    // <0 = no override
    bool commitmentEnabled = true;
    float commitmentDirectionSeconds = 10.0f;
    float commitmentProgressCheckSeconds = 1.0f;
    float commitmentMinimumProgressMeters = 0.5f;
    float commitmentCandidateDistanceMeters = 20.0f;
    bool commitmentAllowReverse = false;
    bool commitmentAvoidRecentPath = true;
    float commitmentRecentPathAvoidRadius = 5.0f;
    bool commitmentVisibleEnemyAllowsCombatMovement = true;
    float commitmentForwardBias = 2.0f;
    float commitmentTargetProgressBias = 4.0f;
    float commitmentOpenDistanceBias = 6.0f;
    float commitmentReversePenalty = 8.0f;
    // Persistent exploration target distance ahead of the actor (Explore goal),
    // and the distance at which it counts as reached. Metres.
    float travelTargetDistanceMeters = 60.0f;
    float travelTargetReachedMeters = 4.0f;
};

// Resolved combat tuning carried by an NPC for its current life. `active` is
// false when no profile resolved, so callers keep their existing defaults.
struct NpcBehaviorTuning
{
    bool active = false;
    float aimErrorDeg = -1.0f;
    float reactionDelay = 0.0f;
    float fireCadenceMultiplier = 1.0f;
    float aggression = -1.0f;
    float preferredRange = -1.0f;
    // Target selection (neutral defaults reproduce nearest-target behavior).
    float targetStickiness = 0.0f;
    float targetSwitchThreshold = 0.0f;
    float lowHealthTargetBias = 0.0f;
    float threatBias = 0.0f;
    float distanceTargetBias = 1.0f;
    // Weapon selection.
    float weaponRangeBias = 1.0f;
    float weaponDamageBias = 0.0f;
    float weaponSafetyBias = 0.0f;
    float weaponSwitchThreshold = 0.0f;
    std::string pursuitMode = "last_known_position";
    bool continueThroughCover = true;
    bool pursueLastKnownPosition = true;
    std::string afterReachingLastKnown = "look_around";
    int pursuitSearchTicks = 600;
    std::string informationMode = "memory";
    int radarDelayTicks = 0;
    float radarErrorMeters = 0.0f;
    std::string radarMemoryMode = "normal";
    int radarMemoryTicks = 1800;
    int rememberedPathPoints = 12;
    bool continuePredictedPath = false;

    // Movement replanning + commitment, resolved to concrete values. Older
    // profiles resolve to the compatibility defaults (0.9 s / 2.5 m).
    float repathIntervalSeconds = 0.9f;
    float goalMoveThresholdMeters = 2.5f;
    bool commitmentEnabled = true;
    float commitmentDirectionSeconds = 10.0f;
    float commitmentProgressCheckSeconds = 1.0f;
    float commitmentMinimumProgressMeters = 0.5f;
    float commitmentCandidateDistanceMeters = 20.0f;
    bool commitmentAllowReverse = false;
    bool commitmentAvoidRecentPath = true;
    float commitmentRecentPathAvoidRadius = 5.0f;
    bool commitmentVisibleEnemyAllowsCombatMovement = true;
    float commitmentForwardBias = 2.0f;
    float commitmentTargetProgressBias = 4.0f;
    float commitmentOpenDistanceBias = 6.0f;
    float commitmentReversePenalty = 8.0f;
    float travelTargetDistanceMeters = 60.0f;
    float travelTargetReachedMeters = 4.0f;
};

class BehaviorProfileRegistry
{
public:
    static BehaviorProfileRegistry& instance();

    bool load(const std::string& path = "config/behavior-profiles.json");
    bool pollReload();
    uint64_t revision() const { return mRevision; }

    const BehaviorProfileDefinition* get(const std::string& id) const;
    const std::vector<BehaviorProfileDefinition>& all() const { return mProfiles; }

private:
    BehaviorProfileRegistry() = default;

    std::vector<BehaviorProfileDefinition> mProfiles;
    std::unordered_map<std::string, int> mIndexById;
    std::string mPath = "config/behavior-profiles.json";
    std::filesystem::file_time_type mLastWrite{};
    uint64_t mRevision = 0;
    bool mWatchLogged = false;
};

// Resolve a profile id to a value struct. Unknown/empty id -> active=false.
NpcBehaviorTuning resolveNpcBehavior(const std::string& id);

// World-independent selftest for behavior-profile parsing: defaults when the
// movement/commitment fields are omitted, clamping with a validation warning,
// and the compatibility defaults (repath 0.9 s / goal threshold 2.5 m).
bool behaviorProfileSelfTest(std::string& report);
