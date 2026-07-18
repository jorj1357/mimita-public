#include "network/server.h"
#include "network/net_mode.h"
#include "network/multiplayer-context.h"
#include "network/coordinator-client.h"
#include "network/ice-transport.h"
#include "void-death/void-death.h"
#include "combat/weapon-data.h"
#include "combat/weapon-registry.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"

#include <cstdio>
#include <chrono>
#include <filesystem>
#include <random>
#include <system_error>
#include <thread>

namespace MimitaNet {

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
    printf("%s [SERVER] coordinator=%s\n", serverTimestamp(), getCoordinatorUrl().c_str());
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

    int reuseAddr = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuseAddr, sizeof(reuseAddr)) == SOCKET_ERROR)
        printf("%s [SERVER] WARNING: setsockopt SO_REUSEADDR failed error=%d (non-fatal)\n", serverTimestamp(), WSAGetLastError());

    setNonBlocking(sock);

    sockaddr_in bindAddr{};
    if (options.connectExplicit && parseAddress(options.connect, bindAddr))
    {
        // Use explicitly provided address
    }
    else
    {
        bindAddr.sin_family = AF_INET;
        bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        bindAddr.sin_port = htons(DEFAULT_PORT);
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

        printf("%s [SERVER] bound to %s\n", serverTimestamp(), addressToString(bindAddr).c_str());
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
    DisagreementRetransmitState disagreementRetransmit;

    // ── Dedicated server ICE support ──
    std::unique_ptr<IceAgent> iceListenerAgent;
    std::string iceSessionId;
    uint64_t lastIceCoordinatorPollMs = 0;
    std::vector<std::unique_ptr<IGameTransport>> pendingIceTransports;
    bool iceEnabled = options.iceEnabled;

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
    {
        if (hostedRoomSession().active)
        {
            printf("[ROOM DUPLICATE ERROR] existingCode=%s attemptedCode=%s caller=headless-server-runServer\n",
                   hostedRoomSession().roomCode.c_str(), serverCode.c_str());
        }

        if (iceEnabled)
        {
            // ── ICE mode: single registration via coordinatorIceHost ──
            printf("%s [SERVER ICE] initializing ICE listener\n", serverTimestamp());
            IceConfiguration iceCfg = loadIceConfig();
            auto iceAgent = std::make_unique<IceAgent>();
            if (iceAgent->initialize(iceCfg) && iceAgent->gatherCandidates()
                && waitForAgentState(*iceAgent, IceAgentState::GatheringComplete, 15000))
            {
                iceSessionId = "host_" + std::to_string(GetCurrentProcessId())
                    + "_" + std::to_string(nowMs());
                auto hostResult = coordinatorIceHost(iceSessionId, iceAgent->localSdp());
                if (hostResult.ok)
                {
                    serverCode = hostResult.roomCode;
                    serverJoinToken = hostResult.joinToken;
                    iceListenerAgent = std::move(iceAgent);
                    lastIceCoordinatorPollMs = nowMs();
                    setServerCoordinatorState(serverCode, serverJoinToken);
                    hostedRoomSession().active = true;
                    hostedRoomSession().roomCode = serverCode;
                    hostedRoomSession().hostToken = serverJoinToken;
                    hostedRoomSession().joinToken = serverJoinToken;
                    hostedRoomSession().serverProcessId = (uint64_t)GetCurrentProcessId();
                    hostedRoomSession().coordinatorRoomType = "ice";
                    hostedRoomSession().createdAtMs = nowMs();
                    printf("%s [SERVER ICE] registered: code=%s\n",
                           serverTimestamp(), serverCode.c_str());
                }
                else
                {
                    printf("%s [SERVER ICE] coordinatorIceHost failed; LAN only\n", serverTimestamp());
                }
            }
            else
            {
                printf("%s [SERVER ICE] agent init/gather failed; LAN only\n", serverTimestamp());
            }
        }
        else
        {
            // ── Normal (non-ICE) mode: standard coordinator registration ──
            std::string regMapName = options.mapName.empty() ? "funworld3" : options.mapName;
            std::string regServerName = options.name.empty() ? "MiMITA Server" : options.name;
            CoordinatorRoomInfo room = coordinatorRegister(
                regServerName, "", DEFAULT_PORT, regServerName,
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

    while (true)
    {
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
        int fromLen = sizeof(from);

        // Drain all pending packets
        for (;;)
        {
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
                }
                break;
            }
            ++totalPacketsIn;

            PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
            if (bytes < (int)sizeof(PacketHeader) || header->magic != PROTOCOL_MAGIC || header->version != PROTOCOL_VERSION)
            {
                printf("%s [SERVER PACKET] rejected invalid header magic=0x%08x ver=%u\n",
                       serverTimestamp(), header->magic, header->version);
                continue;
            }

            // Update lastHeardMs for ANY valid packet from a known player
            if (header->type != PACKET_HELLO)
            {
                auto it = players.find(header->playerId);
                if (it != players.end())
                    it->second.lastHeardMs = nowMs();
            }

            // 7 15 2026can we do better than this somehow
            //  yanderedev leads me to think that a bunch of if thens might 
            // be inefficient but i dont know im 2 seocnds old 
            if (header->type == PACKET_HELLO)
                handleHello(sock, from, buffer, bytes, players, nextPlayerId, tick, totalPacketsOut, &world);
            else if (header->type == PACKET_JOIN_REQUEST)
                handleJoinRequest(sock, from, buffer, bytes, players, nextPlayerId, tick, totalPacketsOut, &world);
            else if (header->type == PACKET_RECONNECT_REQUEST)
                handleReconnectRequest(sock, from, buffer, bytes, players, tick, totalPacketsOut);
            else if (header->type == PACKET_INPUT)
                handleInputPacket(buffer, bytes, players, world, nextEntityId, npcs);
            else if (header->type == PACKET_DISCONNECT)
                handleDisconnect(players, buffer);
            else if (header->type == PACKET_SPAWN_NPC_REQUEST)
                handleSpawnNpcRequest(buffer, bytes, players, nextEntityId, npcs);
            else if (header->type == PACKET_TELEPORT_REQUEST)
                handleTeleportRequest(buffer, bytes, players, world);
            else if (header->type == PACKET_EXPLODE_REQUEST)
                handleExplodeRequest(buffer, bytes, players);
            else if (header->type == PACKET_SHOT_REQUEST)
                handleShotRequest(sock, from, buffer, bytes, players, world, tick, totalPacketsOut, &disagreementRetransmit);
            else if (header->type == PACKET_PELLET_BLAST_REQUEST)
                handlePelletBlastRequest(sock, from, buffer, bytes, players, world, tick, totalPacketsOut, &disagreementRetransmit);
            else if (header->type == PACKET_PROJECTILE_FIRE_REQUEST)
                handleProjectileFireRequest(sock, from, buffer, bytes, players, projectiles,
                                            nextProjectileId, world, tick, totalPacketsOut);
            else if (header->type == PACKET_ATTACK_REQUEST)
                handleAttackRequest(sock, from, buffer, bytes, players, projectiles,
                                    nextProjectileId, world, tick, totalPacketsOut);
            else if (header->type == PACKET_MELEE_HIT_REQUEST)
                handleMeleeHitRequest(sock, from, buffer, bytes, players, tick, totalPacketsOut);
            else if (header->type == PACKET_CHAT_MESSAGE)
                handleChatMessage(sock, buffer, bytes, players, tick, totalPacketsOut);
            else if (header->type == PACKET_PING)
                handlePing(sock, from, buffer, bytes, tick);
            else if (header->type == PACKET_RELOAD_REQUEST)
                handleReloadRequest(sock, from, buffer, bytes, players,
                                    tick, totalPacketsOut);
            else if (header->type == PACKET_GODBALL_STATE)
                handleGodballState(sock, players, buffer, bytes);
            else if (header->type == PACKET_NPC_DAMAGE_REQUEST)
                handleNpcDamageRequest(sock, buffer, bytes, from, players, npcs, tick, totalPacketsOut);
            else if (header->type == PACKET_SERVER_COMMAND)
                handleServerCommand(buffer, bytes, players, npcs);
            else if (header->type == PACKET_CLIENT_MAP_READY && bytes >= (int)sizeof(ClientMapReadyPacket))
            {
                const ClientMapReadyPacket* ready = reinterpret_cast<const ClientMapReadyPacket*>(buffer);
                if (ready->header.playerId != ready->assignedPlayerId)
                {
                    printf("%s [SERVER MAP READY REJECT] reason=assignedPlayerId-mismatch "
                           "headerPlayerId=%u assignedPlayerId=%u\n",
                           serverTimestamp(), ready->header.playerId, ready->assignedPlayerId);
                    continue;
                }
                auto it = players.find(ready->assignedPlayerId);
                if (it == players.end())
                {
                    printf("%s [SERVER MAP READY REJECT] reason=player-not-found id=%u\n",
                           serverTimestamp(), ready->assignedPlayerId);
                    continue;
                }
                std::string readyMap = normalizeMapId(ready->mapId);
                std::string serverMap = normalizeMapId(getServerMapId());
                if (readyMap != serverMap)
                {
                    printf("%s [SERVER MAP READY REJECT] reason=map-mismatch id=%u "
                           "readyMap=%s serverMap=%s\n",
                           serverTimestamp(), ready->assignedPlayerId,
                           readyMap.c_str(), serverMap.c_str());
                    continue;
                }
                if (!it->second.spawned)
                {
                    it->second.spawned = true;
                    it->second.vel = glm::vec3(0.0f);
                    it->second.clientStateUpdated = false;
                    printf("%s [SERVER MAP READY] id=%u name=\"%s\" mapId=%s spawned=1\n",
                           serverTimestamp(), it->second.id, it->second.name.c_str(),
                           ready->mapId);
                }
            }
        }

        ::StructuredLogger::instance().tick();

        // Accumulator-based timing: run simulation ticks for accumulated debt
        int steps = 0;
        while (accumulator >= (double)SERVER_DT && steps < MAX_STEPS)
        {
            struct {
                uint64_t simUs = 0, snapshotUs = 0, iceUs = 0, collisionUs = 0;
            } tickProfile;

            auto t0 = std::chrono::steady_clock::now();
            handleClientTimeout(players);
            for (auto& kv : players)
                pushPositionHistory(kv.second, tick);
            for (auto& kv : players)
                simulatePlayer(kv.second, world);
            tickWeaponRuntimes(players, tick);

            auto tCollision = std::chrono::steady_clock::now();
            resolvePlayerCollision(players);
            checkVoidDeath(players, npcs);
            tickProfile.collisionUs = (uint64_t)std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() - tCollision).count();

            for (auto& kv : npcs)
                simulateNpc(kv.second, players);
            tickServerProjectiles(sock, players, projectiles, world, SERVER_DT, tick, totalPacketsOut);
            tickServerSwordCombat(sock, players, world, SERVER_DT, tick, totalPacketsOut);
            tickProfile.simUs = (uint64_t)std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() - t0).count();

            auto t1 = std::chrono::steady_clock::now();
            if (iceEnabled && iceListenerAgent && tick % 30 == 0)
            {
                uint64_t nowIce = nowMs();
                if (nowIce - lastIceCoordinatorPollMs > 500) { /* poll logic */ }
            }
            tickIcePeers(serverCode, iceSessionId, pendingIceTransports);
            tickServerIceTransports(sock, players, npcs, projectiles,
                                    nextEntityId, nextPlayerId,
                                    pendingIceTransports, world, tick, totalPacketsOut);
            tickProfile.iceUs = (uint64_t)std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() - t1).count();

            auto t2 = std::chrono::steady_clock::now();
            buildAndSendSnapshot(sock, players, npcs, tick, totalPacketsOut);
            tickDisagreementRetransmit(sock, players, disagreementRetransmit, totalPacketsOut);
            tickProfile.snapshotUs = (uint64_t)std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() - t2).count();

            // Profile logging — every 600 ticks
            static uint64_t s_totalSimUs = 0, s_totalSnapshotUs = 0, s_totalIceUs = 0, s_totalCollisionUs = 0;
            static uint32_t s_profileTicks = 0;
            s_totalSimUs += tickProfile.simUs;
            s_totalSnapshotUs += tickProfile.snapshotUs;
            s_totalIceUs += tickProfile.iceUs;
            s_totalCollisionUs += tickProfile.collisionUs;
            s_profileTicks++;
            if (s_profileTicks >= 600)
            {
                Debug::log(Debug::Category::General,
                    "[TICK PROFILE] simAvg=%.2f collisionAvg=%.2f snapshotAvg=%.2f iceAvg=%.2f players=%zu projectiles=%zu\n",
                    (double)s_totalSimUs / s_profileTicks / 1000.0,
                    (double)s_totalCollisionUs / s_profileTicks / 1000.0,
                    (double)s_totalSnapshotUs / s_profileTicks / 1000.0,
                    (double)s_totalIceUs / s_profileTicks / 1000.0,
                    players.size(), projectiles.size());
                s_totalSimUs = s_totalSnapshotUs = s_totalIceUs = s_totalCollisionUs = 0;
                s_profileTicks = 0;
            }

            accumulator -= (double)SERVER_DT;
            ++tick;
            ++steps;
        }

        // Heartbeat to coordinator every ~15 seconds
        {
            const uint64_t now = nowMs();
            if (!serverCode.empty() && now - lastCoordinatorHb >= 15000)
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
            printf("%s [SERVER STATUS] tick=%u players=%zu packetsIn=%llu packetsOut=%llu\n",
                   serverTimestamp(), tick, players.size(),
                   (unsigned long long)totalPacketsIn, (unsigned long long)totalPacketsOut);
            lastLog = nowMs();
        }

        // Remeasure elapsed time for post-simulation work (heartbeat, logs)
        auto postSimNow = std::chrono::steady_clock::now();
        double postElapsed = std::chrono::duration<double>(postSimNow - currentTime).count();
        currentTime = postSimNow;
        if (postElapsed > 0.001) // only add if more than negligible
            accumulator += postElapsed;

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

        if (settings && settings->iceEnabled)
        {
            // ── ICE mode: single registration via coordinatorIceHost ──
            if (initServerIceListener(state))
            {
                printf("[LISTEN SERVER] ICE registered: code=%s\n", state.serverCode.c_str());
                hostedRoomSession().coordinatorRoomType = "ice";
            }
            else
            {
                printf("[LISTEN SERVER] ICE init failed; LAN only\n");
            }
        }
        else
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
        int fromLen = sizeof(from);

        for (;;)
        {
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
            ++state.totalPacketsIn;

            PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
            if (bytes < (int)sizeof(PacketHeader) ||
                header->magic != PROTOCOL_MAGIC ||
                header->version != PROTOCOL_VERSION)
            {
                printf("[LISTEN SERVER] rejected invalid header magic=0x%08x ver=%u\n",
                       header->magic, header->version);
                continue;
            }

            if (header->type != PACKET_HELLO)
            {
                auto it = state.players.find(header->playerId);
                if (it != state.players.end())
                    it->second.lastHeardMs = nowMs();
            }

            if (header->type == PACKET_HELLO)
                handleHello(state.sock, from, buffer, bytes, state.players,
                            state.nextPlayerId, state.tick, state.totalPacketsOut, &state.world);
            else if (header->type == PACKET_JOIN_REQUEST)
                handleJoinRequest(state.sock, from, buffer, bytes, state.players,
                                  state.nextPlayerId, state.tick, state.totalPacketsOut, &state.world);
            else if (header->type == PACKET_RECONNECT_REQUEST)
                handleReconnectRequest(state.sock, from, buffer, bytes, state.players,
                                       state.tick, state.totalPacketsOut);
            else if (header->type == PACKET_INPUT)
                handleInputPacket(buffer, bytes, state.players, state.world,
                                  state.nextEntityId, state.npcs);
            else if (header->type == PACKET_DISCONNECT)
                handleDisconnect(state.players, buffer);
            else if (header->type == PACKET_SPAWN_NPC_REQUEST)
                handleSpawnNpcRequest(buffer, bytes, state.players,
                                      state.nextEntityId, state.npcs);
            else if (header->type == PACKET_TELEPORT_REQUEST)
                handleTeleportRequest(buffer, bytes, state.players, state.world);
            else if (header->type == PACKET_EXPLODE_REQUEST)
                handleExplodeRequest(buffer, bytes, state.players);
            else if (header->type == PACKET_SHOT_REQUEST)
                handleShotRequest(state.sock, from, buffer, bytes, state.players,
                                  state.world, state.tick, state.totalPacketsOut,
                                  &state.disagreementRetransmit);
            else if (header->type == PACKET_PELLET_BLAST_REQUEST)
                handlePelletBlastRequest(state.sock, from, buffer, bytes, state.players,
                                         state.world, state.tick, state.totalPacketsOut,
                                         &state.disagreementRetransmit);
            else if (header->type == PACKET_PROJECTILE_FIRE_REQUEST)
                handleProjectileFireRequest(state.sock, from, buffer, bytes, state.players,
                                            state.projectiles, state.nextProjectileId,
                                            state.world, state.tick, state.totalPacketsOut);
            else if (header->type == PACKET_ATTACK_REQUEST)
                handleAttackRequest(state.sock, from, buffer, bytes, state.players,
                                    state.projectiles, state.nextProjectileId,
                                    state.world, state.tick, state.totalPacketsOut);
            else if (header->type == PACKET_MELEE_HIT_REQUEST)
                handleMeleeHitRequest(state.sock, from, buffer, bytes, state.players,
                                      state.tick, state.totalPacketsOut);
            else if (header->type == PACKET_CHAT_MESSAGE)
                handleChatMessage(state.sock, buffer, bytes, state.players,
                                  state.tick, state.totalPacketsOut);
            else if (header->type == PACKET_PING)
                handlePing(state.sock, from, buffer, bytes, state.tick);
            else if (header->type == PACKET_RELOAD_REQUEST)
                handleReloadRequest(state.sock, from, buffer, bytes, state.players,
                                    state.tick, state.totalPacketsOut);
            else if (header->type == PACKET_GODBALL_STATE)
                handleGodballState(state.sock, state.players, buffer, bytes);
            else if (header->type == PACKET_NPC_DAMAGE_REQUEST)
                handleNpcDamageRequest(state.sock, buffer, bytes, from,
                                       state.players, state.npcs, state.tick,
                                       state.totalPacketsOut);
            else if (header->type == PACKET_SERVER_COMMAND)
                handleServerCommand(buffer, bytes, state.players, state.npcs);
            else if (header->type == PACKET_CLIENT_MAP_READY && bytes >= (int)sizeof(ClientMapReadyPacket))
            {
                const ClientMapReadyPacket* ready = reinterpret_cast<const ClientMapReadyPacket*>(buffer);
                if (ready->header.playerId != ready->assignedPlayerId)
                {
                    printf("%s [SERVER MAP READY REJECT] reason=assignedPlayerId-mismatch "
                           "headerPlayerId=%u assignedPlayerId=%u\n",
                           serverTimestamp(), ready->header.playerId, ready->assignedPlayerId);
                    continue;
                }
                auto it = state.players.find(ready->assignedPlayerId);
                if (it == state.players.end())
                {
                    printf("%s [SERVER MAP READY REJECT] reason=player-not-found id=%u\n",
                           serverTimestamp(), ready->assignedPlayerId);
                    continue;
                }
                std::string readyMap = normalizeMapId(ready->mapId);
                std::string serverMap = normalizeMapId(getServerMapId());
                if (readyMap != serverMap)
                {
                    printf("%s [SERVER MAP READY REJECT] reason=map-mismatch id=%u "
                           "readyMap=%s serverMap=%s\n",
                           serverTimestamp(), ready->assignedPlayerId,
                           readyMap.c_str(), serverMap.c_str());
                    continue;
                }
                if (!it->second.spawned)
                {
                    it->second.spawned = true;
                    it->second.vel = glm::vec3(0.0f);
                    it->second.clientStateUpdated = false;
                    printf("%s [SERVER MAP READY] id=%u name=\"%s\" mapId=%s spawned=1\n",
                           serverTimestamp(), it->second.id, it->second.name.c_str(),
                           ready->mapId);
                }
            }
        }

        handleClientTimeout(state.players);
        for (auto& kv : state.players)
            pushPositionHistory(kv.second, state.tick);
        for (auto& kv : state.players)
            simulatePlayer(kv.second, state.world);
        tickWeaponRuntimes(state.players, state.tick);
        resolvePlayerCollision(state.players);
        checkVoidDeath(state.players, state.npcs);
        for (auto& kv : state.npcs)
            simulateNpc(kv.second, state.players);
        tickServerProjectiles(state.sock, state.players, state.projectiles,
                              state.world, SERVER_DT, state.tick,
                              state.totalPacketsOut);

        tickServerSwordCombat(state.sock, state.players,
                              state.world, SERVER_DT, state.tick,
                              state.totalPacketsOut);

        tickIceCoordinator(state);
        tickIcePeers(state.serverCode, state.iceSessionId, state.pendingIceTransports);
        tickServerIceTransports(state.sock, state.players, state.npcs,
                                state.projectiles, state.nextEntityId,
                                state.nextPlayerId, state.pendingIceTransports,
                                state.world, state.tick, state.totalPacketsOut);

        buildAndSendSnapshot(state.sock, state.players, state.npcs,
                             state.tick, state.totalPacketsOut);

        tickDisagreementRetransmit(state.sock, state.players,
                                   state.disagreementRetransmit,
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

    while (state.serverRunning)
    {
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
    opts.connect = "127.0.0.1:" + std::to_string(settings.port);

    // Delegate to existing runServer
    return runServer(opts);
}

} // namespace MimitaNet
