#include "player-animation-config.h"

#include "animation-rotation.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
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
    // Version-2 explicit encodings. Exactly one of the two should be present.
    const bool hasEuler =
        j.contains("rotationEulerDegrees") && j["rotationEulerDegrees"].is_array() &&
        j["rotationEulerDegrees"].size() >= 3;
    const bool hasQuat =
        j.contains("rotationQuaternionWxyz") && j["rotationQuaternionWxyz"].is_array() &&
        j["rotationQuaternionWxyz"].size() >= 4;
    if (hasEuler && hasQuat) {
        printf("[ANIM WARNING] part keyframe has both rotationEulerDegrees and "
               "rotationQuaternionWxyz; using the Euler field\n");
    }
    if (hasEuler) {
        const glm::vec3 euler(j["rotationEulerDegrees"][0], j["rotationEulerDegrees"][1],
                              j["rotationEulerDegrees"][2]);
        part.rotation = euler;
        part.rotationQuat = AnimRotation::eulerXyzDegreesToQuat(euler);
        read = true;
    } else if (hasQuat) {
        const glm::quat q(j["rotationQuaternionWxyz"][0].get<float>(),
                          j["rotationQuaternionWxyz"][1].get<float>(),
                          j["rotationQuaternionWxyz"][2].get<float>(),
                          j["rotationQuaternionWxyz"][3].get<float>());
        part.rotationQuat = AnimRotation::safeNormalize(q);
        part.rotation = AnimRotation::eulerXyzDegreesFromQuat(part.rotationQuat);
        read = true;
    } else if (j.contains("rotation") && j["rotation"].is_array() && j["rotation"].size() >= 3) {
        // Legacy version-1 Euler XYZ degrees.
        const glm::vec3 euler(j["rotation"][0], j["rotation"][1], j["rotation"][2]);
        part.rotation = euler;
        part.rotationQuat = AnimRotation::eulerXyzDegreesToQuat(euler);
        read = true;
    }
    if (j.contains("scale") && j["scale"].is_array() && j["scale"].size() >= 3) {
        part.scale = glm::vec3(j["scale"][0], j["scale"][1], j["scale"][2]);
        read = true;
    }
    return read;
}

static AnimInterp parseInterp(const std::string& s)
{
    if (s == "smooth") return AnimInterp::Smooth;
    if (s == "easein") return AnimInterp::EaseIn;
    if (s == "easeout") return AnimInterp::EaseOut;
    if (s == "easeinout") return AnimInterp::EaseInOut;
    if (s == "step" || s == "cut") return AnimInterp::Step;
    return AnimInterp::Linear;
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
            loaded.layers.animations[it.key()] = parseAnimClipJson(it.value());
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

        // First pass: parse all poses to ensure alias targets exist
        std::unordered_map<std::string, nlohmann::json> poseJsons;
        for (auto poseIt = weapon["poses"].begin(); poseIt != weapon["poses"].end(); ++poseIt) {
            if (poseIt.value().is_object())
                poseJsons[poseIt.key()] = poseIt.value();
        }

        // Second pass: resolve aliases and register poses
        for (auto& [poseName, poseJson] : poseJsons) {
            WeaponPoseConfig poseCfg;
            // Resolve alias if present
            if (poseJson.contains("alias") && poseJson["alias"].is_string()) {
                std::string aliasTarget = poseJson["alias"].get<std::string>();
                auto aliasIt = poseJsons.find(aliasTarget);
                if (aliasIt != poseJsons.end()) {
                    poseCfg = parseWeaponPose(aliasIt->second);
                } else {
                    printf("[ANIM WARNING] weapon=%s pose=%s alias='%s' not found, using defaults\n",
                           weaponId.c_str(), poseName.c_str(), aliasTarget.c_str());
                    poseCfg.useWeaponPose = true;
                }
            } else {
                poseCfg = parseWeaponPose(poseJson);
            }
            loaded.weaponPoses[weaponId + ":" + poseName] = poseCfg;
            if (poseName == activePose || poseName == "idle" || poseName == "equipped")
                loaded.weaponPoses[weaponId] = poseCfg;
        }

        // Validate that key poses exist for HAFS-type weapons
        if (weaponId == "hafs") {
            auto checkPose = [&](const std::string& pn) {
                if (loaded.weaponPoses.find(weaponId + ":" + pn) == loaded.weaponPoses.end())
                    printf("[ANIM WARNING] weapon=%s pose='%s' is missing from animations.json\n",
                           weaponId.c_str(), pn.c_str());
            };
            checkPose("idle");
            checkPose("slash");
            checkPose("lunge");
        }
    }
}

} // namespace

