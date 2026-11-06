// 10 02 2026
// Grenade effect registry built from per-weapon config.
#include "combat/grenade-registry.h"

#include <algorithm>

#include "combat/weapon-data.h"
#include "combat/weapon-registry.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"

#include <nlohmann/json.hpp>

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
        def.durationTicks = std::max(0, effect.durationTicks);
        def.fuseTicks = std::max(1, effect.fuseTicks);
        def.damagePerTick = std::max(0, effect.damagePerTick);
        def.damageIntervalTicks = std::max(1, effect.damageIntervalTicks);
        def.damagesEnemiesOnly = effect.damagesEnemiesOnly;
        def.spawnsAreaEffect = effect.spawnsAreaEffect &&
            def.areaKind != AreaEffectKind::None;
        def.directEffectDistance = std::max(0.0f, effect.directEffectDistance);
        def.maxEffectDistance = std::max(def.directEffectDistance,
            effect.maxEffectDistance);
        def.notLookingMultiplier = std::clamp(effect.notLookingMultiplier, 0.0f, 1.0f);
        def.noLineOfSightMultiplier = std::clamp(effect.noLineOfSightMultiplier, 0.0f, 1.0f);
        def.visualTicks = std::max(0, effect.visualTicks);
        def.visualRadius = std::max(0.0f, effect.visualRadius);
        def.visualAlphaStart = std::clamp(effect.visualAlphaStart, 0.0f, 1.0f);
        def.visualAlphaEnd = std::clamp(effect.visualAlphaEnd, 0.0f, 1.0f);
        def.visualColorR = std::clamp(effect.visualColorR, 0.0f, 1.0f);
        def.visualColorG = std::clamp(effect.visualColorG, 0.0f, 1.0f);
        def.visualColorB = std::clamp(effect.visualColorB, 0.0f, 1.0f);
        def.cameraOverlay = effect.cameraOverlay;
        def.audioOnApply = effect.audioOnApply;
        defs[def.id] = std::move(def);
    }
    mDefs = std::move(defs);
    mWeaponRevision = WeaponRegistry::instance().revision();
    Debug::log(Debug::Category::Weapons,
        "[GRENADE] published %zu grenade effects from weapon definitions\n",
        mDefs.size());
    for (const auto& kv : mDefs) {
        const GrenadeDefinition& g = kv.second;
        const WeaponDefinition* weapon = WeaponRegistry::instance().get(g.weaponId);
        const auto param = [weapon](const char* key, float fallback) {
            if (!weapon) return fallback;
            const auto it = weapon->customParams.find(key);
            return it == weapon->customParams.end() ? fallback : it->second;
        };
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Weapons, StructuredLevel::Important,
            "grenade.config.resolved", g.id, "weapon-snapshot", 0,
            nlohmann::json{{"id", g.id},
                           {"area_kind", (int)g.areaKind},
                           {"radius_m", g.radius},
                           {"duration_ticks", g.durationTicks},
                           {"duration_s", g.durationSeconds()},
                           {"fuse_ticks", g.fuseTicks},
                           {"damage_per_tick", g.damagePerTick},
                           {"damage_interval_ticks", g.damageIntervalTicks},
                           {"damages_enemies_only", g.damagesEnemiesOnly},
                           {"visual_ticks", g.visualTicks},
                           {"visual_radius_m", g.visualRadius},
                           {"visual_alpha_start", g.visualAlphaStart},
                           {"visual_alpha_end", g.visualAlphaEnd},
                           {"projectile_radius_m", weapon ? weapon->projectileRadius : 0.0f},
                           {"forward_speed_mps", weapon ? weapon->projectileSpeed : 0.0f},
                           {"upward_bias_mps", param("upBias", 0.0f)},
                           {"gravity_mps2", param("gravity", 0.0f)},
                           {"drag", param("drag", 0.0f)},
                           {"restitution", param("bounceRestitution", 0.0f)},
                           {"friction", param("bounceFriction", 0.0f)},
                           {"max_bounces", param("maxBounceCount", 0.0f)},
                           {"visual_shape", param("projectileVisualSphere", 0.0f) > 0.5f
                               ? "sphere" : "procedural_cylinder"},
                           {"visual_length_m", param("projectileVisualLength", 0.0f)},
                           {"visual_radius_m", param("projectileVisualRadius", 0.0f)},
                           {"outline_enabled", param("projectileOutlineEnabled", 0.0f) > 0.5f},
                           {"glow_enabled", param("projectileGlowEnabled", 0.0f) > 0.5f},
                           {"light_enabled", param("projectileLightEnabled", 0.0f) > 0.5f},
                           {"light_offset", {param("projectileLightOffsetX", 0.0f),
                                              param("projectileLightOffsetY", 0.0f),
                                              param("projectileLightOffsetZ", 0.0f)}},
                           {"light_radius_m", param("projectileLightRadius", 0.0f)},
                           {"camera_overlay", g.cameraOverlay}});
    }
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
        if (smoke->durationTicks != 600) fail("smoke lifetime should be 600 ticks (10 s)");
    }
    const GrenadeDefinition* dark = reg.get("darkbang");
    if (!dark || dark->areaKind != AreaEffectKind::DarkBang) fail("darkbang grenade missing/mistyped");
    else {
        if (dark->fuseTicks != 120) fail("darkbang fuse should be 120 ticks");
        if (dark->visualTicks != 30) fail("darkbang visual should be 30 ticks");
        if (dark->cameraOverlay != "world_hud") fail("darkbang should overlay world_hud");
    }
    const GrenadeDefinition* frag = reg.get("frag");
    if (!frag) fail("frag grenade missing");
    else if (frag->spawnsAreaEffect) fail("frag should not leave a lingering area effect");
    else {
        if (frag->fuseTicks != 120) fail("frag fuse should be 120 ticks");
        if (frag->visualTicks != 30) fail("frag blast visual should be 30 ticks");
    }

    report += ok ? "PASS\n" : "FAIL\n";
    return ok;
}
