// 10 02 2026
/* purpose
* Load generic grenade/tool definitions from config/grenades.json. Each entry
* maps a grenade id to a weapon id plus the area effect it leaves behind
* (fire/smoke/dark-bang) and its throw/impact policy.
* Reuses the shared weapon registry for the thrown projectile; this registry
* only owns the grenade-to-area-effect mapping.
* Does NOT own projectiles, damage application, or rendering.
*/
#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

#include "combat/area-effect.h"

struct GrenadeDefinition
{
    std::string id;                 // "frag", "smoke", "darkbang", "fire"
    std::string weaponId;           // weapon/tool used to throw it
    AreaEffectKind areaKind = AreaEffectKind::None;
    float radius = 4.0f;
    float height = 3.0f;
    float durationSeconds = 0.0f;
    int damagePerTick = 0;
    int damageIntervalTicks = 10;
    bool damagesEnemiesOnly = true;
    // Frag is a direct explosion (no lingering area); smoke/fire/darkbang leave
    // an area effect.
    bool spawnsAreaEffect = false;
    float fuseSeconds = 2.0f;
    float directEffectDistance = 5.0f;
    float maxEffectDistance = 20.0f;
    float notLookingMultiplier = 0.5f;
    float noLineOfSightMultiplier = 0.05f;
};

class GrenadeRegistry
{
public:
    static GrenadeRegistry& instance();

    bool load(const std::string& path = "config/grenades.json");
    bool pollReload();

    const GrenadeDefinition* get(const std::string& id) const;
    const std::unordered_map<std::string, GrenadeDefinition>& all() const { return mDefs; }

private:
    GrenadeRegistry() = default;
    std::string resolvePath(const std::string& path) const;

    std::unordered_map<std::string, GrenadeDefinition> mDefs;
    std::filesystem::file_time_type mLastWrite{};
    std::string mPath = "config/grenades.json";
};

// World-independent selftest for grenade definition loading + area mapping.
bool grenadeRegistrySelfTest(std::string& report);
