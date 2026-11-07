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
    std::string name;            // display name; id stays stable
};

struct AuthoredChunkConnector
{
    std::string id;
    std::string side;        // east, west, north, south, up, down
    std::string orientation; // doorway, tunnel, open, ...
    std::string role = "bidirectional"; // entry, exit, bidirectional, none
    std::vector<std::string> tags;
    // Explicit links override auto-compatible generation. Entries are
    // "<chunkId>/<connectorId>" and are stored on both endpoints.
    std::vector<std::string> links;
    glm::vec3 position{0.0f};
    // Contact tolerance used when matching a neighbor connector.
    float radius = 0.5f;
    std::string name;
};

struct ChunkPart
{
    std::string id;
    std::string kind = "box"; // box, wall, floor, ceiling, tunnel, doorway, prop
    glm::vec3 position{0.0f};
    glm::vec3 size{1.0f};
    glm::vec3 rotationEuler{0.0f};
    // Per-part default material and optional per-face overrides.
    // Face order: +X, -X, +Y, -Y, +Z, -Z.
    std::string material;
    std::string faceMaterials[6];
    // Default primitive UVs repeat once per world unit. These multipliers
    // scale that density; per-face entries override individual axes when > 0.
    glm::vec2 uvScale{1.0f, 1.0f};
    glm::vec2 faceUvScale[6]{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f},
                            {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}};
    bool collision = true;
    bool visible = true;
    std::string name;
};

struct ChunkSpawnPoint
{
    std::string id;
    std::string name;
    glm::vec3 position{0.0f};
    float radius = 0.5f;
    // "human", "npc", or "any".
    std::string allowedActors = "any";
    bool enabled = true;
};

struct AuthoredChunk
{
    std::string id;
    std::string geometryPath;
    std::string blobSha256;
    glm::vec3 boundsMin{0.0f};
    glm::vec3 boundsMax{0.0f};
    std::vector<ChunkPart> parts;
    std::vector<AuthoredChunkConnector> connectors;
    std::vector<ChunkSpawnPoint> spawnPoints;
    std::vector<std::string> tags;
    bool enabled = true;
    std::string name;
};

struct GenerationZone
{
    std::string id;
    std::string name;
    glm::vec3 boundsMin{0.0f};
    glm::vec3 boundsMax{0.0f};
    uint32_t seed = 0;
    std::string generatorVersion = "1";
    std::string ruleSet = "default";
    std::vector<std::string> chunkDependencies;
    std::string modeId;
    // Grid model. XY is the default horizontal plane; Z is up.
    std::string plane = "xy";
    glm::ivec3 repeatAxes{1, 1, 0};
    glm::vec3 origin{0.0f};
    glm::vec3 cellSize{104.0f, 104.0f, 104.0f};
    glm::vec3 chunkSizeVec{104.0f, 104.0f, 104.0f};
    glm::ivec3 streamRadius{1, 1, 0};
    std::string spacingMode = "center"; // center, connector
    float connectorGap = 0.0f;
    int maxBranchDepth = 8;
    bool allowCycles = true;
    bool allowCrossings = true;
    // Legacy scalar kept for older map files and the existing room streamer.
    float chunkSize = 104.0f;
    int streamRadiusChunks = 1;
    bool enabled = true;
};

struct NpcPreset
{
    std::string id;
    std::string roleId;
    std::string movementProfile;
    std::string weaponSet;
    std::string appearance;
    std::string animation;
    int rewardGold = 0;
    int rewardXp = 0;
    std::vector<std::string> tags;
    bool enabled = true;
};

struct WorldWeaponOverride
{
    std::string id;
    std::string weaponId;
    std::string field;
    std::string value;
    bool enabled = true;
};

struct AuthoredInteraction
{
    std::string id;
    std::string actionId;
    std::string prompt;
    std::string targetKind;
    std::string targetId;
    float range = 3.0f;
    // "press" activates once on the F key-down edge. "hold" accumulates fixed
    // simulation ticks and only activates on completion.
    std::string inputMode = "press";
    int holdTicks = 0;
    bool enabled = true;
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
    // Reusable authoring references and world-defined rewards.
    std::string presetId;
    std::string interactionId;
    int rewardGold = 0;
    int rewardXp = 0;
    std::string name;            // display name; id stays stable
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
    // Optional per-face material overrides. Face order: +X, -X, +Y, -Y, +Z, -Z.
    // Empty entries fall back to `material`.
    std::string faceMaterials[6];
    bool enabled = true;
    std::vector<std::string> tags;
    std::string name;
};

