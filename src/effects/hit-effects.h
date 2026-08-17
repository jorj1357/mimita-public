#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <glm/glm.hpp>

class Camera;

extern bool gBloodFXEnabled;
extern bool gHitFxTraceEnabled;
extern bool gDashFXEnabled;
struct DeathEllipsoidConfig {
    bool enabled = true;
    float lifetime = 3.0f;
    float length = 8.0f;
    float radius = 1.5f;
    float baseAlpha = 0.35f;
    glm::vec4 color{1.0f, 0.0f, 0.0f, 0.75f};
    bool fade = true;
};
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

struct DamageNumberConfig {
    bool enabled = true;
    // Depth-test mode for damage-number text:
    //   "always_on_top" -> depth test OFF, depth write OFF (floats over everything)
    //   "occluded"      -> depth test ON,  depth write OFF (hidden by walls/bodies)
    //   "normal"        -> depth test ON,  depth write ON
    std::string depthMode = "always_on_top";
    // Legacy alias for depthMode == "occluded" (kept for backward compat).
    bool occluded = false;
    float fontSize = 0.96f;
    float lifetime = 1.0f;
    float startOpacity = 1.0f;
    float endOpacity = 0.0f;
    float fadeStart = 0.0f;
    float fadeEnd = 1.0f;
    float worldOffsetX = 0.0f;
    float worldOffsetY = 0.0f;
    float worldOffsetZ = 0.25f;
    float screenOffsetX = 0.0f;
    float screenOffsetY = 0.0f;
    float moveX = 0.0f;
    float moveY = 0.0f;
    float moveZ = 0.8f;
    float moveSpeed = 1.0f;
    float startScale = 1.0f;
    float endScale = 1.0f;
    float randomHorizontalSpread = 0.0f;
    float randomVerticalSpread = 0.0f;
    float spawnJitter = 0.0f;
    float spawnDelay = 0.0f;
    glm::vec3 textColor{1.0f, 0.15f, 0.15f};
    glm::vec3 criticalColor{1.0f, 0.9f, 0.1f};
    glm::vec3 healingColor{0.15f, 1.0f, 0.15f};
    bool outlineEnabled = false;
    float outlineThickness = 1.0f;
    glm::vec3 outlineColor{0.0f, 0.0f, 0.0f};
    bool shadowEnabled = false;
    glm::vec2 shadowOffset{1.0f, 1.0f};
    bool bold = false;
    bool italic = false;
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
    // Directional spawn offset: the burst spawns at position + offset.
    // forwardOffset: along movement direction (negative = behind player)
    // rightOffset: perpendicular to movement (positive = right side)
    // upOffset: vertical offset
    float forwardOffset = -1.0f;
    float rightOffset = 0.0f;
    float upOffset = 0.0f;
    // World-space direct offset added after local-space offset.
    glm::vec3 offset{0.0f};
    // Non-uniform per-axis scale multiplier (applied on top of the stretch).
    glm::vec3 scale{1.0f};
    // Euler rotation (degrees) applied to the stretch axis.
    glm::vec3 rotation{0.0f};
    // Stretch axis for the elongated sphere.
    // Supported: "forward", "right", "up", "world_x", "world_y", "world_z"
    // "forward" = direction of movement (default, for trail effects)
    // "up" = vertical (for landing shockwave flattening)
    // "right" = horizontal perpendicular (for sideways spread)
    std::string stretchAxis = "forward";
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
        // When the local player is hit by a server NPC/player, show damage
        // numbers + body hit effects + elongated sphere at the real hit point.
        bool victimHitEffects = true;
    } core;

    struct DamageImpactSphereConfig {
        bool enabled = true;
        float length = 2.67f;
        float radius = 0.5f;
        glm::vec3 color{1.0f, 0.3f, 0.3f};
        float alpha = 0.5f;
        float lifetime = 0.5f;
    } damageImpactSphere;

    struct EntityImpactConfig {
        bool enabled = true;
        glm::vec3 color{0.9f, 0.02f, 0.02f};
        float lifetime = 0.18f;
        float startRadius = 0.12f;
        float endRadius = 0.4f;
    } entityImpact;

    struct WorldImpactConfig {
        bool enabled = true;
        glm::vec3 color{0.55f, 0.55f, 0.55f};
        float lifetime = 0.5f;
        float startRadius = 0.1f;
        float endRadius = 5.0f;
        float alpha = 0.5f;
    } worldImpact;

    struct WorldDebrisConfig {
        bool enabled = true;
        int count = 12;
        glm::vec3 color{0.42f, 0.40f, 0.38f};
        float surfaceOffset = 0.12f;
        float lifetimeBase = 0.5f;
        float lifetimeForce = 0.15f;
        float startScale = 0.8f;
        float scaleForce = 1.0f;
        float endScale = 4.0f;
        float endScaleForce = 12.0f;
        float speed = 1.0f;
        float gravity = 15.0f;
    } worldDebris;

    struct ImpactTickConfig {
        glm::vec3 color{0.1f, 0.5f, 1.0f};
        float radius = 0.15f;
        float lifetime = 0.016f;
    } impactTick;

    struct FootstepConfig {
        glm::vec3 color{1.0f, 1.0f, 1.0f};
        float lifetime = 0.5f;
        float startRadius = 0.18f;
        float endRadius = 0.06f;
    } footstep;

    struct DashConfig {
        glm::vec3 color{0.2f, 0.6f, 1.0f};
        float lifetime = 0.8f;
        float startRadius = 0.35f;
        float endRadius = 0.1f;
    } dash;

    struct PerfectDashConfig {
        glm::vec3 color{1.0f, 0.8f, 0.1f};
        float lifetime = 1.2f;
        float startRadius = 0.7f;
        float endRadius = 0.05f;
    } perfectDash;

    struct FreezeConfig {
        glm::vec3 color{0.2f, 1.0f, 0.3f};
        float lifetime = 0.1f;
        float scale = 0.2f;
    } freeze;

    struct FreezeTrailConfig {
        glm::vec3 color{0.1f, 0.1f, 0.4f};
        float lifetime = 0.05f;
        float length = 2.0f;
        float radius = 0.4f;
        float alpha = 0.4f;
    } freezeTrail;

    struct DownDashConfig {
        glm::vec3 color{0.1f, 0.8f, 0.8f};
        float lifetime = 0.2f;
        float length = 3.0f;
        float radius = 0.5f;
        float alpha = 0.35f;
    } downDash;

    struct HitmarkerVisualConfig {
        bool enabled = true;
        float duration = 0.5f;
        float size = 28.0f;
    } hitmarkerVisual;

    std::vector<HitFxKeyframe> sphereTimeline;
    HitFxParticleConfig particles;
    HitFxDirectionalShape elongatedSphere;
    HitFxImpactDisc impactDisc;
    HitFxCurves curves;
    LegacyContactSphereConfig legacyContactSphere;
    MovementDashBurstConfig movementDashBurst;
    MovementDashBurstConfig groundJumpBurst;
    MovementDashBurstConfig airJumpBurst;
    MovementDashBurstConfig walkBurst;
    MovementDashBurstConfig landingBurst;
    MovementDashBurstConfig healthGained;
    DeathEllipsoidConfig deathEllipsoid;
    DamageNumberConfig damageNumber;
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
    float hitDistance = -1.0f;
};

