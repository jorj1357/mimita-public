#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <filesystem>
#include <glm/glm.hpp>

struct WeaponCollisionSphereConfig {
    std::string name;
    bool enabled = true;
    glm::vec3 offset{0.0f};
    float radius = 0.08f;
    glm::vec3 scale{1.0f};
    glm::vec3 rotationDegrees{0.0f};
};

struct WeaponCollisionCapsuleConfig {
    std::string name;
    bool enabled = true;
    glm::vec3 start{0.0f};
    glm::vec3 end{0.0f, 0.0f, 1.0f};
    float radius = 0.08f;
    glm::vec3 scale{1.0f};
    glm::vec3 rotationDegrees{0.0f};
};

struct WeaponCollisionGeneratedSpheresConfig {
    bool enabled = false;
    int count = 8;
    glm::vec3 start{0.0f};
    glm::vec3 end{0.0f, 0.0f, 1.4f};
    float radius = 0.12f;
};

struct WeaponCollisionEntry {
    bool enabled = true;
    bool visible = false;        // show capsule wireframes in-game (from JSON "visible" field)
    bool collidesWithWorld = true;
    float collisionSkin = 0.05f;
    std::string source = "capsule";  // "capsule" (default, single smooth capsule) or "json" (legacy spheres)

    // Backward compat: singular capsule
    WeaponCollisionCapsuleConfig capsule;
    // New: plural capsules array
    std::vector<WeaponCollisionCapsuleConfig> capsules;
    std::vector<WeaponCollisionSphereConfig> spheres;
    WeaponCollisionGeneratedSpheresConfig generatedSpheres;
};

class Player;

class WeaponCollisionJsonConfig {
public:
    static WeaponCollisionJsonConfig& instance();

    const WeaponCollisionEntry* get(const std::string& weaponId) const;
    void pollHotReload();
    void reloadNow();
    // Apply config to player's runtime data (debug visuals + collision).
    // Must be called after recomputeWeaponCapsule so weaponCollisionWorld is set.
    void applyCollisionConfig(Player& player);

private:
    WeaponCollisionJsonConfig() = default;
    void load();

    std::unordered_map<std::string, WeaponCollisionEntry> mConfigs;
    std::filesystem::file_time_type mLastWriteTime;
    std::chrono::steady_clock::time_point mLastCheckTime;
    float mPollInterval = 0.25f;
    bool mLoaded = false;
    bool mLastLoadOk = false;
};
