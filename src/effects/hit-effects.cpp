#include "effects/hit-effects.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>

#include <nlohmann/json.hpp>

#include "effects/effect-part.h"
#include "debug/debug-visuals.h"
#include "debug/debug-log.h"
#include "gui/ui-system.h"
#include "camera.h"
#include "replay/replay.h"

using json = nlohmann::json;

bool gBloodFXEnabled = true;

static constexpr int MAX_BURSTS = 64;
static HitBurstEffect gBursts[MAX_BURSTS];
static int gBurstCount = 0;
static int gGlobalTick = 0;

static HitFxConfigData gConfig;
static std::filesystem::file_time_type gConfigLastWrite;
static std::string gConfigPath = "config/hitfx.json";
static bool gConfigLoaded = false;
static bool gDebugPanelEnabled = false;

static float evalCurve(const std::string& curve, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    if (curve == "ease_in") return t * t;
    if (curve == "ease_out") return 1.0f - (1.0f - t) * (1.0f - t);
    if (curve == "ease_in_out") return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) * 0.5f;
    if (curve == "exponential") return t < 0.5f ? std::pow(2.0f, 10.0f * t - 10.0f) * 0.5f : (2.0f - std::pow(2.0f, -10.0f * t + 10.0f)) * 0.5f;
    return t;
}

static float stageProgress(int age, int ticks)
{
    return ticks > 0 ? std::clamp((float)age / (float)ticks, 0.0f, 1.0f) : 1.0f;
}

static void readVec3(const json& j, const char* key, glm::vec3& out)
{
    if (j.contains(key) && j[key].is_array() && j[key].size() >= 3)
        out = glm::vec3((float)j[key][0], (float)j[key][1], (float)j[key][2]);
}

