// 2026-10-20
/* purpose
* Load and hot-reload the actor-pair capsule/push tuning as one validated file.
* Keep the previous valid data when a save is incomplete or invalid.
* Does NOT solve collision or own gameplay state.
*/
#include "config/actor-collision-config.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "debug/debug-log.h"
#include "utils/json-comments.h"

using json = nlohmann::json;

namespace {

std::filesystem::file_time_type lastWriteOf(const std::string& path)
{
    std::error_code ec;
    return std::filesystem::last_write_time(path, ec);
}

bool finitePositive(float value)
{
    return std::isfinite(value) && value > 0.0f;
}

bool readFloat(const json& object, const char* key, float& out, float fallback,
               std::string& error, const std::string& profileId)
{
    (void)fallback;
    if (!object.contains(key))
        return true;
    if (!object[key].is_number())
    {
        error = "profile '" + profileId + "' field '" + key + "' is not a number";
        return false;
    }
    out = object[key].get<float>();
    if (!std::isfinite(out))
    {
        error = "profile '" + profileId + "' field '" + key + "' is not finite";
        return false;
    }
    return true;
}

bool readPush(const json& object, ActorCollisionPushTuning& push,
              std::string& error, const std::string& profileId)
{
    if (!object.contains("push"))
        return true;
    if (!object["push"].is_object())
    {
        error = "profile '" + profileId + "' push is not an object";
        return false;
    }
    const json& p = object["push"];
    if (!readFloat(p, "weak_hit_distance_m", push.weakHitDistanceM,
                   push.weakHitDistanceM, error, profileId)) return false;
    if (!readFloat(p, "weak_hit_motion_seconds", push.weakHitMotionSeconds,
                   push.weakHitMotionSeconds, error, profileId)) return false;
    if (!readFloat(p, "min_impact_speed_mps", push.minImpactSpeedMps,
                   push.minImpactSpeedMps, error, profileId)) return false;
    if (!readFloat(p, "max_impact_speed_mps", push.maxImpactSpeedMps,
                   push.maxImpactSpeedMps, error, profileId)) return false;
    if (!readFloat(p, "max_knockback_speed_mps", push.maxKnockbackSpeedMps,
                   push.maxKnockbackSpeedMps, error, profileId)) return false;
    if (!readFloat(p, "impact_speed_curve_exponent", push.impactSpeedCurveExponent,
                   push.impactSpeedCurveExponent, error, profileId)) return false;
    if (!readFloat(p, "impact_angle_min_scale", push.impactAngleMinScale,
                   push.impactAngleMinScale, error, profileId)) return false;
    if (!readFloat(p, "impact_angle_curve_exponent", push.impactAngleCurveExponent,
                   push.impactAngleCurveExponent, error, profileId)) return false;
    if (!readFloat(p, "support_max_impact_speed_mps", push.supportMaxImpactSpeedMps,
                   push.supportMaxImpactSpeedMps, error, profileId)) return false;
    if (!readFloat(p, "support_top_tolerance_m", push.supportTopToleranceM,
                   push.supportTopToleranceM, error, profileId)) return false;
    if (!readFloat(p, "support_snap_max_m", push.supportSnapMaxM,
                   push.supportSnapMaxM, error, profileId)) return false;

    push.weakHitDistanceM = std::max(0.0f, push.weakHitDistanceM);
    push.weakHitMotionSeconds = std::max(0.01f, push.weakHitMotionSeconds);
    push.minImpactSpeedMps = std::max(0.0f, push.minImpactSpeedMps);
    push.maxImpactSpeedMps = std::max(push.minImpactSpeedMps, push.maxImpactSpeedMps);
    push.maxKnockbackSpeedMps = std::max(0.0f, push.maxKnockbackSpeedMps);
    push.impactSpeedCurveExponent = std::max(0.01f, push.impactSpeedCurveExponent);
    push.impactAngleMinScale = std::clamp(push.impactAngleMinScale, 0.0f, 1.0f);
    push.impactAngleCurveExponent = std::max(0.01f, push.impactAngleCurveExponent);
    push.supportMaxImpactSpeedMps = std::max(0.0f, push.supportMaxImpactSpeedMps);
    push.supportTopToleranceM = std::max(0.0f, push.supportTopToleranceM);
    push.supportSnapMaxM = std::max(push.supportTopToleranceM, push.supportSnapMaxM);
    return true;
}

} // namespace

