#include "player.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

#include "debug/debug-log.h"

namespace {
const char* PLAYER_PROCEDURAL_CONFIG_PATH = "config/player-procedural.json";
float gPlayerProceduralReloadTimer = 0.25f;
std::filesystem::file_time_type gPlayerProceduralLastWrite{};
std::chrono::steady_clock::time_point gPlayerProceduralLastCheck{};
} // anonymous namespace

PlayerProceduralConfig gPlayerProcedural{
    -45.0f,
    -20.0f,
    15.0f,
    -55.0f,
    -25.0f,
    -10.0f,
    0.15f,
    8.0f,
    0.6f,
    0.3f,
    0.25f,
    0.45f,
    75.0f,
    65.0f,
    -20.0f,
    12.0f,
    0.12f,
    6.5f,
    6.0f,
    -15.0f,
    -8.0f,
    -5.0f,
    0.06f,
    0.05f,
    3.0f,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    0.5f,
    0.0f,
    0.0f,
    {}
};

template<typename T>
void readJsonValue(
    const nlohmann::json& j,
    const char* key,
    T& value
)
{
    if (j.contains(key))
        value = j[key].get<T>();
}

bool reloadPlayerProceduralConfig()
{
    std::ifstream file(PLAYER_PROCEDURAL_CONFIG_PATH);
    if (!file.is_open())
        return false;

    try
    {
        nlohmann::json j;
        file >> j;

        PlayerProceduralConfig loaded = gPlayerProcedural;
        readJsonValue(j, "leftArmRaise", loaded.leftArmRaise);
        readJsonValue(j, "leftArmForward", loaded.leftArmForward);
        readJsonValue(j, "leftArmTwist", loaded.leftArmTwist);
        readJsonValue(j, "rightArmRaise", loaded.rightArmRaise);
        readJsonValue(j, "rightArmForward", loaded.rightArmForward);
        readJsonValue(j, "rightArmTwist", loaded.rightArmTwist);
        readJsonValue(j, "weaponSwayAmount", loaded.weaponSwayAmount);
        readJsonValue(j, "weaponSwaySpeed", loaded.weaponSwaySpeed);
        readJsonValue(j, "torsoAimYawStrength", loaded.torsoAimYawStrength);
        readJsonValue(j, "torsoAimPitchStrength", loaded.torsoAimPitchStrength);
        readJsonValue(j, "armAimYawStrength", loaded.armAimYawStrength);
        readJsonValue(j, "armAimPitchStrength", loaded.armAimPitchStrength);

        readJsonValue(j, "armSwingAmount", loaded.armSwingAmount);
        readJsonValue(j, "legSwingAmount", loaded.legSwingAmount);
        readJsonValue(j, "torsoLeanAmount", loaded.torsoLeanAmount);
        readJsonValue(j, "headCounterAmount", loaded.headCounterAmount);
        readJsonValue(j, "bobHeight", loaded.bobHeight);
        readJsonValue(j, "walkFrequency", loaded.walkFrequency);
        readJsonValue(j, "walkFrequencyMultiplier", loaded.walkFrequencyMultiplier);
        readJsonValue(j, "reloadArmLowerZ", loaded.reloadArmLowerZ);
        readJsonValue(j, "reloadArmLowerX", loaded.reloadArmLowerX);
        readJsonValue(j, "reloadHandLower", loaded.reloadHandLower);
        readJsonValue(j, "armInfluenceMultiplier", loaded.armInfluenceMultiplier);
        readJsonValue(j, "idleSwayAmount", loaded.idleSwayAmount);
        readJsonValue(j, "idleSwaySpeed", loaded.idleSwaySpeed);
        readJsonValue(j, "revolverOffsetX", loaded.revolverOffsetX);
        readJsonValue(j, "revolverOffsetY", loaded.revolverOffsetY);
        readJsonValue(j, "revolverOffsetZ", loaded.revolverOffsetZ);
        readJsonValue(j, "revolverRotX", loaded.revolverRotX);
        readJsonValue(j, "revolverRotY", loaded.revolverRotY);
        readJsonValue(j, "revolverRotZ", loaded.revolverRotZ);
        readJsonValue(j, "shotgunOffsetX", loaded.shotgunOffsetX);
        readJsonValue(j, "shotgunOffsetY", loaded.shotgunOffsetY);
        readJsonValue(j, "shotgunOffsetZ", loaded.shotgunOffsetZ);
        readJsonValue(j, "shotgunRotX", loaded.shotgunRotX);
        readJsonValue(j, "shotgunRotY", loaded.shotgunRotY);
        readJsonValue(j, "shotgunRotZ", loaded.shotgunRotZ);
        readJsonValue(j, "walkStartTickOnEnter", loaded.walkStartTickOnEnter);
        readJsonValue(j, "animationStateTransitionFrames", loaded.animationStateTransitionFrames);

        // Parse axis locks
        if (j.contains("axisLocks")) {
            const auto& locks = j["axisLocks"];
            for (auto it = locks.begin(); it != locks.end(); ++it) {
                AxisLock lock;
                if (it->contains("rotation") && it->at("rotation").size() >= 3) {
                    lock.x = it->at("rotation")[0].get<bool>();
                    lock.y = it->at("rotation")[1].get<bool>();
                    lock.z = it->at("rotation")[2].get<bool>();
                }
                loaded.axisLocks[it.key()] = lock;
            }
        }

        // Parse per-weapon pose configs (root-level entries with weapon IDs)
        {
            for (auto it = j.begin(); it != j.end(); ++it) {
                const std::string& key = it.key();
                // Skip known non-weapon keys
                if (key == "layers" || key == "axisLocks" || key == "walkStartTickOnEnter" ||
                    key == "animationStateTransitionFrames" || key == "leftArmRaise" ||
                    key == "leftArmForward" || key == "leftArmTwist" ||
                    key == "rightArmRaise" || key == "rightArmForward" || key == "rightArmTwist" ||
                    key == "weaponSwayAmount" || key == "weaponSwaySpeed" ||
                    key == "torsoAimYawStrength" || key == "torsoAimPitchStrength" ||
                    key == "armAimYawStrength" || key == "armAimPitchStrength" ||
                    key == "armSwingAmount" || key == "legSwingAmount" ||
                    key == "torsoLeanAmount" || key == "headCounterAmount" ||
                    key == "bobHeight" || key == "walkFrequency" || key == "walkFrequencyMultiplier" ||
                    key == "reloadArmLowerZ" || key == "reloadArmLowerX" || key == "reloadHandLower" ||
                    key == "armInfluenceMultiplier" || key == "idleSwayAmount" || key == "idleSwaySpeed" ||
                    key == "revolverOffsetX" || key == "revolverOffsetY" || key == "revolverOffsetZ" ||
                    key == "revolverRotX" || key == "revolverRotY" || key == "revolverRotZ" ||
                    key == "shotgunOffsetX" || key == "shotgunOffsetY" || key == "shotgunOffsetZ" ||
                    key == "shotgunRotX" || key == "shotgunRotY" || key == "shotgunRotZ")
                    continue;

                WeaponPoseConfig poseCfg;
                poseCfg.useWeaponPose = it->value("useWeaponPose", true);
                if (it->contains("leftArm")) {
                    const auto& la = it->at("leftArm");
                    if (la.contains("translation") && la["translation"].size() >= 3)
                        poseCfg.leftArm.translation = glm::vec3(la["translation"][0], la["translation"][1], la["translation"][2]);
                    if (la.contains("rotation") && la["rotation"].size() >= 3)
                        poseCfg.leftArm.rotation = glm::vec3(la["rotation"][0], la["rotation"][1], la["rotation"][2]);
                }
                if (it->contains("rightArm")) {
                    const auto& ra = it->at("rightArm");
                    if (ra.contains("translation") && ra["translation"].size() >= 3)
                        poseCfg.rightArm.translation = glm::vec3(ra["translation"][0], ra["translation"][1], ra["translation"][2]);
                    if (ra.contains("rotation") && ra["rotation"].size() >= 3)
                        poseCfg.rightArm.rotation = glm::vec3(ra["rotation"][0], ra["rotation"][1], ra["rotation"][2]);
                }
                loaded.weaponPoses[key] = poseCfg;
                printf("[WEAPON POSE] weapon=%s usePose=%s\n",
                       key.c_str(), poseCfg.useWeaponPose ? "true" : "false");
            }
        }

        // Parse layered animation config (under "layers" key)
        if (j.contains("layers")) {
            const auto& layers = j["layers"];
            if (layers.contains("animations")) {
                const auto& anims = layers["animations"];
                for (auto it = anims.begin(); it != anims.end(); ++it) {
                    AnimClip clip;
                    clip.durationTicks = it->value("durationTicks", 60);
                    clip.loop = it->value("loop", true);
                    clip.speedScaleFromVelocity = it->value("speedScaleFromVelocity", true);
                    clip.speedBased = it->value("speedBased", true);
                    clip.inputTriggered = it->value("inputTriggered", false);
                    clip.tickBasedReturnToIdle = it->value("tickBasedReturnToIdle", false);
                    if (it->contains("keyframes")) {
                        for (const auto& kf : it->at("keyframes")) {
                            AnimKeyframe keyframe;
                            keyframe.tick = kf.value("tick", 0);
                            if (kf.contains("parts")) {
                                for (auto p = kf["parts"].begin(); p != kf["parts"].end(); ++p) {
                                    AnimKeyframePart part;
                                    if (p->contains("translation") && p->at("translation").size() >= 3)
                                        part.translation = glm::vec3(p->at("translation")[0], p->at("translation")[1], p->at("translation")[2]);
                                    if (p->contains("rotation") && p->at("rotation").size() >= 3)
                                        part.rotation = glm::vec3(p->at("rotation")[0], p->at("rotation")[1], p->at("rotation")[2]);
                                    keyframe.parts[p.key()] = part;
                                }
                            }
                            clip.keyframes.push_back(keyframe);
                        }
                    }
                    loaded.layers.animations[it.key()] = clip;
                }
            }

            if (layers.contains("reloadOverlay")) {
                const auto& ro = layers["reloadOverlay"];
                if (ro.contains("translation") && ro["translation"].size() >= 3)
                    loaded.layers.reloadOverlay.translation = glm::vec3(ro["translation"][0], ro["translation"][1], ro["translation"][2]);
                if (ro.contains("rotation") && ro["rotation"].size() >= 3)
                    loaded.layers.reloadOverlay.rotation = glm::vec3(ro["rotation"][0], ro["rotation"][1], ro["rotation"][2]);
            }
        }

        gPlayerProcedural = loaded;
        printf("[HOT RELOAD] player procedural config reloaded\n");
        return true;
    }
    catch (const std::exception& e)
    {
        printf("[HOT RELOAD] player procedural config reload failed: %s\n", e.what());
        return false;
    }
}

void updatePlayerProceduralHotReload(float dt)
{
    (void)dt;
    // Use wall-clock throttling so this can be called from main loop too
    const auto now = std::chrono::steady_clock::now();
    if (gPlayerProceduralLastCheck.time_since_epoch().count() == 0) {
        gPlayerProceduralLastCheck = now;
        return;
    }
    if (now - gPlayerProceduralLastCheck < std::chrono::milliseconds(250))
        return;
    gPlayerProceduralLastCheck = now;

    std::error_code ec;
    if (!std::filesystem::exists(PLAYER_PROCEDURAL_CONFIG_PATH, ec) || ec)
        return;

    const auto writeTime = std::filesystem::last_write_time(PLAYER_PROCEDURAL_CONFIG_PATH, ec);
    if (ec || writeTime == gPlayerProceduralLastWrite)
        return;

    gPlayerProceduralLastWrite = writeTime;
    if (reloadPlayerProceduralConfig()) {
        printf("[PROC HOT RELOAD] config reloaded: writeTime changed\n");
    }
}