// One authored vehicle placement. The template owns chassis/wheel/tuning data;
// this record owns only where an instance starts in the map.
struct MapVehicleSpawn
{
    std::string id;
    std::string templateId = "car_sedan";
    glm::vec3 position{0.0f};
    float yaw = 0.0f;
    bool enabled = true;
    std::string name;
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
    std::string name;
};

// One authored-data problem found by validatePayloadAuthoring. Warnings keep the
// map playable but describe data the author should fix; errors mean the runtime
// cannot build a usable cart route.
struct PayloadAuthoringIssue
{
    std::string severity;   // "error" | "warning" | "info"
    std::string code;
    std::string message;
};

struct PayloadConfig;

// Validate cart origin, route ordering, checkpoint/final flags, segment length,
// radius, wait ticks, and time bonuses. Returns false when any error exists.
bool validatePayloadAuthoring(const PayloadConfig& payload,
                              std::vector<PayloadAuthoringIssue>& issues);

// Load the configured Payload maps and report authoring issues. Checks data and
// ownership only; it is not gameplay or visual acceptance.
bool payloadAuthoringSelfTest(std::string& report);

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

struct MapGLBObjectOverride
{
    std::string id;
    std::string name;
    glm::vec3 position{0.0f};
    std::string material;
};

struct ArenaBallConfig
{
    bool present = false;
    glm::vec3 spawn{0.0f, 0.0f, 2.0f};
    float radius = 0.9125f;
    float visualRadius = 0.95f;
    float massKg = 30.0f;
    float restitution = 0.6f;
    float friction = 0.35f;
    float linearDamping = 0.03f;
    float maxSpeed = 60.0f;
};

struct ArenaGoalConfig
{
    std::string id;
    std::string team;
    glm::vec3 center{0.0f};
    glm::vec3 halfExtents{1.0f, 1.0f, 1.0f};
    glm::vec3 facing{0.0f, 0.0f, 1.0f};
    bool enabled = true;
};

struct ArenaKickoffConfig
{
    std::string id;
    std::string team;
    glm::vec3 position{0.0f};
    float yaw = 0.0f;
    bool enabled = true;
};

struct ArenaConfig
{
    bool present = false;
    ArenaBallConfig ball;
    std::vector<ArenaGoalConfig> goals;
    std::vector<ArenaKickoffConfig> kickoffs;
};

struct BoostPadConfig
{
    std::string id;
    std::string kind = "small";
    glm::vec3 position{0.0f};
    float radius = 1.0f;
    int rechargeTicks = 240;
    bool enabled = true;
};

struct MapAttachment
{
    std::string id;
    std::string name;
    std::string parentKind;
    std::string parentId;
    std::string childKind;
    std::string childId;
    bool anchored = true;
    float mass = 1.0f;
    float density = 700.0f;
    float materialStrength = 100.0f;
};

struct MapObjectiveConfig
{
    std::string mapId;
    std::vector<BombSite> bombSites;
    std::vector<MapEntity> entities;
    std::vector<AuthoredBlock> blocks;
    std::vector<MapVehicleSpawn> vehicles;
    std::vector<MapGLBObjectOverride> glbObjects;
    std::vector<AuthoredChunk> chunks;
    std::vector<MapAttachment> attachments;
    std::vector<GenerationZone> generationZones;
    std::vector<NpcPreset> npcPresets;
    std::vector<WorldWeaponOverride> weaponOverrides;
    std::vector<AuthoredInteraction> interactions;
    bool generateWholeWorld = false;
    PayloadConfig payload;
    ArenaConfig arena;
    std::vector<BoostPadConfig> boostPads;
    float plantSeconds = 3.0f;
    float defuseSeconds = 5.0f;
    float explosionSeconds = 40.0f;
    bool loaded = false;
};

