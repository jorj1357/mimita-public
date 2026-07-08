#include "config/camera-config.h"

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

} // namespace

CamConfig& CamConfig::instance()
{
    static CamConfig config;
    return config;
}

bool CamConfig::load(const std::string& path)
{
    if (mPath != path) {
        mPath = path;
        mWatchLogged = false;
    }

    const std::string fileName = fileNameOf(mPath);
    if (!mWatchLogged) {
        Debug::warn(Debug::Category::General,
            "[CAM CONFIG] Watching: %s\n", fileName.c_str());
        mWatchLogged = true;
    }

    const auto writeTime = getLastWrite(mPath);
    std::ifstream file(mPath);
    if (!file.is_open()) {
        mLastWrite = writeTime;
        Debug::warn(Debug::Category::General,
            "[CAM CONFIG] Missing %s; using defaults.\n", mPath.c_str());
        return false;
    }

    try {
        json root;
        file >> root;

        CameraConfigData next;
        if (root.contains("thirdPerson")) {
            const auto& tp = root["thirdPerson"];

            if (tp.contains("offset")) {
                next.offset.x = tp["offset"].value("x", next.offset.x);
                next.offset.y = tp["offset"].value("y", next.offset.y);
                next.offset.z = tp["offset"].value("z", next.offset.z);
            }
            next.fov = tp.value("fov", next.fov);
            next.positionStiffness = tp.value("positionStiffness", next.positionStiffness);
            next.rotationStiffness = tp.value("rotationStiffness", next.rotationStiffness);
            next.stiffnessEnabled = tp.value("stiffnessEnabled", next.stiffnessEnabled);
            next.collisionEnabled = tp.value("collisionEnabled", next.collisionEnabled);
            next.lookAheadDistance = tp.value("lookAheadDistance", next.lookAheadDistance);
        }

        mData = next;
        mLastWrite = writeTime;
        Debug::warn(Debug::Category::General,
            "[CAM CONFIG] Loaded successfully: %s  "
            "offset=(%.1f %.1f %.1f) fov=%.0f stiffness=%.2f stiffEnabled=%d collision=%d\n",
            fileName.c_str(),
            mData.offset.x, mData.offset.y, mData.offset.z,
            mData.fov, mData.positionStiffness, (int)mData.stiffnessEnabled, (int)mData.collisionEnabled);
        return true;
    } catch (const json::parse_error& e) {
        mLastWrite = writeTime;
        Debug::error(Debug::Category::General,
            "[CAM CONFIG] Parse error in %s: %s. Keeping previous valid settings.\n",
            mPath.c_str(), e.what());
    } catch (const std::exception& e) {
        mLastWrite = writeTime;
        Debug::error(Debug::Category::General,
            "[CAM CONFIG] Error loading %s: %s. Keeping previous valid settings.\n",
            mPath.c_str(), e.what());
    }
    return false;
}

bool CamConfig::pollReload()
{
    const auto writeTime = getLastWrite(mPath);
    if (writeTime == std::filesystem::file_time_type{} || writeTime == mLastWrite)
        return false;

    Debug::warn(Debug::Category::General,
        "[CAM CONFIG] Detected change: %s\n", fileNameOf(mPath).c_str());
    Debug::warn(Debug::Category::General,
        "[CAM CONFIG] Reloading...\n");
    return load(mPath);
}
