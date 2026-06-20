#include "effects/hit-effects.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#include <glm/glm.hpp>
#include <glad/glad.h>
#include <nlohmann/json.hpp>

#include "effects/effect-part.h"
#include "debug/debug-visuals.h"
#include "debug/debug-log.h"
#include "gui/ui-system.h"
#include "camera.h"
#include "renderer/renderer.h"
#include "replay/replay.h"

using json = nlohmann::json;

static constexpr int MAX_BURSTS = 64;
static HitBurstEffect gBursts[MAX_BURSTS];
static int gBurstCount = 0;
static int gGlobalTick = 0;

static HitFxConfig gConfig;
static std::filesystem::file_time_type gConfigLastWrite;
static std::string gConfigPath = "config/hitfx.json";
static bool gDebugPanelEnabled = false;

bool gBloodFXEnabled = false;
bool gHitFxTraceEnabled = false;
bool gDashFXEnabled = true;


extern Renderer* gRenderer;

static float evalCurve(const std::string& curve, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    if (curve == "ease_in") return t * t;
    if (curve == "ease_out") return 1.0f - (1.0f - t) * (1.0f - t);
    if (curve == "ease_in_out") return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) * 0.5f;
    if (curve == "exponential") return t < 0.5f ? std::pow(2.0f, 10.0f * t - 10.0f) * 0.5f : (2.0f - std::pow(2.0f, -10.0f * t + 10.0f)) * 0.5f;
    if (curve == "linear") return t;
    return t;
}

static float lerp(float a, float b, float t) { return a + (b - a) * t; }

static glm::vec3 lerpVec3(const glm::vec3& a, const glm::vec3& b, float t)
{
    return glm::vec3(lerp(a.x, b.x, t), lerp(a.y, b.y, t), lerp(a.z, b.z, t));
}

static glm::vec3 readVec3Json(const json& j)
{
    if (j.is_array() && j.size() >= 3)
        return glm::vec3((float)j[0], (float)j[1], (float)j[2]);
    return glm::vec3(1.0f);
}

static void readKeyframe(const json& j, HitFxKeyframe& k)
{
    if (j.contains("name")) k.name = j["name"];
    if (j.contains("startTick")) k.startTick = j["startTick"];
    if (j.contains("endTick")) k.endTick = j["endTick"];
    if (j.contains("startRadius")) k.startRadius = j["startRadius"];
    if (j.contains("endRadius")) k.endRadius = j["endRadius"];
    if (j.contains("colorStart")) k.colorStart = readVec3Json(j["colorStart"]);
    if (j.contains("colorEnd")) k.colorEnd = readVec3Json(j["colorEnd"]);
    if (j.contains("alphaStart")) k.alphaStart = j["alphaStart"];
    if (j.contains("alphaEnd")) k.alphaEnd = j["alphaEnd"];
    if (j.contains("brightnessStart")) k.brightnessStart = j["brightnessStart"];
    if (j.contains("brightnessEnd")) k.brightnessEnd = j["brightnessEnd"];
}

