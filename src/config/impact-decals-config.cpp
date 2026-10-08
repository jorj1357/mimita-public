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
#include <cmath>
#include <sstream>

#include <nlohmann/json.hpp>

#include "debug/debug-log.h"
#include "debug/structured-log.h"

using json = nlohmann::json;

namespace {

std::string fileFingerprint(const std::string& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) return "missing";

    uint64_t hash = 1469598103934665603ull;
    uint64_t bytes = 0;
    char buffer[4096];
    while (input.read(buffer, sizeof(buffer)) || input.gcount() > 0) {
        const std::streamsize count = input.gcount();
        bytes += static_cast<uint64_t>(count);
        for (std::streamsize i = 0; i < count; ++i) {
            hash ^= static_cast<unsigned char>(buffer[i]);
            hash *= 1099511628211ull;
        }
    }

    std::ostringstream result;
    result << bytes << ":" << std::hex << hash;
    return result.str();
}

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
    cfg.gravity = readJsonFloat(s, "gravity", cfg.gravity);
    if (!std::isfinite(cfg.gravity) || cfg.gravity < 0.0f)
        cfg.gravity = 0.0f;
    if (s.contains("debris")) {
        const auto& d = s["debris"];
        cfg.debris.enabled = readJsonBool(d, "enabled", cfg.debris.enabled);
        cfg.debris.countFraction = readJsonFloat(d, "countFraction", cfg.debris.countFraction);
        cfg.debris.color = readJsonVec3(d, "color", cfg.debris.color);
        cfg.debris.alpha = readJsonFloat(d, "alpha", cfg.debris.alpha);
        cfg.debris.sizeMin = readJsonFloat(d, "sizeMin", cfg.debris.sizeMin);
        cfg.debris.sizeMax = readJsonFloat(d, "sizeMax", cfg.debris.sizeMax);
        cfg.debris.sizeJitter = readJsonFloat(d, "sizeJitter", cfg.debris.sizeJitter);
        cfg.debris.spawnOffset = readJsonFloat(d, "spawnOffset", cfg.debris.spawnOffset);
        cfg.debris.speedMin = readJsonFloat(d, "speedMin", cfg.debris.speedMin);
        cfg.debris.speedMax = readJsonFloat(d, "speedMax", cfg.debris.speedMax);
        cfg.debris.verticalVelocityMin = readJsonFloat(d, "verticalVelocityMin", cfg.debris.verticalVelocityMin);
        cfg.debris.verticalVelocityMax = readJsonFloat(d, "verticalVelocityMax", cfg.debris.verticalVelocityMax);
        cfg.debris.coneDegreesMin = readJsonFloat(d, "coneDegreesMin", cfg.debris.coneDegreesMin);
        cfg.debris.coneDegreesMax = readJsonFloat(d, "coneDegreesMax", cfg.debris.coneDegreesMax);
        cfg.debris.lifetimeMin = readJsonFloat(d, "lifetimeMin", cfg.debris.lifetimeMin);
        cfg.debris.lifetimeMax = readJsonFloat(d, "lifetimeMax", cfg.debris.lifetimeMax);
        cfg.debris.gravity = readJsonFloat(d, "gravity", cfg.debris.gravity);
        cfg.debris.drag = readJsonFloat(d, "drag", cfg.debris.drag);
        cfg.debris.affectedByGravity = readJsonBool(d, "affectedByGravity", cfg.debris.affectedByGravity);
        cfg.debris.rotationRandomDegrees = readJsonFloat(d, "rotationRandomDegrees", cfg.debris.rotationRandomDegrees);
        cfg.debris.angularSpeedMin = readJsonFloat(d, "angularSpeedMin", cfg.debris.angularSpeedMin);
        cfg.debris.angularSpeedMax = readJsonFloat(d, "angularSpeedMax", cfg.debris.angularSpeedMax);
    }
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
    cfg.renderDistance = readJsonFloat(j, "renderDistance", cfg.renderDistance);
    cfg.renderFadeStartDistance = readJsonFloat(j, "renderFadeStartDistance", cfg.renderFadeStartDistance);
    cfg.renderFadeEndDistance = readJsonFloat(j, "renderFadeEndDistance", cfg.renderFadeEndDistance);
    cfg.maxCount = readJsonInt(j, "maxCount", cfg.maxCount);
    if (j.contains("texture") && j["texture"].is_string())
        cfg.texture = j["texture"].get<std::string>();
    cfg.textureScale = readJsonFloat(j, "textureScale", cfg.textureScale);
    cfg.randomRotationDegrees = readJsonFloat(j, "randomRotationDegrees", cfg.randomRotationDegrees);
    if (j.contains("stagger")) {
        const auto& s = j["stagger"];
        cfg.stagger.enabled = readJsonBool(s, "enabled", cfg.stagger.enabled);
        cfg.stagger.decalsPerTick = readJsonInt(s, "decals_per_tick", cfg.stagger.decalsPerTick);
        cfg.stagger.startDelayTicks = readJsonInt(s, "start_delay_ticks", cfg.stagger.startDelayTicks);
        cfg.stagger.maxTicks = readJsonInt(s, "max_ticks", cfg.stagger.maxTicks);
        cfg.stagger.rayBudgetPerFrame = readJsonInt(s, "ray_budget_per_frame", cfg.stagger.rayBudgetPerFrame);
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
    if (j.contains("clientFeedback")) {
        const auto& f = j["clientFeedback"];
        cfg.clientFeedback.enabled = readJsonBool(f, "enabled", cfg.clientFeedback.enabled);
        cfg.clientFeedback.minCount = readJsonInt(f, "minCount", cfg.clientFeedback.minCount);
        cfg.clientFeedback.maxCount = readJsonInt(f, "maxCount", cfg.clientFeedback.maxCount);
        cfg.clientFeedback.damageAtMax = readJsonFloat(f, "damageAtMax", cfg.clientFeedback.damageAtMax);
        cfg.clientFeedback.forceAtMax = readJsonFloat(f, "forceAtMax", cfg.clientFeedback.forceAtMax);
        cfg.clientFeedback.forwardOffsetMin = readJsonFloat(f, "forwardOffsetMin", cfg.clientFeedback.forwardOffsetMin);
        cfg.clientFeedback.forwardOffsetMax = readJsonFloat(f, "forwardOffsetMax", cfg.clientFeedback.forwardOffsetMax);
        cfg.clientFeedback.rightOffset = readJsonFloat(f, "rightOffset", cfg.clientFeedback.rightOffset);
        cfg.clientFeedback.upOffset = readJsonFloat(f, "upOffset", cfg.clientFeedback.upOffset);
        cfg.clientFeedback.sizeMin = readJsonFloat(f, "sizeMin", cfg.clientFeedback.sizeMin);
        cfg.clientFeedback.sizeMax = readJsonFloat(f, "sizeMax", cfg.clientFeedback.sizeMax);
        cfg.clientFeedback.lifetimeMin = readJsonFloat(f, "lifetimeMin", cfg.clientFeedback.lifetimeMin);
        cfg.clientFeedback.lifetimeMax = readJsonFloat(f, "lifetimeMax", cfg.clientFeedback.lifetimeMax);
        cfg.clientFeedback.alphaMin = readJsonFloat(f, "alphaMin", cfg.clientFeedback.alphaMin);
        cfg.clientFeedback.alphaMax = readJsonFloat(f, "alphaMax", cfg.clientFeedback.alphaMax);
        cfg.clientFeedback.color = readJsonVec3(f, "color", cfg.clientFeedback.color);
    }
    readForce(j, cfg.force);
}

} // anonymous namespace

