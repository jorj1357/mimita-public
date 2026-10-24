// 2026-09-07 16:20 EST
/* purpose
* parse and validate config/playervisuals.json
* atomically retain the last valid player-visual presentation state
* report reload failures without interrupting gameplay
* does NOT render outlines or inspect player entities
* does NOT apply competitive restrictions
* does NOT mutate simulation state
*/
#include "config/player-visuals-config.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include <fstream>
#include <cmath>
#include <nlohmann/json.hpp>
#include <sstream>
#include <algorithm>
#include <initializer_list>

namespace {
using json = nlohmann::json;
std::filesystem::file_time_type writeTime(const std::filesystem::path& p) {
    std::error_code ec;
    return std::filesystem::last_write_time(p, ec);
}
bool finite(float v) { return std::isfinite(v); }
bool onlyKeys(const json& object, std::initializer_list<const char*> allowed,
              const std::string& name, std::string& err) {
    for (auto it = object.begin(); it != object.end(); ++it) {
        bool found = false;
        for (const char* key : allowed) {
            if (it.key() == key) { found = true; break; }
        }
        if (!found) {
            err = name + "." + it.key() + " is not a supported setting";
            return false;
        }
    }
    return true;
}
bool validColor(const json& o, const char* key, const std::string& name, std::string& err) {
    if (!o.contains(key)) return true;
    const json& value = o.at(key);
    if (!value.is_array() || value.size() != 3) {
        err = name + " must be [r,g,b]";
        return false;
    }
    for (const auto& channel : value) {
        if (!channel.is_number() || !finite(channel.get<float>()) ||
            channel.get<float>() < 0.0f || channel.get<float>() > 255.0f) {
            err = name + " color values must be 0..255";
            return false;
        }
    }
    return true;
}
glm::vec3 readColor(const json& o, const char* key, const glm::vec3& fallback) {
    if (!o.contains(key) || !o.at(key).is_array() || o.at(key).size() < 3) return fallback;
    return {o.at(key)[0].get<float>(), o.at(key)[1].get<float>(), o.at(key)[2].get<float>()};
}
bool readSettings(const json& root, const char* name, PlayerOutlineSettings& s, std::string& err) {
    if (!root.contains(name)) return true;
    if (!root[name].is_object()) { err = std::string(name) + " must be an object"; return false; }
    const json& o = root[name];
    if (!onlyKeys(o, {"enabled", "thickness", "alpha", "color", "visibleThroughWalls", "disappearOnDeath"},
                  std::string(name) + ".outline", err)) return false;
    if (!validColor(o, "color", std::string(name) + ".outline.color", err)) return false;
    if (!o.contains("enabled") && !o.contains("thickness") && !o.contains("alpha") &&
        !o.contains("color") && !o.contains("visibleThroughWalls") &&
        !o.contains("disappearOnDeath")) return true;
    if (o.contains("enabled")) s.enabled = o.at("enabled").get<bool>();
    if (o.contains("thickness")) s.thickness = o.at("thickness").get<float>();
    if (o.contains("alpha")) s.alpha = o.at("alpha").get<float>();
    if (o.contains("color")) {
        const auto& c = o.at("color");
        s.color = {c[0].get<float>(), c[1].get<float>(), c[2].get<float>()};
    }
    if (o.contains("visibleThroughWalls")) s.visibleThroughWalls = o.at("visibleThroughWalls").get<bool>();
    if (o.contains("disappearOnDeath")) s.disappearOnDeath = o.at("disappearOnDeath").get<bool>();
    if (!finite(s.thickness) || !finite(s.alpha) || s.thickness < 0.0f) { err = std::string(name) + " outline numeric value is invalid"; return false; }
    return true;
}

bool readTrail(const json& value, PlayerTrailSettings& settings, const std::string& name, std::string& err) {
    if (!value.is_object()) { err = name + " must be an object"; return false; }
    if (!onlyKeys(value, {"enabled", "minSpeed", "sampleIntervalSeconds", "lifetimeSeconds", "maxSamples", "alpha", "width", "color"}, name, err)) return false;
    if (!validColor(value, "color", name + ".color", err)) return false;
    settings.enabled = value.value("enabled", settings.enabled);
    settings.minSpeed = value.value("minSpeed", settings.minSpeed);
    settings.sampleIntervalSeconds = value.value("sampleIntervalSeconds", settings.sampleIntervalSeconds);
    settings.lifetimeSeconds = value.value("lifetimeSeconds", settings.lifetimeSeconds);
    settings.maxSamples = value.value("maxSamples", settings.maxSamples);
    settings.alpha = value.value("alpha", settings.alpha);
    settings.width = value.value("width", settings.width);
    settings.color = readColor(value, "color", settings.color);
    if (!finite(settings.minSpeed) || !finite(settings.sampleIntervalSeconds) ||
        !finite(settings.lifetimeSeconds) || !finite(settings.alpha) || !finite(settings.width) ||
        settings.minSpeed < 0.0f || settings.sampleIntervalSeconds <= 0.0f ||
        settings.lifetimeSeconds <= 0.0f || settings.maxSamples < 2 || settings.maxSamples > 64 ||
        settings.alpha < 0.0f || settings.alpha > 1.0f || settings.width < 0.01f || settings.width > 4.0f) {
        err = name + " has an invalid range"; return false;
    }
    return true;
}

bool readContrast(const json& value, PlayerContrastSettings& settings, const std::string& name, std::string& err) {
    if (!value.is_object()) { err = name + " must be an object"; return false; }
    if (!onlyKeys(value, {"enabled", "brightness", "tint", "tintStrength"}, name, err)) return false;
    if (!validColor(value, "tint", name + ".tint", err)) return false;
    settings.enabled = value.value("enabled", settings.enabled);
    settings.brightness = value.value("brightness", settings.brightness);
    settings.tint = readColor(value, "tint", settings.tint);
    settings.tintStrength = value.value("tintStrength", settings.tintStrength);
    if (!finite(settings.brightness) || !finite(settings.tintStrength) ||
        settings.brightness < 0.0f || settings.brightness > 4.0f ||
        settings.tintStrength < 0.0f || settings.tintStrength > 1.0f) {
        err = name + " brightness must be 0..4 and tintStrength 0..1"; return false;
    }
    return true;
}

bool readPointLight(const json& value, PlayerPointLightSettings& settings,
                    const std::string& name, std::string& err) {
    if (!value.is_object()) { err = name + " must be an object"; return false; }
    if (!onlyKeys(value, {"enabled", "intensity", "radius", "heightOffset", "color"}, name, err)) return false;
    if (!validColor(value, "color", name + ".color", err)) return false;
    settings.enabled = value.value("enabled", settings.enabled);
    settings.intensity = value.value("intensity", settings.intensity);
    settings.radius = value.value("radius", settings.radius);
    settings.heightOffset = value.value("heightOffset", settings.heightOffset);
    settings.color = readColor(value, "color", settings.color);
    if (!finite(settings.intensity) || !finite(settings.radius) || !finite(settings.heightOffset) ||
        settings.intensity < 0.0f || settings.intensity > 10.0f ||
        settings.radius < 0.1f || settings.radius > 1000.0f ||
        std::abs(settings.heightOffset) > 100.0f) {
        err = name + " intensity must be 0..10, radius 0.1..1000, and heightOffset -100..100";
        return false;
    }
    return true;
}
}

