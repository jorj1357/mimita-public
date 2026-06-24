#include "effects/hit-effects.h"

#include <algorithm>
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
bool gDebugPanelEnabled = false;

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
