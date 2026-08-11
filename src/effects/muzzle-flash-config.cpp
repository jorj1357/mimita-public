// 08 11 2026, 11 30
/* purpose
* Loads and hot-reloads config/effects/muzzle-flash.json for the single-tick muzzle flash.
* Mirrors the crosshair-config pattern: stat the file mtime, reload on change.
* Does NOT spawn or render effects; EffectPartSystem::spawnMuzzleFlash consumes the data.
* Does NOT own size scaling or any other gameplay config.
*/
#include "muzzle-flash-config.h"

#include <chrono>
#include <filesystem>
#include <fstream>

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

MuzzleFlashConfig& MuzzleFlashConfig::instance()
{
    static MuzzleFlashConfig config;
    return config;
}

bool MuzzleFlashConfig::load(const std::string& path)
{
    mPath = path;
    std::ifstream file(path);
    if (!file.is_open()) {
        Debug::warn(Debug::Category::Weapons,
            "[MUZZLE FLASH] No config file: %s (using defaults)\n", path.c_str());
        mLastModified = modifiedTime(path);
        return false;
    }

    try {
        json j;
        file >> j;
        MuzzleFlashSettings d;
        d.enabled = j.value("enabled", d.enabled);
        if (j.contains("color") && j["color"].is_array() && j["color"].size() >= 3) {
            d.color.r = j["color"][0].get<float>();
            d.color.g = j["color"][1].get<float>();
            d.color.b = j["color"][2].get<float>();
        }
        d.lifetime = j.value("lifetime", d.lifetime);
        d.scale = j.value("scale", d.scale);
        d.endScale = j.value("endScale", d.endScale);
        d.alpha = j.value("alpha", d.alpha);
        mData = d;
        mLastModified = modifiedTime(path);
        return true;
    } catch (...) {
        Debug::warn(Debug::Category::Weapons,
            "[MUZZLE FLASH] Error parsing %s (keeping previous config)\n", path.c_str());
        return false;
    }
}

bool MuzzleFlashConfig::reload()
{
    return load(mPath);
}

bool MuzzleFlashConfig::pollReload()
{
    const int64_t current = modifiedTime(mPath);
    return current != 0 && current != mLastModified && reload();
}
