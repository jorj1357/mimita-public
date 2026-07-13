#include "network/server.h"
#include "network/net_mode.h"
#include "network/multiplayer-context.h"
#include "network/coordinator-client.h"
#include "void-death/void-death.h"

#include <cstdio>
#include <chrono>
#include <filesystem>
#include <random>
#include <system_error>
#include <thread>

namespace MimitaNet {

int runServer(const LaunchOptions& options)
{
    setvbuf(stdout, nullptr, _IONBF, 0);

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
    if (!parseAddress(options.connect, bindAddr))
    {
        bindAddr.sin_family = AF_INET;
        bindAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
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
    uint32_t nextPlayerId = 1;
    uint32_t nextEntityId = 1000;
    uint32_t tick = 0;
    uint64_t lastLog = nowMs();
    uint64_t totalPacketsIn = 0;
    uint64_t totalPacketsOut = 0;

    // Startup NPCs (controlled by --npcs and --no-npcs flags)
    {
        uint32_t npcCount = options.npcsEnabled ? options.npcCount : 0;
        for (uint32_t i = 0; i < npcCount; ++i)
        {
            ServerNpc npc;
            npc.entityId = nextEntityId++;
            npc.name = "NPC " + std::to_string(i + 1);
            npc.pos = {4.0f + i * 2.0f, 8.0f, 30.0f};
            npc.phase = i * 2.0f;
            npcs[npc.entityId] = npc;
        }
        printf("%s [SERVER NPC STARTUP] enabled=%d requested=%u spawned=%zu\n",
               serverTimestamp(), (int)options.npcsEnabled, npcCount, npcs.size());
    }

    // Register dedicated server with coordinator (use actual options)
    std::string serverCode = options.serverCode.empty() ? generateServerCode() : options.serverCode;
    std::string serverJoinToken;
    {
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
            printf("%s [SERVER] coordinator registered code=%s map=%s name=%s\n",
                   serverTimestamp(), serverCode.c_str(), regMapName.c_str(), regServerName.c_str());
        }
        else if (!options.serverCode.empty())
        {
            // Pre-generated code but coordinator unreachable — still set it for display
            printf("%s [SERVER] coordinator unreachable; using pre-assigned code=%s (LAN-only)\n",
                   serverTimestamp(), serverCode.c_str());
        }
    }

    uint64_t lastCoordinatorHb = 0;

    while (true)
    {
        uint64_t frameStart = nowMs();
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
                    printf("%s [NET RX ERROR] recvfrom failed error=%d\n", serverTimestamp(), wsaErr);
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

            if (header->type == PACKET_HELLO)
                handleHello(sock, from, buffer, bytes, players, nextPlayerId, tick, totalPacketsOut);
            else if (header->type == PACKET_JOIN_REQUEST)
                handleJoinRequest(sock, from, buffer, bytes, players, nextPlayerId, tick, totalPacketsOut);
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
                handleShotRequest(sock, from, buffer, bytes, players, world, tick, totalPacketsOut);
            else if (header->type == PACKET_CHAT_MESSAGE)
                handleChatMessage(sock, buffer, bytes, players, tick, totalPacketsOut);
            else if (header->type == PACKET_PING)
                handlePing(sock, from, buffer, bytes, tick);
            else if (header->type == PACKET_NPC_DAMAGE_REQUEST)
                handleNpcDamageRequest(sock, buffer, bytes, from, players, npcs, tick, totalPacketsOut);
            else if (header->type == PACKET_SERVER_COMMAND)
                handleServerCommand(buffer, bytes, players, npcs);
        }

        // Post-tick simulation and snapshot
        handleClientTimeout(players);
        for (auto& kv : players)
            pushPositionHistory(kv.second, tick);
        for (auto& kv : players)
            simulatePlayer(kv.second, world);
        resolvePlayerCollision(players);
        checkVoidDeath(players, npcs);
        for (auto& kv : npcs)
            simulateNpc(kv.second, players);

        buildAndSendSnapshot(sock, players, npcs, tick, totalPacketsOut);

        // Heartbeat to coordinator every ~15 seconds
        {
            const uint64_t now = nowMs();
            if (!serverCode.empty() && now - lastCoordinatorHb >= 15000)
            {
                coordinatorHeartbeat(serverCode, (int)players.size());
                lastCoordinatorHb = now;
            }
        }

        // Status log every second
        if (nowMs() - lastLog >= 1000)
        {
            printf("%s [SERVER STATUS] tick=%u players=%zu packetsIn=%llu packetsOut=%llu\n",
                   serverTimestamp(), tick, players.size(),
                   (unsigned long long)totalPacketsIn, (unsigned long long)totalPacketsOut);
            for (const auto& kv : players)
                printf("%s [SERVER PLAYER] id=%u name=\"%s\" pos=(%.1f,%.1f,%.1f)\n",
                       serverTimestamp(), kv.second.id, kv.second.name.c_str(),
                       kv.second.pos.x, kv.second.pos.y, kv.second.pos.z);
            lastLog = nowMs();
        }

        ++tick;
        uint64_t elapsed = nowMs() - frameStart;
        uint64_t targetMs = (uint64_t)(1000.0f / SERVER_TICK_RATE);
        if (elapsed < targetMs)
            std::this_thread::sleep_for(std::chrono::milliseconds(targetMs - elapsed));
    }
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
    state.nextPlayerId = 1;
    state.nextEntityId = 1000;
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
        npc.pos = {4.0f + i * 2.0f, 8.0f, 30.0f};
        npc.phase = i * 2.0f;
        state.npcs[npc.entityId] = npc;
    }
    printf("[LISTEN SERVER NPC STARTUP] enabled=%d requested=%u spawned=%zu\n",
           (int)npcsEnabled, npcCount, state.npcs.size());

    // Register with coordinator (skip if external server process handles it)
    if (!settings || !settings->externalProcessLaunched)
    {
        CoordinatorRoomInfo room = coordinatorRegister(
            hostSessionId, publicIp, port, state.serverName,
            "funworld3", "sandbox", 32);

        if (!room.code.empty())
        {
            state.serverCode = room.code;
            state.joinToken = room.joinToken;
            setServerCoordinatorState(room.code, room.joinToken);
            printf("[LISTEN SERVER] coordinator registered code=%s joinToken=%s\n",
                   room.code.c_str(), room.joinToken.substr(0, 12).c_str());
        }
        else
        {
            setServerCoordinatorState("", "");
            printf("[LISTEN SERVER] coordinator unreachable — running in LAN-only mode code=%s\n",
                   state.serverCode.c_str());
        }
    }
    else
    {
        // External server process handles coordinator registration
        printf("[LISTEN SERVER] external server process running; skipping coordinator registration\n");
    }

    printf("[LISTEN SERVER] started port=%u code=%s\n", port, state.serverCode.c_str());
    return true;
}

