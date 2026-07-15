#include "network/ice/ice-server.h"
#include "network/ice/ice-agent.h"
#include "network/ice/ice-config.h"
#include "network/coordinator-client.h"
#include "network/packets.h"
#include "debug/debug-log.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <chrono>
#include <windows.h>
#include <vector>
#include <unordered_map>
#include <memory>
#include <random>
#include <ctime>

// ── Client peer state ──────────────────────────────────────────────
struct IceClientPeer {
    uint32_t clientId = 0;
    uint32_t playerEntityId = 0;
    std::unique_ptr<IceAgent> agent;
    bool authenticated = false;
    uint64_t lastPacketMs = 0;

    // Player simulation state
    float px = 1.0f, py = 5.0f, pz = 30.0f;
    float vx = 0, vy = 0, vz = 0;
    float yaw = 0;
    float aimX = 1.0f, aimY = 0, aimZ = 0;
    int health = 100;
    bool alive = true;

    // For reconnection
    std::string joinToken;
};

static std::unordered_map<uint32_t, IceClientPeer> gPeers;
static std::mt19937 gRng((unsigned int)std::time(nullptr));

static std::string generateToken()
{
    std::uniform_int_distribution<int> dist(0, 15);
    const char* hex = "0123456789abcdef";
    std::string t(32, '0');
    for (auto& c : t) c = hex[dist(gRng)];
    return t;
}

static bool processPeerPacket(IceClientPeer& peer, const char* data, size_t size,
                               uint32_t tick, const std::string& roomCode,
                               std::string& responseType,
                               std::vector<char>& responseData)
{
    if (size < sizeof(MimitaNet::PacketHeader)) return false;
    auto* hdr = reinterpret_cast<const MimitaNet::PacketHeader*>(data);
    if (hdr->magic != MimitaNet::PROTOCOL_MAGIC) return false;

    peer.lastPacketMs = GetTickCount64();

    if (hdr->type == MimitaNet::PACKET_JOIN_REQUEST)
    {
        if (size < sizeof(MimitaNet::JoinRequestPacket)) return false;
        auto* join = reinterpret_cast<const MimitaNet::JoinRequestPacket*>(data);
        std::string token((const char*)join->joinToken);
        token = token.c_str();

        bool valid = MimitaNet::coordinatorIceValidateJoin(roomCode, token);

        if (valid)
        {
            peer.authenticated = true;
            peer.joinToken = token;
            std::string name((const char*)join->name);
            printf("[ICE SERVER] JOIN ACCEPT client=%u name=%s\n", peer.clientId, name.c_str());

            MimitaNet::JoinAcceptPacket accept{};
            accept.header.magic = MimitaNet::PROTOCOL_MAGIC;
            accept.header.version = MimitaNet::PROTOCOL_VERSION;
            accept.header.type = MimitaNet::PACKET_JOIN_ACCEPT;
            accept.header.playerId = peer.clientId;
            accept.assignedPlayerId = peer.clientId;
            accept.tickRate = 60.0f;
            memcpy(accept.approvedName, join->name, sizeof(accept.approvedName));
            memcpy(accept.mapId, "funworldv3", 11);
            std::string rt = "recon_" + std::to_string(peer.clientId);
            memcpy(accept.reconnectToken, rt.c_str(), std::min(rt.size() + 1, sizeof(accept.reconnectToken)));

            responseType = "join_accept";
            responseData.resize(sizeof(accept));
            memcpy(responseData.data(), &accept, sizeof(accept));
            return true;
        }
        else
        {
            printf("[ICE SERVER] JOIN REJECT client=%u (invalid token)\n", peer.clientId);
            MimitaNet::JoinRejectPacket reject{};
            reject.header.magic = MimitaNet::PROTOCOL_MAGIC;
            reject.header.version = MimitaNet::PROTOCOL_VERSION;
            reject.header.type = MimitaNet::PACKET_JOIN_REJECT;
            reject.reason = 2;
            responseType = "join_reject";
            responseData.resize(sizeof(reject));
            memcpy(responseData.data(), &reject, sizeof(reject));
            return true;
        }
    }

    if (!peer.authenticated) return false;

    if (hdr->type == MimitaNet::PACKET_INPUT)
    {
        if (size < sizeof(MimitaNet::InputPacket)) return false;
        auto* input = reinterpret_cast<const MimitaNet::InputPacket*>(data);
        if (input->header.playerId != peer.clientId) return false;

        peer.yaw = input->yaw;
        peer.aimX = input->camForwardX;
        peer.aimY = input->camForwardY;
        peer.aimZ = input->camForwardZ;

        // Apply movement from input
        float dt = 1.0f / 60.0f;
        peer.vx += input->wishX * 20.0f * dt;
        peer.vz += input->wishY * 20.0f * dt;

        if ((input->stateFlags & MimitaNet::NET_STATE_JUMPING) && peer.py <= 5.01f)
            peer.vy = 12.0f;
        if (input->stateFlags & MimitaNet::NET_STATE_DASHING) {
            peer.vx += cosf(peer.yaw) * 30.0f * dt;
            peer.vz += sinf(peer.yaw) * 30.0f * dt;
        }

        return true;
    }

    if (hdr->type == MimitaNet::PACKET_DISCONNECT)
    {
        responseType = "disconnect";
        return true;
    }

    return false;
}