void HitEffects::loadConfig(const std::string& path)
{
    gConfigPath = path;
    std::ifstream file(path);
    if (!file.is_open()) {
        Debug::log(Debug::Category::NpcCombat, "[HITFX] config not found: %s\n", path.c_str());
        return;
    }
    try {
        json j;
        file >> j;

        HitFxConfig cfg;

        // Set burst defaults before JSON override
        cfg.groundJumpBurst.lengthStart = 1.5f;
        cfg.groundJumpBurst.lengthEnd = 0.5f;
        cfg.groundJumpBurst.radiusStart = 0.25f;
        cfg.groundJumpBurst.radiusEnd = 0.15f;
        cfg.groundJumpBurst.colorStart = {1.0f, 0.85f, 0.2f};
        cfg.groundJumpBurst.colorEnd = {1.0f, 0.6f, 0.1f};
        cfg.groundJumpBurst.forwardOffset = 0.0f;
        cfg.groundJumpBurst.upOffset = 0.0f;

        cfg.airJumpBurst.lengthStart = 3.0f;
        cfg.airJumpBurst.lengthEnd = 6.0f;
        cfg.airJumpBurst.radiusStart = 0.4f;
        cfg.airJumpBurst.radiusEnd = 0.1f;
        cfg.airJumpBurst.colorStart = {0.5f, 0.3f, 1.0f};
        cfg.airJumpBurst.colorEnd = {0.2f, 0.1f, 0.8f};
        cfg.airJumpBurst.forwardOffset = 0.0f;
        cfg.airJumpBurst.upOffset = -1.5f; // below player for upward propulsion look

        cfg.movementDashBurst.forwardOffset = -0.8f; // behind player

        if (j.contains("enabled")) cfg.enabled = j["enabled"];
        if (j.contains("hotReload")) cfg.hotReload = j["hotReload"];

        if (j.contains("core")) {
            const json& c = j["core"];
            if (c.contains("lifetimeTicks")) cfg.core.lifetimeTicks = c["lifetimeTicks"];
            if (c.contains("spawnAtHitLocation")) cfg.core.spawnAtHitLocation = c["spawnAtHitLocation"];
            if (c.contains("directional")) cfg.core.directional = c["directional"];
            if (c.contains("useBlood")) cfg.core.useBlood = c["useBlood"];
            if (c.contains("damageNumbers")) cfg.core.damageNumbers = c["damageNumbers"];
            if (c.contains("entityImpact")) cfg.core.entityImpact = c["entityImpact"];
            if (c.contains("worldImpact")) cfg.core.worldImpact = c["worldImpact"];
            if (c.contains("bulletImpact")) cfg.core.bulletImpact = c["bulletImpact"];
        }

        if (j.contains("sphereTimeline") && j["sphereTimeline"].is_array()) {
            cfg.sphereTimeline.clear();
            for (const auto& item : j["sphereTimeline"]) {
                HitFxKeyframe kf;
                readKeyframe(item, kf);
                cfg.sphereTimeline.push_back(kf);
            }
        }

        if (j.contains("particles")) {
            const json& p = j["particles"];
            if (p.contains("enabled")) cfg.particles.enabled = p["enabled"];
            if (p.contains("texturePath")) cfg.particles.texturePath = p["texturePath"];
            if (p.contains("tintColor")) cfg.particles.tintColor = readVec3Json(p["tintColor"]);
            if (p.contains("alpha")) cfg.particles.alpha = p["alpha"];
            if (p.contains("brightness")) cfg.particles.brightness = p["brightness"];
            if (p.contains("count")) cfg.particles.count = p["count"];
            if (p.contains("lifetimeTicks")) cfg.particles.lifetimeTicks = p["lifetimeTicks"];
            if (p.contains("coneAngleDegrees")) cfg.particles.coneAngleDegrees = p["coneAngleDegrees"];
            if (p.contains("speed")) cfg.particles.speed = p["speed"];
            if (p.contains("speedRandomness")) cfg.particles.speedRandomness = p["speedRandomness"];
            if (p.contains("sizeStart")) cfg.particles.sizeStart = p["sizeStart"];
            if (p.contains("sizeEnd")) cfg.particles.sizeEnd = p["sizeEnd"];
            if (p.contains("drag")) cfg.particles.drag = p["drag"];
            if (p.contains("gravity")) cfg.particles.gravity = p["gravity"];
            if (p.contains("spawnDirection")) cfg.particles.spawnDirection = p["spawnDirection"];
        }

        if (j.contains("directionalShapes")) {
            const json& ds = j["directionalShapes"];
            if (ds.contains("elongatedSphere")) {
                const json& e = ds["elongatedSphere"];
                if (e.contains("enabled")) cfg.elongatedSphere.enabled = e["enabled"];
                if (e.contains("alignToHitDirection")) cfg.elongatedSphere.alignToHitDirection = e["alignToHitDirection"];
                if (e.contains("startTick")) cfg.elongatedSphere.startTick = e["startTick"];
                if (e.contains("endTick")) cfg.elongatedSphere.endTick = e["endTick"];
                if (e.contains("lengthStart")) cfg.elongatedSphere.lengthStart = e["lengthStart"];
                if (e.contains("lengthEnd")) cfg.elongatedSphere.lengthEnd = e["lengthEnd"];
                if (e.contains("radiusStart")) cfg.elongatedSphere.radiusStart = e["radiusStart"];
                if (e.contains("radiusEnd")) cfg.elongatedSphere.radiusEnd = e["radiusEnd"];
                if (e.contains("colorStart")) cfg.elongatedSphere.colorStart = readVec3Json(e["colorStart"]);
                if (e.contains("colorEnd")) cfg.elongatedSphere.colorEnd = readVec3Json(e["colorEnd"]);
                if (e.contains("alphaStart")) cfg.elongatedSphere.alphaStart = e["alphaStart"];
                if (e.contains("alphaEnd")) cfg.elongatedSphere.alphaEnd = e["alphaEnd"];
                if (e.contains("brightnessStart")) cfg.elongatedSphere.brightnessStart = e["brightnessStart"];
                if (e.contains("brightnessEnd")) cfg.elongatedSphere.brightnessEnd = e["brightnessEnd"];
            }
            if (ds.contains("impactDisc")) {
                const json& d = ds["impactDisc"];
                if (d.contains("enabled")) cfg.impactDisc.enabled = d["enabled"];
                if (d.contains("normalFacesHitDirection")) cfg.impactDisc.normalFacesHitDirection = d["normalFacesHitDirection"];
                if (d.contains("startTick")) cfg.impactDisc.startTick = d["startTick"];
                if (d.contains("endTick")) cfg.impactDisc.endTick = d["endTick"];
                if (d.contains("radiusStart")) cfg.impactDisc.radiusStart = d["radiusStart"];
                if (d.contains("radiusEnd")) cfg.impactDisc.radiusEnd = d["radiusEnd"];
                if (d.contains("thickness")) cfg.impactDisc.thickness = d["thickness"];
                if (d.contains("colorStart")) cfg.impactDisc.colorStart = readVec3Json(d["colorStart"]);
                if (d.contains("colorEnd")) cfg.impactDisc.colorEnd = readVec3Json(d["colorEnd"]);
                if (d.contains("alphaStart")) cfg.impactDisc.alphaStart = d["alphaStart"];
                if (d.contains("alphaEnd")) cfg.impactDisc.alphaEnd = d["alphaEnd"];
                if (d.contains("brightnessStart")) cfg.impactDisc.brightnessStart = d["brightnessStart"];
                if (d.contains("brightnessEnd")) cfg.impactDisc.brightnessEnd = d["brightnessEnd"];
            }
        }

        if (j.contains("curves")) {
            const json& cv = j["curves"];
            if (cv.contains("radiusCurve")) cfg.curves.radiusCurve = cv["radiusCurve"];
            if (cv.contains("alphaCurve")) cfg.curves.alphaCurve = cv["alphaCurve"];
            if (cv.contains("brightnessCurve")) cfg.curves.brightnessCurve = cv["brightnessCurve"];
            if (cv.contains("particleSizeCurve")) cfg.curves.particleSizeCurve = cv["particleSizeCurve"];
        }

        if (j.contains("legacyContactSphere")) {
            const json& l = j["legacyContactSphere"];
            if (l.contains("enabled")) cfg.legacyContactSphere.enabled = l["enabled"];
            if (l.contains("color")) cfg.legacyContactSphere.color = readVec3Json(l["color"]);
            if (l.contains("alpha")) cfg.legacyContactSphere.alpha = l["alpha"];
            if (l.contains("lifetimeSeconds")) cfg.legacyContactSphere.lifetimeSeconds = l["lifetimeSeconds"];
            if (l.contains("startRadius")) cfg.legacyContactSphere.startRadius = l["startRadius"];
            if (l.contains("endRadius")) cfg.legacyContactSphere.endRadius = l["endRadius"];
        }

        if (j.contains("deathEllipsoid")) {
            const json& d = j["deathEllipsoid"];
            if (d.contains("enabled")) cfg.deathEllipsoid.enabled = d["enabled"];
            if (d.contains("lifetime")) cfg.deathEllipsoid.lifetime = d["lifetime"];
            if (d.contains("length")) cfg.deathEllipsoid.length = d["length"];
            if (d.contains("radius")) cfg.deathEllipsoid.radius = d["radius"];
            if (d.contains("baseAlpha")) cfg.deathEllipsoid.baseAlpha = d["baseAlpha"];
            if (d.contains("color") && d["color"].is_array() && d["color"].size() >= 4)
                cfg.deathEllipsoid.color = glm::vec4((float)d["color"][0], (float)d["color"][1],
                                                     (float)d["color"][2], (float)d["color"][3]);
            if (d.contains("fade")) cfg.deathEllipsoid.fade = d["fade"];
            Debug::log(Debug::Category::NpcCombat,
                "[HITFX CONFIG] deathEllipsoid enabled=%d lifetime=%.2f length=%.2f radius=%.2f\n",
                (int)cfg.deathEllipsoid.enabled, cfg.deathEllipsoid.lifetime,
                cfg.deathEllipsoid.length, cfg.deathEllipsoid.radius);
        }

        auto loadBurstConfig = [&](const json& jc, MovementDashBurstConfig& burstCfg) {
            if (jc.contains("enabled")) burstCfg.enabled = jc["enabled"];
            if (jc.contains("lifetimeTicks")) burstCfg.lifetimeTicks = jc["lifetimeTicks"];
            if (jc.contains("lengthStart")) burstCfg.lengthStart = jc["lengthStart"];
            if (jc.contains("lengthEnd")) burstCfg.lengthEnd = jc["lengthEnd"];
            if (jc.contains("radiusStart")) burstCfg.radiusStart = jc["radiusStart"];
            if (jc.contains("radiusEnd")) burstCfg.radiusEnd = jc["radiusEnd"];
            if (jc.contains("colorStart")) burstCfg.colorStart = readVec3Json(jc["colorStart"]);
            if (jc.contains("colorEnd")) burstCfg.colorEnd = readVec3Json(jc["colorEnd"]);
            if (jc.contains("alphaStart")) burstCfg.alphaStart = jc["alphaStart"];
            if (jc.contains("alphaEnd")) burstCfg.alphaEnd = jc["alphaEnd"];
            if (jc.contains("brightnessStart")) burstCfg.brightnessStart = jc["brightnessStart"];
            if (jc.contains("brightnessEnd")) burstCfg.brightnessEnd = jc["brightnessEnd"];
            if (jc.contains("speedScaling")) burstCfg.speedScaling = jc["speedScaling"];
            if (jc.contains("speedThreshold")) burstCfg.speedThreshold = jc["speedThreshold"];
            if (jc.contains("speedScaleMin")) burstCfg.speedScaleMin = jc["speedScaleMin"];
            if (jc.contains("speedScaleMax")) burstCfg.speedScaleMax = jc["speedScaleMax"];
            if (jc.contains("forwardOffset") || jc.contains("directionalOffset")) {
                burstCfg.forwardOffset = jc.contains("forwardOffset")
                    ? (float)jc["forwardOffset"] : (float)jc["directionalOffset"];
            }
            if (jc.contains("rightOffset")) burstCfg.rightOffset = jc["rightOffset"];
            if (jc.contains("upOffset")) burstCfg.upOffset = jc["upOffset"];
        };

        if (j.contains("movementDashBurst")) loadBurstConfig(j["movementDashBurst"], cfg.movementDashBurst);
        if (j.contains("groundJumpBurst")) loadBurstConfig(j["groundJumpBurst"], cfg.groundJumpBurst);
        if (j.contains("airJumpBurst")) loadBurstConfig(j["airJumpBurst"], cfg.airJumpBurst);

        gConfig = cfg;
        auto ec = std::filesystem::last_write_time(path);
        gConfigLastWrite = ec;
        Debug::log(Debug::Category::NpcCombat, "[HITFX] Loaded %s\n", path.c_str());
    } catch (const std::exception& e) {
        Debug::log(Debug::Category::NpcCombat, "[HITFX] ERROR parsing config, keeping previous values: %s\n", e.what());
    }
}

