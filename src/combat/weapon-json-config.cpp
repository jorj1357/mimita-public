#include "weapon-json-config.h"

#include "weapon-data.h"
#include "weapon-registry.h"
#include "weapon-melee-config.h"
#include "actor-preset-weapons.h"
#include "gamemode/match-roles.h"
#include "../debug/debug-log.h"
#include "../debug/structured-log.h"
#include "../network/network-weapons.h"
#include "../utils/path_utils.h"
#include "../utils/json-file-watch.h"
#include "perf/perf-spike.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
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
std::string gWeaponConfigSignature;
std::string gLastRejectedSignature;
std::unordered_set<std::string> gBuiltinWeaponIds;
using AnimationFileState = std::pair<std::string, std::filesystem::file_time_type>;
std::vector<AnimationFileState> gAnimationLibraryFiles;
bool gAnimationLibraryStateInitialized = false;

JsonFileWatch& weaponFileWatch()
{
    static JsonFileWatch watch(
        {resolveAssetPath("config/weapons"),
         resolveAssetPath("config/animations/library")},
        std::chrono::milliseconds(250));
    return watch;
}

void pruneUnsetFields(json& value)
{
    if (!value.is_object()) return;
    for (auto it = value.begin(); it != value.end();) {
        if (it.value().is_null()) {
            it = value.erase(it);
        } else {
            pruneUnsetFields(it.value());
            ++it;
        }
    }
}

std::filesystem::path perWeaponDirectory()
{
    return std::filesystem::path(weaponConfigPath()).parent_path() / "weapons";
}

bool readAnimationLibraryState(std::vector<AnimationFileState>& state)
{
    MIMITA_PERF_SCOPE("Config::AnimationLibraryScan");
    state.clear();
    const std::filesystem::path root(resolveAssetPath("config/animations/library"));
    std::error_code ec;
    if (!std::filesystem::is_directory(root, ec))
        return !ec;
    for (std::filesystem::recursive_directory_iterator it(root, ec), end;
         it != end; it.increment(ec)) {
        if (ec) return false;
        if (!it->is_regular_file(ec) || ec) {
            if (ec) return false;
            continue;
        }
        if (it->path().extension() != ".json") continue;
        const auto writeTime = it->last_write_time(ec);
        if (ec) return false;
        state.emplace_back(it->path().generic_string(), writeTime);
    }
    if (ec) return false;
    std::sort(state.begin(), state.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });
    return true;
}

bool animationLibraryChanged()
{
    std::vector<AnimationFileState> current;
    if (!readAnimationLibraryState(current)) return false;
    if (!gAnimationLibraryStateInitialized) {
        gAnimationLibraryFiles = std::move(current);
        gAnimationLibraryStateInitialized = true;
        return false;
    }
    if (current == gAnimationLibraryFiles) return false;
    gAnimationLibraryFiles = std::move(current);
    return true;
}

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
    if (type == "none" || type == "nothing" || type == "empty")
        def.behaviorType = WeaponBehaviorType::None;
    else if (type == "projectile")
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
    weaponJsonFloat(root, "damage_scale", def.damageScale);
    weaponJsonFloat(root, "headshot_multiplier", def.headshotMultiplier);
    weaponJsonFloat(root, "fire_delay", def.fireDelay);
    weaponJsonFloat(root, "reload_time", def.reloadTime);
    weaponJsonInt(root, "magazine_size", def.magazineSize);
    weaponJsonInt(root, "reserve_ammo", def.reserveSize);
    weaponJsonInt(root, "pellet_count", def.pelletCount);
    weaponJsonFloat(root, "spread", def.spread);
    weaponJsonBool(root, "square_spread", def.squareSpread);
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
    weaponJsonString(root, "animation_profile", def.animationProfile);
    if (def.animationProfile.empty() && root.contains("animation") &&
        root["animation"].is_object() && root["animation"].contains("profile") &&
        root["animation"]["profile"].is_string()) {
        def.animationProfile = root["animation"]["profile"].get<std::string>();
    }
}

