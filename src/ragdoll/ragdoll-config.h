#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

struct RagdollPartConfig {
    glm::vec3 offset{0.0f};
    float mass = 1.0f;
    float radius = 0.2f;
};

struct RagdollConfigData {
    bool enabled = true;
    float lifetimeSeconds = 30.0f;
    float deathImpulseMultiplier = 1.0f;
    float spawnVelocityMultiplier = 1.0f;
    bool inheritPlayerVelocity = true;
    bool inheritPlayerAngularVelocity = true;
    float gravityScale = 1.0f;
    float linearDamping = 0.1f;
    float angularDamping = 0.2f;
    bool selfCollision = true;
    bool worldCollision = true;
    bool playerCollision = true;
    bool npcCollision = true;
    float jointStiffness = 1000.0f;
    float jointDamping = 50.0f;
    float jointBreakForce = 1000000.0f;
    std::unordered_map<std::string, RagdollPartConfig> parts;
};

class RagdollConfig {
public:
    static RagdollConfig& instance();
    bool load(const std::string& path = "config/ragdolldeath.json");
    bool pollReload();
    const RagdollConfigData& data() const { return mData; }
    RagdollConfigData& data() { return mData; }
private:
    RagdollConfig() = default;
    RagdollConfigData mData;
    std::string mPath = "config/ragdolldeath.json";
    std::filesystem::file_time_type mLastWrite{};
    bool mWatchLogged = false;
};
