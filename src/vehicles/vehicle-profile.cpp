// 2026-11-24

#include "vehicles/vehicle-profile.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <mutex>
#include <fstream>
#include <set>

#include <nlohmann/json.hpp>

#include "debug/debug-log.h"
#include "utils/json-comments.h"

using json = nlohmann::json;

namespace {

int64_t modifiedTime(const std::string& path)
{
    std::error_code ec;
    const auto time = std::filesystem::last_write_time(path, ec);
    if (ec)
        return 0;
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               time.time_since_epoch())
        .count();
}

float readFloat(const json& root, const char* key, float fallback,
                float minimum, float maximum)
{
    if (!root.contains(key) || !root[key].is_number())
        return fallback;
    const float value = root[key].get<float>();
    if (!std::isfinite(value))
        return fallback;
    return std::clamp(value, minimum, maximum);
}

bool readBool(const json& root, const char* key, bool fallback)
{
    if (!root.contains(key) || !root[key].is_boolean())
        return fallback;
    return root[key].get<bool>();
}

std::string readString(const json& root, const char* key,
                       const std::string& fallback)
{
    if (!root.contains(key) || !root[key].is_string())
        return fallback;
    return root[key].get<std::string>();
}

// Piecewise-linear curve as (input, output) pairs. Accepts [[x,y], ...] or
// [{"x":x,"y":y}, ...] so authors can annotate points without a schema change.
std::vector<glm::vec2> readCurve(const json& root, const char* key,
                                 const std::vector<glm::vec2>& fallback)
{
    if (!root.contains(key) || !root[key].is_array() || root[key].empty())
        return fallback;
    std::vector<glm::vec2> out;
    out.reserve(root[key].size());
    for (const json& item : root[key])
    {
        float x = 0.0f;
        float y = 0.0f;
        if (item.is_array() && item.size() >= 2 && item[0].is_number() &&
            item[1].is_number())
        {
            x = item[0].get<float>();
            y = item[1].get<float>();
        }
        else if (item.is_object() && item.contains("x") && item.contains("y"))
        {
            x = item["x"].get<float>();
            y = item["y"].get<float>();
        }
        else
        {
            return fallback;
        }
        if (!std::isfinite(x) || !std::isfinite(y))
            return fallback;
        out.emplace_back(x, y);
    }
    return out;
}

float sampleCurve(const std::vector<glm::vec2>& curve, float input,
                  float fallback)
{
    if (curve.empty())
        return fallback;
    if (input <= curve.front().x)
        return curve.front().y;
    for (size_t i = 1; i < curve.size(); ++i)
    {
        if (input <= curve[i].x)
        {
            const glm::vec2& a = curve[i - 1];
            const glm::vec2& b = curve[i];
            const float span = b.x - a.x;
            if (span <= 1e-6f)
                return b.y;
            const float t = (input - a.x) / span;
            return a.y + (b.y - a.y) * t;
        }
    }
    return curve.back().y;
}

}  // namespace

float vehicleProfileCurveSample(const std::vector<glm::vec2>& curve, float input,
                                float fallback)
{
    return sampleCurve(curve, input, fallback);
}

VehicleProfileRegistry& VehicleProfileRegistry::instance()
{
    static VehicleProfileRegistry registry;
    return registry;
}

