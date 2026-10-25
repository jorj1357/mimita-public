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
    // Optional spawn yaw for authored spawnpoint entities (degrees).
    float yaw = 0.0f;
    float radius = 1.0f;
    bool enabled = true;
    bool visible = true;
    bool oneShot = false;
    std::string tag;
    std::string monsterPool = "default";
    std::string pickupId;
    std::string bossId;
    std::string damageType = "generic";
    int spawnCount = 1;
    int maxAlive = 1;
    int damage = 0;
    int damageIntervalTicks = 60;
    int spawnCooldownTicks = 60;
    int checkpointRequirement = 0;
    // Zombie Tower run data. `order` is the checkpoint progression index;
    // `required` marks an encounter that must be cleared before the run can win.
    int order = 0;
    bool required = false;
    // 0 = use `radius` for monster-zone activation.
    float activationRadius = 0.0f;
    // Boss placeholder health multiplier (1 = role base health).
    float healthMultiplier = 1.0f;
    // Runtime-only activation state, never serialized.
    int runtimeSpawned = 0;
    bool runtimeCleared = false;
    bool runtimeActivated = false;
    bool runtimePlayerActivated = false;
    uint32_t lastActivationTick = 0;
};

struct AuthoredBlock
{
    std::string id;
    std::string shape = "box";
    glm::vec3 position{0.0f};
    glm::vec3 size{1.0f};
    glm::vec3 rotationEuler{0.0f};
    std::string material;
    bool enabled = true;
    std::vector<std::string> tags;
};

struct PayloadRoutePoint
{
    std::string id;
    glm::vec3 position{0.0f};
    float radius = 2.0f;
    int order = 0;
    int waitTicks = 0;
    bool enabled = true;
    // A route point is a checkpoint unless explicitly authored otherwise.
    bool checkpoint = true;
    // The final route point wins the round for the attacking team.
    bool final = false;
    // Extra clock time awarded once when this checkpoint is reached.
    float timeBonusSeconds = 0.0f;
};

struct PayloadConfig
{
    bool present = false;
    bool hasCartOrigin = false;
    glm::vec3 cartOrigin{0.0f};
    float cartYaw = 0.0f;
    // "route" follows the route tangent; "fixed" keeps cart_yaw for the whole
    // round and is hot-reloadable.
    bool cartFixedFacing = false;
    // Physical/visual cart box dimensions (meters), separate from the
    // gameplay interaction box so the GLB model size never changes the area.
    glm::vec3 cartSize{2.0f, 1.5f, 1.0f};
    glm::vec3 interactionSize{3.0f, 2.25f, 1.5f};
    glm::vec3 interactionOffset{0.0f};
    glm::vec3 interactionRotationEuler{0.0f};
    std::vector<PayloadRoutePoint> route;
};

struct MapObjectiveConfig
{
    std::string mapId;
    std::vector<BombSite> bombSites;
    std::vector<MapEntity> entities;
    std::vector<AuthoredBlock> blocks;
    PayloadConfig payload;
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

    const AuthoredBlock* findBlock(const std::string& id) const;
    AuthoredBlock* findBlockMutable(const std::string& id);
    const std::vector<AuthoredBlock>& blocks() const { return mCurrent.blocks; }
    std::vector<AuthoredBlock>& blocksMutable() { return mCurrent.blocks; }
    std::string createBlock(const glm::vec3& position, const glm::vec3& size,
                            const std::string& requestedId = "");
    bool deleteBlock(const std::string& id);

    const PayloadConfig& payload() const { return mCurrent.payload; }
    PayloadConfig& payloadMutable() { return mCurrent.payload; }
    const PayloadRoutePoint* findPayloadRoutePoint(const std::string& id) const;
    PayloadRoutePoint* findPayloadRoutePointMutable(const std::string& id);
    std::string createPayloadRoutePoint(const glm::vec3& position,
                                        const std::string& requestedId = "");
    bool deletePayloadRoutePoint(const std::string& id);
    void renumberPayloadRoute();

    // Monotonic authored-data revision. Editor history and dirty state compare
    // this instead of polling pointers or timestamps.
    uint64_t revision() const { return mRevision; }
    uint64_t savedRevision() const { return mSavedRevision; }
    bool dirty() const { return mRevision != mSavedRevision; }
    void markChanged();
    MapObjectiveConfig snapshotAuthored() const;
    void restoreAuthored(const MapObjectiveConfig& snapshot);

    // Clear runtime activation state for a new run without reloading the file.
    void resetRuntimeState();
    const std::string& configPath() const { return mPath; }

    // Resolve the config path for a map id under config/maps/.
    static std::string pathForMap(const std::string& mapId);

private:
    MapConfigRegistry() = default;
    std::string resolveConfigPath() const;
    std::string uniqueId(const std::string& prefix) const;
    bool writeBackup();
    void pruneBackups();

    MapObjectiveConfig mCurrent;
    bool mEntityVisibility = false;
    std::filesystem::file_time_type mLastWrite{};
    std::string mPath;
    uint64_t mRevision = 0;
    uint64_t mSavedRevision = 0;
};

// World-independent selftest for map objective config parsing + site lookup.
bool mapConfigSelfTest(std::string& report);

// Focused selftest for authored entity trigger geometry and save round-trip
// (unknown top-level keys are preserved across save).
bool mapEntityConfigSelfTest(std::string& report);