static void readStage(const json& j, StageConfig& s)
{
    if (j.contains("enabled")) s.enabled = j["enabled"];
    if (j.contains("ticks")) s.ticks = j["ticks"];
    if (j.contains("alpha")) s.alpha = j["alpha"];
    if (j.contains("brightness")) s.brightness = j["brightness"];
    readVec3(j, "color", s.color);
    if (j.contains("sizeCurve")) s.sizeCurve = j["sizeCurve"];
    if (j.contains("alphaCurve")) s.alphaCurve = j["alphaCurve"];
    if (j.contains("brightnessCurve")) s.brightnessCurve = j["brightnessCurve"];
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

        if (j.contains("whiteImpactStar"))
            readStage(j["whiteImpactStar"], gConfig.whiteStar);
        if (j.contains("whiteImpactStar") && j["whiteImpactStar"].contains("spikeCount"))
            gConfig.whiteStar.spikeCount = j["whiteImpactStar"]["spikeCount"];
        if (j.contains("whiteImpactStar") && j["whiteImpactStar"].contains("innerRadius"))
            gConfig.whiteStar.innerRadius = j["whiteImpactStar"]["innerRadius"];
        if (j.contains("whiteImpactStar") && j["whiteImpactStar"].contains("outerRadius"))
            gConfig.whiteStar.outerRadius = j["whiteImpactStar"]["outerRadius"];
        if (j.contains("whiteImpactStar") && j["whiteImpactStar"].contains("rotationSpeed"))
            gConfig.whiteStar.rotationSpeed = j["whiteImpactStar"]["rotationSpeed"];
        if (j.contains("whiteImpactStar") && j["whiteImpactStar"].contains("randomRotation"))
            gConfig.whiteStar.randomRotation = j["whiteImpactStar"]["randomRotation"];

        if (j.contains("brightBlueBurst"))
            readStage(j["brightBlueBurst"], gConfig.blueBurst);
        if (j.contains("brightBlueBurst") && j["brightBlueBurst"].contains("radius"))
            gConfig.blueBurst.radius = j["brightBlueBurst"]["radius"];

        if (j.contains("darkBlueFade"))
            readStage(j["darkBlueFade"], gConfig.blueFade);
        if (j.contains("darkBlueFade") && j["darkBlueFade"].contains("radius"))
            gConfig.blueFade.radius = j["darkBlueFade"]["radius"];

        if (j.contains("impactConeParticles")) {
            const json& c = j["impactConeParticles"];
            if (c.contains("enabled")) gConfig.cone.enabled = c["enabled"];
            if (c.contains("particleCount")) gConfig.cone.particleCount = c["particleCount"];
            if (c.contains("coneAngleDegrees")) gConfig.cone.coneAngleDegrees = c["coneAngleDegrees"];
            if (c.contains("initialSpeed")) gConfig.cone.initialSpeed = c["initialSpeed"];
            if (c.contains("speedVariation")) gConfig.cone.speedVariation = c["speedVariation"];
            if (c.contains("gravity")) gConfig.cone.gravity = c["gravity"];
            if (c.contains("drag")) gConfig.cone.drag = c["drag"];
            if (c.contains("ticks")) gConfig.cone.ticks = c["ticks"];
            if (c.contains("startSize")) gConfig.cone.startSize = c["startSize"];
            if (c.contains("endSize")) gConfig.cone.endSize = c["endSize"];
            if (c.contains("brightness")) gConfig.cone.brightness = c["brightness"];
            if (c.contains("alpha")) gConfig.cone.alpha = c["alpha"];
            readVec3(c, "color", gConfig.cone.color);
        }

        if (j.contains("elongatedImpactSphere")) {
            const json& e = j["elongatedImpactSphere"];
            if (e.contains("enabled")) gConfig.oval.enabled = e["enabled"];
            if (e.contains("ticks")) gConfig.oval.ticks = e["ticks"];
            if (e.contains("startLength")) gConfig.oval.startLength = e["startLength"];
            if (e.contains("endLength")) gConfig.oval.endLength = e["endLength"];
            if (e.contains("startRadius")) gConfig.oval.startRadius = e["startRadius"];
            if (e.contains("endRadius")) gConfig.oval.endRadius = e["endRadius"];
            if (e.contains("alphaStart")) gConfig.oval.alphaStart = e["alphaStart"];
            if (e.contains("alphaEnd")) gConfig.oval.alphaEnd = e["alphaEnd"];
            if (e.contains("brightness")) gConfig.oval.brightness = e["brightness"];
            readVec3(e, "color", gConfig.oval.color);
            if (e.contains("sizeCurve")) gConfig.oval.sizeCurve = e["sizeCurve"];
            if (e.contains("alphaCurve")) gConfig.oval.alphaCurve = e["alphaCurve"];
            if (e.contains("brightnessCurve")) gConfig.oval.brightnessCurve = e["brightnessCurve"];
        }

        if (j.contains("perpendicularImpactDisc")) {
            const json& d = j["perpendicularImpactDisc"];
            if (d.contains("enabled")) gConfig.disc.enabled = d["enabled"];
            if (d.contains("ticks")) gConfig.disc.ticks = d["ticks"];
            if (d.contains("startRadius")) gConfig.disc.startRadius = d["startRadius"];
            if (d.contains("endRadius")) gConfig.disc.endRadius = d["endRadius"];
            if (d.contains("alphaStart")) gConfig.disc.alphaStart = d["alphaStart"];
            if (d.contains("alphaEnd")) gConfig.disc.alphaEnd = d["alphaEnd"];
            if (d.contains("brightness")) gConfig.disc.brightness = d["brightness"];
            readVec3(d, "color", gConfig.disc.color);
            if (d.contains("sizeCurve")) gConfig.disc.sizeCurve = d["sizeCurve"];
            if (d.contains("alphaCurve")) gConfig.disc.alphaCurve = d["alphaCurve"];
            if (d.contains("brightnessCurve")) gConfig.disc.brightnessCurve = d["brightnessCurve"];
        }

        auto ec = std::filesystem::last_write_time(path);
        gConfigLastWrite = ec;
        gConfigLoaded = true;
        Debug::log(Debug::Category::NpcCombat, "[HITFX] Reloaded %s\n", path.c_str());
    } catch (const std::exception& e) {
        Debug::log(Debug::Category::NpcCombat, "[HITFX] parse error: %s\n", e.what());
    }
}