bool VehicleProfileRegistry::loadFile(const std::string& path,
                                      VehicleProfile& out) const
{
    std::ifstream file(path);
    if (!file.is_open())
        return false;

    json root;
    try
    {
        root = parseJsonConfig(file);
    }
    catch (const std::exception&)
    {
        return false;
    }
    if (!root.is_object())
        return false;

    VehicleProfile profile;
    profile.id = readString(root, "id", "");
    if (profile.id.empty())
        return false;
    profile.description = readString(root, "description", "");
    profile.source = readString(root, "source", profile.source);
    profile.gravityMps2 =
        readFloat(root, "gravityMps2", profile.gravityMps2, 0.0f, 100.0f);

    if (root.contains("transmission") && root["transmission"].is_object())
    {
        const json& transmission = root["transmission"];
        profile.automatic =
            readBool(transmission, "automatic", profile.automatic);
        profile.engineAutoStart = readBool(transmission, "engine_auto_start",
                                           profile.engineAutoStart);
    }

    if (root.contains("boost") && root["boost"].is_object())
    {
        const json& boost = root["boost"];
        profile.boost.max =
            readFloat(boost, "max", profile.boost.max, 0.0f, 100000.0f);
        profile.boost.fullDrainTicks =
            readFloat(boost, "fullDrainTicks", profile.boost.fullDrainTicks,
                      1.0f, 100000.0f);
        profile.boost.drainPerTick = readFloat(
            boost, "drainPerTick", profile.boost.drainPerTick, 0.0f, 10000.0f);
        if (profile.boost.drainPerTick <= 0.0f &&
            profile.boost.fullDrainTicks > 0.0f)
        {
            profile.boost.drainPerTick =
                profile.boost.max / profile.boost.fullDrainTicks;
        }
        profile.boost.groundAccelMps2 = readFloat(
            boost, "groundAccelMps2", profile.boost.groundAccelMps2, 0.0f, 500.0f);
        profile.boost.airAccelMps2 = readFloat(
            boost, "airAccelMps2", profile.boost.airAccelMps2, 0.0f, 500.0f);
        profile.boost.minBoostSeconds = readFloat(
            boost, "minBoostSeconds", profile.boost.minBoostSeconds, 0.0f, 5.0f);
        profile.boost.force =
            readFloat(boost, "force", profile.boost.force, 0.0f, 1.0e7f);
        profile.boost.airForce =
            readFloat(boost, "airForce", profile.boost.airForce, 0.0f, 1.0e7f);
    }

    if (root.contains("jump") && root["jump"].is_object())
    {
        const json& jump = root["jump"];
        profile.jump.immediateDeltaVelMps = readFloat(
            jump, "immediateDeltaVelMps", profile.jump.immediateDeltaVelMps,
            0.0f, 100.0f);
        profile.jump.sustainedAccelMps2 = readFloat(
            jump, "sustainedAccelMps2", profile.jump.sustainedAccelMps2,
            0.0f, 500.0f);
        profile.jump.sustainedWindowSeconds = readFloat(
            jump, "sustainedWindowSeconds", profile.jump.sustainedWindowSeconds,
            0.0f, 5.0f);
        profile.jump.minSustainSeconds = readFloat(
            jump, "minSustainSeconds", profile.jump.minSustainSeconds,
            0.0f, 5.0f);
        profile.jump.preMinAccelScale = readFloat(
            jump, "preMinAccelScale", profile.jump.preMinAccelScale, 0.0f, 5.0f);
        profile.jump.stickyAccelMps2 = readFloat(
            jump, "stickyAccelMps2", profile.jump.stickyAccelMps2, 0.0f, 100.0f);
        profile.jump.stickyTicks = static_cast<int>(readFloat(
            jump, "stickyTicks", static_cast<float>(profile.jump.stickyTicks),
            0.0f, 120.0f));
        profile.jump.doubleJumpDeltaVelMps = readFloat(
            jump, "doubleJumpDeltaVelMps", profile.jump.doubleJumpDeltaVelMps,
            0.0f, 100.0f);
        profile.jump.doubleJumpWindowSeconds = readFloat(
            jump, "doubleJumpWindowSeconds",
            profile.jump.doubleJumpWindowSeconds, 0.0f, 10.0f);
        profile.jump.dodgeDeadzone = readFloat(
            jump, "dodgeDeadzone", profile.jump.dodgeDeadzone, 0.0f, 3.0f);
        profile.jump.infiniteJumps =
            readBool(jump, "infiniteJumps", profile.jump.infiniteJumps);
    }

    if (root.contains("flip") && root["flip"].is_object())
    {
        const json& flip = root["flip"];
        profile.flip.angularAccelPitch = readFloat(
            flip, "angularAccelPitch", profile.flip.angularAccelPitch,
            0.0f, 500.0f);
        profile.flip.angularAccelRoll = readFloat(
            flip, "angularAccelRoll", profile.flip.angularAccelRoll,
            0.0f, 500.0f);
        profile.flip.torqueSeconds = readFloat(
            flip, "torqueSeconds", profile.flip.torqueSeconds, 0.0f, 5.0f);
        profile.flip.maxAngularVelocity = readFloat(
            flip, "maxAngularVelocity", profile.flip.maxAngularVelocity,
            0.0f, 100.0f);
        profile.flip.pitchLockSeconds = readFloat(
            flip, "pitchLockSeconds", profile.flip.pitchLockSeconds, 0.0f, 5.0f);
        profile.flip.initialDeltaVelMps = readFloat(
            flip, "initialDeltaVelMps", profile.flip.initialDeltaVelMps,
            0.0f, 100.0f);
        profile.flip.forwardMaxSpeedScale = readFloat(
            flip, "forwardMaxSpeedScale", profile.flip.forwardMaxSpeedScale,
            0.0f, 10.0f);
        profile.flip.sideMaxSpeedScale = readFloat(
            flip, "sideMaxSpeedScale", profile.flip.sideMaxSpeedScale,
            0.0f, 10.0f);
        profile.flip.backwardMaxSpeedScale = readFloat(
            flip, "backwardMaxSpeedScale", profile.flip.backwardMaxSpeedScale,
            0.0f, 10.0f);
        profile.flip.backwardScaleX = readFloat(
            flip, "backwardScaleX", profile.flip.backwardScaleX, 0.0f, 10.0f);
        profile.flip.zDampFactor = readFloat(
            flip, "zDampFactor", profile.flip.zDampFactor, 0.0f, 1.0f);
        profile.flip.zDampStartSeconds = readFloat(
            flip, "zDampStartSeconds", profile.flip.zDampStartSeconds,
            0.0f, 5.0f);
        profile.flip.zDampEndSeconds = readFloat(
            flip, "zDampEndSeconds", profile.flip.zDampEndSeconds, 0.0f, 5.0f);
        profile.flip.directionCancelThreshold = readFloat(
            flip, "directionCancelThreshold",
            profile.flip.directionCancelThreshold, 0.0f, 2.0f);
    }

    if (root.contains("airControl") && root["airControl"].is_object())
    {
        const json& air = root["airControl"];
        profile.airControl.pitchAccel = readFloat(
            air, "pitchAccel", profile.airControl.pitchAccel, 0.0f, 500.0f);
        profile.airControl.yawAccel = readFloat(
            air, "yawAccel", profile.airControl.yawAccel, 0.0f, 500.0f);
        profile.airControl.rollAccel = readFloat(
            air, "rollAccel", profile.airControl.rollAccel, 0.0f, 500.0f);
        profile.airControl.pitchDamping = readFloat(
            air, "pitchDamping", profile.airControl.pitchDamping, 0.0f, 100.0f);
        profile.airControl.yawDamping = readFloat(
            air, "yawDamping", profile.airControl.yawDamping, 0.0f, 100.0f);
        profile.airControl.rollDamping = readFloat(
            air, "rollDamping", profile.airControl.rollDamping, 0.0f, 100.0f);
        profile.airControl.maxAngularVelocity = readFloat(
            air, "maxAngularVelocity", profile.airControl.maxAngularVelocity,
            0.0f, 100.0f);
        profile.airControl.inputScaledDamping = readBool(
            air, "inputScaledDamping", profile.airControl.inputScaledDamping);
        profile.airControl.throttleAirAccelMps2 = readFloat(
            air, "throttleAirAccelMps2", profile.airControl.throttleAirAccelMps2,
            0.0f, 100.0f);
    }

    if (root.contains("steering") && root["steering"].is_object())
    {
        const json& steering = root["steering"];
        profile.steering.maxAngleDeg = readFloat(
            steering, "maxAngleDeg", profile.steering.maxAngleDeg, 0.0f, 90.0f);
        profile.steering.speedFalloff = readFloat(
            steering, "speedFalloff", profile.steering.speedFalloff,
            0.0f, 10.0f);
        profile.steering.returnRate = readFloat(
            steering, "returnRate", profile.steering.returnRate, 0.0f, 100.0f);
    }

    if (root.contains("powerslide") && root["powerslide"].is_object())
    {
        const json& powerslide = root["powerslide"];
        profile.powerslide.gripScale = readFloat(
            powerslide, "gripScale", profile.powerslide.gripScale, 0.0f, 10.0f);
    }

    if (root.contains("wall") && root["wall"].is_object())
    {
        const json& wall = root["wall"];
        profile.wall.adhesionForce = readFloat(
            wall, "adhesionForce", profile.wall.adhesionForce, 0.0f, 1.0e8f);
        profile.wall.adhesionMaxNormalUpDot = readFloat(
            wall, "adhesionMaxNormalUpDot",
            profile.wall.adhesionMaxNormalUpDot, -1.0f, 1.0f);
        profile.wall.slideGravityScale = readFloat(
            wall, "slideGravityScale", profile.wall.slideGravityScale,
            0.0f, 10.0f);
    }

    if (root.contains("ballContact") && root["ballContact"].is_object())
    {
        const json& contact = root["ballContact"];
        profile.ballContact.wheelImpulseScale = readFloat(
            contact, "wheelImpulseScale", profile.ballContact.wheelImpulseScale,
            0.0f, 10.0f);
        profile.ballContact.chassisImpulseScale = readFloat(
            contact, "chassisImpulseScale",
            profile.ballContact.chassisImpulseScale, 0.0f, 10.0f);
    }

    if (root.contains("ground") && root["ground"].is_object())
    {
        const json& ground = root["ground"];
        profile.ground.maxSpeedMps = readFloat(
            ground, "maxSpeedMps", profile.ground.maxSpeedMps, 0.0f, 200.0f);
        profile.ground.supersonicMps = readFloat(
            ground, "supersonicMps", profile.ground.supersonicMps, 0.0f, 200.0f);
        profile.ground.maxUnboostedMps = readFloat(
            ground, "maxUnboostedMps", profile.ground.maxUnboostedMps,
            0.0f, 200.0f);
        profile.ground.coastingDecelMps2 = readFloat(
            ground, "coastingDecelMps2", profile.ground.coastingDecelMps2,
            0.0f, 200.0f);
        profile.ground.brakingDecelMps2 = readFloat(
            ground, "brakingDecelMps2", profile.ground.brakingDecelMps2,
            0.0f, 500.0f);
        profile.ground.throttleDeadzone = readFloat(
            ground, "throttleDeadzone", profile.ground.throttleDeadzone,
            0.0f, 1.0f);
        profile.ground.powerslideRisePerSecond = readFloat(
            ground, "powerslideRisePerSecond",
            profile.ground.powerslideRisePerSecond, 0.0f, 50.0f);
        profile.ground.powerslideFallPerSecond = readFloat(
            ground, "powerslideFallPerSecond",
            profile.ground.powerslideFallPerSecond, 0.0f, 50.0f);
        profile.ground.powerslideLatFrictionScale = readFloat(
            ground, "powerslideLatFrictionScale",
            profile.ground.powerslideLatFrictionScale, 0.0f, 1.0f);
        profile.ground.powerslideLongFrictionMin = readFloat(
            ground, "powerslideLongFrictionMin",
            profile.ground.powerslideLongFrictionMin, 0.0f, 2.0f);
        profile.ground.powerslideLongFrictionMax = readFloat(
            ground, "powerslideLongFrictionMax",
            profile.ground.powerslideLongFrictionMax, 0.0f, 2.0f);
        profile.ground.attitudeLock = readBool(
            ground, "attitudeLock", profile.ground.attitudeLock);
        profile.ground.alignToSurface = readBool(
            ground, "alignToSurface", profile.ground.alignToSurface);
        profile.ground.alignRateDegPerSecond = readFloat(
            ground, "alignRateDegPerSecond",
            profile.ground.alignRateDegPerSecond, 0.0f, 3600.0f);
        profile.ground.maxYawRateRadPerSecond = readFloat(
            ground, "maxYawRateRadPerSecond",
            profile.ground.maxYawRateRadPerSecond, 0.0f, 20.0f);
        profile.ground.yawAccelRadPerSecond2 = readFloat(
            ground, "yawAccelRadPerSecond2",
            profile.ground.yawAccelRadPerSecond2, 0.0f, 200.0f);
    }

    if (root.contains("ballHit") && root["ballHit"].is_object())
    {
        const json& hit = root["ballHit"];
        profile.ballHit.extraImpulseEnabled = readBool(
            hit, "extraImpulseEnabled", profile.ballHit.extraImpulseEnabled);
        profile.ballHit.extraImpulseScale = readFloat(
            hit, "extraImpulseScale", profile.ballHit.extraImpulseScale,
            0.0f, 100.0f);
        profile.ballHit.extraImpulseZScale = readFloat(
            hit, "extraImpulseZScale", profile.ballHit.extraImpulseZScale,
            0.0f, 2.0f);
        profile.ballHit.extraImpulseForwardScale = readFloat(
            hit, "extraImpulseForwardScale",
            profile.ballHit.extraImpulseForwardScale, 0.0f, 2.0f);
        profile.ballHit.extraImpulseMaxDeltaVelMps = readFloat(
            hit, "extraImpulseMaxDeltaVelMps",
            profile.ballHit.extraImpulseMaxDeltaVelMps, 0.0f, 500.0f);
        profile.ballHit.extraImpulseCurve = readCurve(
            hit, "extraImpulseCurve", profile.ballHit.extraImpulseCurve);
        profile.ballHit.carBallFriction = readFloat(
            hit, "carBallFriction", profile.ballHit.carBallFriction, 0.0f, 10.0f);
        profile.ballHit.carBallRestitution = readFloat(
            hit, "carBallRestitution", profile.ballHit.carBallRestitution,
            0.0f, 1.0f);
    }

    if (root.contains("settle") && root["settle"].is_object())
    {
        const json& settle = root["settle"];
        profile.settle.stickyForceGravityScale = readFloat(
            settle, "stickyForceGravityScale",
            profile.settle.stickyForceGravityScale, 0.0f, 5.0f);
        profile.settle.autoFlipImpulseMps = readFloat(
            settle, "autoFlipImpulseMps", profile.settle.autoFlipImpulseMps,
            0.0f, 50.0f);
        profile.settle.autoFlipAngularAccel = readFloat(
            settle, "autoFlipAngularAccel",
            profile.settle.autoFlipAngularAccel, 0.0f, 200.0f);
        profile.settle.autoFlipSeconds = readFloat(
            settle, "autoFlipSeconds", profile.settle.autoFlipSeconds, 0.0f, 5.0f);
        profile.settle.autoFlipNormalZThreshold = readFloat(
            settle, "autoFlipNormalZThreshold",
            profile.settle.autoFlipNormalZThreshold, -1.0f, 1.0f);
        profile.settle.autoFlipRollThresholdRad = readFloat(
            settle, "autoFlipRollThresholdRad",
            profile.settle.autoFlipRollThresholdRad, 0.0f, 4.0f);
        profile.settle.autoRollForceMps2 = readFloat(
            settle, "autoRollForceMps2", profile.settle.autoRollForceMps2,
            0.0f, 100.0f);
        profile.settle.autoRollAngularAccel = readFloat(
            settle, "autoRollAngularAccel", profile.settle.autoRollAngularAccel,
            0.0f, 200.0f);
    }

    if (root.contains("bindings") && root["bindings"].is_object())
    {
        for (auto it = root["bindings"].begin();
             it != root["bindings"].end(); ++it)
        {
            if (it.value().is_string())
                profile.bindings[it.key()] = it.value().get<std::string>();
        }
    }

    out = std::move(profile);
    return true;
}

