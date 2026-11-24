// 2026-11-24

#include "vehicles/vehicle-profile.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
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

}  // namespace

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
        profile.boost.force =
            readFloat(boost, "force", profile.boost.force, 0.0f, 1.0e7f);
        profile.boost.airForce =
            readFloat(boost, "airForce", profile.boost.airForce, 0.0f, 1.0e7f);
    }

    if (root.contains("jump") && root["jump"].is_object())
    {
        const json& jump = root["jump"];
        profile.jump.impulse =
            readFloat(jump, "impulse", profile.jump.impulse, 0.0f, 1.0e8f);
        profile.jump.heldForce =
            readFloat(jump, "heldForce", profile.jump.heldForce, 0.0f, 1.0e8f);
        profile.jump.heldWindowSeconds = readFloat(
            jump, "heldWindowSeconds", profile.jump.heldWindowSeconds,
            0.0f, 5.0f);
        profile.jump.minimumHoldSeconds = readFloat(
            jump, "minimumHoldSeconds", profile.jump.minimumHoldSeconds,
            0.0f, 5.0f);
        profile.jump.doubleJumpImpulse = readFloat(
            jump, "doubleJumpImpulse", profile.jump.doubleJumpImpulse,
            0.0f, 1.0e8f);
        profile.jump.secondJumpWindowSeconds = readFloat(
            jump, "secondJumpWindowSeconds",
            profile.jump.secondJumpWindowSeconds, 0.0f, 10.0f);
    }

    if (root.contains("airControl") && root["airControl"].is_object())
    {
        const json& air = root["airControl"];
        profile.airControl.pitchTorque = readFloat(
            air, "pitchTorque", profile.airControl.pitchTorque, 0.0f, 1.0e7f);
        profile.airControl.rollTorque = readFloat(
            air, "rollTorque", profile.airControl.rollTorque, 0.0f, 1.0e7f);
        profile.airControl.pitchDamping = readFloat(
            air, "pitchDamping", profile.airControl.pitchDamping, 0.0f, 1.0e6f);
        profile.airControl.rollDamping = readFloat(
            air, "rollDamping", profile.airControl.rollDamping, 0.0f, 1.0e6f);
        profile.airControl.yawEnabled =
            readBool(air, "yawEnabled", profile.airControl.yawEnabled);
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
    mDirectory = directory;
    std::unordered_map<std::string, VehicleProfile> profiles;
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
        profilePaths[profile.id] = entry.path().string();
        profiles[profile.id] = std::move(profile);
    }

    mProfiles = std::move(profiles);
    mProfilePaths = std::move(profilePaths);
    mFileTimes = std::move(times);
    ++mRevision;
    Debug::log(Debug::Category::Physics,
               "[VEHICLE PROFILE] loaded %zu profiles from %s\n",
               mProfiles.size(), directory.c_str());
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

const VehicleProfile* VehicleProfileRegistry::find(const std::string& id) const
{
    auto it = mProfiles.find(id);
    return it != mProfiles.end() ? &it->second : nullptr;
}

std::vector<std::string> VehicleProfileRegistry::ids() const
{
    std::vector<std::string> out;
    out.reserve(mProfiles.size());
    for (const auto& [id, _] : mProfiles)
        out.push_back(id);
    std::sort(out.begin(), out.end());
    return out;
}
