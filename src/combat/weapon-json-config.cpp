#include "weapon-json-config.h"

#include "weapon-data.h"
#include "weapon-registry.h"
#include "../debug/debug-log.h"
#include "../network/network-weapons.h"
#include "hot-reload/game-api.h"
#include "hot-reload/generic-runtime.h"
#include "hot-reload/hot-reload-system.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace WeaponData {
namespace {

using json = nlohmann::json;

// Resolve config/weapons.json across candidate locations so the file is found
// regardless of the process working directory (launcher runs extract the game
// to a different folder than the repo). Candidates are checked in order.
static std::string resolveWeaponConfigPathOnce()
{
    const char* primary = "config/weapons.json";
    std::error_code ec;
    if (std::filesystem::exists(primary, ec) && !ec)
        return primary;

    char exePath[MAX_PATH];
    const DWORD n = GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    if (n > 0 && n < MAX_PATH)
    {
        const std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();
        const std::vector<std::string> candidates = {
            (exeDir / "config" / "weapons.json").string(),
            (exeDir.parent_path() / "config" / "weapons.json").string(),
        };
        for (const std::string& c : candidates)
        {
            if (std::filesystem::exists(c, ec) && !ec)
                return c;
        }
    }
    return primary;  // fall back to CWD-relative (may fail -> builtin defaults)
}

const std::string& weaponConfigPath()
{
    static const std::string path = resolveWeaponConfigPathOnce();
    return path;
}

json gWeaponConfigRoot = json::object();
std::filesystem::file_time_type gWeaponConfigLastWrite{};
std::chrono::steady_clock::time_point gWeaponConfigLastCheck{};
bool gWeaponConfigHasWriteTime = false;

std::string normalizedToken(std::string value)
{
    value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char c) {
        return c == '_' || c == '-' || c == ' ';
    }), value.end());
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return (char)std::tolower(c);
    });
    return value;
}

bool weaponJsonVec3(const json& root, const char* key, glm::vec3& out)
{
    if (!root.contains(key) || !root[key].is_array() || root[key].size() < 3)
        return false;
    out = glm::vec3(root[key][0].get<float>(), root[key][1].get<float>(), root[key][2].get<float>());
    return true;
}

void weaponJsonFloat(const json& root, const char* key, float& out)
{
    if (root.contains(key) && root[key].is_number())
        out = root[key].get<float>();
}

void weaponJsonInt(const json& root, const char* key, int& out)
{
    if (root.contains(key) && root[key].is_number_integer())
        out = root[key].get<int>();
}

void weaponJsonBool(const json& root, const char* key, bool& out)
{
    if (root.contains(key) && root[key].is_boolean())
        out = root[key].get<bool>();
}

void weaponJsonString(const json& root, const char* key, std::string& out)
{
    if (root.contains(key) && root[key].is_string())
        out = root[key].get<std::string>();
}

void weaponJsonFireMode(const json& root, WeaponDefinition& def)
{
    if (!root.contains("fire_mode") || !root["fire_mode"].is_string())
        return;
    const std::string mode = normalizedToken(root["fire_mode"].get<std::string>());
    if (mode == "automatic" || mode == "auto")
        def.fireMode = WeaponFireMode::Automatic;
    else if (mode == "charge")
        def.fireMode = WeaponFireMode::Charge;
    else
        def.fireMode = WeaponFireMode::SemiAuto;
}

void weaponJsonNetworkMode(const json& root, WeaponDefinition& def)
{
    if (!root.contains("network_mode") || !root["network_mode"].is_string())
        return;
    const std::string mode = normalizedToken(root["network_mode"].get<std::string>());
    def.networkMode = mode == "clientonly"
        ? WeaponNetworkMode::ClientOnly
        : WeaponNetworkMode::Normal;
}