bool VehicleProfileRegistry::loadDirectory(const std::string& directory)
{
    std::unique_lock<std::shared_mutex> publishLock(mProfilesMutex);
    mDirectory = directory;
    std::unordered_map<std::string, ProfilePtr> profiles;
    std::unordered_map<std::string, std::string> profilePaths;
    std::unordered_map<std::string, int64_t> times;

    std::error_code ec;
    if (!std::filesystem::is_directory(directory, ec))
    {
        Debug::warn(Debug::Category::Physics,
                    "[VEHICLE PROFILE] missing directory %s\n",
                    directory.c_str());
        mProfiles.clear();
        mProfilePaths.clear();
        mFileTimes.clear();
        ++mRevision;
        return false;
    }

    for (const auto& entry :
         std::filesystem::directory_iterator(directory, ec))
    {
        if (ec || !entry.is_regular_file())
            continue;
        if (entry.path().extension() != ".json")
            continue;
        VehicleProfile profile;
        if (!loadFile(entry.path().string(), profile))
        {
            Debug::warn(Debug::Category::Physics,
                        "[VEHICLE PROFILE] invalid %s; skipped\n",
                        entry.path().string().c_str());
            continue;
        }
        times[entry.path().string()] = modifiedTime(entry.path().string());
        // Read the id BEFORE moving the profile into the snapshot: evaluation
        // order of the key and the moved argument is unspecified, and reading a
        // moved-from string collapsed every profile onto the empty key.
        const std::string id = profile.id;
        profilePaths[id] = entry.path().string();
        profiles[id] = std::make_shared<const VehicleProfile>(std::move(profile));
    }

    mProfiles = std::move(profiles);
    mProfilePaths = std::move(profilePaths);
    mFileTimes = std::move(times);
    ++mRevision;
    Debug::log(Debug::Category::Physics,
               "[VEHICLE PROFILE] loaded %zu profiles from %s\n",
               mProfiles.size(), directory.c_str());
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Physics, StructuredLevel::Important,
        "vehicle.profile.registry", "vehicle-profile",
        "profile registry published a new snapshot", 0,
        {{"revision", mRevision}, {"count", mProfiles.size()},
         {"directory", directory}});
    return !mProfiles.empty();
}

