// 09 11 2026
/* purpose
* Define and load reusable NPC behavior profiles from config/behavior-profiles.json.
* A profile is combat tuning (aim error, reaction delay, fire cadence, aggression,
* preferred range) referenced by role definitions through behavior_profile.
* Humans have no NPC behavior profile; this is consumed by NPC combat/decisions.
* Does NOT assign profiles, simulate actors, or own combat state.
* Does NOT fail hard on bad JSON - keeps the last valid data and logs an error.
*/
#pragma once

#include <filesystem>
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
};

class BehaviorProfileRegistry
{
public:
    static BehaviorProfileRegistry& instance();

    bool load(const std::string& path = "config/behavior-profiles.json");
    bool pollReload();

    const BehaviorProfileDefinition* get(const std::string& id) const;
    const std::vector<BehaviorProfileDefinition>& all() const { return mProfiles; }

private:
    BehaviorProfileRegistry() = default;

    std::vector<BehaviorProfileDefinition> mProfiles;
    std::unordered_map<std::string, int> mIndexById;
    std::string mPath = "config/behavior-profiles.json";
    std::filesystem::file_time_type mLastWrite{};
    bool mWatchLogged = false;
};

// Resolve a profile id to a value struct. Unknown/empty id -> active=false.
NpcBehaviorTuning resolveNpcBehavior(const std::string& id);
