#pragma once

#include <filesystem>
#include <string>

enum class GameplayAimMode {
    Crosshair,
    WorldHit,
    Farpoint,
    CamForward,
    Physical
};

const char* gameplayAimModeName(GameplayAimMode mode);

// Parse a JSON/config string ("crosshair", "world_hit", "farpoint",
// "camforward", "physical") into an aim mode. Returns false for unknown values
// so callers can warn once and keep their existing behavior.
bool gameplayAimModeFromString(const std::string& value, GameplayAimMode& out);

enum class DashMode {
    Glide,
    TF2
};

const char* dashModeName(DashMode mode);

struct GameplayConfigData {
    GameplayAimMode aimMode = GameplayAimMode::Crosshair;
    DashMode dashMode = DashMode::Glide;
    float farpointDistance = 1000000.0f;
};

class GameplayConfig {
public:
    static GameplayConfig& instance();

    bool load(const std::string& path = "config/gameplay.json");
    bool pollReload();

    // Effective value: the active gamemode override when one is set, otherwise
    // the player's config/gameplay.json value. The override is process-local and
    // is applied by the gamemode/client state owner, never persisted.
    GameplayAimMode aimMode() const { return mAimOverrideActive ? mAimOverride : mData.aimMode; }
    const char* aimModeName() const { return gameplayAimModeName(aimMode()); }
    DashMode dashMode() const { return mData.dashMode; }
    const char* dashModeName() const { return ::dashModeName(mData.dashMode); }
    float farpointDistance() const { return mData.farpointDistance; }

    // Force (or clear) the aim mode for all actors while a gamemode is active.
    void setAimModeOverride(GameplayAimMode mode) { mAimOverride = mode; mAimOverrideActive = true; }
    void clearAimModeOverride() { mAimOverrideActive = false; }
    bool hasAimModeOverride() const { return mAimOverrideActive; }

private:
    GameplayConfig() = default;

    GameplayConfigData mData;
    GameplayAimMode mAimOverride = GameplayAimMode::Crosshair;
    bool mAimOverrideActive = false;
    std::string mPath = "config/gameplay.json";
    std::filesystem::file_time_type mLastWrite{};
    bool mWatchLogged = false;
};
