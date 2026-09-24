// 07 19 2026, 11 05
/* purpose
* Owns authoritative server startup, fixed-step scheduling, and tick orchestration.
* Routes packets into server subsystems and reports server tick health diagnostics.
* Keeps gameplay simulation at the shared fixed 60 Hz server delta.
* Does NOT render, implement client prediction, or create local-only gameplay rules.
* Does NOT own weapon definitions, projectile physics internals, or packet schemas.
* Does NOT change simulation delta to compensate for load.
*/

#include "network/server.h"
#include "network/server-context.h"
#include "network/actor-state.h"
#include "ecs/actor-entities.h"
#include "network/dynamic-replication.h"
#include "network/net_mode.h"
#include "network/server-gamemode.h"
#include "gamemode/gamemode.h"
#include "gamemode/match-roles.h"
#include "npc/npc-behavior.h"
#include "gamemode/gamemode-map-pool.h"
#include "duel/duel-weapon-pool.h"
#include "network/community-server-config.h"
#include "network/multiplayer-context.h"
#include "hot-reload/generic-runtime.h"
#include "hot-reload/hot-server-policy.h"
#include "hot-reload/hot-npc-lifecycle.h"
#include "hot-reload/hot-server-tick.h"
#include "live-code/net-hot-log.h"
#include "network/coordinator-client.h"
#include "network/network-weapons.h"
#include "network/ice-transport.h"
#include "void-death/void-death.h"
#include "combat/weapon-data.h"
#include "combat/weapon-registry.h"
#include "combat/area-effect.h"
#include "npc/npc.h"
#include "npc/npc-difficulty-config.h"
#include "npc/npc-combat-log.h"
#include "entities/player.h"
#include "world/world.h"
#include "map/map-catalog.h"
#include "config/networking-config.h"
#include "config/movement-config.h"
#include "config/spawn-velocity-config.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "hot-reload/hot-reload-system.h"
#include "hot-reload/generation-distribution.h"
#include "live-code/live-behavior.h"
#include "live-code/live-identity.h"
#include "live-code/live-journal.h"
#include "project/project-control.h"
#include "audio/audio.h"
#include "persistence/persistence-queue.h"
#include "auth/auth-system.h"
#include "debug/structured-log.h"

#include <algorithm>
#include <cstdio>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <random>
#include <system_error>
#include <thread>
#include <windows.h>

// Global-scope accessor into the client GUI's listen-server state (gui-main.cpp).
namespace MimitaNet { struct ListenServerState; }
MimitaNet::ListenServerState* getListenServerState();

