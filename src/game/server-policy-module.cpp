// 2026-11-16
// Whole-game server policy owner.
//
// Resident server code keeps sockets, ICE, scheduling, and session containers.
// This owner performs server startup/config/world policy inside the
// replaceable generation, behind the optional GameServerPolicyV1 export.

#include "host/abi/game-server-abi.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <new>
#include <string>
#include <unordered_map>
#include <vector>

#include "network/server.h"
#include "network/server-gamemode.h"
#include "network/server-ice-policy.h"
#include "network/server-session-policy.h"
#include "network/community-server-config.h"
#include "network/coordinator-client.h"
#include "physics/runtime-objects.h"
#include "audio/audio.h"
#include "vehicles/vehicle-system.h"
#include "vehicles/vehicle-config.h"
#include "combat/weapon-data.h"
#include "combat/weapon-registry.h"
#include "npc/npc.h"
#include "npc/npc-behavior.h"
#include "npc/npc-difficulty-config.h"
#include "entities/player.h"
#include "world/world.h"
#include "map/map-authored-geometry.h"
#include "gamemode/arena-ball.h"
#include "gamemode/gamemode.h"
#include "gamemode/gamemode-map-pool.h"
#include "gamemode/map-config.h"
#include "gamemode/match-roles.h"
#include "gamemode/mode-pack-registry.h"
#include "gamemode/monster-pools.h"
#include "config/networking-config.h"
#include "config/movement-config.h"
#include "config/collision-config.h"
#include "config/spawn-velocity-config.h"
#include "combat/actor-preset-weapons.h"
#include "auth/auth-system.h"
#include "debug/structured-log.h"

namespace {

std::vector<std::string> communityMapPool() {
    return GamemodeMapPool::instance().list();
}

std::string gamemodeDefaultMap(const std::string& communityModeId) {
    if (communityModeId.empty() || communityModeId == "sandbox") {
        return {};
    }
    MimitaNet::CommunityServerConfig& cfg =
        MimitaNet::CommunityServerConfig::instance();
    if (cfg.modes().empty()) {
        cfg.load();
    }
    const MimitaNet::CommunityMode* mode = cfg.modeById(communityModeId);
    const std::string gamemodeId = mode ? mode->gamemodeId : communityModeId;
    const Gamemode& gamemode = GamemodeRegistry::instance().get(gamemodeId);
    if (!gamemode.maps.empty()) {
        return gamemode.maps.front();
    }
    return {};
}

void setError(char* error, std::size_t size, const char* text) {
    if (!error || size == 0) {
        return;
    }
    std::snprintf(error, size, "%s", text ? text : "unknown server policy error");
}

glm::vec3 spawnPosition(const mimita::host::ServerConfigureV1& request,
                        const glm::vec3& fallback) {
    if (request.spawnOverrideEnabled == 0) {
        return fallback;
    }
    return glm::vec3(request.spawnOverride[0], request.spawnOverride[1],
                     request.spawnOverride[2]);
}

bool validStartup(const mimita::host::ServerStartupV1* request) {
    return request &&
           request->version == mimita::host::kGameServerPolicyAbiVersion &&
           request->structSize >= sizeof(mimita::host::ServerStartupV1);
}

bool validWorldLoad(const mimita::host::ServerWorldLoadV1* request) {
    return request &&
           request->version == mimita::host::kGameServerPolicyAbiVersion &&
           request->structSize >= sizeof(mimita::host::ServerWorldLoadV1) &&
           request->headlessWorld && request->npcWorld;
}

bool validConfigure(const mimita::host::ServerConfigureV1* request) {
    return request &&
           request->version == mimita::host::kGameServerPolicyAbiVersion &&
           request->structSize >= sizeof(mimita::host::ServerConfigureV1) &&
           request->headlessWorld && request->npcs && request->nextEntityId;
}

}  // namespace

