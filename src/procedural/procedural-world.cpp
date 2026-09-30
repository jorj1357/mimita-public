// 09 28 2026, 00 00
/* purpose
* Implement the procedural-world configuration loader and deterministic room
* placement math shared by the server and every client.
* Reads config/procedural-world.json and each referenced room definition JSON.
* Does NOT touch the network, physics worlds, or rendering.
*/

#include "procedural/procedural-world.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>

#include <glm/gtc/matrix_transform.hpp>
#include <nlohmann/json.hpp>

#include "debug/debug-log.h"
#include "network/server.h"
#include "network/server-gamemode.h"
#include "npc/npc.h"
#include "physics/physical-entity.h"
#include "world/world-gltf-loader.h"

using json = nlohmann::json;

namespace MimitaProcedural {
namespace {

ProceduralWorldConfig gConfig;
std::string gConfigPath = "config/procedural-world.json";
std::unordered_map<std::string, std::filesystem::file_time_type> gConfigTimes;

glm::vec3 readVec3(const json& j, const char* key, glm::vec3 def)
{
    if (!j.contains(key)) return def;
    const auto& arr = j[key];
    if (arr.is_array() && arr.size() >= 3)
        return {arr[0].get<float>(), arr[1].get<float>(), arr[2].get<float>()};
    return def;
}

std::vector<glm::vec3> readVec3List(const json& j, const char* key)
{
    std::vector<glm::vec3> out;
    if (!j.contains(key)) return out;
    const auto& arr = j[key];
    if (!arr.is_array()) return out;
    for (const auto& item : arr)
    {
        if (item.is_array() && item.size() >= 3)
            out.push_back({item[0].get<float>(), item[1].get<float>(),
                           item[2].get<float>()});
    }
    return out;
}

bool loadRoomDefinition(const std::string& path, ProceduralRoomDefinition& out)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        Debug::warn(Debug::Category::General,
                    "[PROCEDURAL] could not open room %s\n", path.c_str());
        return false;
    }
    try
    {
        json j = json::parse(file, nullptr, true, true);
        out.id = j.value("id", out.id);
        out.geometryPath = j.value("geometry", out.geometryPath);
        if (j.contains("entrance"))
        {
            out.entrancePosition = readVec3(j["entrance"], "position", out.entrancePosition);
            out.entranceDirection = readVec3(j["entrance"], "direction", out.entranceDirection);
        }
        if (j.contains("exit"))
        {
            out.exitPosition = readVec3(j["exit"], "position", out.exitPosition);
            out.exitDirection = readVec3(j["exit"], "direction", out.exitDirection);
        }
        out.enemySpawns = readVec3List(j, "enemySpawns");
        out.boundsMin = readVec3(j, "boundsMin", out.boundsMin);
        out.boundsMax = readVec3(j, "boundsMax", out.boundsMax);
        if (j.contains("spawnpoint") && j["spawnpoint"].is_object())
        {
            out.hasPlayerSpawn = true;
            out.playerSpawnPosition = readVec3(
                j["spawnpoint"], "position", out.playerSpawnPosition);
            out.playerSpawnRotationDegrees = readVec3(
                j["spawnpoint"], "rotation_degrees",
                out.playerSpawnRotationDegrees);
        }
        if (j.contains("door") && j["door"].is_object())
        {
            out.hasDoor = true;
            out.doorPosition = readVec3(
                j["door"], "position", out.exitPosition);
            out.doorRotationDegrees = readVec3(
                j["door"], "rotation_degrees", out.doorRotationDegrees);
            out.doorHalfExtents = readVec3(
                j["door"], "half_extents", out.doorHalfExtents);
        }
    }
    catch (const std::exception& e)
    {
        Debug::warn(Debug::Category::General,
                    "[PROCEDURAL] room %s parse error: %s\n", path.c_str(), e.what());
        return false;
    }
    return !out.id.empty() && !out.geometryPath.empty();
}

} // anonymous namespace

