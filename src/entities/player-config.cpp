#include "player.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

#include "debug/debug-log.h"
#include "player-animation-config.h"

namespace {
const char* PLAYER_PROCEDURAL_CONFIG_PATH = "config/player-procedural.json";
std::filesystem::file_time_type gPlayerProceduralLastWrite{};
std::chrono::steady_clock::time_point gPlayerProceduralLastCheck{};
} // anonymous namespace

PlayerProceduralConfig gPlayerProcedural{};

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

void readJsonVec3(
    const nlohmann::json& j,
    const char* key,
    glm::vec3& value
)
{
    if (j.contains(key) && j[key].is_array() && j[key].size() >= 3)
        value = glm::vec3(j[key][0].get<float>(), j[key][1].get<float>(), j[key][2].get<float>());
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
        readJsonValue(j, "walkStartTickOnEnter", loaded.walkStartTickOnEnter);
        readJsonValue(j, "animationStateTransitionFrames", loaded.animationStateTransitionFrames);

        // Parse dash pose config
        if (j.contains("dashPose")) {
            const auto& dp = j["dashPose"];
            readJsonVec3(dp, "torsoRotation", loaded.dashPose.torsoRotation);
            readJsonVec3(dp, "torsoTranslation", loaded.dashPose.torsoTranslation);
            readJsonVec3(dp, "headRotation", loaded.dashPose.headRotation);
            readJsonVec3(dp, "headTranslation", loaded.dashPose.headTranslation);
            readJsonVec3(dp, "leftArmRotation", loaded.dashPose.leftArmRotation);
            readJsonVec3(dp, "leftArmTranslation", loaded.dashPose.leftArmTranslation);
            readJsonVec3(dp, "rightArmRotation", loaded.dashPose.rightArmRotation);
            readJsonVec3(dp, "rightArmTranslation", loaded.dashPose.rightArmTranslation);
            readJsonVec3(dp, "leftLegRotation", loaded.dashPose.leftLegRotation);
            readJsonVec3(dp, "leftLegTranslation", loaded.dashPose.leftLegTranslation);
            readJsonVec3(dp, "rightLegRotation", loaded.dashPose.rightLegRotation);
            readJsonVec3(dp, "rightLegTranslation", loaded.dashPose.rightLegTranslation);
            readJsonValue(dp, "blendInTime", loaded.dashPose.blendInTime);
            readJsonValue(dp, "blendOutTime", loaded.dashPose.blendOutTime);
            readJsonValue(dp, "snapIn", loaded.dashPose.snapIn);
        }

        // Parse freeze pose config
        if (j.contains("freezePose")) {
            const auto& fp = j["freezePose"];
            readJsonVec3(fp, "torsoRotation", loaded.freezePose.torsoRotation);
            readJsonVec3(fp, "torsoTranslation", loaded.freezePose.torsoTranslation);
            readJsonVec3(fp, "headRotation", loaded.freezePose.headRotation);
            readJsonVec3(fp, "headTranslation", loaded.freezePose.headTranslation);
            readJsonVec3(fp, "leftArmRotation", loaded.freezePose.leftArmRotation);
            readJsonVec3(fp, "leftArmTranslation", loaded.freezePose.leftArmTranslation);
            readJsonVec3(fp, "rightArmRotation", loaded.freezePose.rightArmRotation);
            readJsonVec3(fp, "rightArmTranslation", loaded.freezePose.rightArmTranslation);
            readJsonVec3(fp, "leftLegRotation", loaded.freezePose.leftLegRotation);
            readJsonVec3(fp, "leftLegTranslation", loaded.freezePose.leftLegTranslation);
            readJsonVec3(fp, "rightLegRotation", loaded.freezePose.rightLegRotation);
            readJsonVec3(fp, "rightLegTranslation", loaded.freezePose.rightLegTranslation);
            readJsonValue(fp, "blendInTime", loaded.freezePose.blendInTime);
            readJsonValue(fp, "blendOutTime", loaded.freezePose.blendOutTime);
            readJsonValue(fp, "snapIn", loaded.freezePose.snapIn);
        }

        // Parse idle procedural animation config
        if (j.contains("idle")) {
            const auto& idle = j["idle"];
            readJsonValue(idle, "armRotationDegrees", loaded.idleArmRotationDeg);
            readJsonValue(idle, "legRotationDegrees", loaded.idleLegRotationDeg);
            readJsonValue(idle, "torsoRotationDegrees", loaded.idleTorsoRotationDeg);
            readJsonValue(idle, "headRotationDegrees", loaded.idleHeadRotationDeg);
            readJsonValue(idle, "armSpeed", loaded.idleArmSpeed);
            readJsonValue(idle, "legSpeed", loaded.idleLegSpeed);
            readJsonValue(idle, "torsoSpeed", loaded.idleTorsoSpeed);
            readJsonValue(idle, "headSpeed", loaded.idleHeadSpeed);
            readJsonValue(idle, "breathingAmount", loaded.idleBreathingAmount);
            readJsonValue(idle, "breathingSpeed", loaded.idleBreathingSpeed);
            readJsonValue(idle, "debugStrength", loaded.idleDebugStrength);
        }

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

        loadAnimationConfig(loaded);

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
    bool changed = false;
    if (std::filesystem::exists(PLAYER_PROCEDURAL_CONFIG_PATH, ec) && !ec) {
        const auto writeTime = std::filesystem::last_write_time(PLAYER_PROCEDURAL_CONFIG_PATH, ec);
        if (!ec && writeTime != gPlayerProceduralLastWrite) {
            gPlayerProceduralLastWrite = writeTime;
            changed = true;
        }
    }
    if (animationConfigChanged())
        changed = true;
    if (!changed)
        return;

    if (reloadPlayerProceduralConfig()) {
        printf("[PROC HOT RELOAD] animation config reloaded: writeTime changed\n");
    }
}
