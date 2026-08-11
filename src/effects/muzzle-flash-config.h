// 08 11 2026, 11 30
/* purpose
* Declares the JSON-driven muzzle flash config (single-tick white sphere at the muzzle tip).
* Controls enabled, color, lifetime, scale, endScale, and alpha.
* Reloads on file change via pollReload (runs every frame like other gameplay configs).
* Does NOT spawn or render effects; EffectPartSystem::spawnMuzzleFlash reads this config.
* Does NOT own size scaling or hit-fx logic.
*/
#pragma once

#include <cstdint>
#include <string>

#include <glm/glm.hpp>

struct MuzzleFlashSettings {
    bool enabled = true;
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    float lifetime = 1.0f / 60.0f;  // single tick at 60 Hz
    float scale = 0.15f;
    float endScale = 0.15f;
    float alpha = 1.0f;
};

class MuzzleFlashConfig {
public:
    static MuzzleFlashConfig& instance();

    bool load(const std::string& path = "config/effects/muzzle-flash.json");
    bool reload();
    bool pollReload();

    const MuzzleFlashSettings& data() const { return mData; }

private:
    MuzzleFlashConfig() = default;

    MuzzleFlashSettings mData;
    std::string mPath = "config/effects/muzzle-flash.json";
    int64_t mLastModified = 0;
};