void HitEffects::pollReload()
{
    if (!gConfig.hotReload || gConfigPath.empty()) return;
    std::error_code ec;
    auto wt = std::filesystem::last_write_time(gConfigPath, ec);
    if (ec) return;
    if (wt != gConfigLastWrite) {
        Debug::log(Debug::Category::NpcCombat, "[HITFX] File changed\n");
        loadConfig(gConfigPath);
        Debug::log(Debug::Category::NpcCombat, "[HITFX] Reloaded %s\n", gConfigPath.c_str());
    }
}

const HitFxConfig& HitEffects::config() { return gConfig; }
HitFxConfig& HitEffects::mutableConfig() { return gConfig; }

static void renderSphereTimeline(const HitBurstEffect& b, int age, const Camera& camera)
{
    for (const auto& kf : gConfig.sphereTimeline) {
        if (age < kf.startTick || age > kf.endTick) continue;
        int range = std::max(1, kf.endTick - kf.startTick);
        float t = (float)(age - kf.startTick) / (float)range;
        t = evalCurve(gConfig.curves.radiusCurve, t);
        float radius = lerp(kf.startRadius, kf.endRadius, t);
        float alpha = lerp(kf.alphaStart, kf.alphaEnd, evalCurve(gConfig.curves.alphaCurve, (float)(age - kf.startTick) / (float)range));
        float brightness = lerp(kf.brightnessStart, kf.brightnessEnd, evalCurve(gConfig.curves.brightnessCurve, (float)(age - kf.startTick) / (float)range));
        glm::vec3 color = lerpVec3(kf.colorStart, kf.colorEnd, (float)(age - kf.startTick) / (float)range);
        glm::vec4 col{color.x * brightness, color.y * brightness, color.z * brightness, std::clamp(alpha, 0.0f, 1.0f)};
        DebugVis::drawFilledSphere(camera, b.position, radius, col);
    }
}

