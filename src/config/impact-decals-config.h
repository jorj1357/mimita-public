// 08 14 2026, 09 15
/* purpose
* Declares the ImpactDecalsConfig singleton that reads config/impact_decals.json.
* Owns per-effect settings for blood surface splats, bullet holes, and world cracks.
* Does NOT spawn or render effects.
* Does NOT own hit detection or damage logic.
*/
#pragma once

#include <filesystem>
#include <string>
#include <glm/glm.hpp>

struct ImpactDecalSprayConfig {
    bool enabled = true;
    int minCount = 4;
    int maxCount = 40;
    float sizeMin = 0.05f;
    float sizeMax = 0.18f;
    float bigFraction = 0.7f;
    float speedMin = 3.0f;
    float speedMax = 16.0f;
    float coneDegreesMin = 8.0f;
    float coneDegreesMax = 30.0f;
    float lifetimeMin = 1.0f;
    float lifetimeMax = 3.0f;
    float alphaMin = 0.6f;
    float alphaMax = 0.95f;
};

struct ImpactForceConfig {
    float maxDistance = 1.0f;
    float minDistance = 30.0f;
    float minForce = 0.08f;
};

struct ImpactDecalGroupConfig {
    bool enabled = true;
    int count = 12;
    int minCount = 3;
    float coneDegrees = 35.0f;
    float coneDistance = 6.0f;
    float radius = 0.025f;
    float minRadius = 0.012f;
    float height = 0.05f;
    float length = 0.35f;
    float thickness = 0.015f;
    glm::vec3 color{0.75f, 0.01f, 0.02f};
    float colorVariation = 0.0f;
    float alpha = 0.9f;
    float lifetime = 30.0f;
    float fadeTime = 5.0f;
    int maxCount = 256;
    struct StaggerConfig {
        bool enabled = false;
        int decalsPerTick = 1;
        int startDelayTicks = 0;
        int maxTicks = 60;
        int rayBudgetPerFrame = 2;  // max ray-traces per frame for deferred blood decals
    } stagger;
    struct ColorOverLifetimeConfig {
        bool enabled = false;
        glm::vec3 startColor{0.8f, 0.0f, 0.0f};
        glm::vec3 endColor{0.18f, 0.0f, 0.0f};
        float darkenStartSeconds = 1.0f;
        float darkenEndSeconds = 12.0f;
    } colorOverLifetime;
    struct CrackArmsConfig {
        int baseCount = 3;
        float weaponForceMultiplier = 0.08f;
        int maxCount = 12;
    } crackArms;
    struct CrackChainConfig {
        int minSegments = 1;
        int maxSegments = 5;
        float segmentLengthMin = 0.05f;
        float segmentLengthMax = 0.22f;
        float turnDegreesMin = -45.0f;
        float turnDegreesMax = 45.0f;
        float jaggedness = 0.35f;
    } crackChain;
    float crackCenterThickness = 0.035f;
    float crackOuterThickness = 0.006f;
    ImpactDecalSprayConfig spray;
    ImpactForceConfig force;
};

struct ImpactDecalsData {
    bool enabled = true;
    ImpactDecalGroupConfig blood;
    ImpactDecalGroupConfig bulletHoles;
    ImpactDecalGroupConfig worldCracks;
};

class ImpactDecalsConfig {
public:
    static ImpactDecalsConfig& instance();

    bool load(const std::string& path = "config/impact_decals.json");
    bool pollReload();

    const ImpactDecalsData& data() const { return mData; }

private:
    ImpactDecalsConfig() = default;

    ImpactDecalsData mData;
    std::string mPath = "config/impact_decals.json";
    std::filesystem::file_time_type mLastWrite{};
};