namespace mimita::game {

int serverPolicyStartup(const mimita::host::ServerStartupV1* request,
                        mimita::host::ServerStartupResultV1* result) {
    if (!result) {
        return 0;
    }
    std::memset(result, 0, sizeof(*result));
    result->version = mimita::host::kGameServerPolicyAbiVersion;
    result->structSize = sizeof(*result);
    if (!validStartup(request)) {
        setError(result->error, sizeof(result->error),
                 "invalid server startup request");
        return 0;
    }

    AuthSystem::instance().init(
        request->sessionToken ? request->sessionToken : "");
    setServerAudioMode(true);
    VehicleSystem::instance().setServerProcess(true);
    VehicleSystem::instance().loadConfig("config/vehicles");
    VehicleSystem::instance().registerFixedTick();
    WeaponData::registerBuiltinWeapons();

    NpcDifficultyConfig::instance().load("config/npc-difficulty.json");
    MimitaNet::CommunityServerConfig::instance().load();
    GamemodeRegistry::instance().loadDirectory("config/gamemodes");
    std::vector<std::string> packDiagnostics;
    MimitaGamemode::ModePackRegistry::instance().loadDirectory(
        "config/mode-packs", &packDiagnostics);
    MatchRoleRegistry::instance().load("config/roles.json");
    MatchRoleRegistry::instance().loadActorPresets("config/actor-presets");
    MonsterPoolRegistry::instance().load("config/monster-pools.json");
    BehaviorProfileRegistry::instance().load("config/behavior-profiles.json");
    GamemodeMapPool::instance().load("config/gamemode-good-maps.json");

    if (MimitaNet::gServerBodyTemplate.empty()) {
        MimitaNet::loadServerBodyTemplateFromGlb(
            nullptr, MimitaNet::gServerBodyTemplate);
    }

    const std::string requestedMode =
        request->gameMode ? request->gameMode : "";
    std::string mapName =
        (request->mapName && *request->mapName) ? request->mapName : "funworld3";
    if ((!request->mapName || !*request->mapName)) {
        const std::string modeDefault = gamemodeDefaultMap(requestedMode);
        if (!modeDefault.empty()) {
            mapName = modeDefault;
        }
    }
    const std::string mapPath =
        (request->mapPath && *request->mapPath)
            ? request->mapPath
            : ("assets/maps/" + mapName + ".glb");
    std::snprintf(result->resolvedMapName, sizeof(result->resolvedMapName),
                  "%s", mapName.c_str());
    std::snprintf(result->resolvedMapPath, sizeof(result->resolvedMapPath),
                  "%s", mapPath.c_str());

    const MimitaNet::CommunityMode* selectedMode =
        MimitaNet::CommunityServerConfig::instance().modeById(requestedMode);
    result->resolvedMaxPlayers =
        selectedMode && selectedMode->maxPlayers > 0
            ? std::min(request->maxPlayers,
                       static_cast<std::uint32_t>(selectedMode->maxPlayers))
            : request->maxPlayers;
    result->ok = 1;

    StructuredLogger::instance().writeEvent(
        StructuredCategory::Network, StructuredLevel::Important,
        "server.policy.startup", requestedMode,
        "server startup config loaded in the replaceable generation", 0,
        nlohmann::json{{"map", mapName},
                       {"map_path", mapPath},
                       {"mode", requestedMode},
                       {"gamemode", request->gamemodeId ? request->gamemodeId : ""},
                       {"max_players", result->resolvedMaxPlayers},
                       {"npc_count", request->npcCount},
                       {"mode_pack_diagnostics", packDiagnostics.size()},
                       {"runtime_revision", 1}},
        __FILE__, __LINE__, __func__);
    return 1;
}

int serverPolicyWorldLoad(const mimita::host::ServerWorldLoadV1* request,
                          mimita::host::ServerWorldLoadResultV1* result) {
    if (!result) {
        return 0;
    }
    std::memset(result, 0, sizeof(*result));
    result->version = mimita::host::kGameServerPolicyAbiVersion;
    result->structSize = sizeof(*result);
    if (!validWorldLoad(request)) {
        setError(result->error, sizeof(result->error),
                 "invalid server world load request");
        return 0;
    }

    auto& world = *static_cast<MimitaNet::HeadlessWorld*>(
        request->headlessWorld);
    auto& npcWorld = *static_cast<World*>(request->npcWorld);
    const std::string mapName = request->mapName ? request->mapName : "";
    const std::string mapPath = request->mapPath ? request->mapPath : "";
    const std::string gameMode = request->gameMode ? request->gameMode : "";

    using StartupClock = std::chrono::steady_clock;
    auto phaseStarted = StartupClock::now();
    const bool loaded = MimitaNet::loadHeadlessWorld(mapPath.c_str(), world);
    const double headlessLoadMs = std::chrono::duration<double, std::milli>(
        StartupClock::now() - phaseStarted).count();
    result->mapLoaded = loaded ? 1u : 0u;
    double authoredBaseCaptureMs = 0.0;
    double authoredGeometryApplyMs = 0.0;
    double authoredCacheSeedMs = 0.0;
    if (loaded) {
        phaseStarted = StartupClock::now();
        MimitaNet::serverCaptureAuthoredBaseWorld(world, mapName);
        authoredBaseCaptureMs = std::chrono::duration<double, std::milli>(
            StartupClock::now() - phaseStarted).count();

        phaseStarted = StartupClock::now();
        applyAuthoredMapGeometry(
            world, mapName,
            GamemodeRegistry::instance().get(gameMode).worldStreaming.enabled);
        authoredGeometryApplyMs = std::chrono::duration<double, std::milli>(
            StartupClock::now() - phaseStarted).count();

        phaseStarted = StartupClock::now();
        MimitaNet::serverSeedAuthoredCache(world);
        authoredCacheSeedMs = std::chrono::duration<double, std::milli>(
            StartupClock::now() - phaseStarted).count();
    }

    phaseStarted = StartupClock::now();
    MimitaNet::buildNpcWorldCollision(npcWorld, world);
    const double npcCollisionBuildMs = std::chrono::duration<double, std::milli>(
        StartupClock::now() - phaseStarted).count();
    result->triangleCount = static_cast<std::uint32_t>(world.triangles.size());
    result->spawnCount = static_cast<std::uint32_t>(world.spawnPoints.size());
    result->npcCollisionTriangles =
        static_cast<std::uint32_t>(npcWorld.collisionMesh.triangles.size());
    result->ok = 1;

    StructuredLogger::instance().writeEvent(
        StructuredCategory::Network, StructuredLevel::Important,
        "server.policy.world", mapName,
        "server collision and NPC world prepared in the replaceable generation",
        0,
        nlohmann::json{{"map", mapName},
                       {"path", mapPath},
                       {"mode", gameMode},
                       {"map_loaded", loaded},
                       {"triangles", result->triangleCount},
                       {"spawn_points", result->spawnCount},
                        {"npc_collision_triangles", result->npcCollisionTriangles},
                        {"headless_load_ms", headlessLoadMs},
                        {"authored_base_capture_ms", authoredBaseCaptureMs},
                        {"authored_geometry_apply_ms", authoredGeometryApplyMs},
                        {"authored_cache_seed_ms", authoredCacheSeedMs},
                        {"npc_collision_build_ms", npcCollisionBuildMs}},
        __FILE__, __LINE__, __func__);
    return 1;
}

int serverPolicyConfigure(const mimita::host::ServerConfigureV1* request,
                          mimita::host::ServerConfigureResultV1* result) {
    if (!result) {
        return 0;
    }
    std::memset(result, 0, sizeof(*result));
    result->version = mimita::host::kGameServerPolicyAbiVersion;
    result->structSize = sizeof(*result);
    if (!validConfigure(request)) {
        setError(result->error, sizeof(result->error),
                 "invalid server configure request");
        return 0;
    }

    auto& world = *static_cast<MimitaNet::HeadlessWorld*>(
        request->headlessWorld);
    auto& npcs = *static_cast<std::unordered_map<std::uint32_t,
                                                MimitaNet::ServerNpc>*>(
        request->npcs);
    const std::string mapName = request->mapName ? request->mapName : "";
    const std::string gameMode = request->gameMode ? request->gameMode : "";

    if (request->duel) {
        const std::string gamemodeId =
            request->gamemodeId ? request->gamemodeId : "duel";
        const Gamemode& gamemode =
            GamemodeRegistry::instance().get(gamemodeId);
        MimitaNet::ServerGamemodeState duelRules;
        duelRules.goalValue = gamemode.goalValue;
        duelRules.countdownSeconds = gamemode.countdownSeconds;
        duelRules.rematchSeconds = gamemode.rematchSeconds;
        duelRules.teamAName = gamemode.teamNames.size() > 0
                                  ? gamemode.teamNames[0]
                                  : "RED";
        duelRules.teamBName = gamemode.teamNames.size() > 1
                                  ? gamemode.teamNames[1]
                                  : "BLUE";
        duelRules.spawnOffsetRadius = gamemode.spawnOffsetRadius;
        duelRules.mapPool = GamemodeMapPool::instance().list();
        duelRules.rotateMaps = true;
        duelRules.mapId = mapName;
        MimitaNet::serverGamemodeStart(duelRules);
    } else {
        MimitaNet::serverCommunityMapStart(
            communityMapPool(), mapName, request->autoMapRotation != 0,
            request->mapRotationMinutes, request->weaponSetId);
        if (request->weaponSetExplicit) {
            MimitaNet::serverCommunitySetWeaponSet(request->weaponSetId);
        }
        MimitaNet::serverCommunitySetMode(gameMode);
        if (gameMode != "sandbox") {
            MimitaNet::serverCommunityStartMatch(false);
        }
    }

    if (MapConfigRegistry::instance().current().arena.present) {
        std::string ballError;
        const ArenaBall::ArenaBallSpawn ballId = ArenaBall::spawnFromMap(
            MapConfigRegistry::instance().current(), &ballError);
        result->arenaBallSpawned = ballId != 0 ? 1u : 0u;
        if (ballId == 0 && !ballError.empty()) {
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Physics, StructuredLevel::Errors,
                "arena.ball.spawn-failed", mapName,
                "map authored an arena ball but the server could not spawn it",
                0, nlohmann::json{{"map", mapName}, {"error", ballError}},
                __FILE__, __LINE__, __func__);
        }
    }

