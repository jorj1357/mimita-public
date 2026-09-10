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

    // Optional per-axis rotation limits in the parent's local frame.
    bool hasRotationLimits = false;
    glm::vec3 rotMinDeg{-180.0f, -180.0f, -180.0f};
    glm::vec3 rotMaxDeg{ 180.0f,  180.0f,  180.0f};
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

    // Rigid-body solver
    int solverIterations = 24;
    float maxFallSpeed = 60.0f;
    float restitution = 0.0f;
    float friction = 0.5f;
    bool selfCollision = true;
    float bodyLinearDamping = 0.05f;
    float bodyAngularDamping = 0.05f;

    // Mass (kg). Names match the body part IDs; globalMultiplier scales all.
    std::unordered_map<std::string, float> massKg;
    float massGlobalMultiplier = 1.0f;

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
    float grabCompliance = 0.02f;
    float grabGraceDistance = 0.5f;

    // Arms
    float armExtendStrength = 40.0f;
    float armExtendMaxSpeed = 12.0f;

    // Weapon
    bool oneHandedWeaponEnabled = true;
    std::string weaponHand = "right";

    // Camera
    std::string cameraMode = "locked_to_head";
    float cameraSmoothFactor = 0.0f;
    bool thirdPersonAllowed = true;

    // Torso look direction
    float torsoLookSpring = 8.0f;
    float torsoMaxAngularStep = 15.0f;

    // Head aim
    float headRotationStrength = 12.0f;
    float headRotationSpeed = 18.0f;

    // Exit
    bool exitPreserveVelocity = true;
    float exitHopVelocity = 3.0f;
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
