#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

struct RagdollModeCapsuleConfig {
    float radius = 0.15f;
    float halfHeight = 0.15f;
    glm::vec3 offset{0.0f};
};

struct RagdollModeAttachmentConfig {
    std::string parent;
    glm::vec3 offset{0.0f};
    float coneLimitDeg = 90.0f;
};

struct RagdollModeConfigData {
    bool enabled = true;
    std::string toggleKey = "G";
    float gravityScale = 1.0f;
    float linearDamping = 0.15f;
    float angularDamping = 0.3f;
    float jointStiffness = 2000.0f;
    float jointDamping = 80.0f;
    bool worldCollision = true;

    std::unordered_map<std::string, RagdollModeCapsuleConfig> capsules;
    std::unordered_map<std::string, RagdollModeAttachmentConfig> attachments;

    // Grab settings
    std::string leftKey = "A";
    std::string rightKey = "D";
    int extendLeftMouse = 0;
    int extendRightMouse = 1;
    float extendForce = 50.0f;
    float grabReach = 2.5f;
    float grabRadius = 0.3f;

    // Weapon
    bool oneHandedWeaponEnabled = true;
    std::string weaponHand = "right";

    // Camera
    std::string cameraMode = "locked_to_head";
    float cameraSmoothFactor = 0.0f;
};

class RagdollModeConfig {
public:
    static RagdollModeConfig& instance();
    bool load(const std::string& path = "config/ragdoll.json");
    bool pollReload();
    const RagdollModeConfigData& data() const { return mData; }
    RagdollModeConfigData& data() { return mData; }

private:
    RagdollModeConfig() = default;
    RagdollModeConfigData mData;
    std::string mPath = "config/ragdoll.json";
    std::filesystem::file_time_type mLastWrite{};
    bool mWatchLogged = false;
};
