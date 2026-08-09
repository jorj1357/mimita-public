// 07 21 2026, 16 50
/* purpose
* Owns ICE CLI debug server/client entry points and full-server client probes.
* Exercises libjuice transport with Mimita network packet structures and lifecycle packets.
* Keeps raw ICE diagnostics separate from the authoritative server gameplay path.
* Does NOT own production server movement validation or matchmaking policy.
* Does NOT render gameplay, change movement formulas, or run weapon migration.
* Does NOT make ICE connected state create authoritative gameplay players.
*/

#include "network/ice/ice-server.h"
#include "network/ice/ice-agent.h"
#include "network/ice/ice-config.h"
#include "network/coordinator-client.h"
#include "network/packets.h"
#include "network/movement-validation.h"
#include "network/snapshot-chunks.h"
#include "network/test-events.h"
#include "network/badconn/badconn.h"
#include "debug/debug-log.h"

#include <algorithm>
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

namespace {

class IceAgentBadconnTransport final : public IGameTransport
{
public:
    explicit IceAgentBadconnTransport(IceAgent& agent)
        : mAgent(agent)
    {
    }

    bool send(const void* data, size_t size) override
    {
        return mAgent.send(data, size);
    }

    void poll(std::vector<ReceivedPacket>& out) override
    {
        (void)out;
    }

    bool connected() const override
    {
        return true;
    }