static void renderElongatedSphere(const HitBurstEffect& b, int age, const Camera& camera)
{
    const auto& cfg = gConfig.elongatedSphere;
    if (!cfg.enabled || age < cfg.startTick || age > cfg.endTick) return;
    int range = std::max(1, cfg.endTick - cfg.startTick);
    float t = (float)(age - cfg.startTick) / (float)range;
    float len = lerp(cfg.lengthStart, cfg.lengthEnd, evalCurve(gConfig.curves.radiusCurve, t));
    float rad = lerp(cfg.radiusStart, cfg.radiusEnd, evalCurve(gConfig.curves.radiusCurve, t));
    float alpha = lerp(cfg.alphaStart, cfg.alphaEnd, evalCurve(gConfig.curves.alphaCurve, t));
    float brightness = lerp(cfg.brightnessStart, cfg.brightnessEnd, evalCurve(gConfig.curves.brightnessCurve, t));
    glm::vec3 color = lerpVec3(cfg.colorStart, cfg.colorEnd, t);
    glm::vec4 col{color.x * brightness, color.y * brightness, color.z * brightness, std::clamp(alpha, 0.0f, 1.0f)};

    glm::vec3 dir = glm::length(b.direction) > 0.001f ? glm::normalize(b.direction) : glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 scaleVec = dir * (len / std::max(rad, 0.001f)) + glm::vec3(1.0f) - dir;
    DebugVis::drawFilledSphere(camera, b.position, rad, col, scaleVec);
}

