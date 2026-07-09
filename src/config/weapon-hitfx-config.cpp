#include "config/weapon-hitfx-config.h"

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

static glm::vec3 readJsonVec3(const json& j, const char* key, glm::vec3 def)
{
    if (!j.contains(key)) return def;
    const auto& arr = j[key];
    if (arr.is_array() && arr.size() >= 3)
        return {arr[0].get<float>(), arr[1].get<float>(), arr[2].get<float>()};
    return def;
}

static void readHitForce(const json& j, WeaponHitFxForceConfig& cfg)
{
    if (!j.contains("hitForce")) return;
    const auto& f = j["hitForce"];
    cfg.enabled = readJsonBool(f, "enabled", cfg.enabled);
    cfg.minForce = readJsonFloat(f, "minForce", cfg.minForce);
    cfg.maxForce = readJsonFloat(f, "maxForce", cfg.maxForce);
    cfg.weaponMultiplier = readJsonFloat(f, "weaponMultiplier", cfg.weaponMultiplier);
    cfg.angleEnabled = readJsonBool(f, "angleEnabled", cfg.angleEnabled);
    cfg.momentumEnabled = readJsonBool(f, "momentumEnabled", cfg.momentumEnabled);
}

static void readDebris(const json& j, WeaponHitFxDebrisConfig& cfg)
{
    if (!j.contains("debris")) return;
    const auto& d = j["debris"];
    cfg.enabled = readJsonBool(d, "enabled", cfg.enabled);
    cfg.count = (int)readJsonFloat(d, "count", (float)cfg.count);
    cfg.countForceScale = readJsonFloat(d, "countForceScale", cfg.countForceScale);
    cfg.speed = readJsonFloat(d, "speed", cfg.speed);
    cfg.speedForceScale = readJsonFloat(d, "speedForceScale", cfg.speedForceScale);
    cfg.lifetime = readJsonFloat(d, "lifetime", cfg.lifetime);
    cfg.lifetimeForceScale = readJsonFloat(d, "lifetimeForceScale", cfg.lifetimeForceScale);
    cfg.size = readJsonFloat(d, "size", cfg.size);
    cfg.sizeForceScale = readJsonFloat(d, "sizeForceScale", cfg.sizeForceScale);
    cfg.endSize = readJsonFloat(d, "endSize", cfg.endSize);
    cfg.endSizeForceScale = readJsonFloat(d, "endSizeForceScale", cfg.endSizeForceScale);
    cfg.gravity = readJsonFloat(d, "gravity", cfg.gravity);
    cfg.color = readJsonVec3(d, "color", cfg.color);
}

static void readBlood(const json& j, WeaponHitFxBloodConfig& cfg)
{
    if (!j.contains("blood")) return;
    const auto& b = j["blood"];
    cfg.enabled = readJsonBool(b, "enabled", cfg.enabled);
    cfg.particleCount = (int)readJsonFloat(b, "particleCount", (float)cfg.particleCount);
    cfg.particleCountForceScale = readJsonFloat(b, "particleCountForceScale", cfg.particleCountForceScale);
    cfg.bloodConeDegrees = readJsonFloat(b, "bloodConeDegrees", cfg.bloodConeDegrees);
    cfg.bloodConeForceScale = readJsonFloat(b, "bloodConeForceScale", cfg.bloodConeForceScale);
    cfg.debrisCount = (int)readJsonFloat(b, "debrisCount", (float)cfg.debrisCount);
    cfg.debrisCountForceScale = readJsonFloat(b, "debrisCountForceScale", cfg.debrisCountForceScale);
    cfg.debrisConeDegrees = readJsonFloat(b, "debrisConeDegrees", cfg.debrisConeDegrees);
    cfg.debrisConeForceScale = readJsonFloat(b, "debrisConeForceScale", cfg.debrisConeForceScale);
    cfg.baseSpeed = readJsonFloat(b, "baseSpeed", cfg.baseSpeed);
    cfg.speedForceScale = readJsonFloat(b, "speedForceScale", cfg.speedForceScale);
    cfg.baseLifetime = readJsonFloat(b, "baseLifetime", cfg.baseLifetime);
    cfg.lifetimeForceScale = readJsonFloat(b, "lifetimeForceScale", cfg.lifetimeForceScale);
    cfg.decalCount = (int)readJsonFloat(b, "decalCount", (float)cfg.decalCount);
    cfg.decalCountForceScale = readJsonFloat(b, "decalCountForceScale", cfg.decalCountForceScale);
    cfg.decalRadius = readJsonFloat(b, "decalRadius", cfg.decalRadius);
    cfg.decalRadiusForceScale = readJsonFloat(b, "decalRadiusForceScale", cfg.decalRadiusForceScale);
    cfg.decalMaxRadius = readJsonFloat(b, "decalMaxRadius", cfg.decalMaxRadius);
    cfg.decalLifetime = readJsonFloat(b, "decalLifetime", cfg.decalLifetime);
    cfg.maxBloodParticles = (int)readJsonFloat(b, "maxBloodParticles", (float)cfg.maxBloodParticles);
    cfg.maxBloodDecals = (int)readJsonFloat(b, "maxBloodDecals", (float)cfg.maxBloodDecals);
}

