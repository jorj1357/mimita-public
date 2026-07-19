#include "network/server.h"
#include "network/game-transport.h"
#include "network/ice-transport.h"
#include "network/ice/ice-config.h"
#include "network/coordinator-client.h"

#include <algorithm>
#include <cstdio>

namespace MimitaNet {

std::vector<std::unique_ptr<PendingIcePeer>> gPendingIcePeers;

// ── Wait for agent state (blocking, for startup only) ────────────────

bool waitForAgentState(IceAgent& agent, IceAgentState target, int timeoutMs)
{
    int waited = 0;
    while (waited < timeoutMs)
    {
        agent.tick();
        std::vector<IceEvent> evs;
        agent.pollEvents(evs);
        auto s = agent.state();
        if (s == target || s == IceAgentState::Completed || s == IceAgentState::Connected)
            return true;
        if (s == IceAgentState::Failed)
            return false;
        Sleep(50);
        waited += 50;
    }
    return false;
}

// ── Non-blocking agent tick ──────────────────────────────────────────
// Ticks the agent once, returns current state. Never blocks.
static IceAgentState tickAgentOnce(IceAgent& agent)
{
    agent.tick();
    std::vector<IceEvent> evs;
    agent.pollEvents(evs);
    return agent.state();
}

// ── Initialize ICE listener ──────────────────────────────────────────

bool initServerIceListener(ListenServerState& state)
{
    printf("[SERVER ICE] initializing ICE listener\n");

    // Request TURN credentials from coordinator
    TurnCredentials turnCreds = coordinatorRequestTurnCredentials();
    IceConfiguration iceConfig;
    if (turnCreds.ok)
        iceConfig = loadIceConfigWithTurn(turnCreds.host, turnCreds.port,
                                           turnCreds.username, turnCreds.credential);
    else
        iceConfig = loadIceConfig();
    if (iceConfig.turn.password.empty())
        printf("[SERVER ICE] WARNING: no TURN credentials; direct connections only\n");

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

    // Non-blocking gather loop (up to 15s, but returns immediately for tick loop)
    {
        int waited = 0;
        while (waited < 15000)
        {
            auto s = tickAgentOnce(*agent);
            if (s == IceAgentState::GatheringComplete || s == IceAgentState::Connected || s == IceAgentState::Completed)
                break;
            if (s == IceAgentState::Failed) {
                printf("[SERVER ICE] FATAL: gather failed\n");
                return false;
            }
            Sleep(50);
            waited += 50;
        }
        if (waited >= 15000) {
            printf("[SERVER ICE] FATAL: gather timeout\n");
            return false;
        }
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

    hostedRoomSession().active = true;
    hostedRoomSession().roomCode = hostResult.roomCode;
    hostedRoomSession().hostToken = hostResult.joinToken;
    hostedRoomSession().joinToken = hostResult.joinToken;
    hostedRoomSession().serverProcessId = (uint64_t)GetCurrentProcessId();
    hostedRoomSession().coordinatorRoomType = "ice";
    hostedRoomSession().createdAtMs = nowMs();
    setServerCoordinatorState(hostResult.roomCode, hostResult.joinToken);

    printf("[SERVER ICE] listener registered: code=%s session=%s\n",
           hostResult.roomCode.c_str(), state.iceSessionId.c_str());
    return true;
}

// ── Poll coordinator for new requests + advance existing peers ───────

void tickIceCoordinator(ListenServerState& state)
{
    if (!state.iceEnabled || !state.iceListenerAgent)
        return;

    uint64_t now = nowMs();
    if (now - state.lastIceCoordinatorPollMs < 500)
        return;
    state.lastIceCoordinatorPollMs = now;

    // Non-blocking: poll coordinator for pending client requests
    auto pending = coordinatorIceHostPoll(state.serverCode, state.iceSessionId);
    if (pending.hasRequest)
    {
        // Validate SDP before creating agent
        if (pending.clientIceDescription.find("a=ice-ufrag:") == std::string::npos ||
            pending.clientIceDescription.find("a=ice-pwd:") == std::string::npos)
        {
            printf("[ICE HOST REQUEST] req=%s REJECTED: invalid SDP (missing ufrag/pwd)\n",
                   pending.requestId.substr(0, 12).c_str());
            return;
        }

        // Create per-client peer agent
        auto peer = std::make_unique<PendingIcePeer>();
        peer->requestId = pending.requestId;
        peer->clientSessionId = pending.clientSessionId;
        peer->clientIceDescription = pending.clientIceDescription;
        peer->startedAtMs = now;
        peer->lastEventMs = now;

        printf("[ICE HOST REQUEST] req=%s client=%s creating peer agent...\n",
               pending.requestId.substr(0, 12).c_str(),
               pending.clientSessionId.substr(0, 12).c_str());

        // Get TURN credentials for peer agent
        TurnCredentials turnCreds = coordinatorRequestTurnCredentials();
        IceConfiguration iceConfig;
        if (turnCreds.ok)
            iceConfig = loadIceConfigWithTurn(turnCreds.host, turnCreds.port,
                                               turnCreds.username, turnCreds.credential);
        else
            iceConfig = loadIceConfig();

        auto clientAgent = std::make_unique<IceAgent>();
        if (!clientAgent->initialize(iceConfig))
        {
            printf("[ICE HOST REQUEST] peer agent init failed\n");
            return;
        }
        if (!clientAgent->gatherCandidates())
        {
            printf("[ICE HOST REQUEST] peer agent gather failed\n");
            return;
        }
        peer->agent = std::move(clientAgent);
        peer->state = PendingIcePeer::State::Gathering;
        gPendingIcePeers.push_back(std::move(peer));
    }
}

// ── Tick all pending ICE peers (non-blocking) ────────────────────────

void tickIcePeers(const std::string& serverCode, const std::string& iceSessionId,
                  std::vector<std::unique_ptr<IGameTransport>>& pendingIceTransports)
{
    uint64_t now = nowMs();

    for (auto it = gPendingIcePeers.begin(); it != gPendingIcePeers.end(); )
    {
        PendingIcePeer& peer = **it;
        peer.lastEventMs = now;
        IceAgent* agent = peer.agent.get();
        if (!agent) { it = gPendingIcePeers.erase(it); continue; }

        IceAgentState agentState = tickAgentOnce(*agent);

        if (agentState == IceAgentState::Failed) {
            printf("[ICE PEER FAIL] req=%s state=failed\n", peer.requestId.substr(0, 12).c_str());
            coordinatorIceRequestComplete(serverCode, peer.requestId);
            it = gPendingIcePeers.erase(it);
            continue;
        }

        if (peer.state == PendingIcePeer::State::Gathering)
        {
            if (agentState == IceAgentState::GatheringComplete ||
                agentState == IceAgentState::Connected ||
                agentState == IceAgentState::Completed)
            {
                printf("[ICE HOST GATHER] req=%s complete. Applying client SDP...\n",
                       peer.requestId.substr(0, 12).c_str());

                if (!agent->setRemoteDescription(peer.clientIceDescription))
                {
                    printf("[ICE HOST GATHER] setRemoteDescription FAILED\n");
                    coordinatorIceRequestComplete(serverCode, peer.requestId);
                    it = gPendingIcePeers.erase(it);
                    continue;
                }
                peer.state = PendingIcePeer::State::Connecting;
            }
            else if (now - peer.startedAtMs > 15000)
            {
                printf("[ICE HOST GATHER] req=%s timed out (15s)\n",
                       peer.requestId.substr(0, 12).c_str());
                coordinatorIceRequestComplete(serverCode, peer.requestId);
                it = gPendingIcePeers.erase(it);
                continue;
            }
        }
        else if (peer.state == PendingIcePeer::State::Connecting)
        {
            if (agentState == IceAgentState::Connected || agentState == IceAgentState::Completed)
            {
                printf("[ICE HOST CONNECT] req=%s connected!\n",
                       peer.requestId.substr(0, 12).c_str());
                agent->logSelectedPath();
                peer.state = PendingIcePeer::State::Connected;

                coordinatorIceHostAnswer(serverCode, iceSessionId,
                                         peer.requestId, agent->localSdp());

                auto transport = std::make_unique<IceTransport>(std::move(peer.agent));
                pendingIceTransports.push_back(std::move(transport));
                printf("[ICE PEER] transport pushed (pending=%zu)\n", pendingIceTransports.size());

                coordinatorIceRequestComplete(serverCode, peer.requestId);
                it = gPendingIcePeers.erase(it);
                continue;
            }
            else if (now - peer.startedAtMs > 30000)
            {
                printf("[ICE HOST CONNECT] req=%s timed out (30s)\n",
                       peer.requestId.substr(0, 12).c_str());
                coordinatorIceRequestComplete(serverCode, peer.requestId);
                it = gPendingIcePeers.erase(it);
                continue;
            }
        }

        ++it;
    }
}

// ── Poll all connected players' ICE transports ───────────────────────

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
    char buf[2048];

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
            if (rp.bytes.size() < (int)sizeof(PacketHeader) || rp.bytes.size() > sizeof(buf))
                continue;
            memcpy(buf, rp.bytes.data(), rp.bytes.size());
            PacketHeader* header = reinterpret_cast<PacketHeader*>(buf);
            if (header->magic != PROTOCOL_MAGIC || header->version != PROTOCOL_VERSION)
                continue;

            sockaddr_in from = player.addr;

            if (header->type == PACKET_INPUT)
                handleInputPacket(buf, (int)rp.bytes.size(), players, world, nextEntityId, npcs);
            else if (header->type == PACKET_DISCONNECT)
                handleDisconnect(players, buf);
            else if (header->type == PACKET_SHOT_REQUEST)
                handleShotRequest(sock, from, buf, (int)rp.bytes.size(), players, world, tick, totalPacketsOut);
            else if (header->type == PACKET_MELEE_HIT_REQUEST)
                handleMeleeHitRequest(sock, from, buf, (int)rp.bytes.size(), players, tick, totalPacketsOut);
            else if (header->type == PACKET_PELLET_BLAST_REQUEST)
                handlePelletBlastRequest(sock, from, buf, (int)rp.bytes.size(), players, world, tick, totalPacketsOut);
            else if (header->type == PACKET_PROJECTILE_FIRE_REQUEST)
                handleProjectileFireRequest(sock, from, buf, (int)rp.bytes.size(), players,
                                            projectiles, nextEntityId, world, tick, totalPacketsOut);
            else if (header->type == PACKET_CHAT_MESSAGE)
                handleChatMessage(sock, buf, (int)rp.bytes.size(), players, tick, totalPacketsOut);
            else if (header->type == PACKET_PING)
                handlePing(sock, from, buf, (int)rp.bytes.size(), tick);
            else if (header->type == PACKET_NPC_DAMAGE_REQUEST)
                handleNpcDamageRequest(sock, buf, (int)rp.bytes.size(), from, players, npcs, tick, totalPacketsOut);
            else if (header->type == PACKET_SERVER_COMMAND)
                handleServerCommand(buf, (int)rp.bytes.size(), players, npcs);
            else if (header->type == PACKET_GODBALL_STATE)
                handleGodballState(sock, players, buf, (int)rp.bytes.size());
            else if (header->type == PACKET_RELIABLE_EVENT_ACK && (int)rp.bytes.size() >= (int)sizeof(ReliableEventAckPacket))
                handleReliableEventAck(buf, (int)rp.bytes.size(), players);
            else if (header->type == PACKET_CLIENT_MAP_READY && (int)rp.bytes.size() >= (int)sizeof(ClientMapReadyPacket))
            {
                const ClientMapReadyPacket* ready = reinterpret_cast<const ClientMapReadyPacket*>(buf);
                if (ready->header.playerId == ready->assignedPlayerId)
                {
                    auto pi = players.find(ready->assignedPlayerId);
                    if (pi != players.end() && !pi->second.spawned)
                    {
                        pi->second.spawned = true;
                        pi->second.vel = glm::vec3(0.0f);
                        pi->second.clientStateUpdated = false;
                        printf("%s [MAP READY/ICE] id=%u name=\"%s\"\n",
                               serverTimestamp(), pi->second.id, pi->second.name.c_str());
                    }
                }
            }
        }
    }

    // 2. Process pending ICE transports (unregistered clients)
    static uint32_t nextIceClientId = 1000000;
    for (auto it = pendingIceTransports.begin(); it != pendingIceTransports.end(); )
    {
        IGameTransport* transport = it->get();
        std::vector<ReceivedPacket> pkts;
        transport->poll(pkts);

        sockaddr_in iceAddr{};
        iceAddr.sin_family = AF_INET;
        iceAddr.sin_addr.s_addr = htonl(nextIceClientId++);
        iceAddr.sin_port = htons(1);

        bool processed = false;
        for (const ReceivedPacket& rp : pkts)
        {
            if (rp.bytes.size() < (int)sizeof(PacketHeader) || rp.bytes.size() > sizeof(buf))
                continue;
            memcpy(buf, rp.bytes.data(), rp.bytes.size());
            PacketHeader* header = reinterpret_cast<PacketHeader*>(buf);
            if (header->magic != PROTOCOL_MAGIC || header->version != PROTOCOL_VERSION)
                continue;

            if (header->type == PACKET_HELLO && (int)rp.bytes.size() >= (int)sizeof(HelloPacket))
            {
                printf("[ICE GAME RX] Hello via ICE transport\n");
                // Assign transport BEFORE calling handler so WelcomePacket goes through ICE
                uint32_t newId = nextPlayerId++;
                ServerPlayer& p = players[newId];
                p.id = newId;
                p.addr = iceAddr;
                p.transport = std::move(*it);
                p.lastHeardMs = nowMs();
                p.lastShotSerial = 0;
                p.lastProjectileFireSerial = 0;
                p.lastMeleeAttackSerial = 0;
                p.projectileFireCooldown = 0.0f;
                p.name = uniquePlayerName(players,
                    reinterpret_cast<const HelloPacket*>(buf)->name, newId);
                glm::vec3 spawnPos = {1.0f + (float)((newId - 1) % 16) * 1.5f, 5.0f, 30.0f};
                if (!world.spawnPoints.empty()) {
                    size_t idx = (newId - 1) % world.spawnPoints.size();
                    spawnPos = world.spawnPoints[idx].position;
                }
                beginAuthoritativeTransform(p, spawnPos, glm::vec3(0.0f), 0.0f, "ice_hello");
                p.spawned = false;

                // Send WelcomePacket through transport
                WelcomePacket welcome{};
                welcome.header.type = PACKET_WELCOME;
                welcome.header.tick = tick;
                welcome.header.playerId = newId;
                welcome.header.transformEpoch = p.transformEpoch;
                welcome.assignedPlayerId = newId;
                welcome.tickRate = SERVER_TICK_RATE;
                copyName(welcome.approvedName, p.name);
                std::memset(welcome.reconnectToken, 0, sizeof(welcome.reconnectToken));
                std::strncpy(welcome.reconnectToken, p.reconnectToken.c_str(), sizeof(welcome.reconnectToken) - 1);
                std::memset(welcome.mapId, 0, sizeof(welcome.mapId));
                std::strncpy(welcome.mapId, getServerMapId().c_str(), sizeof(welcome.mapId) - 1);
                p.transport->send(&welcome, sizeof(welcome));
                ++totalPacketsOut;
                printf("[ICE PLAYER ASSIGN] id=%u name=\"%s\" transport=%p\n",
                       newId, p.name.c_str(), (void*)p.transport.get());
                processed = true;
                break;
            }
            else if (header->type == PACKET_JOIN_REQUEST && (int)rp.bytes.size() >= (int)sizeof(JoinRequestPacket))
            {
                printf("[ICE GAME RX] JoinRequest via ICE transport\n");
                const JoinRequestPacket* joinReq = reinterpret_cast<const JoinRequestPacket*>(buf);

                // Validate ICE join token
                if (!coordinatorIceValidateJoin(getServerCoordinatorCode(), joinReq->joinToken))
                {
                    printf("[ICE JOIN REJECT] invalid token\n");
                    JoinRejectPacket reject{};
                    reject.header.type = PACKET_JOIN_REJECT;
                    reject.reason = 2;
                    transport->send(&reject, sizeof(reject));
                    processed = true;
                    break;
                }

                // Assign transport before sending JoinAccept
                uint32_t newId = nextPlayerId++;
                ServerPlayer& p = players[newId];
                p.id = newId;
                p.addr = iceAddr;
                p.transport = std::move(*it);
                p.lastHeardMs = nowMs();
                p.joinTokenValidated = true;
                p.joinToken = joinReq->joinToken;
                p.name = uniquePlayerName(players, joinReq->name, newId);
                glm::vec3 spawnPos = {1.0f + (float)((newId - 1) % 16) * 1.5f, 5.0f, 30.0f};
                if (!world.spawnPoints.empty()) {
                    size_t idx = (newId - 1) % world.spawnPoints.size();
                    spawnPos = world.spawnPoints[idx].position;
                }
                beginAuthoritativeTransform(p, spawnPos, glm::vec3(0.0f), 0.0f, "ice_join");
                p.spawned = false;

                JoinAcceptPacket accept{};
                accept.header.type = PACKET_JOIN_ACCEPT;
                accept.header.tick = tick;
                accept.header.playerId = newId;
                accept.header.transformEpoch = p.transformEpoch;
                accept.assignedPlayerId = newId;
                accept.tickRate = SERVER_TICK_RATE;
                copyName(accept.approvedName, p.name);
                std::string rt = generateReconnectToken();
                p.reconnectToken = rt;
                std::memset(accept.reconnectToken, 0, sizeof(accept.reconnectToken));
                std::strncpy(accept.reconnectToken, rt.c_str(), sizeof(accept.reconnectToken) - 1);
                std::memset(accept.mapId, 0, sizeof(accept.mapId));
                std::strncpy(accept.mapId, getServerMapId().c_str(), sizeof(accept.mapId) - 1);
                p.transport->send(&accept, sizeof(accept));
                ++totalPacketsOut;
                printf("[ICE PLAYER ASSIGN] id=%u name=\"%s\" via JoinRequest\n", newId, p.name.c_str());
                processed = true;
                break;
            }
        }

        if (processed)
            it = pendingIceTransports.erase(it);
        else
            ++it;
    }
}

// ── Send data to a specific player ───────────────────────────────────

bool serverSendToPlayer(SOCKET sock, const ServerPlayer& player,
                         const void* data, size_t size)
{
    if (player.transport)
        return player.transport->send(data, size);
    if (sock == INVALID_SOCKET) return false;
    int sent = sendto(sock, (const char*)data, (int)size, 0,
                      (sockaddr*)&player.addr, sizeof(player.addr));
    return sent != SOCKET_ERROR;
}

} // namespace MimitaNet