static void simulatePlayer(IceClientPeer& p, float dt)
{
    if (!p.alive) return;
    p.px += p.vx * dt;
    p.py += p.vy * dt;
    p.pz += p.vz * dt;
    p.vy -= 25.0f * dt;
    if (p.py < 5.0f) { p.py = 5.0f; p.vy = 0; }
    p.vx *= 0.92f;
    p.vz *= 0.92f;
}

int runIceServer(const IceServerOptions& opts)
{
    printf("[ICE SERVER] starting...\n");
    fflush(stdout);

    IceConfiguration iceConfig = loadIceConfig();
    if (opts.disableRelay) iceConfig.turn.password.clear();
    if (!iceConfig.turn.password.empty() && iceConfig.turn.password.empty()) {
        printf("[ICE SERVER] no TURN password\n"); return 1;
    }

    IceAgent hostAgent;
    if (!hostAgent.initialize(iceConfig)) { printf("[ICE SERVER] agent init failed\n"); return 1; }
    if (!hostAgent.gatherCandidates()) { printf("[ICE SERVER] gather failed\n"); return 1; }

    // Wait for gathering
    {
        int w = 0;
        while (w < 15000) {
            std::vector<IceEvent> evs; hostAgent.pollEvents(evs);
            if (hostAgent.state() == IceAgentState::GatheringComplete) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); w += 50;
        }
        if (hostAgent.state() != IceAgentState::GatheringComplete) {
            printf("[ICE SERVER] gather timeout\n"); return 1;
        }
    }

    std::string sessionId = "iceserver_" + std::to_string(GetCurrentProcessId());
    auto hostResult = MimitaNet::coordinatorIceHost(sessionId, hostAgent.localSdp());
    if (!hostResult.ok) { printf("[ICE SERVER] coord register failed\n"); return 1; }

    printf("\n");
    printf("============================================\n");
    printf("  MIMITA ICE SERVER READY\n");
    printf("  Room Code: %s\n", hostResult.roomCode.c_str());
    printf("  Join: mimita.exe --ice-connect %s\n", hostResult.roomCode.c_str());
    printf("============================================\n");
    printf("\n");
    fflush(stdout);

    // Wait for first client
    std::string clientDesc;
    int timeoutMs = opts.timeoutSeconds * 1000;
    int waited = 0;
    while (waited < timeoutMs) {
        auto pollResult = MimitaNet::coordinatorIcePoll(hostResult.roomCode, sessionId);
        if (pollResult.ok && pollResult.status == "client_ready" && !pollResult.clientIceDescription.empty()) {
            clientDesc = pollResult.clientIceDescription;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        waited += 200;
    }
    if (clientDesc.empty()) { printf("[ICE SERVER] no client joined\n"); return 1; }
    if (!hostAgent.setRemoteDescription(clientDesc)) { printf("[ICE SERVER] remote desc failed\n"); return 1; }

    // Wait for connection
    {
        int w = 0; bool connected = false;
        while (w < 20000) {
            std::vector<IceEvent> evs; hostAgent.pollEvents(evs);
            auto s = hostAgent.state();
            if (s == IceAgentState::Connected || s == IceAgentState::Completed) { connected = true; break; }
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); w += 50;
        }
        if (!connected) { printf("[ICE SERVER] connection timeout\n"); return 1; }
    }

    printf("[ICE SERVER] client connected!\n");
    hostAgent.logSelectedPath();

    // Drain any events that arrived during connection
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    std::vector<IceEvent> drain; hostAgent.pollEvents(drain);

    // Register client as peer
    uint32_t nextClientId = 1001;
    auto& peer = gPeers[nextClientId];
    peer.clientId = nextClientId;
    peer.playerEntityId = nextClientId;
    peer.lastPacketMs = GetTickCount64();

    // Game loop
    uint64_t tick = 0;
    uint64_t now = GetTickCount64();
    uint64_t lastSnapshotMs = now;
    uint64_t lastHbMs = now;

    while (waited < timeoutMs)
    {
        now = GetTickCount64();

        // Poll ICE for incoming data
        std::vector<IceEvent> evs;
        hostAgent.pollEvents(evs);
        for (auto& ev : evs)
        {
            if (ev.type == IceEventType::Recv && ev.data.size() >= (int)sizeof(MimitaNet::PacketHeader))
            {
                std::string respType;
                std::vector<char> respData;
                processPeerPacket(peer, ev.data.data(), ev.data.size(), (uint32_t)tick,
                                  hostResult.roomCode, respType, respData);
                if (!respData.empty())
                    hostAgent.send(respData.data(), respData.size());
            }
        }

        // Simulate
        simulatePlayer(peer, 1.0f / 60.0f);
        tick++;

        // Send snapshots at 20 Hz
        if (now - lastSnapshotMs >= 50)
        {
            lastSnapshotMs = now;

            MimitaNet::SnapshotChunkPacket snap{};
            snap.header.magic = MimitaNet::PROTOCOL_MAGIC;
            snap.header.version = MimitaNet::PROTOCOL_VERSION;
            snap.header.type = MimitaNet::PACKET_SNAPSHOT;
            snap.serverTick = (uint32_t)tick;

            int idx = 0;
            for (auto& [id, p] : gPeers) {
                if (!p.authenticated) continue;
                auto& e = snap.entities[idx++];
                e.networkEntityId = p.playerEntityId;
                e.entityType = MimitaNet::ENTITY_PLAYER;
                e.active = p.alive ? 1 : 0;
                e.ownerClientId = p.clientId;
                e.px = p.px; e.py = p.py; e.pz = p.pz;
                e.vx = p.vx; e.vy = p.vy; e.vz = p.vz;
                e.yaw = p.yaw;
                e.aimX = p.aimX; e.aimY = p.aimY; e.aimZ = p.aimZ;
                e.health = p.health;
            }
            snap.entityCount = (uint16_t)idx;
            snap.payloadBytes = (uint16_t)(idx * sizeof(MimitaNet::CompactEntityData));

            size_t snapSize = sizeof(MimitaNet::PacketHeader) + 12
                            + idx * sizeof(MimitaNet::CompactEntityData);
            hostAgent.send(&snap, snapSize);
        }

        // Heartbeat coordinator every 15s
        if (now - lastHbMs >= 15000) {
            lastHbMs = now;
            MimitaNet::coordinatorIcePoll(hostResult.roomCode, sessionId);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        waited += 10;
    }

    printf("[ICE SERVER] shutting down\n");
    MimitaNet::coordinatorIceDone(hostResult.roomCode);
    hostAgent.shutdown();
    return 0;
}

