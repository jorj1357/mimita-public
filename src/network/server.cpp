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
#include "network/net_mode.h"
#include "network/multiplayer-context.h"
#include "network/coordinator-client.h"
#include "network/network-weapons.h"
#include "network/ice-transport.h"
#include "void-death/void-death.h"
#include "combat/weapon-data.h"
#include "combat/weapon-registry.h"
#include "debug/debug-log.h"
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
    return type >= PACKET_HELLO && type <= PACKET_DAMAGE_CONFIRMED_EVENT;
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

} // namespace

// Forward declaration for background listen server thread
static void listenServerThreadFunc(ListenServerState& state);

int runServer(const LaunchOptions& options)
{
    setvbuf(stdout, nullptr, _IONBF, 0);

    ::StructuredLogger::instance().init();

    WeaponData::registerBuiltinWeapons();
    printf("%s [SERVER] registered built-in weapons\n", serverTimestamp());

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
           options.noCoordinator ? "(disabled)" : getCoordinatorUrl().c_str());
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
           "requested=%s actual=%s family=AF_INET reuse=1 nonblocking=%d noCoordinator=%d\n",
           serverTimestamp(), PROTOCOL_VERSION, (unsigned long)GetCurrentProcessId(),
           processPath().c_str(), currentDirectory().c_str(),
           requestedBind.c_str(), addressToString(actualBindAddr).c_str(),
           (int)nonBlockingOk, (int)options.noCoordinator);
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

    // ── Dedicated server ICE support ──
    bool iceEnabled = options.iceEnabled;
    ListenServerState dedicatedIceState;
    dedicatedIceState.iceEnabled = iceEnabled;
    std::vector<PendingServerTransport> pendingIceTransports;

    // Startup NPCs (controlled by --npcs and --no-npcs flags)
    {
        uint32_t npcCount = options.npcsEnabled ? options.npcCount : 0;
        for (uint32_t i = 0; i < npcCount; ++i)
        {
            ServerNpc npc;
            npc.entityId = nextEntityId++;
            npc.name = "NPC " + std::to_string(i + 1);
            if (!world.spawnPoints.empty())
            {
                size_t idx = i % world.spawnPoints.size();
                npc.pos = world.spawnPoints[idx].position;
                npc.yaw = world.spawnPoints[idx].yaw;
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

    // ── Register server with coordinator ──
    // When ICE is enabled, use coordinatorIceHost which handles NAT traversal.
    // Otherwise use the normal coordinatorRegister for LAN/direct-IP servers.
    std::string serverCode = options.serverCode.empty() ? generateServerCode() : options.serverCode;
    std::string serverJoinToken;
    bool iceRegistrationSucceeded = false;
    {
        if (options.noCoordinator)
        {
            printf("%s [SERVER] coordinator disabled by --no-coordinator; LAN-only code=%s\n",
                   serverTimestamp(), serverCode.c_str());
        }
        else if (hostedRoomSession().active)
        {
            printf("[ROOM DUPLICATE ERROR] existingCode=%s attemptedCode=%s caller=headless-server-runServer\n",
                   hostedRoomSession().roomCode.c_str(), serverCode.c_str());
        }

        if (!options.noCoordinator && iceEnabled)
        {
            if (initServerIceListener(dedicatedIceState))
            {
                serverCode = dedicatedIceState.serverCode;
                serverJoinToken = dedicatedIceState.joinToken;
                iceRegistrationSucceeded = true;
                printf("%s [SERVER ICE] registered: code=%s\n",
                       serverTimestamp(), serverCode.c_str());
            }
            else
            {
                printf("%s [SERVER ICE] init failed; falling back to normal coordinator registration\n",
                       serverTimestamp());
            }
        }
        if (!options.noCoordinator && !iceRegistrationSucceeded)
        {
            // ── Normal (non-ICE) mode: standard coordinator registration ──
            std::string regMapName = options.mapName.empty() ? "funworld3" : options.mapName;
            std::string regServerName = options.name.empty() ? "MiMITA Server" : options.name;
            CoordinatorRoomInfo room = coordinatorRegister(
                regServerName, "", actualPort, regServerName,
                regMapName, "sandbox", MAX_PLAYERS);
            if (!room.code.empty())
            {
                serverCode = room.code;
                serverJoinToken = room.joinToken;
                setServerCoordinatorState(room.code, room.joinToken);
                hostedRoomSession().active = true;
                hostedRoomSession().roomCode = room.code;
                hostedRoomSession().hostToken = room.joinToken;
                hostedRoomSession().joinToken = room.joinToken;
                hostedRoomSession().serverProcessId = (uint64_t)GetCurrentProcessId();
                hostedRoomSession().coordinatorRoomType = "normal";
                hostedRoomSession().createdAtMs = nowMs();
                printf("%s [SERVER] coordinator registered code=%s map=%s\n",
                       serverTimestamp(), serverCode.c_str(), regMapName.c_str());
                printf("[ROOM OWNER] subsystem=headless-server code=%s\n", serverCode.c_str());
            }
            else if (!options.serverCode.empty())
            {
                printf("%s [SERVER] coordinator unreachable; using pre-assigned code=%s (LAN-only)\n",
                       serverTimestamp(), serverCode.c_str());
            }
        }
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

    // Accumulator-based fixed-step timing
    auto previousTime = std::chrono::steady_clock::now();
    double accumulator = 0.0;
    constexpr int MAX_STEPS = 5;
    ServerLoopPerf loopPerf;
    uint32_t lastPerfTick = 0;
    uint64_t lastPerfMs = nowMs();

    while (true)
    {
        auto loopStart = std::chrono::steady_clock::now();
        // Auto-exit when --timeout is set (for CI/agent testing)
        if (options.timeoutSecs > 0 && nowMs() - serverStartMs > (uint64_t)options.timeoutSecs * 1000)
        {
            printf("%s [SERVER] timeout=%us reached, exiting\n", serverTimestamp(), options.timeoutSecs);
            break;
        }

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

        ::StructuredLogger::instance().tick();

        // Accumulator-based timing: run simulation ticks for accumulated debt
        int steps = 0;
        while (accumulator >= (double)SERVER_DT && steps < MAX_STEPS)
        {
            handleClientTimeout(players);
            for (auto& kv : players)
                pushPositionHistory(kv.second, tick);

            for (auto& kv : players)
            {
                simulatePlayer(kv.second, world);
                if (kv.second.justRespawned)
                {
                    kv.second.justRespawned = false;
                    completeAuthoritativeSpawn(sock, kv.second, false);
                }
                // Retry spawn sync for players awaiting ACK (every 6 ticks ≈ 100ms)
                if (kv.second.spawnState == ServerPlayer::AwaitingSpawnAck && (tick % 6 == 0))
                    retrySpawnSync(sock, kv.second);
            }
            tickWeaponRuntimes(players, tick);

            resolvePlayerCollision(players);
            checkVoidDeath(players, npcs);

            for (auto& kv : npcs)
                simulateNpc(kv.second, players);
            tickServerProjectiles(sock, players, projectiles, world, SERVER_DT, tick, totalPacketsOut);
            tickServerPhysicalContactWeapons(sock, players, world, SERVER_DT, tick, totalPacketsOut);

            if (iceEnabled)
                tickIceCoordinator(dedicatedIceState);
            tickIcePeers(serverCode, dedicatedIceState.iceSessionId,
                         pendingIceTransports);
            tickServerIceTransports(sock, players, npcs, projectiles,
                                    nextEntityId, nextProjectileId,
                                    nextPlayerId, pendingIceTransports, world,
                                    tick, totalPacketsIn, totalPacketsOut,
                                    &transportStats, &disagreementRetransmit);

            buildAndSendSnapshot(sock, players, npcs, tick, totalPacketsOut);
            tickDisagreementRetransmit(sock, players, disagreementRetransmit, totalPacketsOut);
            tickReliableGameplayEvents(sock, players, totalPacketsOut);

            accumulator -= (double)SERVER_DT;
            ++tick;
            ++steps;
        }
        const bool cappedCatchup = steps >= MAX_STEPS && accumulator >= (double)SERVER_DT;

        // Heartbeat to coordinator every ~15 seconds
        {
            const uint64_t now = nowMs();
            if (!options.noCoordinator && !serverCode.empty() && now - lastCoordinatorHb >= 15000)
            {
                coordinatorHeartbeat(serverCode, (int)players.size());
                lastCoordinatorHb = now;
            }
        }

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

        // Remeasure elapsed time for post-simulation work (heartbeat, logs)
        auto postSimNow = std::chrono::steady_clock::now();
        double postElapsed = std::chrono::duration<double>(postSimNow - currentTime).count();
        currentTime = postSimNow;
        if (postElapsed > 0.001) // only add if more than negligible
            accumulator += postElapsed;

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

    ::StructuredLogger::instance().shutdown();
    closesocket(sock);
    netShutdown();
    printf("%s [SERVER] shutdown complete\n", serverTimestamp());
    return 0;
}

// ─── Listen Server ─────────────────────────────────────────────────────────

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

    sockaddr_in bindAddr{};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);
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

    state.serverCode = settings && !settings->serverCode.empty() ? settings->serverCode : generateServerCode();
    state.publicIp = publicIp;
    state.hostSessionId = hostSessionId;
    state.active = true;
    state.port = port;
    state.players.clear();
    state.npcs.clear();
    state.projectiles.clear();
    state.nextPlayerId = 1;
    state.nextEntityId = 1000;
    state.nextProjectileId = 1;
    state.tick = 0;
    state.lastLog = 0;
    state.totalPacketsIn = 0;
    state.totalPacketsOut = 0;
    state.startTimeMs = nowMs();
    state.accumulator = 0.0f;
    state.lastHeartbeatMs = 0;

    if (settings)
        state.serverName = settings->serverName;

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
            npc.pos = state.world.spawnPoints[idx].position;
            npc.yaw = state.world.spawnPoints[idx].yaw;
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

    // Register with coordinator (skip if external server process handles it)
    if (!settings || !settings->externalProcessLaunched)
    {
        if (hostedRoomSession().active)
        {
            printf("[ROOM DUPLICATE ERROR] existingCode=%s attemptedCode=%s caller=startListenServer\n",
                   hostedRoomSession().roomCode.c_str(), state.serverCode.c_str());
        }

        bool iceRegistrationSucceeded = false;
        if (settings && settings->iceEnabled)
        {
            // ── ICE mode: single registration via coordinatorIceHost ──
            if (initServerIceListener(state))
            {
                printf("[LISTEN SERVER] ICE registered: code=%s\n", state.serverCode.c_str());
                hostedRoomSession().coordinatorRoomType = "ice";
                iceRegistrationSucceeded = true;
            }
            else
            {
                printf("[LISTEN SERVER] ICE init failed; falling back to normal coordinator registration\n");
            }
        }
        if (!iceRegistrationSucceeded)
        {
            // ── Normal (non-ICE) mode: standard coordinator registration ──
            std::string regMap = settings ? settings->mapName : "funworld3";
            CoordinatorRoomInfo room = coordinatorRegister(
                hostSessionId, publicIp, port, state.serverName,
                regMap, "sandbox", 32);

            if (!room.code.empty())
            {
                state.serverCode = room.code;
                state.joinToken = room.joinToken;
                setServerCoordinatorState(room.code, room.joinToken);
                hostedRoomSession().active = true;
                hostedRoomSession().roomCode = room.code;
                hostedRoomSession().hostToken = room.joinToken;
                hostedRoomSession().joinToken = room.joinToken;
                hostedRoomSession().serverProcessId = (uint64_t)GetCurrentProcessId();
                hostedRoomSession().coordinatorRoomType = "normal";
                hostedRoomSession().createdAtMs = nowMs();
                printf("[LISTEN SERVER] coordinator registered code=%s\n", room.code.c_str());
                printf("[ROOM OWNER] subsystem=listen-server code=%s\n", room.code.c_str());
            }
            else
            {
                setServerCoordinatorState("", "");
                printf("[LISTEN SERVER] coordinator unreachable; LAN-only mode code=%s\n",
                       state.serverCode.c_str());
            }
        }
    }
    else
    {
        printf("[LISTEN SERVER] external server process handles coordinator registration\n");
    }

    printf("[LISTEN SERVER] started port=%u code=%s\n", port, state.serverCode.c_str());

    // Spawn background thread for genuine 60 Hz independent server timing
    state.serverRunning = true;
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

    // Deregister with coordinator
    if (!state.serverCode.empty())
        coordinatorLeave(state.serverCode);

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

        handleClientTimeout(state.players);
        for (auto& kv : state.players)
            pushPositionHistory(kv.second, state.tick);
        for (auto& kv : state.players)
        {
            simulatePlayer(kv.second, state.world);
            if (kv.second.justRespawned)
            {
                kv.second.justRespawned = false;
                completeAuthoritativeSpawn(state.sock, kv.second, false);
            }
            if (kv.second.spawnState == ServerPlayer::AwaitingSpawnAck && (state.tick % 6 == 0))
                retrySpawnSync(state.sock, kv.second);
        }
        tickWeaponRuntimes(state.players, state.tick);
        resolvePlayerCollision(state.players);
        checkVoidDeath(state.players, state.npcs);
        for (auto& kv : state.npcs)
            simulateNpc(kv.second, state.players);
        tickServerProjectiles(state.sock, state.players, state.projectiles,
                              state.world, SERVER_DT, state.tick,
                              state.totalPacketsOut);

        tickServerPhysicalContactWeapons(state.sock, state.players,
                                         state.world, SERVER_DT, state.tick,
                                         state.totalPacketsOut);

        tickIceCoordinator(state);
        tickIcePeers(state.serverCode, state.iceSessionId, state.pendingIceTransports);
        tickServerIceTransports(state.sock, state.players, state.npcs,
                                state.projectiles, state.nextEntityId,
                                state.nextProjectileId, state.nextPlayerId,
                                state.pendingIceTransports, state.world,
                                state.tick, state.totalPacketsIn,
                                state.totalPacketsOut, nullptr,
                                &state.disagreementRetransmit);

        buildAndSendSnapshot(state.sock, state.players, state.npcs,
                             state.tick, state.totalPacketsOut);

        tickDisagreementRetransmit(state.sock, state.players,
                                   state.disagreementRetransmit,
                                   state.totalPacketsOut);
        tickReliableGameplayEvents(state.sock, state.players,
                                   state.totalPacketsOut);

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

        // Heartbeat to coordinator every ~15 seconds
        if (!state.serverCode.empty() && !state.joinToken.empty() &&
            now - state.lastHeartbeatMs >= 15000)
        {
            coordinatorHeartbeat(state.serverCode, (int)state.players.size());
            state.lastHeartbeatMs = now;
        }

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
}

void tickListenServer(ListenServerState& state, float /*dt*/)
{
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

std::string generateServerCode()
{
    static const char chars[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    static std::mt19937 rng((unsigned int)std::chrono::steady_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<int> dist(0, 29);
    std::string code;
    for (int i = 0; i < 7; ++i)
        code += chars[dist(rng)];
    return code;
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
    opts.npcsEnabled = settings.startupNpcsEnabled;
    opts.npcCount = settings.startupNpcCount;
    opts.iceEnabled = settings.iceEnabled;
    opts.bind = "0.0.0.0:" + std::to_string(settings.port);
    opts.bindExplicit = true;

    // Delegate to existing runServer
    return runServer(opts);
}

} // namespace MimitaNet
