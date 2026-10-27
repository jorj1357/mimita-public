#include "effects/hit-effects.h"

#include <cstdlib>
#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>

#include "effects/effect-part.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include <nlohmann/json.hpp>
#include "replay/replay.h"
#include "replay/replay.h"

constexpr int MAX_BURSTS = 64;
extern HitBurstEffect gBursts[MAX_BURSTS];
extern int gBurstCount;
extern int gGlobalTick;
extern HitFxConfig gConfig;
extern bool gDashFXEnabled;
extern bool gHitFxTraceEnabled;

void HitEffects::spawnHitParticles(const HitBurstEffect& b)
{
    const auto& cfg = gConfig.particles;
    if (!cfg.enabled) return;

    float coneRad = glm::radians(cfg.coneAngleDegrees);
    glm::vec3 baseDir = b.direction;
    if (cfg.spawnDirection == "opposite_hit_direction")
        baseDir = -b.direction;
    if (glm::length(baseDir) < 0.001f) baseDir = glm::vec3(0.0f, 0.0f, 1.0f);
    baseDir = glm::normalize(baseDir);

    for (int i = 0; i < cfg.count; ++i) {
        float angle = (float)std::rand() / (float)RAND_MAX * 6.2831853f;
        float spread = (float)std::rand() / (float)RAND_MAX * coneRad;
        float speed = cfg.speed + ((float)std::rand() / (float)RAND_MAX - 0.5f) * 2.0f * cfg.speedRandomness;
        speed = std::max(0.0f, speed);

        glm::vec3 up(0, 0, 1);
        if (std::abs(glm::dot(baseDir, up)) > 0.99f) up = glm::vec3(1, 0, 0);
        glm::vec3 right = glm::normalize(glm::cross(baseDir, up));
        glm::vec3 localUp = glm::cross(right, baseDir);
        glm::vec3 vel = glm::normalize(baseDir + (std::cos(angle) * right + std::sin(angle) * localUp) * std::tan(spread)) * speed;

        float startSz = cfg.sizeStart;
        float endSz = cfg.sizeEnd;

        EffectPart e;
        e.position = b.position;
        e.velocity = vel;
        e.color = glm::vec4(cfg.tintColor.x * cfg.brightness, cfg.tintColor.y * cfg.brightness, cfg.tintColor.z * cfg.brightness, cfg.alpha);
        e.texturePath = cfg.texturePath;
        e.maxLifetime = (float)cfg.lifetimeTicks / 60.0f;
        e.scale = startSz;
        e.endScale = endSz;
        e.gravity = cfg.gravity;
        e.drag = cfg.drag;
        e.affectedByGravity = cfg.gravity > 0.0f;
        e.replayType = "hitfx_particle";
        EffectPartSystem::instance().spawn(e);
    }
}

void HitEffects::spawnMeleeContact(const glm::vec3& position, const glm::vec3& direction,
                                   float force01, float randomSeed)
{
    const auto& cfg = gConfig.meleeContact;
    if (!gConfig.enabled || !cfg.enabled) return;

    static int lastContactFxLogTick = -1000;
    if (gGlobalTick - lastContactFxLogTick >= 30) {
        lastContactFxLogTick = gGlobalTick;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::General, StructuredLevel::Important,
            "melee.contact.fx", "melee-contact",
            "spawned client melee white sphere and spark stages",
            (uint32_t)gGlobalTick,
            nlohmann::json{{"force", force01},
                           {"position", {position.x, position.y, position.z}},
                           {"sparks", cfg.sparks.size()}},
            __FILE__, __LINE__, __FUNCTION__);
    }

    const float force = glm::clamp(force01, 0.0f, 1.0f);
    const float diameter = cfg.baseDiameter + (cfg.maxDiameter - cfg.baseDiameter) * force;
    const float alpha = cfg.baseAlpha + (cfg.maxAlpha - cfg.baseAlpha) * force;
    const glm::vec3 color = glm::mix(cfg.baseColor, cfg.maxColor, force);

    EffectPart sphere;
    sphere.position = position;
    sphere.replayType = "melee_contact_sphere";
    sphere.color = color;
    sphere.alpha = alpha;
    sphere.scale = diameter * 0.5f;
    sphere.maxLifetime = std::max(1, cfg.whiteSphereTicks) / 60.0f;
    sphere.lifetime = 0.0f;
    sphere.billboardText = false;
    EffectPartSystem::instance().spawn(sphere);

    for (const auto& stage : cfg.sparks) {
        if (stage.texturePath.empty()) continue;
        EffectPart spark;
        spark.position = position;
        spark.normal = glm::length(direction) > 0.001f
            ? glm::normalize(direction) : glm::vec3(0.0f, 1.0f, 0.0f);
        spark.replayType = "hitfx_particle";
        spark.texturePath = stage.texturePath;
        spark.color = glm::vec3(1.0f);
        spark.alpha = stage.alpha;
        spark.scale = stage.size;
        spark.endScale = stage.size;
        spark.billboardText = true;
        spark.maxLifetime = std::max(1, stage.endTick - stage.startTick) / 60.0f;
        // Negative lifetime delays the stage until its contact tick.
        spark.lifetime = -(float)std::max(1, stage.startTick) / 60.0f;
        const float r = std::sin(randomSeed * 12.9898f + stage.startTick * 78.233f) * 43758.5453f;
        const float unit = r - std::floor(r);
        spark.rotation.z = glm::radians(unit * stage.randomRotationDegrees);
        spark.angularVelocity = glm::vec3(0.0f, 0.0f, 0.0f);
        EffectPartSystem::instance().spawn(spark);
    }
}

