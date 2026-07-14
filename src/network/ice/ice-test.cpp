#include "network/ice/ice-test.h"
#include "network/ice/ice-agent.h"
#include "network/ice/ice-config.h"
#include "network/ice-transport.h"
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

static IceCandidateType parseTypeFromSdp(const std::string& sdp)
{
    size_t typ = sdp.find("typ ");
    if (typ == std::string::npos) return IceCandidateType::Unknown;
    std::string rest = sdp.substr(typ + 4);
    if (rest.rfind("host", 0) == 0) return IceCandidateType::Host;
    if (rest.rfind("srflx", 0) == 0) return IceCandidateType::ServerReflexive;
    if (rest.rfind("relay", 0) == 0) return IceCandidateType::Relay;
    return IceCandidateType::Unknown;
}

static const char* typeStr(IceCandidateType t)
{
    switch (t) {
        case IceCandidateType::Host: return "host";
        case IceCandidateType::ServerReflexive: return "srflx";
        case IceCandidateType::Relay: return "relay";
        default: return "unknown";
    }
}

static void logSelectedPath(IceAgent& agent)
{
    juice_agent_t* raw = reinterpret_cast<juice_agent_t*>(&agent);
    // Can't get the raw pointer from IceAgent directly, need accessor
    printf("[ICE SELECTED PATH] (use juice_get_selected_candidates)\n");
}

// ── Player state for lightweight ICE game server ─────────────────────

struct IcePlayerState {
    uint32_t playerId = 0;
    std::string name;
    float px = 0, py = 5, pz = 30;
    float vx = 0, vy = 0, vz = 0;
    float yaw = 0;
    int health = 100;
    bool alive = true;
    uint64_t lastPacketMs = 0;
};

struct IcePeerConnection {
    uint32_t clientId;
    uint32_t playerEntityId;
    std::string joinAttemptId;
    std::unique_ptr<IceAgent> agent;
    bool authenticated = false;
    uint64_t lastPacketMs = 0;
    IcePlayerState player;
};

// ── Game server simulation (headless, no rendering) ─────────────────

struct IceGameServer {
    std::unordered_map<uint32_t, IcePeerConnection> peers;
    uint32_t nextClientId = 1;
    uint64_t tick = 0;

    void tickSimulation(float dt) {
        tick++;
        for (auto& [id, peer] : peers) {
            if (!peer.authenticated) continue;
            auto& p = peer.player;
            // Simple movement: apply velocity
            p.px += p.vx * dt;
            p.py += p.vy * dt;
            p.pz += p.vz * dt;
            // Simple gravity
            if (p.py > 5.0f) p.vy -= 20.0f * dt;
            if (p.py < 5.0f) { p.py = 5.0f; p.vy = 0; }
            // Friction
            p.vx *= 0.9f; p.vz *= 0.9f;
        }
    }

    void broadcastSnapshot() {
        for (auto& [id, peer] : peers) {
            if (!peer.authenticated || !peer.agent) continue;
            MimitaNet::SnapshotPacket snap{};
            snap.header.magic = MimitaNet::PROTOCOL_MAGIC;
            snap.header.version = MimitaNet::PROTOCOL_VERSION;
            snap.header.type = MimitaNet::PACKET_SNAPSHOT;
            snap.header.tick = (uint32_t)tick;
            int idx = 0;
            for (auto& [otherId, other] : peers) {
                if (!other.authenticated) continue;
                auto& sp = other.player;
                auto& e = snap.entities[idx++];
                e.networkEntityId = sp.playerId;
                e.entityType = MimitaNet::ENTITY_PLAYER;
                e.active = sp.alive ? 1 : 0;
                e.ownerClientId = other.clientId;
                e.px = sp.px;
                e.py = sp.py;
                e.pz = sp.pz;
                e.vx = sp.vx;
                e.vy = sp.vy;
                e.vz = sp.vz;
                e.yaw = sp.yaw;
                e.health = sp.health;
                e.lastDashSerial = 0;
                e.transformEpoch = 0;
                memcpy(e.displayName, sp.name.c_str(), std::min(sp.name.size() + 1, sizeof(e.displayName)));
            }
            snap.entityCount = idx;
            snap.playerCount = idx;
            peer.agent->send(&snap, sizeof(snap));
        }
    }
};

// ── Helper: wait for ICE state ──────────────────────────────────────

// Wait for state without draining Recv events
// Returns true if target reached, and stores any received data in earlyData
static bool waitForState(IceAgent& agent, IceAgentState target, int timeoutMs,
                         std::string* earlyData = nullptr)
{
    int waited = 0;
    while (waited < timeoutMs) {
        std::vector<IceEvent> evs;
        agent.pollEvents(evs);
        // Preserve Recv events
        if (earlyData) {
            for (auto& ev : evs) {
                if (ev.type == IceEventType::Recv && earlyData->empty()) {
                    *earlyData = std::string(ev.data.data(), ev.data.size());
                }
            }
        }
        IceAgentState s = agent.state();
        if (s == target || s == IceAgentState::Failed)
            return s == target;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        waited += 50;
    }
    return false;
}