namespace MimitaNet {

namespace {

struct ServerLoopPerf
{
    std::array<double, 240> loopMs{};
    uint32_t sampleCount = 0;
    uint32_t sampleWrite = 0;
    uint64_t totalLoopUs = 0;
    double maxLoopMs = 0.0;
    uint64_t overrunCount = 0;
    uint64_t cappedCatchupCount = 0;
};

struct ServerTransportStats
{
    uint64_t recvAttempts = 0;
    uint64_t recvWouldBlock = 0;
    uint64_t recvErrors = 0;
    uint64_t malformedPackets = 0;
    uint64_t protocolMismatches = 0;
    uint64_t unknownPacketTypes = 0;
    uint64_t helloPackets = 0;
    uint64_t joinPackets = 0;
    uint64_t reconnectPackets = 0;
    uint64_t inputPackets = 0;
};

std::string processPath()
{
    char path[MAX_PATH] = {};
    DWORD length = GetModuleFileNameA(nullptr, path, (DWORD)sizeof(path));
    if (length == 0 || length >= sizeof(path))
        return "(unknown)";
    return std::string(path, path + length);
}

std::string currentDirectory()
{
    std::error_code error;
    std::filesystem::path path = std::filesystem::current_path(error);
    if (error)
        return "(unknown)";
    return path.string();
}

sockaddr_in actualSocketAddress(SOCKET sock, const sockaddr_in& fallback)
{
    sockaddr_in actual = fallback;
    int actualLen = sizeof(actual);
    if (getsockname(sock, (sockaddr*)&actual, &actualLen) != 0)
        actual = fallback;
    return actual;
}

bool resolveServerBindAddress(const LaunchOptions& options,
                              sockaddr_in& bindAddr,
                              std::string& requested)
{
    if (options.bindExplicit)
        requested = options.bind;
    else if (options.connectExplicit)
        requested = options.connect;
    else
        requested = "0.0.0.0:" + std::to_string(DEFAULT_PORT);
    return parseAddress(requested, bindAddr, true);
}

void countPacketType(ServerTransportStats& stats, uint8_t type)
{
    if (type == PACKET_HELLO)
        ++stats.helloPackets;
    else if (type == PACKET_JOIN_REQUEST)
        ++stats.joinPackets;
    else if (type == PACKET_RECONNECT_REQUEST)
        ++stats.reconnectPackets;
    else if (type == PACKET_INPUT)
        ++stats.inputPackets;
}

bool isKnownPacketType(uint8_t type)
{
    // 2026-08-29: TODO use an explicit switch here so future packet types
    // cannot be silently rejected by an outdated numeric range.
    // 2026-09-12: raised to the newest defined type so client->server
    // PACKET_CODE_GENERATION is accepted.
    return type >= PACKET_HELLO && type <= PACKET_CONSTRAINT_SNAPSHOT;
}

void recordServerLoopPerf(ServerLoopPerf& perf, uint64_t loopUs, bool cappedCatchup)
{
    const double loopMs = (double)loopUs / 1000.0;
    perf.loopMs[perf.sampleWrite % perf.loopMs.size()] = loopMs;
    ++perf.sampleWrite;
    if (perf.sampleCount < perf.loopMs.size())
        ++perf.sampleCount;
    perf.totalLoopUs += loopUs;
    perf.maxLoopMs = std::max(perf.maxLoopMs, loopMs);
    if (loopUs > 16667)
        ++perf.overrunCount;
    if (cappedCatchup)
        ++perf.cappedCatchupCount;
}

double serverLoopP95Ms(const ServerLoopPerf& perf)
{
    if (perf.sampleCount == 0)
        return 0.0;
    std::array<double, 240> copy = perf.loopMs;
    std::sort(copy.begin(), copy.begin() + perf.sampleCount);
    size_t idx = (size_t)std::ceil((double)perf.sampleCount * 0.95) - 1;
    if (idx >= perf.sampleCount)
        idx = perf.sampleCount - 1;
    return copy[idx];
}

void reportServerPerf(const char* label,
                      ServerLoopPerf& perf,
                      uint32_t tick,
                      uint32_t previousTick,
                      uint64_t elapsedMs)
{
    const ServerProjectilePerfStats projectile = consumeServerProjectilePerfStats();
    const double elapsedSec = std::max(0.001, (double)elapsedMs / 1000.0);
    const double hz = (double)(tick - previousTick) / elapsedSec;
    const double avgLoopMs = perf.sampleCount > 0
        ? (double)perf.totalLoopUs / (double)perf.sampleCount / 1000.0
        : 0.0;
    const double p95LoopMs = serverLoopP95Ms(perf);
    const double projectileSimMs = (double)projectile.projectileSimUs / 1000.0;

    printf("%s [SERVER PERF] hz=%.1f loopAvg=%.3fms loopP95=%.3fms loopMax=%.3fms "
           "overruns=%llu cappedCatchup=%llu activeProjectiles=%u moving=%u sleeping=%u "
           "projectileSim=%.3fms triQueries=%llu triCandidates=%llu triMax=%u "
           "playerCapsuleCandidates=%llu playerCapsuleMax=%u projectileCorrections=%llu correctionBytes=%llu\n",
           label, hz, avgLoopMs, p95LoopMs, perf.maxLoopMs,
           (unsigned long long)perf.overrunCount,
           (unsigned long long)perf.cappedCatchupCount,
           projectile.activeProjectiles, projectile.movingProjectiles,
           projectile.sleepingProjectiles, projectileSimMs,
           (unsigned long long)projectile.triangleQueryCount,
           (unsigned long long)projectile.triangleCandidateTotal,
           projectile.triangleCandidateMax,
           (unsigned long long)projectile.playerCapsuleCandidateTotal,
           projectile.playerCapsuleCandidateMax,
           (unsigned long long)projectile.correctionPackets,
           (unsigned long long)projectile.correctionBytes);

    perf = ServerLoopPerf{};
}

// ── Hot server tick orchestration policy (network.tick) ────────────────
// The cold loop owns the call sequence, sockets, threads, and persistent state;
// a hot `server.tick` system owns the orchestration decisions. Both the
// dedicated and listen tick bodies resolve this one shared policy path.
constexpr uint32_t kServerMaxCatchupSteps = 5;

struct ServerTickPolicy
{
    bool requestShutdown = false;
    bool snapshotDue = true;
    bool runGameplayDomain = true;
    bool runPostMovement = true;
    bool advanceGeneration = true;
    uint32_t catchupCap = 0;  // 0 = leave the cold cap
    char reason[48] = {};
};

ServerTickPolicy resolveServerTickPolicy(uint64_t tick, uint32_t catchupSteps,
                                         uint32_t playersActive,
                                         uint32_t projectilesActive,
                                         bool autoExitDue, bool snapshotDue,
                                         bool gameplayDomainDue)
{
    GameServerTickV1 request{};
    request.structSize = sizeof(GameServerTickV1);
    request.abiVersion = GAME_SERVER_TICK_VERSION;
    request.tick = tick;
    // Generation metadata is 0 here to avoid a per-tick status() lock; a hot
    // policy that needs it resolves `runtime.info` instead.
    request.generation = 0;
    request.dt = (float)SERVER_DT;
    request.catchupSteps = catchupSteps;
    request.maxCatchup = kServerMaxCatchupSteps;
    request.playersActive = playersActive;
    request.projectilesActive = projectilesActive;
    request.autoExitDue = autoExitDue ? 1u : 0u;
    request.snapshotDue = snapshotDue ? 1u : 0u;
    request.gameplayDomainDue = gameplayDomainDue ? 1u : 0u;

    void* callable =
        MimitaRuntime::GenericRuntime::instance().capability(GAME_CAP_SERVER_TICK);
    auto fn = callable ? reinterpret_cast<GameServerTickFn>(callable) : nullptr;
    if (fn)
        fn(nullptr, &request);
    else
        HotServerTickImpl::evaluate(request);

    ServerTickPolicy policy;
    if (!request.handled)
        return policy;  // cold defaults
    policy.requestShutdown = request.outRequestShutdown != 0u;
    policy.snapshotDue = request.outSnapshotDue != 0u;
    policy.runGameplayDomain = request.outRunGameplayDomain != 0u;
    policy.runPostMovement = request.outRunPostMovement != 0u;
    policy.advanceGeneration = request.outAdvanceGeneration != 0u;
    policy.catchupCap = request.outCatchupSteps;
    std::snprintf(policy.reason, sizeof(policy.reason), "%s", request.reason);
    return policy;
}

} // namespace

// Forward declaration for background listen server thread
static void listenServerThreadFunc(ListenServerState& state);

namespace {

ServerGameOverrides gServerOverrides;

std::vector<std::string> communityMapPool()
{
    // Automatic community rotation is deliberately restricted to the
    // validated, JSON-owned gamemode pool. Explicit map commands still use
    // the normal map-loading path and may select maps outside this pool.
    return GamemodeMapPool::instance().list();
}

} // namespace

std::string gServerHostPlayerName;

ServerGameOverrides& serverGameOverrides()
{
    return gServerOverrides;
}

bool isServerHost()
{
    // Dedicated server process.
    {
        const char* cmd = GetCommandLineA();
        if (cmd && (strstr(cmd, "--server") || strstr(cmd, "-server")))
            return true;
    }
    // Listen server active in this process (hosting via the in-game menu).
    if (hostedRoomSession().active)
        return true;
    if (ListenServerState* s = ::getListenServerState())
        return s->active;
    return false;
}

int runServer(const LaunchOptions& options)
{
    setvbuf(stdout, nullptr, _IONBF, 0);

    // Disable audio on dedicated server — it only wastes resources
    setServerAudioMode(true);

    ::StructuredLogger::instance().init();

    // Server-side live-code lifecycle. The dedicated server bypasses gameInit
    // (main.cpp handles --server before it), so it must start the hot loader and
    // the live journal itself. Without this the authoritative path silently used
    // the JSON fallback and wrote no policy evidence.
    LiveEventJournal::instance().init();
    LiveIdentity::setProcess("server");
    LiveIdentity::setSessionId((std::uint64_t)nowMs());
    HotReloadSystem::instance().startup();
    Project::ProjectControl::instance().init(std::filesystem::current_path());
    {
        const HotReloadSystem::Status liveStatus = HotReloadSystem::instance().status();
        printf("%s [SERVER LIVE CODE] loaded=%d generation=%u code_hash=%s\n",
               serverTimestamp(), (int)liveStatus.loaded, liveStatus.activeGeneration,
               liveStatus.activeHash.empty() ? "(none)" : liveStatus.activeHash.c_str());
        LiveEventJournal::Fields started;
        started.result = "started";
        started.hotGeneration = liveStatus.activeGeneration;
        started.hotHash = liveStatus.activeHash;
        started.extra = std::string("\"loaded\":") + (liveStatus.loaded ? "1" : "0");
        LiveEventJournal::instance().record("server.started", started);
        emitNetworkLog(2u, "NETWORK", "network.server.started",
                       "dedicated server started", 0);
    }

    // Load the standard player body shape headlessly so the authoritative
    // server can reconstruct real body-part hitboxes (players + NPCs) for hit
    // validation. This is the single source of truth for body shape.
    if (gServerBodyTemplate.empty())
        loadServerBodyTemplateFromGlb(nullptr, gServerBodyTemplate);

    WeaponData::registerBuiltinWeapons();
    printf("%s [SERVER] registered built-in weapons\n", serverTimestamp());

    // Load NPC difficulty config so server-authoritative NPC damage/fire rate
    // honors config/npc-difficulty.json (hot-reloaded in the server loop below).
    NpcDifficultyConfig::instance().load("config/npc-difficulty.json");
    CommunityServerConfig::instance().load();
    GamemodeRegistry::instance().loadDirectory("config/gamemodes");
    MatchRoleRegistry::instance().load("config/roles.json");
    BehaviorProfileRegistry::instance().load("config/behavior-profiles.json");
    GamemodeMapPool::instance().load("config/gamemode-good-maps.json");
    DuelWeaponPool::instance().load("config/duel-weapons.json");
    npcLogSetProc("server");

    // Validate grenade launcher config at startup
    {
        const WeaponDefinition* glDef = WeaponRegistry::instance().get("grenade_launcher");
        if (glDef) {
            printf("%s [SERVER GRENADE CONFIG] projectileSpeed=%.1f projectileRadius=%.2f "
                   "projectileLifetime=%.1f fireDelay=%.2f customParams=%zu\n",
                   serverTimestamp(), glDef->projectileSpeed, glDef->projectileRadius,
                   glDef->projectileLifetime, glDef->fireDelay, glDef->customParams.size());
            auto cp = [&](const char* key, float fb) {
                auto it = glDef->customParams.find(key);
                return it != glDef->customParams.end() ? it->second : fb;
            };
            printf("%s [SERVER GRENADE PHYSICS] gravity=%.1f drag=%.2f restitution=%.2f "
                   "friction=%.2f upBias=%.1f maxBounce=%.0f forwardSpeed=%.1f\n",
                   serverTimestamp(), cp("gravity", 20.0f), cp("drag", 0.15f),
                   cp("bounceRestitution", 0.35f), cp("bounceFriction", 0.5f),
                   cp("upBias", 4.0f), cp("maxBounceCount", 10.0f),
                   cp("forwardSpeed", 18.0f));
        } else {
            printf("%s [SERVER GRENADE CONFIG] grenade_launcher NOT FOUND in registry\n", serverTimestamp());
        }
    }

    printf("%s [SERVER] ========================================\n", serverTimestamp());
    printf("%s [SERVER] MiMITA Dedicated Server\n", serverTimestamp());
    printf("%s [SERVER] protocol version=%u\n", serverTimestamp(), PROTOCOL_VERSION);
    printf("%s [SERVER] tick rate=%.0f Hz\n", serverTimestamp(), SERVER_TICK_RATE);
    printf("%s [SERVER] max players=%d\n", serverTimestamp(), MAX_PLAYERS);
    printf("%s [SERVER] timeout=%llums\n", serverTimestamp(), (unsigned long long)CLIENT_TIMEOUT_MS);
    printf("%s [SERVER] coordinator=%s\n", serverTimestamp(),
           getCoordinatorUrl().c_str());
    printf("%s [SERVER] ========================================\n", serverTimestamp());

    printf("%s [SERVER START SETTINGS] map=%s npcCount=%u serverName=%s roomCode=%s\n",
           serverTimestamp(),
           options.mapName.c_str(), options.npcCount,
           options.name.c_str(), "pending");

    // Determine map path from options
    std::string mapName = options.mapName.empty() ? "funworld3" : options.mapName;
    std::string mapPath = "assets/maps/" + mapName + ".glb";
    setServerMapId(mapName);
    printf("%s [SERVER MAP] mapId=%s path=%s\n", serverTimestamp(), mapName.c_str(), mapPath.c_str());

    {
        std::error_code ec;
        bool exists = std::filesystem::exists(mapPath, ec);
        bool isDir = std::filesystem::is_directory(mapPath, ec);
        auto fsize = exists && !isDir ? std::filesystem::file_size(mapPath, ec) : 0;
        printf("%s [SERVER WORLD PATH] original=%s exists=%d isFile=%d isDirectory=%d size=%lld\n",
               serverTimestamp(), mapPath.c_str(), (int)exists, (int)(exists && !isDir), (int)isDir, (long long)fsize);
    }

    HeadlessWorld world;
    if (!loadHeadlessWorld(mapPath.c_str(), world))
        printf("%s [SERVER WORLD] WARNING: headless GLB collision load failed; using floor fallback\n", serverTimestamp());

    // Real client World + NpcSystem so online NPCs run the exact local NPC AI.
    World npcWorld;
    buildNpcWorldCollision(npcWorld, world);
    NpcSystem npcSystem;
    Player mirrorPlayer;
    std::unordered_set<uint32_t> npcIdsAlive;

    if (!netStartup())
    {
        printf("%s [SERVER] FATAL: WSAStartup failed\n", serverTimestamp());
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET)
    {
        printf("%s [SERVER] FATAL: socket() failed error=%d\n", serverTimestamp(), WSAGetLastError());
        netShutdown();
        return 1;
    }
    disableUdpConnReset(sock);

    int reuseAddr = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuseAddr, sizeof(reuseAddr)) == SOCKET_ERROR)
        printf("%s [SERVER] WARNING: setsockopt SO_REUSEADDR failed error=%d (non-fatal)\n", serverTimestamp(), WSAGetLastError());

    const bool nonBlockingOk = setNonBlocking(sock);

    sockaddr_in bindAddr{};
    std::string requestedBind;
    if (!resolveServerBindAddress(options, bindAddr, requestedBind))
    {
        printf("%s [SERVER] FATAL: invalid bind address requested=%s family=AF_INET\n",
               serverTimestamp(), requestedBind.c_str());
        closesocket(sock);
        netShutdown();
        return 1;
    }
    if (bind(sock, (sockaddr*)&bindAddr, sizeof(bindAddr)) == SOCKET_ERROR)
    {
        int err = WSAGetLastError();
        printf("%s [SERVER] FATAL: bind() failed error=%d\n", serverTimestamp(), err);
        if (err == WSAEADDRINUSE)
            printf("%s [SERVER] HINT: Address %s is already in use. Is another server already running?\n",
                   serverTimestamp(), addressToString(bindAddr).c_str());
        closesocket(sock);
        netShutdown();
        return 1;
    }

    sockaddr_in actualBindAddr = actualSocketAddress(sock, bindAddr);
    const uint16_t actualPort = ntohs(actualBindAddr.sin_port);
    printf("%s [SERVER TRANSPORT READY] protocol=%u role=dedicated-udp pid=%lu exe=\"%s\" cwd=\"%s\" "
           "requested=%s actual=%s family=AF_INET reuse=1 nonblocking=%d\n",
           serverTimestamp(), PROTOCOL_VERSION, (unsigned long)GetCurrentProcessId(),
           processPath().c_str(), currentDirectory().c_str(),
           requestedBind.c_str(), addressToString(actualBindAddr).c_str(),
           (int)nonBlockingOk);
    printf("%s [SERVER] bound to %s\n", serverTimestamp(), addressToString(actualBindAddr).c_str());
    printf("%s [SERVER] waiting for connections...\n", serverTimestamp());

    std::unordered_map<uint32_t, ServerPlayer> players;
    std::unordered_map<uint32_t, ServerNpc> npcs;
    std::unordered_map<uint32_t, ServerProjectile> projectiles;
    uint32_t nextPlayerId = 1;
    uint32_t nextEntityId = 1000;
    uint32_t nextProjectileId = 1;
    uint32_t tick = 0;
    uint64_t lastLog = nowMs();
    uint64_t totalPacketsIn = 0;
    uint64_t totalPacketsOut = 0;
    ServerPacketStats transportStats;
    DisagreementRetransmitState disagreementRetransmit;

    // Generic authoritative server context: valid for the whole server run so
    // kernel capabilities (projectile.spawn, damage.apply, ...) can perform real
    // world actions on behalf of hot code. The raw containers remain private.
    MimitaNet::ServerContextV1 serverContext;
    serverContext.sock = static_cast<std::uintptr_t>(sock);
    serverContext.players = &players;
    serverContext.npcs = &npcs;
    serverContext.projectiles = &projectiles;
    serverContext.nextProjectileId = &nextProjectileId;
    serverContext.tick = &tick;
    serverContext.totalPacketsOut = &totalPacketsOut;
    serverContext.world = &world;
    MimitaNet::setActiveServerContext(&serverContext);
    struct ServerContextGuard {
        ~ServerContextGuard() { MimitaNet::setActiveServerContext(nullptr); }
    } serverContextGuard;

    // ── Dedicated server ICE support ──
    ListenServerState dedicatedIceState;
    dedicatedIceState.serverName = options.name.empty() ? "MiMITA Server" : options.name;
    dedicatedIceState.mapName = mapName;
    dedicatedIceState.gameMode = options.gameMode;
    dedicatedIceState.maxPlayers = options.maxPlayers;
    dedicatedIceState.weaponSetId = options.weaponSetId;
    dedicatedIceState.autoMapRotation = options.autoMapRotation;
    dedicatedIceState.mapRotationMinutes = options.mapRotationMinutes;
    dedicatedIceState.discordNotification = options.discordNotification;
    dedicatedIceState.passwordProtected = options.passwordProtected;
    dedicatedIceState.password = options.password;
    dedicatedIceState.hostPlayerName = options.hostPlayerName;
    // The host is identified by name so the person who launched the server can
    // run host commands (healthall / setspawn / npc_delete_all), no account.
    gServerHostPlayerName = options.hostPlayerName;
    dedicatedIceState.port = actualPort;
    std::vector<PendingServerTransport> pendingIceTransports;

    // Duel vs community mode selection is hot (net.server-policy).
    GameServerModeV1 modePolicy{};
    modePolicy.structSize = sizeof(GameServerModeV1);
    modePolicy.duelRequested = options.duel ? 1u : 0u;
    modePolicy.gameModeIsSandbox = options.gameMode == "sandbox" ? 1u : 0u;
    auto serverPolicyFn = reinterpret_cast<GameServerPolicyLookupFn>(
        MimitaRuntime::GenericRuntime::instance().capability(GAME_CAP_SERVER_POLICY));
    if (serverPolicyFn && serverPolicyFn(nullptr) && serverPolicyFn(nullptr)->mode)
        serverPolicyFn(nullptr)->mode(nullptr, &modePolicy);
    else
        HotServerPolicyImpl::mode(modePolicy);

    // Duel mode: run a first-to-goal PvP match between the first two players.
    if (modePolicy.useDuel)
    {
        const Gamemode& gm = GamemodeRegistry::instance().get(options.gamemodeId);
        ServerGamemodeState duelRules;
        duelRules.goalValue = gm.goalValue;
        duelRules.countdownSeconds = gm.countdownSeconds;
        duelRules.rematchSeconds = gm.rematchSeconds;
        duelRules.teamAName = gm.teamNames.size() > 0 ? gm.teamNames[0] : "RED";
        duelRules.teamBName = gm.teamNames.size() > 1 ? gm.teamNames[1] : "BLUE";
        duelRules.spawnOffsetRadius = gm.spawnOffsetRadius;
        duelRules.mapPool = GamemodeMapPool::instance().list();
        duelRules.rotateMaps = true;
        duelRules.mapId = mapName;
        serverGamemodeStart(duelRules);
        printf("%s [SERVER DUEL] enabled gamemode=%s goal=%d countdown=%.1fs rematch=%.1fs\n",
               serverTimestamp(), options.gamemodeId.c_str(), gm.goalValue,
               gm.countdownSeconds, gm.rematchSeconds);
    }
    else
    {
        serverCommunityMapStart(communityMapPool(), mapName,
                                options.autoMapRotation,
                                options.mapRotationMinutes,
                                options.weaponSetId);
        serverCommunitySetMode(options.gameMode);
        serverCommunitySetWeaponSet(options.weaponSetId);
        if (modePolicy.startMatch)
            serverCommunityStartMatch(false);
    }

    // Startup NPCs. The EXE no longer permanently decides the count: it records
    // the launch options as live facts and asks the hot npc.lifecycle policy.
    // The per-tick reconciliation then keeps the automatic set aligned, so a
    // live policy/config change applies without a restart.
    serverGameOverrides().startupNpcsEnabled = options.npcsEnabled;
    serverGameOverrides().startupNpcCount = options.npcCount;
    {
        auto npcLifecycleFn = reinterpret_cast<GameNpcLifecycleFn>(
            MimitaRuntime::GenericRuntime::instance().capability(
                GAME_CAP_NPC_LIFECYCLE));
        NpcLifecyclePolicyV1 npcPlan{};
        npcPlan.structSize = sizeof(NpcLifecyclePolicyV1);
        npcPlan.reason = GAME_NPC_LIFECYCLE_STARTUP;
        npcPlan.configStartupEnabled = options.npcsEnabled ? 1u : 0u;
        npcPlan.requestedCount = options.npcCount;
        npcPlan.spawnPointCount = (uint32_t)world.spawnPoints.size();
        npcPlan.maxSpawn = 256u;
        if (npcLifecycleFn)
            npcLifecycleFn(nullptr, &npcPlan);
        else
            HotNpcLifecycleImpl::evaluate(npcPlan);
        uint32_t npcCount = npcPlan.spawnCount;
        for (uint32_t i = 0; i < npcCount; ++i)
        {
            ServerNpc npc;
            npc.entityId = nextEntityId++;
            npc.origin = GAME_NPC_ORIGIN_STARTUP;
            npc.name = "NPC " + std::to_string(i + 1);
            if (npcPlan.outHealth > 0)
                npc.health = (int)npcPlan.outHealth;
            if (npcPlan.startingWeapon[0])
                npc.startingWeapon = npcPlan.startingWeapon;
            // Generic origin component authority at creation.
            {
                const EntityId npcIdentity =
                    Ecs::ensure(EntityRealm::Server, EntityDomain::Npc,
                                npc.entityId);
                const std::uint64_t weaponHash = npcPlan.startingWeapon[0]
                    ? static_cast<std::uint64_t>(
                          gameHash(npcPlan.startingWeapon))
                    : 0;
                actorStateWriteOrigin(Ecs::raw(npcIdentity), npc.origin,
                                      weaponHash);
                actorStateWriteLifecycle(Ecs::raw(npcIdentity), 1u, 0u, 0.0f);
            }
            if (npcPlan.useSpawnPoints && !world.spawnPoints.empty())
            {
                size_t idx = i % world.spawnPoints.size();
                npc.pos = effectiveServerSpawn(world.spawnPoints[idx].position);
                npc.yaw = world.spawnPoints[idx].yaw;
                // Hot spawn policy: allows suppressing/relocating startup NPCs
                // (e.g. never spawn one on a player spawn point).
                {
                    ActorSpawnPolicyV1 sp{};
                    sp.kind = GAME_ACTOR_SPAWN_NPC;
                    sp.index = i;
                    sp.spawnPointCount = (uint32_t)world.spawnPoints.size();
                    sp.candidateCount = world.spawnPoints.size() < 8
                        ? (uint32_t)world.spawnPoints.size() : 8u;
                    for (uint32_t c = 0; c < sp.candidateCount; ++c) {
                        sp.candidatePosition[c][0] = world.spawnPoints[c].position.x;
                        sp.candidatePosition[c][1] = world.spawnPoints[c].position.y;
                        sp.candidatePosition[c][2] = world.spawnPoints[c].position.z;
                        sp.candidateYaw[c] = world.spawnPoints[c].yaw;
                    }
                    sp.chosenPosition[0] = npc.pos.x;
                    sp.chosenPosition[1] = npc.pos.y;
                    sp.chosenPosition[2] = npc.pos.z;
                    sp.chosenYaw = npc.yaw;
                    if (LiveBehavior::dispatchGameplayEvent64(
                            GAME_EVENT_ACTOR_SPAWN_POLICY, &sp, sizeof(sp), 0, 0, 0) &&
                        sp.handled)
                    {
                        if (sp.suppress)
                            continue;
                        npc.pos = glm::vec3(sp.position[0], sp.position[1], sp.position[2]);
                        npc.yaw = sp.yaw;
                    }
                }
                printf("%s [SERVER NPC SPAWN] reason=startup entityId=%u npcIndex=%u "
                       "spawnpoint=%zu position=(%.2f,%.2f,%.2f)\n",
                       serverTimestamp(), npc.entityId, i, idx,
                       npc.pos.x, npc.pos.y, npc.pos.z);
            }
            else
            {
                npc.pos = {4.0f + i * 2.0f, 8.0f, 30.0f};
            }
            npc.phase = i * 2.0f;
            npcs[npc.entityId] = npc;
        }
        printf("%s [SERVER NPC STARTUP] enabled=%d requested=%u spawned=%zu\n",
               serverTimestamp(), (int)options.npcsEnabled, npcCount, npcs.size());
    }

    // ── Register server with coordinator (ICE only) ──
    std::string serverCode;
    {
        if (hostedRoomSession().active)
        {
            printf("[ROOM DUPLICATE ERROR] existingCode=%s attemptedCode=%s caller=headless-server-runServer\n",
                   hostedRoomSession().roomCode.c_str(), serverCode.c_str());
        }

        if (!initServerIceListener(dedicatedIceState))
        {
            printf("%s [SERVER ICE] init failed — aborting. Check STUN/TURN connectivity to %s:3478\n",
                   serverTimestamp(), getCoordinatorUrl().c_str());
            return 1;
        }
        serverCode = dedicatedIceState.serverCode;
        printf("%s [SERVER ICE] registered: code=%s\n",
               serverTimestamp(), serverCode.c_str());
    }

    // ── Write room code to --room-file (after registration) ──
    if (!options.roomFilePath.empty() && !serverCode.empty())
    {
        FILE* rf = fopen(options.roomFilePath.c_str(), "w");
        if (rf)
        {
            fprintf(rf, "%s\n", serverCode.c_str());
            fclose(rf);
            printf("[SERVER] wrote room code %s to %s\n", serverCode.c_str(), options.roomFilePath.c_str());
        }
    }

    uint64_t lastCoordinatorHb = 0;
    uint64_t iceCoordinatorPollCount = 0;

    uint64_t serverStartMs = nowMs();

    // Server runs in-process — do NOT override the game's crash handler.
    // The game's crash handler (installCrashHandler) writes minidumps and
    // crash logs. Overriding it would suppress those diagnostics and
    // change the exit code to STATUS_CONTROL_C_EXIT (0xC000013A).

    // Accumulator-based fixed-step timing
    auto previousTime = std::chrono::steady_clock::now();
    double accumulator = 0.0;
    constexpr int MAX_STEPS = 5;
    ServerLoopPerf loopPerf;
    uint32_t lastPerfTick = 0;
    uint64_t lastPerfMs = nowMs();

    PersistenceQueue::instance().beginSession(serverCode, AuthSystem::instance().user().sessionToken,
                                              AuthSystem::instance().user().id);
    bool shutdownRequested = false;
    while (true)
    {
        auto loopStart = std::chrono::steady_clock::now();
        // Auto-exit when --timeout is set (for CI/agent testing)
        if (options.timeoutSecs > 0 && nowMs() - serverStartMs > (uint64_t)options.timeoutSecs * 1000)
        {
            printf("%s [SERVER] timeout=%us reached, exiting\n", serverTimestamp(), options.timeoutSecs);
            break;
        }

        // Hot-reload networkingconfig.json so the hosted/dedicated server picks
        // up server_smoothing (and the shared badconn block) live.
        NetworkingConfig::instance().pollReload();

        // Hot-reload config/npc-difficulty.json so server NPC damage/fire rate
        // edits apply live without a server restart.
        NpcDifficultyConfig::instance().pollReload();
        static uint64_t dedicatedNpcDifficultyRevision = 0;
        if (dedicatedNpcDifficultyRevision != NpcDifficultyConfig::instance().revision())
        {
            npcSystem.refreshDifficultyTuning();
            dedicatedNpcDifficultyRevision = NpcDifficultyConfig::instance().revision();
        }
        CommunityServerConfig::instance().pollReload();
        DuelWeaponPool::instance().pollReload();
        // The authoritative server owns respawn velocity. Reload it here so
        // changing spawnvelocity.json affects the next life without restart.
        SpawnVelocityConfig::instance().pollReload();

        // Movement tuning is owned by the hot C++ Source authority; the server
        // reads it through movement.tuning, so movement JSON is not polled here.

        // Refresh cached role movement presets whose files changed on disk.
        RoleMovementCache::instance().pollReload();

        // Hot-reload config/roles.json so role/profile references apply live.
    MatchRoleRegistry::instance().pollReload();
    BehaviorProfileRegistry::instance().pollReload();
    RoleMovementCache::instance().pollReload();

        // Hot-reload config/weapons.json (rate-limited to 250ms internally) so
        // live weapon damage/falloff edits apply without a server restart.
        WeaponData::reloadBuiltinWeaponsIfChanged();

        // Measure wall-clock elapsed time
        auto currentTime = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(currentTime - previousTime).count();
        previousTime = currentTime;
        if (elapsed > 0.1) elapsed = 0.1;
        accumulator += elapsed;

        char buffer[2048];
        sockaddr_in from{};

        // Drain all pending packets
        for (;;)
        {
            int fromLen = sizeof(from);
            ++transportStats.recvAttempts;
            int bytes = recvfrom(sock, buffer, sizeof(buffer), 0, (sockaddr*)&from, &fromLen);
            if (bytes <= 0)
            {
                int wsaErr = WSAGetLastError();
                if (wsaErr != WSAEWOULDBLOCK)
                {
                    sockaddr_in localEp{};
                    int localEpLen = sizeof(localEp);
                    std::string localStr = "(unknown)";
                    if (getsockname(sock, (sockaddr*)&localEp, &localEpLen) == 0)
                        localStr = addressToString(localEp);
                    printf("%s [NET RX ERROR] sock=%d error=%d local=%s\n",
                           serverTimestamp(), (int)sock, wsaErr, localStr.c_str());
                    ++transportStats.recvErrors;
                }
                else
                    ++transportStats.recvWouldBlock;
                break;
            }
            TransportReceiveEvent event{};
            event.connectionId = makeUdpConnectionId(from);
            event.remoteEndpoint = from;
            event.payload = reinterpret_cast<const uint8_t*>(buffer);
            event.payloadBytes = bytes;
            event.receivedAtMs = nowMs();
            event.transportKind = TransportKind::Udp;
            processServerPacket(sock, event, players, npcs, projectiles,
                                nextPlayerId, nextEntityId, nextProjectileId,
                                world, tick, totalPacketsIn, totalPacketsOut,
                                &transportStats, &disagreementRetransmit);
        }

        ::StructuredLogger::instance().pollConfig();
        ::StructuredLogger::instance().tick();

        // Send the coordinator heartbeat BEFORE the simulation/gameplay work so
        // a long or starved tick cannot delay it past the room timeout (the room
        // would expire and later joins would fail). Rate-limited to 500ms
        // inside tickIceCoordinator, so the end-of-loop call is a no-op here.
        tickIceCoordinator(dedicatedIceState, players.size());

        // Accumulator-based timing: run simulation ticks for accumulated debt
        int steps = 0;
        while (accumulator >= (double)SERVER_DT && steps < MAX_STEPS)
        {
            // Hot fixed-tick orchestration policy (network.tick). Cold owns the
            // loop/sockets/threads; the hot system returns the decisions below.
            const ServerTickPolicy tickPolicy = resolveServerTickPolicy(
                tick, (uint32_t)steps, (uint32_t)players.size(),
                (uint32_t)projectiles.size(), false, true, true);

            // Coordinated live-code switch: when a candidate is validated, hold
            // it and announce the shared switch tick so the server and all
            // connected clients activate the same generation at that tick. The
            // old generation stays live until then.
            HotReloadSystem& hotReload = HotReloadSystem::instance();
            // Quorum-gated coordinated switch. Phase 0 announces a candidate so
            // peers acquire/verify and report READY; the switch is scheduled ONLY
            // once every required peer is READY for the exact generation. The
            // server holds its own activation (far-future switch tick) until then.
            static std::uint32_t announcedCandidateGen = 0;
            static bool switchCommitted = false;  // post-quorum SWITCH scheduled
            constexpr std::uint32_t kHoldSwitchTick = 0x7FFFFFFFu;
            bool doAnnounce = false;
            bool isSwitch = false;
            std::uint32_t switchTick = 0;
            if (hotReload.candidateReady())
            {
                const std::uint32_t candGen = hotReload.candidateGeneration();
                std::vector<std::uint64_t> required;
                for (auto& pe : players)
                    required.push_back(pe.first);
                const bool quorum = !required.empty() &&
                    MimitaRuntime::GenerationDistribution::instance().quorumReady(
                        required, candGen);
                // Pre-commit: a newer candidate may supersede the announced one.
                // Post-commit (switchCommitted): do NOT cancel a coordinated
                // SWITCH; let G activate, then H becomes the next candidate.
                if (candGen != announcedCandidateGen && !switchCommitted)
                {
                    doAnnounce = true;
                    isSwitch = false;
                    hotReload.requestSwitchAtTick(kHoldSwitchTick);  // hold
                    announcedCandidateGen = candGen;
                }
                else if (candGen == announcedCandidateGen && quorum &&
                         !switchCommitted &&
                         (!hotReload.switchPending() ||
                          hotReload.switchAtTick() == kHoldSwitchTick))
                {
                    doAnnounce = true;
                    isSwitch = true;
                    switchCommitted = true;
                    switchTick = tick + 30;
                }
            }
            if (doAnnounce)
            {
                if (isSwitch)
                {
                    hotReload.requestSwitchAtTick(switchTick);
                    MimitaRuntime::GenerationDistribution::instance().scheduleSwitch(
                        hotReload.candidateGeneration(), switchTick);
                }
                CodeGenerationPacket announce{};
                announce.header.type = PACKET_CODE_GENERATION;
                announce.header.tick = tick;
                announce.generation = hotReload.candidateGeneration();
                announce.direction = 1;  // server -> clients
                announce.phase = isSwitch ? 2u : 0u;  // 0 acquire/verify, 2 switch
                announce.switchTick = isSwitch ? switchTick : 0u;
                auto hexValue = [](char c) -> uint64_t {
                    if (c >= '0' && c <= '9') return (uint64_t)(c - '0');
                    if (c >= 'a' && c <= 'f') return (uint64_t)(c - 'a' + 10);
                    if (c >= 'A' && c <= 'F') return (uint64_t)(c - 'A' + 10);
                    return 0;
                };
                const std::string candidateHash = hotReload.candidateCodeHash();
                for (int i = 0; i + 1 < (int)candidateHash.size() && i < 16; i += 2)
                    announce.codeHash = (announce.codeHash << 8) |
                        (hexValue(candidateHash[i]) << 4) |
                        hexValue(candidateHash[i + 1]);
                announce.moduleSetHash =
                    MimitaRuntime::GenericRuntime::instance().manifestHash();
                announce.hotAbiVersion = MIMITA_GAME_API_VERSION;
                announce.logicalCodeHash =
                    announce.codeHash ^ (announce.moduleSetHash * 1099511628211ull);
                // Advertise the REAL platform artifact hash so peers can request
                // the exact immutable artifact by content identity.
                {
                    std::vector<unsigned char> candidateArtifact;
                    std::uint32_t candGen = 0;
                    std::uint64_t candHash = 0;
                    if (hotReload.readCandidateArtifact(candidateArtifact, candGen,
                                                        candHash))
                        announce.platformPackageHash = candHash;
                    else
                        announce.platformPackageHash = announce.generation;
                }
                // Carry the REAL bounded manifest (identity + ABI + declared
                // capability/schema/dependency requirements) so the peer verifies
                // the exact facts the server associated with G. Metadata only;
                // artifact bytes travel on the separate content-addressed stream.
                GenerationManifestPacket manifestPkt{};
                bool haveManifest = false;
                if (!isSwitch)
                {
                    MimitaRuntime::GenerationManifestV1 m{};
                    if (hotReload.buildCandidateManifest(m))
                    {
                        manifestPkt.header.type = PACKET_GENERATION_MANIFEST;
                        manifestPkt.header.tick = tick;
                        manifestPkt.manifestVersion = GENERATION_MANIFEST_VERSION;
                        manifestPkt.logicalGenerationId = m.logicalGenerationId;
                        manifestPkt.logicalBehaviorHash = m.logicalBehaviorHash;
                        manifestPkt.platformArtifactHash = m.platformArtifactHash;
                        manifestPkt.platformArtifactSize = m.platformArtifactSize;
                        manifestPkt.hotAbiVersion = m.hotAbiVersion;
                        manifestPkt.requiredCapabilityCount = m.requiredCapabilityCount;
                        manifestPkt.requiredSchemaCount = m.requiredSchemaCount;
                        manifestPkt.requiredDependencyCount = m.requiredDependencyCount;
                        for (uint32_t i = 0;
                             i < m.requiredCapabilityCount &&
                             i < GENERATION_MANIFEST_MAX_REQUIREMENTS;
                             ++i)
                            manifestPkt.requiredCapabilities[i] = m.requiredCapabilities[i];
                        for (uint32_t i = 0;
                             i < m.requiredSchemaCount &&
                             i < GENERATION_MANIFEST_MAX_REQUIREMENTS;
                             ++i)
                        {
                            manifestPkt.requiredSchemas[i] = m.requiredSchemas[i];
                            manifestPkt.requiredSchemaVersions[i] =
                                m.requiredSchemaVersions[i];
                        }
                        for (uint32_t i = 0;
                             i < m.requiredDependencyCount &&
                             i < GENERATION_MANIFEST_MAX_REQUIREMENTS;
                             ++i)
                            manifestPkt.requiredDependencies[i] = m.requiredDependencies[i];
                        haveManifest = true;
                    }
                }
                for (auto& pe : players)
                {
                    if (pe.second.transport)
                        pe.second.transport->send(&announce, sizeof(announce));
                    else
                        sendto(sock, (const char*)&announce, sizeof(announce), 0,
                               (sockaddr*)&pe.second.addr, sizeof(pe.second.addr));
                    if (haveManifest)
                    {
                        if (pe.second.transport)
                            pe.second.transport->send(&manifestPkt, sizeof(manifestPkt));
                        else
                            sendto(sock, (const char*)&manifestPkt, sizeof(manifestPkt), 0,
                                   (sockaddr*)&pe.second.addr, sizeof(pe.second.addr));
                    }
                    // Track per-peer readiness for THIS logical generation.
                    MimitaRuntime::GenerationIdentityV1 ident{};
                    ident.logicalGenerationId = announce.generation;
                    ident.logicalBehaviorHash = announce.logicalCodeHash;
                    ident.platformArtifactHash = announce.platformPackageHash;
                    ident.abiVersion = MIMITA_GAME_API_VERSION;
                    MimitaRuntime::GenerationDistribution::instance().announce(
                        pe.first, ident);
                }
                printf("%s [SERVER LIVE CODE] announce switch generation=%u switchTick=%u\n",
                       serverTimestamp(), announce.generation, switchTick);
                LiveEventJournal::Fields announced;
                announced.tick = tick;
                announced.hotGeneration = announce.generation;
                announced.result = isSwitch ? "switch" : "announce";
                announced.extra = std::string("\"switch_tick\":") +
                    std::to_string(isSwitch ? switchTick : 0u);
                LiveEventJournal::instance().record("server.hot_generation_announced",
                                                    announced);
            }
            if (hotReload.pollAndAdvance(tick))
            {
                switchCommitted = false;
                announcedCandidateGen = 0;
                const HotReloadSystem::Status liveStatus = hotReload.status();
                printf("%s [SERVER LIVE CODE] activated generation=%u hash=%s\n",
                       serverTimestamp(), liveStatus.activeGeneration,
                       liveStatus.activeHash.c_str());
                LiveEventJournal::Fields activated;
                activated.tick = tick;
                activated.hotGeneration = liveStatus.activeGeneration;
                activated.hotHash = liveStatus.activeHash;
                activated.result = "activated";
                LiveEventJournal::instance().record("server.hot_generation_activated",
                                                    activated);
            }
            LiveIdentity::setSimulationTick(tick);

            // Generic runtime systems execute in the server's fixed step too, so
            // a hot-registered system runs authoritatively on the dedicated
            // server with no per-system EXE call site.
            if (tickPolicy.runGameplayDomain)
            {
                MimitaRuntime::GenericRuntime& runtime =
                    MimitaRuntime::GenericRuntime::instance();
                void* runtimeHost = LiveBehavior::hostContext(tick);
                // Bind the authoritative headless collision world so hot
                // movement systems can resolve server actors with the shared
                // physics.move primitive.
                LiveBehavior::setDispatchHeadlessWorld(&world);
                runtime.runDomain(GAME_DOMAIN_GAMEPLAY, tick, (float)SERVER_DT, runtimeHost);
                // The active gamemode's own systems (data-driven domain routing).
                runtime.runActiveModeDomain(tick, (float)SERVER_DT, runtimeHost);
                runtime.runRegisteredDomains(tick, (float)SERVER_DT, runtimeHost);
                // Deliver any generic events emitted by those systems this tick.
                LiveBehavior::drainEvents(64);
            }

            handleClientTimeout(players, sock, tick, totalPacketsOut);
            for (auto& kv : players)
            {
                kv.second.shotsThisTick = 0;
                kv.second.attackPktsThisTick = 0;
            }
            for (auto& kv : players)
            {
                // A disconnected/stale player's body freezes in place (slot kept
                // alive for the reconnect grace window). No simulation, no death.
                if (kv.second.connectionStale)
                    continue;
                simulatePlayer(kv.second, world, tick);
                updateServerBroadcastInterp(kv.second, tick);
                pushPositionHistory(kv.second, tick);
                if (kv.second.justRespawned)
                {
                    kv.second.justRespawned = false;
                    completeAuthoritativeSpawn(sock, kv.second, false, tick);
                }
            }
            tickWeaponRuntimes(players, tick);
            tickHeldFireIntents(sock, players, npcs, projectiles,
                                nextProjectileId, tick, totalPacketsOut);

            resolvePlayerCollision(players);
            checkVoidDeath(players, npcs);

            // Simulate all NPCs once per tick. simulateSharedNpcs already walks
            // the entire NpcSystem; looping over npcs here would advance every
            // NPC once per NPC and make them move N× faster.
            simulateSharedNpcs(sock, players, npcs, npcSystem, npcWorld,
                               mirrorPlayer, npcIdsAlive, projectiles,
                               nextProjectileId, tick, totalPacketsOut);
            // Hot actor-movement post pass: runs after NPC AI has written the
            // generic intent, so one hot system can move players and NPCs.
            if (tickPolicy.runPostMovement)
            {
                MimitaRuntime::GenericRuntime& runtime =
                    MimitaRuntime::GenericRuntime::instance();
                void* postHost = LiveBehavior::hostContext(tick);
                LiveBehavior::setDispatchHeadlessWorld(&world);
                runtime.runDomain(GAME_DOMAIN_POST_MOVEMENT, tick, (float)SERVER_DT, postHost);
                LiveBehavior::drainEvents(64);
            }
            tickServerPhysicalContactWeapons(sock, players, world, SERVER_DT, tick, totalPacketsOut);

            tickIcePeers(serverCode, dedicatedIceState.iceSessionId,
                         pendingIceTransports);
            tickServerIceTransports(sock, players, npcs, projectiles,
                                    nextEntityId, nextProjectileId,
                                    nextPlayerId, pendingIceTransports, world,
                                    tick, totalPacketsIn, totalPacketsOut,
                                    &transportStats, &disagreementRetransmit);

            if (tickPolicy.snapshotDue)
                buildAndSendSnapshot(sock, players, npcs, tick, totalPacketsOut);
            tickDisagreementRetransmit(sock, players, disagreementRetransmit, totalPacketsOut);
            serverReplicateDynamicComponents(sock, players, tick, totalPacketsOut);
            tickReliableGameplayEvents(sock, players, totalPacketsOut);
            serverGamemodeTick(sock, players, world, npcWorld, npcs, npcSystem,
                           npcIdsAlive, tick, totalPacketsOut);
            tickServerProgression(sock, players, true, totalPacketsOut);

            accumulator -= (double)SERVER_DT;
            ++tick;
            ++steps;
            if (tickPolicy.requestShutdown)
                shutdownRequested = true;
            if (tickPolicy.catchupCap > 0 && (uint32_t)steps >= tickPolicy.catchupCap)
                break;
        }
        if (shutdownRequested)
            break;
        const bool cappedCatchup = steps >= MAX_STEPS && accumulator >= (double)SERVER_DT;

        // Poll coordinator for incoming ICE requests every outer loop
        // (rate-limited to 500ms inside tickIceCoordinator).
        tickIceCoordinator(dedicatedIceState, players.size());
        fflush(stdout);

        // ICE rooms stay alive via coordinatorIceHostPoll in tickIceCoordinator

        // Timing measurement every 600 ticks (~10 seconds)
        static uint32_t s_lastTimingTick = 0;
        static uint64_t s_timingStartMs = nowMs();
        if (tick - s_lastTimingTick >= 600)
        {
            uint64_t nowMsVal = nowMs();
            double elapsedSec = (double)(nowMsVal - s_timingStartMs) / 1000.0;
            double actualHz = (double)(tick - s_lastTimingTick) / elapsedSec;
            printf("%s [SERVER TIMING] ticks=%u elapsed=%.1fs actualHz=%.1f players=%zu projectiles=%zu\n",
                   serverTimestamp(), tick - s_lastTimingTick, elapsedSec, actualHz,
                   players.size(), projectiles.size());
            s_lastTimingTick = tick;
            s_timingStartMs = nowMsVal;
        }

        // Heartbeat every ~300 ticks (~5 seconds) to prove server is alive
        {
            static uint32_t s_lastHeartbeatTick = 0;
            if (tick - s_lastHeartbeatTick >= 300)
            {
                printf("%s [SERVER HEARTBEAT] tick=%u players=%zu — alive\n",
                       serverTimestamp(), tick, players.size());
                fflush(stdout);
                emitNetworkLog(1u, "NETWORK", "network.server_tick",
                               "server tick heartbeat", tick);
                s_lastHeartbeatTick = tick;
            }
        }

        // Status log every second
        if (nowMs() - lastLog >= 1000)
        {
            printf("%s [SERVER STATUS] tick=%u players=%zu packetsIn=%llu packetsOut=%llu "
                   "recvAttempts=%llu recvWouldBlock=%llu recvErrors=%llu malformed=%llu "
                   "protocolMismatch=%llu unknown=%llu hello=%llu join=%llu reconnect=%llu input=%llu\n",
                   serverTimestamp(), tick, players.size(),
                   (unsigned long long)totalPacketsIn, (unsigned long long)totalPacketsOut,
                   (unsigned long long)transportStats.recvAttempts,
                   (unsigned long long)transportStats.recvWouldBlock,
                   (unsigned long long)transportStats.recvErrors,
                   (unsigned long long)transportStats.malformedPackets,
                   (unsigned long long)transportStats.protocolMismatches,
                   (unsigned long long)transportStats.unknownPacketTypes,
                   (unsigned long long)transportStats.helloPackets,
                   (unsigned long long)transportStats.joinPackets,
                   (unsigned long long)transportStats.reconnectPackets,
                   (unsigned long long)transportStats.inputPackets);
            lastLog = nowMs();
        }

        // Per-second structured divergence log: the server's authoritative view
        // of every player (position/velocity/HP/ground state). Write to the
        // Network log file so a test can compare this against each client's
        // own per-second report. Gated by debuglogger.json "network" level.
        {
            static uint64_t s_lastServerDivergenceLog = 0;
            const uint64_t nowDiv = nowMs();
            if (nowDiv - s_lastServerDivergenceLog >= 1000 &&
                ::StructuredLogger::instance().shouldLog(
                    ::StructuredCategory::Network, ::StructuredLevel::Verbose))
            {
                s_lastServerDivergenceLog = nowDiv;
                std::string msg;
                char buf[256];
                snprintf(buf, sizeof(buf), "tick=%u players=%zu npcs=%zu",
                         tick, players.size(), npcs.size());
                msg += buf;
                for (const auto& kv : players)
                {
                    const ServerPlayer& p = kv.second;
                    snprintf(buf, sizeof(buf),
                        " | p%u pos=(%.1f,%.1f,%.1f) vel=(%.1f,%.1f,%.1f) "
                        "hp=%d onGround=%d yaw=%.1f stale=%d",
                        p.id, p.pos.x, p.pos.y, p.pos.z,
                        p.vel.x, p.vel.y, p.vel.z,
                        p.health, (int)p.onGround, p.yaw,
                        (int)p.connectionStale);
                    msg += buf;
                }
                ::StructuredLogger::Entry e;
                e.category = ::StructuredCategory::Network;
                e.level = ::StructuredLevel::Verbose;
                e.eventId = "server-divergence";
                e.reason = "server authoritative player state";
                e.sourceFile = __FILE__;
                e.sourceLine = __LINE__;
                e.functionName = __FUNCTION__;
                if (msg.size() > 3000) msg.resize(3000);
                e.message = msg;
                ::StructuredLogger::instance().write(e);
            }
        }

        // NOTE: no extra `accumulator += postElapsed` here. The loop-top
        // `elapsed` measurement already covers the full previous iteration
        // (body work + sleep). Adding the current iteration's body time a
        // second time made the accumulator grow faster than real time and the
        // server run at ~80Hz instead of the locked 60Hz (SERVER_DT). That
        // clock mismatch is what made clients think snapshots were missing and
        // extrapolate remote bodies ahead of their real position.
        const uint64_t loopUs = (uint64_t)std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - loopStart).count();
        recordServerLoopPerf(loopPerf, loopUs, cappedCatchup);
        const uint64_t perfNowMs = nowMs();
        if (perfNowMs - lastPerfMs >= 1000)
        {
            reportServerPerf(serverTimestamp(), loopPerf, tick, lastPerfTick,
                             perfNowMs - lastPerfMs);
            lastPerfTick = tick;
            lastPerfMs = perfNowMs;
        }

        // Sleep only when no simulation debt remains. Use microseconds for precision.
        if (accumulator < (double)SERVER_DT)
        {
            double sleepSec = (double)SERVER_DT - accumulator;
            uint64_t sleepUs = (uint64_t)(sleepSec * 1000000.0);
            if (sleepUs > 1000)
                std::this_thread::sleep_for(std::chrono::microseconds(sleepUs));
        }
    }

    // Clean requested shutdown is an explicit, classified cause: the server
    // reaches its own shutdown path and returns 0. Any other exit never
    // executes these records, so a missing shutdown.completed identifies a
    // crash/transport failure/termination.
    {
        LiveEventJournal::Fields requested;
        requested.tick = tick;
        requested.result = "requested";
        LiveEventJournal::instance().record("server.shutdown.requested", requested);
        emitNetworkLog(2u, "NETWORK", "network.shutdown_requested",
                       "server shutdown requested", tick);
    }

    PersistenceQueue::instance().flushBlocking();
    HotReloadSystem::instance().unloadGameDLL();
    if (!serverCode.empty())
    {
        printf("%s [SERVER] deregistering room %s\n", serverTimestamp(), serverCode.c_str());
        coordinatorIceDone(serverCode);
    }
    closesocket(sock);
    netShutdown();
    {
        LiveEventJournal::Fields completed;
        completed.tick = tick;
        completed.result = "clean";
        completed.extra = std::string("\"cause\":\"requested_shutdown\",\"exit_code\":0");
        LiveEventJournal::instance().record("server.shutdown.completed", completed);
        emitNetworkLog(2u, "NETWORK", "network.shutdown_completed",
                       "clean server shutdown", tick);
    }
    LiveEventJournal::instance().shutdown();
    ::StructuredLogger::instance().shutdown();
    printf("%s [SERVER] shutdown complete\n", serverTimestamp());
    return 0;
}

