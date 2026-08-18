// 08 14 2026, 09 15
/* purpose
* Loads config/impact_decals.json into ImpactDecalsData at startup and on file change.
* Owns defaults and hot reload polling for blood, bullet hole, and world crack decals.
* Does NOT spawn or render effects.
* Does NOT own hit detection or damage logic.
*/
#include "config/impact-decals-config.h"

#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

#include "debug/debug-log.h"

using json = nlohmann::json;

namespace {

std::filesystem::file_time_type getLastWrite(const std::string& path)
{
    std::error_code ec;
    const auto time = std::filesystem::last_write_time(path, ec);
    return ec ? std::filesystem::file_time_type{} : time;
}

std::string fileNameOf(const std::string& path)
{
    return std::filesystem::path(path).filename().string();
}

template<typename T>
static T readJsonFloat(const json& j, const char* key, T def)
{
    return j.contains(key) ? j[key].get<T>() : def;
}

static bool readJsonBool(const json& j, const char* key, bool def)
{
    return j.contains(key) ? j[key].get<bool>() : def;
}

static int readJsonInt(const json& j, const char* key, int def)
{
    return j.contains(key) ? j[key].get<int>() : def;
}

static glm::vec3 readJsonVec3(const json& j, const char* key, glm::vec3 def)
{
    if (!j.contains(key)) return def;
    const auto& arr = j[key];
    if (arr.is_array() && arr.size() >= 3)
        return {arr[0].get<float>(), arr[1].get<float>(), arr[2].get<float>()};
    return def;
}

static void readSpray(const json& j, ImpactDecalSprayConfig& cfg)
{
    if (!j.contains("spray")) return;
    const auto& s = j["spray"];
    cfg.enabled = readJsonBool(s, "enabled", cfg.enabled);
    cfg.minCount = readJsonInt(s, "minCount", cfg.minCount);
    cfg.maxCount = readJsonInt(s, "maxCount", cfg.maxCount);
    cfg.sizeMin = readJsonFloat(s, "sizeMin", cfg.sizeMin);
    cfg.sizeMax = readJsonFloat(s, "sizeMax", cfg.sizeMax);
    cfg.bigFraction = readJsonFloat(s, "bigFraction", cfg.bigFraction);
    cfg.speedMin = readJsonFloat(s, "speedMin", cfg.speedMin);
    cfg.speedMax = readJsonFloat(s, "speedMax", cfg.speedMax);
    cfg.coneDegreesMin = readJsonFloat(s, "coneDegreesMin", cfg.coneDegreesMin);
    cfg.coneDegreesMax = readJsonFloat(s, "coneDegreesMax", cfg.coneDegreesMax);
    cfg.lifetimeMin = readJsonFloat(s, "lifetimeMin", cfg.lifetimeMin);
    cfg.lifetimeMax = readJsonFloat(s, "lifetimeMax", cfg.lifetimeMax);
    cfg.alphaMin = readJsonFloat(s, "alphaMin", cfg.alphaMin);
    cfg.alphaMax = readJsonFloat(s, "alphaMax", cfg.alphaMax);
}

static void readForce(const json& j, ImpactForceConfig& cfg)
{
    if (!j.contains("force")) return;
    const auto& f = j["force"];
    cfg.maxDistance = readJsonFloat(f, "maxDistance", cfg.maxDistance);
    cfg.minDistance = readJsonFloat(f, "minDistance", cfg.minDistance);
    cfg.minForce = readJsonFloat(f, "minForce", cfg.minForce);
}

static void readGroup(const json& j, ImpactDecalGroupConfig& cfg)
{
    cfg.enabled = readJsonBool(j, "enabled", cfg.enabled);
    cfg.count = readJsonInt(j, "count", cfg.count);
    cfg.minCount = readJsonInt(j, "minCount", cfg.minCount);
    cfg.coneDegrees = readJsonFloat(j, "coneDegrees", cfg.coneDegrees);
    cfg.coneDistance = readJsonFloat(j, "coneDistance", cfg.coneDistance);
    cfg.radius = readJsonFloat(j, "radius", cfg.radius);
    cfg.minRadius = readJsonFloat(j, "minRadius", cfg.minRadius);
    cfg.height = readJsonFloat(j, "height", cfg.height);
    cfg.length = readJsonFloat(j, "length", cfg.length);
    cfg.thickness = readJsonFloat(j, "thickness", cfg.thickness);
    cfg.color = readJsonVec3(j, "color", cfg.color);
    cfg.colorVariation = readJsonFloat(j, "colorVariation", cfg.colorVariation);
    cfg.alpha = readJsonFloat(j, "alpha", cfg.alpha);
    cfg.lifetime = readJsonFloat(j, "lifetime", cfg.lifetime);
    cfg.fadeTime = readJsonFloat(j, "fadeTime", cfg.fadeTime);
    cfg.maxCount = readJsonInt(j, "maxCount", cfg.maxCount);
    if (j.contains("stagger")) {
        const auto& s = j["stagger"];
        cfg.stagger.enabled = readJsonBool(s, "enabled", cfg.stagger.enabled);
        cfg.stagger.decalsPerTick = readJsonInt(s, "decals_per_tick", cfg.stagger.decalsPerTick);
        cfg.stagger.startDelayTicks = readJsonInt(s, "start_delay_ticks", cfg.stagger.startDelayTicks);
        cfg.stagger.maxTicks = readJsonInt(s, "max_ticks", cfg.stagger.maxTicks);
    }
    if (j.contains("color_over_lifetime")) {
        const auto& c = j["color_over_lifetime"];
        cfg.colorOverLifetime.enabled = readJsonBool(c, "enabled", cfg.colorOverLifetime.enabled);
        cfg.colorOverLifetime.startColor = readJsonVec3(c, "start_color", cfg.colorOverLifetime.startColor);
        cfg.colorOverLifetime.endColor = readJsonVec3(c, "end_color", cfg.colorOverLifetime.endColor);
        cfg.colorOverLifetime.darkenStartSeconds = readJsonFloat(c, "darken_start_seconds", cfg.colorOverLifetime.darkenStartSeconds);
        cfg.colorOverLifetime.darkenEndSeconds = readJsonFloat(c, "darken_end_seconds", cfg.colorOverLifetime.darkenEndSeconds);
    }
    if (j.contains("arms")) {
        const auto& a = j["arms"];
        cfg.crackArms.baseCount = readJsonInt(a, "base_count", cfg.crackArms.baseCount);
        cfg.crackArms.weaponForceMultiplier = readJsonFloat(a, "weapon_force_multiplier", cfg.crackArms.weaponForceMultiplier);
        cfg.crackArms.maxCount = readJsonInt(a, "max_count", cfg.crackArms.maxCount);
    }
    if (j.contains("chain")) {
        const auto& c = j["chain"];
        cfg.crackChain.minSegments = readJsonInt(c, "min_segments", cfg.crackChain.minSegments);
        cfg.crackChain.maxSegments = readJsonInt(c, "max_segments", cfg.crackChain.maxSegments);
        cfg.crackChain.segmentLengthMin = readJsonFloat(c, "segment_length_min", cfg.crackChain.segmentLengthMin);
        cfg.crackChain.segmentLengthMax = readJsonFloat(c, "segment_length_max", cfg.crackChain.segmentLengthMax);
        cfg.crackChain.turnDegreesMin = readJsonFloat(c, "turn_degrees_min", cfg.crackChain.turnDegreesMin);
        cfg.crackChain.turnDegreesMax = readJsonFloat(c, "turn_degrees_max", cfg.crackChain.turnDegreesMax);
        cfg.crackChain.jaggedness = readJsonFloat(c, "jaggedness", cfg.crackChain.jaggedness);
    }
    cfg.crackCenterThickness = readJsonFloat(j, "center_thickness", cfg.crackCenterThickness);
    cfg.crackOuterThickness = readJsonFloat(j, "outer_thickness", cfg.crackOuterThickness);
    readSpray(j, cfg.spray);
    readForce(j, cfg.force);
}

} // anonymous namespace