void HitEffects::pollReload()
{
    if (gConfigPath.empty()) return;
    std::error_code ec;
    auto wt = std::filesystem::last_write_time(gConfigPath, ec);
    if (ec) return;
    if (wt != gConfigLastWrite) {
        Debug::log(Debug::Category::NpcCombat, "[HITFX] File changed\n");
        loadConfig(gConfigPath);
    }
}

const HitFxConfigData& HitEffects::config() { return gConfig; }

static void spawnWhiteStar(const HitBurstEffect& b, int age, const Camera& camera)
{
    const WhiteImpactStarConfig& cfg = gConfig.whiteStar;
    if (!cfg.enabled || age >= cfg.ticks) return;
    float t = stageProgress(age, cfg.ticks);
    float scale = cfg.innerRadius + (cfg.outerRadius - cfg.innerRadius) * evalCurve(cfg.sizeCurve, t);
    float alpha = cfg.alpha * evalCurve(cfg.alphaCurve, t);
    float bright = cfg.brightness * evalCurve(cfg.brightnessCurve, t);
    glm::vec4 col{cfg.color.x * bright, cfg.color.y * bright, cfg.color.z * bright, alpha};
    int count = cfg.spikeCount;
    float rotOff = cfg.randomRotation ? (float)(b.spawnTick * 137) : 0.0f;
    float rot = glm::radians((float)age * cfg.rotationSpeed + rotOff);
    for (int i = 0; i < count; ++i) {
        float a0 = (float)i / (float)count * 6.2831853f + rot;
        float a1 = a0 + 3.14159f / (float)count;
        glm::vec3 d0(std::cos(a0) * std::cos(a0 * 0.5f), std::sin(a0) * std::cos(a0 * 0.5f), std::sin(a0 * 0.5f));
        glm::vec3 d1(std::cos(a1) * std::cos(a1 * 0.5f), std::sin(a1) * std::cos(a1 * 0.5f), std::sin(a1 * 0.5f));
        if (glm::length(d0) > 0.001f) d0 = glm::normalize(d0);
        if (glm::length(d1) > 0.001f) d1 = glm::normalize(d1);
        DebugVis::drawFilledBeam(camera, b.position, b.position + d0 * scale, 0.04f, col);
        DebugVis::drawFilledBeam(camera, b.position, b.position + d1 * scale, 0.04f, col);
    }
}

static void spawnBlueBurst(const HitBurstEffect& b, int age, const Camera& camera)
{
    const BrightBlueBurstConfig& cfg = gConfig.blueBurst;
    if (!cfg.enabled || age >= cfg.ticks) return;
    float t = stageProgress(age, cfg.ticks);
    float radius = cfg.radius * evalCurve(cfg.sizeCurve, t);
    float alpha = cfg.alpha * evalCurve(cfg.alphaCurve, t);
    float bright = cfg.brightness * evalCurve(cfg.brightnessCurve, t);
    glm::vec4 col{cfg.color.x * bright, cfg.color.y * bright, cfg.color.z * bright, alpha};
    DebugVis::drawFilledSphere(camera, b.position, radius, col);
}

static void spawnBlueFade(const HitBurstEffect& b, int age, const Camera& camera)
{
    const DarkBlueFadeConfig& cfg = gConfig.blueFade;
    if (!cfg.enabled || age >= cfg.ticks) return;
    float t = stageProgress(age, cfg.ticks);
    float radius = cfg.radius * evalCurve(cfg.sizeCurve, t);
    float alpha = cfg.alpha * evalCurve(cfg.alphaCurve, t);
    float bright = cfg.brightness * evalCurve(cfg.brightnessCurve, t);
    glm::vec4 col{cfg.color.x * bright, cfg.color.y * bright, cfg.color.z * bright, alpha};
    DebugVis::drawFilledSphere(camera, b.position, radius, col);
}