AnimClip parseAnimClipJson(const nlohmann::json& j)
{
    AnimClip clip;
    clip.durationTicks = j.value("durationTicks", 60);
    clip.loop = j.value("loop", true);
    clip.speedScaleFromVelocity = j.value("speedScaleFromVelocity", true);
    clip.speedBased = j.value("speedBased", true);
    clip.inputTriggered = j.value("inputTriggered", false);
    clip.tickBasedReturnToIdle = j.value("tickBasedReturnToIdle", false);
    clip.priority = j.value("priority", 0);
    clip.blendInTicks = j.value("blendInTicks", 0);
    clip.blendOutTicks = j.value("blendOutTicks", 0);

    if (j.contains("markers") && j["markers"].is_array()) {
        static const char* phaseNames[4] = {
            "phase.windup", "phase.active", "phase.recovery", "action.end"};
        std::array<int, 4> seen{{0, 0, 0, 0}};
        int sourceCount = 0;
        for (const auto& marker : j["markers"]) {
            if (!marker.is_object() || !marker.contains("name") ||
                !marker["name"].is_string())
                continue;
            const std::string name = marker["name"].get<std::string>();
            int index = -1;
            for (int i = 0; i < 4; ++i)
                if (name == phaseNames[i]) { index = i; break; }
            if (index < 0)
                continue;

            clip.hasMeleePhaseTimeline = true;
            if (!marker.contains("tick") || !marker["tick"].is_number_integer())
                throw std::runtime_error("Melee marker '" + name + "' needs an integer tick");
            if (++seen[index] != 1)
                throw std::runtime_error("duplicate Melee marker '" + name + "'");
            clip.phaseTicks[index] = marker["tick"].get<int>();
            clip.phaseSourceTicks[index] = clip.phaseTicks[index];

            if (marker.contains("sourceTick")) {
                if (!marker["sourceTick"].is_number_integer())
                    throw std::runtime_error("Melee marker '" + name + "' has a non-integer sourceTick");
                clip.phaseSourceTicks[index] = marker["sourceTick"].get<int>();
                ++sourceCount;
            }
        }

        if (clip.hasMeleePhaseTimeline) {
            if (seen != std::array<int, 4>{{1, 1, 1, 1}})
                throw std::runtime_error("Melee clip must contain exactly one of each required phase marker");
            if (sourceCount != 0 && sourceCount != 4)
                throw std::runtime_error("Melee clip must provide sourceTick on all phase markers or none");
            if (clip.phaseTicks[0] != 0 || clip.phaseTicks[3] != clip.durationTicks ||
                clip.durationTicks <= 0 ||
                !std::is_sorted(clip.phaseTicks.begin(), clip.phaseTicks.end()) ||
                std::adjacent_find(clip.phaseTicks.begin(), clip.phaseTicks.end()) != clip.phaseTicks.end())
                throw std::runtime_error("Melee runtime markers must increase from tick 0 through durationTicks");
            if (clip.phaseSourceTicks[0] != 0 ||
                !std::is_sorted(clip.phaseSourceTicks.begin(), clip.phaseSourceTicks.end()) ||
                std::adjacent_find(clip.phaseSourceTicks.begin(), clip.phaseSourceTicks.end()) != clip.phaseSourceTicks.end())
                throw std::runtime_error("Melee sourceTick markers must increase from tick 0");
        }
    }

    if (!j.contains("keyframes") || !j["keyframes"].is_array()) {
        if (clip.hasMeleePhaseTimeline)
            throw std::runtime_error("Melee clip needs a keyframes array");
        return clip;
    }

    for (const auto& kf : j["keyframes"]) {
        AnimKeyframe keyframe;
        keyframe.tick = kf.value("tick", 0);
        if (kf.contains("interp") && kf["interp"].is_string())
            keyframe.interp = parseInterp(kf["interp"].get<std::string>());
        if (kf.contains("parts") && kf["parts"].is_object()) {
            for (auto p = kf["parts"].begin(); p != kf["parts"].end(); ++p) {
                AnimKeyframePart part;
                readAnimPart(p.value(), part);
                keyframe.parts[p.key()] = part;
            }
        }
        clip.keyframes.push_back(keyframe);
    }
    if (clip.hasMeleePhaseTimeline && !clip.keyframes.empty()) {
        const int lastAuthoredTick = std::max_element(
            clip.keyframes.begin(), clip.keyframes.end(),
            [](const AnimKeyframe& a, const AnimKeyframe& b) { return a.tick < b.tick; })->tick;
        if (clip.phaseSourceTicks[3] <= lastAuthoredTick)
            throw std::runtime_error("Melee action.end.sourceTick must follow the last authored pose tick");
    }
    return clip;
}

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
        root = nlohmann::json::parse(file, nullptr, true, true);
        parseSway(root, loaded);
        parseLayers(root, loaded);
        parseWeaponPoses(root, loaded);
    } catch (const std::exception& e) {
        printf("[HOT RELOAD] animation config reload failed: %s\n", e.what());
    }
}