bool VehicleProfileRegistry::pollReload()
{
    std::error_code ec;
    if (!std::filesystem::is_directory(mDirectory, ec))
        return false;

    bool changed = false;
    std::set<std::string> seen;
    for (const auto& entry :
         std::filesystem::directory_iterator(mDirectory, ec))
    {
        if (ec || !entry.is_regular_file())
            continue;
        if (entry.path().extension() != ".json")
            continue;
        const std::string path = entry.path().string();
        seen.insert(path);
        const int64_t time = modifiedTime(path);
        auto it = mFileTimes.find(path);
        if (it == mFileTimes.end() || it->second != time)
            changed = true;
    }
    if (seen.size() != mFileTimes.size())
        changed = true;
    if (changed)
        return loadDirectory(mDirectory);
    return false;
}

VehicleProfileRegistry::ProfilePtr VehicleProfileRegistry::find(
    const std::string& id) const
{
    std::shared_lock<std::shared_mutex> readLock(mProfilesMutex);
    auto it = mProfiles.find(id);
    return it != mProfiles.end() ? it->second : nullptr;
}

VehicleProfileRegistry::ProfilePtr VehicleProfileRegistry::defaultProfile() const
{
    if (ProfilePtr named = find("rl_soccar"))
        return named;
    const std::vector<std::string> all = ids();
    if (all.empty())
        return nullptr;
    return find(all.front());
}

std::vector<std::string> VehicleProfileRegistry::ids() const
{
    std::shared_lock<std::shared_mutex> readLock(mProfilesMutex);
    std::vector<std::string> out;
    out.reserve(mProfiles.size());
    for (const auto& [id, _] : mProfiles)
        out.push_back(id);
    std::sort(out.begin(), out.end());
    return out;
}
