#pragma once

#include <cstdint>
#include <string>
#include <glm/glm.hpp>

class Camera;

extern bool gBloodFXEnabled;
inline bool isBloodFXEnabled() { return gBloodFXEnabled; }

struct StageConfig {
    bool enabled = true;
    int ticks = 2;
    float alpha = 1.0f;
    float brightness = 1.0f;
    glm::vec3 color{1.0f};
    std::string sizeCurve = "linear";
    std::string alphaCurve = "linear";
    std::string brightnessCurve = "linear";
};

struct WhiteImpactStarConfig : StageConfig {
    int spikeCount = 12;
    float innerRadius = 0.08f;
    float outerRadius = 0.35f;
    float rotationSpeed = 0.0f;
    bool randomRotation = true;
};

struct BrightBlueBurstConfig : StageConfig {
    float radius = 0.4f;
};

struct DarkBlueFadeConfig : StageConfig {
    float radius = 0.8f;
};

struct ImpactConeConfig {
    bool enabled = true;
    int particleCount = 20;
    float coneAngleDegrees = 35.0f;
    float initialSpeed = 3.0f;
    float speedVariation = 1.0f;
    float gravity = 0.0f;
    float drag = 0.0f;
    int ticks = 10;
    float startSize = 0.03f;
    float endSize = 0.12f;
    float brightness = 2.0f;
    float alpha = 1.0f;
    glm::vec3 color{1.0f};
};

struct ElongatedSphereConfig {
    bool enabled = true;
    int ticks = 6;
    float startLength = 0.2f;
    float endLength = 1.0f;
    float startRadius = 0.1f;
    float endRadius = 0.3f;
    float alphaStart = 1.0f;
    float alphaEnd = 0.0f;
    float brightness = 2.0f;
    glm::vec3 color{1.0f};
    std::string sizeCurve = "ease_out";
    std::string alphaCurve = "ease_out";
    std::string brightnessCurve = "ease_out";
};

struct PerpendicularDiscConfig {
    bool enabled = true;
    int ticks = 8;
    float startRadius = 0.1f;
    float endRadius = 1.2f;
    float alphaStart = 1.0f;
    float alphaEnd = 0.0f;
    float brightness = 2.5f;
    glm::vec3 color{1.0f};
    std::string sizeCurve = "ease_out";
    std::string alphaCurve = "ease_out";
    std::string brightnessCurve = "ease_out";
};

struct HitFxConfigData {
    WhiteImpactStarConfig whiteStar;
    BrightBlueBurstConfig blueBurst;
    DarkBlueFadeConfig blueFade;
    ImpactConeConfig cone;
    ElongatedSphereConfig oval;
    PerpendicularDiscConfig disc;
};

struct HitBurstEffect {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 0.0f, 1.0f};
    int spawnTick = 0;
    int totalTicks = 24;
    bool alive = true;
};

namespace HitEffects {

void spawnHitEffects(glm::vec3 hitPoint, const glm::vec3& hitNormal,
                     int damage, const std::string& sourceId = "",
                     const std::string& targetId = "");

void updateHitBursts(float dt);
void renderHitBursts(const Camera& camera);
void clearHitBursts();
int activeBurstCount();
int debugBurstCount();

void loadConfig(const std::string& path);
void pollReload();
const HitFxConfigData& config();

} // namespace HitEffects