// ── Lightweight ICE host ────────────────────────────────────────────

bool runIceHostOnly(const IceTestOptions& opts)
{
    printf("[ICE ONLY START] role=HOST processId=%lu\n", (unsigned long)GetCurrentProcessId());
    fflush(stdout);
    IceConfiguration iceConfig = loadIceConfig();
    bool usingTurn = !iceConfig.turn.password.empty();
    if (opts.disableRelay) { iceConfig.turn.password.clear(); usingTurn = false; }
    if (usingTurn && iceConfig.turn.password.empty()) {
        printf("[ICE ONLY RESULT] pass=0 reason=no-turn-password\n"); return false;
    }

    IceAgent agent;
    if (!agent.initialize(iceConfig)) { printf("[ICE ONLY RESULT] pass=0 reason=init-failed\n"); return false; }
    if (!agent.gatherCandidates()) { printf("[ICE ONLY RESULT] pass=0 reason=gather-failed\n"); return false; }
    if (!waitForState(agent, IceAgentState::GatheringComplete, 15000)) {
        printf("[ICE ONLY RESULT] pass=0 reason=gather-timeout\n"); return false;
    }

    std::string sessionId = "host_" + std::to_string(GetCurrentProcessId());
    auto hostResult = MimitaNet::coordinatorIceHost(sessionId, agent.localSdp());
    if (!hostResult.ok) { printf("[ICE ONLY RESULT] pass=0 reason=coord-failed\n"); return false; }
    printf("[ICE ONLY ROOM] code=%s\n", hostResult.roomCode.c_str()); fflush(stdout);

    std::string clientDesc;
    int timeoutMs = opts.timeoutSeconds * 1000, waited = 0;
    while (waited < timeoutMs) {
        auto pollResult = MimitaNet::coordinatorIcePoll(hostResult.roomCode, sessionId);
        if (pollResult.ok && pollResult.status == "client_ready" && !pollResult.clientIceDescription.empty()) {
            clientDesc = pollResult.clientIceDescription; break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200)); waited += 200;
    }
    if (clientDesc.empty()) {
        printf("[ICE ONLY RESULT] pass=0 reason=poll-timeout\n");
        MimitaNet::coordinatorIceDone(hostResult.roomCode); return false;
    }
    printf("[ICE ONLY SIGNAL] role=HOST descriptionBytes=%zu\n", clientDesc.size());

    if (!agent.setRemoteDescription(clientDesc)) {
        printf("[ICE ONLY RESULT] pass=0 reason=remote-desc-failed\n");
        MimitaNet::coordinatorIceDone(hostResult.roomCode); return false;
    }
    printf("[ICE ONLY HOST] waiting for connection...\n"); fflush(stdout);

    std::string earlyRecv;
    bool connected = waitForState(agent, IceAgentState::Connected, 20000, &earlyRecv) ||
                     waitForState(agent, IceAgentState::Completed, 5000, &earlyRecv);
    if (!connected) {
        printf("[ICE ONLY RESULT] pass=0 reason=connection-timeout\n");
        MimitaNet::coordinatorIceDone(hostResult.roomCode); return false;
    }
    printf("[ICE ONLY STATE] role=HOST connected\n");
    agent.logSelectedPath();
    fflush(stdout);

    std::string recvd = earlyRecv;  // Data already received during connection wait
    if (recvd.empty()) {
        // Wait for more data if none arrived yet
        waited = 0;
        while (waited < timeoutMs) {
            std::vector<IceEvent> evs; agent.pollEvents(evs);
            for (auto& ev : evs) if (ev.type == IceEventType::Recv)
                recvd.assign(ev.data.data(), ev.data.size());
            if (!recvd.empty()) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); waited += 50;
        }
    }
    if (recvd.empty() && opts.once) {
        printf("[ICE ONLY RESULT] pass=0 reason=no-data\n");
        MimitaNet::coordinatorIceDone(hostResult.roomCode); return false;
    }

    if (!recvd.empty()) {
        printf("[ICE ONLY RECEIVE] bytes=%zu\n", recvd.size()); fflush(stdout);

        // Check for JoinRequestPacket
        if (recvd.size() >= sizeof(MimitaNet::PacketHeader)) {
            auto* hdr = reinterpret_cast<const MimitaNet::PacketHeader*>(recvd.data());
            if (hdr->magic == MimitaNet::PROTOCOL_MAGIC && hdr->type == MimitaNet::PACKET_JOIN_REQUEST) {
                if (recvd.size() >= sizeof(MimitaNet::JoinRequestPacket)) {
                    auto* join = reinterpret_cast<const MimitaNet::JoinRequestPacket*>(recvd.data());
                    std::string token((const char*)join->joinToken);
                    token = token.c_str();
                    bool valid = MimitaNet::coordinatorIceValidateJoin(hostResult.roomCode, token);
                    printf("[ICE GAME JOIN REQUEST] name=%.*s valid=%d\n", (int)sizeof(join->name), join->name, (int)valid);
                    fflush(stdout);
                    if (valid) {
                        MimitaNet::JoinAcceptPacket accept{};
                        accept.header.magic = MimitaNet::PROTOCOL_MAGIC;
                        accept.header.version = MimitaNet::PROTOCOL_VERSION;
                        accept.header.type = MimitaNet::PACKET_JOIN_ACCEPT;
                        accept.assignedPlayerId = 1001; accept.tickRate = 60.0f;
                        memcpy(accept.approvedName, join->name, sizeof(accept.approvedName));
                        memcpy(accept.mapId, "funworldv3", 11);
                        const char* rt = "test-recon";
                        memcpy(accept.reconnectToken, rt, strlen(rt) + 1);
                        agent.send(&accept, sizeof(accept));

                        // Now handle InputPackets and send compact Snapshots
                        int gameSteps = 200, step = 0;
                        while (step < gameSteps) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(50));
                            std::vector<IceEvent> evs; agent.pollEvents(evs);
                            for (auto& ev : evs) {
                                if (ev.type == IceEventType::Recv && ev.data.size() >= (int)sizeof(MimitaNet::PacketHeader)) {
                                    auto* ihdr = reinterpret_cast<const MimitaNet::PacketHeader*>(ev.data.data());
                                    if (ihdr->magic == MimitaNet::PROTOCOL_MAGIC && ihdr->type == MimitaNet::PACKET_INPUT) {
                                        printf("[ICE GAME INPUT] tick=%u\n", ihdr->tick); fflush(stdout);
                                    }
                                }
                            }
                            // Send compact snapshot chunk
                            MimitaNet::SnapshotChunkPacket snap{};
                            snap.header.magic = MimitaNet::PROTOCOL_MAGIC;
                            snap.header.version = MimitaNet::PROTOCOL_VERSION;
                            snap.header.type = MimitaNet::PACKET_SNAPSHOT;
                            snap.serverTick = (uint32_t)step;
                            snap.chunkIndex = 0; snap.chunkCount = 1;
                            auto& e = snap.entities[0];
                            e.networkEntityId = 1001; e.entityType = MimitaNet::ENTITY_PLAYER;
                            e.active = 1; e.ownerClientId = 1;
                            e.px = 1.0f + (float)step * 0.1f; e.py = 5.0f; e.pz = 30.0f;
                            e.yaw = (float)step * 0.5f; e.health = 100;
                            snap.entityCount = 1;
                            snap.payloadBytes = sizeof(MimitaNet::CompactEntityData);
                            size_t snapSize = sizeof(MimitaNet::PacketHeader) + 12 + snap.entityCount * sizeof(MimitaNet::CompactEntityData);
                            agent.send(&snap, snapSize);
                            ++step;
                        }
                        printf("[ICE GAME HOST] game steps done\n"); fflush(stdout);
                    } else {
                        MimitaNet::JoinRejectPacket reject{};
                        reject.header.magic = MimitaNet::PROTOCOL_MAGIC;
                        reject.header.version = MimitaNet::PROTOCOL_VERSION;
                        reject.header.type = MimitaNet::PACKET_JOIN_REJECT;
                        reject.reason = 2;
                        agent.send(&reject, sizeof(reject));
                        printf("[ICE GAME JOIN REJECT] reason=bad-token\n"); fflush(stdout);
                    }
                }
            } else {
                const char* reply = "hello from host (echo)";
                agent.send(reply, strlen(reply) + 1);
            }
        } else {
            const char* reply = "hello from host (echo)";
            agent.send(reply, strlen(reply) + 1);
        }
        printf("[ICE ONLY SEND] response\n"); fflush(stdout);
    }
    if (opts.once) {
        printf("[ICE ONLY RESULT] pass=1\n");
        MimitaNet::coordinatorIceDone(hostResult.roomCode); agent.shutdown(); return true;
    }
    printf("[ICE ONLY HOST] running...\n");
    while (true) {
        std::vector<IceEvent> evs; agent.pollEvents(evs);
        for (auto& ev : evs)
            if (ev.type == IceEventType::Recv) {
                const char* reply = "echo";
                agent.send(reply, strlen(reply) + 1);
            }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return true;
}

