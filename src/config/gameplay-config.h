#pragma once

#include <filesystem>
#include <string>

enum class GameplayAimMode {
    Crosshair,
    WorldHit
};

const char* gameplayAimModeName(GameplayAimMode mode);

struct GameplayConfigData {
    GameplayAimMode aimMode = GameplayAimMode::Crosshair;
};

class GameplayConfig {
public:
    static GameplayConfig& instance();

    bool load(const std::string& path = "config/gameplay.json");
    bool pollReload();

    GameplayAimMode aimMode() const { return mData.aimMode; }
    const char* aimModeName() const { return gameplayAimModeName(mData.aimMode); }

private:
    GameplayConfig() = default;

    GameplayConfigData mData;
    std::string mPath = "config/gameplay.json";
    std::filesystem::file_time_type mLastWrite{};
    bool mWatchLogged = false;
};