    const bool fcsOwnsNpcRoster = gameMode == "funcounterstrike";
    const std::uint32_t npcCount =
        request->npcsEnabled && !fcsOwnsNpcRoster ? request->npcCount : 0;
    for (std::uint32_t index = 0; index < npcCount; ++index) {
        MimitaNet::ServerNpc npc;
        npc.entityId = (*request->nextEntityId)++;
        npc.name = "NPC " + std::to_string(index + 1);
        if (!world.spawnPoints.empty()) {
            const std::size_t spawnIndex = index % world.spawnPoints.size();
            const auto& spawn = world.spawnPoints[spawnIndex];
            npc.pos = spawnPosition(*request, spawn.position);
            npc.yaw = spawn.yaw;
        } else {
            npc.pos = {4.0f + index * 2.0f, 8.0f, 30.0f};
        }
        npc.phase = index * 2.0f;
        npcs[npc.entityId] = npc;
    }
    result->startupNpcCount = npcCount;
    result->ok = 1;

    StructuredLogger::instance().writeEvent(
        StructuredCategory::Network, StructuredLevel::Important,
        "server.policy.configure", gameMode,
        "server map, match, and startup actors configured by the generation", 0,
        nlohmann::json{{"map", mapName},
                       {"mode", gameMode},
                       {"duel", request->duel != 0},
                       {"arena_ball_spawned", result->arenaBallSpawned != 0},
                       {"startup_npcs", result->startupNpcCount}},
        __FILE__, __LINE__, __func__);
    return 1;
}