void weaponJsonBehaviorType(const json& root, WeaponDefinition& def)
{
    if (!root.contains("behavior_type") || !root["behavior_type"].is_string())
        return;
    const std::string type = normalizedToken(root["behavior_type"].get<std::string>());
    if (type == "projectile")
        def.behaviorType = WeaponBehaviorType::Projectile;
    else if (type == "godball")
        def.behaviorType = WeaponBehaviorType::Godball;
    else if (type == "melee")
        def.behaviorType = WeaponBehaviorType::Melee;
    else if (type == "swordsword")
        def.behaviorType = WeaponBehaviorType::Swordsword;
    else if (type == "hafs")
        def.behaviorType = WeaponBehaviorType::Hafs;
    else if (type == "quickhit" || type == "quick_hit")
        def.behaviorType = WeaponBehaviorType::QuickHit;
    else if (type == "spyknife" || type == "spy_knife")
        def.behaviorType = WeaponBehaviorType::SpyKnife;
    else if (type == "rocketlauncher" || type == "rocket")
        def.behaviorType = WeaponBehaviorType::RocketLauncher;
    else if (type == "grenadelauncher" || type == "grenade")
        def.behaviorType = WeaponBehaviorType::GrenadeLauncher;
    else if (type == "throwngrenade" || type == "throwable" || type == "grenadethrow")
        def.behaviorType = WeaponBehaviorType::Grenade;
    else
        def.behaviorType = WeaponBehaviorType::Hitscan;
}

void applyWeaponExecutionType(WeaponDefinition& def)
{
    def.executionType = weaponExecutionTypeForBehavior(def.behaviorType);
}

void applyWeaponIdentityJson(WeaponDefinition& def, const json& root)
{
    weaponJsonString(root, "id", def.id);
    weaponJsonString(root, "display_name", def.displayName);
    weaponJsonString(root, "displayName", def.displayName);
    weaponJsonInt(root, "slot", def.slot);
    if (root.contains("model") && root["model"].is_object())
        weaponJsonString(root["model"], "path", def.modelPath);
    if (root.contains("viewmodel") && root["viewmodel"].is_object()) {
        const json& view = root["viewmodel"];
        weaponJsonVec3(view, "position", def.viewModelOffset);
        weaponJsonVec3(view, "rotation_degrees", def.viewModelRotation);
        if (view.contains("attachment") && view["attachment"].is_object()) {
            weaponJsonVec3(view["attachment"], "position", def.attachmentOffset);
            weaponJsonVec3(view["attachment"], "rotation_degrees", def.attachmentRotation);
        }
        if (view.contains("scale") && view["scale"].is_number())
            def.weaponScale = view["scale"].get<float>();
        else if (view.contains("scale") && view["scale"].is_array() && view["scale"].size() >= 1)
            def.weaponScale = view["scale"][0].get<float>();
    }
}

void applyWeaponStatsJson(WeaponDefinition& def, const json& root)
{
    weaponJsonFloat(root, "damage", def.damage);
    weaponJsonFloat(root, "headshot_multiplier", def.headshotMultiplier);
    weaponJsonFloat(root, "fire_delay", def.fireDelay);
    weaponJsonFloat(root, "reload_time", def.reloadTime);
    weaponJsonInt(root, "magazine_size", def.magazineSize);
    weaponJsonInt(root, "pellet_count", def.pelletCount);
    weaponJsonFloat(root, "spread", def.spread);
    weaponJsonFloat(root, "recoil", def.recoil);
    weaponJsonFloat(root, "projectile_speed", def.projectileSpeed);
    weaponJsonFloat(root, "projectile_radius", def.projectileRadius);
    weaponJsonFloat(root, "projectile_lifetime", def.projectileLifetime);
    weaponJsonFireMode(root, def);
    weaponJsonNetworkMode(root, def);
    weaponJsonBehaviorType(root, def);
    weaponJsonBool(root, "hitscan", def.hitscan);
    weaponJsonFloat(root, "beam_thickness", def.beamThickness);
    weaponJsonFloat(root, "beam_world_thickness", def.beamWorldThickness);
    weaponJsonBool(root, "uses_physics_projectile", def.usesPhysicsProjectile);
    weaponJsonString(root, "pose_id", def.poseId);
}