static void renderImpactDisc(const HitBurstEffect& b, int age, const Camera& camera)
{
    const auto& cfg = gConfig.impactDisc;
    if (!cfg.enabled || age < cfg.startTick || age > cfg.endTick) return;
    int range = std::max(1, cfg.endTick - cfg.startTick);
    float t = (float)(age - cfg.startTick) / (float)range;
    float radius = lerp(cfg.radiusStart, cfg.radiusEnd, evalCurve(gConfig.curves.radiusCurve, t));
    float alpha = lerp(cfg.alphaStart, cfg.alphaEnd, evalCurve(gConfig.curves.alphaCurve, t));
    float brightness = lerp(cfg.brightnessStart, cfg.brightnessEnd, evalCurve(gConfig.curves.brightnessCurve, t));
    glm::vec3 color = lerpVec3(cfg.colorStart, cfg.colorEnd, t);
    glm::vec4 col{color.x * brightness, color.y * brightness, color.z * brightness, std::clamp(alpha, 0.0f, 1.0f)};
    glm::vec3 nrm = glm::length(b.normal) > 0.001f ? glm::normalize(b.normal) : glm::vec3(0.0f, 0.0f, 1.0f);
    DebugVis::drawFilledDecal(camera, b.position, nrm, radius, col);
}

static void spawnParticles(const HitBurstEffect& b, int age)
{
    const auto& cfg = gConfig.particles;
    if (!cfg.enabled) return;
    if (age != 0) return;

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
        e.maxLifetime = (float)cfg.lifetimeTicks / 60.0f;
        e.scale = startSz;
        e.endScale = endSz;
        e.gravity = cfg.gravity;
        e.thickness = cfg.drag;
        e.replayType = "hitfx_particle";
        EffectPartSystem::instance().spawn(e);
    }
}