// Validate the authored arena block fail-closed: a ball spawn and at least one
// enabled goal are required before the mode runtime may score anything.
bool validateArenaAuthoring(const MapObjectiveConfig& config,
                            std::vector<PayloadAuthoringIssue>& issues);

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
    // Load a project/version document from an explicit path without moving it
    // into config/maps. Used by the project browser and live edit session.
    bool loadFromFile(const std::string& mapId, const std::string& path);
    // Serialize the current authored document. The base preserves unknown
    // top-level keys during a normal file save.
    std::string toJsonString(bool pretty = true) const;
    // Re-read the current map file if it changed on disk.
    bool pollReload();

    const MapObjectiveConfig& current() const { return mCurrent; }
    const std::string& mapId() const { return mCurrent.mapId; }

    // Find a site by id; nullptr when absent.
    const BombSite* findSite(const std::string& id) const;
    // Index of the site containing a world position (within its radius), or -1.
    int siteIndexAt(const glm::vec3& position) const;

    // Runtime editing (writes back to the JSON file so positions persist).
    std::vector<BombSite>& bombSitesMutable() { return mCurrent.bombSites; }
    const std::vector<BombSite>& bombSites() const { return mCurrent.bombSites; }
    std::string createBombSite(const glm::vec3& position, float radius = 4.0f,
                               const std::string& requestedId = "");
    bool deleteBombSite(const std::string& id);
    bool setSitePosition(const std::string& id, const glm::vec3& position);
    bool setSiteVisibility(const std::string& id, bool visible);
    bool setSiteRadius(const std::string& id, float radius);
    bool save(bool withBackup = true);

    const MapEntity* findEntity(const std::string& id) const;
    MapEntity* findEntityMutable(const std::string& id);
    std::vector<MapEntity>& entitiesMutable() { return mCurrent.entities; }
    std::string createEntity(const std::string& type, const glm::vec3& position,
                             const std::string& requestedId = "");
    bool deleteEntity(const std::string& id);

    const AuthoredBlock* findBlock(const std::string& id) const;
    AuthoredBlock* findBlockMutable(const std::string& id);
    const std::vector<AuthoredBlock>& blocks() const { return mCurrent.blocks; }
    std::vector<AuthoredBlock>& blocksMutable() { return mCurrent.blocks; }
    std::string createBlock(const glm::vec3& position, const glm::vec3& size,
                            const std::string& requestedId = "");
    bool deleteBlock(const std::string& id);

    const MapVehicleSpawn* findVehicle(const std::string& id) const;
    MapVehicleSpawn* findVehicleMutable(const std::string& id);
    const std::vector<MapVehicleSpawn>& vehicles() const { return mCurrent.vehicles; }
    std::vector<MapVehicleSpawn>& vehiclesMutable() { return mCurrent.vehicles; }
    std::string createVehicle(const std::string& templateId,
                              const glm::vec3& position, float yaw,
                              const std::string& requestedId = "");
    bool deleteVehicle(const std::string& id);

    const std::vector<MapGLBObjectOverride>& glbObjects() const { return mCurrent.glbObjects; }
    std::vector<MapGLBObjectOverride>& glbObjectsMutable() { return mCurrent.glbObjects; }
    const MapGLBObjectOverride* findGLBObjectOverride(const std::string& id) const;
    MapGLBObjectOverride* findGLBObjectOverrideMutable(const std::string& id);
    void setGLBObjectOverride(const std::string& id, const std::string& name,
                              const glm::vec3& position);
    void removeGLBObjectOverride(const std::string& id);

    std::vector<AuthoredChunk>& chunksMutable() { return mCurrent.chunks; }
    const std::vector<AuthoredChunk>& chunks() const { return mCurrent.chunks; }
    std::vector<MapAttachment>& attachmentsMutable() { return mCurrent.attachments; }
    const std::vector<MapAttachment>& attachments() const { return mCurrent.attachments; }
    std::vector<GenerationZone>& generationZonesMutable() { return mCurrent.generationZones; }
    const std::vector<GenerationZone>& generationZones() const { return mCurrent.generationZones; }
    std::vector<NpcPreset>& npcPresetsMutable() { return mCurrent.npcPresets; }
    const std::vector<NpcPreset>& npcPresets() const { return mCurrent.npcPresets; }
    std::vector<WorldWeaponOverride>& weaponOverridesMutable() { return mCurrent.weaponOverrides; }
    const std::vector<WorldWeaponOverride>& weaponOverrides() const { return mCurrent.weaponOverrides; }
    std::vector<AuthoredInteraction>& interactionsMutable() { return mCurrent.interactions; }
    const std::vector<AuthoredInteraction>& interactions() const { return mCurrent.interactions; }

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
