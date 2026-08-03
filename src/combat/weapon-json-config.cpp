#include "weapon-json-config.h"

#include "weapon-data.h"
#include "weapon-registry.h"
#include "../debug/debug-log.h"
#include "../network/network-weapons.h"

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
    else if (type == "rocketlauncher" || type == "rocket")
        def.behaviorType = WeaponBehaviorType::RocketLauncher;
    else if (type == "grenadelauncher" || type == "grenade")
        def.behaviorType = WeaponBehaviorType::GrenadeLauncher;
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
    weaponJsonBehaviorType(root, def);
    weaponJsonBool(root, "hitscan", def.hitscan);
    weaponJsonFloat(root, "beam_thickness", def.beamThickness);
    weaponJsonFloat(root, "beam_world_thickness", def.beamWorldThickness);
    weaponJsonBool(root, "uses_physics_projectile", def.usesPhysicsProjectile);
    weaponJsonString(root, "pose_id", def.poseId);
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
    applyWeaponExecutionType(def);
    applyWeaponSoundJson(def, root);
    applyWeaponCustomParamsJson(def, root);
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

void registerWeaponFromJson(WeaponDefinition def)
{
    if (gWeaponConfigRoot.contains(def.id))
        applyWeaponJson(def, gWeaponConfigRoot[def.id]);
    else
        applyWeaponExecutionType(def);
    WeaponRegistry::instance().registerWeapon(def);
    // Assign a stable network ID for the generic AttackRequest pipeline
    MimitaNet::registerWeaponDefNetworkId(def.id);
}

bool reloadBuiltinWeaponsIfChanged()
{
    const auto now = std::chrono::steady_clock::now();
    if (gWeaponConfigLastCheck.time_since_epoch().count() != 0 &&
        now - gWeaponConfigLastCheck < std::chrono::milliseconds(250))
        return false;
    gWeaponConfigLastCheck = now;

    std::error_code ec;
    if (!std::filesystem::exists(weaponConfigPath(), ec) || ec)
        return false;
    const auto writeTime = std::filesystem::last_write_time(weaponConfigPath(), ec);
    if (ec || (gWeaponConfigHasWriteTime && writeTime == gWeaponConfigLastWrite))
        return false;

    gWeaponConfigLastWrite = writeTime;
    gWeaponConfigHasWriteTime = true;
    registerBuiltinWeapons();
    Debug::log(Debug::Category::Weapons, "[WEAPON] hot reloaded %s", weaponConfigPath().c_str());
    return true;
}

} // namespace WeaponData