void stopListenServer(ListenServerState& state)
{
    if (!state.active)
        return;

    printf("[LISTEN SERVER] stopping code=%s players=%zu uptime=%llus\n",
           state.serverCode.c_str(), state.players.size(),
           (unsigned long long)((nowMs() - state.startTimeMs) / 1000));

    // Deregister with coordinator
    if (!state.serverCode.empty())
        coordinatorLeave(state.serverCode);

    closesocket(state.sock);
    state.sock = INVALID_SOCKET;
    state.active = false;
    state.accumulator = 0.0f;
    netShutdown();
}

void tickListenServer(ListenServerState& state, float dt)
{
    if (!state.active || state.sock == INVALID_SOCKET)
        return;

    uint64_t frameStart = nowMs();

    state.accumulator += dt;
    if (state.accumulator > 0.05f)
        state.accumulator = 0.05f;

    while (state.accumulator >= SERVER_DT)
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
                    printf("[LISTEN SERVER RX ERROR] recvfrom failed error=%d\n", wsaErr);
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
                            state.nextPlayerId, state.tick, state.totalPacketsOut);
            else if (header->type == PACKET_JOIN_REQUEST)
                handleJoinRequest(state.sock, from, buffer, bytes, state.players,
                                  state.nextPlayerId, state.tick, state.totalPacketsOut);
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
                                  state.world, state.tick, state.totalPacketsOut);
            else if (header->type == PACKET_CHAT_MESSAGE)
                handleChatMessage(state.sock, buffer, bytes, state.players,
                                  state.tick, state.totalPacketsOut);
            else if (header->type == PACKET_PING)
                handlePing(state.sock, from, buffer, bytes, state.tick);
            else if (header->type == PACKET_NPC_DAMAGE_REQUEST)
                handleNpcDamageRequest(state.sock, buffer, bytes, from,
                                       state.players, state.npcs, state.tick,
                                       state.totalPacketsOut);
            else if (header->type == PACKET_SERVER_COMMAND)
                handleServerCommand(buffer, bytes, state.players, state.npcs);
        }

        handleClientTimeout(state.players);
        for (auto& kv : state.players)
            pushPositionHistory(kv.second, state.tick);
        for (auto& kv : state.players)
            simulatePlayer(kv.second, state.world);
        resolvePlayerCollision(state.players);
        checkVoidDeath(state.players, state.npcs);
        for (auto& kv : state.npcs)
            simulateNpc(kv.second, state.players);

        buildAndSendSnapshot(state.sock, state.players, state.npcs,
                             state.tick, state.totalPacketsOut);

        uint64_t now = nowMs();
        if (now - state.lastLog >= 1000)
        {
            printf("[LISTEN SERVER] tick=%u players=%zu packetsIn=%llu packetsOut=%llu code=%s\n",
                   state.tick, state.players.size(),
                   (unsigned long long)state.totalPacketsIn,
                   (unsigned long long)state.totalPacketsOut,
                   state.serverCode.c_str());
            for (const auto& kv : state.players)
                printf("[LISTEN SERVER] player id=%u name=\"%s\" pos=(%.1f,%.1f,%.1f)\n",
                       kv.second.id, kv.second.name.c_str(),
                       kv.second.pos.x, kv.second.pos.y, kv.second.pos.z);
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
        state.accumulator -= SERVER_DT;
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
    opts.connect = "127.0.0.1:" + std::to_string(settings.port);

    // Delegate to existing runServer
    return runServer(opts);
}

} // namespace MimitaNet