void HitEffects::onHit(const HitEvent& event)
{
    if (!gConfig.enabled) return;

    // 1. Legacy contact sphere
    if (gConfig.legacyContactSphere.enabled) {
        EffectPart e;
        e.position = event.position;
        const auto& l = gConfig.legacyContactSphere;
        e.color = glm::vec4(l.color.x, l.color.y, l.color.z, l.alpha);
        e.maxLifetime = l.lifetimeSeconds;
        e.scale = l.startRadius;
        e.endScale = l.endRadius;
        e.replayType = "contact_sphere";
        EffectPartSystem::instance().spawn(e);
    }

    // 2. Entity impact sphere (red)
    if (event.hitEntity && gConfig.core.entityImpact) {
        EffectPartSystem::instance().spawnEntityImpact(
            event.position, event.normal, event.attacker, event.victim);
    }

    // 3. World impact sphere (gray)
    if (event.hitWorld && gConfig.core.worldImpact) {
        EffectPartSystem::instance().spawnWorldImpact(event.position, event.normal);
    }

    // 4. Bullet impact hole
    if (event.hitWorld && gConfig.core.bulletImpact) {
        EffectPartSystem::instance().spawnBulletImpact(event.position);
    }

    // 5. Blood effect
    if (event.hitEntity && gConfig.core.entityImpact) {
        EffectPartSystem::instance().spawnBloodEffect(
            event.position, event.direction, (float)event.damage,
            event.attacker, event.victim);
    }

    // 6. Damage number
    if (gConfig.core.damageNumbers) {
        EffectPart e;
        e.position = event.position;
        e.color = glm::vec4(1.0f, 0.9f, 0.1f, 1.0f);
        e.maxLifetime = 1.2f;
        e.scale = 0.35f;
        e.label = std::to_string(event.damage);
        e.replayType = "damage_number";
        EffectPartSystem::instance().spawn(e);
    }

    // 7. HitFX timeline (burst)
    spawnHitEffects(event.position, event.direction, event.normal, event.damage,
                    event.attacker, event.victim);

    if (gHitFxTraceEnabled) {
        Debug::log(Debug::Category::NpcCombat,
            "[HITFX] source=%s event=%s damage=%d spawnDamage=%d spawnEntityImpact=%d spawnBlood=%d spawnParticles=%d\n",
            event.weaponSource.c_str(),
            event.hitEntity ? "entity_hit" : "world_hit",
            event.damage,
            (int)gConfig.core.damageNumbers,
            (int)(event.hitEntity && gConfig.core.entityImpact),
            (int)(event.hitEntity && gConfig.core.entityImpact),
            (int)gConfig.particles.enabled);
    }
}