bool loadProceduralWorldConfig(const std::string& modeConfigPath)
{
    std::ifstream file(modeConfigPath);
    if (!file.is_open())
    {
        Debug::warn(Debug::Category::General,
                    "[PROCEDURAL] could not open %s\n", modeConfigPath.c_str());
        return false;
    }

    ProceduralWorldConfig candidate;
    candidate.defaultSeed = gConfig.defaultSeed;
    std::vector<std::string> roomSourcePaths;
    try
    {
        json j = json::parse(file, nullptr, true, true);
        candidate.defaultSeed = j.value("default_seed", candidate.defaultSeed);

        if (j.contains("modes") && j["modes"].is_object())
        {
            for (auto it = j["modes"].begin(); it != j["modes"].end(); ++it)
            {
                const json& m = it.value();
                ProceduralModeDefinition def;
                def.id = m.value("id", it.key());
                def.roomId = m.value("room", def.roomId);
                def.enemiesPerRoom = m.value("enemies_per_room", def.enemiesPerRoom);
                def.spacing = m.value("spacing", def.spacing);
                def.origin = readVec3(m, "origin", def.origin);
                def.axis = readVec3(m, "axis", def.axis);
                def.doorHalfExtents =
                    readVec3(m, "door_half_extents", def.doorHalfExtents);
                if (!def.id.empty())
                    candidate.modes[def.id] = def;
            }
        }

        if (j.contains("rooms") && j["rooms"].is_object())
        {
            for (auto it = j["rooms"].begin(); it != j["rooms"].end(); ++it)
            {
                if (!it.value().is_string()) continue;
                const std::string roomPath = it.value().get<std::string>();
                roomSourcePaths.push_back(roomPath);
                ProceduralRoomDefinition room;
                if (loadRoomDefinition(roomPath, room))
                    candidate.rooms[room.id] = room;
            }
        }
    }
    catch (const std::exception& e)
    {
        Debug::warn(Debug::Category::General,
                    "[PROCEDURAL] config %s parse error: %s\n",
                    modeConfigPath.c_str(), e.what());
        return false;
    }

    if (candidate.modes.empty() || candidate.rooms.empty())
    {
        Debug::warn(Debug::Category::General,
                    "[PROCEDURAL] config %s has no modes/rooms\n",
                    modeConfigPath.c_str());
        return false;
    }

    gConfig = std::move(candidate);
    gConfigPath = modeConfigPath;
    gConfigTimes.clear();
    auto rememberTime = [](const std::string& path) {
        std::error_code ec;
        const auto time = std::filesystem::last_write_time(path, ec);
        if (!ec) gConfigTimes[path] = time;
    };
    rememberTime(modeConfigPath);
    for (const std::string& path : roomSourcePaths)
        rememberTime(path);
    Debug::log(Debug::Category::General,
               "[PROCEDURAL] loaded config modes=%zu rooms=%zu defaultSeed=%u\n",
               gConfig.modes.size(), gConfig.rooms.size(), gConfig.defaultSeed);
    return true;
}

bool reloadProceduralWorldConfigIfChanged()
{
    if (gConfig.modes.empty())
        return loadProceduralWorldConfig(gConfigPath);
    std::error_code ec;
    for (const auto& entry : gConfigTimes)
    {
        const auto time = std::filesystem::last_write_time(entry.first, ec);
        if (!ec && time != entry.second)
            return loadProceduralWorldConfig(gConfigPath);
        ec.clear();
    }
    return false;
}

const ProceduralWorldConfig& proceduralWorldConfig()
{
    return gConfig;
}

const ProceduralModeDefinition* proceduralModeById(const std::string& id)
{
    const auto it = gConfig.modes.find(id);
    return it == gConfig.modes.end() ? nullptr : &it->second;
}

const ProceduralRoomDefinition* proceduralRoomById(const std::string& id)
{
    const auto it = gConfig.rooms.find(id);
    return it == gConfig.rooms.end() ? nullptr : &it->second;
}

glm::mat4 proceduralRoomTransform(const ProceduralModeDefinition& mode,
                                  uint32_t roomSlot)
{
    const glm::vec3 axis =
        glm::length(mode.axis) > 0.0001f ? glm::normalize(mode.axis)
                                         : glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::vec3 offset =
        mode.origin + axis * (mode.spacing * static_cast<float>(roomSlot));
    return glm::translate(glm::mat4(1.0f), offset);
}

glm::vec3 proceduralTransformPoint(const glm::mat4& transform,
                                   const glm::vec3& point)
{
    return glm::vec3(transform * glm::vec4(point, 1.0f));
}