PlayerVisualsConfig& PlayerVisualsConfig::instance() { static PlayerVisualsConfig c; return c; }
PlayerVisualsConfig::PlayerVisualsConfig() = default;

bool PlayerVisualsConfig::parseAndValidate(const std::string& text, PlayerVisualsData& out) {
    try {
        const json root = json::parse(text);
        if (!root.is_object()) { mLastError = "root must be an object"; return false; }
        for (auto it = root.begin(); it != root.end(); ++it) {
            const std::string key = it.key();
            if (key.rfind("comment_", 0) == 0 || key == "renderOrder" ||
                key == "self" || key == "enemy" || key == "teammate") continue;
            mLastError = key + " is not a supported root setting";
            return false;
        }
        if (root.contains("renderOrder")) {
            out.renderOrder = root.at("renderOrder").get<std::vector<std::string>>();
            for (size_t i = 0; i < out.renderOrder.size(); ++i) {
                const auto& name = out.renderOrder[i];
                if (name != "trail" && name != "capsule" && name != "outline" && name != "wireframe") {
                    mLastError = "renderOrder entries must be trail, capsule, outline, or wireframe";
                    return false;
                }
                for (size_t j = 0; j < i; ++j) {
                    if (out.renderOrder[j] == name) {
                        mLastError = "renderOrder entries must be unique";
                        return false;
                    }
                }
            }
        }
        for (const char* layer : {"self", "enemy", "teammate"}) {
            const json& section = root.contains(layer) ? root.at(layer) : json::object();
            if (!section.is_object()) { mLastError = std::string(layer) + " must be an object"; return false; }
            const bool isSelf = std::string(layer) == "self";
            if (isSelf) {
                if (!onlyKeys(section, {"outline", "capsule", "wireframe", "contrast"}, layer, mLastError)) return false;
            } else if (!onlyKeys(section, {"outline", "capsule", "wireframe", "contrast", "trail", "pointLight"}, layer, mLastError)) return false;
            if (section.contains("outline")) {
                json wrapper = json::object();
                wrapper[layer] = section.at("outline");
                PlayerOutlineSettings* target = &out.self;
                if (std::string(layer) == "enemy") target = &out.enemy;
                if (std::string(layer) == "teammate") target = &out.teammate;
                if (!readSettings(wrapper, layer, *target, mLastError)) return false;
            }
            PlayerCapsuleSettings* capsule = &out.selfCapsule;
            PlayerWireframeSettings* wire = &out.selfWireframe;
            PlayerContrastSettings* contrast = &out.selfContrast;
            PlayerTrailSettings* trail = nullptr;
            PlayerPointLightSettings* pointLight = nullptr;
            if (std::string(layer) == "enemy") { capsule = &out.enemyCapsule; wire = &out.enemyWireframe; }
            if (std::string(layer) == "enemy") { contrast = &out.enemyContrast; trail = &out.enemyTrail; pointLight = &out.enemyPointLight; }
            if (std::string(layer) == "teammate") { capsule = &out.teammateCapsule; wire = &out.teammateWireframe; contrast = &out.teammateContrast; trail = &out.teammateTrail; pointLight = &out.teammatePointLight; }
            if (std::string(layer) == "self") {
                if (section.contains("contrast") && !readContrast(section.at("contrast"), *contrast, "self.contrast", mLastError)) return false;
            } else {
                if (section.contains("contrast") && !readContrast(section.at("contrast"), *contrast, layer + std::string(".contrast"), mLastError)) return false;
                if (section.contains("trail") && !readTrail(section.at("trail"), *trail, layer + std::string(".trail"), mLastError)) return false;
                if (section.contains("pointLight") && !readPointLight(section.at("pointLight"), *pointLight, layer + std::string(".pointLight"), mLastError)) return false;
            }
            if (section.contains("capsule")) {
                const auto& c = section.at("capsule");
                if (!c.is_object()) { mLastError = layer + std::string(".capsule must be an object"); return false; }
                if (!onlyKeys(c, {"enabled", "geometrySource", "alpha", "color", "frontFaceCull", "backFaceCull", "depthTest", "depthWrite", "visibleThroughWalls", "scale"}, layer + std::string(".capsule"), mLastError)) return false;
                if (!validColor(c, "color", layer + std::string(".capsule.color"), mLastError)) return false;
                capsule->enabled = c.value("enabled", capsule->enabled);
                capsule->geometrySource = c.value("geometrySource", capsule->geometrySource);
                capsule->alpha = c.value("alpha", capsule->alpha);
                capsule->color = readColor(c, "color", capsule->color);
                capsule->frontFaceCull = c.value("frontFaceCull", capsule->frontFaceCull);
                capsule->backFaceCull = c.value("backFaceCull", capsule->backFaceCull);
                capsule->depthTest = c.value("depthTest", capsule->depthTest);
                capsule->depthWrite = c.value("depthWrite", capsule->depthWrite);
                capsule->visibleThroughWalls = c.value("visibleThroughWalls", capsule->visibleThroughWalls);
                capsule->scale = c.value("scale", capsule->scale);
                if (capsule->geometrySource != "collision" || !finite(capsule->alpha) ||
                    !finite(capsule->scale) || capsule->alpha < 0.0f || capsule->alpha > 1.0f ||
                    capsule->scale <= 0.0f || capsule->scale > 10.0f) {
                    mLastError = layer + std::string(".capsule supports geometrySource=collision, alpha 0..1, and scale 0..10");
                    return false;
                }
            }
            if (section.contains("wireframe")) {
                const auto& w = section.at("wireframe");
                if (!w.is_object()) { mLastError = layer + std::string(".wireframe must be an object"); return false; }
                if (!onlyKeys(w, {"enabled", "alpha", "color", "lineWidth", "visibleThroughWalls", "disappearOnDeath"}, layer + std::string(".wireframe"), mLastError)) return false;
                if (!validColor(w, "color", layer + std::string(".wireframe.color"), mLastError)) return false;
                wire->enabled = w.value("enabled", wire->enabled);
                wire->alpha = w.value("alpha", wire->alpha);
                wire->color = readColor(w, "color", wire->color);
                wire->lineWidth = w.value("lineWidth", wire->lineWidth);
                wire->visibleThroughWalls = w.value("visibleThroughWalls", wire->visibleThroughWalls);
                wire->disappearOnDeath = w.value("disappearOnDeath", wire->disappearOnDeath);
                if (!finite(wire->alpha) || !finite(wire->lineWidth) || wire->alpha < 0.0f ||
                    wire->alpha > 1.0f || wire->lineWidth <= 0.0f || wire->lineWidth > 8.0f) {
                    mLastError = layer + std::string(".wireframe alpha must be 0..1 and lineWidth 0..8");
                    return false;
                }
            }
        }
        return true;
    } catch (const std::exception& e) { mLastError = e.what(); return false; }
}