void applyWeaponKnockbackJson(WeaponDefinition& def, const json& root)
{
    // Top-level keys win; custom_params act as the legacy fallback.
    const auto readKb = [&root](const char* key, float& out) {
        if (root.contains(key) && root[key].is_number())
            out = root[key].get<float>();
        else if (root.contains("custom_params") && root["custom_params"].is_object() &&
                 root["custom_params"].contains(key) &&
                 root["custom_params"][key].is_number())
            out = root["custom_params"][key].get<float>();
    };
    readKb("shooter_knockback", def.shooterKnockback);
    readKb("shooter_knockback_vertical", def.shooterKnockbackVertical);
    readKb("self_impulse_multiplier", def.selfImpulseMultiplier);
    readKb("victim_knockback", def.victimKnockback);
    readKb("victim_knockback_per_damage", def.victimKnockbackPerDamage);
    readKb("victim_knockback_vertical_fraction", def.victimKnockbackVerticalFraction);
    readKb("enemy_impulse_multiplier", def.enemyImpulseMultiplier);
}

void applyWeaponSoundJson(WeaponDefinition& def, const json& root)
{
    if (root.contains("sound") && root["sound"].is_object()) {
        const json& sound = root["sound"];
        weaponJsonString(sound, "shoot", def.soundShoot);
        weaponJsonString(sound, "reload", def.soundReload);
        weaponJsonString(sound, "hit", def.soundHit);
        weaponJsonString(sound, "dry_fire", def.soundDryFire);
        weaponJsonString(sound, "equip", def.soundEquip);
        def.soundPitchVariation = sound.value("pitch_variation", def.soundPitchVariation);
        def.soundVolumeVariation = sound.value("volume_variation", def.soundVolumeVariation);
    }
}

void applyWeaponCustomParamsJson(WeaponDefinition& def, const json& root)
{
    if (root.contains("custom_params") && root["custom_params"].is_object()) {
        for (auto it = root["custom_params"].begin(); it != root["custom_params"].end(); ++it) {
            if (it.value().is_number())
                def.customParams[it.key()] = it.value().get<float>();
        }
    }
}

void applyWeaponRenderJson(WeaponDefinition& def, const json& root)
{
    if (root.contains("render") && root["render"].is_object())
        weaponJsonVec3(root["render"], "color", def.tint);
    else if (root.contains("color") && root["color"].is_array())
        weaponJsonVec3(root, "color", def.tint);
}

void applyWeaponJson(WeaponDefinition& def, const json& root)
{
    if (!root.is_object())
        return;
    applyWeaponIdentityJson(def, root);
    applyWeaponStatsJson(def, root);
    weaponJsonBehaviorType(root, def);
    applyWeaponExecutionType(def);
    applyWeaponSoundJson(def, root);
    applyWeaponCustomParamsJson(def, root);
    applyWeaponKnockbackJson(def, root);
    applyWeaponRenderJson(def, root);

}

} // namespace

void loadWeaponJsonConfig()
{
    gWeaponConfigRoot = json::object();
    std::ifstream file(weaponConfigPath());
    if (!file.is_open())
    {
        Debug::warn(Debug::Category::Weapons,
            "[WEAPON CONFIG] could not open %s; using builtin defaults\n",
            weaponConfigPath().c_str());
        return;
    }
    Debug::log(Debug::Category::Weapons,
        "[WEAPON CONFIG] loaded %s\n", weaponConfigPath().c_str());
    try {
        file >> gWeaponConfigRoot;
        if (!gWeaponConfigRoot.is_object())
            gWeaponConfigRoot = json::object();
        std::error_code ec;
        if (std::filesystem::exists(weaponConfigPath(), ec) && !ec) {
            gWeaponConfigLastWrite = std::filesystem::last_write_time(weaponConfigPath(), ec);
            gWeaponConfigHasWriteTime = !ec;
        }
    } catch (const std::exception& e) {
        Debug::log(Debug::Category::Weapons, "[WEAPON] config parse failed: %s", e.what());
        gWeaponConfigRoot = json::object();
    }
}