// ─── Listen Server ─────────────────────────────────────────────────────────

ListenServerState::~ListenServerState()
{
    // Unique_ptr members (NpcSystem/World/Player) need complete types; this
    // TU includes them, so the state can safely outlive other translation units.
}

bool startListenServer(ListenServerState& state, uint16_t port,
    const std::string& publicIp, const std::string& hostSessionId,
    const ServerLaunchSettings* settings)
{
    if (state.active)
        return false;

    if (!netStartup())
    {
        printf("[LISTEN SERVER] FATAL: WSAStartup failed\n");
        return false;
    }

    state.sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (state.sock == INVALID_SOCKET)
    {
        printf("[LISTEN SERVER] FATAL: socket() failed error=%d\n", WSAGetLastError());
        netShutdown();
        return false;
    }
    disableUdpConnReset(state.sock);

    int reuseAddr = 1;
    setsockopt(state.sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuseAddr, sizeof(reuseAddr));
    setNonBlocking(state.sock);

    const bool localOnly = settings && settings->startLocalServer;

    sockaddr_in bindAddr{};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_addr.s_addr = localOnly ? inet_addr("127.0.0.1") : htonl(INADDR_ANY);
    bindAddr.sin_port = htons(port);

    if (bind(state.sock, (sockaddr*)&bindAddr, sizeof(bindAddr)) == SOCKET_ERROR)
    {
        int err = WSAGetLastError();
        printf("[LISTEN SERVER] FATAL: bind() port=%u failed error=%d\n", port, err);
        if (err == WSAEADDRINUSE)
            printf("[LISTEN SERVER] HINT: Port %u already in use\n", port);
        closesocket(state.sock);
        netShutdown();
        return false;
    }

    if (localOnly)
        printf("[LISTEN SERVER] bound to port %u (localhost only)\n", port);
    else
        printf("[LISTEN SERVER] bound to port %u (all interfaces)\n", port);

    // Determine map path from settings
    std::string mapName = settings ? settings->mapName : "funworld3";
    std::string mapPath = settings ? settings->resolvedMapPath : "";
    if (mapPath.empty())
        mapPath = "assets/maps/" + mapName + ".glb";
    setServerMapId(mapName);
    printf("[LISTEN SERVER MAP] mapId=%s path=%s\n", mapName.c_str(), mapPath.c_str());
    if (!loadHeadlessWorld(mapPath.c_str(), state.world))
        printf("[LISTEN SERVER] WARNING: headless world load failed; using floor fallback\n");

    // Load the standard player body shape headlessly for hit validation.
    if (gServerBodyTemplate.empty())
        loadServerBodyTemplateFromGlb(nullptr, gServerBodyTemplate);

    // Real client World + NpcSystem so online NPCs run the exact local NPC AI.
    state.npcWorld = std::make_unique<World>();
    buildNpcWorldCollision(*state.npcWorld, state.world);
    state.npcSystem = std::make_unique<NpcSystem>();
    state.mirrorPlayer = std::make_unique<Player>();
    state.npcIdsAlive.clear();

    state.publicIp = publicIp;
    state.hostSessionId = hostSessionId;
    state.active = true;
    state.port = port;
    state.players.clear();
    state.npcs.clear();
    state.projectiles.clear();
    AreaEffectSystem::instance().clear();
    state.nextPlayerId = 1;
    state.nextEntityId = 1000;
    state.nextProjectileId = 1;
    state.tick = 0;
    state.lastLog = 0;
    state.totalPacketsIn = 0;
    state.totalPacketsOut = 0;
    state.startTimeMs = nowMs();
    state.accumulator = 0.0f;
    if (settings)
    {
        state.serverName = settings->serverName;
        state.mapName = settings->mapName;
        state.gameMode = settings->gameMode;
        state.maxPlayers = settings->maxPlayers;
        state.weaponSetId = settings->weaponSetId;
        state.autoMapRotation = settings->autoMapRotation;
        state.mapRotationMinutes = settings->mapRotationMinutes;
        state.discordNotification = settings->discordNotification;
        state.passwordProtected = settings->passwordProtected;
        state.password = settings->password;
        state.hostPlayerName = settings->hostPlayerName;
    }
    if (settings && !settings->duelMode)
        serverCommunityMapStart(communityMapPool(), state.mapName,
                                state.autoMapRotation,
                                state.mapRotationMinutes,
                                state.weaponSetId);
        serverCommunitySetWeaponSet(state.weaponSetId);
    if (settings && !settings->duelMode)
        serverCommunitySetMode(settings->gameMode);
    if (settings && !settings->duelMode && settings->gameMode != "sandbox")
        serverCommunityStartMatch(false);
    gServerHostPlayerName = settings ? settings->hostPlayerName : "";

    // Startup NPCs
    bool npcsEnabled = !settings || settings->startupNpcsEnabled;
    uint32_t npcCount = npcsEnabled ? (settings ? settings->startupNpcCount : 3) : 0;
    for (uint32_t i = 0; i < npcCount; ++i)
    {
        ServerNpc npc;
        npc.entityId = state.nextEntityId++;
        npc.name = "NPC " + std::to_string(i + 1);
        if (!state.world.spawnPoints.empty())
        {
            size_t idx = i % state.world.spawnPoints.size();
            npc.pos = effectiveServerSpawn(state.world.spawnPoints[idx].position);
            npc.yaw = state.world.spawnPoints[idx].yaw;
            {
                ActorSpawnPolicyV1 sp{};
                sp.kind = GAME_ACTOR_SPAWN_NPC;
                sp.index = i;
                sp.spawnPointCount = (uint32_t)state.world.spawnPoints.size();
                sp.candidateCount = state.world.spawnPoints.size() < 8
                    ? (uint32_t)state.world.spawnPoints.size() : 8u;
                for (uint32_t c = 0; c < sp.candidateCount; ++c) {
                    sp.candidatePosition[c][0] = state.world.spawnPoints[c].position.x;
                    sp.candidatePosition[c][1] = state.world.spawnPoints[c].position.y;
                    sp.candidatePosition[c][2] = state.world.spawnPoints[c].position.z;
                    sp.candidateYaw[c] = state.world.spawnPoints[c].yaw;
                }
                sp.chosenPosition[0] = npc.pos.x;
                sp.chosenPosition[1] = npc.pos.y;
                sp.chosenPosition[2] = npc.pos.z;
                sp.chosenYaw = npc.yaw;
                if (LiveBehavior::dispatchGameplayEvent64(
                        GAME_EVENT_ACTOR_SPAWN_POLICY, &sp, sizeof(sp), 0, 0, 0) &&
                    sp.handled)
                {
                    if (sp.suppress)
                        continue;
                    npc.pos = glm::vec3(sp.position[0], sp.position[1], sp.position[2]);
                    npc.yaw = sp.yaw;
                }
            }
            printf("[LISTEN SERVER NPC SPAWN] reason=startup entityId=%u npcIndex=%u "
                   "spawnpoint=%zu position=(%.2f,%.2f,%.2f)\n",
                   npc.entityId, i, idx,
                   npc.pos.x, npc.pos.y, npc.pos.z);
        }
        else
        {
            npc.pos = {4.0f + i * 2.0f, 8.0f, 30.0f};
        }
        npc.phase = i * 2.0f;
        state.npcs[npc.entityId] = npc;
    }
    printf("[LISTEN SERVER NPC STARTUP] enabled=%d requested=%u spawned=%zu\n",
           (int)npcsEnabled, npcCount, state.npcs.size());

    // Register with coordinator (skip if external server process handles it, or local-only)
    if (localOnly)
    {
        // Generate a local-only room code (LOCAL-XXXX)
        const char* chars = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
        std::string suffix;
        {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dist(0, 31);
            for (int i = 0; i < 4; ++i)
                suffix += chars[dist(gen)];
        }
        state.serverCode = "LOCAL-" + suffix;
        setServerCoordinatorState("LOCAL", "");
        printf("[LISTEN SERVER] local-only mode: code=%s\n", state.serverCode.c_str());
    }
    else if (!settings || !settings->externalProcessLaunched)
    {
        if (hostedRoomSession().active)
        {
            printf("[ROOM DUPLICATE ERROR] existingCode=%s attemptedCode=%s caller=startListenServer\n",
                   hostedRoomSession().roomCode.c_str(), state.serverCode.c_str());
        }

        if (!initServerIceListener(state))
        {
            printf("[LISTEN SERVER] ICE init failed — aborting. Check STUN/TURN connectivity.\n");
            closesocket(state.sock);
            netShutdown();
            return false;
        }
        printf("[LISTEN SERVER] ICE registered: code=%s\n", state.serverCode.c_str());
        hostedRoomSession().coordinatorRoomType = "ice";
    }

    printf("[LISTEN SERVER] started port=%u code=%s\n", port, state.serverCode.c_str());

    // Spawn background thread for genuine 60 Hz independent server timing
    PersistenceQueue::instance().beginSession(state.serverCode, AuthSystem::instance().user().sessionToken,
                                              AuthSystem::instance().user().id);
    state.serverRunning = true;
    // The listen server's background thread now owns hot-reload activation (it
    // is the safe point). Suppress the main render thread's pollAndAdvance so a
    // swap can never happen while this thread is mid-tick inside a hot call.
    HotReloadSystem::instance().setExternalTickOwner(true);
    state.serverThread = std::thread(listenServerThreadFunc, std::ref(state));

    return true;
}