glm::vec3 proceduralRoomEntrance(const ProceduralModeDefinition& mode,
                                 const ProceduralRoomDefinition& room,
                                 uint32_t roomSlot)
{
    return proceduralTransformPoint(proceduralRoomTransform(mode, roomSlot),
                                    room.entrancePosition);
}

glm::vec3 proceduralRoomExit(const ProceduralModeDefinition& mode,
                             const ProceduralRoomDefinition& room,
                             uint32_t roomSlot)
{
    return proceduralTransformPoint(proceduralRoomTransform(mode, roomSlot),
                                    room.exitPosition);
}

glm::mat4 proceduralRoomDoorTransform(const ProceduralModeDefinition& mode,
                                      const ProceduralRoomDefinition& room,
                                      uint32_t roomSlot)
{
    const glm::vec3 position = room.hasDoor ? room.doorPosition
                                            : room.exitPosition;
    const glm::vec3 degrees = room.hasDoor ? room.doorRotationDegrees
                                           : glm::vec3(0.0f);
    glm::mat4 local = glm::translate(glm::mat4(1.0f), position);
    local = glm::rotate(local, glm::radians(degrees.z), glm::vec3(0, 0, 1));
    local = glm::rotate(local, glm::radians(degrees.y), glm::vec3(0, 1, 0));
    local = glm::rotate(local, glm::radians(degrees.x), glm::vec3(1, 0, 0));
    return proceduralRoomTransform(mode, roomSlot) * local;
}

glm::vec3 proceduralRoomDoorHalfExtents(
    const ProceduralModeDefinition& mode,
    const ProceduralRoomDefinition& room)
{
    return room.hasDoor && glm::length(room.doorHalfExtents) > 0.0001f
        ? room.doorHalfExtents : mode.doorHalfExtents;
}

std::vector<glm::vec3> proceduralRoomEnemySpawns(
    const ProceduralModeDefinition& mode,
    const ProceduralRoomDefinition& room,
    uint32_t roomSlot)
{
    const glm::mat4 transform = proceduralRoomTransform(mode, roomSlot);
    std::vector<glm::vec3> out;
    out.reserve(room.enemySpawns.size());
    for (const glm::vec3& local : room.enemySpawns)
        out.push_back(proceduralTransformPoint(transform, local));
    return out;
}

} // namespace MimitaProcedural