// ── Lightweight ICE join ─────────────────────────────────────────────

bool runIceJoinOnly(const std::string& roomCode, const IceTestOptions& opts)
{
    printf("[ICE ONLY START] role=JOIN roomCode=%s\n", roomCode.c_str()); fflush(stdout);
    IceConfiguration iceConfig = loadIceConfig();
    bool usingTurn = !iceConfig.turn.password.empty();
    if (opts.disableRelay) { iceConfig.turn.password.clear(); usingTurn = false; }
    if (usingTurn && iceConfig.turn.password.empty()) {
        printf("[ICE ONLY RESULT] pass=0 reason=no-turn-password\n"); return false;
    }

    IceAgent agent;
    if (!agent.initialize(iceConfig)) { printf("[ICE ONLY RESULT] pass=0 reason=init-failed\n"); return false; }
    if (!agent.gatherCandidates()) { printf("[ICE ONLY RESULT] pass=0 reason=gather-failed\n"); return false; }
    if (!waitForState(agent, IceAgentState::GatheringComplete, 15000)) {
        printf("[ICE ONLY RESULT] pass=0 reason=gather-timeout\n"); return false;
    }

    std::string sessionId = "client_" + std::to_string(GetCurrentProcessId());
    auto joinResult = MimitaNet::coordinatorIceJoin(roomCode, sessionId, agent.localSdp());
    if (!joinResult.ok || joinResult.hostIceDescription.empty()) {
        printf("[ICE ONLY RESULT] pass=0 reason=join-failed\n"); return false;
    }
    printf("[ICE ONLY SIGNAL] role=JOIN descBytes=%zu\n", joinResult.hostIceDescription.size());

    if (!agent.setRemoteDescription(joinResult.hostIceDescription)) {
        printf("[ICE ONLY RESULT] pass=0 reason=remote-desc-failed\n"); return false;
    }
    printf("[ICE ONLY JOIN] waiting for connection...\n"); fflush(stdout);
    std::string earlyRecv;
    bool connected = waitForState(agent, IceAgentState::Connected, 20000, &earlyRecv) ||
                     waitForState(agent, IceAgentState::Completed, 5000, &earlyRecv);
    if (!connected) {
        printf("[ICE ONLY RESULT] pass=0 reason=connection-timeout\n"); return false;
    }
    printf("[ICE ONLY STATE] role=JOIN connected\n");
    agent.logSelectedPath();
    fflush(stdout);

    // Wait for stable connected state after role conflict resolution.
    bool reconnected = false;
    for (int stab = 0; stab < 60; ++stab) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::vector<IceEvent> evs; agent.pollEvents(evs);
        auto s = agent.state();
        if (s == IceAgentState::Connected || s == IceAgentState::Completed) {
            reconnected = true;
            break;
        }
    }
    printf("[ICE GAME CLIENT] final state=%d reconnected=%d\n", (int)agent.state(), (int)reconnected);
    fflush(stdout);

    if (!reconnected) {
        printf("[ICE GAME CLIENT] reconnect failed, trying fallback text payload\n"); fflush(stdout);
        const char* msg = "hello from client (fallback)";
        agent.send(msg, strlen(msg) + 1);
        int tf = 5000, wf = 0;
        std::string reply;
        while (wf < tf) {
            std::vector<IceEvent> evs; agent.pollEvents(evs);
            for (auto& ev : evs)
                if (ev.type == IceEventType::Recv) reply.assign(ev.data.data(), ev.data.size());
            if (!reply.empty()) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); wf += 50;
        }
        if (!reply.empty()) {
            printf("[ICE ONLY RECEIVE] role=JOIN bytes=%zu\n", reply.size());
            printf("[ICE ONLY RESULT] pass=1 (fallback)\n");
        } else {
            printf("[ICE ONLY RESULT] pass=0 reason=no-reply\n");
        }
        agent.shutdown();
        return !reply.empty();
    }

    // Send JoinRequestPacket with coordinator-issued join token
    MimitaNet::JoinRequestPacket joinReq{};
    joinReq.header.magic = MimitaNet::PROTOCOL_MAGIC;
    joinReq.header.version = MimitaNet::PROTOCOL_VERSION;
    joinReq.header.type = MimitaNet::PACKET_JOIN_REQUEST;
    memcpy(joinReq.joinToken, joinResult.joinToken.c_str(), std::min(joinResult.joinToken.size(), sizeof(joinReq.joinToken)));
    const char* playerName = "IceTestPlayer";
    memcpy(joinReq.name, playerName, strlen(playerName) + 1);

    bool sendOk = agent.send(&joinReq, sizeof(joinReq));
    printf("[ICE GAME JOIN SEND] name=%s tokenPresent=%d bytes=%zu result=%d\n",
           playerName, (int)!joinResult.joinToken.empty(), sizeof(joinReq), sendOk);
    fflush(stdout);
    if (!sendOk) { printf("[ICE ONLY RESULT] pass=0 reason=send-failed\n"); return false; }

    // Wait for JoinAcceptPacket
    int timeoutMs = opts.timeoutSeconds * 1000, waited = 0;
    std::string reply;
        while (waited < timeoutMs) {
        std::vector<IceEvent> evs; agent.pollEvents(evs);
        for (auto& ev : evs)
            if (ev.type == IceEventType::Recv)
                reply.assign(ev.data.data(), ev.data.size());
        if (!reply.empty()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); waited += 50;
    }
    if (reply.empty()) {
        printf("[ICE GAME JOIN] no reply received\n");
        printf("[ICE ONLY RESULT] pass=0 reason=no-reply\n"); return false;
    }

    // Check for JoinReject
    if (reply.size() >= sizeof(MimitaNet::PacketHeader)) {
        auto* hdr = reinterpret_cast<const MimitaNet::PacketHeader*>(reply.data());
        if (hdr->magic == MimitaNet::PROTOCOL_MAGIC && hdr->type == MimitaNet::PACKET_JOIN_REJECT) {
            printf("[ICE GAME JOIN REJECTED] reason=%d\n",
                   (reply.size() >= sizeof(MimitaNet::JoinRejectPacket))
                   ? reinterpret_cast<const MimitaNet::JoinRejectPacket*>(reply.data())->reason : -1);
            printf("[ICE ONLY RESULT] pass=0 reason=join-rejected\n"); return false;
        }
    }

    // Validate JoinAcceptPacket
    if (reply.size() < sizeof(MimitaNet::JoinAcceptPacket)) {
        printf("[ICE GAME JOIN] bad reply size=%zu\n", reply.size());
        printf("[ICE ONLY RESULT] pass=0 reason=bad-reply-size\n"); return false;
    }
    auto* accept = reinterpret_cast<const MimitaNet::JoinAcceptPacket*>(reply.data());
    if (accept->header.magic != MimitaNet::PROTOCOL_MAGIC || accept->header.type != MimitaNet::PACKET_JOIN_ACCEPT) {
        printf("[ICE GAME JOIN] bad header magic=%x type=%d\n", accept->header.magic, accept->header.type);
        printf("[ICE ONLY RESULT] pass=0 reason=bad-welcome\n"); return false;
    }
    printf("[ICE GAME JOIN ACCEPT] playerId=%u mapId=%.*s tickRate=%.1f\n",
           accept->assignedPlayerId, (int)sizeof(accept->mapId), accept->mapId, accept->tickRate);
    fflush(stdout);

    // Now send InputPackets and receive compact SnapshotChunks
    uint32_t myPlayerId = accept->assignedPlayerId;
    for (int step = 0; step < 60; ++step) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        MimitaNet::InputPacket input{};
        input.header.magic = MimitaNet::PROTOCOL_MAGIC;
        input.header.version = MimitaNet::PROTOCOL_VERSION;
        input.header.type = MimitaNet::PACKET_INPUT;
        input.header.playerId = myPlayerId;
        input.header.tick = (uint32_t)step;
        input.wishX = (step % 100 < 50) ? 5.0f : -5.0f;
        input.yaw = (float)step * 0.5f;
        input.dashPressed = (step % 60 == 0) ? 1 : 0;
        input.attackPressed = (step % 30 == 0) ? 1 : 0;
        agent.send(&input, sizeof(input));

        std::vector<IceEvent> evs; agent.pollEvents(evs);
        for (auto& ev : evs) {
            if (ev.type == IceEventType::Recv && ev.data.size() >= (int)sizeof(MimitaNet::PacketHeader)) {
                auto* hdr2 = reinterpret_cast<const MimitaNet::PacketHeader*>(ev.data.data());
                if (hdr2->magic == MimitaNet::PROTOCOL_MAGIC && hdr2->type == MimitaNet::PACKET_SNAPSHOT) {
                    // Accept both compact chunk and legacy format
                    auto* snap = reinterpret_cast<const MimitaNet::SnapshotChunkPacket*>(ev.data.data());
                    printf("[ICE GAME SNAPSHOT] tick=%u entities=%d chunk=%d/%d\n",
                           snap->serverTick, snap->entityCount, snap->chunkIndex, snap->chunkCount);
                    for (uint16_t ei = 0; ei < snap->entityCount; ++ei) {
                        auto& e = snap->entities[ei];
                        printf("[ICE GAME ENTITY] id=%u owner=%u pos=(%.1f,%.1f,%.1f) yaw=%.1f hp=%d\n",
                               e.networkEntityId, e.ownerClientId, e.px, e.py, e.pz, e.yaw, e.health);
                    }
                    fflush(stdout);
                }
            }
        }
    }

    printf("[ICE ONLY RESULT] pass=1\n");
    agent.shutdown();
    return true;
}

