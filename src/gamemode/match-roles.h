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

#include "physics/movement/movement-types.h"

struct MatchRoleDefinition
{
    std::string id;
    int team = -1;  // preferred team, -1 = any
    int health = 0; // 0 = no override (use default/legacy max health)
    std::string movementPreset;
    std::string weaponSet;
    std::string startingWeapon;
    std::string behaviorProfile;
};

class MatchRoleRegistry
{
public:
    static MatchRoleRegistry& instance();

    bool load(const std::string& path = "config/roles.json");
    bool pollReload();

    const MatchRoleDefinition* get(const std::string& id) const;
    const std::vector<MatchRoleDefinition>& all() const { return mRoles; }

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