void* serverPolicyCreatePlayer() {
    return new (std::nothrow) Player();
}

void serverPolicyDestroyPlayer(void* player) {
    delete static_cast<Player*>(player);
}

int serverPolicyMakeUdpConnectionId(std::uint32_t addressNetwork,
                                    std::uint16_t portNetwork,
                                    mimita::host::ServerConnectionIdV1* out) {
    if (!out) {
        return 0;
    }
    sockaddr_in endpoint{};
    endpoint.sin_family = AF_INET;
    endpoint.sin_addr.s_addr = addressNetwork;
    endpoint.sin_port = portNetwork;
    const MimitaNet::TransportConnectionId id =
        MimitaNet::makeUdpConnectionId(endpoint);
    out->kind = static_cast<std::uint32_t>(id.kind);
    out->reserved = 0;
    out->value = id.value;
    return 1;
}

int serverPolicyAllocateIceConnectionId(
    mimita::host::ServerConnectionIdV1* result) {
    if (!result) {
        return 0;
    }
    const MimitaNet::TransportConnectionId id =
        MimitaNet::allocateIceConnectionId();
    result->kind = static_cast<std::uint32_t>(id.kind);
    result->reserved = 0;
    result->value = id.value;
    return 1;
}

int serverPolicyLegacyEndpoint(
    const mimita::host::ServerConnectionIdV1* id,
    std::uint32_t* addressNetwork, std::uint16_t* portNetwork) {
    if (!id || !addressNetwork || !portNetwork) {
        return 0;
    }
    const MimitaNet::TransportConnectionId connection{
        static_cast<MimitaNet::TransportKind>(id->kind), id->value};
    const sockaddr_in endpoint =
        MimitaNet::legacyEndpointForTransportConnection(connection);
    *addressNetwork = endpoint.sin_addr.s_addr;
    *portNetwork = endpoint.sin_port;
    return 1;
}