static void readSound(const json& j, WeaponHitFxSoundConfig& cfg)
{
    if (!j.contains("sound")) return;
    const auto& s = j["sound"];
    cfg.enabled = readJsonBool(s, "enabled", cfg.enabled);
    cfg.baseVolume = readJsonFloat(s, "baseVolume", cfg.baseVolume);
    cfg.volumeSeverityScale = readJsonFloat(s, "volumeSeverityScale", cfg.volumeSeverityScale);
    cfg.volumeNearFactor = readJsonFloat(s, "volumeNearFactor", cfg.volumeNearFactor);
    cfg.pitchBase = readJsonFloat(s, "pitchBase", cfg.pitchBase);
    cfg.pitchSeverityScale = readJsonFloat(s, "pitchSeverityScale", cfg.pitchSeverityScale);
    cfg.pitchMin = readJsonFloat(s, "pitchMin", cfg.pitchMin);
    cfg.pitchMax = readJsonFloat(s, "pitchMax", cfg.pitchMax);
    cfg.nearDistance = readJsonFloat(s, "nearDistance", cfg.nearDistance);
}

static void readPerWeapon(const json& j, const std::string& key, WeaponHitFxPerWeapon& out)
{
    if (!j.contains(key)) return;
    const auto& pw = j[key];
    readHitForce(pw, out.hitForce);
    readDebris(pw, out.debris);
    readBlood(pw, out.blood);
    readSound(pw, out.sound);
}

static void mergeForce(const WeaponHitFxForceConfig& src, WeaponHitFxForceConfig& dst)
{
    if (!src.enabled) return;
    dst.minForce = src.minForce;
    dst.maxForce = src.maxForce;
    dst.weaponMultiplier = src.weaponMultiplier;
    dst.angleEnabled = src.angleEnabled;
    dst.momentumEnabled = src.momentumEnabled;
}

static void mergeDebris(const WeaponHitFxDebrisConfig& src, WeaponHitFxDebrisConfig& dst)
{
    if (!src.enabled) return;
    dst.count = src.count;
    dst.countForceScale = src.countForceScale;
    dst.speed = src.speed;
    dst.speedForceScale = src.speedForceScale;
    dst.lifetime = src.lifetime;
    dst.lifetimeForceScale = src.lifetimeForceScale;
    dst.size = src.size;
    dst.sizeForceScale = src.sizeForceScale;
    dst.endSize = src.endSize;
    dst.endSizeForceScale = src.endSizeForceScale;
    dst.gravity = src.gravity;
    dst.color = src.color;
}

static void mergeBlood(const WeaponHitFxBloodConfig& src, WeaponHitFxBloodConfig& dst)
{
    if (!src.enabled) return;
    dst.particleCount = src.particleCount;
    dst.particleCountForceScale = src.particleCountForceScale;
    dst.bloodConeDegrees = src.bloodConeDegrees;
    dst.bloodConeForceScale = src.bloodConeForceScale;
    dst.debrisCount = src.debrisCount;
    dst.debrisCountForceScale = src.debrisCountForceScale;
    dst.debrisConeDegrees = src.debrisConeDegrees;
    dst.debrisConeForceScale = src.debrisConeForceScale;
    dst.baseSpeed = src.baseSpeed;
    dst.speedForceScale = src.speedForceScale;
    dst.baseLifetime = src.baseLifetime;
    dst.lifetimeForceScale = src.lifetimeForceScale;
    dst.decalCount = src.decalCount;
    dst.decalCountForceScale = src.decalCountForceScale;
    dst.decalRadius = src.decalRadius;
    dst.decalRadiusForceScale = src.decalRadiusForceScale;
    dst.decalMaxRadius = src.decalMaxRadius;
    dst.decalLifetime = src.decalLifetime;
    dst.maxBloodParticles = src.maxBloodParticles;
    dst.maxBloodDecals = src.maxBloodDecals;
}