// ── Server-authoritative procedural runtime ─────────────────────────────
// Lives in MimitaNet so it can use the existing server state and collision
// owners. The heavy logic is here; Orchestration (broadcast, tick ordering)
// stays in server-gamemode.cpp.
namespace MimitaNet {
namespace {

using MimitaProcedural::ProceduralModeDefinition;
using MimitaProcedural::ProceduralRoomDefinition;
using MimitaProcedural::ProceduralRoomState;
using MimitaProcedural::ProceduralWorldState;

// Template collision loaded once from the room GLB and reused for every room
// instance on the server (the client keeps its own render template).
HeadlessWorld gServerRoomTemplate;
bool gServerRoomTemplateLoaded = false;
std::string gServerRoomTemplatePath;

void appendWorldTriangles(HeadlessWorld& world,
                          const std::vector<CollisionTriangle>& tris)
{
    for (const CollisionTriangle& t : tris)
    {
        world.triangles.push_back(t);
        world.boundsMin = glm::min(world.boundsMin,
                                   glm::min(t.a, glm::min(t.b, t.c)));
        world.boundsMax = glm::max(world.boundsMax,
                                   glm::max(t.a, glm::max(t.b, t.c)));
    }
    buildHeadlessCollisionChunks(world);
}

void appendRoomGeometry(HeadlessWorld& world, World& npcWorld,
                        const ProceduralModeDefinition& mode, uint32_t slot)
{
    if (!gServerRoomTemplateLoaded)
        return;
    appendHeadlessWorldInstance(
        world, gServerRoomTemplate,
        MimitaProcedural::proceduralRoomTransform(mode, slot));
    buildNpcWorldCollision(npcWorld, world);
}

void addBarrier(ProceduralWorldState& p, const ProceduralModeDefinition& mode,
                const ProceduralRoomDefinition& room, uint32_t slot,
                HeadlessWorld& world)
{
    const glm::mat4 transform = MimitaProcedural::proceduralRoomDoorTransform(
        mode, room, slot);
    const glm::vec3 half = MimitaProcedural::proceduralRoomDoorHalfExtents(
        mode, room);
    std::vector<CollisionTriangle> tris;
    buildBoxCollisionTriangles(tris, glm::vec3(0.0f), half);
    for (CollisionTriangle& triangle : tris)
    {
        triangle.a = MimitaProcedural::proceduralTransformPoint(transform, triangle.a);
        triangle.b = MimitaProcedural::proceduralTransformPoint(transform, triangle.b);
        triangle.c = MimitaProcedural::proceduralTransformPoint(transform, triangle.c);
        triangle.normal = glm::normalize(glm::cross(triangle.b - triangle.a,
                                                    triangle.c - triangle.a));
    }
    p.barrierTriangleCount = tris.size();
    appendWorldTriangles(world, tris);
    p.barrierActive = true;
    p.currentBarrierTransform = transform;
    p.exitLocked = true;
}

// Removes the most recently appended barrier. Safe because a barrier is always
// the tail of the triangle list when it is unlocked (the next room's geometry
// is only appended afterwards).
void removeBarrier(ProceduralWorldState& p, HeadlessWorld& world)
{
    if (!p.barrierActive || p.barrierTriangleCount == 0)
        return;
    const size_t base = world.triangles.size() >= p.barrierTriangleCount
        ? world.triangles.size() - p.barrierTriangleCount
        : 0;
    truncateHeadlessWorld(world, base);
    p.barrierActive = false;
    p.barrierTriangleCount = 0;
    p.exitLocked = false;
}

void spawnEncounterRoom(ProceduralWorldState& p,
                        const ProceduralModeDefinition& mode,
                        const ProceduralRoomDefinition& room,
                        uint32_t roomNumber, uint32_t slot,
                        std::unordered_map<uint32_t, ServerNpc>& npcs)
{
    const std::vector<glm::vec3> spawns =
        MimitaProcedural::proceduralRoomEnemySpawns(mode, room, slot);
    // Infinite Dungeon Slayer difficulty grows with the room number:
    // room 1 has 1 enemy, room 2 has 2, and so on.
    const uint32_t count = std::max(1u, roomNumber);
    for (uint32_t i = 0; i < count; ++i)
    {
        ServerNpc npc;
        npc.entityId = p.nextNpcId++;
        npc.name = "NPC " + std::to_string(npc.entityId);
        npc.pos = spawns.empty() ? glm::vec3(0.0f) : spawns[i % spawns.size()];
        npc.difficulty = 1.0f;
        npc.proceduralRoomNumber = roomNumber;
        npcs[npc.entityId] = npc;
        p.encounterNpcIds.insert(npc.entityId);
    }
}

} // anonymous namespace

bool serverProceduralWorldStart(const std::string& modeId, uint32_t seed,
                                HeadlessWorld& world, World& npcWorld,
                                std::unordered_map<uint32_t, ServerNpc>& npcs)
{
    if (MimitaProcedural::proceduralWorldConfig().modes.empty())
        MimitaProcedural::loadProceduralWorldConfig();

    const ProceduralModeDefinition* mode =
        MimitaProcedural::proceduralModeById(modeId);
    if (!mode)
    {
        Debug::warn(Debug::Category::General,
                    "[PROCEDURAL] start rejected unknown mode=%s\n", modeId.c_str());
        return false;
    }
    const ProceduralRoomDefinition* room =
        MimitaProcedural::proceduralRoomById(mode->roomId);
    if (!room)
    {
        Debug::warn(Debug::Category::General,
                    "[PROCEDURAL] start rejected unknown room=%s\n",
                    mode->roomId.c_str());
        return false;
    }

    if (!gServerRoomTemplateLoaded || gServerRoomTemplatePath != room->geometryPath)
    {
        gServerRoomTemplate = HeadlessWorld{};
        if (!loadHeadlessWorld(room->geometryPath.c_str(), gServerRoomTemplate))
        {
            gServerRoomTemplateLoaded = false;
            Debug::warn(Debug::Category::General,
                        "[PROCEDURAL] failed to load room geometry %s\n",
                        room->geometryPath.c_str());
            return false;
        }
        gServerRoomTemplateLoaded = true;
        gServerRoomTemplatePath = room->geometryPath;
    }

    ServerGamemodeState& d = serverGamemodeState();
    ProceduralWorldState& p = d.procedural;
    p = ProceduralWorldState{};
    p.enabled = true;
    p.seed = seed;
    p.modeId = modeId;
    p.roomId = room->id;
    p.nextNpcId = 200000;
    p.playerSpawnLocal = room->hasPlayerSpawn ? room->playerSpawnPosition
                                              : room->entrancePosition;
    p.playerSpawnYaw = std::atan2(room->entranceDirection.y,
                                  room->entranceDirection.x);
    if (room->hasPlayerSpawn)
        p.playerSpawnYaw = glm::radians(room->playerSpawnRotationDegrees.z);

    // Prefer the Blender-authored node named with "spawnpoint". The generic
    // GLB extractor already applies the node hierarchy transforms, so the
    // result is room-local and receives the same procedural room transform as
    // the geometry and enemy spawns.
    if (!room->hasPlayerSpawn)
    {
        World spawnMetadata;
        extractSpawnPointsFromGLB(spawnMetadata, room->geometryPath.c_str());
        const SpawnPoint* selected = nullptr;
        for (const SpawnPoint& candidate : spawnMetadata.spawnPoints)
        {
            std::string tag = candidate.tag;
            std::transform(tag.begin(), tag.end(), tag.begin(),
                           [](unsigned char c) { return (char)std::tolower(c); });
            if (tag.find("spawnpoint") != std::string::npos)
            {
                selected = &candidate;
                break;
            }
            if (!selected)
                selected = &candidate;
        }
        if (selected)
        {
            p.playerSpawnLocal = selected->position;
            const glm::vec3 forward =
                selected->rotation * glm::vec3(0.0f, 1.0f, 0.0f);
            p.playerSpawnYaw = std::atan2(forward.y, forward.x);
            p.playerSpawnFromGlb = true;
            printf("[PROCEDURAL] player spawnpoint tag=%s local=(%.2f,%.2f,%.2f) "
                   "yaw=%.2f\n",
                   selected->tag.c_str(), p.playerSpawnLocal.x,
                   p.playerSpawnLocal.y, p.playerSpawnLocal.z,
                   p.playerSpawnYaw);
        }
        else
        {
            printf("[PROCEDURAL] no GLB spawnpoint found; using configured "
                   "entrance local=(%.2f,%.2f,%.2f)\n",
                   p.playerSpawnLocal.x, p.playerSpawnLocal.y,
                   p.playerSpawnLocal.z);
        }
    }
    p.baseTriangleCount = world.triangles.size();
    p.geometryAppended = true;

    // Lobby (slot 0) and the first combat room (slot 1). Combat room N uses
    // slot N so entrance/exit/spawn transforms stay deterministic.
    appendRoomGeometry(world, npcWorld, *mode, 0);
    appendRoomGeometry(world, npcWorld, *mode, 1);

    p.generatedRooms = 1;
    p.currentRoom = 1;
    p.highestAccessibleRoom = 1;
    p.roomState = ProceduralRoomState::Active;
    spawnEncounterRoom(p, *mode, *room, 1, 1, npcs);
    addBarrier(p, *mode, *room, 1, world);
    buildNpcWorldCollision(npcWorld, world);
    ++p.stateVersion;

    Debug::warn(Debug::Category::General,
        "[PROCEDURAL] started mode=%s seed=%u room=%s enemies=%u baseTris=%zu worldTris=%zu\n",
        modeId.c_str(), seed, room->id.c_str(), mode->enemiesPerRoom,
        p.baseTriangleCount, world.triangles.size());
    return true;
}

bool serverProceduralWorldTick(SOCKET /*sock*/,
                               std::unordered_map<uint32_t, ServerPlayer>& /*players*/,
                               HeadlessWorld& world,
                               World& npcWorld,
                               std::unordered_map<uint32_t, ServerNpc>& npcs,
                               NpcSystem& /*npcSystem*/,
                               uint32_t /*tick*/)
{
    ServerGamemodeState& d = serverGamemodeState();
    ProceduralWorldState& p = d.procedural;
    if (!p.enabled)
        return false;

    const bool configChanged = MimitaProcedural::reloadProceduralWorldConfigIfChanged();
    const ProceduralModeDefinition* mode =
        MimitaProcedural::proceduralModeById(p.modeId);
    const ProceduralRoomDefinition* room =
        MimitaProcedural::proceduralRoomById(p.roomId);
    if (!mode || !room)
        return false;

    if (configChanged && p.barrierActive)
    {
        removeBarrier(p, world);
        addBarrier(p, *mode, *room, p.currentRoom, world);
        buildNpcWorldCollision(npcWorld, world);
        ++p.stateVersion;
    }

    uint32_t total = 0;
    uint32_t alive = 0;
    for (const auto& kv : npcs)
    {
        if (kv.second.proceduralRoomNumber != p.currentRoom)
            continue;
        ++total;
        if (kv.second.health > 0)
            ++alive;
    }
    p.encounterActorCount = total;
    p.aliveEncounterActors = alive;

    if (p.roomState != ProceduralRoomState::Active || total == 0 || alive != 0)
        return false;

    // Room cleared: unlock the exit and immediately activate the next room.
    p.roomState = ProceduralRoomState::Complete;
    removeBarrier(p, world);

    const uint32_t next = p.currentRoom + 1;
    if (next > p.generatedRooms)
    {
        appendRoomGeometry(world, npcWorld, *mode, next);
        p.generatedRooms = next;
    }
    spawnEncounterRoom(p, *mode, *room, next, next, npcs);
    p.currentRoom = next;
    p.highestAccessibleRoom = next;
    p.roomState = ProceduralRoomState::Active;
    addBarrier(p, *mode, *room, next, world);
    buildNpcWorldCollision(npcWorld, world);
    ++p.stateVersion;

    Debug::warn(Debug::Category::General,
        "[PROCEDURAL] room complete -> generated room=%u enemies=%u alive=%u\n",
        next, p.encounterActorCount, p.aliveEncounterActors);
    return true;
}

void serverProceduralWorldStop(HeadlessWorld& world, World& npcWorld,
                               std::unordered_map<uint32_t, ServerNpc>& npcs,
                               NpcSystem& npcSystem)
{
    ServerGamemodeState& d = serverGamemodeState();
    ProceduralWorldState& p = d.procedural;
    if (!p.enabled && !p.geometryAppended)
        return;

    const std::vector<uint32_t> ids(p.encounterNpcIds.begin(),
                                    p.encounterNpcIds.end());
    for (uint32_t id : ids)
        npcs.erase(id);
    if (!ids.empty())
        npcSystem.destroySelected(ids);

    if (p.geometryAppended)
    {
        truncateHeadlessWorld(world, p.baseTriangleCount);
        buildNpcWorldCollision(npcWorld, world);
    }

    const uint32_t removedNpcs = static_cast<uint32_t>(ids.size());
    p = ProceduralWorldState{};
    p.pendingDisableBroadcast = true;
    ++d.stateVersion;
    d.stateBroadcastPending = true;

    Debug::warn(Debug::Category::General,
        "[PROCEDURAL] stopped; removedNpcs=%u worldTris=%zu\n",
        removedNpcs, world.triangles.size());
}

bool serverProceduralWorldOwnsNpc(uint32_t entityId)
{
    const auto& ids = serverGamemodeState().procedural.encounterNpcIds;
    return ids.find(entityId) != ids.end();
}

bool serverProceduralWorldTeleportTarget(glm::vec3& outPosition)
{
    ServerGamemodeState& d = serverGamemodeState();
    const ProceduralWorldState& p = d.procedural;
    if (!p.enabled)
        return false;
    const ProceduralModeDefinition* mode =
        MimitaProcedural::proceduralModeById(p.modeId);
    const ProceduralRoomDefinition* room =
        MimitaProcedural::proceduralRoomById(p.roomId);
    if (!mode || !room)
        return false;
    const uint32_t slot = std::max(p.highestAccessibleRoom, p.currentRoom);
    outPosition = MimitaProcedural::proceduralTransformPoint(
        MimitaProcedural::proceduralRoomTransform(*mode, slot),
        p.playerSpawnLocal);
    return true;
}

} // namespace MimitaNet
