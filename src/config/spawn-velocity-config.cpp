// 2026-09-08 12:01 EST
/* purpose
* implementation of hot-reloadable spawn velocity config
* loads and parses config/spawnvelocity.json via nlohmann::json
* provides computeSpawnImpulse() to calculate the one-tick velocity on respawn
* hot-reloads when the JSON file is saved at runtime
* does NOT apply impulse directly — callers in death-system.cpp and server-players.cpp
*/

#include "config/spawn-velocity-config.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

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

float readJsonFloat(const json& j, const char* key, float def)
{
    return j.contains(key) ? j[key].get<float>() : def;
}

bool readJsonBool(const json& j, const char* key, bool def)
{
    return j.contains(key) ? j[key].get<bool>() : def;
}

std::string readJsonString(const json& j, const char* key, const std::string& def)
{
    return j.contains(key) ? j[key].get<std::string>() : def;
}

glm::vec3 readJsonVec3(const json& j, const char* key, glm::vec3 def)
{
    if (!j.contains(key)) return def;
    const auto& arr = j[key];
    if (arr.is_array() && arr.size() >= 3)
        return {arr[0].get<float>(), arr[1].get<float>(), arr[2].get<float>()};
    return def;
}

glm::vec2 readJsonVec2(const json& j, const char* key, glm::vec2 def)
{
    if (!j.contains(key)) return def;
    const auto& arr = j[key];
    if (arr.is_array() && arr.size() >= 2)
        return {arr[0].get<float>(), arr[1].get<float>()};
    return def;
}

} // anonymous namespace

SpawnVelocityConfig& SpawnVelocityConfig::instance()
{
    static SpawnVelocityConfig config;
    return config;
}

bool SpawnVelocityConfig::load(const std::string& path)
{
    mPath = path;
    if (!mWatchLogged) {
        Debug::log(Debug::Category::General, "[SPAWN VELOCITY CONFIG] watching %s\n",
                   fileNameOf(mPath).c_str());
        mWatchLogged = true;
    }

    std::ifstream file(mPath);
    if (!file.is_open()) {
        Debug::warn(Debug::Category::General, "[SPAWN VELOCITY CONFIG] could not open %s\n",
                    mPath.c_str());
        return false;
    }

    try {
        json j;
        file >> j;

        mData.enabled = readJsonBool(j, "enabled", mData.enabled);
        mData.mode = readJsonString(j, "mode", mData.mode);
        mData.speed = readJsonFloat(j, "speed", mData.speed);
        mData.direction = readJsonVec3(j, "direction", mData.direction);
        mData.applyVertical = readJsonBool(j, "apply_vertical", mData.applyVertical);
        mData.randomRange = readJsonVec2(j, "random_range", mData.randomRange);

        mLastWrite = getLastWrite(mPath);

        Debug::log(Debug::Category::General,
                   "[SPAWN VELOCITY CONFIG] loaded: enabled=%d mode=%s speed=%.1f "
                   "dir=(%.1f,%.1f,%.1f) vertical=%d range=(%.0f,%.0f)\n",
                   (int)mData.enabled, mData.mode.c_str(), mData.speed,
                   mData.direction.x, mData.direction.y, mData.direction.z,
                   (int)mData.applyVertical, mData.randomRange.x, mData.randomRange.y);
    } catch (const json::parse_error& e) {
        Debug::warn(Debug::Category::General,
                    "[SPAWN VELOCITY CONFIG] JSON parse error: %s\n", e.what());
        return false;
    } catch (const std::exception& e) {
        Debug::warn(Debug::Category::General,
                    "[SPAWN VELOCITY CONFIG] error: %s\n", e.what());
        return false;
    }

    return true;
}

bool SpawnVelocityConfig::pollReload()
{
    const auto writeTime = getLastWrite(mPath);
    if (writeTime == std::filesystem::file_time_type{} || writeTime == mLastWrite)
        return false;
    return load(mPath);
}

glm::vec3 SpawnVelocityConfig::computeSpawnImpulse(float playerYaw) const
{
    if (!mData.enabled)
        return glm::vec3(0.0f);

    const float speed = mData.speed;
    glm::vec3 dir(0.0f);

    if (mData.mode == "fixed") {
        const float len = glm::length(mData.direction);
        dir = len > 0.001f ? mData.direction / len : glm::vec3(1.0f, 0.0f, 0.0f);
    }
    else if (mData.mode == "look") {
        const float rad = glm::radians(playerYaw);
        dir = glm::vec3(std::cos(rad), std::sin(rad), 0.0f);
    }
    else if (mData.mode == "random") {
        const float minA = mData.randomRange.x;
        const float maxA = mData.randomRange.y;
        const float angle = minA + static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * (maxA - minA);
        const float rad = glm::radians(angle);
        dir = glm::vec3(std::cos(rad), std::sin(rad), 0.0f);
    }
    else if (mData.mode == "world_x") {
        dir = glm::vec3(1.0f, 0.0f, 0.0f);
    }
    else if (mData.mode == "world_y") {
        dir = glm::vec3(0.0f, 1.0f, 0.0f);
    }
    else if (mData.mode == "world_z") {
        dir = glm::vec3(0.0f, 0.0f, 1.0f);
    }
    else if (mData.mode == "radial") {
        const float rad = glm::radians(
            static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 360.0f);
        dir = glm::vec3(std::cos(rad), std::sin(rad), 0.0f);
    }
    else {
        Debug::warn(Debug::Category::General,
                    "[SPAWN VELOCITY] unknown mode '%s', falling back to random\n",
                    mData.mode.c_str());
        const float rad = glm::radians(
            static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 360.0f);
        dir = glm::vec3(std::cos(rad), std::sin(rad), 0.0f);
    }

    if (!mData.applyVertical)
        dir.z = 0.0f;

    return dir * speed;
}
