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
    float gravity = 2.5f;
    struct DebrisConfig {
        bool enabled = true;
        float countFraction = 0.33f;
        glm::vec3 color{0.35f, 0.30f, 0.25f};
        float alpha = 0.7f;
        float sizeMin = 0.02f;
        float sizeMax = 0.05f;
        float sizeJitter = 0.0f;
        float spawnOffset = 0.05f;
        float speedMin = 2.0f;
        float speedMax = 4.0f;
        float verticalVelocityMin = 1.0f;
        float verticalVelocityMax = 2.0f;
        float coneDegreesMin = 8.0f;
        float coneDegreesMax = 30.0f;
        float lifetimeMin = 1.0f;
        float lifetimeMax = 2.0f;
        float gravity = 3.0f;
        float drag = 0.0f;
        bool affectedByGravity = true;
        float rotationRandomDegrees = 360.0f;
        float angularSpeedMin = 0.0f;
        float angularSpeedMax = 6.28f;
    } debris;
};

struct ImpactClientBloodFeedbackConfig {
    bool enabled = true;
    int minCount = 2;
    int maxCount = 5;
    float damageAtMax = 100.0f;
    float forceAtMax = 20.0f;
    float forwardOffsetMin = 0.45f;
    float forwardOffsetMax = 0.85f;
    float rightOffset = 0.55f;
    float upOffset = 0.45f;
    float sizeMin = 0.08f;
    float sizeMax = 0.18f;
    float lifetimeMin = 0.18f;
    float lifetimeMax = 0.45f;
    float alphaMin = 0.35f;
    float alphaMax = 0.75f;
    glm::vec3 color{0.8f, 0.01f, 0.02f};
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
    float renderDistance = 60.0f;
    float renderFadeStartDistance = 40.0f;
    float renderFadeEndDistance = 60.0f;
    int maxCount = 256;
    // Flat textured decal rendering. The texture is sampled through an
    // alpha-blended surface-aligned quad (2 triangles) instead of a solid
    // cylinder, so decal cost no longer scales with triangle count.
    std::string texture;
    // Radius multiplier applied to the sampled texture's opaque footprint.
    float textureScale = 1.0f;
    // In-plane random rotation range in degrees (variety for repeated splats).
    float randomRotationDegrees = 360.0f;
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
    ImpactDecalSprayConfig spray;
    ImpactClientBloodFeedbackConfig clientFeedback;
    ImpactForceConfig force;
};

struct GoreChunkConfig {
    bool enabled = true;
    int countMin = 1;
    int countMax = 7;
    float sizeMin = 0.035f;
    float sizeMax = 0.14f;
    float speedMin = 2.0f;
    float speedMax = 9.0f;
    float coneDegreesMin = 10.0f;
    float coneDegreesMax = 38.0f;
    float gravity = 22.0f;
    float drag = 0.4f;
    float lifetimeMin = 0.6f;
    float lifetimeMax = 1.6f;
    glm::vec3 color{0.62f, 0.035f, 0.035f};
    std::string texture;
};

struct GoreMistConfig {
    bool enabled = true;
    int countMin = 6;
    int countMax = 26;
    float sizeMin = 0.15f;
    float sizeMax = 0.7f;
    float endSizeScale = 2.4f;
    float speedMin = 1.0f;
    float speedMax = 4.5f;
    float coneDegreesMin = 18.0f;
    float coneDegreesMax = 50.0f;
    float gravity = 2.0f;
    float drag = 2.0f;
    float lifetimeMin = 0.28f;
    float lifetimeMax = 0.75f;
    float alphaMin = 0.12f;
    float alphaMax = 0.42f;
    std::string texture;
};

struct GoreConfig {
    bool enabled = true;
    GoreChunkConfig fleshChunks;
    GoreChunkConfig boneShards;
    GoreMistConfig mist;
    float destroyedChunkMultiplier = 3.0f;
    float destroyedBoneMultiplier = 2.5f;
    float destroyedMistMultiplier = 4.0f;
    float destroyedDecalMultiplier = 2.0f;
    float destroyedForceMultiplier = 1.5f;
    bool exitConeEnabled = true;
    float headDepthM = 0.28f;
    float torsoDepthM = 0.18f;
    float limbDepthM = 0.10f;
    float headConeDegrees = 10.0f;
    float torsoConeDegrees = 18.0f;
    float limbConeDegrees = 26.0f;
    float exitMistCountScale = 1.8f;
};

struct ImpactDecalsData {
    bool enabled = true;
    ImpactDecalGroupConfig blood;
    ImpactDecalGroupConfig bulletHoles;
    ImpactDecalGroupConfig worldCracks;
    ImpactDecalGroupConfig explosionMarks;
    GoreConfig gore;
};

class ImpactDecalsConfig {
public:
    static ImpactDecalsConfig& instance();

    bool load(const std::string& path = "config/impact_decals.json");
    bool pollReload();
    void setRuntimeBloodEnabled(bool enabled);
    void clearRuntimeBloodOverride();
    // Victim-screen blood particles only. Independent of blood.enabled, which
    // remains the all-blood switch.
    void setRuntimeClientFeedbackEnabled(bool enabled);
    void clearRuntimeClientFeedbackOverride();

    const ImpactDecalsData& data() const { return mData; }

private:
    ImpactDecalsConfig() = default;

    ImpactDecalsData mData;
    std::string mPath = "config/impact_decals.json";
    std::filesystem::file_time_type mLastWrite{};
    bool mRuntimeBloodOverride = false;
    bool mRuntimeBloodEnabled = true;
    bool mRuntimeClientFeedbackOverride = false;
    bool mRuntimeClientFeedbackEnabled = true;
};