void HitEffects::spawnMovementDashBurst(const glm::vec3& position, const glm::vec3& direction, float speed)
{
    if (!gConfig.enabled || !gConfig.movementDashBurst.enabled || !gDashFXEnabled) return;
    if (gBurstCount >= MAX_BURSTS) {
        gBursts[0] = gBursts[gBurstCount - 1];
        gBurstCount--;
    }

    const auto& cfg = gConfig.movementDashBurst;
    glm::vec3 dir = glm::length(direction) > 0.001f ? glm::normalize(direction) : glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 fwd = dir;
    glm::vec3 up(0.0f, 0.0f, 1.0f);
    glm::vec3 right = glm::normalize(glm::cross(fwd, up));
    up = glm::normalize(glm::cross(right, fwd));

    glm::vec3 localOffset = fwd * cfg.forwardOffset + right * cfg.rightOffset + up * cfg.upOffset;
    glm::vec3 spawnPos = position + localOffset + cfg.offset;
    spawnPos.z += 0.05f;

    Debug::log(Debug::Category::General, "[DASH FX] position=(%.2f,%.2f,%.2f) direction=(%.2f,%.2f,%.2f) offset=(%.2f,%.2f,%.2f) spawn=(%.2f,%.2f,%.2f)",
               position.x, position.y, position.z,
               dir.x, dir.y, dir.z,
               localOffset.x, localOffset.y, localOffset.z,
               spawnPos.x, spawnPos.y, spawnPos.z);

    HitBurstEffect& b = gBursts[gBurstCount++];
    b.position = spawnPos;
    b.direction = dir;
    b.normal = glm::vec3(0.0f, 0.0f, 1.0f);
    b.spawnTick = gGlobalTick;
    b.totalTicks = cfg.lifetimeTicks;
    b.alive = true;
    b.dashBurst = true;
    b.dashSpeed = speed;
    b.burstType = BurstType::Dash;

    {
        ReplayEffectEvent ev;
        ev.type = "hit_burst";
        ev.position = spawnPos;
        ev.direction = dir;
        ev.normal = b.normal;
        ev.sourceActorId = "hitfx";
        ev.targetActorId = "burst";
        ev.lifetime = (float)b.totalTicks / 60.0f;
        ev.burstType = (int)BurstType::Dash;
        ev.dashSpeed = speed;
        ev.dashBurst = true;
        captureReplayEffect(ev);
    }
}

