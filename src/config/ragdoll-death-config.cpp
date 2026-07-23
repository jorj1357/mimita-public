#include "config/ragdoll-death-config.h"

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

float readJsonFloat(const json& j, const char* key, float def)
{
    return j.contains(key) ? j[key].get<float>() : def;
}

bool readJsonBool(const json& j, const char* key, bool def)
{
    return j.contains(key) ? j[key].get<bool>() : def;
}

int readJsonInt(const json& j, const char* key, int def)
{
    return j.contains(key) ? j[key].get<int>() : def;
}

glm::vec3 readJsonVec3(const json& j, const char* key, glm::vec3 def)
{
    if (!j.contains(key)) return def;
    const auto& arr = j[key];
    if (arr.is_array() && arr.size() >= 3)
        return {arr[0].get<float>(), arr[1].get<float>(), arr[2].get<float>()};
    return def;
}

}

RagdollDeathConfig& RagdollDeathConfig::instance()
{
    static RagdollDeathConfig config;
    return config;
}

bool RagdollDeathConfig::load(const std::string& path)
{
    mPath = path;
    if (!mWatchLogged) {
        Debug::log(Debug::Category::General, "[RAGDOLL DEATH CONFIG] watching %s\n", fileNameOf(mPath).c_str());
        mWatchLogged = true;
    }

    std::ifstream file(mPath);
    if (!file.is_open()) {
        Debug::warn(Debug::Category::General, "[RAGDOLL DEATH CONFIG] could not open %s\n", mPath.c_str());
        return false;
    }

    try {
        json j;
        file >> j;

        mData.enabled = readJsonBool(j, "enabled", mData.enabled);
        mData.totalTicks = readJsonInt(j, "totalTicks", mData.totalTicks);
        mData.startAlpha = readJsonFloat(j, "startAlpha", mData.startAlpha);
        mData.endAlpha = readJsonFloat(j, "endAlpha", mData.endAlpha);
        mData.startRotation = readJsonVec3(j, "startRotation", mData.startRotation);
        mData.endRotation = readJsonVec3(j, "endRotation", mData.endRotation);

        mLastWrite = getLastWrite(mPath);

        Debug::log(Debug::Category::General, "[RAGDOLL DEATH CONFIG] loaded: totalTicks=%d startAlpha=%.2f endAlpha=%.2f rot=(%.0f,%.0f,%.0f)->(%.0f,%.0f,%.0f)\n",
                   mData.totalTicks, mData.startAlpha, mData.endAlpha,
                   mData.startRotation.x, mData.startRotation.y, mData.startRotation.z,
                   mData.endRotation.x, mData.endRotation.y, mData.endRotation.z);
    } catch (const json::parse_error& e) {
        Debug::warn(Debug::Category::General, "[RAGDOLL DEATH CONFIG] JSON parse error: %s\n", e.what());
        return false;
    } catch (const std::exception& e) {
        Debug::warn(Debug::Category::General, "[RAGDOLL DEATH CONFIG] error: %s\n", e.what());
        return false;
    }

    return true;
}

bool RagdollDeathConfig::pollReload()
{
    const auto writeTime = getLastWrite(mPath);
    if (writeTime == std::filesystem::file_time_type{} || writeTime == mLastWrite)
        return false;
    return load(mPath);
}
