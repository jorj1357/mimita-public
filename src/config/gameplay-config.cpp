#include "config/gameplay-config.h"

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

bool parseAimMode(const json& value, GameplayAimMode& out)
{
    if (!value.is_string())
        return false;

    const std::string mode = value.get<std::string>();
    if (mode == "crosshair") {
        out = GameplayAimMode::Crosshair;
        return true;
    }
    if (mode == "world_hit") {
        out = GameplayAimMode::WorldHit;
        return true;
    }
    if (mode == "farpoint") {
        out = GameplayAimMode::Farpoint;
        return true;
    }
    return false;
}

bool parseDashMode(const json& value, DashMode& out)
{
    if (!value.is_string())
        return false;

    const std::string mode = value.get<std::string>();
    if (mode == "glide") {
        out = DashMode::Glide;
        return true;
    }
    if (mode == "tf2") {
        out = DashMode::TF2;
        return true;
    }
    return false;
}

} // namespace

const char* dashModeName(DashMode mode)
{
    switch (mode) {
        case DashMode::Glide: return "glide";
        case DashMode::TF2: return "tf2";
    }
    return "glide";
}

const char* gameplayAimModeName(GameplayAimMode mode)
{
    switch (mode) {
        case GameplayAimMode::WorldHit: return "world_hit";
        case GameplayAimMode::Farpoint: return "farpoint";
        case GameplayAimMode::Crosshair:
        default: return "crosshair";
    }
}

GameplayConfig& GameplayConfig::instance()
{
    static GameplayConfig config;
    return config;
}

bool GameplayConfig::load(const std::string& path)
{
    if (mPath != path) {
        mPath = path;
        mWatchLogged = false;
    }

    const std::string fileName = fileNameOf(mPath);
    if (!mWatchLogged) {
        Debug::warn(Debug::Category::Weapons,
            "[GAMEPLAY CONFIG] Watching: %s\n", fileName.c_str());
        mWatchLogged = true;
    }

    const auto writeTime = getLastWrite(mPath);
    std::ifstream file(mPath);
    if (!file.is_open()) {
        mLastWrite = writeTime;
        Debug::warn(Debug::Category::Weapons,
            "[GAMEPLAY CONFIG] Missing %s; using defaults. Applied: aim_mode=%s\n",
            mPath.c_str(), aimModeName());
        return false;
    }

    try {
        json root;
        file >> root;

        GameplayConfigData next;
        if (root.contains("aim_mode") && !parseAimMode(root["aim_mode"], next.aimMode)) {
            mLastWrite = writeTime;
            Debug::error(Debug::Category::Weapons,
                "[GAMEPLAY CONFIG] Invalid aim_mode in %s; expected \"crosshair\", \"world_hit\", or \"farpoint\". Keeping previous valid settings.\n",
                mPath.c_str());
            return false;
        }
        if (root.contains("dash_mode") && !parseDashMode(root["dash_mode"], next.dashMode)) {
            mLastWrite = writeTime;
            Debug::error(Debug::Category::Weapons,
                "[GAMEPLAY CONFIG] Invalid dash_mode in %s; expected \"glide\" or \"tf2\". Keeping previous valid settings.\n",
                mPath.c_str());
            return false;
        }

        if (root.contains("farpoint_distance"))
            next.farpointDistance = std::max(1.0f, root["farpoint_distance"].get<float>());

        mData = next;
        mLastWrite = writeTime;
        Debug::warn(Debug::Category::Weapons,
            "[GAMEPLAY CONFIG] Loaded successfully: %s\n", fileName.c_str());
        Debug::warn(Debug::Category::Weapons,
            "[GAMEPLAY CONFIG] Applied: aim_mode=%s dash_mode=%s farpoint_distance=%.0f\n",
            aimModeName(), dashModeName(), mData.farpointDistance);
        return true;
    } catch (const json::parse_error& e) {
        mLastWrite = writeTime;
        Debug::error(Debug::Category::Weapons,
            "[GAMEPLAY CONFIG] Parse error in %s: %s. Keeping previous valid settings.\n",
            mPath.c_str(), e.what());
    } catch (const std::exception& e) {
        mLastWrite = writeTime;
        Debug::error(Debug::Category::Weapons,
            "[GAMEPLAY CONFIG] Error loading %s: %s. Keeping previous valid settings.\n",
            mPath.c_str(), e.what());
    }
    return false;
}

bool GameplayConfig::pollReload()
{
    const auto writeTime = getLastWrite(mPath);
    if (writeTime == std::filesystem::file_time_type{} || writeTime == mLastWrite)
        return false;

    Debug::warn(Debug::Category::Weapons,
        "[GAMEPLAY CONFIG] Detected change: %s\n", fileNameOf(mPath).c_str());
    Debug::warn(Debug::Category::Weapons,
        "[GAMEPLAY CONFIG] Reloading...\n");
    return load(mPath);
}
