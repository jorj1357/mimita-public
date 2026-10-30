// Runtime actor-preset weapon overlay. This owns only the merge from a loaded
// actor preset into WeaponRegistry; it does not own JSON parsing or firing.
#include "combat/actor-preset-weapons.h"

#include "combat/weapon-registry.h"
#include "gamemode/gamemode.h"
#include "gamemode/match-roles.h"

#include <algorithm>
#include <optional>
#include <utility>

namespace ActorPresetWeapons {
namespace {
std::string gActivePreset;
bool gDamageNumbersEnabled = true;
bool gHitEffectsEnabled = true;
bool gWorldImpactEffectsEnabled = true;
bool gBloodEffectsEnabled = true;
bool gMuzzleFlashEnabled = true;
bool gHitMarkersEnabled = true;
bool gHitSoundsEnabled = true;
std::optional<GamemodePresentation> gModePresentation;

bool sameModePresentation(const GamemodePresentation& a,
                          const GamemodePresentation& b)
{
    return a.hasDamageNumbers == b.hasDamageNumbers && a.damageNumbers == b.damageNumbers &&
        a.hasHitEffects == b.hasHitEffects && a.hitEffects == b.hitEffects &&
        a.hasWorldImpactEffects == b.hasWorldImpactEffects && a.worldImpactEffects == b.worldImpactEffects &&
        a.hasWorldImpactDecals == b.hasWorldImpactDecals && a.worldImpactDecals == b.worldImpactDecals &&
        a.hasWorldHitEffects == b.hasWorldHitEffects && a.worldHitEffects == b.worldHitEffects &&
        a.hasHitMarkers == b.hasHitMarkers && a.hitMarkers == b.hitMarkers &&
        a.hasHitSounds == b.hasHitSounds && a.hitSounds == b.hitSounds &&
        a.hasHitFlinch == b.hasHitFlinch && a.hitFlinch == b.hitFlinch &&
        a.hasBlood == b.hasBlood && a.blood == b.blood &&
        a.hasKillfeed == b.hasKillfeed && a.killfeed == b.killfeed &&
        a.hasRagdolls == b.hasRagdolls && a.ragdolls == b.ragdolls &&
        a.hasEnemyHealthbars == b.hasEnemyHealthbars && a.enemyHealthbars == b.enemyHealthbars &&
        a.hasPlayerOutlines == b.hasPlayerOutlines && a.playerOutlines == b.playerOutlines &&
        a.hasFriendlyOutlines == b.hasFriendlyOutlines && a.friendlyOutlines == b.friendlyOutlines &&
        a.hasEnemyOutlineAlpha == b.hasEnemyOutlineAlpha && a.enemyOutlineAlpha == b.enemyOutlineAlpha &&
        a.hasTeamOutlineColors == b.hasTeamOutlineColors &&
        a.bloodScale == b.bloodScale && a.bloodLifetimeScale == b.bloodLifetimeScale &&
        a.worldImpactDecalScale == b.worldImpactDecalScale &&
        a.worldImpactDecalLifetimeScale == b.worldImpactDecalLifetimeScale &&
        a.worldImpactDecalAlphaScale == b.worldImpactDecalAlphaScale &&
        a.worldHitEffectScale == b.worldHitEffectScale &&
        a.worldHitEffectLifetimeScale == b.worldHitEffectLifetimeScale &&
        a.friendlyOutlineColor.x == b.friendlyOutlineColor.x &&
        a.friendlyOutlineColor.y == b.friendlyOutlineColor.y &&
        a.friendlyOutlineColor.z == b.friendlyOutlineColor.z &&
        a.enemyOutlineColor.x == b.enemyOutlineColor.x &&
        a.enemyOutlineColor.y == b.enemyOutlineColor.y &&
        a.enemyOutlineColor.z == b.enemyOutlineColor.z;
}

std::optional<bool> modeFlag(PresentationFlag flag)
{
    if (!gModePresentation) return std::nullopt;
    const auto& p = *gModePresentation;
    switch (flag) {
    case PresentationFlag::DamageNumbers:
        if (p.hasDamageNumbers) return p.damageNumbers;
        break;
    case PresentationFlag::HitEffects:
        if (p.hasHitEffects) return p.hitEffects;
        break;
    case PresentationFlag::WorldImpactEffects:
        if (p.hasWorldImpactEffects) return p.worldImpactEffects;
        break;
    case PresentationFlag::Blood:
        if (p.hasBlood) return p.blood;
        break;
    case PresentationFlag::HitMarkers:
        if (p.hasHitMarkers) return p.hitMarkers;
        break;
    case PresentationFlag::HitSounds:
        if (p.hasHitSounds) return p.hitSounds;
        break;
    }
    return std::nullopt;
}

void applyModePresentation(WeaponDefinition& weapon)
{
    if (!gModePresentation) return;
    const auto& p = *gModePresentation;
    if (p.hasDamageNumbers) weapon.damageNumbersEnabled = p.damageNumbers;
    if (p.hasHitEffects) weapon.hitEffectsEnabled = p.hitEffects;
    if (p.hasWorldImpactEffects) weapon.worldImpactEffectsEnabled = p.worldImpactEffects;
    if (p.hasBlood) weapon.bloodEffectsEnabled = p.blood;
}

void applyModePresentationToRegistry()
{
    auto effective = WeaponRegistry::instance().all();
    for (auto& entry : effective) applyModePresentation(entry.second);
    WeaponRegistry::instance().setActiveDefinitions(std::move(effective));
}

void applyModeGlobalFlags()
{
    if (!gModePresentation) return;
    const auto& p = *gModePresentation;
    if (p.hasDamageNumbers) gDamageNumbersEnabled = p.damageNumbers;
    if (p.hasHitEffects) gHitEffectsEnabled = p.hitEffects;
    if (p.hasWorldImpactEffects) gWorldImpactEffectsEnabled = p.worldImpactEffects;
    if (p.hasBlood) gBloodEffectsEnabled = p.blood;
    if (p.hasHitMarkers) gHitMarkersEnabled = p.hitMarkers;
    if (p.hasHitSounds) gHitSoundsEnabled = p.hitSounds;
}

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
    if (o.hasDamageScale) weapon.damageScale = std::max(0.0f, o.damageScale);
    if (o.hasHeadshotMultiplier) weapon.headshotMultiplier = std::max(0.0f, o.headshotMultiplier);
    if (o.hasSpread) weapon.spread = std::max(0.0f, o.spread);
    if (o.hasRecoil) weapon.recoil = std::max(0.0f, o.recoil);
    if (o.hasFireDelay) weapon.fireDelay = std::max(0.0f, o.fireDelay);
    if (o.hasReloadTime) weapon.reloadTime = std::max(0.0f, o.reloadTime);
    if (o.hasMagazineSize) weapon.magazineSize = std::max(0, o.magazineSize);
    if (o.hasReserveAmmo) weapon.reserveSize = std::max(0, o.reserveAmmo);
    if (o.hasHitscan) weapon.hitscan = o.hitscan;
    if (o.hasBeamThickness) weapon.beamThickness = std::max(0.0f, o.beamThickness);
    if (o.hasWorldThickness) weapon.beamWorldThickness = std::max(0.0f, o.worldThickness);
    if (o.hasSquareSpread) weapon.squareSpread = o.squareSpread;
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
    gHitMarkersEnabled = !preset.presentation.hasHitMarkers || preset.presentation.hitMarkers;
    gHitSoundsEnabled = !preset.presentation.hasHitSounds || preset.presentation.hitSounds;