static void spawnConeParticles(const HitBurstEffect& b, int age, const Camera& camera)
{
    (void)camera;
    const ImpactConeConfig& cfg = gConfig.cone;
    if (!cfg.enabled) return;
    if (age != 0) return;
    float coneRad = glm::radians(cfg.coneAngleDegrees);
    for (int i = 0; i < cfg.particleCount; ++i) {
        float angle = (float)std::rand() / (float)RAND_MAX * 6.2831853f;
        float spread = (float)std::rand() / (float)RAND_MAX * coneRad;
        float speed = cfg.initialSpeed + ((float)std::rand() / (float)RAND_MAX - 0.5f) * 2.0f * cfg.speedVariation;
        speed = std::max(0.0f, speed);
        glm::vec3 dir = b.normal;
        glm::vec3 up(0, 0, 1);
        if (std::abs(glm::dot(dir, up)) > 0.99f) up = glm::vec3(1, 0, 0);
        glm::vec3 right = glm::normalize(glm::cross(dir, up));
        glm::vec3 localUp = glm::cross(right, dir);
        glm::vec3 vel = glm::normalize(dir + (std::cos(angle) * right + std::sin(angle) * localUp) * std::tan(spread)) * speed;
        float startSz = cfg.startSize;
        float endSz = cfg.endSize;
        EffectPart e;
        e.position = b.position;
        e.velocity = vel;
        e.color = glm::vec4(cfg.color.x * cfg.brightness, cfg.color.y * cfg.brightness, cfg.color.z * cfg.brightness, cfg.alpha);
        e.maxLifetime = (float)cfg.ticks / 60.0f;
        e.scale = startSz;
        e.endScale = endSz;
        e.gravity = cfg.gravity;
        e.thickness = cfg.drag;
        e.replayType = "hitfx_cone";
        EffectPartSystem::instance().spawn(e);
    }
}

static void spawnElongatedSphere(const HitBurstEffect& b, int age, const Camera& camera)
{
    const ElongatedSphereConfig& cfg = gConfig.oval;
    if (!cfg.enabled || age >= cfg.ticks) return;
    float t = stageProgress(age, cfg.ticks);
    float len = cfg.startLength + (cfg.endLength - cfg.startLength) * evalCurve(cfg.sizeCurve, t);
    float rad = cfg.startRadius + (cfg.endRadius - cfg.startRadius) * evalCurve(cfg.sizeCurve, t);
    float alpha = cfg.alphaStart + (cfg.alphaEnd - cfg.alphaStart) * evalCurve(cfg.alphaCurve, t);
    float bright = cfg.brightness * evalCurve(cfg.brightnessCurve, t);
    glm::vec4 col{cfg.color.x * bright, cfg.color.y * bright, cfg.color.z * bright, alpha};
    glm::vec3 dir = glm::length(b.normal) > 0.001f ? glm::normalize(b.normal) : glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 scaleVec = dir * (len / std::max(rad, 0.001f)) + glm::vec3(1.0f) - dir;
    DebugVis::drawFilledSphere(camera, b.position, rad, col, scaleVec);
}

