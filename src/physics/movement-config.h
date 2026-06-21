#pragma once

#include <string>

struct MovementConfig {
    static MovementConfig& instance();

    bool load(const std::string& path = "config/movement.json");
    void pollReload();

    const std::string& lastError() const { return mLastError; }

    // TODO: per-game-mode configs — merge / override from game mode profile
    // TODO: per-map configs — load map-specific overrides from map metadata
    // TODO: per-character configs — apply per-character tuning profiles
    // TODO: server-authoritative configs — enforce values from server in multiplayer

private:
    MovementConfig() = default;

    std::string mPath = "config/movement.json";
    std::string mLastError;
    int64_t mLastWriteTime = 0;
};
