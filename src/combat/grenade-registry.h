// 10 02 2026
/* purpose
* Publish grenade/tool effect definitions from the shared weapon registry.
* Per-weapon config owns the grenade_effect block; this registry only adapts it
* to the area-effect system (fire/smoke/dark-bang) and throw policy.
* Reuses the shared weapon registry for the thrown projectile.
* Does NOT own projectiles, damage application, or rendering.
*/
#pragma once

#include <cstdint>
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
    // Authoritative fixed-tick timing; seconds are derived for display.
    int durationTicks = 0;
    int fuseTicks = 120;
    int damagePerTick = 0;
    int damageIntervalTicks = 10;
    bool damagesEnemiesOnly = true;
    // Frag is a direct explosion (no lingering area); smoke/fire/darkbang leave
    // an area effect.
    bool spawnsAreaEffect = false;
    float directEffectDistance = 5.0f;
    float maxEffectDistance = 20.0f;
    float notLookingMultiplier = 0.5f;
    float noLineOfSightMultiplier = 0.05f;
    int visualTicks = 0;
    float visualRadius = 0.0f;
    float visualAlphaStart = 0.5f;
    float visualAlphaEnd = 0.0f;
    float visualColorR = 1.0f;
    float visualColorG = 1.0f;
    float visualColorB = 1.0f;
    std::string cameraOverlay = "none";
    std::string audioOnApply;

    float durationSeconds() const { return static_cast<float>(durationTicks) / 60.0f; }
    float fuseSeconds() const { return static_cast<float>(fuseTicks) / 60.0f; }
};

class GrenadeRegistry
{
public:
    static GrenadeRegistry& instance();

    // Rebuild from the currently published weapon definitions. Called whenever
    // the weapon registry reloads; hot reload therefore follows the same atomic
    // weapon-config snapshot.
    bool rebuildFromWeapons();
    bool pollReload();

    const GrenadeDefinition* get(const std::string& id) const;
    const std::unordered_map<std::string, GrenadeDefinition>& all() const { return mDefs; }

private:
    GrenadeRegistry() = default;

    std::unordered_map<std::string, GrenadeDefinition> mDefs;
    uint64_t mWeaponRevision = 0;
};

// World-independent selftest for grenade definition loading + area mapping.
bool grenadeRegistrySelfTest(std::string& report);
