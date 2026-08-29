// 07 21 2026, 17 10
/* purpose
* Owns dedicated-server ICE listener setup and ICE transport packet dispatch.
* Routes ICE game packets into the same server handlers as UDP networking.
* Maintains pending ICE peer state while the main server tick keeps authority.
* Does NOT implement movement validation, packet schemas, or gameplay simulation.
* Does NOT own coordinator persistence, renderer state, or client prediction.
* Does NOT bypass server ownership checks for ICE-originated packets.
*/

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

    IceHostMetadata metadata;
    metadata.serverName = state.serverName.empty() ? "MiMITA Server" : state.serverName;
    metadata.map = state.mapName.empty() ? "funworldv3" : state.mapName;
    metadata.gamemode = state.gameMode.empty() ? "sandbox" : state.gameMode;
    metadata.maxPlayers = (int)state.maxPlayers;
    metadata.passwordProtected = state.passwordProtected;
    metadata.hostPlayerName = state.hostPlayerName;
    metadata.port = state.port;

    auto hostResult = coordinatorIceHost(state.iceSessionId, agent->localSdp(), metadata);
    if (!hostResult.ok)
    {
        printf("[SERVER ICE] FATAL: coordinatorIceHost failed\n");
        return false;
    }

    state.serverCode = hostResult.roomCode;
    state.joinToken = hostResult.joinToken;
    state.iceListenerAgent = std::move(agent);
    state.lastIceCoordinatorPollMs = nowMs();

    hostedRoomSession().active = true;
    hostedRoomSession().roomCode = hostResult.roomCode;
    hostedRoomSession().hostToken = hostResult.joinToken;
    hostedRoomSession().joinToken = hostResult.joinToken;
    hostedRoomSession().serverProcessId = (uint64_t)GetCurrentProcessId();
    hostedRoomSession().coordinatorRoomType = "ice";
    hostedRoomSession().createdAtMs = nowMs();
    setServerCoordinatorState(hostResult.roomCode, hostResult.joinToken);
    setServerPasswordState(state.passwordProtected, state.password);

    printf("[SERVER ICE] listener registered: code=%s session=%s\n",
           hostResult.roomCode.c_str(), state.iceSessionId.c_str());
    return true;
}

// ── Poll coordinator for new requests + advance existing peers ───────