void HitEffects::spawnGroundJumpBurst(const glm::vec3& position, const glm::vec3& direction)
{
    if (!gConfig.enabled || !gConfig.groundJumpBurst.enabled || !gDashFXEnabled) return;
    if (gBurstCount >= MAX_BURSTS) {
        gBursts[0] = gBursts[gBurstCount - 1];
        gBurstCount--;
    }
    const auto& cfg = gConfig.groundJumpBurst;
    glm::vec3 dir = glm::length(direction) > 0.001f ? glm::normalize(direction) : glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 up(0.0f, 0.0f, 1.0f);
    glm::vec3 right = glm::normalize(glm::cross(dir, up));
    if (glm::length(right) < 0.001f) right = glm::vec3(1.0f, 0.0f, 0.0f);
    up = glm::normalize(glm::cross(right, dir));
    glm::vec3 localOffset = dir * cfg.forwardOffset + right * cfg.rightOffset + up * cfg.upOffset;
    glm::vec3 spawnPos = position + localOffset + cfg.offset;
    HitBurstEffect& b = gBursts[gBurstCount++];
    b.position = spawnPos;
    b.direction = dir;
    b.normal = glm::vec3(0.0f, 0.0f, 1.0f);
    b.spawnTick = gGlobalTick;
    b.totalTicks = cfg.lifetimeTicks;
    b.alive = true;
    b.dashBurst = false;
    b.dashSpeed = 0.0f;
    b.burstType = BurstType::GroundJump;

    {
        ReplayEffectEvent ev;
        ev.type = "hit_burst";
        ev.position = spawnPos;
        ev.direction = dir;
        ev.normal = b.normal;
        ev.sourceActorId = "hitfx";
        ev.targetActorId = "burst";
        ev.lifetime = (float)b.totalTicks / 60.0f;
        ev.burstType = (int)BurstType::GroundJump;
        captureReplayEffect(ev);
    }
}

void HitEffects::spawnAirJumpBurst(const glm::vec3& position, const glm::vec3& direction)
{
    if (!gConfig.enabled || !gConfig.airJumpBurst.enabled || !gDashFXEnabled) return;
    if (gBurstCount >= MAX_BURSTS) {
        gBursts[0] = gBursts[gBurstCount - 1];
        gBurstCount--;
    }
    const auto& cfg = gConfig.airJumpBurst;
    glm::vec3 dir = glm::length(direction) > 0.001f ? glm::normalize(direction) : glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 up(0.0f, 0.0f, 1.0f);
    glm::vec3 right = glm::normalize(glm::cross(dir, up));
    if (glm::length(right) < 0.001f) right = glm::vec3(1.0f, 0.0f, 0.0f);
    up = glm::normalize(glm::cross(right, dir));
    glm::vec3 localOffset = dir * cfg.forwardOffset + right * cfg.rightOffset + up * cfg.upOffset;
    glm::vec3 spawnPos = position + localOffset + cfg.offset;
    HitBurstEffect& b = gBursts[gBurstCount++];
    b.position = spawnPos;
    b.direction = dir;
    b.normal = glm::vec3(0.0f, 0.0f, 1.0f);
    b.spawnTick = gGlobalTick;
    b.totalTicks = cfg.lifetimeTicks;
    b.alive = true;
    b.dashBurst = false;
    b.dashSpeed = 0.0f;
    b.burstType = BurstType::AirJump;

    {
        ReplayEffectEvent ev;
        ev.type = "hit_burst";
        ev.position = spawnPos;
        ev.direction = dir;
        ev.normal = b.normal;
        ev.sourceActorId = "hitfx";
        ev.targetActorId = "burst";
        ev.lifetime = (float)b.totalTicks / 60.0f;
        ev.burstType = (int)BurstType::AirJump;
        captureReplayEffect(ev);
    }
}