void stopListenServer(ListenServerState& state)
{
    if (!state.active)
        return;

    printf("[LISTEN SERVER] stopping code=%s players=%zu uptime=%llus tick=%u\n",
           state.serverCode.c_str(), state.players.size(),
           (unsigned long long)((nowMs() - state.startTimeMs) / 1000),
           state.tick);

    // Signal background thread to stop
    state.serverRunning = false;

    if (state.serverThread.joinable())
        state.serverThread.join();

    // The tick thread has stopped; return activation ownership to the main thread.
    HotReloadSystem::instance().setExternalTickOwner(false);

    // Flush any remaining persistence events before shutdown
    {
        PersistenceQueue::instance().flushBlocking();
        const size_t depth = PersistenceQueue::instance().queueDepth();
        if (depth > 0)
            printf("[PERSISTENCE] WARNING: %zu events could not be flushed on shutdown\n", depth);
    }

    // Deregister with coordinator (skip for local-only servers)
    if (!state.serverCode.empty() && state.serverCode.find("LOCAL-") != 0)
        coordinatorIceDone(state.serverCode);

    closesocket(state.sock);
    state.sock = INVALID_SOCKET;
    state.active = false;
    netShutdown();
}

// ── One tick of the authoritative server simulation ────────────────────
// This is the shared body used by both the background listen server thread
// and extracted from the old tickListenServer accumulator loop.
static void simulateOneServerTick(ListenServerState& state)
{
    // Hot-reload barrier: activate a validated candidate at the TOP of this
    // fixed tick, on the LISTEN SERVER THREAD (the same thread that runs every
    // hot call). This is the safe point; the main render thread sets the tick
    // owner so its own pollAndAdvance becomes a no-op.
    HotReloadSystem::instance().pollAndAdvanceFromTickOwner(state.tick);

    // Hot fixed-tick orchestration policy (network.tick), shared with the
    // dedicated loop so both tick bodies use one policy path.
    const ServerTickPolicy tickPolicy = resolveServerTickPolicy(
        state.tick, 0, (uint32_t)state.players.size(),
        (uint32_t)state.projectiles.size(), false, true, true);

    // Hot-reload networkingconfig.json so hosted servers pick up changes live.
    NetworkingConfig::instance().pollReload();

    // Hot-reload NPC difficulty so hosted-server NPCs honor the JSON live.
    NpcDifficultyConfig::instance().pollReload();
    static uint64_t listenNpcDifficultyRevision = 0;
    if (listenNpcDifficultyRevision != NpcDifficultyConfig::instance().revision())
    {
        state.npcSystem->refreshDifficultyTuning();
        listenNpcDifficultyRevision = NpcDifficultyConfig::instance().revision();
    }
    CommunityServerConfig::instance().pollReload();
    SpawnVelocityConfig::instance().pollReload();
    MatchRoleRegistry::instance().pollReload();
    BehaviorProfileRegistry::instance().pollReload();

    {
        char buffer[2048];
        sockaddr_in from{};
        for (;;)
        {
            int fromLen = sizeof(from);
            int bytes = recvfrom(state.sock, buffer, sizeof(buffer), 0,
                                 (sockaddr*)&from, &fromLen);
            if (bytes <= 0)
            {
                int wsaErr = WSAGetLastError();
                if (wsaErr != WSAEWOULDBLOCK)
                {
                    sockaddr_in localEp{};
                    int localEpLen = sizeof(localEp);
                    std::string localStr = "(unknown)";
                    if (getsockname(state.sock, (sockaddr*)&localEp, &localEpLen) == 0)
                        localStr = addressToString(localEp);
                    printf("[LISTEN SERVER RX ERROR] sock=%d error=%d local=%s\n",
                           (int)state.sock, wsaErr, localStr.c_str());
                }
                break;
            }
            TransportReceiveEvent event{};
            event.connectionId = makeUdpConnectionId(from);
            event.remoteEndpoint = from;
            event.payload = reinterpret_cast<const uint8_t*>(buffer);
            event.payloadBytes = bytes;
            event.receivedAtMs = nowMs();
            event.transportKind = TransportKind::Udp;
            processServerPacket(state.sock, event, state.players, state.npcs,
                                state.projectiles, state.nextPlayerId,
                                state.nextEntityId, state.nextProjectileId,
                                state.world, state.tick, state.totalPacketsIn,
                                state.totalPacketsOut, nullptr,
                                &state.disagreementRetransmit);
        }

        // Heartbeat before simulation so a long tick cannot let the room expire.
        // Skip ICE coordinator for local-only servers.
        if (state.serverCode.find("LOCAL-") != 0)
            tickIceCoordinator(state, state.players.size());

        LiveIdentity::setSimulationTick(state.tick);

        // Generic runtime systems execute in the listen server's fixed step too,
        // matching the dedicated loop (shared tick policy path).
        if (tickPolicy.runGameplayDomain)
        {
            MimitaRuntime::GenericRuntime& runtime =
                MimitaRuntime::GenericRuntime::instance();
            void* runtimeHost = LiveBehavior::hostContext(state.tick);
            LiveBehavior::setDispatchHeadlessWorld(&state.world);
            runtime.runDomain(GAME_DOMAIN_GAMEPLAY, state.tick,
                              (float)SERVER_DT, runtimeHost);
            runtime.runActiveModeDomain(state.tick, (float)SERVER_DT, runtimeHost);
            runtime.runRegisteredDomains(state.tick, (float)SERVER_DT, runtimeHost);
            LiveBehavior::drainEvents(64);
        }

        handleClientTimeout(state.players, state.sock, state.tick, state.totalPacketsOut);
        for (auto& kv : state.players)
        {
            kv.second.shotsThisTick = 0;
            kv.second.attackPktsThisTick = 0;
        }
        // Bind the authoritative headless collision world for server-side hot
        // movement on this tick.
        LiveBehavior::setDispatchHeadlessWorld(&state.world);
        for (auto& kv : state.players)
        {
            // Disconnected players freeze in place; their slot survives the
            // reconnect grace window so a returning client is restored.
            if (kv.second.connectionStale)
                continue;
            simulatePlayer(kv.second, state.world, state.tick);
            updateServerBroadcastInterp(kv.second, state.tick);
            pushPositionHistory(kv.second, state.tick);
            if (kv.second.justRespawned)
            {
                kv.second.justRespawned = false;
                completeAuthoritativeSpawn(state.sock, kv.second, false, state.tick);
            }
        }
        tickWeaponRuntimes(state.players, state.tick);
        tickHeldFireIntents(state.sock, state.players, state.npcs, state.projectiles,
                            state.nextProjectileId, state.tick, state.totalPacketsOut);
        resolvePlayerCollision(state.players);
        checkVoidDeath(state.players, state.npcs);
        simulateSharedNpcs(state.sock, state.players, state.npcs,
                           *state.npcSystem, *state.npcWorld, *state.mirrorPlayer,
                           state.npcIdsAlive, state.projectiles, state.nextProjectileId,
                           state.tick, state.totalPacketsOut);
        // Hot actor-movement post pass (after NPC AI writes intent).
        if (tickPolicy.runPostMovement)
        {
            MimitaRuntime::GenericRuntime& runtime =
                MimitaRuntime::GenericRuntime::instance();
            void* postHost = LiveBehavior::hostContext(state.tick);
            LiveBehavior::setDispatchHeadlessWorld(&state.world);
            runtime.runDomain(GAME_DOMAIN_POST_MOVEMENT, state.tick,
                              (float)SERVER_DT, postHost);
            LiveBehavior::drainEvents(64);
        }
        tickServerPhysicalContactWeapons(state.sock, state.players,
                                         state.world, SERVER_DT, state.tick,
                                         state.totalPacketsOut);

        // Skip ICE coordinator for local-only servers
        if (state.serverCode.find("LOCAL-") != 0)
        {
            tickIceCoordinator(state, state.players.size());
            tickIcePeers(state.serverCode, state.iceSessionId, state.pendingIceTransports);
        }
        tickServerIceTransports(state.sock, state.players, state.npcs,
                                state.projectiles, state.nextEntityId,
                                state.nextProjectileId, state.nextPlayerId,
                                state.pendingIceTransports, state.world,
                                state.tick, state.totalPacketsIn,
                                state.totalPacketsOut, nullptr,
                                &state.disagreementRetransmit);

        if (tickPolicy.snapshotDue)
            buildAndSendSnapshot(state.sock, state.players, state.npcs,
                                 state.tick, state.totalPacketsOut);

        tickDisagreementRetransmit(state.sock, state.players,
                                   state.disagreementRetransmit,
                                   state.totalPacketsOut);
        serverReplicateDynamicComponents(state.sock, state.players, state.tick,
                                         state.totalPacketsOut);
        tickReliableGameplayEvents(state.sock, state.players,
                                   state.totalPacketsOut);
        serverGamemodeTick(state.sock, state.players, state.world, *state.npcWorld,
                       state.npcs, *state.npcSystem, state.npcIdsAlive,
                       state.tick, state.totalPacketsOut);

        tickServerProgression(state.sock, state.players, true, state.totalPacketsOut);

        uint64_t now = nowMs();
        if (now - state.lastLog >= 1000)
        {
            printf("[LISTEN SERVER] tick=%u players=%zu packetsIn=%llu packetsOut=%llu code=%s\n",
                   state.tick, state.players.size(),
                   (unsigned long long)state.totalPacketsIn,
                   (unsigned long long)state.totalPacketsOut,
                   state.serverCode.c_str());
            if (state.players.size() <= 10)
            {
                for (const auto& kv : state.players)
                    printf("[LISTEN SERVER] player id=%u name=\"%s\" pos=(%.1f,%.1f,%.1f)\n",
                           kv.second.id, kv.second.name.c_str(),
                           kv.second.pos.x, kv.second.pos.y, kv.second.pos.z);
            }
            state.lastLog = now;
        }

        // ICE rooms stay alive via coordinatorIceHostPoll in tickIceCoordinator

        if (tickPolicy.requestShutdown)
            state.serverRunning = false;

        ++state.tick;
    }
}

