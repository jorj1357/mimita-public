#include "player-animation-config.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace {

const char* ANIMATIONS_CONFIG_PATH = "config/animations.json";
std::filesystem::file_time_type gAnimationsLastWrite{};

bool readAnimPart(const nlohmann::json& j, AnimKeyframePart& part)
{
    bool read = false;
    if (j.contains("translation") && j["translation"].is_array() && j["translation"].size() >= 3) {
        part.translation = glm::vec3(j["translation"][0], j["translation"][1], j["translation"][2]);
        read = true;
    }
    if (j.contains("rotation") && j["rotation"].is_array() && j["rotation"].size() >= 3) {
        part.rotation = glm::vec3(j["rotation"][0], j["rotation"][1], j["rotation"][2]);
        read = true;
    }
    return read;
}

AnimClip parseClip(const nlohmann::json& j)
{
    AnimClip clip;
    clip.durationTicks = j.value("durationTicks", 60);
    clip.loop = j.value("loop", true);
    clip.speedScaleFromVelocity = j.value("speedScaleFromVelocity", true);
    clip.speedBased = j.value("speedBased", true);
    clip.inputTriggered = j.value("inputTriggered", false);
    clip.tickBasedReturnToIdle = j.value("tickBasedReturnToIdle", false);
    if (!j.contains("keyframes") || !j["keyframes"].is_array())
        return clip;

    for (const auto& kf : j["keyframes"]) {
        AnimKeyframe keyframe;
        keyframe.tick = kf.value("tick", 0);
        if (kf.contains("parts") && kf["parts"].is_object()) {
            for (auto p = kf["parts"].begin(); p != kf["parts"].end(); ++p) {
                AnimKeyframePart part;
                readAnimPart(p.value(), part);
                keyframe.parts[p.key()] = part;
            }
        }
        clip.keyframes.push_back(keyframe);
    }
    return clip;
}

WeaponPoseConfig parseWeaponPose(const nlohmann::json& j)
{
    WeaponPoseConfig poseCfg;
    poseCfg.useWeaponPose = j.value("useWeaponPose", j.value("use_weapon_pose", true));
    if (j.contains("leftArm"))
        readAnimPart(j["leftArm"], poseCfg.leftArm);
    if (j.contains("rightArm"))
        readAnimPart(j["rightArm"], poseCfg.rightArm);
    return poseCfg;
}

void parseLayers(const nlohmann::json& root, PlayerProceduralConfig& loaded)
{
    if (!root.contains("layers") || !root["layers"].is_object())
        return;

    const auto& layers = root["layers"];
    if (layers.contains("animations") && layers["animations"].is_object()) {
        loaded.layers.animations.clear();
        for (auto it = layers["animations"].begin(); it != layers["animations"].end(); ++it)
            loaded.layers.animations[it.key()] = parseClip(it.value());
    }
    if (layers.contains("reloadOverlay") && layers["reloadOverlay"].is_object())
        readAnimPart(layers["reloadOverlay"], loaded.layers.reloadOverlay);
}

void parseSway(const nlohmann::json& root, PlayerProceduralConfig& loaded)
{
    if (!root.contains("sway") || !root["sway"].is_object())
        return;
    const auto& sway = root["sway"];
    if (sway.contains("weapon_amount") && sway["weapon_amount"].is_number())
        loaded.weaponSwayAmount = sway["weapon_amount"].get<float>();
    if (sway.contains("weapon_speed") && sway["weapon_speed"].is_number())
        loaded.weaponSwaySpeed = sway["weapon_speed"].get<float>();
    if (sway.contains("idle_amount") && sway["idle_amount"].is_number())
        loaded.idleSwayAmount = sway["idle_amount"].get<float>();
    if (sway.contains("idle_speed") && sway["idle_speed"].is_number())
        loaded.idleSwaySpeed = sway["idle_speed"].get<float>();
}

void parseWeaponPoses(const nlohmann::json& root, PlayerProceduralConfig& loaded)
{
    if (!root.contains("weapons") || !root["weapons"].is_object())
        return;

    loaded.weaponPoses.clear();
    for (auto it = root["weapons"].begin(); it != root["weapons"].end(); ++it) {
        if (!it.value().is_object())
            continue;
        const std::string weaponId = it.key();
        const auto& weapon = it.value();
        const std::string activePose = weapon.value("active_pose", "idle");
        if (!weapon.contains("poses") || !weapon["poses"].is_object()) {
            loaded.weaponPoses[weaponId] = parseWeaponPose(weapon);
            continue;
        }
        for (auto poseIt = weapon["poses"].begin(); poseIt != weapon["poses"].end(); ++poseIt) {
            if (!poseIt.value().is_object())
                continue;
            const WeaponPoseConfig poseCfg = parseWeaponPose(poseIt.value());
            loaded.weaponPoses[weaponId + ":" + poseIt.key()] = poseCfg;
            if (poseIt.key() == activePose || poseIt.key() == "idle" || poseIt.key() == "equipped")
                loaded.weaponPoses[weaponId] = poseCfg;
        }
    }
}

} // namespace

bool animationConfigChanged()
{
    std::error_code ec;
    if (!std::filesystem::exists(ANIMATIONS_CONFIG_PATH, ec) || ec)
        return false;
    const auto writeTime = std::filesystem::last_write_time(ANIMATIONS_CONFIG_PATH, ec);
    if (ec || writeTime == gAnimationsLastWrite)
        return false;
    gAnimationsLastWrite = writeTime;
    return true;
}

void loadAnimationConfig(PlayerProceduralConfig& loaded)
{
    std::ifstream file(ANIMATIONS_CONFIG_PATH);
    if (!file.is_open())
        return;

    try {
        nlohmann::json root;
        file >> root;
        parseSway(root, loaded);
        parseLayers(root, loaded);
        parseWeaponPoses(root, loaded);
    } catch (const std::exception& e) {
        printf("[HOT RELOAD] animation config reload failed: %s\n", e.what());
    }
}
