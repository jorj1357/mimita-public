// 10 02 2026
// Grenade definition loader (config/grenades.json).
#include "combat/grenade-registry.h"

#include <fstream>

#include <nlohmann/json.hpp>

#include "debug/debug-log.h"
#include "utils/json-comments.h"
#include "utils/path_utils.h"

using json = nlohmann::json;

namespace {

std::filesystem::file_time_type getLastWrite(const std::string& path)
{
    std::error_code ec;
    const auto time = std::filesystem::last_write_time(path, ec);
    return ec ? std::filesystem::file_time_type{} : time;
}

std::string resolveUnder(const std::string& relative)
{
    std::error_code ec;
    if (std::filesystem::exists(relative, ec) && !ec) return relative;
    const auto cwd = std::filesystem::current_path(ec);
    if (!ec && std::filesystem::exists(cwd / relative, ec) && !ec)
        return (cwd / relative).string();
    std::filesystem::path exeDir(getExecutableDirectory());
    for (int level = 0; level < 6 && !exeDir.empty(); ++level) {
        const auto candidate = exeDir / relative;
        if (std::filesystem::exists(candidate, ec) && !ec) return candidate.string();
        const auto parent = exeDir.parent_path();
        if (parent == exeDir) break;
        exeDir = parent;
    }
    return relative;
}

AreaEffectKind areaKindFromString(const std::string& s)
{
    if (s == "fire") return AreaEffectKind::Fire;
    if (s == "smoke") return AreaEffectKind::Smoke;
    if (s == "darkbang" || s == "flash") return AreaEffectKind::DarkBang;
    return AreaEffectKind::None;
}

} // namespace

GrenadeRegistry& GrenadeRegistry::instance()
{
    static GrenadeRegistry registry;
    return registry;
}

bool GrenadeRegistry::load(const std::string& path)
{
    mPath = resolveUnder(path);
    mLastWrite = getLastWrite(mPath);
    std::ifstream file(mPath);
    if (!file.is_open()) {
        Debug::warn(Debug::Category::Weapons,
            "[GRENADE] missing %s; no grenade defs loaded\n", mPath.c_str());
        return false;
    }
    try {
        const json root = parseJsonConfig(file);
        const json* list = nullptr;
        if (root.contains("grenades") && root["grenades"].is_array())
            list = &root["grenades"];
        else if (root.is_array())
            list = &root;
        if (!list) return false;

        std::unordered_map<std::string, GrenadeDefinition> defs;
        for (const auto& item : *list) {
            if (!item.is_object()) continue;
            GrenadeDefinition def;
            def.id = item.value("id", std::string{});
            if (def.id.empty()) continue;
            def.weaponId = item.value("weapon_id", std::string{});
            def.areaKind = areaKindFromString(item.value("area_kind", std::string{}));
            def.radius = std::max(0.5f, item.value("radius", def.radius));
            def.height = std::max(0.0f, item.value("height", def.height));
            def.durationSeconds = std::max(0.0f, item.value("duration_seconds", def.durationSeconds));
            def.damagePerTick = std::max(0, item.value("damage_per_tick", def.damagePerTick));
            def.damageIntervalTicks = std::max(1, item.value("damage_interval_ticks", def.damageIntervalTicks));
            def.damagesEnemiesOnly = item.value("damages_enemies_only", def.damagesEnemiesOnly);
            def.spawnsAreaEffect = item.value("spawns_area_effect", def.areaKind != AreaEffectKind::None);
            defs[def.id] = std::move(def);
        }
        mDefs = std::move(defs);
        Debug::warn(Debug::Category::Weapons,
            "[GRENADE] loaded %zu grenade defs from %s\n", mDefs.size(), mPath.c_str());
        return !mDefs.empty();
    } catch (const std::exception& e) {
        Debug::error(Debug::Category::Weapons,
            "[GRENADE] parse error in %s: %s; keeping previous defs\n", mPath.c_str(), e.what());
        return false;
    }
}

bool GrenadeRegistry::pollReload()
{
    if (mPath.empty()) return false;
    const auto write = getLastWrite(mPath);
    if (write == std::filesystem::file_time_type{} || write == mLastWrite) return false;
    return load(mPath);
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

    GrenadeRegistry& reg = GrenadeRegistry::instance();
    const bool loaded = reg.load();
    report += "loaded=" + std::string(loaded ? "yes" : "no") +
              " defs=" + std::to_string(reg.all().size()) + "\n";
    if (!loaded) { report += "FAIL\n"; return false; }

    const GrenadeDefinition* fire = reg.get("fire");
    if (!fire) fail("fire grenade missing");
    else {
        if (fire->areaKind != AreaEffectKind::Fire) fail("fire area_kind should be Fire");
        if (fire->damagePerTick != 10) fail("fire damage_per_tick should be 10");
        if (fire->damageIntervalTicks != 10) fail("fire damage_interval_ticks should be 10");
        if (!fire->spawnsAreaEffect) fail("fire should spawn an area effect");
    }
    const GrenadeDefinition* smoke = reg.get("smoke");
    if (!smoke || smoke->areaKind != AreaEffectKind::Smoke) fail("smoke grenade missing/mistyped");
    const GrenadeDefinition* dark = reg.get("darkbang");
    if (!dark || dark->areaKind != AreaEffectKind::DarkBang) fail("darkbang grenade missing/mistyped");
    const GrenadeDefinition* frag = reg.get("frag");
    if (!frag) fail("frag grenade missing");

    report += ok ? "PASS\n" : "FAIL\n";
    return ok;
}
