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
#include <cstdint>
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

struct MapEntity
{
    std::string id;
    std::string type;
    glm::vec3 position{0.0f};
    glm::vec3 size{1.0f};
    float radius = 1.0f;
    // Trigger shape for volume entities: "sphere" (radius) or "box" (size).
    // Empty keeps the legacy inference (box when size != 1x1x1, else sphere).
    std::string shape;
    bool enabled = true;
    bool visible = true;
    bool oneShot = false;
    std::string tag;
    std::string monsterPool = "default";
    // Role id spawned by a monster_zone (e.g. "zombie"). Empty = global NPC default.
    std::string monsterRole;
    std::string pickupId;
    std::string bossId;
    std::string damageType = "generic";
    int spawnCount = 1;
    int maxAlive = 1;
    int damage = 0;
    int damageIntervalTicks = 60;
    int spawnCooldownTicks = 60;
    int checkpointRequirement = 0;
    // Monotonic order for checkpoints so progress cannot go backwards.
    int checkpointIndex = 0;
    bool runtimeActivated = false;
    uint32_t lastActivationTick = 0;
};

struct MapObjectiveConfig
{
    std::string mapId;
    std::vector<BombSite> bombSites;
    std::vector<MapEntity> entities;
    float plantSeconds = 3.0f;
    float defuseSeconds = 5.0f;
    float explosionSeconds = 40.0f;
    bool loaded = false;
};

// True when a world point lies inside an authored entity's trigger volume: its
// sphere of `radius`, or its box of `size` when the size exceeds the 1x1x1
// default. Shared by the server run runtime and the debug/editor visuals so the
// authored volume has exactly one owner.
bool mapEntityContainsPoint(const MapEntity& entity, const glm::vec3& point);

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

    const MapEntity* findEntity(const std::string& id) const;
    MapEntity* findEntityMutable(const std::string& id);
    std::vector<MapEntity>& entitiesMutable() { return mCurrent.entities; }
    std::string createEntity(const std::string& type, const glm::vec3& position,
                             const std::string& requestedId = "");
    bool deleteEntity(const std::string& id);
    void setEntityVisibility(bool visible);
    bool entityVisibility() const { return mEntityVisibility; }
    void setEntityVisibilityFor(const std::string& id, bool visible);
    const std::string& configPath() const { return mPath; }

    // Resolve the config path for a map id under config/maps/.
    static std::string pathForMap(const std::string& mapId);

private:
    MapConfigRegistry() = default;
    std::string resolveConfigPath() const;

    MapObjectiveConfig mCurrent;
    bool mEntityVisibility = false;
    std::filesystem::file_time_type mLastWrite{};
    std::string mPath;
};

// World-independent selftest for map objective config parsing + site lookup.
bool mapConfigSelfTest(std::string& report);

// Focused selftest for authored entity trigger geometry and save round-trip
// (unknown top-level keys are preserved across save).
bool mapEntityConfigSelfTest(std::string& report);