ImpactDecalsConfig& ImpactDecalsConfig::instance()
{
    static ImpactDecalsConfig config;
    return config;
}

void ImpactDecalsConfig::setRuntimeBloodEnabled(bool enabled)
{
    mRuntimeBloodOverride = true;
    mRuntimeBloodEnabled = enabled;
    mData.blood.enabled = enabled;
}

void ImpactDecalsConfig::clearRuntimeBloodOverride()
{
    if (!mRuntimeBloodOverride) return;
    mRuntimeBloodOverride = false;
    // Re-read the persistent JSON value instead of restoring a stale value
    // captured when the match began. Other impact settings remain untouched.
    load(mPath);
}

bool ImpactDecalsConfig::load(const std::string& path)
{
    if (mPath != path)
        mPath = path;

    const std::string fileName = fileNameOf(mPath);
    const auto writeTime = getLastWrite(mPath);
    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        "impact_decals.config_read", "impact-decals-config", "load_or_reload", 0,
        nlohmann::json{
            {"path", mPath}, {"file", fileName},
            {"fingerprint", fileFingerprint(mPath)},
            {"runtime_blood_override", mRuntimeBloodOverride},
            {"runtime_blood_enabled", mRuntimeBloodEnabled}
        }, __FILE__, __LINE__, __FUNCTION__);
    std::ifstream file(mPath);
    if (!file.is_open()) {
        mLastWrite = writeTime;
        Debug::warn(Debug::Category::Weapons,
            "[IMPACT DECALS] Missing %s; using defaults.\n", mPath.c_str());
        return false;
    }

    try {
        json root;
        root = json::parse(file, nullptr, true, true);

        ImpactDecalsData data;
        data.enabled = readJsonBool(root, "enabled", data.enabled);
        if (root.contains("blood"))
            readGroup(root["blood"], data.blood);
        if (root.contains("bulletHoles"))
            readGroup(root["bulletHoles"], data.bulletHoles);
        if (root.contains("worldCracks"))
            readGroup(root["worldCracks"], data.worldCracks);

        mData = data;
        if (mRuntimeBloodOverride)
            mData.blood.enabled = mRuntimeBloodEnabled;
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
    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        "impact_decals.config_change_detected", "impact-decals-config",
        "file_timestamp_changed", 0,
        nlohmann::json{
            {"path", mPath}, {"fingerprint", fileFingerprint(mPath)},
            {"runtime_blood_override", mRuntimeBloodOverride},
            {"runtime_blood_enabled", mRuntimeBloodEnabled}
        }, __FILE__, __LINE__, __FUNCTION__);
    return load(mPath);
}