int serverPolicyClearActorPreset(std::uint32_t playerId) {
    MimitaNet::serverClearActorPreset(playerId);
    return 1;
}

int serverPolicyPollReload(void* npcSystem, std::uint32_t tick) {
    NetworkingConfig::instance().pollReload();
    NpcDifficultyConfig::instance().pollReload();
    static std::uint64_t difficultyRevision = 0;
    const std::uint64_t currentRevision =
        NpcDifficultyConfig::instance().revision();
    if (npcSystem && difficultyRevision != currentRevision) {
        static_cast<NpcSystem*>(npcSystem)->refreshDifficultyTuning();
        difficultyRevision = currentRevision;
    }
    MimitaNet::CommunityServerConfig::instance().pollReload();
    MimitaGamemode::ModePackRegistry::instance().pollReload();
    SpawnVelocityConfig::instance().pollReload();
    MovementJsonConfig::instance().pollReload();
    CollisionConfig::instance().pollHotReload(tick);
    RoleMovementCache::instance().pollReload();
    if (MatchRoleRegistry::instance().pollReload()) {
        ActorPresetWeapons::refresh();
    }
    BehaviorProfileRegistry::instance().pollReload();
    RoleMovementCache::instance().pollReload();
    WeaponData::reloadBuiltinWeaponsIfChanged();
    return 1;
}

int serverPolicyAuthIdentity(
    mimita::host::ServerAuthIdentityV1* result) {
    if (!result) {
        return 0;
    }
    result->version = mimita::host::kGameServerPolicyAbiVersion;
    result->structSize = sizeof(*result);
    const auto& user = AuthSystem::instance().user();
    result->userId = static_cast<std::uint64_t>(user.id);
    std::snprintf(result->token, sizeof(result->token), "%s",
                  user.sessionToken.c_str());
    return 1;
}

