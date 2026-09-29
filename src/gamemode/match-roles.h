// 09 10 2026
/* purpose
* Define and load reusable match-role definitions from config/roles.json.
* A role is an ID plus referenced gameplay profile IDs (movement/weapon/behavior).
* Humans and NPCs are assigned roles from this one registry.
* Does NOT assign roles, simulate actors, or own team/scoring state.
* Does NOT fail hard on bad JSON - keeps the last valid data and logs an error.
*/
#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

#include "physics/movement/movement-types.h"

struct ActorPresetPresentation
{
    bool hasDamageNumbers = false;
    bool damageNumbers = true;
    bool hasHitEffects = false;
    bool hitEffects = true;
    bool hasWorldImpactEffects = false;
    bool worldImpactEffects = true;
    bool hasBloodEffects = false;
    bool bloodEffects = true;
    bool hasMuzzleFlash = false;
    bool muzzleFlash = true;
};

struct ActorPresetWeaponOverride
{
    bool hasDamage = false;
    float damage = 0.0f;
    bool hasFireDelay = false;
    float fireDelay = 0.0f;
    bool hasReloadTime = false;
    float reloadTime = 0.0f;
    bool hasMagazineSize = false;
    int magazineSize = 0;
    bool hasReserveAmmo = false;
    int reserveAmmo = 0;
    bool hasHitscan = false;
    bool hitscan = true;
    bool hasBeamThickness = false;
    float beamThickness = 0.0f;
    bool hasWorldThickness = false;
    float worldThickness = 0.0f;
    bool hasRange = false;
    float range = 0.0f;
    bool hasTracerEnabled = false;
    bool tracerEnabled = true;
    bool hasTracerThickness = false;
    float tracerThickness = 0.0f;
    std::vector<std::string> allowedBodyParts;
    ActorPresetPresentation presentation;
};

struct MatchRoleDefinition
{
    std::string id;
    std::string displayName;
    bool actorPreset = false;
    int team = -1;  // preferred team, -1 = any
    int health = 0; // 0 = no override (use default/legacy max health)
    std::string movementPreset;
    std::string weaponSet;
    std::string startingWeapon;
    std::string behaviorProfile;
    std::string avatarName;
    bool avatarForced = false;
    std::vector<std::string> allowedAvatars;
    std::vector<int> allowedTeams;
    float cameraFov = 0.0f;
    bool forceFov = false;
    bool forceFirstPerson = false;
    ActorPresetPresentation presentation;
    std::unordered_map<std::string, ActorPresetWeaponOverride> weaponOverrides;
};

class MatchRoleRegistry
{
public:
    static MatchRoleRegistry& instance();

    bool load(const std::string& path = "config/roles.json");
    bool loadActorPresets(const std::string& directory = "config/actor-presets");
    bool pollReload();

    const MatchRoleDefinition* get(const std::string& id) const;
    const MatchRoleDefinition* getActorPreset(const std::string& id) const;
    const std::vector<MatchRoleDefinition>& all() const { return mRoles; }
    std::vector<const MatchRoleDefinition*> actorPresets() const;
    const std::string& actorPresetDirectory() const { return mPresetDirectory; }

    // Stable 1-based role index for the wire: 0 = none, 1..N = mRoles[i-1].
    int indexOf(const std::string& id) const;
    const std::string& idForIndex(int index) const;

private:
    MatchRoleRegistry() = default;

    std::vector<MatchRoleDefinition> mRoles;
    std::unordered_map<std::string, int> mIndexById;
    std::string mPath = "config/roles.json";
    std::filesystem::file_time_type mLastWrite{};
    bool mWatchLogged = false;
    std::string mPresetDirectory = "config/actor-presets";
    std::unordered_map<std::string, std::filesystem::file_time_type> mPresetWrites;
    std::string mSelectedActorPreset;
};

// Cache of parsed role movement presets. A preset name is resolved once through
// MovementJsonConfig::loadPresetInto (which reads config/movement/*.json); the
// parsed MovementConfig is then reused every simulation tick. pollReload()
// refreshes any cached preset whose file changed so role movement never keeps
// stale values after a movement hot reload. Entries are updated in place and
// never erased, so returned pointers stay valid for the process lifetime.
class RoleMovementCache
{
public:
    static RoleMovementCache& instance();

    // Returns the cached config for a preset name, or nullptr when the name is
    // empty or unknown (callers fall back to their legacy/default movement).
    const MovementConfig* get(const std::string& preset);
    // Re-reads cached preset files that changed on disk.
    bool pollReload();

private:
    RoleMovementCache() = default;

    struct Entry
    {
        bool valid = false;
        MovementConfig config;
        std::string path;
        std::filesystem::file_time_type write{};
    };

    std::unordered_map<std::string, Entry> mEntries;
};