// ── Background thread: runs the listen server at a genuine 60 Hz ─────
// Uses accumulator-based timing (not sleep_until deadline) so that late
// wakes are caught up on the next iteration rather than drifting forever.
static void listenServerThreadFunc(ListenServerState& state)
{
    using namespace std::chrono;
    constexpr double kFixedDt = 1.0 / 60.0;
    constexpr auto kFixedDtNs = nanoseconds(1'000'000'000 / 60);
    constexpr int kMaxCatchup = 5;

    auto previousTime = steady_clock::now();
    double accumulator = 0.0;

    printf("[LISTEN SERVER THREAD] started with accumulator timing\n");
    emitNetworkLog(2u, "NETWORK", "network.server.thread_started",
                   "listen server thread started", 0);

    uint64_t lastLogTick = 0;
    uint64_t lastHzLog = 0;
    ServerLoopPerf loopPerf;
    uint32_t lastPerfTick = 0;
    uint64_t lastPerfMs = nowMs();

    while (state.serverRunning)
    {
        auto loopStart = steady_clock::now();
        auto currentTime = steady_clock::now();
        double elapsed = duration_cast<duration<double>>(currentTime - previousTime).count();
        previousTime = currentTime;

        // Clamp excessive elapsed time (pauses, debugger stops) to prevent spiral
        if (elapsed > 0.1) elapsed = 0.1;

        accumulator += elapsed;

        int steps = 0;
        while (accumulator >= kFixedDt && steps < kMaxCatchup)
        {
            simulateOneServerTick(state);
            accumulator -= kFixedDt;
            ++steps;
        }
        const bool cappedCatchup = steps >= kMaxCatchup && accumulator >= kFixedDt;

        // Log Hz every ~10 seconds
        if (state.tick - lastHzLog >= 600)
        {
            printf("[LISTEN SERVER THREAD] tick=%u accumulator=%.4f catchupSteps=%d\n",
                   state.tick, accumulator, steps);
            lastHzLog = state.tick;
        }

        // Flush structured logger periodically
        if (state.tick - lastLogTick >= 60)
        {
            ::StructuredLogger::instance().pollConfig();
            ::StructuredLogger::instance().tick();
            lastLogTick = state.tick;
        }

        const uint64_t loopUs = (uint64_t)duration_cast<duration<double, std::micro>>(
            steady_clock::now() - loopStart).count();
        recordServerLoopPerf(loopPerf, loopUs, cappedCatchup);
        const uint64_t perfNowMs = nowMs();
        if (perfNowMs - lastPerfMs >= 1000)
        {
            reportServerPerf("[LISTEN SERVER]", loopPerf, state.tick,
                             lastPerfTick, perfNowMs - lastPerfMs);
            lastPerfTick = state.tick;
            lastPerfMs = perfNowMs;
        }

        // Sleep only for the remaining time until the next tick is due.
        if (accumulator < kFixedDt)
        {
            double sleepSec = kFixedDt - accumulator;
            if (sleepSec > 0.001) // only sleep if more than 1ms
            {
                auto sleepUs = duration_cast<microseconds>(duration<double>(sleepSec));
                std::this_thread::sleep_for(sleepUs);
            }
        }
    }

    printf("[LISTEN SERVER THREAD] exiting\n");
    emitNetworkLog(2u, "NETWORK", "network.server.thread_exiting",
                   "listen server thread exiting", state.tick);
}

void tickListenServer(ListenServerState& state, float /*dt*/)
{
    const auto& user = AuthSystem::instance().user();
    PersistenceQueue::instance().setHostToken(user.sessionToken, user.id);
    // No-op: the background thread handles all authoritative server ticks.
    // This function exists only as compatibility for the render-loop caller.
    if (!state.serverRunning && state.active)
    {
        static uint64_t lastWarn = 0;
        uint64_t now = nowMs();
        if (now - lastWarn > 5000)
        {
            printf("[LISTEN SERVER] thread died unexpectedly at tick=%u\n", state.tick);
            lastWarn = now;
        }
    }
}

// ─── Run server with explicit settings (from GUI process launch) ──────────

int runServerWithSettings(const ServerLaunchSettings& settings)
{
    setvbuf(stdout, nullptr, _IONBF, 0);

    printf("============================================================\n");
    printf("                    MiMITA SERVER\n");
    printf("          This window is the server you started.\n");
    printf("       Keep it open while people are playing.\n");
    printf("     Closing this window will stop your server.\n");
    printf("============================================================\n");
    printf("Version: %u\n", PROTOCOL_VERSION);
    printf("PID: %lu\n", (unsigned long)GetCurrentProcessId());
    printf("Mode: Headless authoritative server\n");
    printf("Map: %s\n", settings.mapName.c_str());
    printf("Players: 0 / %u\n", settings.maxPlayers);
    printf("Started by: mimita.exe Start Server button\n");
    printf("Executable path: ...\n");
    printf("Working directory: ...\n");
    printf("============================================================\n");
    printf("[SERVER CONFIG] map=%s gamemode=%s maxPlayers=%u npcs=%d count=%u\n",
           settings.mapName.c_str(), settings.gameMode.c_str(),
           settings.maxPlayers, (int)settings.startupNpcsEnabled, settings.startupNpcCount);

    // Use server launch settings by creating a LaunchOptions equivalent
    LaunchOptions opts;
    opts.server = true;
    opts.mapName = settings.mapName;
    opts.name = settings.serverName;
    opts.hostPlayerName = settings.hostPlayerName;
    opts.maxPlayers = settings.maxPlayers;
    opts.passwordProtected = settings.passwordProtected;
    opts.password = settings.password;
    opts.npcsEnabled = settings.startupNpcsEnabled;
    opts.npcCount = settings.startupNpcCount;
    opts.bind = "0.0.0.0:" + std::to_string(settings.port);
    opts.bindExplicit = true;
    opts.duel = settings.duelMode;
    opts.gamemodeId = settings.gamemodeId;
    opts.gameMode = settings.gameMode;
    opts.weaponSetId = settings.weaponSetId;
    opts.autoMapRotation = settings.autoMapRotation;
    opts.mapRotationMinutes = settings.mapRotationMinutes;
    opts.discordNotification = settings.discordNotification;

    // Delegate to existing runServer
    return runServer(opts);
}

} // namespace MimitaNet
