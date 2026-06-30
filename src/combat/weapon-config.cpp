#include "weapon-config.h"

#include <cstdio>
#include <fstream>

#include <nlohmann/json.hpp>

#include "devtools/terminal.h"

using json = nlohmann::json;

static const char* SINGLE_CONFIG_PATH = "config/weapons.json";

WeaponConfig& WeaponConfig::instance() {
    static WeaponConfig cfg;
    return cfg;
}

static WeaponViewModelConfig parseOne(const json& root) {
    WeaponViewModelConfig cfg;
    if (root.contains("model") && root["model"].is_object())
        cfg.modelPath = root["model"].value("path", "");

    if (root.contains("viewmodel") && root["viewmodel"].is_object()) {
        auto& vm = root["viewmodel"];
        cfg.enabled = vm.value("enabled", true);
        if (vm.contains("position") && vm["position"].is_array() && vm["position"].size() >= 3)
            cfg.positionOffset = glm::vec3((float)vm["position"][0], (float)vm["position"][1], (float)vm["position"][2]);
        if (vm.contains("rotation_degrees") && vm["rotation_degrees"].is_array() && vm["rotation_degrees"].size() >= 3)
            cfg.rotationDegrees = glm::vec3((float)vm["rotation_degrees"][0], (float)vm["rotation_degrees"][1], (float)vm["rotation_degrees"][2]);
        if (vm.contains("scale") && vm["scale"].is_array() && vm["scale"].size() >= 3)
            cfg.scale = glm::vec3((float)vm["scale"][0], (float)vm["scale"][1], (float)vm["scale"][2]);
        if (vm.contains("color") && vm["color"].is_array() && vm["color"].size() >= 3)
            cfg.color = glm::vec3((float)vm["color"][0], (float)vm["color"][1], (float)vm["color"][2]);
    }

    if (root.contains("animations") && root["animations"].is_object()) {
        auto& anims = root["animations"];
        if (anims.contains("fire") && anims["fire"].is_object()) {
            auto& fire = anims["fire"];
            cfg.hasFireAnim = true;
            cfg.fireAnim.duration = fire.value("duration", 0.12f);
            if (fire.contains("position_offset") && fire["position_offset"].is_array() && fire["position_offset"].size() >= 3)
                cfg.fireAnim.positionOffset = glm::vec3((float)fire["position_offset"][0], (float)fire["position_offset"][1], (float)fire["position_offset"][2]);
            if (fire.contains("rotation_offset") && fire["rotation_offset"].is_array() && fire["rotation_offset"].size() >= 3)
                cfg.fireAnim.rotationOffset = glm::vec3((float)fire["rotation_offset"][0], (float)fire["rotation_offset"][1], (float)fire["rotation_offset"][2]);
            cfg.fireAnim.recover = fire.value("recover", true);
        }
        if (anims.contains("reload_pose") && anims["reload_pose"].is_object()) {
            auto& rp = anims["reload_pose"];
            cfg.hasReloadPose = true;
            if (rp.contains("position") && rp["position"].is_array() && rp["position"].size() >= 3)
                cfg.reloadPose.position = glm::vec3((float)rp["position"][0], (float)rp["position"][1], (float)rp["position"][2]);
            if (rp.contains("rotation") && rp["rotation"].is_array() && rp["rotation"].size() >= 3)
                cfg.reloadPose.rotation = glm::vec3((float)rp["rotation"][0], (float)rp["rotation"][1], (float)rp["rotation"][2]);
        }
    }
    return cfg;
}

void WeaponConfig::load() {
    mConfigs.clear();
    std::ifstream file(SINGLE_CONFIG_PATH);
    if (!file.is_open()) {
        printf("[WEAPON CONFIG] No config at %s; all weapons use defaults\n", SINGLE_CONFIG_PATH);
        mLoaded = true;
        return;
    }

    try {
        json root;
        file >> root;

        for (auto it = root.begin(); it != root.end(); ++it) {
            const std::string& weaponId = it.key();
            if (!it.value().is_object()) continue;
            mConfigs[weaponId] = parseOne(it.value());
        }

        mLoaded = true;

        if (std::filesystem::exists(SINGLE_CONFIG_PATH))
            mLastWriteTime = std::filesystem::last_write_time(SINGLE_CONFIG_PATH);
        mLastCheckTime = std::chrono::steady_clock::now();

        printf("[WEAPON CONFIG] Loaded %s (%zu weapons)\n", SINGLE_CONFIG_PATH, mConfigs.size());
        for (auto& pair : mConfigs) {
            auto& cfg = pair.second;
            printf("[WEAPON CONFIG]   %s: path=%s pos=(%.2f,%.2f,%.2f) rot=(%.1f,%.1f,%.1f) scale=(%.2f,%.2f,%.2f)%s%s\n",
                   pair.first.c_str(),
                   cfg.modelPath.empty() ? "(def)" : cfg.modelPath.c_str(),
                   cfg.positionOffset.x, cfg.positionOffset.y, cfg.positionOffset.z,
                   cfg.rotationDegrees.x, cfg.rotationDegrees.y, cfg.rotationDegrees.z,
                   cfg.scale.x, cfg.scale.y, cfg.scale.z,
                   cfg.hasFireAnim ? " [fire]" : "",
                   cfg.hasReloadPose ? " [reload]" : "");
        }
        Terminal::instance().addLog(std::string("[WEAPON CONFIG] Loaded ") + SINGLE_CONFIG_PATH);

    } catch (const std::exception& e) {
        printf("[WEAPON CONFIG ERROR] Failed to parse %s: %s\n", SINGLE_CONFIG_PATH, e.what());
        Terminal::instance().addLog(std::string("[WEAPON CONFIG ERROR] ") + e.what());
        if (!mLoaded) {
            mLoaded = true;
        }
    }
}

WeaponConfig::WeaponConfig() {
    load();
}

const WeaponViewModelConfig* WeaponConfig::get(const std::string& weaponId) const {
    if (!mLoaded) return nullptr;
    auto it = mConfigs.find(weaponId);
    if (it == mConfigs.end()) return nullptr;
    return &it->second;
}

void WeaponConfig::reloadNow() {
    load();
}

void WeaponConfig::pollHotReload() {
    if (!mLoaded) {
        load();
        return;
    }

    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration<float>(now - mLastCheckTime).count() < mPollInterval)
        return;
    mLastCheckTime = now;

    if (!std::filesystem::exists(SINGLE_CONFIG_PATH))
        return;

    auto writeTime = std::filesystem::last_write_time(SINGLE_CONFIG_PATH);
    if (writeTime == mLastWriteTime)
        return;
    mLastWriteTime = writeTime;

    printf("[WEAPON CONFIG] Reloaded %s\n", SINGLE_CONFIG_PATH);
    Terminal::instance().addLog(std::string("[WEAPON CONFIG] Reloaded ") + SINGLE_CONFIG_PATH);
    load();
}