    void close() override
    {
    }

private:
    IceAgent& mAgent;
};

bool sendIcePacket(IceAgent& agent, const void* data, size_t size)
{
    if (badconn::active() && badconn::processOutgoing(data, size))
        return true;
    return agent.send(data, size);
}

void tickIceBadconn(IceAgent& agent)
{
    if (!badconn::active())
        return;
    IceAgentBadconnTransport transport(agent);
    badconn::tick(&transport);
}

void processIceBadconnIncoming(std::vector<IceEvent>& events)
{
    if (!badconn::active())
        return;

    std::vector<IceEvent> kept;
    std::vector<ReceivedPacket> packets;
    kept.reserve(events.size());
    packets.reserve(events.size());
    for (IceEvent& event : events)
    {
        if (event.type != IceEventType::Recv)
        {
            kept.push_back(std::move(event));
            continue;
        }
        ReceivedPacket packet;
        packet.bytes.assign(event.data.begin(), event.data.end());
        packet.receivedAtMs = GetTickCount64();
        packets.push_back(std::move(packet));
    }

    badconn::processIncoming(packets);
    for (ReceivedPacket& packet : packets)
    {
        IceEvent event{};
        event.type = IceEventType::Recv;
        event.data.assign(packet.bytes.begin(), packet.bytes.end());
        kept.push_back(std::move(event));
    }
    events.swap(kept);
}

} // namespace

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
                    sendIcePacket(hostAgent, respData.data(), respData.size());
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
            sendIcePacket(hostAgent, &snap, snapSize);
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

    if (!opts.badconnPreset.empty())
    {
        badconn::loadConfig(badconn::configPath());
        if (opts.badconnPreset == "0")
            badconn::disable();
        else if (!badconn::activatePreset(opts.badconnPreset))
        {
            printf("[ICE CLIENT] badconn preset %s not found\n",
                   opts.badconnPreset.c_str());
            return 1;
        }
    }

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
    auto beginJoin = MimitaNet::coordinatorIceBeginJoin(roomCode, sessionId, agent.localSdp());
    if (!beginJoin.ok || beginJoin.requestId.empty()) {
        printf("[ICE CLIENT] begin join failed error=%s\n",
               beginJoin.errorCode.c_str()); return 1;
    }

    std::string hostAnswer;
    {
        int w = 0;
        while (hostAnswer.empty() && w < 30000) {
            auto poll = MimitaNet::coordinatorIceClientPoll(roomCode, beginJoin.requestId);
            if (poll.ok && !poll.hostIceDescription.empty()) {
                hostAnswer = poll.hostIceDescription;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            w += 100;
        }
    }
    if (hostAnswer.empty()) {
        printf("[ICE CLIENT] host answer timeout req=%s\n",
               beginJoin.requestId.substr(0, 12).c_str()); return 1;
    }

    if (!agent.setRemoteDescription(hostAnswer)) {
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
    badconn::noteConnectionEstablished();
    MimitaNet::emitTestEvent("ice_connected",
        "\"role\":\"client\",\"clientIndex\":" + std::to_string(opts.clientIndex));

    // Drain pending events
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::vector<IceEvent> drain; agent.pollEvents(drain);

    const bool reconnectMode = !opts.reconnectToken.empty();
    if (reconnectMode)
    {
        MimitaNet::ReconnectRequestPacket req{};
        req.header.magic = MimitaNet::PROTOCOL_MAGIC;
        req.header.version = MimitaNet::PROTOCOL_VERSION;
        req.header.type = MimitaNet::PACKET_RECONNECT_REQUEST;
        std::strncpy(req.reconnectToken, opts.reconnectToken.c_str(),
                     sizeof(req.reconnectToken) - 1);
        if (!sendIcePacket(agent, &req, sizeof(req))) {
            printf("[ICE CLIENT] reconnect send failed\n"); return 1;
        }
        MimitaNet::emitTestEvent("reconnect_request_sent",
            "\"clientIndex\":" + std::to_string(opts.clientIndex));
    }
    else
    {
        MimitaNet::JoinRequestPacket joinReq{};
        joinReq.header.magic = MimitaNet::PROTOCOL_MAGIC;
        joinReq.header.version = MimitaNet::PROTOCOL_VERSION;
        joinReq.header.type = MimitaNet::PACKET_JOIN_REQUEST;
        std::strncpy(joinReq.joinToken, beginJoin.joinToken.c_str(),
                     sizeof(joinReq.joinToken) - 1);
        std::string pname = opts.clientIndex > 0
            ? "IcePlayer" + std::to_string(opts.clientIndex)
            : "IcePlayer";
        std::strncpy(joinReq.name, pname.c_str(), sizeof(joinReq.name) - 1);
        if (!sendIcePacket(agent, &joinReq, sizeof(joinReq))) {
            printf("[ICE CLIENT] join send failed\n"); return 1;
        }
        MimitaNet::emitTestEvent("join_request_sent",
            "\"clientIndex\":" + std::to_string(opts.clientIndex));
    }

    // Wait for JoinAccept or ReconnectAccept from the normal server decoder.
    std::string acceptData;
    int timeoutMs = opts.timeoutSeconds * 1000, waited = 0;
    while (waited < timeoutMs) {
        std::vector<IceEvent> evs; agent.pollEvents(evs);
        processIceBadconnIncoming(evs);
        tickIceBadconn(agent);
        for (auto& ev : evs) {
            if (ev.type != IceEventType::Recv ||
                ev.data.size() < (int)sizeof(MimitaNet::PacketHeader))
                continue;
            auto* header =
                reinterpret_cast<const MimitaNet::PacketHeader*>(ev.data.data());
            if (header->magic != MimitaNet::PROTOCOL_MAGIC ||
                header->version != MimitaNet::PROTOCOL_VERSION)
                continue;
            const uint8_t expected =
                reconnectMode ? MimitaNet::PACKET_RECONNECT_ACCEPT
                              : MimitaNet::PACKET_JOIN_ACCEPT;
            if (header->type == expected ||
                header->type == MimitaNet::PACKET_JOIN_REJECT)
            {
                acceptData.assign(ev.data.data(), ev.data.size());
                break;
            }
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

    uint32_t myId = 0;
    uint32_t movementSequence = 1;
    uint32_t spawnGeneration = 0;
    uint32_t transformEpoch = 0;
    std::string reconnectToken;
    bool gameplayActive = false;
    if (reconnectMode)
    {
        if (acceptData.size() < sizeof(MimitaNet::ReconnectAcceptPacket)) {
            printf("[ICE CLIENT] bad reconnect accept size\n"); return 1;
        }
        auto* accept = reinterpret_cast<const MimitaNet::ReconnectAcceptPacket*>(acceptData.data());
        if (accept->header.magic != MimitaNet::PROTOCOL_MAGIC ||
            accept->header.type != MimitaNet::PACKET_RECONNECT_ACCEPT) {
            printf("[ICE CLIENT] bad reconnect accept header\n"); return 1;
        }
        myId = accept->assignedPlayerId;
        spawnGeneration = accept->spawnGeneration;
        transformEpoch = accept->header.transformEpoch;
        reconnectToken = accept->reconnectToken;
        gameplayActive = true;
        printf("[ICE CLIENT] reconnected as player %u\n", myId);
        MimitaNet::emitTestEvent("reconnect_confirmed",
            "\"clientIndex\":" + std::to_string(opts.clientIndex) +
            ",\"playerId\":" + std::to_string(myId) +
            ",\"spawnGeneration\":" + std::to_string(spawnGeneration) +
            ",\"transformEpoch\":" + std::to_string(transformEpoch) +
            ",\"reconnectToken\":\"" + MimitaNet::testEventJsonEscape(reconnectToken) + "\"");
    }
    else
    {
        if (acceptData.size() < sizeof(MimitaNet::JoinAcceptPacket)) {
            printf("[ICE CLIENT] bad accept size\n"); return 1;
        }
        auto* accept = reinterpret_cast<const MimitaNet::JoinAcceptPacket*>(acceptData.data());
        if (accept->header.magic != MimitaNet::PROTOCOL_MAGIC ||
            accept->header.type != MimitaNet::PACKET_JOIN_ACCEPT) {
            printf("[ICE CLIENT] bad accept header\n"); return 1;
        }
        myId = accept->assignedPlayerId;
        transformEpoch = accept->header.transformEpoch;
        reconnectToken = accept->reconnectToken;
        printf("[ICE CLIENT] joined as player %u\n", myId);
        MimitaNet::emitTestEvent("join_accepted",
            "\"clientIndex\":" + std::to_string(opts.clientIndex) +
            ",\"playerId\":" + std::to_string(myId) +
            ",\"protocolVersion\":" + std::to_string(MimitaNet::PROTOCOL_VERSION) +
            ",\"map\":\"" + MimitaNet::testEventJsonEscape(accept->mapId) + "\"" +
            ",\"reconnectToken\":\"" + MimitaNet::testEventJsonEscape(reconnectToken) + "\"");

        MimitaNet::ClientMapReadyPacket ready{};
        ready.header.magic = MimitaNet::PROTOCOL_MAGIC;
        ready.header.version = MimitaNet::PROTOCOL_VERSION;
        ready.header.type = MimitaNet::PACKET_CLIENT_MAP_READY;
        ready.header.playerId = myId;
        ready.header.transformEpoch = transformEpoch;
        ready.assignedPlayerId = myId;
        std::strncpy(ready.mapId, accept->mapId, sizeof(ready.mapId) - 1);
        if (!sendIcePacket(agent, &ready, sizeof(ready))) {
            printf("[ICE CLIENT] map-ready send failed\n"); return 1;
        }
        MimitaNet::emitTestEvent("map_ready_sent",
            "\"clientIndex\":" + std::to_string(opts.clientIndex) +
            ",\"playerId\":" + std::to_string(myId) +
            ",\"map\":\"" + MimitaNet::testEventJsonEscape(ready.mapId) + "\"");
    }

    float clientPx = 1.0f + (float)std::max(0, opts.clientIndex - 1) * 1.5f;
    float clientPy = 5.0f;
    float clientPz = 30.0f;
    float clientVx = 0.0f;
    float clientVy = 0.0f;
    float clientVz = 0.0f;
    int currentHealth = 100;
    bool haveLocalSnapshot = false;
    bool remoteSnapshotReported = false;
    bool movementReported = false;
    bool postRespawnMovementReported = false;
    bool postReconnectMovementReported = false;
    bool deathPending = false;
    bool deathConfirmed = false;
    uint32_t deathSpawnGeneration = 0;
    uint16_t respawnSerial = 0;
    uint16_t pendingRespawnSerial = 0;
    int completedDeathCycles = 0;
    uint64_t nextDeathAtMs = GetTickCount64() + 800;

    // Game loop: receive lifecycle/snapshots and send normal InputPackets.
    int step = 0;
    while (step < 6000 && waited < timeoutMs) {
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 Hz

        std::vector<IceEvent> evs; agent.pollEvents(evs);
        processIceBadconnIncoming(evs);
        tickIceBadconn(agent);
        for (auto& ev : evs) {
            if (ev.type != IceEventType::Recv ||
                ev.data.size() < (int)sizeof(MimitaNet::PacketHeader))
                continue;

            auto* h = reinterpret_cast<const MimitaNet::PacketHeader*>(ev.data.data());
            if (h->magic != MimitaNet::PROTOCOL_MAGIC ||
                h->version != MimitaNet::PROTOCOL_VERSION)
                continue;

            if (h->type == MimitaNet::PACKET_PLAYER_RESPAWNED &&
                ev.data.size() >= (int)sizeof(MimitaNet::PlayerRespawnedPacket))
            {
                auto* spawn =
                    reinterpret_cast<const MimitaNet::PlayerRespawnedPacket*>(ev.data.data());
                const bool deathCycleSpawn =
                    deathConfirmed && spawn->spawnGeneration != deathSpawnGeneration;
                spawnGeneration = spawn->spawnGeneration;
                transformEpoch = spawn->transformEpoch;
                currentHealth = spawn->health;
                movementSequence = 1;
                gameplayActive = false;

                // The spawn sync is delivered over the reliable-event transport;
                // acknowledge it so the server stops retransmitting.
                if (spawn->eventId != 0)
                {
                    MimitaNet::ReliableEventAckPacket evAck{};
                    evAck.header.magic = MimitaNet::PROTOCOL_MAGIC;
                    evAck.header.version = MimitaNet::PROTOCOL_VERSION;
                    evAck.header.type = MimitaNet::PACKET_RELIABLE_EVENT_ACK;
                    evAck.header.playerId = myId;
                    evAck.eventId = spawn->eventId;
                    evAck.eventSessionId = spawn->eventSessionId;
                    sendIcePacket(agent, &evAck, sizeof(evAck));
                }

                MimitaNet::emitTestEvent("spawn_sent",
                    "\"clientIndex\":" + std::to_string(opts.clientIndex) +
                    ",\"playerId\":" + std::to_string(myId) +
                    ",\"spawnGeneration\":" + std::to_string(spawnGeneration) +
                    ",\"transformEpoch\":" + std::to_string(transformEpoch) +
                    ",\"health\":" + std::to_string(currentHealth));

                MimitaNet::SpawnAckPacket ack{};
                ack.header.magic = MimitaNet::PROTOCOL_MAGIC;
                ack.header.version = MimitaNet::PROTOCOL_VERSION;
                ack.header.type = MimitaNet::PACKET_SPAWN_ACK;
                ack.header.playerId = myId;
                ack.header.transformEpoch = transformEpoch;
                ack.spawnGeneration = spawnGeneration;
                ack.transformEpoch = transformEpoch;
                sendIcePacket(agent, &ack, sizeof(ack));
                MimitaNet::emitTestEvent("spawn_ack",
                    "\"clientIndex\":" + std::to_string(opts.clientIndex) +
                    ",\"playerId\":" + std::to_string(myId) +
                    ",\"spawnGeneration\":" + std::to_string(spawnGeneration) +
                    ",\"transformEpoch\":" + std::to_string(transformEpoch));

                if (deathCycleSpawn)
                {
                    ++completedDeathCycles;
                    deathPending = false;
                    deathConfirmed = false;
                    pendingRespawnSerial = 0;
                    nextDeathAtMs = GetTickCount64() + 250;
                    MimitaNet::emitTestEvent("respawn_confirmed",
                        "\"clientIndex\":" + std::to_string(opts.clientIndex) +
                        ",\"playerId\":" + std::to_string(myId) +
                        ",\"cycle\":" + std::to_string(completedDeathCycles) +
                        ",\"spawnGeneration\":" + std::to_string(spawnGeneration) +
                        ",\"transformEpoch\":" + std::to_string(transformEpoch));
                }
            }
            else if (h->type == MimitaNet::PACKET_SPAWN_ACTIVATED &&
                     ev.data.size() >= (int)sizeof(MimitaNet::SpawnActivatedPacket))
            {
                auto* act =
                    reinterpret_cast<const MimitaNet::SpawnActivatedPacket*>(ev.data.data());
                if (act->spawnGeneration == spawnGeneration)
                {
                    gameplayActive = true;
                    MimitaNet::emitTestEvent("spawn_activated",
                        "\"clientIndex\":" + std::to_string(opts.clientIndex) +
                        ",\"playerId\":" + std::to_string(myId) +
                        ",\"spawnGeneration\":" + std::to_string(act->spawnGeneration) +
                        ",\"transformEpoch\":" + std::to_string(act->transformEpoch));
                }
            }
            else if (h->type == MimitaNet::PACKET_SNAPSHOT)
            {
                MimitaNet::SnapshotChunkPacket snap{};
                std::string parseError;
                if (!MimitaNet::parseSnapshotChunk(ev.data.data(), ev.data.size(),
                                                   snap, &parseError))
                    continue;
                printf("[ICE CLIENT SNAP] tick=%u entities=%d\n",
                       snap.serverTick, snap.entityCount);
                for (uint16_t ei = 0; ei < snap.entityCount; ++ei) {
                    auto& e = snap.entities[ei];
                    printf("  entity=%u owner=%u pos=(%.1f,%.1f,%.1f) yaw=%.1f hp=%d gen=%u epoch=%u\n",
                           e.networkEntityId, e.ownerClientId,
                           e.px, e.py, e.pz, e.yaw, e.health,
                           e.spawnGeneration, (unsigned)e.transformEpoch);
                    if (e.entityType == MimitaNet::ENTITY_PLAYER &&
                        e.ownerClientId == myId)
                    {
                        haveLocalSnapshot = true;
                        clientPx = e.px;
                        clientPy = e.py;
                        clientPz = e.pz;
                        clientVx = e.vx;
                        clientVy = e.vy;
                        clientVz = e.vz;
                        currentHealth = e.health;
                        if (e.spawnGeneration != 0)
                            spawnGeneration = e.spawnGeneration;
                        if (e.transformEpoch != 0)
                            transformEpoch = e.transformEpoch;
                        if (deathPending && !deathConfirmed && e.health <= 0)
                        {
                            deathConfirmed = true;
                            pendingRespawnSerial = ++respawnSerial;
                            MimitaNet::emitTestEvent("death_confirmed",
                                "\"clientIndex\":" + std::to_string(opts.clientIndex) +
                                ",\"playerId\":" + std::to_string(myId) +
                                ",\"cycle\":" + std::to_string(completedDeathCycles + 1) +
                                ",\"spawnGeneration\":" + std::to_string(deathSpawnGeneration));
                        }
                    }
                    else if (e.entityType == MimitaNet::ENTITY_PLAYER &&
                             e.ownerClientId != 0 &&
                             e.ownerClientId != myId &&
                             !remoteSnapshotReported)
                    {
                        remoteSnapshotReported = true;
                        MimitaNet::emitTestEvent("remote_snapshot_received",
                            "\"clientIndex\":" + std::to_string(opts.clientIndex) +
                            ",\"playerId\":" + std::to_string(myId) +
                            ",\"remotePlayerId\":" + std::to_string(e.ownerClientId) +
                            ",\"serverTick\":" + std::to_string(snap.serverTick));
                    }
                }
                MimitaNet::emitTestEvent("snapshot_received",
                    "\"clientIndex\":" + std::to_string(opts.clientIndex) +
                    ",\"playerId\":" + std::to_string(myId) +
                    ",\"serverTick\":" + std::to_string(snap.serverTick) +
                    ",\"entities\":" + std::to_string(snap.entityCount));
                fflush(stdout);
            }
        }

        if (gameplayActive && haveLocalSnapshot &&
            opts.deathRespawnCycles > 0 &&
            completedDeathCycles < opts.deathRespawnCycles &&
            !deathPending &&
            GetTickCount64() >= nextDeathAtMs)
        {
            MimitaNet::ExplodeRequestPacket explode{};
            explode.header.magic = MimitaNet::PROTOCOL_MAGIC;
            explode.header.version = MimitaNet::PROTOCOL_VERSION;
            explode.header.type = MimitaNet::PACKET_EXPLODE_REQUEST;
            explode.header.playerId = myId;
            explode.header.tick = (uint32_t)step;
            explode.header.transformEpoch = transformEpoch;
            if (sendIcePacket(agent, &explode, sizeof(explode)))
            {
                deathPending = true;
                deathConfirmed = false;
                deathSpawnGeneration = spawnGeneration;
                MimitaNet::emitTestEvent("death_requested",
                    "\"clientIndex\":" + std::to_string(opts.clientIndex) +
                    ",\"playerId\":" + std::to_string(myId) +
                    ",\"cycle\":" + std::to_string(completedDeathCycles + 1) +
                    ",\"spawnGeneration\":" + std::to_string(spawnGeneration));
            }
        }

        if (myId != 0 && (gameplayActive || pendingRespawnSerial != 0))
        {
            const bool dashPressed = (step % 90) == 10;
            const bool downDashPressed = (step % 120) == 20;
            const bool freezeHeld = (step % 150) < 10;
            const bool jumpHeld = (step % 75) < 6;

            MimitaNet::InputPacket input{};
            input.header.magic = MimitaNet::PROTOCOL_MAGIC;
            input.header.version = MimitaNet::PROTOCOL_VERSION;
            input.header.type = MimitaNet::PACKET_INPUT;
            input.header.playerId = myId;
            input.header.tick = (uint32_t)step;
            input.header.transformEpoch = transformEpoch;
            input.wishX = (step % 200 < 100) ? 1.0f : -1.0f;
            input.wishY = (step % 260 < 130) ? 0.2f : -0.2f;
            input.camForwardX = 1.0f;
            input.camForwardY = 0.0f;
            input.camForwardZ = 0.0f;
            input.yaw = (float)step * 0.2f;
            input.clientPx = clientPx;
            input.clientPy = clientPy;
            input.clientPz = clientPz;
            input.clientVx = clientVx;
            input.clientVy = clientVy;
            input.clientVz = clientVz;
            input.sizeScale = 1.0f;
            input.transformEpoch = transformEpoch;
            input.spawnGeneration = spawnGeneration;
            input.respawnSerial = pendingRespawnSerial;
            input.movementSequence = movementSequence++;
            input.clientSimulationTick = (uint64_t)step;
            input.stateFlags =
                (uint16_t)(MimitaNet::NET_STATE_WALKING |
                (jumpHeld ? MimitaNet::NET_STATE_JUMPING : 0) |
                (dashPressed ? MimitaNet::NET_STATE_DASHING : 0) |
                (downDashPressed ? MimitaNet::NET_STATE_DOWN_DASHING : 0) |
                (freezeHeld ? MimitaNet::NET_STATE_FREEZING : 0));
            input.movementFlags =
                MimitaNet::MOVEMENT_REPORT_DASH_AVAILABLE |
                MimitaNet::MOVEMENT_REPORT_DOWN_DASH_AVAILABLE |
                MimitaNet::MOVEMENT_REPORT_FREEZE_AVAILABLE |
                MimitaNet::MOVEMENT_REPORT_GROUND_RETURN_AVAILABLE |
                (jumpHeld ? MimitaNet::MOVEMENT_REPORT_JUMP_HELD : 0u) |
                (dashPressed ? MimitaNet::MOVEMENT_REPORT_DASH_PRESSED : 0u) |
                (downDashPressed ? MimitaNet::MOVEMENT_REPORT_DOWN_DASH_PRESSED : 0u) |
                (freezeHeld ? MimitaNet::MOVEMENT_REPORT_FREEZE_HELD : 0u);
            sendIcePacket(agent, &input, sizeof(input));

            if (!movementReported)
            {
                movementReported = true;
                MimitaNet::emitTestEvent("movement_sent",
                    "\"clientIndex\":" + std::to_string(opts.clientIndex) +
                    ",\"playerId\":" + std::to_string(myId) +
                    ",\"spawnGeneration\":" + std::to_string(spawnGeneration) +
                    ",\"transformEpoch\":" + std::to_string(transformEpoch));
            }
            if (completedDeathCycles > 0 && gameplayActive &&
                !postRespawnMovementReported)
            {
                postRespawnMovementReported = true;
                MimitaNet::emitTestEvent("post_respawn_movement",
                    "\"clientIndex\":" + std::to_string(opts.clientIndex) +
                    ",\"playerId\":" + std::to_string(myId) +
                    ",\"cycle\":" + std::to_string(completedDeathCycles));
            }
            if (reconnectMode && gameplayActive && !postReconnectMovementReported)
            {
                postReconnectMovementReported = true;
                MimitaNet::emitTestEvent("post_reconnect_movement",
                    "\"clientIndex\":" + std::to_string(opts.clientIndex) +
                    ",\"playerId\":" + std::to_string(myId));
            }
        }

        if (reconnectMode && postReconnectMovementReported)
            break;
        if (!reconnectMode && opts.deathRespawnCycles > 0 &&
            completedDeathCycles >= opts.deathRespawnCycles &&
            postRespawnMovementReported)
            break;
        if (!reconnectMode && opts.deathRespawnCycles == 0 &&
            remoteSnapshotReported && movementReported && step > 120)
            break;

        step++;
        waited += 16;
    }

    printf("[ICE CLIENT] done (steps=%d deaths=%d reconnect=%d health=%d)\n",
           step, completedDeathCycles, (int)reconnectMode, currentHealth);
    agent.shutdown();
    return 0;
}