    for (const auto& entry : preset.weaponOverrides) {
        auto it = effective.find(entry.first);
        if (it == effective.end())
            continue;
        applyOverride(it->second, entry.second);
    }

    // Explicit match-wide values take precedence over actor and weapon preset
    // presentation after their regular overlays have been resolved.
    for (auto& entry : effective) applyModePresentation(entry.second);

    gDamageNumbersEnabled = true;
    gHitEffectsEnabled = true;
    gWorldImpactEffectsEnabled = true;
    gBloodEffectsEnabled = true;
    gHitMarkersEnabled = true;
    gHitSoundsEnabled = true;
    if (preset.presentation.hasDamageNumbers) gDamageNumbersEnabled = preset.presentation.damageNumbers;
    if (preset.presentation.hasHitEffects) gHitEffectsEnabled = preset.presentation.hitEffects;
    if (preset.presentation.hasWorldImpactEffects) gWorldImpactEffectsEnabled = preset.presentation.worldImpactEffects;
    if (preset.presentation.hasBloodEffects) gBloodEffectsEnabled = preset.presentation.bloodEffects;
    if (preset.presentation.hasHitMarkers) gHitMarkersEnabled = preset.presentation.hitMarkers;
    if (preset.presentation.hasHitSounds) gHitSoundsEnabled = preset.presentation.hitSounds;
    applyModeGlobalFlags();

