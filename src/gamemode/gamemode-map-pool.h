// 08 10 2026, 14 34
/* purpose
* Declares the allowed gamemode maps pool loaded from config/gamemode-good-maps.json.
* Provides randomMap() for the client to pick its map when queueing, and list()
* for the server to auto-rotate maps between rematches.
* Hot-reloads so map lists can change live without a rebuild.
* Does NOT own queue/matchmaking, gamemode rules, or gameplay logic.
*/

#pragma once

#include <filesystem>
#include <string>
#include <vector>

class GamemodeMapPool
{
public:
    static GamemodeMapPool& instance();

    void load(const std::string& path);
    void pollReload();

    std::vector<std::string> list() const { return mMaps; }
    // Random map from the pool (falls back to a default if empty).
    std::string randomMap() const;
    bool has(const std::string& mapId) const;

private:
    GamemodeMapPool() = default;

    std::vector<std::string> mMaps;
    std::string mPath;
    std::filesystem::file_time_type mLastWrite;
};