ActorCollisionConfig& ActorCollisionConfig::instance()
{
    static ActorCollisionConfig config;
    return config;
}

ActorCollisionConfig::ActorCollisionConfig()
{
    load();
}

ActorCollisionData ActorCollisionConfig::defaults()
{
    ActorCollisionData data;
    ActorCollisionProfile profile;
    profile.id = "default";
    profile.capsuleRadiusM = 0.7f;
    profile.capsuleHeightM = 3.6f;
    profile.deadCapsuleLengthM = 2.6f;
    data.profiles.emplace(profile.id, profile);
    return data;
}

bool ActorCollisionConfig::validate(ActorCollisionData& candidate, std::string& error)
{
    if (!std::isfinite(candidate.broadphaseCellSizeM) ||
        candidate.broadphaseCellSizeM < 0.25f)
    {
        error = "broadphase.cell_size_m must be finite and >= 0.25";
        return false;
    }
    if (candidate.maxCandidatePairs < 1)
    {
        error = "broadphase.max_candidate_pairs must be >= 1";
        return false;
    }
    if (candidate.profiles.empty())
    {
        error = "profiles must not be empty";
        return false;
    }
    auto defaultIt = candidate.profiles.find("default");
    if (defaultIt == candidate.profiles.end())
    {
        error = "profiles.default is required";
        return false;
    }
    for (auto& entry : candidate.profiles)
    {
        ActorCollisionProfile& p = entry.second;
        p.id = entry.first;
        if (!finitePositive(p.capsuleRadiusM))
        {
            error = "profile '" + entry.first + "' capsule_radius_m must be > 0";
            return false;
        }
        if (!finitePositive(p.capsuleHeightM))
        {
            error = "profile '" + entry.first + "' capsule_height_m must be > 0";
            return false;
        }
        if (!std::isfinite(p.deadCapsuleLengthM) || p.deadCapsuleLengthM <= 0.0f)
        {
            error = "profile '" + entry.first + "' dead_capsule_length_m must be > 0";
            return false;
        }
        if (p.capsuleHeightM < p.capsuleRadiusM * 2.0f)
        {
            error = "profile '" + entry.first + "' capsule_height_m must be >= 2 * radius";
            return false;
        }
        if (p.deadCapsuleLengthM < p.capsuleRadiusM * 2.0f)
        {
            error = "profile '" + entry.first +
                    "' dead_capsule_length_m must be >= 2 * radius";
            return false;
        }
    }
    return true;
}