void applyWeaponRecoilJson(WeaponDefinition& def, const json& root)
{
    const char* keyName = root.contains("recoil_definition")
        ? "recoil_definition" : "recoil";
    if (!root.contains(keyName) || !root[keyName].is_object())
        return;
    const json& r = root[keyName];
    weaponJsonBool(r, "enabled", def.recoilDefinition.enabled);
    weaponJsonFloat(r, "pitch_per_shot", def.recoilDefinition.pitchPerShot);
    weaponJsonFloat(r, "yaw_per_shot", def.recoilDefinition.yawPerShot);
    weaponJsonFloat(r, "roll_per_shot", def.recoilDefinition.rollPerShot);
    weaponJsonFloat(r, "accumulation_per_shot", def.recoilDefinition.accumulationPerShot);
    weaponJsonFloat(r, "max_accumulation", def.recoilDefinition.maxAccumulation);
    weaponJsonFloat(r, "recovery_delay", def.recoilDefinition.recoveryDelay);
    weaponJsonFloat(r, "recovery_rate", def.recoilDefinition.recoveryRate);
    weaponJsonFloat(r, "camera_scale", def.recoilDefinition.cameraScale);
    weaponJsonFloat(r, "camera_ads_scale", def.recoilDefinition.cameraAdsScale);
    weaponJsonFloat(r, "camera_recovery_rate", def.recoilDefinition.cameraRecoveryRate);
    weaponJsonFloat(r, "viewmodel_back_per_shot", def.recoilDefinition.viewmodelBackPerShot);
    weaponJsonFloat(r, "viewmodel_pitch_per_shot", def.recoilDefinition.viewmodelPitchPerShot);
    weaponJsonFloat(r, "viewmodel_yaw_per_shot", def.recoilDefinition.viewmodelYawPerShot);
    weaponJsonFloat(r, "viewmodel_roll_per_shot", def.recoilDefinition.viewmodelRollPerShot);
    weaponJsonFloat(r, "viewmodel_recovery_rate", def.recoilDefinition.viewmodelRecoveryRate);
    weaponJsonFloat(r, "viewmodel_ads_scale", def.recoilDefinition.viewmodelAdsScale);
    weaponJsonFloat(r, "arm_pitch_per_shot", def.recoilDefinition.armPitchPerShot);
    weaponJsonFloat(r, "arm_yaw_per_shot", def.recoilDefinition.armYawPerShot);
    weaponJsonFloat(r, "arm_roll_per_shot", def.recoilDefinition.armRollPerShot);
    weaponJsonFloat(r, "arm_recovery_rate", def.recoilDefinition.armRecoveryRate);
    weaponJsonFloat(r, "arm_ads_scale", def.recoilDefinition.armAdsScale);

    auto& rdef = def.recoilDefinition;
    rdef.accumulationPerShot = std::max(0.0f, rdef.accumulationPerShot);
    rdef.maxAccumulation = std::max(0.0f, rdef.maxAccumulation);
    rdef.recoveryDelay = std::max(0.0f, rdef.recoveryDelay);
    rdef.recoveryRate = std::max(0.0f, rdef.recoveryRate);
    rdef.cameraRecoveryRate = std::max(0.0f, rdef.cameraRecoveryRate);
    rdef.viewmodelRecoveryRate = std::max(0.0f, rdef.viewmodelRecoveryRate);
    rdef.armRecoveryRate = std::max(0.0f, rdef.armRecoveryRate);
}

void applyWeaponProjectileImpactJson(WeaponDefinition& def, const json& root)
{
    // Top-level keys win; custom_params act as a fallback, matching knockback.
    const auto readFloatKey = [&root](const char* key, float& out) {
        if (root.contains(key) && root[key].is_number())
            out = root[key].get<float>();
        else if (root.contains("custom_params") && root["custom_params"].is_object() &&
                 root["custom_params"].contains(key) &&
                 root["custom_params"][key].is_number())
            out = root["custom_params"][key].get<float>();
    };
    readFloatKey("projectile_mass", def.projectileMass);
    readFloatKey("projectile_density", def.projectileDensity);
    readFloatKey("projectile_base_radius", def.projectileBaseRadius);
    readFloatKey("cut_energy_scale", def.cutEnergyScale);
    readFloatKey("cut_radius_scale", def.cutRadiusScale);
    readFloatKey("penetration_scale", def.penetrationScale);
    float penetration = (float)def.penetrationCount;
    readFloatKey("penetration_count", penetration);
    def.penetrationCount = std::max(1, (int)std::lround(penetration));
    float shape = (float)def.projectileShapeId;
    readFloatKey("projectile_shape", shape);
    def.projectileShapeId = (uint32_t)std::max(0.0f, shape);
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
        def.soundVolume = sound.value("volume", def.soundVolume);
        def.soundPitch = sound.value("pitch", def.soundPitch);
        def.soundStartSeconds = sound.value("start_seconds", def.soundStartSeconds);
        def.soundEndSeconds = sound.value("end_seconds", def.soundEndSeconds);
        def.soundRetrigger = sound.value("retrigger", def.soundRetrigger);
    }
    def.soundPitch = root.value("fireSoundPitch", def.soundPitch);
    def.soundStartSeconds = root.value("fireSoundStartSeconds", def.soundStartSeconds);
    def.soundEndSeconds = root.value("fireSoundEndSeconds", def.soundEndSeconds);
    def.soundRetrigger = root.value("fireSoundRetrigger", def.soundRetrigger);
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

