// aug 18 2026, 14 30
/* purpose
* Declares the JSON-driven dynamic light config (per-weapon muzzle flash lights).
* Controls enabled, color, intensity, radius, lifetime, fade, randomization.
* Reloads on file change via pollReload (runs every frame like other gameplay configs).
* Does NOT spawn or render lights; DynamicLightManager reads this config.
* Does NOT own the directional lighting or ambient config.
*/
#pragma once

#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

struct WeaponLightSettings {
    bool enabled = true;
    glm::vec3 color{1.0f, 0.72f, 0.35f};
    float intensity = 1.8f;
    float radius = 8.0f;
    float lifetime = 0.055f;
    float fadeIn = 0.0f;
    float fadeOut = 0.055f;
    glm::vec3 positionOffset{0.0f};
    float randomIntensityMin = 0.8f;
    float randomIntensityMax = 1.15f;
    float randomRadiusMin = 0.85f;
    float randomRadiusMax = 1.10f;
};

struct DynamicLightConfigData {
    int maxActive = 32;
    int maxPerFrame = 8;
    float minIntensityCull = 0.01f;
    float minRadiusCull = 0.1f;
    std::string quality = "smooth";

    // weaponId -> effectName -> settings
    std::unordered_map<std::string,
        std::unordered_map<std::string, WeaponLightSettings>> weaponLights;
};

class DynamicLightConfig {
public:
    static DynamicLightConfig& instance();

    bool load(const std::string& path = "config/lighting.json");
    bool reload();
    bool pollReload();

    const DynamicLightConfigData& data() const { return mData; }

    const WeaponLightSettings& weaponLight(const std::string& weaponId,
                                           const std::string& effectName) const;

private:
    DynamicLightConfig() = default;

    DynamicLightConfigData mData;
    std::string mPath = "config/lighting.json";
    int64_t mLastModified = 0;

    static const WeaponLightSettings sEmptyWeaponLight;
};
