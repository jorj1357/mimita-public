#include "weapon-config.h"

#include <cstdio>
#include <fstream>

#include <nlohmann/json.hpp>

#include "devtools/terminal.h"

using json = nlohmann::json;

WeaponConfig& WeaponConfig::instance() {
    static WeaponConfig cfg;
    return cfg;
}

static std::string configPath(const std::string& weaponId) {
    return "config/weapons/" + weaponId + ".json";
}

static WeaponViewModelConfig defaults() {
    WeaponViewModelConfig cfg;
    cfg.modelPath = "assets/objects/weapons/mimita-rpg-v3.glb";
    cfg.positionOffset = glm::vec3(0.35f, -0.75f, -0.35f);
    cfg.rotationDegrees = glm::vec3(0.0f);
    cfg.scale = glm::vec3(1.0f);
    cfg.enabled = true;
    return cfg;
}

bool WeaponConfig::load(const std::string& weaponId) {
    mWeaponId = weaponId;
    const std::string path = configPath(weaponId);

    std::ifstream file(path);
    if (!file.is_open()) {
        printf("[WEAPON CONFIG] No config found at %s; using defaults\n", path.c_str());
        mConfig = defaults();
        mLoaded = true;
        return true;
    }

    try {
        json root;
        file >> root;

        WeaponViewModelConfig cfg = defaults();

        // model.path
        if (root.contains("model") && root["model"].is_object()) {
            auto& model = root["model"];
            cfg.modelPath = model.value("path", cfg.modelPath);
        }

        // viewmodel block
        if (root.contains("viewmodel") && root["viewmodel"].is_object()) {
            auto& vm = root["viewmodel"];
            cfg.enabled = vm.value("enabled", true);

            if (vm.contains("position") && vm["position"].is_array() && vm["position"].size() >= 3)
                cfg.positionOffset = glm::vec3((float)vm["position"][0], (float)vm["position"][1], (float)vm["position"][2]);

            if (vm.contains("rotation_degrees") && vm["rotation_degrees"].is_array() && vm["rotation_degrees"].size() >= 3)
                cfg.rotationDegrees = glm::vec3((float)vm["rotation_degrees"][0], (float)vm["rotation_degrees"][1], (float)vm["rotation_degrees"][2]);

            if (vm.contains("scale") && vm["scale"].is_array() && vm["scale"].size() >= 3)
                cfg.scale = glm::vec3((float)vm["scale"][0], (float)vm["scale"][1], (float)vm["scale"][2]);
        }

        mConfig = cfg;
        mLoaded = true;

        if (std::filesystem::exists(path))
            mLastWriteTime = std::filesystem::last_write_time(path);
        mLastCheckTime = std::chrono::steady_clock::now();

        printf("[WEAPON CONFIG] Loaded %s\n", path.c_str());
        printf("[WEAPON CONFIG] %s model.path = %s\n", weaponId.c_str(), cfg.modelPath.c_str());
        printf("[WEAPON CONFIG] %s position = %.2f,%.2f,%.2f\n", weaponId.c_str(),
               cfg.positionOffset.x, cfg.positionOffset.y, cfg.positionOffset.z);
        printf("[WEAPON CONFIG] %s rotation_degrees = %.1f,%.1f,%.1f\n", weaponId.c_str(),
               cfg.rotationDegrees.x, cfg.rotationDegrees.y, cfg.rotationDegrees.z);
        printf("[WEAPON CONFIG] %s scale = %.2f,%.2f,%.2f\n", weaponId.c_str(),
               cfg.scale.x, cfg.scale.y, cfg.scale.z);
        Terminal::instance().addLog("[WEAPON CONFIG] Loaded " + path);
        return true;

    } catch (const std::exception& e) {
        printf("[WEAPON CONFIG ERROR] Failed to parse %s: %s\n", path.c_str(), e.what());
        printf("[WEAPON CONFIG ERROR] Keeping last good config.\n");
        Terminal::instance().addLog("[WEAPON CONFIG ERROR] Failed to parse " + path);
        if (!mLoaded) {
            mConfig = defaults();
            mLoaded = true;
        }
        return false;
    }
}

const WeaponViewModelConfig* WeaponConfig::get(const std::string& weaponId) const {
    if (!mLoaded || mWeaponId != weaponId)
        return nullptr;
    return &mConfig;
}

void WeaponConfig::pollHotReload(const std::string& weaponId) {
    if (!mLoaded || mWeaponId != weaponId) {
        load(weaponId);
        return;
    }

    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration<float>(now - mLastCheckTime).count() < mPollInterval)
        return;
    mLastCheckTime = now;

    const std::string path = configPath(weaponId);
    if (!std::filesystem::exists(path))
        return;

    auto writeTime = std::filesystem::last_write_time(path);
    if (writeTime == mLastWriteTime)
        return;
    mLastWriteTime = writeTime;

    printf("[WEAPON CONFIG] Reloaded %s\n", path.c_str());
    Terminal::instance().addLog("[WEAPON CONFIG] Reloaded " + path);
    load(weaponId);
}
