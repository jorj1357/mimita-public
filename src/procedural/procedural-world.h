// 09 28 2026, 00 00
/* purpose
* Own the data model, shared configuration, and deterministic placement math for
* the procedural-world (Infinite Dungeon Slayer) feature.
* The server owns all authoritative state; clients only reconstruct geometry
* from the replicated seed + generated-room count.
* Does NOT own networking sockets, NPC AI, physics, or rendering.
* Does NOT let a client decide that a room is complete.
*/

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glm/glm.hpp>

namespace MimitaProcedural {

// Authoritative room lifecycle. Mirrored on the wire by
// ProceduralRoomStateNetwork (packets.h).
enum class ProceduralRoomState : uint8_t
{
    Waiting = 0,
    Active = 1,
    Complete = 2
};

// Server-authoritative procedural-world state. Embedded in the existing
// ServerGamemodeState so there is one source of truth. The non-replicated
// bookkeeping fields are used by the server only.
struct ProceduralWorldState
{
    bool enabled = false;
    uint32_t seed = 583294;

    uint32_t currentRoom = 0;          // 0 = lobby, >= 1 = combat room
    uint32_t generatedRooms = 0;       // combat rooms generated so far
    uint32_t highestAccessibleRoom = 0;

    ProceduralRoomState roomState = ProceduralRoomState::Waiting;

    uint32_t encounterActorCount = 0;
    uint32_t aliveEncounterActors = 0;

    bool exitLocked = true;
    uint32_t stateVersion = 0;

    // ── Server-only bookkeeping (not replicated) ─────────────────────
    std::string modeId;
    std::string roomId;
    glm::vec3 playerSpawnLocal{0.0f};
    float playerSpawnYaw = 0.0f;
    bool playerSpawnFromGlb = false;
    // Every procedural encounter NPC id (mirrors + real NpcSystem bodies).
    // Used to suppress respawn and to remove only procedural actors on stop.
    std::unordered_set<uint32_t> encounterNpcIds;
    uint32_t nextNpcId = 200000;
    // Base HeadlessWorld triangle count before any procedural geometry was
    // appended, so stop can truncate back to the plain sandbox map.
    size_t baseTriangleCount = 0;
    bool geometryAppended = false;
    // Currently active authoritative exit barrier (server HeadlessWorld).
    bool barrierActive = false;
    size_t barrierTriangleCount = 0;
    glm::mat4 currentBarrierTransform{1.0f};
    // Periodic broadcast cadence + one final disabled-state broadcast so clients
    // clear rooms after stop.
    uint32_t lastBroadcastTick = 0;
    bool pendingDisableBroadcast = false;
};

// Manually editable room definition (config/procedural-world/rooms/*.json).
// The GLB owns visible geometry, static collision, materials, and textures.
// MiMITA owns entrance, exit, enemy spawns, trigger, room state, door lock,
// and encounter membership.
struct ProceduralRoomDefinition
{
    std::string id;
    std::string geometryPath;
    glm::vec3 entrancePosition{0.0f};
    glm::vec3 entranceDirection{0.0f, 1.0f, 0.0f};
    glm::vec3 exitPosition{0.0f};
    glm::vec3 exitDirection{0.0f, 1.0f, 0.0f};
    std::vector<glm::vec3> enemySpawns;
    // Real bounds measured from the updated GLB (used for placement).
    glm::vec3 boundsMin{0.0f};
    glm::vec3 boundsMax{0.0f};
    // Optional JSON-owned player spawn. When present, this is the source of
    // truth instead of a Blender GLB spawn node.
    bool hasPlayerSpawn = false;
    glm::vec3 playerSpawnPosition{0.0f};
    glm::vec3 playerSpawnRotationDegrees{0.0f};
    // Door is room-local and can be tuned without changing the mode recipe.
    bool hasDoor = false;
    glm::vec3 doorPosition{0.0f};
    glm::vec3 doorRotationDegrees{0.0f};
    glm::vec3 doorHalfExtents{0.0f};
};

// Mode recipe (config/procedural-world.json: modes.<id>).
struct ProceduralModeDefinition
{
    std::string id;
    std::string roomId;
    uint32_t enemiesPerRoom = 5;
    // Measured from the real room GLB bounds, not a hardcoded 100 m guess.
    float spacing = 104.0f;
    glm::vec3 origin{0.0f, 1000.0f, 0.0f};
    glm::vec3 axis{0.0f, 1.0f, 0.0f};
    glm::vec3 doorHalfExtents{10.0f, 4.0f, 1.0f};

    // Infinite-world streaming policy. A chunk is a deterministic square
    // area containing a bounded number of repeated authored blocks.
    bool streamingEnabled = false;
    float chunkSize = 1000.0f;
    float blockSpacing = 100.0f;
    uint32_t minBlocksPerChunk = 1;
    uint32_t maxBlocksPerChunk = 4;
    uint32_t streamRadiusChunks = 1;
    uint32_t loadedRoomRadius = 2;
};

struct ProceduralWorldConfig
{
    uint32_t defaultSeed = 583294;
    std::unordered_map<std::string, ProceduralModeDefinition> modes;
    std::unordered_map<std::string, ProceduralRoomDefinition> rooms;
};

// Loads config/procedural-world.json and every room definition it references.
// Safe to call repeatedly; returns false and keeps the previous config on
// failure.
bool loadProceduralWorldConfig(
    const std::string& modeConfigPath = "config/procedural-world.json");

// Reloads the mode/room JSON only when one of its source files changed.
// Returns true when a new valid config was installed.
bool reloadProceduralWorldConfigIfChanged();

const ProceduralWorldConfig& proceduralWorldConfig();
const ProceduralModeDefinition* proceduralModeById(const std::string& id);
const ProceduralRoomDefinition* proceduralRoomById(const std::string& id);

// Deterministic straight-line room placement: the lobby is slot 0, combat
// room N is slot N. Derived from the mode origin + axis and the measured room
// spacing. Seed is carried for future variation and for reproducible geometry.
glm::mat4 proceduralRoomTransform(const ProceduralModeDefinition& mode,
                                  uint32_t roomSlot);

glm::vec3 proceduralTransformPoint(const glm::mat4& transform,
                                   const glm::vec3& point);

// World-space entrance/exit/enemy-spawn points for one room slot.
glm::vec3 proceduralRoomEntrance(const ProceduralModeDefinition& mode,
                                 const ProceduralRoomDefinition& room,
                                 uint32_t roomSlot);
glm::vec3 proceduralRoomExit(const ProceduralModeDefinition& mode,
                             const ProceduralRoomDefinition& room,
                             uint32_t roomSlot);
glm::mat4 proceduralRoomDoorTransform(const ProceduralModeDefinition& mode,
                                      const ProceduralRoomDefinition& room,
                                      uint32_t roomSlot);
glm::vec3 proceduralRoomDoorHalfExtents(
    const ProceduralModeDefinition& mode,
    const ProceduralRoomDefinition& room);
std::vector<glm::vec3> proceduralRoomEnemySpawns(
    const ProceduralModeDefinition& mode,
    const ProceduralRoomDefinition& room,
    uint32_t roomSlot);

// Stable seed + integer-coordinate generation. These functions are pure and
// are shared by the server and clients; no process-local RNG state is used.
std::vector<glm::vec3> proceduralChunkBlockPositions(
    const ProceduralModeDefinition& mode, uint32_t seed,
    int32_t chunkX, int32_t chunkZ);
uint64_t proceduralChunkHash(uint32_t seed, int32_t chunkX, int32_t chunkZ);

} // namespace MimitaProcedural