// Hot tool definition override. The hot C++ definition is authoritative; this
// applies the fields it marks present and leaves everything else JSON/builtin.
// A missing provider or unknown tool is a safe no-op.
void applyHotToolDefinition(WeaponDefinition& def)
{
    void* raw = MimitaRuntime::GenericRuntime::instance().capability(
        GAME_CAP_TOOL_DEFINITION);
    if (!raw)
        return;
    auto fn = reinterpret_cast<GameToolDefinitionQueryFn>(raw);
    GameToolDefinitionV1 q{};
    q.structSize = sizeof(GameToolDefinitionV1);
    q.toolKey = gameHash(def.id.c_str());
    if (!fn(nullptr, &q) || q.found == 0)
        return;

    const std::uint32_t m = q.presentMask;
    if (m & GAME_TOOL_FIELD_DAMAGE) {
        def.damage = q.damage;
        def.headshotMultiplier = q.headshotMultiplier;
    }
    if (m & GAME_TOOL_FIELD_BEHAVIOR) {
        def.behaviorType = static_cast<WeaponBehaviorType>(q.behaviorType);
        def.fireMode = static_cast<WeaponFireMode>(q.fireMode);
        def.networkMode = static_cast<WeaponNetworkMode>(q.networkMode);
        def.hitscan = q.hitscan != 0;
    }
    if (m & GAME_TOOL_FIELD_TIMING) {
        def.fireDelay = q.fireDelay;
        def.reloadTime = q.reloadTime;
        if (q.equipPoseTime > 0.0f)
            def.customParams["equipPoseTime"] = q.equipPoseTime;
        if (q.unequipPoseTime > 0.0f)
            def.customParams["unequipPoseTime"] = q.unequipPoseTime;
    }
    if (m & GAME_TOOL_FIELD_AMMO) {
        def.magazineSize = q.magazineSize;
        def.pelletCount = q.pelletCount;
        def.spread = q.spread;
        def.recoil = q.recoil;
        if (q.reserveAmmo >= 0)
            def.customParams["reserveAmmo"] = static_cast<float>(q.reserveAmmo);
    }
    if (m & GAME_TOOL_FIELD_PROJECTILE) {
        def.projectileSpeed = q.projectileSpeed;
        def.projectileRadius = q.projectileRadius;
        def.projectileLifetime = q.projectileLifetime;
    }
    if (m & GAME_TOOL_FIELD_MODEL) {
        if (q.modelPath[0])
            def.modelPath = q.modelPath;
        if (q.scale > 0.0f)
            def.weaponScale = q.scale;
        def.attachmentOffset = glm::vec3(q.attachmentPosition[0],
                                         q.attachmentPosition[1],
                                         q.attachmentPosition[2]);
        def.attachmentRotation = glm::vec3(q.attachmentRotation[0],
                                           q.attachmentRotation[1],
                                           q.attachmentRotation[2]);
    }
    if (m & GAME_TOOL_FIELD_SOUNDS) {
        if (q.soundShoot[0])
            def.soundShoot = q.soundShoot;
        if (q.soundReload[0])
            def.soundReload = q.soundReload;
        if (q.soundEquip[0])
            def.soundEquip = q.soundEquip;
    }
    if (q.soundHit[0])
        def.soundHit = q.soundHit;
    if (q.soundDryFire[0])
        def.soundDryFire = q.soundDryFire;
    if (q.displayName[0])
        def.displayName = q.displayName;
    if (m & GAME_TOOL_FIELD_SLOT)
        def.slot = static_cast<int>(q.slot);
    const std::uint32_t paramCount = q.paramCount < 8 ? q.paramCount : 8u;
    for (std::uint32_t i = 0; i < paramCount; ++i) {
        if (q.params[i].key[0])
            def.customParams[q.params[i].key] = q.params[i].value;
    }
    applyWeaponExecutionType(def);
}

