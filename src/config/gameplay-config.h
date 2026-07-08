#pragma once

#include <filesystem>
#include <string>

enum class GameplayAimMode {
    Crosshair,
    WorldHit
};

const char* gameplayAimModeName(GameplayAimMode mode);

enum class DashMode {
    Glide,
    TF2
};

const char* dashModeName(DashMode mode);

struct GameplayConfigData {
    GameplayAimMode aimMode = GameplayAimMode::Crosshair;
    DashMode dashMode = DashMode::Glide;
};

class GameplayConfig {
public:
    static GameplayConfig& instance();

    bool load(const std::string& path = "config/gameplay.json");
    bool pollReload();

    GameplayAimMode aimMode() const { return mData.aimMode; }
    const char* aimModeName() const { return gameplayAimModeName(mData.aimMode); }
    DashMode dashMode() const { return mData.dashMode; }
    const char* dashModeName() const { return ::dashModeName(mData.dashMode); }

private:
    GameplayConfig() = default;

    GameplayConfigData mData;
    std::string mPath = "config/gameplay.json";
    std::filesystem::file_time_type mLastWrite{};
    bool mWatchLogged = false;
};
