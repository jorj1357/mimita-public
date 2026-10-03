// 10 02 2026
/* purpose
* Load per-map objective data from config/maps/<mapId>.json.
* Owns bomb-site definitions (id, position, radius, debug visibility) and the
* mode objective timers (plant/defuse/explosion seconds) for a loaded map.
* Editable at runtime via the site debug commands; JSON is the source of truth.
* Does NOT path, render, or own gameplay state (the server objective owns that).
*/
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <glm/glm.hpp>

struct BombSite
{
    std::string id;              // "A", "B", ...
    glm::vec3 position{0.0f};
    float radius = 4.0f;
    bool visibleDebug = false;
    bool hasPosition = false;    // false = not yet authored/verified
};

struct MapObjectiveConfig
{
    std::string mapId;
    std::vector<BombSite> bombSites;
    float plantSeconds = 3.0f;
    float defuseSeconds = 5.0f;
    float explosionSeconds = 40.0f;
    bool loaded = false;
};

class MapConfigRegistry
{
public:
    static MapConfigRegistry& instance();

    // Load (or reload) the config for a map id. Missing file -> defaults with
    // no sites (planting is disabled until sites are authored). Keeps the last
    // valid data on parse error.
    bool load(const std::string& mapId);
    // Re-read the current map file if it changed on disk.
    bool pollReload();

    const MapObjectiveConfig& current() const { return mCurrent; }
    const std::string& mapId() const { return mCurrent.mapId; }

    // Find a site by id; nullptr when absent.
    const BombSite* findSite(const std::string& id) const;
    // Index of the site containing a world position (within its radius), or -1.
    int siteIndexAt(const glm::vec3& position) const;

    // Runtime editing (writes back to the JSON file so positions persist).
    bool setSitePosition(const std::string& id, const glm::vec3& position);
    bool setSiteVisibility(const std::string& id, bool visible);
    bool save();

    // Resolve the config path for a map id under config/maps/.
    static std::string pathForMap(const std::string& mapId);

private:
    MapConfigRegistry() = default;
    std::string resolveConfigPath() const;

    MapObjectiveConfig mCurrent;
    std::filesystem::file_time_type mLastWrite{};
    std::string mPath;
};

// World-independent selftest for map objective config parsing + site lookup.
bool mapConfigSelfTest(std::string& report);
