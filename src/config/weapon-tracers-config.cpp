// 08 08 2026, 16 20
/* purpose
* Implements the hot-reloadable per-weapon tracer config.
* Loads config/weapon-tracers.json (defaults + per-weapon overrides) and
* re-applies it on mtime change (mirrors weapon-hitfx-config).
* Does NOT render tracers or own weapon behavior.
*/

#include "config/weapon-tracers-config.h"

#include "debug/debug-log.h"

#include <algorithm>
#include <chrono>
#include <fstream>

#include <nlohmann/json.hpp>

namespace {
using Clock = std::chrono::steady_clock;

WeaponTracerConfig readTracerConfig(const nlohmann::json& j,
                                    const WeaponTracerConfig& fallback)
{
    WeaponTracerConfig c = fallback;
    if (j.contains("enabled") && j["enabled"].is_boolean())
        c.enabled = j["enabled"].get<bool>();
    if (j.contains("thickness") && j["thickness"].is_number())
    {
        c.thickness = j["thickness"].get<float>();
        c.thicknessSet = true;
    }
    if (j.contains("end_thickness") && j["end_thickness"].is_number())
        c.endThickness = j["end_thickness"].get<float>();
    if (j.contains("lifetime") && j["lifetime"].is_number())
        c.lifetime = j["lifetime"].get<float>();
    if (j.contains("scale") && j["scale"].is_number())
        c.scale = j["scale"].get<float>();
    if (j.contains("end_scale") && j["end_scale"].is_number())
        c.endScale = j["end_scale"].get<float>();
    if (j.contains("start_alpha") && j["start_alpha"].is_number())
        c.startAlpha = j["start_alpha"].get<float>();
    if (j.contains("end_alpha") && j["end_alpha"].is_number())
        c.endAlpha = j["end_alpha"].get<float>();
    if (j.contains("color") && j["color"].is_array() && j["color"].size() >= 3)
        c.color = glm::vec3(j["color"][0].get<float>(),
                            j["color"][1].get<float>(),
                            j["color"][2].get<float>());
    return c;
}
} // namespace

WeaponTracersConfig& WeaponTracersConfig::instance()
{
    static WeaponTracersConfig cfg;
    return cfg;
}

bool WeaponTracersConfig::load(const std::string& path)
{
    mPath = path;
    std::error_code ec;
    const auto mtime = std::filesystem::last_write_time(mPath, ec);
    if (ec)
    {
        if (!mWatchLogged)
        {
            Debug::warn(Debug::Category::Weapons,
                        "[WEAPON TRACERS] config file not found: %s — using defaults\n",
                        path.c_str());
            mWatchLogged = true;
        }
        return false;
    }

    std::ifstream file(mPath);
    if (!file.is_open())
        return false;

    nlohmann::json root;
    try
    {
        file >> root;
    }
    catch (const std::exception&)
    {
        Debug::warn(Debug::Category::Weapons,
                    "[WEAPON TRACERS] malformed JSON in %s — keeping previous settings\n",
                    path.c_str());
        return false;
    }

    WeaponTracerConfig defaults = mDefaults;
    if (root.contains("defaults") && root["defaults"].is_object())
        defaults = readTracerConfig(root["defaults"], defaults);

    std::unordered_map<std::string, WeaponTracerConfig> perWeapon;
    for (auto it = root.begin(); it != root.end(); ++it)
    {
        if (it.key() == "version" || it.key() == "comment" || it.key() == "defaults")
            continue;
        if (!it.value().is_object())
            continue;
        perWeapon[it.key()] = readTracerConfig(it.value(), defaults);
    }

    mDefaults = defaults;
    mPerWeapon = std::move(perWeapon);
    mLastWrite = mtime;
    Debug::log(Debug::Category::Weapons,
               "[WEAPON TRACERS] loaded %zu weapon tracer configs from %s\n",
               mPerWeapon.size(), path.c_str());
    return true;
}

bool WeaponTracersConfig::pollReload()
{
    std::error_code ec;
    const auto mtime = std::filesystem::last_write_time(mPath, ec);
    if (ec)
        return false;
    if (mtime == mLastWrite)
        return false;
    Debug::log(Debug::Category::Weapons,
               "[WEAPON TRACERS] hot reloading %s\n", mPath.c_str());
    return load(mPath);
}

const WeaponTracerConfig& WeaponTracersConfig::forWeapon(const std::string& weaponId) const
{
    if (weaponId.empty())
        return mDefaults;
    auto it = mPerWeapon.find(weaponId);
    if (it == mPerWeapon.end())
        return mDefaults;
    return it->second;
}
