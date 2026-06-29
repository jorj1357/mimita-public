#pragma once

#include <string>
#include <chrono>
#include <filesystem>
#include <glm/glm.hpp>

struct WeaponFireAnim {
    float duration = 0.12f;
    glm::vec3 positionOffset{0.0f};
    glm::vec3 rotationOffset{0.0f};
    bool recover = true;
};

struct WeaponReloadPose {
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};
};

struct WeaponViewModelConfig {
    std::string modelPath; // empty = use weapon definition's modelPath
    glm::vec3 positionOffset{0.35f, -0.75f, -0.35f};
    glm::vec3 rotationDegrees{0.0f};
    glm::vec3 scale{1.0f};
    bool enabled = true;

    WeaponFireAnim fireAnim;
    WeaponReloadPose reloadPose;
    bool hasFireAnim = false;
    bool hasReloadPose = false;
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