int serverPolicyProjectilePerf(
    mimita::host::ServerProjectilePerfV1* result) {
    if (!result) {
        return 0;
    }
    result->version = mimita::host::kGameServerPolicyAbiVersion;
    result->structSize = sizeof(*result);
    const MimitaNet::ServerProjectilePerfStats stats =
        MimitaNet::consumeServerProjectilePerfStats();
    result->projectileSimUs = stats.projectileSimUs;
    result->triangleQueryCount = stats.triangleQueryCount;
    result->triangleCandidateTotal = stats.triangleCandidateTotal;
    result->triangleCandidateMax = stats.triangleCandidateMax;
    result->playerCapsuleCandidateTotal = stats.playerCapsuleCandidateTotal;
    result->playerCapsuleCandidateMax = stats.playerCapsuleCandidateMax;
    result->correctionPackets = stats.correctionPackets;
    result->correctionBytes = stats.correctionBytes;
    result->activeProjectiles = stats.activeProjectiles;
    result->movingProjectiles = stats.movingProjectiles;
    result->sleepingProjectiles = stats.sleepingProjectiles;
    return 1;
}

int serverPolicyClearRuntimeObjects(const char* reason) {
    clearRuntimeObjects(reason ? reason : "server-policy");
    return 1;
}

int serverPolicyCoordinatorIceDone(const char* roomCode) {
    if (!roomCode || !*roomCode) {
        return 1;
    }
    const std::string code(roomCode);
    if (code.find("LOCAL-") == 0) {
        return 1;
    }
    MimitaNet::coordinatorIceDone(code);
    return 1;
}

int serverPolicyShutdown() {
    VehicleSystem::instance().setServerProcess(false);
    return 1;
}

int serverPolicyIceDecide(mimita::host::ServerIcePolicyV1* inout) {
    if (!inout ||
        inout->version != mimita::host::kGameServerPolicyAbiVersion ||
        inout->structSize < sizeof(mimita::host::ServerIcePolicyV1)) {
        return 0;
    }
    MimitaNet::serverIcePolicyDecide(*inout);
    return 1;
}

int serverPolicySessionDecide(mimita::host::ServerSessionPolicyV1* inout) {
    if (!inout ||
        inout->version != mimita::host::kGameServerPolicyAbiVersion ||
        inout->structSize < sizeof(mimita::host::ServerSessionPolicyV1)) {
        return 0;
    }
    MimitaNet::serverSessionPolicyDecide(*inout);
    return 1;
}

}  // namespace mimita::game

extern "C" __declspec(dllexport)
const mimita::host::GameServerPolicyV1* GetMimitaGameServerPolicyV1(
    std::uint32_t abiVersion) {
    if (abiVersion != mimita::host::kGameServerPolicyAbiVersion) {
        return nullptr;
    }
    static const mimita::host::GameServerPolicyV1 api = [] {
        mimita::host::GameServerPolicyV1 policy{};
        policy.abiVersion = mimita::host::kGameServerPolicyAbiVersion;
        policy.structSize = static_cast<std::uint32_t>(sizeof(policy));
        policy.runtimeRevision = 1;
        policy.startup = &mimita::game::serverPolicyStartup;
        policy.world_load = &mimita::game::serverPolicyWorldLoad;
        policy.configure = &mimita::game::serverPolicyConfigure;
        policy.create_player = &mimita::game::serverPolicyCreatePlayer;
        policy.destroy_player = &mimita::game::serverPolicyDestroyPlayer;
        policy.make_udp_connection_id =
            &mimita::game::serverPolicyMakeUdpConnectionId;
        policy.allocate_ice_connection_id =
            &mimita::game::serverPolicyAllocateIceConnectionId;
        policy.legacy_endpoint_for_connection =
            &mimita::game::serverPolicyLegacyEndpoint;
        policy.clear_actor_preset = &mimita::game::serverPolicyClearActorPreset;
        policy.poll_reload = &mimita::game::serverPolicyPollReload;
        policy.auth_identity = &mimita::game::serverPolicyAuthIdentity;
        policy.projectile_perf = &mimita::game::serverPolicyProjectilePerf;
        policy.clear_runtime_objects =
            &mimita::game::serverPolicyClearRuntimeObjects;
        policy.coordinator_ice_done =
            &mimita::game::serverPolicyCoordinatorIceDone;
        policy.shutdown = &mimita::game::serverPolicyShutdown;
        policy.ice_decide = &mimita::game::serverPolicyIceDecide;
        policy.session_decide = &mimita::game::serverPolicySessionDecide;
        return policy;
    }();
    return &api;
}
