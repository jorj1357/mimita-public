#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <glm/glm.hpp>

class Camera;

extern bool gBloodFXEnabled;
extern bool gHitFxTraceEnabled;
extern bool gDashFXEnabled;
inline bool isBloodFXEnabled() { return gBloodFXEnabled; }
inline bool isHitFxTraceEnabled() { return gHitFxTraceEnabled; }
inline bool isDashFXEnabled() { return gDashFXEnabled; }

struct HitFxKeyframe {
    std::string name;
    int startTick = 0;
    int endTick = 1;
    float startRadius = 0.12f;
    float endRadius = 0.18f;
    glm::vec3 colorStart{1.0f};
    glm::vec3 colorEnd{1.0f};
    float alphaStart = 1.0f;
    float alphaEnd = 1.0f;
    float brightnessStart = 3.0f;
    float brightnessEnd = 2.5f;
};

struct HitFxParticleConfig {
    bool enabled = true;
    std::string texturePath;
    glm::vec3 tintColor{1.0f};
    float alpha = 0.85f;
    float brightness = 2.0f;
    int count = 18;
    int lifetimeTicks = 24;
    float coneAngleDegrees = 35.0f;
    float speed = 4.0f;
    float speedRandomness = 1.5f;
    float sizeStart = 0.04f;
    float sizeEnd = 0.12f;
    float drag = 0.05f;
    float gravity = 0.0f;
    std::string spawnDirection = "opposite_hit_direction";
};

struct HitFxDirectionalShape {
    bool enabled = true;
    bool alignToHitDirection = true;
    int startTick = 0;
    int endTick = 12;
    float lengthStart = 0.15f;
    float lengthEnd = 1.0f;
    float radiusStart = 0.08f;
    float radiusEnd = 0.25f;
    glm::vec3 colorStart{1.0f};
    glm::vec3 colorEnd{0.1f, 0.35f, 1.0f};
    float alphaStart = 0.9f;
    float alphaEnd = 0.0f;
    float brightnessStart = 2.5f;
    float brightnessEnd = 0.0f;
};

struct HitFxImpactDisc {
    bool enabled = true;
    bool normalFacesHitDirection = true;
    int startTick = 0;
    int endTick = 10;
    float radiusStart = 0.1f;
    float radiusEnd = 0.9f;
    float thickness = 0.025f;
    glm::vec3 colorStart{1.0f};
    glm::vec3 colorEnd{0.0f, 0.2f, 1.0f};
    float alphaStart = 1.0f;
    float alphaEnd = 0.0f;
    float brightnessStart = 3.0f;
    float brightnessEnd = 0.0f;
};

struct HitFxCurves {
    std::string radiusCurve = "ease_out";
    std::string alphaCurve = "ease_out";
    std::string brightnessCurve = "linear";
    std::string particleSizeCurve = "ease_out";
};

struct LegacyContactSphereConfig {
    bool enabled = false;
    glm::vec3 color{1.0f, 0.15f, 0.1f};
    float alpha = 1.0f;
    float lifetimeSeconds = 0.25f;
    float startRadius = 0.18f;
    float endRadius = 0.27f;
};

struct MovementDashBurstConfig {
    bool enabled = true;
    int lifetimeTicks = 8;
    float lengthStart = 0.5f;
    float lengthEnd = 2.5f;
    float radiusStart = 0.18f;
    float radiusEnd = 0.35f;
    glm::vec3 colorStart{1.0f, 1.0f, 1.0f};
    glm::vec3 colorEnd{0.75f, 0.9f, 1.0f};
    float alphaStart = 0.85f;
    float alphaEnd = 0.0f;
    float brightnessStart = 2.5f;
    float brightnessEnd = 0.0f;
    bool speedScaling = true;
    float speedThreshold = 12.0f;
    float speedScaleMin = 1.0f;
    float speedScaleMax = 2.5f;
};

struct HitFxConfig {
    bool enabled = true;
    bool hotReload = true;

    struct Core {
        int lifetimeTicks = 60;
        bool spawnAtHitLocation = true;
        bool directional = true;
        bool useBlood = false;
        bool damageNumbers = true;
        bool entityImpact = true;
        bool worldImpact = true;
        bool bulletImpact = true;
    } core;

    std::vector<HitFxKeyframe> sphereTimeline;
    HitFxParticleConfig particles;
    HitFxDirectionalShape elongatedSphere;
    HitFxImpactDisc impactDisc;
    HitFxCurves curves;
    LegacyContactSphereConfig legacyContactSphere;
    MovementDashBurstConfig movementDashBurst;
};

struct HitEvent {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 direction;
    bool hitEntity = false;
    bool hitWorld = false;
    int damage = 0;
    std::string attacker;
    std::string victim;
    std::string weaponSource = "unknown";
    float knockbackForce = 0.0f;
};

struct HitBurstEffect {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 0.0f, 1.0f};
    glm::vec3 direction{0.0f, 0.0f, 1.0f};
    int spawnTick = 0;
    int totalTicks = 60;
    bool alive = true;
    bool dashBurst = false;
    float dashSpeed = 0.0f;
};

struct HitBurstSnapshot {
    glm::vec3 position;
    int ageTicks;
    int totalTicks;
    bool alive;
};

namespace HitEffects {

void onHit(const HitEvent& event);

// legacy -- still called internally by onHit, but weapons should use onHit
void spawnHitEffects(glm::vec3 hitPoint, const glm::vec3& hitDirection,
                     const glm::vec3& hitNormal, int damage,
                     const std::string& sourceId = "",
                     const std::string& targetId = "");

void spawnMovementDashBurst(const glm::vec3& position, const glm::vec3& direction, float speed = 0.0f);

void updateHitBursts(float dt);
void renderHitBursts(const Camera& camera);
void clearHitBursts();
int activeBurstCount();
int debugBurstCount();
int collectBurstSnapshots(HitBurstSnapshot* out, int maxCount);

void loadConfig(const std::string& path);
void pollReload();
const HitFxConfig& config();

} // namespace HitEffects