int runIceClient(const std::string& roomCode, const IceServerOptions& opts)
{
    printf("[ICE CLIENT] connecting to room %s...\n", roomCode.c_str());
    fflush(stdout);

    IceConfiguration iceConfig = loadIceConfig();
    if (opts.disableRelay) iceConfig.turn.password.clear();

    IceAgent agent;
    if (!agent.initialize(iceConfig)) { printf("[ICE CLIENT] init failed\n"); return 1; }
    if (!agent.gatherCandidates()) { printf("[ICE CLIENT] gather failed\n"); return 1; }

    {
        int w = 0;
        while (w < 15000) {
            std::vector<IceEvent> evs; agent.pollEvents(evs);
            if (agent.state() == IceAgentState::GatheringComplete) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); w += 50;
        }
    }

    std::string sessionId = "iceclient_" + std::to_string(GetCurrentProcessId());
    auto joinResult = MimitaNet::coordinatorIceJoin(roomCode, sessionId, agent.localSdp());
    if (!joinResult.ok || joinResult.hostIceDescription.empty()) {
        printf("[ICE CLIENT] join failed\n"); return 1;
    }

    if (!agent.setRemoteDescription(joinResult.hostIceDescription)) {
        printf("[ICE CLIENT] remote desc failed\n"); return 1;
    }

    {
        int w = 0; bool connected = false;
        while (w < 20000) {
            std::vector<IceEvent> evs; agent.pollEvents(evs);
            auto s = agent.state();
            if (s == IceAgentState::Connected || s == IceAgentState::Completed) { connected = true; break; }
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); w += 50;
        }
        if (!connected) { printf("[ICE CLIENT] connection timeout\n"); return 1; }
    }

    printf("[ICE CLIENT] connected!\n");
    agent.logSelectedPath();

    // Drain pending events
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::vector<IceEvent> drain; agent.pollEvents(drain);

    // Send JoinRequest
    MimitaNet::JoinRequestPacket joinReq{};
    joinReq.header.magic = MimitaNet::PROTOCOL_MAGIC;
    joinReq.header.version = MimitaNet::PROTOCOL_VERSION;
    joinReq.header.type = MimitaNet::PACKET_JOIN_REQUEST;
    memcpy(joinReq.joinToken, joinResult.joinToken.c_str(),
           std::min(joinResult.joinToken.size(), sizeof(joinReq.joinToken)));
    const char* pname = "IcePlayer";
    memcpy(joinReq.name, pname, strlen(pname) + 1);
    if (!agent.send(&joinReq, sizeof(joinReq))) {
        printf("[ICE CLIENT] join send failed\n"); return 1;
    }

    // Wait for JoinAccept
    std::string acceptData;
    int timeoutMs = opts.timeoutSeconds * 1000, waited = 0;
    while (waited < timeoutMs) {
        std::vector<IceEvent> evs; agent.pollEvents(evs);
        for (auto& ev : evs) {
            if (ev.type == IceEventType::Recv)
                acceptData.assign(ev.data.data(), ev.data.size());
        }
        if (!acceptData.empty()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); waited += 50;
    }
    if (acceptData.empty()) { printf("[ICE CLIENT] no join response\n"); return 1; }

    // Check for reject
    if (acceptData.size() >= sizeof(MimitaNet::PacketHeader)) {
        auto* h = reinterpret_cast<const MimitaNet::PacketHeader*>(acceptData.data());
        if (h->type == MimitaNet::PACKET_JOIN_REJECT) {
            printf("[ICE CLIENT] join rejected\n"); return 1;
        }
    }
    if (acceptData.size() < sizeof(MimitaNet::JoinAcceptPacket)) {
        printf("[ICE CLIENT] bad accept size\n"); return 1;
    }
    auto* accept = reinterpret_cast<const MimitaNet::JoinAcceptPacket*>(acceptData.data());
    if (accept->header.magic != MimitaNet::PROTOCOL_MAGIC) {
        printf("[ICE CLIENT] bad accept header\n"); return 1;
    }
    uint32_t myId = accept->assignedPlayerId;
    printf("[ICE CLIENT] joined as player %u\n", myId);

    // Game loop: send inputs, receive snapshots
    int step = 0;
    while (step < 6000 && waited < timeoutMs) {
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 Hz

        MimitaNet::InputPacket input{};
        input.header.magic = MimitaNet::PROTOCOL_MAGIC;
        input.header.version = MimitaNet::PROTOCOL_VERSION;
        input.header.type = MimitaNet::PACKET_INPUT;
        input.header.playerId = myId;
        input.header.tick = (uint32_t)step;
        input.wishX = (step % 200 < 100) ? 5.0f : -5.0f;
        input.yaw = (float)step * 0.2f;
        input.stateFlags = (step % 120 == 0) ? (uint16_t)MimitaNet::NET_STATE_DASHING : 0;
        agent.send(&input, sizeof(input));

        std::vector<IceEvent> evs; agent.pollEvents(evs);
        for (auto& ev : evs) {
            if (ev.type == IceEventType::Recv && ev.data.size() >= (int)sizeof(MimitaNet::PacketHeader)) {
                auto* h = reinterpret_cast<const MimitaNet::PacketHeader*>(ev.data.data());
                if (h->magic == MimitaNet::PROTOCOL_MAGIC && h->type == MimitaNet::PACKET_SNAPSHOT) {
                    auto* snap = reinterpret_cast<const MimitaNet::SnapshotChunkPacket*>(ev.data.data());
                    printf("[ICE CLIENT SNAP] tick=%u entities=%d\n", snap->serverTick, snap->entityCount);
                    for (uint16_t ei = 0; ei < snap->entityCount; ++ei) {
                        auto& e = snap->entities[ei];
                        printf("  entity=%u pos=(%.1f,%.1f,%.1f) yaw=%.1f hp=%d\n",
                               e.networkEntityId, e.px, e.py, e.pz, e.yaw, e.health);
                    }
                    fflush(stdout);
                }
            }
        }

        step++;
        waited += 16;
    }

    printf("[ICE CLIENT] done (steps=%d)\n", step);
    agent.shutdown();
    return 0;
}
