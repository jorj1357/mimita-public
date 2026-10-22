// 2026-09-07 16:20 EST
/* purpose
* load and hot-reload local player presentation preferences
* expose validated enemy, teammate, and self outline settings
* provide one effective local policy input to the player renderer
* does NOT own player simulation, collision, or network authority
* does NOT decide team membership
* does NOT render geometry
*/
#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <glm/glm.hpp>

struct PlayerOutlineSettings {
    bool enabled = false;
    float thickness = 1.0f;
    float alpha = 1.0f;
    glm::vec3 color{255.0f, 255.0f, 255.0f};
    bool visibleThroughWalls = false;
    bool disappearOnDeath = true;
};

struct PlayerCapsuleSettings {
    bool enabled = false;
    std::string geometrySource = "collision";
    float alpha = 0.15f;
    glm::vec3 color{0.0f};
    bool frontFaceCull = true;
    bool backFaceCull = false;
    bool depthTest = true;
    bool depthWrite = false;
    bool visibleThroughWalls = false;
    float scale = 1.05f;
};

struct PlayerWireframeSettings {
    bool enabled = false;
    float alpha = 0.9f;
    glm::vec3 color{255.0f};
    float lineWidth = 1.0f;
    bool visibleThroughWalls = false;
    bool disappearOnDeath = true;
};

struct PlayerTrailSettings {
    bool enabled = false;
    float minSpeed = 35.0f;
    float sampleIntervalSeconds = 0.035f;
    float lifetimeSeconds = 0.22f;
    int maxSamples = 12;
    float alpha = 0.65f;
    float width = 3.0f;
    glm::vec3 color{255.0f, 80.0f, 40.0f};
};

struct PlayerPointLightSettings {
    bool enabled = false;
    float intensity = 1.5f;
    float radius = 8.0f;
    float heightOffset = 1.0f;
    glm::vec3 color{255.0f, 110.0f, 45.0f};
};

struct PlayerContrastSettings {
    bool enabled = false;
    float brightness = 1.0f;
    glm::vec3 tint{255.0f};
    float tintStrength = 0.0f;
};

struct PlayerVisualsData {
    std::vector<std::string> renderOrder{"trail", "capsule", "outline", "wireframe"};
    PlayerOutlineSettings self;
    PlayerOutlineSettings enemy;
    PlayerOutlineSettings teammate;
    PlayerCapsuleSettings selfCapsule, enemyCapsule, teammateCapsule;
    PlayerWireframeSettings selfWireframe, enemyWireframe, teammateWireframe;
    PlayerTrailSettings enemyTrail, teammateTrail;
    PlayerPointLightSettings enemyPointLight, teammatePointLight;
    PlayerContrastSettings selfContrast, enemyContrast, teammateContrast;
};

class PlayerVisualsConfig {
public:
    static PlayerVisualsConfig& instance();
    bool load();
    bool reload();
    bool pollReload();
    const PlayerVisualsData& data() const { return mData; }
    const std::string& lastError() const { return mLastError; }
    std::string describe() const;

    // Temporary in-memory gamemode policy. It never writes
    // config/playervisuals.json. While disabled, every configured player layer
    // (outline/capsule/wireframe) is suppressed; configured enabled switches
    // remain intact so clearing the policy restores the local presentation.
    void setPlayerOutlinesEnabled(bool enabled);
    void clearPlayerOutlinesOverride();
    void setTeamOutlineColors(const glm::vec3& friendly, const glm::vec3& enemy);
    void clearTeamOutlineColorsOverride();
    glm::vec3 effectiveOutlineColor(bool teammate, const glm::vec3& configured) const;
    void setFriendlyOutlinesEnabled(bool enabled);
    bool friendlyOutlinesEnabled() const { return mFriendlyOutlinesEnabled; }
    void setEnemyOutlineAlpha(float alpha);
    void clearEnemyOutlineAlphaOverride();
    float effectiveEnemyOutlineAlpha(float configured) const;
    bool hasEnemyOutlineAlphaOverride() const { return mEnemyOutlineAlphaOverride; }
    bool hasPlayerOutlinesOverride() const { return mPlayerOutlinesOverride; }
    bool playerOutlinesEnabled() const { return mPlayerOutlinesEnabled; }

private:
    PlayerVisualsConfig();
    bool parseAndValidate(const std::string& text, PlayerVisualsData& out);
    std::filesystem::path mPath{"config/playervisuals.json"};
    std::filesystem::file_time_type mLastWrite{};
    std::filesystem::file_time_type mLastAttemptedWrite{};
    PlayerVisualsData mData;
    std::string mLastError;
    bool mPlayerOutlinesOverride = false;
    bool mPlayerOutlinesEnabled = true;
    bool mTeamOutlineColorsOverride = false;
    glm::vec3 mFriendlyOutlineColor{0.0f, 255.0f, 0.0f};
    glm::vec3 mEnemyOutlineColor{255.0f, 0.0f, 0.0f};
    bool mFriendlyOutlinesEnabled = false;
    bool mEnemyOutlineAlphaOverride = false;
    float mEnemyOutlineAlpha = 0.25f;
};
