// Runtime actor-preset weapon overlay. This owns only the merge from a loaded
// actor preset into WeaponRegistry; it does not own JSON parsing or firing.
#include "combat/actor-preset-weapons.h"

#include "combat/weapon-registry.h"
#include "gamemode/match-roles.h"

#include <algorithm>
#include <utility>

namespace ActorPresetWeapons {
namespace {
std::string gActivePreset;
bool gDamageNumbersEnabled = true;
bool gHitEffectsEnabled = true;
bool gWorldImpactEffectsEnabled = true;
bool gBloodEffectsEnabled = true;
bool gMuzzleFlashEnabled = true;

void applyPresentation(WeaponDefinition& weapon,
                       const ActorPresetPresentation& p)
{
    if (p.hasDamageNumbers) weapon.damageNumbersEnabled = p.damageNumbers;
    if (p.hasHitEffects) weapon.hitEffectsEnabled = p.hitEffects;
    if (p.hasWorldImpactEffects) weapon.worldImpactEffectsEnabled = p.worldImpactEffects;
    if (p.hasBloodEffects) weapon.bloodEffectsEnabled = p.bloodEffects;
    if (p.hasMuzzleFlash) weapon.muzzleFlashEnabled = p.muzzleFlash;
}

void applyOverride(WeaponDefinition& weapon,
                   const ActorPresetWeaponOverride& o)
{
    if (o.hasDamage) weapon.damage = o.damage;
    if (o.hasFireDelay) weapon.fireDelay = std::max(0.0f, o.fireDelay);
    if (o.hasReloadTime) weapon.reloadTime = std::max(0.0f, o.reloadTime);
    if (o.hasMagazineSize) weapon.magazineSize = std::max(0, o.magazineSize);
    if (o.hasReserveAmmo) weapon.reserveSize = std::max(0, o.reserveAmmo);
    if (o.hasHitscan) weapon.hitscan = o.hitscan;
    if (o.hasBeamThickness) weapon.beamThickness = std::max(0.0f, o.beamThickness);
    if (o.hasWorldThickness) weapon.beamWorldThickness = std::max(0.0f, o.worldThickness);
    if (o.hasRange) weapon.customParams["range"] = std::max(0.0f, o.range);
    if (o.hasTracerEnabled) weapon.tracerEnabled = o.tracerEnabled;
    if (o.hasTracerThickness) weapon.tracerThickness = std::max(0.0f, o.tracerThickness);

    if (!o.allowedBodyParts.empty()) {
        weapon.allowedBodyParts.clear();
        for (const auto& part : o.allowedBodyParts)
            weapon.allowedBodyParts.insert(part);
    }
    applyPresentation(weapon, o.presentation);
}
} // namespace

bool apply(const MatchRoleDefinition& preset)
{
    auto definitions = WeaponRegistry::instance().all();
    std::unordered_map<std::string, WeaponDefinition> effective = definitions;

    for (auto& entry : effective)
        applyPresentation(entry.second, preset.presentation);

    gDamageNumbersEnabled = !preset.presentation.hasDamageNumbers || preset.presentation.damageNumbers;
    gHitEffectsEnabled = !preset.presentation.hasHitEffects || preset.presentation.hitEffects;
    gWorldImpactEffectsEnabled = !preset.presentation.hasWorldImpactEffects || preset.presentation.worldImpactEffects;
    gBloodEffectsEnabled = !preset.presentation.hasBloodEffects || preset.presentation.bloodEffects;
    gMuzzleFlashEnabled = !preset.presentation.hasMuzzleFlash || preset.presentation.muzzleFlash;

    for (const auto& entry : preset.weaponOverrides) {
        auto it = effective.find(entry.first);
        if (it == effective.end())
            continue;
        applyOverride(it->second, entry.second);
    }

    WeaponRegistry::instance().setActiveDefinitions(std::move(effective));
    gActivePreset = preset.id;
    return true;
}

bool refresh()
{
    if (gActivePreset.empty()) return false;
    const MatchRoleDefinition* preset =
        MatchRoleRegistry::instance().getActorPreset(gActivePreset);
    return preset ? apply(*preset) : false;
}

void clear()
{
    WeaponRegistry::instance().clearActiveDefinitions();
    gActivePreset.clear();
    gDamageNumbersEnabled = true;
    gHitEffectsEnabled = true;
    gWorldImpactEffectsEnabled = true;
    gBloodEffectsEnabled = true;
    gMuzzleFlashEnabled = true;
}

const std::string& activePresetId()
{
    return gActivePreset;
}

bool damageNumbersEnabled() { return gDamageNumbersEnabled; }
bool hitEffectsEnabled() { return gHitEffectsEnabled; }
bool worldImpactEffectsEnabled() { return gWorldImpactEffectsEnabled; }
bool bloodEffectsEnabled() { return gBloodEffectsEnabled; }
bool muzzleFlashEnabled() { return gMuzzleFlashEnabled; }

} // namespace ActorPresetWeapons