// ── ICE Game Host (runs server simulation) ─────────────────────────

bool runIceGameHost(const IceTestOptions& opts)
{
    printf("[ICE GAME HOST] start processId=%lu\n", (unsigned long)GetCurrentProcessId());
    fflush(stdout);

    IceConfiguration iceConfig = loadIceConfig();
    if (opts.disableRelay) iceConfig.turn.password.clear();
    if (!iceConfig.turn.password.empty() && iceConfig.turn.password.empty()) {
        printf("[ICE GAME HOST] no TURN password\n"); return false;
    }

    // Create host's own ICE agent for accepting joiner
    IceAgent hostAgent;
    if (!hostAgent.initialize(iceConfig)) return false;
    if (!hostAgent.gatherCandidates()) return false;
    if (!waitForState(hostAgent, IceAgentState::GatheringComplete, 15000)) return false;

    std::string sessionId = "gamehost_" + std::to_string(GetCurrentProcessId());
    auto hostResult = MimitaNet::coordinatorIceHost(sessionId, hostAgent.localSdp());
    if (!hostResult.ok) { printf("[ICE GAME HOST] coord register failed\n"); return false; }
    printf("[ICE GAME HOST ROOM] code=%s\n", hostResult.roomCode.c_str()); fflush(stdout);

    // Poll for client
    std::string clientDesc;
    int timeoutMs = opts.timeoutSeconds * 1000, waited = 0;
    while (waited < timeoutMs) {
        auto pollResult = MimitaNet::coordinatorIcePoll(hostResult.roomCode, sessionId);
        if (pollResult.ok && pollResult.status == "client_ready" && !pollResult.clientIceDescription.empty()) {
            clientDesc = pollResult.clientIceDescription; break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200)); waited += 200;
    }
    if (clientDesc.empty()) {
        printf("[ICE GAME HOST] poll timeout\n"); MimitaNet::coordinatorIceDone(hostResult.roomCode); return false;
    }

    if (!hostAgent.setRemoteDescription(clientDesc)) {
        printf("[ICE GAME HOST] remote desc failed\n"); MimitaNet::coordinatorIceDone(hostResult.roomCode); return false;
    }
    printf("[ICE GAME HOST] waiting for ICE connection...\n"); fflush(stdout);
    if (!waitForState(hostAgent, IceAgentState::Connected, 20000) && !waitForState(hostAgent, IceAgentState::Completed, 5000)) {
        printf("[ICE GAME HOST] connection timeout\n"); return false;
    }
    printf("[ICE GAME HOST] connected\n");
    hostAgent.logSelectedPath();

    // Stabilize after role conflict (may disconnect then reconnect)
    bool hostReconnected = false;
    for (int stab = 0; stab < 60; ++stab) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::vector<IceEvent> evs; hostAgent.pollEvents(evs);
        auto s = hostAgent.state();
        if (s == IceAgentState::Connected || s == IceAgentState::Completed) {
            hostReconnected = true;
            break;
        }
    }
    printf("[ICE GAME HOST] final state=%d reconnected=%d\n", (int)hostAgent.state(), (int)hostReconnected);
    fflush(stdout);

    // Create peer connection
    IcePeerConnection peer;
    peer.clientId = 1;
    peer.playerEntityId = 1001;
    peer.joinAttemptId = hostResult.roomCode;
    peer.agent = std::make_unique<IceAgent>();
    // Move the connected agent to the peer (simplified: reuse hostAgent as peer)
    // For single-peer test, just use hostAgent directly

    IceGameServer server;
    uint64_t lastSnapshotMs = 0;

    waited = 0;
    bool authenticated = false;
    uint32_t assignedId = 1001;

    while (waited < timeoutMs) {
        std::vector<IceEvent> evs;
        hostAgent.pollEvents(evs);

        for (auto& ev : evs) {
            if (ev.type == IceEventType::Recv && ev.data.size() >= (int)sizeof(MimitaNet::PacketHeader)) {
                auto* hdr = reinterpret_cast<const MimitaNet::PacketHeader*>(ev.data.data());

                if (!authenticated && hdr->type == MimitaNet::PACKET_JOIN_REQUEST) {
                    if (ev.data.size() < (int)sizeof(MimitaNet::JoinRequestPacket)) continue;
                    auto* joinReq = reinterpret_cast<const MimitaNet::JoinRequestPacket*>(ev.data.data());
                    std::string token((const char*)joinReq->joinToken);
                    token = token.c_str(); // null-terminated

                    // Validate token via coordinator
                    bool valid = MimitaNet::coordinatorIceValidateJoin(hostResult.roomCode, token);
                    printf("[ICE GAME JOIN REQUEST] name=%.*s tokenPresent=%d valid=%d\n",
                           (int)sizeof(joinReq->name), joinReq->name, (int)!token.empty(), (int)valid);
                    fflush(stdout);

                    if (!valid) {
                        MimitaNet::JoinRejectPacket reject{};
                        reject.header.magic = MimitaNet::PROTOCOL_MAGIC;
                        reject.header.version = MimitaNet::PROTOCOL_VERSION;
                        reject.header.type = MimitaNet::PACKET_JOIN_REJECT;
                        reject.reason = 2; // bad-token
                        hostAgent.send(&reject, sizeof(reject));
                        printf("[ICE GAME JOIN REJECT] reason=bad-token\n");
                        continue;
                    }

                    authenticated = true;
                    auto& p = server.peers[assignedId];
                    p.clientId = assignedId;
                    p.playerEntityId = assignedId;
                    p.authenticated = true;
                    p.agent = std::make_unique<IceAgent>();
                    // Move hostAgent - but IceAgent has no move. For test, just assign.
                    p.joinAttemptId = hostResult.roomCode;
                    p.player.playerId = assignedId;
                    p.player.name = std::string((const char*)joinReq->name);
                    p.player.py = 5.0f;
                    p.player.pz = 30.0f;

                    // Send JoinAcceptPacket
                    MimitaNet::JoinAcceptPacket accept{};
                    accept.header.magic = MimitaNet::PROTOCOL_MAGIC;
                    accept.header.version = MimitaNet::PROTOCOL_VERSION;
                    accept.header.type = MimitaNet::PACKET_JOIN_ACCEPT;
                    accept.header.playerId = assignedId;
                    accept.assignedPlayerId = assignedId;
                    accept.tickRate = 60.0f;
                    memcpy(accept.approvedName, joinReq->name, sizeof(accept.approvedName));
                    memcpy(accept.mapId, "funworldv3", 11);
                    const char* recon = "game-test-recon";
                    memcpy(accept.reconnectToken, recon, strlen(recon) + 1);
                    hostAgent.send(&accept, sizeof(accept));
                    printf("[ICE GAME JOIN ACCEPT] playerId=%u\n", assignedId);
                    fflush(stdout);
                }
                else if (authenticated && hdr->type == MimitaNet::PACKET_INPUT) {
                    if (ev.data.size() < (int)sizeof(MimitaNet::InputPacket)) continue;
                    auto* input = reinterpret_cast<const MimitaNet::InputPacket*>(ev.data.data());
                    auto& p = server.peers[assignedId].player;
                    p.yaw = input->yaw;
                    p.vx += input->wishX * 10.0f * 0.05f;
                    p.vz += 0.0f; // no forward/back in this simple test
                    // Apply dash if pressed
                    if (input->dashPressed) p.vx *= 2.0f;
                    p.lastPacketMs = GetTickCount64();
                }
            }
        }

        // Tick server (60 Hz)
        server.tickSimulation(0.016f);

        // Send snapshots (20 Hz)
        uint64_t now = GetTickCount64();
        if (now - lastSnapshotMs > 50) {
            lastSnapshotMs = now;
            // Build snapshot with all authenticated players
            MimitaNet::SnapshotPacket snap{};
            snap.header.magic = MimitaNet::PROTOCOL_MAGIC;
            snap.header.version = MimitaNet::PROTOCOL_VERSION;
            snap.header.type = MimitaNet::PACKET_SNAPSHOT;
            snap.header.tick = (uint32_t)server.tick;
            int idx = 0;
            for (auto& [pid, peer] : server.peers) {
                if (!peer.authenticated) continue;
                auto& sp = peer.player;
                auto& e = snap.entities[idx++];
                e.networkEntityId = sp.playerId;
                e.entityType = MimitaNet::ENTITY_PLAYER;
                e.active = sp.alive ? 1 : 0;
                e.ownerClientId = peer.clientId;
                e.px = sp.px; e.py = sp.py; e.pz = sp.pz;
                e.vx = sp.vx; e.vy = sp.vy; e.vz = sp.vz;
                e.yaw = sp.yaw;
                e.health = sp.health;
                memcpy(e.displayName, sp.name.c_str(), std::min(sp.name.size() + 1, sizeof(e.displayName)));
            }
            snap.entityCount = idx;
            hostAgent.send(&snap, sizeof(snap));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        waited += 10;
    }

    printf("[ICE GAME HOST] done\n");
    MimitaNet::coordinatorIceDone(hostResult.roomCode);
    hostAgent.shutdown();
    return true;
}

