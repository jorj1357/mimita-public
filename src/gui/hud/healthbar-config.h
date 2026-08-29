#pragma once

#include <cstdint>
#include <string>

struct HealthbarConfigData {
    bool aimModeEnabled = true;
    float aimConeDegrees = 45.0f;
    float triangleSize = 14.0f;
    float triangleOffset = 8.0f;
    float triangleAlpha = 0.95f;
    int triangleFadeTicks = 8;
    int blinkHpThreshold = 5;
    int blinkTicks = 60;
    bool showNameInAimMode = false;
    bool showHpTextInAimMode = false;
    bool showBarInAimMode = false;
    float maxDistance = 2000.0f;
    float startFadeDistance = 1000.0f;
    float endFadeDistance = 2000.0f;
    glm::vec4 greenColor{0.2f, 1.0f, 0.3f, 1.0f};
    glm::vec4 yellowColor{1.0f, 1.0f, 0.2f, 1.0f};
    glm::vec4 orangeColor{1.0f, 0.5f, 0.1f, 1.0f};
    glm::vec4 redColor{1.0f, 0.1f, 0.1f, 1.0f};
    glm::vec4 blackColor{0.0f, 0.0f, 0.0f, 1.0f};
};

class HealthbarConfig {
public:
    static HealthbarConfig& instance();

    bool load(const std::string& path = "config/healthbar.json");
    bool save();
    bool reload();
    bool pollReload();

    const HealthbarConfigData& data() const { return mData; }
    HealthbarConfigData& edit();

private:
    HealthbarConfig() = default;

    HealthbarConfigData mData;
    std::string mPath = "config/healthbar.json";
    int64_t mLastModified = 0;
};
