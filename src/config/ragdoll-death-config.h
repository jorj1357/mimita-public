#pragma once

#include <filesystem>
#include <string>
#include <glm/glm.hpp>

struct RagdollDeathConfigData {
    bool enabled = true;
    int totalTicks = 180;
    float startAlpha = 1.0f;
    float endAlpha = 0.0f;
    glm::vec3 startRotation{0.0f, 0.0f, 0.0f};
    glm::vec3 endRotation{-90.0f, 0.0f, 0.0f};
};

class RagdollDeathConfig {
public:
    static RagdollDeathConfig& instance();

    bool load(const std::string& path = "config/ragdolldeath.json");
    bool pollReload();

    bool enabled() const { return mData.enabled; }
    int totalTicks() const { return mData.totalTicks; }
    float startAlpha() const { return mData.startAlpha; }
    float endAlpha() const { return mData.endAlpha; }
    glm::vec3 startRotation() const { return mData.startRotation; }
    glm::vec3 endRotation() const { return mData.endRotation; }
    const RagdollDeathConfigData& data() const { return mData; }

private:
    RagdollDeathConfig() = default;

    RagdollDeathConfigData mData;
    std::string mPath = "config/ragdolldeath.json";
    std::filesystem::file_time_type mLastWrite{};
    bool mWatchLogged = false;
};