// Parses the optional data-driven Melee move-set component and publishes it in
// the process-wide registry keyed by the definition id. A definition without
// this component is unaffected.
void applyWeaponMeleeJson(WeaponDefinition& def, const json& root)
{
    MeleeMoveSet moveSet;
    if (parseMeleeMoveSet(def.id, root, moveSet))
        MeleeMoveSetRegistry::instance().set(std::move(moveSet));
}

void applyWeaponJson(WeaponDefinition& def, const json& root)
{
    if (!root.is_object())
        return;
    applyWeaponIdentityJson(def, root);
    applyWeaponStatsJson(def, root);
    applyWeaponRecoilJson(def, root);
    weaponJsonBehaviorType(root, def);
    applyWeaponExecutionType(def);
    applyWeaponSoundJson(def, root);
    applyWeaponCustomParamsJson(def, root);
    applyWeaponKnockbackJson(def, root);
    applyWeaponProjectileImpactJson(def, root);
    applyWeaponRenderJson(def, root);
    applyWeaponMeleeJson(def, root);

}

} // namespace

std::string weaponDefinitionSignature()
{
    MIMITA_PERF_SCOPE("Config::WeaponDefinitionSignature");
    std::vector<std::filesystem::path> files{
        weaponConfigPath(), perWeaponDirectory() / "masterlist" / "weapon-masterlist.json"};
    const auto dir = perWeaponDirectory();
    std::error_code ec;
    if (std::filesystem::is_directory(dir, ec)) {
        for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
            if (ec) break;
            if (entry.is_directory()) {
                const auto file = entry.path() / "config.json";
                if (std::filesystem::is_regular_file(file)) files.push_back(file);
            }
        }
    }
    std::sort(files.begin(), files.end());
    std::string result;
    for (const auto& file : files) {
        const auto stamp = std::filesystem::last_write_time(file, ec);
        result += file.generic_string() + ":";
        result += ec ? "missing" : std::to_string(stamp.time_since_epoch().count());
        result += ";";
        ec.clear();
    }
    return result;
}

nlohmann::json readWeaponDefinitionRoot()
{
    json root = json::object();
    std::ifstream aggregate(weaponConfigPath());
    if (aggregate.is_open()) {
        root = json::parse(aggregate, nullptr, true, true);
        if (!root.is_object())
            throw std::runtime_error("aggregate weapon config must be an object");
    }

    const auto dir = perWeaponDirectory();
    const auto masterPath = dir / "masterlist" / "weapon-masterlist.json";
    json fields;
    json rules;
    if (std::ifstream master(masterPath); master.is_open()) {
        const json spec = json::parse(master, nullptr, true, true);
        fields = spec.at("fields");
        rules = spec.at("rules");
        if (!fields.is_array())
            throw std::runtime_error("weapon masterlist fields must be an array");
    } else {
        throw std::runtime_error("weapon masterlist missing: " + masterPath.string());
    }

    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) return root;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) throw std::runtime_error("cannot enumerate per-weapon configs");
        if (!entry.is_directory()) continue;
        const auto file = entry.path() / "config.json";
        if (!std::filesystem::is_regular_file(file)) continue;
        std::ifstream stream(file);
        json definition = json::parse(stream, nullptr, true, true);
        if (!definition.is_object())
            throw std::runtime_error(file.string() + " must contain an object");
        const std::string id = entry.path().filename().string();
        if (!definition.contains("id") || !definition["id"].is_string() ||
            definition["id"].get<std::string>() != id)
            throw std::runtime_error(file.string() + " id must match its directory");
        if (fields.is_array()) {
            for (const auto& field : fields) {
                const std::string name = field.get<std::string>();
                if (!definition.contains(name))
                    throw std::runtime_error(file.string() + " missing masterlist field: " + name);
                const json& value = definition[name];
                const json& rule = rules.at(name);
                if (value.is_null()) {
                    if (!rule.value("nullable", false))
                        throw std::runtime_error(file.string() + " field cannot be null: " + name);
                } else {
                    const std::string type = value.is_boolean() ? "boolean" :
                        value.is_number() ? "number" : value.is_string() ? "string" :
                        value.is_array() ? "array" : value.is_object() ? "object" : "invalid";
                    if (std::find(rule["types"].begin(), rule["types"].end(), type) ==
                        rule["types"].end())
                        throw std::runtime_error(file.string() + " wrong type for field: " + name);
                }
            }
            for (auto it = definition.begin(); it != definition.end(); ++it) {
                if (std::find(fields.begin(), fields.end(), it.key()) == fields.end())
                    throw std::runtime_error(file.string() + " unknown field: " + it.key());
            }
        }
        pruneUnsetFields(definition);
        root[id] = std::move(definition); // per-weapon file is authoritative
    }
    return root;
}