static void spawnPerpendicularDisc(const HitBurstEffect& b, int age, const Camera& camera)
{
    const PerpendicularDiscConfig& cfg = gConfig.disc;
    if (!cfg.enabled || age >= cfg.ticks) return;
    float t = stageProgress(age, cfg.ticks);
    float radius = cfg.startRadius + (cfg.endRadius - cfg.startRadius) * evalCurve(cfg.sizeCurve, t);
    float alpha = cfg.alphaStart + (cfg.alphaEnd - cfg.alphaStart) * evalCurve(cfg.alphaCurve, t);
    float bright = cfg.brightness * evalCurve(cfg.brightnessCurve, t);
    glm::vec4 col{cfg.color.x * bright, cfg.color.y * bright, cfg.color.z * bright, alpha};
    glm::vec3 nrm = glm::length(b.normal) > 0.001f ? glm::normalize(b.normal) : glm::vec3(0.0f, 0.0f, 1.0f);
    DebugVis::drawFilledDecal(camera, b.position, nrm, radius, col);
}

void HitEffects::spawnHitEffects(glm::vec3 hitPoint, const glm::vec3& hitNormal,
                                  int damage, const std::string& sourceId,
                                  const std::string& targetId)
{
    (void)sourceId;
    (void)targetId;
    if (gBurstCount >= MAX_BURSTS) {
        gBursts[0] = gBursts[gBurstCount - 1];
        gBurstCount--;
    }
    HitBurstEffect& b = gBursts[gBurstCount++];
    b.position = hitPoint;
    b.normal = hitNormal;
    b.spawnTick = gGlobalTick;
    b.totalTicks = std::max({gConfig.whiteStar.ticks, gConfig.blueBurst.ticks, gConfig.blueFade.ticks,
                             gConfig.cone.ticks, gConfig.oval.ticks, gConfig.disc.ticks}) + 1;
    b.alive = true;

    ReplayEffectEvent ev;
    ev.type = "hit_burst";
    ev.position = hitPoint;
    ev.sourceActorId = "hitfx";
    ev.targetActorId = "burst";
    ev.lifetime = (float)b.totalTicks / 60.0f;
    captureReplayEffect(ev);

    {
        EffectPart e;
        e.position = hitPoint;
        e.color = glm::vec4(1.0f, 0.15f, 0.1f, 1.0f);
        e.maxLifetime = 0.25f;
        e.scale = 0.18f;
        e.endScale = 0.27f;
        e.replayType = "contact_sphere";
        EffectPartSystem::instance().spawn(e);
    }
    {
        EffectPart e;
        e.position = hitPoint;
        e.color = glm::vec4(1.0f, 0.9f, 0.1f, 1.0f);
        e.maxLifetime = 1.2f;
        e.scale = 0.35f;
        e.label = std::to_string(damage);
        e.replayType = "damage_number";
        EffectPartSystem::instance().spawn(e);
    }

    Debug::log(Debug::Category::NpcCombat, "[HITFX] spawned at (%.1f %.1f %.1f) damage=%d normal=(%.2f %.2f %.2f)",
               hitPoint.x, hitPoint.y, hitPoint.z, damage, hitNormal.x, hitNormal.y, hitNormal.z);
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

void HitEffects::renderHitBursts(const Camera& camera)
{
    for (int i = 0; i < gBurstCount; ++i) {
        const HitBurstEffect& b = gBursts[i];
        if (!b.alive) continue;
        int age = gGlobalTick - b.spawnTick;

        spawnWhiteStar(b, age, camera);
        spawnBlueBurst(b, age, camera);
        spawnBlueFade(b, age, camera);
        spawnConeParticles(b, age, camera);
        spawnElongatedSphere(b, age, camera);
        spawnPerpendicularDisc(b, age, camera);
    }

    if (gDebugPanelEnabled) {
        char buf[256];
        snprintf(buf, sizeof(buf), "HIT FX\nWhiteStar: spikes=%d ticks=%d\nBlueBurst: ticks=%d\nCone: particles=%d angle=%.0f\nDisc: radius=%.1f\nOval: length=%.1f",
                 gConfig.whiteStar.spikeCount, gConfig.whiteStar.ticks,
                 gConfig.blueBurst.ticks, gConfig.cone.particleCount,
                 gConfig.cone.coneAngleDegrees, gConfig.disc.endRadius, gConfig.oval.endLength);
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
