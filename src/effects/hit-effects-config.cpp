#include "effects/hit-effects.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

#include "debug/debug-log.h"

using json = nlohmann::json;

HitFxConfig gConfig;
static std::filesystem::file_time_type gConfigLastWrite;
static std::string gConfigPath = "config/hitfx.json";
static bool gHitFxWatchLogged = false;
bool gDebugPanelEnabled = false;

static void logHitFxWatchOnce(const std::string& path)
{
    if (gHitFxWatchLogged) return;
    gHitFxWatchLogged = true;
    const std::string fileName = std::filesystem::path(path).filename().string();
    Debug::warn(Debug::Category::NpcCombat, "[HITFX] Watching: %s\n", fileName.c_str());
}

static void logHitFxApplied(const char* key)
{
    Debug::warn(Debug::Category::NpcCombat, "[HITFX] Applied: %s\n", key);
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

bool HitEffects::loadConfig(const std::string& path)
{
    gConfigPath = path;
    logHitFxWatchOnce(path);
    std::ifstream file(path);
    if (!file.is_open()) {
        Debug::warn(Debug::Category::NpcCombat, "[HITFX] config not found: %s\n", path.c_str());
        return false;
    }
    try {
        json j;
        file >> j;

        HitFxConfig cfg;
        std::vector<const char*> appliedKeys;
        auto markHitFxApplied = [&](const char* key) {
            appliedKeys.push_back(key);
        };

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

        cfg.walkBurst.lengthStart = 0.3f;
        cfg.walkBurst.lengthEnd = 0.6f;
        cfg.walkBurst.radiusStart = 0.08f;
        cfg.walkBurst.radiusEnd = 0.04f;
        cfg.walkBurst.colorStart = {0.8f, 0.8f, 0.8f};
        cfg.walkBurst.colorEnd = {0.4f, 0.4f, 0.4f};
        cfg.walkBurst.forwardOffset = -0.6f;
        cfg.walkBurst.rightOffset = 0.0f;
        cfg.walkBurst.upOffset = -0.3f;
        cfg.walkBurst.lifetimeTicks = 6;

        cfg.landingBurst.lengthStart = 1.2f;
        cfg.landingBurst.lengthEnd = 2.0f;
        cfg.landingBurst.radiusStart = 0.15f;
        cfg.landingBurst.radiusEnd = 0.05f;
        cfg.landingBurst.colorStart = {0.6f, 0.6f, 0.6f};
        cfg.landingBurst.colorEnd = {0.2f, 0.2f, 0.2f};
        cfg.landingBurst.forwardOffset = 0.0f;
        cfg.landingBurst.rightOffset = 0.0f;
        cfg.landingBurst.upOffset = -0.5f;
        cfg.landingBurst.lifetimeTicks = 12;

        // healthGained defaults
        cfg.healthGained.lifetimeTicks = 60;
        cfg.healthGained.lengthStart = 2.0f;
        cfg.healthGained.lengthEnd = 4.0f;
        cfg.healthGained.radiusStart = 0.4f;
        cfg.healthGained.radiusEnd = 0.1f;
        cfg.healthGained.alphaStart = 0.5f;
        cfg.healthGained.alphaEnd = 0.0f;
        cfg.healthGained.brightnessStart = 2.0f;
        cfg.healthGained.brightnessEnd = 0.0f;
        cfg.healthGained.colorStart = {0.0f, 1.0f, 0.0f};
        cfg.healthGained.colorEnd = {0.0f, 0.6f, 0.0f};
        cfg.healthGained.stretchAxis = "world_z";

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

        if (j.contains("damageNumbers")) {
            const json& d = j["damageNumbers"];
            if (d.contains("enabled")) {
                cfg.damageNumber.enabled = d["enabled"];
                markHitFxApplied("hitfx_damage_numbers_enabled");
            }
            if (d.contains("fontSize")) {
                cfg.damageNumber.fontSize = d["fontSize"];
                markHitFxApplied("hitfx_text_size");
            }
            if (d.contains("lifetime")) {
                cfg.damageNumber.lifetime = d["lifetime"];
                markHitFxApplied("hitfx_lifetime");
            }
            if (d.contains("startOpacity")) {
                cfg.damageNumber.startOpacity = d["startOpacity"];
                markHitFxApplied("hitfx_start_alpha");
            }
            if (d.contains("endOpacity")) {
                cfg.damageNumber.endOpacity = d["endOpacity"];
                markHitFxApplied("hitfx_end_alpha");
            }
            if (d.contains("fadeStart")) {
                cfg.damageNumber.fadeStart = d["fadeStart"];
                markHitFxApplied("hitfx_fade_start");
            }
            if (d.contains("fadeEnd")) {
                cfg.damageNumber.fadeEnd = d["fadeEnd"];
                markHitFxApplied("hitfx_fade_end");
            }
            if (d.contains("worldOffset") && d["worldOffset"].is_array() && d["worldOffset"].size() >= 3) {
                cfg.damageNumber.worldOffsetX = d["worldOffset"][0];
                cfg.damageNumber.worldOffsetY = d["worldOffset"][1];
                cfg.damageNumber.worldOffsetZ = d["worldOffset"][2];
                markHitFxApplied("hitfx_world_offset");
            }
            if (d.contains("screenOffset") && d["screenOffset"].is_array() && d["screenOffset"].size() >= 2) {
                cfg.damageNumber.screenOffsetX = d["screenOffset"][0];
                cfg.damageNumber.screenOffsetY = d["screenOffset"][1];
                markHitFxApplied("hitfx_screen_offset");
            }
            if (d.contains("moveVelocity") && d["moveVelocity"].is_array() && d["moveVelocity"].size() >= 3) {
                cfg.damageNumber.moveX = d["moveVelocity"][0];
                cfg.damageNumber.moveY = d["moveVelocity"][1];
                cfg.damageNumber.moveZ = d["moveVelocity"][2];
                markHitFxApplied("hitfx_move_velocity");
            }
            if (d.contains("moveSpeed")) {
                cfg.damageNumber.moveSpeed = d["moveSpeed"];
                markHitFxApplied("hitfx_move_speed");
            }
            if (d.contains("startScale")) {
                cfg.damageNumber.startScale = d["startScale"];
                markHitFxApplied("hitfx_start_scale");
            }
            if (d.contains("endScale")) {
                cfg.damageNumber.endScale = d["endScale"];
                markHitFxApplied("hitfx_end_scale");
            }
            if (d.contains("randomHorizontalSpread")) {
                cfg.damageNumber.randomHorizontalSpread = d["randomHorizontalSpread"];
                markHitFxApplied("hitfx_horizontal_spread");
            }
            if (d.contains("randomVerticalSpread")) {
                cfg.damageNumber.randomVerticalSpread = d["randomVerticalSpread"];
                markHitFxApplied("hitfx_vertical_spread");
            }
            if (d.contains("spawnJitter")) {
                cfg.damageNumber.spawnJitter = d["spawnJitter"];
                markHitFxApplied("hitfx_spawn_jitter");
            }
            if (d.contains("spawnDelay")) {
                cfg.damageNumber.spawnDelay = d["spawnDelay"];
                markHitFxApplied("hitfx_spawn_delay");
            }
            if (d.contains("textColor") && d["textColor"].is_array() && d["textColor"].size() >= 3) {
                cfg.damageNumber.textColor = readVec3Json(d["textColor"]);
                markHitFxApplied("hitfx_text_color");
            }
            if (d.contains("criticalColor") && d["criticalColor"].is_array() && d["criticalColor"].size() >= 3) {
                cfg.damageNumber.criticalColor = readVec3Json(d["criticalColor"]);
                markHitFxApplied("hitfx_critical_color");
            }
            if (d.contains("healingColor") && d["healingColor"].is_array() && d["healingColor"].size() >= 3) {
                cfg.damageNumber.healingColor = readVec3Json(d["healingColor"]);
                markHitFxApplied("hitfx_healing_color");
            }
            if (d.contains("outlineEnabled")) {
                cfg.damageNumber.outlineEnabled = d["outlineEnabled"];
                markHitFxApplied("hitfx_outline_enabled");
            }
            if (d.contains("outlineThickness")) {
                cfg.damageNumber.outlineThickness = d["outlineThickness"];
                markHitFxApplied("hitfx_outline_thickness");
            }
            if (d.contains("outlineColor") && d["outlineColor"].is_array() && d["outlineColor"].size() >= 3) {
                cfg.damageNumber.outlineColor = readVec3Json(d["outlineColor"]);
                markHitFxApplied("hitfx_outline_color");
            }
            if (d.contains("shadowEnabled")) {
                cfg.damageNumber.shadowEnabled = d["shadowEnabled"];
                markHitFxApplied("hitfx_shadow_enabled");
            }
            if (d.contains("shadowOffset") && d["shadowOffset"].is_array() && d["shadowOffset"].size() >= 2) {
                cfg.damageNumber.shadowOffset.x = d["shadowOffset"][0];
                cfg.damageNumber.shadowOffset.y = d["shadowOffset"][1];
                markHitFxApplied("hitfx_shadow_offset");
            }
            if (d.contains("bold")) {
                cfg.damageNumber.bold = d["bold"];
                markHitFxApplied("hitfx_bold");
            }
            if (d.contains("italic")) {
                cfg.damageNumber.italic = d["italic"];
                markHitFxApplied("hitfx_italic");
            }
            Debug::log(Debug::Category::NpcCombat,
                "[HITFX CONFIG] damageNumbers enabled=%d fontSize=%.4f lifetime=%.2f\n",
                (int)cfg.damageNumber.enabled, cfg.damageNumber.fontSize, cfg.damageNumber.lifetime);
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
            if (jc.contains("offset")) burstCfg.offset = readVec3Json(jc["offset"]);
            if (jc.contains("scale")) burstCfg.scale = readVec3Json(jc["scale"]);
            if (jc.contains("rotation")) burstCfg.rotation = readVec3Json(jc["rotation"]);
            if (jc.contains("stretchAxis")) burstCfg.stretchAxis = jc["stretchAxis"];
        };

        if (j.contains("movementDashBurst")) loadBurstConfig(j["movementDashBurst"], cfg.movementDashBurst);
        if (j.contains("groundJumpBurst")) loadBurstConfig(j["groundJumpBurst"], cfg.groundJumpBurst);
        if (j.contains("airJumpBurst")) loadBurstConfig(j["airJumpBurst"], cfg.airJumpBurst);
        if (j.contains("walkBurst")) loadBurstConfig(j["walkBurst"], cfg.walkBurst);
        if (j.contains("landingBurst")) loadBurstConfig(j["landingBurst"], cfg.landingBurst);
        if (j.contains("healthGained")) loadBurstConfig(j["healthGained"], cfg.healthGained);

        gConfig = cfg;
        auto ec = std::filesystem::last_write_time(path);
        gConfigLastWrite = ec;
        Debug::warn(Debug::Category::NpcCombat, "[HITFX] Loaded successfully: %s\n", path.c_str());
        for (const char* key : appliedKeys)
            logHitFxApplied(key);
        return true;
    } catch (const std::exception& e) {
        Debug::warn(Debug::Category::NpcCombat,
            "[HITFX] Parse error in %s: %s. Keeping previous valid settings.\n",
            path.c_str(), e.what());
        return false;
    }
}

void HitEffects::pollReload()
{
    if (!gConfig.hotReload || gConfigPath.empty()) return;
    using Clock = std::chrono::steady_clock;
    static Clock::time_point nextCheck;
    const auto now = Clock::now();
    if (now < nextCheck) return;
    nextCheck = now + std::chrono::milliseconds(100);

    std::error_code ec;
    auto wt = std::filesystem::last_write_time(gConfigPath, ec);
    if (ec) return;
    if (wt != gConfigLastWrite) {
        gConfigLastWrite = wt;
        Debug::warn(Debug::Category::NpcCombat, "[HITFX] Detected change. Reloading...\n");
        if (!loadConfig(gConfigPath)) {
            Debug::warn(Debug::Category::NpcCombat,
                "[HITFX] Reload failed; keeping previous valid settings.\n");
        }
    }
}

const HitFxConfig& HitEffects::config() { return gConfig; }
HitFxConfig& HitEffects::mutableConfig() { return gConfig; }