void tickIceCoordinator(ListenServerState& state, size_t playerCount)
{
    static uint64_t s_lastDebugLog = 0;
    static uint64_t s_lastEntryLog = 0;
    uint64_t nowDbg = nowMs();

    // Unconditional heartbeat — proves function is reached at runtime
    if (nowDbg - s_lastEntryLog >= 10000) {
        printf("[TICK ICE ENTERED] agent=%d serverCode=%s\n",
               (int)(state.iceListenerAgent != nullptr), state.serverCode.c_str());
        fflush(stdout);
        s_lastEntryLog = nowDbg;
    }

    if (!state.iceListenerAgent)
    {
        if (nowDbg - s_lastDebugLog >= 5000) {
            printf("[ICE COORD DEBUG] no listener agent; serverCode=%s\n", state.serverCode.c_str());
            fflush(stdout);
            s_lastDebugLog = nowDbg;
        }
        return;
    }

    if (nowDbg - state.lastIceCoordinatorPollMs < 500)
        return;
    state.lastIceCoordinatorPollMs = nowDbg;

    // Non-blocking: poll coordinator for pending client requests
    auto pending = coordinatorIceHostPoll(state.serverCode, state.iceSessionId, (int)playerCount);
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
        peer->startedAtMs = nowDbg;
        peer->lastEventMs = nowDbg;

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
                  std::vector<PendingServerTransport>& pendingIceTransports)
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
                coordinatorIceHostAnswer(serverCode, iceSessionId,
                                         peer.requestId, agent->localSdp());
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

                PendingServerTransport pending{};
                pending.connectionId = allocateIceConnectionId();
                pending.diagnosticEndpoint =
                    legacyEndpointForTransportConnection(pending.connectionId);
                pending.connectedAtMs = now;
                pending.transport =
                    std::make_unique<IceTransport>(std::move(peer.agent));
                pendingIceTransports.push_back(std::move(pending));
                printf("[ICE PEER] transport pushed connection=%llu pending=%zu\n",
                       (unsigned long long)pendingIceTransports.back().connectionId.value,
                       pendingIceTransports.size());

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
                             uint32_t& nextProjectileId,
                             uint32_t& nextPlayerId,
                             std::vector<PendingServerTransport>& pendingIceTransports,
                             const HeadlessWorld& world,
                             uint32_t tick,
                             uint64_t& totalPacketsIn,
                             uint64_t& totalPacketsOut,
                             ServerPacketStats* stats,
                             DisagreementRetransmitState* retransmitState)
{
    // 1. Poll existing players' ICE transports
    // Deferred erasure: handleDisconnect pushes to this vector instead of
    // erasing from the map during iteration, avoiding iterator invalidation.
    std::vector<uint32_t> pendingRemovals;

    for (auto& kv : players)
    {
        ServerPlayer& player = kv.second;
        if (!player.transport)
            continue;
        if (!player.hasConnectionId)
        {
            player.connectionId = allocateIceConnectionId();
            player.hasConnectionId = true;
            player.addr = legacyEndpointForTransportConnection(player.connectionId);
        }

        std::vector<ReceivedPacket> pkts;
        {
            uint64_t pollStart = nowMs();
            player.transport->poll(pkts);
            uint64_t pollUs = nowMs() - pollStart;
            if (pollUs > 5)
            {
                static uint64_t lastIcePollLogMs = 0;
                uint64_t nowIcePoll = nowMs();
                if (nowIcePoll - lastIcePollLogMs >= 1000)
                {
                    lastIcePollLogMs = nowIcePoll;
                    printf("[ICE POLL] playerId=%u pollUs=%llu pkts=%zu\n",
                           player.id, (unsigned long long)pollUs, pkts.size());
                }
            }
        }

        // Limit processed packets per poll to prevent flooding from
        // a single transport (e.g. stale ICE packets after disconnect).
        constexpr size_t kMaxPacketsPerPoll = 64;
        size_t packetsProcessed = 0;
        for (const ReceivedPacket& rp : pkts)
        {
            if (packetsProcessed >= kMaxPacketsPerPoll)
            {
                static uint64_t lastDropLog = 0;
                uint64_t nowDrop = nowMs();
                if (nowDrop - lastDropLog >= 1000)
                {
                    printf("[ICE DROP] playerId=%u dropped=%zu total=%zu\n",
                           player.id, pkts.size() - kMaxPacketsPerPoll, pkts.size());
                    lastDropLog = nowDrop;
                }
                break;
            }
            TransportReceiveEvent event{};
            event.connectionId = player.connectionId;
            event.remoteEndpoint = player.addr;
            event.payload = rp.bytes.data();
            event.payloadBytes = (int)rp.bytes.size();
            event.receivedAtMs = rp.receivedAtMs;
            event.transportKind = TransportKind::Ice;
            processServerPacket(sock, event, players, npcs, projectiles,
                                nextPlayerId, nextEntityId, nextProjectileId,
                                world, tick, totalPacketsIn, totalPacketsOut,
                                stats, retransmitState, &player, nullptr,
                                &pendingRemovals);
            ++packetsProcessed;
        }

        printf("[ICE POLL DONE] playerId=%u pkts=%zu processed=%zu — return to loop\n",
               player.id, pkts.size(), packetsProcessed);
        fflush(stdout);
    }

    // Deferred erasure: remove players that disconnected via ICE transport.
    // The transport was already closed in handleDisconnect; erasing the
    // ServerPlayer here frees the IGameTransport wrapper safely, after the
    // iteration over the map has completed.
    for (uint32_t id : pendingRemovals)
    {
        auto it = players.find(id);
        if (it != players.end())
        {
            printf("[DEFERRED REMOVE] removing id=%u — players left: %zu\n",
                   id, players.size() - 1);
            players.erase(it);
        }
    }
    if (!pendingRemovals.empty())
        fflush(stdout);

    // 2. Process pending ICE transports (unregistered clients)
    for (auto it = pendingIceTransports.begin(); it != pendingIceTransports.end(); )
    {
        PendingServerTransport& pending = *it;
        if (!pending.transport)
        {
            it = pendingIceTransports.erase(it);
            continue;
        }

        std::vector<ReceivedPacket> pkts;
        pending.transport->poll(pkts);

        bool processed = false;
        for (const ReceivedPacket& rp : pkts)
        {
            TransportReceiveEvent event{};
            event.connectionId = pending.connectionId;
            event.remoteEndpoint = pending.diagnosticEndpoint;
            event.payload = rp.bytes.data();
            event.payloadBytes = (int)rp.bytes.size();
            event.receivedAtMs = rp.receivedAtMs;
            event.transportKind = TransportKind::Ice;
            ServerPacketProcessResult result =
                processServerPacket(sock, event, players, npcs, projectiles,
                                    nextPlayerId, nextEntityId,
                                    nextProjectileId, world, tick,
                                    totalPacketsIn, totalPacketsOut, stats,
                                    retransmitState, nullptr,
                                    &pending.transport);
            processed = result.transportConsumed;
            if (processed)
                break;
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