void loadWeaponJsonConfig()
{
    try {
        json candidate = readWeaponDefinitionRoot();
        gWeaponConfigRoot = std::move(candidate);
        gWeaponConfigSignature = weaponDefinitionSignature();
        gLastRejectedSignature.clear();
        Debug::log(Debug::Category::Weapons,
            "[WEAPON CONFIG] loaded %zu definitions from aggregate and per-weapon files\n",
            gWeaponConfigRoot.size());
        std::vector<AnimationFileState> animationFiles;
        if (readAnimationLibraryState(animationFiles)) {
            gAnimationLibraryFiles = std::move(animationFiles);
            gAnimationLibraryStateInitialized = true;
        }
    } catch (const std::exception& e) {
        const std::string rejected = weaponDefinitionSignature();
        if (rejected != gLastRejectedSignature) {
            gLastRejectedSignature = rejected;
            Debug::warn(Debug::Category::Weapons, "[WEAPON] config rejected; retaining last valid definitions: %s", e.what());
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Weapons, StructuredLevel::Errors,
                "config.weapon.invalid", "weapon_definitions",
                "weapon definition snapshot rejected", 0,
                {{"reason", e.what()}, {"retained_previous", !gWeaponConfigRoot.empty()}},
                __FILE__, __LINE__, __FUNCTION__);
        }
    }
}

void registerWeaponFromJson(WeaponDefinition def, bool builtin)
{
    if (gWeaponConfigRoot.contains(def.id))
        applyWeaponJson(def, gWeaponConfigRoot[def.id]);
    else
        applyWeaponExecutionType(def);
    if (builtin)
        gBuiltinWeaponIds.insert(def.id);
    WeaponRegistry::instance().registerWeapon(def);
    // Assign a stable network ID for the generic AttackRequest pipeline
    MimitaNet::registerWeaponDefNetworkId(def.id);
}

// Registers any config/weapons.json entry that no builtin constructor created.
// This is the data-only path required by the weapon and Melee specifications:
// adding a definition (for example captain_falcon_moveset) needs only JSON.
// Builtin ids are skipped so their C++ constructor stays the base definition.
void registerWeaponsFromJsonRoot()
{
    for (auto it = gWeaponConfigRoot.begin(); it != gWeaponConfigRoot.end(); ++it) {
        if (!it.value().is_object())
            continue;
        std::string id = it.key();
        if (it.value().contains("id") && it.value()["id"].is_string())
            id = it.value()["id"].get<std::string>();
        if (id.empty())
            continue;

        WeaponDefinition def;
        def.id = id;
        def.displayName = id;
        def.behaviorType = WeaponBehaviorType::Melee;
        def.executionType = WeaponExecutionType::PhysicalContact;
        def.hitscan = false;
        const bool isBuiltin = gBuiltinWeaponIds.find(id) != gBuiltinWeaponIds.end();
        if (!WeaponRegistry::instance().has(id) || !isBuiltin)
            registerWeaponFromJson(std::move(def), false);

        // Builtins keep their constructor-owned base definition, but both
        // builtins and JSON-only definitions publish the current Melee data.
        WeaponData::MeleeMoveSet moveSet;
        if (WeaponData::parseMeleeMoveSet(id, it.value(), moveSet))
            WeaponData::MeleeMoveSetRegistry::instance().set(std::move(moveSet));
    }
}

bool reloadBuiltinWeaponsIfChanged()
{
    // Config and clip trees are watched off-thread. Only a dirty signal pays
    // for the full signature and library comparison on the gameplay thread.
    if (!weaponFileWatch().consumeDirty())
        return false;

    const bool weaponConfigChanged = weaponDefinitionSignature() != gWeaponConfigSignature;
    const bool animationLibraryWasChanged = animationLibraryChanged();
    if (!weaponConfigChanged && !animationLibraryWasChanged)
        return false;

    registerBuiltinWeapons();
    if (!ActorPresetWeapons::activePresetId().empty()) {
        if (const MatchRoleDefinition* preset =
                MatchRoleRegistry::instance().getActorPreset(
                    ActorPresetWeapons::activePresetId())) {
            ActorPresetWeapons::apply(*preset);
        }
    }
    Debug::log(Debug::Category::Weapons,
        "[WEAPON] hot reloaded %s%s", weaponConfigPath().c_str(),
        animationLibraryWasChanged ? " after animation library edit" : "");
    return true;
}

} // namespace WeaponData