static void mergeSound(const WeaponHitFxSoundConfig& src, WeaponHitFxSoundConfig& dst)
{
    if (!src.enabled) return;
    dst.baseVolume = src.baseVolume;
    dst.volumeSeverityScale = src.volumeSeverityScale;
    dst.volumeNearFactor = src.volumeNearFactor;
    dst.pitchBase = src.pitchBase;
    dst.pitchSeverityScale = src.pitchSeverityScale;
    dst.pitchMin = src.pitchMin;
    dst.pitchMax = src.pitchMax;
    dst.nearDistance = src.nearDistance;
}

} // anonymous namespace

WeaponHitFxConfig& WeaponHitFxConfig::instance()
{
    static WeaponHitFxConfig config;
    return config;
}

bool WeaponHitFxConfig::load(const std::string& path)
{
    if (mPath != path) {
        mPath = path;
        mWatchLogged = false;
    }

    const std::string fileName = fileNameOf(mPath);
    if (!mWatchLogged) {
        Debug::warn(Debug::Category::Weapons,
            "[WEAPON HITFX] Watching: %s\n", fileName.c_str());
        mWatchLogged = true;
    }

    const auto writeTime = getLastWrite(mPath);
    std::ifstream file(mPath);
    if (!file.is_open()) {
        mLastWrite = writeTime;
        Debug::warn(Debug::Category::Weapons,
            "[WEAPON HITFX] Missing %s; using defaults.\n", mPath.c_str());
        return false;
    }

    try {
        json root;
        file >> root;

        Config defs;
        if (root.contains("defaults")) {
            const auto& d = root["defaults"];
            readHitForce(d, defs.hitForce);
            readDebris(d, defs.debris);
            readBlood(d, defs.blood);
            readSound(d, defs.sound);
        }
        mDefaults = defs;

        mPerWeapon.clear();
        if (root.contains("perWeapon")) {
            const auto& pw = root["perWeapon"];
            for (auto it = pw.begin(); it != pw.end(); ++it) {
                WeaponHitFxPerWeapon wp;
                wp.hitForce = defs.hitForce;
                wp.debris = defs.debris;
                wp.blood = defs.blood;
                wp.sound = defs.sound;
                readPerWeapon(pw, it.key(), wp);
                mPerWeapon[it.key()] = wp;
            }
        }

        mLastWrite = writeTime;
        Debug::warn(Debug::Category::Weapons,
            "[WEAPON HITFX] Loaded: %s (%zu weapons)\n",
            fileName.c_str(), mPerWeapon.size());
        return true;
    } catch (const json::parse_error& e) {
        mLastWrite = writeTime;
        Debug::error(Debug::Category::Weapons,
            "[WEAPON HITFX] Parse error in %s: %s\n", mPath.c_str(), e.what());
    } catch (const std::exception& e) {
        mLastWrite = writeTime;
        Debug::error(Debug::Category::Weapons,
            "[WEAPON HITFX] Error loading %s: %s\n", mPath.c_str(), e.what());
    }
    return false;
}

bool WeaponHitFxConfig::pollReload()
{
    const auto writeTime = getLastWrite(mPath);
    if (writeTime == std::filesystem::file_time_type{} || writeTime == mLastWrite)
        return false;

    Debug::warn(Debug::Category::Weapons,
        "[WEAPON HITFX] Detected change: %s\n", fileNameOf(mPath).c_str());
    return load(mPath);
}

const WeaponHitFxForceConfig& WeaponHitFxConfig::forceFor(const std::string& weaponId) const
{
    auto it = mPerWeapon.find(weaponId);
    if (it != mPerWeapon.end()) return it->second.hitForce;
    return mDefaults.hitForce;
}

const WeaponHitFxDebrisConfig& WeaponHitFxConfig::debrisFor(const std::string& weaponId) const
{
    auto it = mPerWeapon.find(weaponId);
    if (it != mPerWeapon.end()) return it->second.debris;
    return mDefaults.debris;
}

const WeaponHitFxBloodConfig& WeaponHitFxConfig::bloodFor(const std::string& weaponId) const
{
    auto it = mPerWeapon.find(weaponId);
    if (it != mPerWeapon.end()) return it->second.blood;
    return mDefaults.blood;
}

const WeaponHitFxSoundConfig& WeaponHitFxConfig::soundFor(const std::string& weaponId) const
{
    auto it = mPerWeapon.find(weaponId);
    if (it != mPerWeapon.end()) return it->second.sound;
    return mDefaults.sound;
}