ImpactDecalsConfig& ImpactDecalsConfig::instance()
{
    static ImpactDecalsConfig config;
    return config;
}

bool ImpactDecalsConfig::load(const std::string& path)
{
    if (mPath != path)
        mPath = path;

    const std::string fileName = fileNameOf(mPath);
    const auto writeTime = getLastWrite(mPath);
    std::ifstream file(mPath);
    if (!file.is_open()) {
        mLastWrite = writeTime;
        Debug::warn(Debug::Category::Weapons,
            "[IMPACT DECALS] Missing %s; using defaults.\n", mPath.c_str());
        return false;
    }

    try {
        json root;
        file >> root;

        ImpactDecalsData data;
        data.enabled = readJsonBool(root, "enabled", data.enabled);
        if (root.contains("blood"))
            readGroup(root["blood"], data.blood);
        if (root.contains("bulletHoles"))
            readGroup(root["bulletHoles"], data.bulletHoles);
        if (root.contains("worldCracks"))
            readGroup(root["worldCracks"], data.worldCracks);

        mData = data;
        mLastWrite = writeTime;
        Debug::warn(Debug::Category::Weapons,
            "[IMPACT DECALS] Loaded: %s (blood=%d bulletHoles=%d worldCracks=%d)\n",
            fileName.c_str(),
            (int)mData.blood.enabled,
            (int)mData.bulletHoles.enabled,
            (int)mData.worldCracks.enabled);
        return true;
    } catch (const json::parse_error& e) {
        mLastWrite = writeTime;
        Debug::error(Debug::Category::Weapons,
            "[IMPACT DECALS] Parse error in %s: %s\n", mPath.c_str(), e.what());
    } catch (const std::exception& e) {
        mLastWrite = writeTime;
        Debug::error(Debug::Category::Weapons,
            "[IMPACT DECALS] Error loading %s: %s\n", mPath.c_str(), e.what());
    }
    return false;
}

bool ImpactDecalsConfig::pollReload()
{
    const auto writeTime = getLastWrite(mPath);
    if (writeTime == std::filesystem::file_time_type{} || writeTime == mLastWrite)
        return false;

    Debug::warn(Debug::Category::Weapons,
        "[IMPACT DECALS] Detected change: %s\n", fileNameOf(mPath).c_str());
    return load(mPath);
}
