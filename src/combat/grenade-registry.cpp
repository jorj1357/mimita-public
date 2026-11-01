// 10 02 2026
// Grenade effect registry built from per-weapon config.
#include "combat/grenade-registry.h"

#include <algorithm>

#include "combat/weapon-data.h"
#include "combat/weapon-registry.h"
#include "debug/debug-log.h"

namespace {

AreaEffectKind areaKindFromWeaponKind(WeaponGrenadeAreaKind kind)
{
    switch (kind) {
    case WeaponGrenadeAreaKind::Fire: return AreaEffectKind::Fire;
    case WeaponGrenadeAreaKind::Smoke: return AreaEffectKind::Smoke;
    case WeaponGrenadeAreaKind::DarkBang: return AreaEffectKind::DarkBang;
    case WeaponGrenadeAreaKind::None:
    default: return AreaEffectKind::None;
    }
}

} // namespace

GrenadeRegistry& GrenadeRegistry::instance()
{
    static GrenadeRegistry registry;
    return registry;
}

bool GrenadeRegistry::rebuildFromWeapons()
{
    std::unordered_map<std::string, GrenadeDefinition> defs;
    for (const auto& kv : WeaponRegistry::instance().all()) {
        const WeaponDefinition& weapon = kv.second;
        const WeaponGrenadeEffect& effect = weapon.grenadeEffect;
        if (!effect.enabled)
            continue;
        GrenadeDefinition def;
        def.id = weapon.id;
        def.weaponId = weapon.id;
        def.areaKind = areaKindFromWeaponKind(effect.areaKind);
        def.radius = std::max(0.5f, effect.radius);
        def.height = std::max(0.0f, effect.height);
        def.durationSeconds = std::max(0.0f, effect.durationSeconds);
        def.damagePerTick = std::max(0, effect.damagePerTick);
        def.damageIntervalTicks = std::max(1, effect.damageIntervalTicks);
        def.damagesEnemiesOnly = effect.damagesEnemiesOnly;
        def.spawnsAreaEffect = effect.spawnsAreaEffect &&
            def.areaKind != AreaEffectKind::None;
        def.fuseSeconds = std::max(0.0f, effect.fuseSeconds);
        def.directEffectDistance = std::max(0.0f, effect.directEffectDistance);
        def.maxEffectDistance = std::max(def.directEffectDistance,
            effect.maxEffectDistance);
        def.notLookingMultiplier = std::clamp(effect.notLookingMultiplier, 0.0f, 1.0f);
        def.noLineOfSightMultiplier = std::clamp(effect.noLineOfSightMultiplier, 0.0f, 1.0f);
        def.audioOnApply = effect.audioOnApply;
        defs[def.id] = std::move(def);
    }
    mDefs = std::move(defs);
    mWeaponRevision = WeaponRegistry::instance().revision();
    Debug::log(Debug::Category::Weapons,
        "[GRENADE] published %zu grenade effects from weapon definitions\n",
        mDefs.size());
    return !mDefs.empty();
}

bool GrenadeRegistry::pollReload()
{
    if (WeaponRegistry::instance().revision() == mWeaponRevision)
        return false;
    return rebuildFromWeapons();
}

const GrenadeDefinition* GrenadeRegistry::get(const std::string& id) const
{
    auto it = mDefs.find(id);
    return it == mDefs.end() ? nullptr : &it->second;
}

bool grenadeRegistrySelfTest(std::string& report)
{
    bool ok = true;
    auto fail = [&](const std::string& why) { ok = false; report += "FAIL: " + why + "\n"; };

    WeaponData::registerBuiltinWeapons();
    GrenadeRegistry& reg = GrenadeRegistry::instance();
    const bool loaded = reg.rebuildFromWeapons();
    report += "loaded=" + std::string(loaded ? "yes" : "no") +
              " defs=" + std::to_string(reg.all().size()) + "\n";
    if (!loaded) { report += "FAIL\n"; return false; }

    const GrenadeDefinition* fire = reg.get("fire");
    if (!fire) fail("fire grenade missing");
    else {
        if (fire->areaKind != AreaEffectKind::Fire) fail("fire area_kind should be Fire");
        if (fire->radius != 5.0f) fail("fire radius should be 5 m");
        if (fire->damagePerTick <= 0) fail("fire damage_per_tick should be positive");
        if (!fire->spawnsAreaEffect) fail("fire should spawn an area effect");
    }
    const GrenadeDefinition* smoke = reg.get("smoke");
    if (!smoke || smoke->areaKind != AreaEffectKind::Smoke) fail("smoke grenade missing/mistyped");
    else {
        if (smoke->radius != 10.0f) fail("smoke radius should be 10 m (20 m diameter)");
        if (smoke->durationSeconds != 10.0f) fail("smoke lifetime should be 10 s");
    }
    const GrenadeDefinition* dark = reg.get("darkbang");
    if (!dark || dark->areaKind != AreaEffectKind::DarkBang) fail("darkbang grenade missing/mistyped");
    const GrenadeDefinition* frag = reg.get("frag");
    if (!frag) fail("frag grenade missing");
    else if (frag->spawnsAreaEffect) fail("frag should not leave a lingering area effect");

    report += ok ? "PASS\n" : "FAIL\n";
    return ok;
}
