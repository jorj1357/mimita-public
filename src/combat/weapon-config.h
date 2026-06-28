#pragma once

#include <string>
#include <chrono>
#include <filesystem>
#include <glm/glm.hpp>

struct WeaponViewModelConfig {
    std::string modelPath = "assets/objects/weapons/mimita-rpg-v3.glb";
    glm::vec3 positionOffset{0.35f, -0.75f, -0.35f};
    glm::vec3 rotationDegrees{0.0f};
    glm::vec3 scale{1.0f};
    bool enabled = true;
};

class WeaponConfig {
public:
    static WeaponConfig& instance();

    bool load(const std::string& weaponId);
    const WeaponViewModelConfig* get(const std::string& weaponId) const;
    void pollHotReload(const std::string& weaponId);

private:
    WeaponConfig() = default;

    std::string mWeaponId;
    WeaponViewModelConfig mConfig;
    std::filesystem::file_time_type mLastWriteTime;
    std::chrono::steady_clock::time_point mLastCheckTime;
    float mPollInterval = 0.25f;
    bool mLoaded = false;
};