void HitEffects::spawnWalkBurst(const glm::vec3& position, const glm::vec3& direction, float speed)
{
    if (!gConfig.enabled || !gConfig.walkBurst.enabled || !gDashFXEnabled) return;
    if (gBurstCount >= MAX_BURSTS) {
        gBursts[0] = gBursts[gBurstCount - 1];
        gBurstCount--;
    }

    const auto& cfg = gConfig.walkBurst;
    glm::vec3 dir = glm::length(direction) > 0.001f ? glm::normalize(direction) : glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 up(0.0f, 0.0f, 1.0f);
    glm::vec3 right = glm::normalize(glm::cross(dir, up));
    if (glm::length(right) < 0.001f) right = glm::vec3(0.0f, 1.0f, 0.0f);
    up = glm::normalize(glm::cross(right, dir));

    glm::vec3 localOffset = dir * cfg.forwardOffset + right * cfg.rightOffset + up * cfg.upOffset;
    glm::vec3 spawnPos = position + localOffset + cfg.offset;

    HitBurstEffect& b = gBursts[gBurstCount++];
    b.position = spawnPos;
    b.direction = dir;
    b.normal = up;
    b.spawnTick = gGlobalTick;
    b.totalTicks = cfg.lifetimeTicks;
    b.alive = true;
    b.dashBurst = false;
    b.dashSpeed = speed;
    b.burstType = BurstType::Walk;

    {
        ReplayEffectEvent ev;
        ev.type = "hit_burst";
        ev.position = spawnPos;
        ev.direction = dir;
        ev.normal = up;
        ev.sourceActorId = "hitfx";
        ev.targetActorId = "burst";
        ev.lifetime = (float)b.totalTicks / 60.0f;
        ev.burstType = (int)BurstType::Walk;
        ev.dashSpeed = speed;
        captureReplayEffect(ev);
    }
}

void HitEffects::spawnHealthGainedEffect(const glm::vec3& position)
{
    if (!gConfig.enabled || !gConfig.healthGained.enabled) return;
    if (gBurstCount >= MAX_BURSTS) {
        gBursts[0] = gBursts[gBurstCount - 1];
        gBurstCount--;
    }
    const auto& cfg = gConfig.healthGained;
    HitBurstEffect& b = gBursts[gBurstCount++];
    b.position = position;
    b.direction = glm::vec3(0.0f, 0.0f, 1.0f);
    b.normal = glm::vec3(0.0f, 0.0f, 1.0f);
    b.spawnTick = gGlobalTick;
    b.totalTicks = cfg.lifetimeTicks;
    b.alive = true;
    b.dashBurst = false;
    b.dashSpeed = 0.0f;
    b.burstType = BurstType::HealthGained;

    {
        ReplayEffectEvent ev;
        ev.type = "hit_burst";
        ev.position = position;
        ev.direction = b.direction;
        ev.normal = b.normal;
        ev.sourceActorId = "hitfx";
        ev.targetActorId = "burst";
        ev.lifetime = (float)b.totalTicks / 60.0f;
        ev.burstType = (int)BurstType::HealthGained;
        captureReplayEffect(ev);
    }
}

void HitEffects::spawnLandingBurst(const glm::vec3& position, const glm::vec3& direction, float speed)
{
    if (!gConfig.enabled || !gConfig.landingBurst.enabled || !gDashFXEnabled) return;
    if (gBurstCount >= MAX_BURSTS) {
        gBursts[0] = gBursts[gBurstCount - 1];
        gBurstCount--;
    }

    const auto& cfg = gConfig.landingBurst;
    glm::vec3 dir = glm::length(direction) > 0.001f ? glm::normalize(direction) : glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 up(0.0f, 0.0f, 1.0f);
    glm::vec3 right = glm::normalize(glm::cross(dir, up));
    if (glm::length(right) < 0.001f) right = glm::vec3(0.0f, 1.0f, 0.0f);
    up = glm::normalize(glm::cross(right, dir));

    glm::vec3 localOffset = dir * cfg.forwardOffset + right * cfg.rightOffset + up * cfg.upOffset;
    glm::vec3 spawnPos = position + localOffset + cfg.offset;

    HitBurstEffect& b = gBursts[gBurstCount++];
    b.position = spawnPos;
    b.direction = dir;
    b.normal = up;
    b.spawnTick = gGlobalTick;
    b.totalTicks = cfg.lifetimeTicks;
    b.alive = true;
    b.dashBurst = false;
    b.dashSpeed = speed;
    b.burstType = BurstType::Landing;

    {
        ReplayEffectEvent ev;
        ev.type = "hit_burst";
        ev.position = spawnPos;
        ev.direction = dir;
        ev.normal = up;
        ev.sourceActorId = "hitfx";
        ev.targetActorId = "burst";
        ev.lifetime = (float)b.totalTicks / 60.0f;
        ev.burstType = (int)BurstType::Landing;
        ev.dashSpeed = speed;
        captureReplayEffect(ev);
    }
}