void HitEffects::spawnHitEffects(glm::vec3 hitPoint, const glm::vec3& hitDirection,
                                  const glm::vec3& hitNormal, int damage,
                                  const std::string& sourceId,
                                  const std::string& targetId)
{
    (void)sourceId;
    (void)targetId;
    if (!gConfig.enabled) return;
    if (gBurstCount >= MAX_BURSTS) {
        gBursts[0] = gBursts[gBurstCount - 1];
        gBurstCount--;
    }
    HitBurstEffect& b = gBursts[gBurstCount++];
    b.position = hitPoint;
    b.direction = glm::length(hitDirection) > 0.001f ? glm::normalize(hitDirection) : hitNormal;
    b.normal = glm::length(hitNormal) > 0.001f ? glm::normalize(hitNormal) : -b.direction;
    b.spawnTick = gGlobalTick;
    b.totalTicks = gConfig.core.lifetimeTicks;
    b.alive = true;

    ReplayEffectEvent ev;
    ev.type = "hit_burst";
    ev.position = hitPoint;
    ev.sourceActorId = "hitfx";
    ev.targetActorId = "burst";
    ev.lifetime = (float)b.totalTicks / 60.0f;
    captureReplayEffect(ev);

    if (gConfig.legacyContactSphere.enabled) {
        EffectPart e;
        e.position = hitPoint;
        const auto& l = gConfig.legacyContactSphere;
        e.color = glm::vec4(l.color.x, l.color.y, l.color.z, l.alpha);
        e.maxLifetime = l.lifetimeSeconds;
        e.scale = l.startRadius;
        e.endScale = l.endRadius;
        e.replayType = "contact_sphere";
        EffectPartSystem::instance().spawn(e);
    }
    if (gConfig.core.damageNumbers) {
        EffectPart e;
        e.position = hitPoint;
        e.color = glm::vec4(1.0f, 0.9f, 0.1f, 1.0f);
        e.maxLifetime = 1.2f;
        e.scale = 0.35f;
        e.label = std::to_string(damage);
        e.replayType = "damage_number";
        EffectPartSystem::instance().spawn(e);
    }
    if (gHitFxTraceEnabled) {
        Debug::log(Debug::Category::NpcCombat, "[HITFX TRACE] Source=hit-effects.cpp Type=contact_sphere pos=(%.1f,%.1f,%.1f)\n",
                   hitPoint.x, hitPoint.y, hitPoint.z);
        Debug::log(Debug::Category::NpcCombat, "[HITFX TRACE] Source=hit-effects.cpp Type=damage_number pos=(%.1f,%.1f,%.1f) damage=%d\n",
                   hitPoint.x, hitPoint.y, hitPoint.z, damage);
    }

    spawnParticles(b, 0);

    Debug::log(Debug::Category::NpcCombat, "[HITFX] spawned at (%.1f %.1f %.1f) damage=%d dir=(%.2f %.2f %.2f) normal=(%.2f %.2f %.2f)",
               hitPoint.x, hitPoint.y, hitPoint.z, damage,
               hitDirection.x, hitDirection.y, hitDirection.z,
               hitNormal.x, hitNormal.y, hitNormal.z);
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

    glm::vec3 offset = fwd * cfg.forwardOffset + right * cfg.rightOffset + up * cfg.upOffset;
    glm::vec3 spawnPos = position + offset;
    spawnPos.z += 0.05f;

    Debug::log(Debug::Category::General, "[DASH FX] position=(%.2f,%.2f,%.2f) direction=(%.2f,%.2f,%.2f) offset=(%.2f,%.2f,%.2f) spawn=(%.2f,%.2f,%.2f)",
               position.x, position.y, position.z,
               dir.x, dir.y, dir.z,
               offset.x, offset.y, offset.z,
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
}

void HitEffects::spawnGroundJumpBurst(const glm::vec3& position)
{
    if (!gConfig.enabled || !gConfig.groundJumpBurst.enabled || !gDashFXEnabled) return;
    if (gBurstCount >= MAX_BURSTS) {
        gBursts[0] = gBursts[gBurstCount - 1];
        gBurstCount--;
    }
    const auto& cfg = gConfig.groundJumpBurst;
    glm::vec3 fwd(0.0f, 0.0f, 1.0f);
    glm::vec3 up(0.0f, 0.0f, 1.0f);
    glm::vec3 right = glm::normalize(glm::cross(fwd, up));
    up = glm::normalize(glm::cross(right, fwd));
    glm::vec3 offset = fwd * cfg.forwardOffset + right * cfg.rightOffset + up * cfg.upOffset;
    glm::vec3 spawnPos = position + offset;
    HitBurstEffect& b = gBursts[gBurstCount++];
    b.position = spawnPos;
    b.direction = glm::vec3(0.0f, 0.0f, 1.0f);
    b.normal = glm::vec3(0.0f, 0.0f, 1.0f);
    b.spawnTick = gGlobalTick;
    b.totalTicks = cfg.lifetimeTicks;
    b.alive = true;
    b.dashBurst = false;
    b.dashSpeed = 0.0f;
    b.burstType = BurstType::GroundJump;
}

void HitEffects::spawnAirJumpBurst(const glm::vec3& position)
{
    if (!gConfig.enabled || !gConfig.airJumpBurst.enabled || !gDashFXEnabled) return;
    if (gBurstCount >= MAX_BURSTS) {
        gBursts[0] = gBursts[gBurstCount - 1];
        gBurstCount--;
    }
    const auto& cfg = gConfig.airJumpBurst;
    glm::vec3 fwd(0.0f, 0.0f, 1.0f);
    glm::vec3 up(0.0f, 0.0f, 1.0f);
    glm::vec3 right = glm::normalize(glm::cross(fwd, up));
    up = glm::normalize(glm::cross(right, fwd));
    glm::vec3 offset = fwd * cfg.forwardOffset + right * cfg.rightOffset + up * cfg.upOffset;
    glm::vec3 spawnPos = position + offset;
    HitBurstEffect& b = gBursts[gBurstCount++];
    b.position = spawnPos;
    b.direction = glm::vec3(0.0f, 0.0f, 1.0f);
    b.normal = glm::vec3(0.0f, 0.0f, 1.0f);
    b.spawnTick = gGlobalTick;
    b.totalTicks = cfg.lifetimeTicks;
    b.alive = true;
    b.dashBurst = false;
    b.dashSpeed = 0.0f;
    b.burstType = BurstType::AirJump;
}

void HitEffects::updateHitBursts(float dt)
{
    (void)dt;
    pollReload();
    gGlobalTick++;
    for (int i = 0; i < gBurstCount; ++i) {
        HitBurstEffect& b = gBursts[i];
        if (!b.alive) continue;
        int age = gGlobalTick - b.spawnTick;
        if (age >= b.totalTicks) {
            b.alive = false;
            if (i < gBurstCount - 1)
                b = gBursts[gBurstCount - 1];
            gBurstCount--;
            i--;
        }
    }
}

static const MovementDashBurstConfig& burstConfigForType(BurstType type) {
    switch (type) {
        case BurstType::GroundJump: return gConfig.groundJumpBurst;
        case BurstType::AirJump: return gConfig.airJumpBurst;
        default: return gConfig.movementDashBurst;
    }
}

static void renderDirectionalBurst(const HitBurstEffect& b, int age, const Camera& camera)
{
    const auto& cfg = burstConfigForType(b.burstType);
    if (!cfg.enabled) return;
    if (age < 0 || age > cfg.lifetimeTicks) return;

    float t = cfg.lifetimeTicks > 0 ? (float)age / (float)cfg.lifetimeTicks : 1.0f;
    t = std::clamp(t, 0.0f, 1.0f);

    float len = lerp(cfg.lengthStart, cfg.lengthEnd, t);
    float rad = lerp(cfg.radiusStart, cfg.radiusEnd, t);
    float alpha = lerp(cfg.alphaStart, cfg.alphaEnd, t);
    float brightness = lerp(cfg.brightnessStart, cfg.brightnessEnd, t);
    glm::vec3 color = lerpVec3(cfg.colorStart, cfg.colorEnd, t);

    if (cfg.speedScaling && b.dashSpeed > cfg.speedThreshold) {
        float scale = std::clamp(b.dashSpeed / cfg.speedThreshold, cfg.speedScaleMin, cfg.speedScaleMax);
        len *= scale;
    }

    // For ground/air jump, the direction is always Z-up (vertical elongated sphere)
    glm::vec3 dir = glm::length(b.direction) > 0.001f ? glm::normalize(b.direction) : glm::vec3(0.0f, 0.0f, 1.0f);

    // Ground jump and air jump are always vertical along Z
    if (b.burstType == BurstType::GroundJump || b.burstType == BurstType::AirJump)
        dir = glm::vec3(0.0f, 0.0f, 1.0f);

    glm::vec4 col{color.x * brightness, color.y * brightness, color.z * brightness, std::clamp(alpha, 0.0f, 1.0f)};

    glm::vec3 scaleVec = dir * (len / std::max(rad, 0.001f)) + glm::vec3(1.0f) - dir;
    DebugVis::drawFilledSphere(camera, b.position, rad, col, scaleVec);
}

void HitEffects::renderHitBursts(const Camera& camera)
{
    for (int i = 0; i < gBurstCount; ++i) {
        const HitBurstEffect& b = gBursts[i];
        if (!b.alive) continue;
        int age = gGlobalTick - b.spawnTick;

        if (b.dashBurst || b.burstType == BurstType::GroundJump || b.burstType == BurstType::AirJump) {
            renderDirectionalBurst(b, age, camera);
        } else {
            renderSphereTimeline(b, age, camera);
            renderElongatedSphere(b, age, camera);
            renderImpactDisc(b, age, camera);
        }
    }

    if (gDebugPanelEnabled) {
        char buf[512];
        int len = std::snprintf(buf, sizeof(buf),
            "HIT FX\nbursts=%d/%d\nspheres=%zu\nparticles(enabled=%d count=%d)\n"
            "elongated(enabled=%d)\ndisc(enabled=%d)\nlifetime=%d\n"
            "LegacySphere: %s\ndmgNum=%d entImpact=%d worldImpact=%d bulletImpact=%d",
            gBurstCount, MAX_BURSTS, gConfig.sphereTimeline.size(),
            (int)gConfig.particles.enabled, gConfig.particles.count,
            (int)gConfig.elongatedSphere.enabled,
            (int)gConfig.impactDisc.enabled,
            gConfig.core.lifetimeTicks,
            gConfig.legacyContactSphere.enabled ? "ON" : "OFF",
            (int)gConfig.core.damageNumbers, (int)gConfig.core.entityImpact,
            (int)gConfig.core.worldImpact, (int)gConfig.core.bulletImpact);
        uiDrawText(buf, 12.0f, 320.0f, 0.32f, {0.3f, 1.0f, 0.5f, 1.0f});
    }
}

void HitEffects::clearHitBursts()
{
    gBurstCount = 0;
}

int HitEffects::activeBurstCount()
{
    return gBurstCount;
}

int HitEffects::debugBurstCount()
{
    return gBurstCount;
}

int HitEffects::collectBurstSnapshots(HitBurstSnapshot* out, int maxCount)
{
    int count = 0;
    for (int i = 0; i < gBurstCount && count < maxCount; ++i) {
        const HitBurstEffect& b = gBursts[i];
        out[count].position = b.position;
        out[count].ageTicks = gGlobalTick - b.spawnTick;
        out[count].totalTicks = b.totalTicks;
        out[count].alive = b.alive;
        count++;
    }
    return count;
}