bool PlayerVisualsConfig::load() {
    std::ifstream f(mPath);
    if (!f) { Debug::warn(Debug::Category::Render, "[PLAYER VISUALS] missing %s; using defaults", mPath.string().c_str()); return false; }
    std::stringstream ss; ss << f.rdbuf();
    PlayerVisualsData candidate = mData;
    if (!parseAndValidate(ss.str(), candidate)) {
        mLastAttemptedWrite = writeTime(mPath);
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Rendering, StructuredLevel::Errors,
            "player_visuals.config.rejected", mPath.string(),
            "invalid player visuals config; previous settings remain active", 0,
            {{"path", mPath.string()}, {"error", mLastError}},
            __FILE__, __LINE__, __FUNCTION__);
        Debug::warn(Debug::Category::Render, "[PLAYER VISUALS] reload failed path=%s reason=%s; keeping previous valid state", mPath.string().c_str(), mLastError.c_str());
        return false;
    }
    mData = std::move(candidate); mLastWrite = writeTime(mPath); mLastAttemptedWrite = mLastWrite; mLastError.clear();
    Debug::log(Debug::Category::Render, "[PLAYER VISUALS] loaded path=%s enemyTrail=%d enemyLight=%d enemyContrast=%d",
        mPath.string().c_str(), (int)mData.enemyTrail.enabled, (int)mData.enemyPointLight.enabled,
        (int)mData.enemyContrast.enabled);
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Rendering, StructuredLevel::Important,
        "player_visuals.config.applied", mPath.string(),
        "validated player visuals config installed for existing actors", 0,
        {{"path", mPath.string()}, {"enemy_trail_enabled", mData.enemyTrail.enabled},
         {"enemy_point_light_enabled", mData.enemyPointLight.enabled},
         {"enemy_contrast_enabled", mData.enemyContrast.enabled},
         {"render_order", mData.renderOrder}},
        __FILE__, __LINE__, __FUNCTION__);
    return true;
}
bool PlayerVisualsConfig::reload() { return load(); }
bool PlayerVisualsConfig::pollReload() {
    const auto wt = writeTime(mPath);
    if (wt == std::filesystem::file_time_type{} || wt == mLastAttemptedWrite) return false;
    return load();
}
void PlayerVisualsConfig::setPlayerOutlinesEnabled(bool enabled) {
    mPlayerOutlinesOverride = true;
    mPlayerOutlinesEnabled = enabled;
}
void PlayerVisualsConfig::clearPlayerOutlinesOverride() {
    mPlayerOutlinesOverride = false;
    mPlayerOutlinesEnabled = true;
}
void PlayerVisualsConfig::setTeamOutlineColors(const glm::vec3& friendly, const glm::vec3& enemy) {
    mTeamOutlineColorsOverride = true;
    mFriendlyOutlineColor = friendly;
    mEnemyOutlineColor = enemy;
}
void PlayerVisualsConfig::clearTeamOutlineColorsOverride() {
    mTeamOutlineColorsOverride = false;
}
glm::vec3 PlayerVisualsConfig::effectiveOutlineColor(bool teammate, const glm::vec3& configured) const {
    if (!mTeamOutlineColorsOverride) return configured;
    return teammate ? mFriendlyOutlineColor : mEnemyOutlineColor;
}

