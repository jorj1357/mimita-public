#include "network/server.h"
#include "network/game-transport.h"
#include "network/ice-transport.h"
#include "network/ice/ice-config.h"
#include "network/coordinator-client.h"

#include <algorithm>
#include <cstdio>

namespace MimitaNet {

// ── ICE agent state polling helper ───────────────────────────────────

bool waitForAgentState(IceAgent& agent, IceAgentState target,
                        int timeoutMs)
{
    int waited = 0;
    while (waited < timeoutMs)
    {
        agent.tick();
        std::vector<IceEvent> evs;
        agent.pollEvents(evs);
        auto s = agent.state();
        if (s == target || s == IceAgentState::Completed ||
            s == IceAgentState::Connected)
            return true;
        if (s == IceAgentState::Failed)
            return false;
        Sleep(50);
        waited += 50;
    }
    return false;
}

// ── Initialize server-side ICE listener ──────────────────────────────
// Creates an ICE agent, registers with coordinator via ice/host endpoint,
// and stores the agent in state.iceListenerAgent.
// Returns the room code for display.

bool initServerIceListener(ListenServerState& state)
{
    printf("[SERVER ICE] initializing ICE listener\n");

    IceConfiguration iceConfig = loadIceConfig();
    if (iceConfig.turn.password.empty())
    {
        printf("[SERVER ICE] WARNING: no TURN password; direct connections only\n");
    }

    auto agent = std::make_unique<IceAgent>();
    if (!agent->initialize(iceConfig))
    {
        printf("[SERVER ICE] FATAL: agent init failed\n");
        return false;
    }
    if (!agent->gatherCandidates())
    {
        printf("[SERVER ICE] FATAL: gather failed\n");
        return false;
    }
    if (!waitForAgentState(*agent, IceAgentState::GatheringComplete, 15000))
    {
        printf("[SERVER ICE] FATAL: gather timeout\n");
        return false;
    }

    state.iceSessionId = "host_" + std::to_string(GetCurrentProcessId())
        + "_" + std::to_string(nowMs());

    auto hostResult = coordinatorIceHost(state.iceSessionId, agent->localSdp());
    if (!hostResult.ok)
    {
        printf("[SERVER ICE] FATAL: coordinatorIceHost failed\n");
        return false;
    }

    state.serverCode = hostResult.roomCode;
    state.joinToken = hostResult.joinToken;
    state.iceListenerAgent = std::move(agent);
    state.iceEnabled = true;
    state.lastIceCoordinatorPollMs = nowMs();

    printf("[SERVER ICE] listener registered: code=%s session=%s\n",
           hostResult.roomCode.c_str(), state.iceSessionId.c_str());
    return true;
}

// ── Poll coordinator for new ICE clients ─────────────────────────────
// Called every ~500ms from the server tick. When a new client is found
// (via coordinatorIcePoll returning "client_ready"), creates a new
// IceAgent for that client and adds it to pendingIceTransports.

void tickIceCoordinator(ListenServerState& state)
{
    if (!state.iceEnabled || !state.iceListenerAgent)
        return;

    uint64_t now = nowMs();
    if (now - state.lastIceCoordinatorPollMs < 500)
        return;
    state.lastIceCoordinatorPollMs = now;

    auto pollResult = coordinatorIcePoll(state.serverCode, state.iceSessionId);
    if (!pollResult.ok)
        return;

    if (pollResult.status == "client_ready" &&
        !pollResult.clientIceDescription.empty())
    {
        printf("[SERVER ICE] new client detected via coordinator polling\n");

        // Create a dedicated ICE agent for this client
        IceConfiguration iceConfig = loadIceConfig();
        auto clientAgent = std::make_unique<IceAgent>();
        if (!clientAgent->initialize(iceConfig))
        {
            printf("[SERVER ICE] client agent init failed\n");
            return;
        }
        if (!clientAgent->gatherCandidates())
        {
            printf("[SERVER ICE] client agent gather failed\n");
            return;
        }
        if (!waitForAgentState(*clientAgent, IceAgentState::GatheringComplete, 10000))
        {
            printf("[SERVER ICE] client agent gather timeout\n");
            return;
        }

        // Set the client's remote description
        clientAgent->setRemoteDescription(pollResult.clientIceDescription);

        // Wait for ICE connection
        if (!waitForAgentState(*clientAgent, IceAgentState::Connected, 20000))
        {
            printf("[SERVER ICE] client agent connection timeout\n");
            return;
        }
        clientAgent->logSelectedPath();

        // Wrap in IceTransport and add to pending transports
        auto transport = std::make_unique<IceTransport>(std::move(clientAgent));
        printf("[SERVER ICE] pushing transport to pending (count before=%zu)\n",
               state.pendingIceTransports.size());
        state.pendingIceTransports.push_back(std::move(transport));
        printf("[SERVER ICE] pending transport now=%zu\n", state.pendingIceTransports.size());

        printf("[SERVER ICE] client ICE transport created and pending\n");
    }
    else if (pollResult.status == "waiting_client")
    {
        printf("[SERVER ICE] waiting for first client...\n");
    }
}

// ── Poll all connected players' ICE transports ───────────────────────
// Called from the server tick loop. Processes packets from:
//   1. All existing players' ICE transports
//   2. Pending (unregistered) ICE transports

void tickServerIceTransports(SOCKET sock,
                             std::unordered_map<uint32_t, ServerPlayer>& players,
                             std::unordered_map<uint32_t, ServerNpc>& npcs,
                             std::unordered_map<uint32_t, ServerProjectile>& projectiles,
                             uint32_t& nextEntityId,
                             uint32_t& nextPlayerId,
                             std::vector<std::unique_ptr<IGameTransport>>& pendingIceTransports,
                             const HeadlessWorld& world,
                             uint32_t tick, uint64_t& totalPacketsOut)
{
    char buffer[2048];

    // 1. Poll existing players' ICE transports
    for (auto& kv : players)
    {
        ServerPlayer& player = kv.second;
        if (!player.transport)
            continue;

        std::vector<ReceivedPacket> pkts;
        player.transport->poll(pkts);

        for (const ReceivedPacket& rp : pkts)
        {
            if (rp.bytes.size() < (int)sizeof(PacketHeader) || rp.bytes.size() > sizeof(buffer))
                continue;
            memcpy(buffer, rp.bytes.data(), rp.bytes.size());
            PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
            if (header->magic != PROTOCOL_MAGIC || header->version != PROTOCOL_VERSION)
                continue;

            sockaddr_in from = player.addr;

            // Dispatch to handlers
            if (header->type == PACKET_INPUT)
                handleInputPacket(buffer, (int)rp.bytes.size(), players, world,
                                  nextEntityId, npcs);
            else if (header->type == PACKET_DISCONNECT)
                handleDisconnect(players, buffer);
            else if (header->type == PACKET_SHOT_REQUEST)
                handleShotRequest(sock, from, buffer, (int)rp.bytes.size(), players, world, tick, totalPacketsOut);
            else if (header->type == PACKET_MELEE_HIT_REQUEST)
                handleMeleeHitRequest(sock, from, buffer, (int)rp.bytes.size(), players, tick, totalPacketsOut);
            else if (header->type == PACKET_PELLET_BLAST_REQUEST)
                handlePelletBlastRequest(sock, from, buffer, (int)rp.bytes.size(), players, world, tick, totalPacketsOut);
            else if (header->type == PACKET_PROJECTILE_FIRE_REQUEST)
                handleProjectileFireRequest(sock, from, buffer, (int)rp.bytes.size(), players,
                                            projectiles, nextEntityId, world, tick, totalPacketsOut);
            else if (header->type == PACKET_CHAT_MESSAGE)
                handleChatMessage(sock, buffer, (int)rp.bytes.size(), players, tick, totalPacketsOut);
            else if (header->type == PACKET_PING)
                handlePing(sock, from, buffer, (int)rp.bytes.size(), tick);
            else if (header->type == PACKET_NPC_DAMAGE_REQUEST)
                handleNpcDamageRequest(sock, buffer, (int)rp.bytes.size(), from,
                                       players, npcs, tick, totalPacketsOut);
            else if (header->type == PACKET_SERVER_COMMAND)
                handleServerCommand(buffer, (int)rp.bytes.size(), players, npcs);
            else if (header->type == PACKET_CLIENT_MAP_READY && (int)rp.bytes.size() >= (int)sizeof(ClientMapReadyPacket))
            {
                const ClientMapReadyPacket* ready = reinterpret_cast<const ClientMapReadyPacket*>(buffer);
                if (ready->header.playerId == ready->assignedPlayerId)
                {
                    auto it = players.find(ready->assignedPlayerId);
                    if (it != players.end() && !it->second.spawned)
                    {
                        it->second.spawned = true;
                        it->second.vel = glm::vec3(0.0f);
                        it->second.clientStateUpdated = false;
                        printf("%s [MAP READY/ICE] id=%u name=\"%s\"\n",
                               serverTimestamp(), it->second.id, it->second.name.c_str());
                    }
                }
            }
        }
    }

    // 2. Poll pending ICE transports (unregistered clients)
    // Each pending transport gets a unique ICE client ID to avoid
    // handleHello's sameAddress collision (all ICE clients would
    // otherwise match the empty address since they don't have real UDP addr).
    static uint32_t nextIceClientId = 1000000;
    for (auto it = pendingIceTransports.begin(); it != pendingIceTransports.end(); )
    {
        IGameTransport* transport = it->get();
        std::vector<ReceivedPacket> pkts;
        transport->poll(pkts);

        // Assign a unique fake address for this ICE client
        sockaddr_in iceAddr{};
        iceAddr.sin_family = AF_INET;
        iceAddr.sin_addr.s_addr = htonl(nextIceClientId++);
        iceAddr.sin_port = htons(1);

        bool processedHello = false;
        for (const ReceivedPacket& rp : pkts)
        {
            if (rp.bytes.size() < (int)sizeof(PacketHeader) || rp.bytes.size() > sizeof(buffer))
                continue;
            memcpy(buffer, rp.bytes.data(), rp.bytes.size());
            PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
            if (header->magic != PROTOCOL_MAGIC || header->version != PROTOCOL_VERSION)
                continue;

            if (header->type == PACKET_HELLO && (int)rp.bytes.size() >= (int)sizeof(HelloPacket))
            {
                printf("[SERVER ICE] Hello via ICE transport\n");

                handleHello(sock, iceAddr, buffer, (int)rp.bytes.size(), players,
                            nextPlayerId, tick, totalPacketsOut, &world);

                uint32_t newPlayerId = nextPlayerId > 1 ? nextPlayerId - 1 : 1;
                auto newPlayer = players.find(newPlayerId);
                if (newPlayer != players.end())
                {
                    newPlayer->second.transport = std::move(*it);
                    printf("[SERVER ICE] player %u assigned ICE transport\n", newPlayerId);
                }
                processedHello = true;
                break;
            }
            else if (header->type == PACKET_JOIN_REQUEST && (int)rp.bytes.size() >= (int)sizeof(JoinRequestPacket))
            {
                printf("[SERVER ICE] JoinRequest via ICE transport\n");

                handleJoinRequest(sock, iceAddr, buffer, (int)rp.bytes.size(), players,
                                  nextPlayerId, tick, totalPacketsOut, &world);

                uint32_t newPlayerId = nextPlayerId > 1 ? nextPlayerId - 1 : 1;
                auto newPlayer = players.find(newPlayerId);
                if (newPlayer != players.end())
                {
                    newPlayer->second.transport = std::move(*it);
                    printf("[SERVER ICE] player %u assigned ICE transport (join)\n", newPlayerId);
                }
                processedHello = true;
                break;
            }
        }

        if (processedHello)
            it = pendingIceTransports.erase(it);
        else
            ++it;
    }
}

// ── Send data to a specific player ───────────────────────────────────
// Uses ICE transport if available, falls back to raw sendto.

bool serverSendToPlayer(SOCKET sock, const ServerPlayer& player,
                         const void* data, size_t size)
{
    if (player.transport)
        return player.transport->send(data, size);

    if (sock == INVALID_SOCKET)
        return false;

    int sent = sendto(sock, (const char*)data, (int)size, 0,
                      (sockaddr*)&player.addr, sizeof(player.addr));
    return sent != SOCKET_ERROR;
}

} // namespace MimitaNet