    WeaponRegistry::instance().setActiveDefinitions(std::move(effective));
    gActivePreset = preset.id;
    return true;
}

bool refresh()
{
    if (gActivePreset.empty()) {
        if (!gModePresentation) return false;
        applyModePresentationToRegistry();
        return true;
    }
    const MatchRoleDefinition* preset =
        MatchRoleRegistry::instance().getActorPreset(gActivePreset);
    return preset ? apply(*preset) : false;
}

void clear()
{
    gActivePreset.clear();
    gDamageNumbersEnabled = true;
    gHitEffectsEnabled = true;
    gWorldImpactEffectsEnabled = true;
    gBloodEffectsEnabled = true;
    gMuzzleFlashEnabled = true;
    gHitMarkersEnabled = true;
    gHitSoundsEnabled = true;
    if (gModePresentation) {
        applyModeGlobalFlags();
        applyModePresentationToRegistry();
    } else {
        WeaponRegistry::instance().clearActiveDefinitions();
    }
}

void setModePresentationOverride(const GamemodePresentation& presentation)
{
    if (gModePresentation && sameModePresentation(*gModePresentation, presentation))
        return;
    gModePresentation = presentation;
    if (!gActivePreset.empty()) {
        refresh();
        return;
    }
    gDamageNumbersEnabled = true;
    gHitEffectsEnabled = true;
    gWorldImpactEffectsEnabled = true;
    gBloodEffectsEnabled = true;
    gHitMarkersEnabled = true;
    gHitSoundsEnabled = true;
    applyModeGlobalFlags();
    applyModePresentationToRegistry();
}

void clearModePresentationOverride()
{
    gModePresentation.reset();
    if (!gActivePreset.empty()) {
        refresh();
        return;
    }
    WeaponRegistry::instance().clearActiveDefinitions();
    gDamageNumbersEnabled = true;
    gHitEffectsEnabled = true;
    gWorldImpactEffectsEnabled = true;
    gBloodEffectsEnabled = true;
    gHitMarkersEnabled = true;
    gHitSoundsEnabled = true;
}

std::optional<bool> modePresentationOverride(PresentationFlag flag)
{
    return modeFlag(flag);
}

std::optional<GamemodePresentation> modePresentationSettings()
{
    return gModePresentation;
}

const std::string& activePresetId()
{
    return gActivePreset;
}

bool avatarAllowedForActivePreset(const std::string& avatar)
{
    if (gActivePreset.empty()) return true;
    const MatchRoleDefinition* preset =
        MatchRoleRegistry::instance().getActorPreset(gActivePreset);
    return !preset || isAvatarAllowedByPolicy(
        avatar, preset->allowedAvatars, preset->deniedAvatars);
}

bool damageNumbersEnabled() { return gDamageNumbersEnabled; }
bool hitEffectsEnabled() { return gHitEffectsEnabled; }
bool worldImpactEffectsEnabled() { return gWorldImpactEffectsEnabled; }
bool bloodEffectsEnabled() { return gBloodEffectsEnabled; }
bool muzzleFlashEnabled() { return gMuzzleFlashEnabled; }
bool hitMarkersEnabled() { return gHitMarkersEnabled; }
bool hitSoundsEnabled() { return gHitSoundsEnabled; }

} // namespace ActorPresetWeapons