void PlayerVisualsConfig::setPlayerWireframesEnabled(bool enabled) {
    mPlayerWireframesOverride = true;
    mPlayerWireframesEnabled = enabled;
}

void PlayerVisualsConfig::clearPlayerWireframesOverride() {
    mPlayerWireframesOverride = false;
    mFriendlyWireframesEnabled = false;
    mTeamWireframeColorsOverride = false;
}

void PlayerVisualsConfig::setTeamWireframeColors(const glm::vec3& friendly, const glm::vec3& enemy) {
    mTeamWireframeColorsOverride = true;
    mFriendlyWireframeColor = friendly;
    mEnemyWireframeColor = enemy;
}

void PlayerVisualsConfig::clearTeamWireframeColorsOverride() {
    mTeamWireframeColorsOverride = false;
}

glm::vec3 PlayerVisualsConfig::effectiveWireframeColor(bool teammate, const glm::vec3& configured) const {
    if (!mTeamWireframeColorsOverride) return configured;
    return teammate ? mFriendlyWireframeColor : mEnemyWireframeColor;
}
void PlayerVisualsConfig::setFriendlyOutlinesEnabled(bool enabled) { mFriendlyOutlinesEnabled = enabled; }
void PlayerVisualsConfig::setEnemyOutlineAlpha(float alpha) {
    mEnemyOutlineAlphaOverride = true;
    mEnemyOutlineAlpha = std::clamp(alpha, 0.0f, 1.0f);
}
void PlayerVisualsConfig::clearEnemyOutlineAlphaOverride() { mEnemyOutlineAlphaOverride = false; }
float PlayerVisualsConfig::effectiveEnemyOutlineAlpha(float configured) const {
    return mEnemyOutlineAlphaOverride ? mEnemyOutlineAlpha : configured;
}
std::string PlayerVisualsConfig::describe() const {
    std::ostringstream s; s << "player visuals enemy=" << (mData.enemy.enabled ? "on" : "off")
      << " teammate=" << (mData.teammate.enabled ? "on" : "off")
      << " self=" << (mData.self.enabled ? "on" : "off"); return s.str();
}