// Register any hot tool definition that has no builtin/JSON counterpart. This is
// what makes adding a brand-new weapon hot: the hot package enumerates its
// recipes and the cold registry adopts unknown ids.
void registerHotTools()
{
    void* raw = MimitaRuntime::GenericRuntime::instance().capability(
        GAME_CAP_TOOL_DEFINITION);
    if (!raw)
        return;
    auto fn = reinterpret_cast<GameToolDefinitionQueryFn>(raw);
    for (std::uint32_t index = 0;; ++index) {
        GameToolDefinitionV1 q{};
        q.structSize = sizeof(GameToolDefinitionV1);
        q.toolKey = 0;
        q.enumerateIndex = index;
        if (!fn(nullptr, &q) || q.found == 0)
            break;
        if (q.id[0] == '\0')
            continue;
        if (WeaponRegistry::instance().has(q.id))
            continue;

        WeaponDefinition def;
        def.id = q.id;
        def.displayName = q.displayName[0] ? q.displayName : q.id;
        applyHotToolDefinition(def);
        def.executionType = weaponExecutionTypeForBehavior(def.behaviorType);
        def.usesPhysicsProjectile =
            def.executionType == WeaponExecutionType::Projectile;
        WeaponRegistry::instance().registerWeapon(def);
        MimitaNet::registerWeaponDefNetworkId(def.id);
        Debug::log(Debug::Category::Weapons,
                   "[WEAPON] hot-registered tool id=%s behavior=%u",
                   def.id.c_str(), static_cast<unsigned>(def.behaviorType));
    }
}

void registerWeaponFromJson(WeaponDefinition def)
{
    if (gWeaponConfigRoot.contains(def.id))
        applyWeaponJson(def, gWeaponConfigRoot[def.id]);
    else
        applyWeaponExecutionType(def);
    applyHotToolDefinition(def);
    WeaponRegistry::instance().registerWeapon(def);
    // Assign a stable network ID for the generic AttackRequest pipeline
    MimitaNet::registerWeaponDefNetworkId(def.id);
}

// Last hot generation whose tool definitions were applied to the registry.
std::uint32_t gLastWeaponHotGeneration = 0xFFFFFFFFu;

bool reloadBuiltinWeaponsIfChanged()
{
    const auto now = std::chrono::steady_clock::now();
    if (gWeaponConfigLastCheck.time_since_epoch().count() != 0 &&
        now - gWeaponConfigLastCheck < std::chrono::milliseconds(250))
        return false;
    gWeaponConfigLastCheck = now;

    // A new hot generation can change gameplay + presentation live; re-apply the
    // hot tool definitions even when config/weapons.json did not change.
    const std::uint32_t hotGeneration =
        HotReloadSystem::instance().status().activeGeneration;
    const bool hotChanged = hotGeneration != gLastWeaponHotGeneration;
    gLastWeaponHotGeneration = hotGeneration;

    std::error_code ec;
    bool jsonChanged = false;
    if (std::filesystem::exists(weaponConfigPath(), ec) && !ec) {
        const auto writeTime =
            std::filesystem::last_write_time(weaponConfigPath(), ec);
        if (!ec && !(gWeaponConfigHasWriteTime && writeTime == gWeaponConfigLastWrite)) {
            gWeaponConfigLastWrite = writeTime;
            gWeaponConfigHasWriteTime = true;
            jsonChanged = true;
        }
    }

    if (!hotChanged && !jsonChanged)
        return false;

    registerBuiltinWeapons();
    Debug::log(Debug::Category::Weapons,
               "[WEAPON] applied %s (hot generation %u)",
               hotChanged ? "hot tool definitions" : "config/weapons.json",
               hotGeneration);
    return true;
}

} // namespace WeaponData
