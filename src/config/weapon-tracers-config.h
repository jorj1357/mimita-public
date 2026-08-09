// 08 08 2026, 16 20
/* purpose
* Hot-reloadable per-weapon bullet tracer visuals (thickness, alpha, color, lifetime).
* Mirrors the weapon-cool-shot-line / weapon_hitfx config pattern so the tracer
* look can be tweaked from config/weapon-tracers.json while the game runs.
* Does NOT decide where tracers spawn, who owns shots, or hit validation.
* Does NOT write files, send packets, or render.
*/

#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

#include <glm/glm.hpp>

struct WeaponTracerConfig {
    bool enabled = true;
    bool thicknessSet = false; // true when the JSON explicitly set thickness
    float thickness = 0.2f;
    float endThickness = 0.0f;
    float lifetime = 0.5f;
    float scale = 0.2f;
    float endScale = 0.0f;
    float startAlpha = 1.0f;
    float endAlpha = 0.0f;
    glm::vec3 color{1.0f, 0.82f, 0.05f};
};

class WeaponTracersConfig {
public:
    static WeaponTracersConfig& instance();

    bool load(const std::string& path = "config/weapon-tracers.json");
    bool pollReload();

    const WeaponTracerConfig& defaultConfig() const { return mDefaults; }
    const WeaponTracerConfig& forWeapon(const std::string& weaponId) const;

private:
    WeaponTracersConfig() = default;

    WeaponTracerConfig mDefaults;
    std::unordered_map<std::string, WeaponTracerConfig> mPerWeapon;
    std::string mPath = "config/weapon-tracers.json";
    std::filesystem::file_time_type mLastWrite{};
    bool mWatchLogged = false;
};