enum class BurstType {
    Movement,
    Dash,
    GroundJump,
    AirJump,
    Walk,
    Landing,
    HealthGained
};

struct HitBurstEffect {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 0.0f, 1.0f};
    glm::vec3 direction{0.0f, 0.0f, 1.0f};
    int spawnTick = 0;
    int totalTicks = 60;
    bool alive = true;
    bool dashBurst = false; // kept for backward compat
    float dashSpeed = 0.0f;
    BurstType burstType = BurstType::Movement;
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
                     const std::string& targetId = "",
                     bool spawnDamageNumber = true);

void spawnMovementDashBurst(const glm::vec3& position, const glm::vec3& direction, float speed = 0.0f);
void spawnGroundJumpBurst(const glm::vec3& position, const glm::vec3& direction);
void spawnAirJumpBurst(const glm::vec3& position, const glm::vec3& direction);
void spawnWalkBurst(const glm::vec3& position, const glm::vec3& direction, float speed = 0.0f);
void spawnLandingBurst(const glm::vec3& position, const glm::vec3& direction, float speed = 0.0f);
void spawnHealthGainedEffect(const glm::vec3& position);

void updateHitBursts(float dt);
void renderHitBursts(const Camera& camera);
void clearHitBursts();
int activeBurstCount();
int debugBurstCount();
int collectBurstSnapshots(HitBurstSnapshot* out, int maxCount);

    bool loadConfig(const std::string& path);
    void pollReload();
    const HitFxConfig& config();
    HitFxConfig& mutableConfig();

} // namespace HitEffects