bool ActorCollisionConfig::load(const std::string& path)
{
    if (mPath != path)
    {
        mPath = path;
        mWatchLogged = false;
    }

    const auto writeTime = lastWriteOf(mPath);
    std::ifstream file(mPath);
    if (!file.is_open())
    {
        mData = defaults();
        mLastWrite = writeTime;
        if (!mWatchLogged)
        {
            Debug::warn(Debug::Category::Collision,
                "[ACTOR COLLISION CONFIG] Missing %s; using built-in defaults.\n",
                mPath.c_str());
            mWatchLogged = true;
        }
        return false;
    }

    try
    {
        json root = parseJsonConfig(file);
        if (!root.is_object())
            throw std::runtime_error("root must be an object");

        ActorCollisionData next = defaults();
        next.revision = mData.revision + 1;

        if (root.contains("broadphase") && root["broadphase"].is_object())
        {
            const json& b = root["broadphase"];
            if (b.contains("cell_size_m"))
                next.broadphaseCellSizeM = b["cell_size_m"].get<float>();
            if (b.contains("max_candidate_pairs"))
                next.maxCandidatePairs = b["max_candidate_pairs"].get<int>();
        }

        // Read the default override first, then inherit it for every explicit
        // profile so a profile only needs to state the fields it changes.
        ActorCollisionProfile base = next.profiles.at("default");
        if (root.contains("profiles") && root["profiles"].is_object())
        {
            const json& profiles = root["profiles"];
            if (profiles.contains("default") && profiles["default"].is_object())
            {
                std::string error;
                const json& j = profiles["default"];
                if (!readFloat(j, "capsule_radius_m", base.capsuleRadiusM,
                               base.capsuleRadiusM, error, "default")) throw std::runtime_error(error);
                if (!readFloat(j, "capsule_height_m", base.capsuleHeightM,
                               base.capsuleHeightM, error, "default")) throw std::runtime_error(error);
                if (!readFloat(j, "dead_capsule_length_m", base.deadCapsuleLengthM,
                               base.deadCapsuleLengthM, error, "default")) throw std::runtime_error(error);
                if (!readPush(j, base.push, error, "default")) throw std::runtime_error(error);
            }
            next.profiles.clear();
            next.profiles.emplace("default", base);
            for (auto it = profiles.begin(); it != profiles.end(); ++it)
            {
                if (!it.value().is_object())
                    throw std::runtime_error("profile '" + it.key() + "' is not an object");
                ActorCollisionProfile p = base;
                p.id = it.key();
                std::string error;
                if (!readFloat(it.value(), "capsule_radius_m", p.capsuleRadiusM,
                               p.capsuleRadiusM, error, it.key())) throw std::runtime_error(error);
                if (!readFloat(it.value(), "capsule_height_m", p.capsuleHeightM,
                               p.capsuleHeightM, error, it.key())) throw std::runtime_error(error);
                if (!readFloat(it.value(), "dead_capsule_length_m", p.deadCapsuleLengthM,
                               p.deadCapsuleLengthM, error, it.key())) throw std::runtime_error(error);
                if (!readPush(it.value(), p.push, error, it.key())) throw std::runtime_error(error);
                next.profiles[it.key()] = p;
            }
        }

        std::string error;
        if (!validate(next, error))
            throw std::runtime_error(error);

        mData = std::move(next);
        mLastWrite = writeTime;
        mLastCheck = std::chrono::steady_clock::now();
        if (!mWatchLogged)
        {
            Debug::log(Debug::Category::Collision,
                "[ACTOR COLLISION CONFIG] Loaded %s revision=%llu profiles=%zu\n",
                mPath.c_str(), (unsigned long long)mData.revision,
                mData.profiles.size());
            mWatchLogged = true;
        }
        return true;
    }
    catch (const std::exception& e)
    {
        mLastWrite = writeTime;
        mLastCheck = std::chrono::steady_clock::now();
        Debug::error(Debug::Category::Collision,
            "[ACTOR COLLISION CONFIG ERROR] %s: %s; keeping previous valid settings.\n",
            mPath.c_str(), e.what());
        return false;
    }
}

bool ActorCollisionConfig::pollHotReload()
{
    const auto now = std::chrono::steady_clock::now();
    if (mLastCheck.time_since_epoch().count() != 0 &&
        now - mLastCheck < std::chrono::milliseconds(250))
        return false;
    mLastCheck = now;

    std::error_code ec;
    if (!std::filesystem::exists(mPath, ec) || ec)
        return false;
    const auto writeTime = lastWriteOf(mPath);
    if (writeTime == std::filesystem::file_time_type{} || writeTime == mLastWrite)
        return false;

    return load(mPath);
}

const ActorCollisionProfile& ActorCollisionConfig::profileFor(
    const std::string& actorPresetId) const
{
    if (!actorPresetId.empty())
    {
        auto it = mData.profiles.find(actorPresetId);
        if (it != mData.profiles.end())
            return it->second;
    }
    return mData.profiles.at("default");
}
