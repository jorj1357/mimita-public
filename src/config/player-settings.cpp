#include "config/player-settings.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cstdio>

#include <nlohmann/json.hpp>

#include "camera.h"
#include "effects/hit-effects.h"

using json = nlohmann::json;

namespace {
PlayerSettings gSettings;

std::string configPath(const std::string& account)
{
    return "config/accounts/" + account + ".json";
}

template <typename T>
void readValue(const json& j, const char* name, T& value)
{
    if (j.contains(name))
        value = j[name].get<T>();
}
}

PlayerSettings& GetPlayerSettings()
{
    return gSettings;
}

bool LoadPlayerSettings(const std::string& account)
{
    std::ifstream file(configPath(account));
    if (!file.is_open())
        return false;

    try {
        json root;
        file >> root;
        const json& j = root.contains("settings") ? root["settings"] : root;
        readValue(j, "fov", gSettings.fov);
        readValue(j, "master_volume", gSettings.masterVolume);
        readValue(j, "music_volume", gSettings.musicVolume);
        readValue(j, "sfx_volume", gSettings.sfxVolume);
        readValue(j, "sensitivity", gSettings.sensitivity);
        readValue(j, "equipped_slot", gSettings.equippedSlot);
        readValue(j, "outfit_png", gSettings.outfitPath);
        readValue(j, "avatar_name", gSettings.avatarName);
        readValue(j, "collision_seam_tolerance", gSettings.collisionSeamTolerance);
        readValue(j, "collision_movement_bias", gSettings.collisionMovementBias);
        readValue(j, "collision_bounce_strength", gSettings.collisionBounceStrength);
        readValue(j, "collision_bounce_min_speed", gSettings.collisionBounceMinSpeed);
        readValue(j, "collision_bounce_max_speed", gSettings.collisionBounceMaxSpeed);
        readValue(j, "weapon_sway_strength", gSettings.weaponSwayStrength);
        readValue(j, "weapon_aim_follow_speed", gSettings.weaponAimFollowSpeed);
        readValue(j, "weapon_recoil_strength", gSettings.weaponRecoilStrength);
        readValue(j, "weapon_weight", gSettings.weaponWeight);
        readValue(j, "weapon_recoil_decay", gSettings.weaponRecoilDecay);
        readValue(j, "freecam_speed", gSettings.freecamSpeed);
        readValue(j, "music_muted", gSettings.musicMuted);
        readValue(j, "debug_combat", gSettings.debugCombat);
        readValue(j, "bloodfx", gSettings.bloodFX);
        readValue(j, "resolution", gSettings.resolution);
        readValue(j, "graphics_preset", gSettings.graphicsPreset);

        gSettings.fov = std::clamp(gSettings.fov, 60.0f, 140.0f);
        gSettings.sensitivity = std::clamp(gSettings.sensitivity, 0.01f, 2.0f);
        gSettings.equippedSlot = std::clamp(gSettings.equippedSlot, 1, 10);
        CAMERA_FOV = gSettings.fov;
        CAMERA_SENS = gSettings.sensitivity;
        gBloodFXEnabled = gSettings.bloodFX;
        return true;
    } catch (const std::exception& e) {
        printf("[PLAYER SETTINGS] load failed: %s\n", e.what());
        return false;
    }
}

bool SavePlayerSettings(const std::string& account)
{
    std::filesystem::create_directories("config/accounts");
    const std::string path = configPath(account);
    json root = json::object();

    {
        std::ifstream input(path);
        if (input.is_open()) {
            try { input >> root; } catch (...) { root = json::object(); }
        }
    }

    json& j = root["settings"];
    j["fov"] = gSettings.fov;
    j["master_volume"] = gSettings.masterVolume;
    j["music_volume"] = gSettings.musicVolume;
    j["sfx_volume"] = gSettings.sfxVolume;
    j["sensitivity"] = gSettings.sensitivity;
    j["equipped_slot"] = gSettings.equippedSlot;
    j["outfit_png"] = gSettings.outfitPath;
    j["avatar_name"] = gSettings.avatarName;
    j["collision_seam_tolerance"] = gSettings.collisionSeamTolerance;
    j["collision_movement_bias"] = gSettings.collisionMovementBias;
    j["collision_bounce_strength"] = gSettings.collisionBounceStrength;
    j["collision_bounce_min_speed"] = gSettings.collisionBounceMinSpeed;
    j["collision_bounce_max_speed"] = gSettings.collisionBounceMaxSpeed;
    j["weapon_sway_strength"] = gSettings.weaponSwayStrength;
    j["weapon_aim_follow_speed"] = gSettings.weaponAimFollowSpeed;
    j["weapon_recoil_strength"] = gSettings.weaponRecoilStrength;
    j["weapon_weight"] = gSettings.weaponWeight;
    j["weapon_recoil_decay"] = gSettings.weaponRecoilDecay;
    j["freecam_speed"] = gSettings.freecamSpeed;
    j["music_muted"] = gSettings.musicMuted;
    j["debug_combat"] = gSettings.debugCombat;
    j["bloodfx"] = gSettings.bloodFX;
    j["resolution"] = gSettings.resolution;
    j["graphics_preset"] = gSettings.graphicsPreset;

    const std::string temporary = path + ".tmp";
    std::ofstream output(temporary, std::ios::trunc);
    if (!output.is_open())
        return false;
    output << root.dump(2);
    output.close();

    std::error_code ec;
    std::filesystem::remove(path, ec);
    ec.clear();
    std::filesystem::rename(temporary, path, ec);
    if (ec) {
        printf("[PLAYER SETTINGS] save failed: %s\n", ec.message().c_str());
        return false;
    }
    return true;
}
