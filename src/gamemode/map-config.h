// 09 11 2026
/* purpose
* Load and cache per-map gameplay metadata from config/maps/<mapId>.json.
* Owns team spawn points and bomb-plant zones for objective gamemodes.
* One owner: the gamemode runtime reads this instead of hardcoding positions.
* Does NOT load geometry, render, or simulate anything.
* Does NOT fail hard on bad JSON - keeps the last valid config and logs an error.
*/
#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

struct MapTeamSpawn
{
    glm::vec3 position{0.0f};
    float yaw = 0.0f;
};

struct MapBombSite
{
    std::string name;
    glm::vec3 center{0.0f};
    float radius = 6.0f;
};

struct MapConfig
{
    std::string id;
    bool loaded = false;
    // [0] = Terrorists, [1] = Counter-Terrorists. Empty = no per-team spawns.
    std::vector<MapTeamSpawn> teamSpawns[2];
    std::vector<MapBombSite> bombSites;
    float bombTimerSeconds = 40.0f;
    float bombPlantSeconds = 3.0f;
    float bombDefuseSeconds = 5.0f;
};

class MapConfigRegistry
{
public:
    static MapConfigRegistry& instance();

    // Returns the config for a map id (e.g. "dust2cyberiav3"), loading and
    // caching it on first use. A missing file returns a not-loaded config.
    const MapConfig& get(const std::string& mapId);

    bool pollReload();

private:
    MapConfigRegistry() = default;

    struct Entry
    {
        MapConfig config;
        std::string path;
        std::filesystem::file_time_type write{};
    };

    bool load(const std::string& mapId, Entry& entry);

    std::unordered_map<std::string, Entry> mEntries;
};