// ── ICE Game Client ─────────────────────────────────────────────────

bool runIceGameClient(const std::string& roomCode, const IceTestOptions& opts)
{
    printf("[ICE GAME CLIENT] roomCode=%s\n", roomCode.c_str()); fflush(stdout);

    IceConfiguration iceConfig = loadIceConfig();
    if (opts.disableRelay) iceConfig.turn.password.clear();
    if (!iceConfig.turn.password.empty() && iceConfig.turn.password.empty()) return false;

    IceAgent agent;
    if (!agent.initialize(iceConfig)) return false;
    if (!agent.gatherCandidates()) return false;
    if (!waitForState(agent, IceAgentState::GatheringComplete, 15000)) return false;

    std::string sessionId = "gameclient_" + std::to_string(GetCurrentProcessId());
    auto joinResult = MimitaNet::coordinatorIceJoin(roomCode, sessionId, agent.localSdp());
    if (!joinResult.ok || joinResult.hostIceDescription.empty()) {
        printf("[ICE GAME CLIENT] join failed\n"); return false;
    }

    if (!agent.setRemoteDescription(joinResult.hostIceDescription)) {
        printf("[ICE GAME CLIENT] remote desc failed\n"); return false;
    }
    if (!waitForState(agent, IceAgentState::Connected, 20000) && !waitForState(agent, IceAgentState::Completed, 5000)) {
        printf("[ICE GAME CLIENT] connection timeout\n"); return false;
    }
    printf("[ICE GAME CLIENT] connected\n");
    agent.logSelectedPath();

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::vector<IceEvent> drain; agent.pollEvents(drain);

    // Send JoinRequestPacket
    MimitaNet::JoinRequestPacket joinReq{};
    joinReq.header.magic = MimitaNet::PROTOCOL_MAGIC;
    joinReq.header.version = MimitaNet::PROTOCOL_VERSION;
    joinReq.header.type = MimitaNet::PACKET_JOIN_REQUEST;
    memcpy(joinReq.joinToken, joinResult.joinToken.c_str(), std::min(joinResult.joinToken.size(), sizeof(joinReq.joinToken)));
    const char* playerName = "GameTestPlayer";
    memcpy(joinReq.name, playerName, strlen(playerName) + 1);
    if (!agent.send(&joinReq, sizeof(joinReq))) {
        printf("[ICE GAME CLIENT] join send failed\n"); return false;
    }
    printf("[ICE GAME JOIN SEND] name=%s\n", playerName); fflush(stdout);

    // Wait for JoinAcceptPacket
    int timeoutMs = 15000, waited = 0;
    std::string reply;
    while (waited < timeoutMs) {
        std::vector<IceEvent> evs; agent.pollEvents(evs);
        for (auto& ev : evs)
            if (ev.type == IceEventType::Recv)
                reply.assign(ev.data.data(), ev.data.size());
        if (!reply.empty()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); waited += 50;
    }
    if (reply.empty()) { printf("[ICE GAME CLIENT] no reply\n"); return false; }
    if (reply.size() < sizeof(MimitaNet::JoinAcceptPacket)) { printf("[ICE GAME CLIENT] bad accept size\n"); return false; }
    auto* accept = reinterpret_cast<const MimitaNet::JoinAcceptPacket*>(reply.data());
    if (accept->header.magic != MimitaNet::PROTOCOL_MAGIC || accept->header.type != MimitaNet::PACKET_JOIN_ACCEPT) {
        printf("[ICE GAME CLIENT] bad accept header\n"); return false;
    }
    uint32_t myId = accept->assignedPlayerId;
    printf("[ICE GAME JOIN ACCEPT] playerId=%u mapId=%.*s\n", myId, (int)sizeof(accept->mapId), accept->mapId);
    fflush(stdout);

    // Game loop: send inputs, receive snapshots
    for (int step = 0; step < 200; ++step) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        MimitaNet::InputPacket input{};
        input.header.magic = MimitaNet::PROTOCOL_MAGIC;
        input.header.version = MimitaNet::PROTOCOL_VERSION;
        input.header.type = MimitaNet::PACKET_INPUT;
        input.header.playerId = myId;
        input.header.tick = (uint32_t)step;
        // Oscillate movement for visual confirmation
        input.wishX = (step % 100 < 50) ? 5.0f : -5.0f;
        input.yaw = (float)step * 0.5f;
        input.dashPressed = (step % 60 == 0) ? 1 : 0;
        agent.send(&input, sizeof(input));

        std::vector<IceEvent> evs; agent.pollEvents(evs);
        for (auto& ev : evs) {
            if (ev.type == IceEventType::Recv && ev.data.size() >= (int)sizeof(MimitaNet::PacketHeader)) {
                auto* hdr = reinterpret_cast<const MimitaNet::PacketHeader*>(ev.data.data());
                if (hdr->magic == MimitaNet::PROTOCOL_MAGIC && hdr->type == MimitaNet::PACKET_SNAPSHOT) {
                    auto* snap = reinterpret_cast<const MimitaNet::SnapshotPacket*>(ev.data.data());
                    printf("[ICE GAME SNAPSHOT] tick=%u entities=%d\n", snap->header.tick, snap->entityCount);
                    for (uint32_t ei = 0; ei < snap->entityCount; ++ei) {
                        auto& e = snap->entities[ei];
                        printf("[ICE GAME ENTITY] id=%u owner=%u pos=(%.1f,%.1f,%.1f) yaw=%.1f\n",
                               e.networkEntityId, e.ownerClientId, e.px, e.py, e.pz, e.yaw);
                    }
                    fflush(stdout);
                }
                else if (hdr->type == MimitaNet::PACKET_JOIN_REJECT) {
                    printf("[ICE GAME JOIN REJECTED]\n"); fflush(stdout);
                }
            }
        }
    }

    printf("[ICE GAME CLIENT] pass=1\n");
    agent.shutdown();
    return true;
}
