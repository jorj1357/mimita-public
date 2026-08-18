// aug 18 2026, 14 30
/* purpose
* Loads and hot-reloads dynamic light config from config/lighting.json.
* Reads the "dynamicLights" and "weaponLights" sections.
* Mirrors the LightingConfig pattern: stat the file mtime, reload on change.
* Does NOT spawn or render lights; DynamicLightManager consumes the data.
* Does NOT own the directional lighting or ambient config.
*/
#include "dynamic-light-config.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <chrono>

#include <nlohmann/json.hpp>

#include "debug/debug-log.h"

using json = nlohmann::json;

static int64_t modifiedTime(const std::string& path)
{
    std::error_code ec;
    const auto time = std::filesystem::last_write_time(path, ec);
    if (ec) return 0;
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        time.time_since_epoch()).count();
}

const WeaponLightSettings DynamicLightConfig::sEmptyWeaponLight{};

DynamicLightConfig& DynamicLightConfig::instance()
{
    static DynamicLightConfig config;
    return config;
}

static WeaponLightSettings parseWeaponLight(const json& j)
{
    WeaponLightSettings w;
    w.enabled = j.value("enabled", w.enabled);
    if (j.contains("color") && j["color"].is_array() && j["color"].size() >= 3) {
        w.color.r = j["color"][0].get<float>();
        w.color.g = j["color"][1].get<float>();
        w.color.b = j["color"][2].get<float>();
    }
    w.intensity = j.value("intensity", w.intensity);
    w.radius = j.value("radius", w.radius);
    w.lifetime = j.value("lifetime", w.lifetime);
    w.fadeIn = j.value("fadeIn", w.fadeIn);
    w.fadeOut = j.value("fadeOut", w.fadeOut);
    if (j.contains("positionOffset") && j["positionOffset"].is_array() && j["positionOffset"].size() >= 3) {
        w.positionOffset.r = j["positionOffset"][0].get<float>();
        w.positionOffset.g = j["positionOffset"][1].get<float>();
        w.positionOffset.b = j["positionOffset"][2].get<float>();
    }
    w.randomIntensityMin = j.value("randomIntensityMin", w.randomIntensityMin);
    w.randomIntensityMax = j.value("randomIntensityMax", w.randomIntensityMax);
    w.randomRadiusMin = j.value("randomRadiusMin", w.randomRadiusMin);
    w.randomRadiusMax = j.value("randomRadiusMax", w.randomRadiusMax);
    return w;
}

bool DynamicLightConfig::load(const std::string& path)
{
    mPath = path;
    std::ifstream file(path);
    if (!file.is_open()) {
        Debug::warn(Debug::Category::Render,
            "[DYNAMIC LIGHT CONFIG] No config file: %s (using defaults)\n", path.c_str());
        mLastModified = modifiedTime(path);
        return false;
    }

    try {
        json j;
        file >> j;

        DynamicLightConfigData d;

        if (j.contains("dynamicLights")) {
            auto& dl = j["dynamicLights"];
            d.maxActive = dl.value("maxActive", d.maxActive);
            d.maxPerFrame = dl.value("maxPerFrame", d.maxPerFrame);
            d.minIntensityCull = dl.value("minIntensityCull", d.minIntensityCull);
            d.minRadiusCull = dl.value("minRadiusCull", d.minRadiusCull);
            d.quality = dl.value("quality", d.quality);
        }

        if (j.contains("weaponLights") && j["weaponLights"].is_object()) {
            for (auto& [weaponId, weaponNode] : j["weaponLights"].items()) {
                if (!weaponNode.is_object()) continue;
                for (auto& [effectName, effectNode] : weaponNode.items()) {
                    if (!effectNode.is_object()) continue;
                    d.weaponLights[weaponId][effectName] = parseWeaponLight(effectNode);
                }
            }
        }

        mData = d;
        mLastModified = modifiedTime(path);
        Debug::log(Debug::Category::Render,
            "[DYNAMIC LIGHT CONFIG] Loaded config from %s\n", path.c_str());
        return true;

    } catch (const std::exception& e) {
        Debug::warn(Debug::Category::Render,
            "[DYNAMIC LIGHT CONFIG] Error loading %s: %s\n", path.c_str(), e.what());
        return false;
    }
}

bool DynamicLightConfig::reload()
{
    return load(mPath);
}

bool DynamicLightConfig::pollReload()
{
    const int64_t current = modifiedTime(mPath);
    return current != 0 && current != mLastModified && reload();
}

const WeaponLightSettings& DynamicLightConfig::weaponLight(
    const std::string& weaponId, const std::string& effectName) const
{
    auto wIt = mData.weaponLights.find(weaponId);
    if (wIt == mData.weaponLights.end()) return sEmptyWeaponLight;
    auto eIt = wIt->second.find(effectName);
    if (eIt == wIt->second.end()) return sEmptyWeaponLight;
    return eIt->second;
}
