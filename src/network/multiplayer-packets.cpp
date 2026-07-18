#include "network/multiplayer-context.h"
#include "network/packets.h"
#include "network/udp-transport.h"
#include "network/ice-transport.h"
#include "network/ice/ice-agent.h"
#include "network/ice/ice-config.h"
#include "network/coordinator-client.h"
#include "analytics/analytics-manager.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace MimitaNet {

void mpSendPacket(MultiplayerContext& ctx, const void* data, int bytes)
{
    if (!ctx.active || !data || bytes <= 0)
        return;
    if (!ctx.transport && ctx.sock == INVALID_SOCKET)
        return;

    // Use ICE transport if available
    if (ctx.transport)
    {
        ctx.transport->send(data, (size_t)bytes);
        ++ctx.packetsSent;
        return;
    }

    int delayMs = 0;
    if (ctx.fakeLagMode == 1)
        delayMs = ctx.fakeLagCurrentMs;
    else if (ctx.fakeLagMode == 2)
        delayMs = ctx.fakeLagStaticMs;

    if (delayMs <= 0)
    {
        int sentBytes = sendto(ctx.sock, (const char*)data, bytes, 0,
                               (sockaddr*)&ctx.serverAddr, sizeof(ctx.serverAddr));
        if (sentBytes == SOCKET_ERROR)
            printf("[NET TX ERROR] sendto failed error=%d\n", WSAGetLastError());
        else
            printf("[NET TX] type=%d bytes=%d\n", ((PacketHeader*)data)->type, bytes);
        ++ctx.packetsSent;
        return;
    }

    QueuedPacket queued;
    queued.bytes.assign((const char*)data, (const char*)data + bytes);
    queued.deliverAtMs = nowMs() + (uint64_t)delayMs;
    ctx.outgoingQueue.push_back(std::move(queued));
    const uint64_t currentMs = nowMs();
    if (currentMs - ctx.lastFakeLagLogMs >= 250)
    {
        printf("[FAKELAG] mode=%d delay=%d packetQueued=%zu\n",
               ctx.fakeLagMode, delayMs, ctx.outgoingQueue.size());
        ctx.lastFakeLagLogMs = currentMs;
    }
}

void flushOutgoingPackets(MultiplayerContext& ctx)
{
    if (!ctx.active)
        return;
    if (!ctx.transport && ctx.sock == INVALID_SOCKET)
        return;

    const uint64_t currentMs = nowMs();
    for (size_t i = 0; i < ctx.outgoingQueue.size(); )
    {
        QueuedPacket& queued = ctx.outgoingQueue[i];
        if (queued.deliverAtMs > currentMs)
        {
            ++i;
            continue;
        }

        if (ctx.transport)
            ctx.transport->send(queued.bytes.data(), queued.bytes.size());
        else
            sendto(ctx.sock, queued.bytes.data(), (int)queued.bytes.size(), 0,
                   (sockaddr*)&ctx.serverAddr, sizeof(ctx.serverAddr));
        ++ctx.packetsSent;
        ctx.outgoingQueue.erase(ctx.outgoingQueue.begin() + i);
    }
}

void mpSetFakeLagMode(MultiplayerContext& ctx, int mode)
{
    ctx.fakeLagMode = std::clamp(mode, 0, 2);
    ctx.fakeLagNextRandomizeMs = 0;
    if (ctx.fakeLagMode == 0)
    {
        ctx.fakeLagCurrentMs = 0;
        for (QueuedPacket& queued : ctx.outgoingQueue)
            queued.deliverAtMs = 0;
        flushOutgoingPackets(ctx);
    }
}

void mpSetFakeLagStatic(MultiplayerContext& ctx, int milliseconds)
{
    ctx.fakeLagStaticMs = std::clamp(milliseconds, 0, 5000);
}

void mpSetFakeLagRange(MultiplayerContext& ctx, int minimumMs, int maximumMs)
{
    minimumMs = std::clamp(minimumMs, 0, 5000);
    maximumMs = std::clamp(maximumMs, 0, 5000);
    if (minimumMs > maximumMs)
        std::swap(minimumMs, maximumMs);
    ctx.fakeLagMinMs = minimumMs;
    ctx.fakeLagMaxMs = maximumMs;
    ctx.fakeLagNextRandomizeMs = 0;
}

// ── Connection lifecycle ──────────────────────────────────────────────
const char* disconnectPolicyName(DisconnectPolicy policy)
{
    switch (policy)
    {
    case DisconnectPolicy::Leave: return "leave";
    case DisconnectPolicy::Timeout: return "timeout";
    case DisconnectPolicy::NewConnection: return "new-connection";
    case DisconnectPolicy::Rejected: return "rejected";
    case DisconnectPolicy::AuthFailure: return "auth-failure";
    case DisconnectPolicy::ConnectionFailure: return "connection-failure";
    case DisconnectPolicy::ServerStopped: return "server-stopped";
    default: return "unknown";
    }
}

void teardownPreviousSession(MultiplayerContext& ctx, DisconnectPolicy policy)
{
    // Log the teardown
    printf("[NET TEARDOWN] policy=%s reason=%s\n",
           disconnectPolicyName(policy),
           ctx.connectionStatus.c_str());

    // Policy: keep reconnectToken only for timeout to same session
    if (policy != DisconnectPolicy::Timeout)
        ctx.reconnectToken.clear();

    // Clear join token, room code, session identity
    ctx.joinToken.clear();
    ctx.roomCode.clear();
    ctx.serverAddress = "127.0.0.1:1357";
    ctx.serverPort = 1357;
    ctx.currentRoomCode.clear();
    ctx.sessionId.clear();

    // Clear local identity and connection flags
    ctx.localPlayerId = 0;
    ctx.active = false;
    ctx.connected = false;
    ctx.connectFailed = false;
    ctx.connectionState = ConnectionState::Disconnected;
    ctx.connectionStatus.clear();
    ctx.waitingForMapLoad = false;
    ctx.clientMapReadySent = false;
    ctx.requiredMapId.clear();
    ctx.showPlayerList = false;

    // Clear remote state
    ctx.remotePlayers.clear();
    ctx.remoteNpcs.clear();
    ctx.remotePlayerInterpolation.clear();
    ctx.remoteNpcInterpolation.clear();
    ctx.networkProjectiles.clear();
    ctx.playerRegistry.clear();
    ctx.predictedProjectileIds.clear();
    ctx.remoteSwordStates.clear();

    // Clear reconciliation state
    ctx.hasLocalServerPosition = false;
    ctx.localPlayerReconciled = false;
    ctx.localServerPosition = glm::vec3(0.0f);
    ctx.localServerVelocity = glm::vec3(0.0f);
    ctx.localServerHealth = 100;
    ctx.lastSeenServerHealth = 100;
    ctx.localServerEpoch = 0;
    ctx.lastAppliedEpoch = 0;
    ctx.pendingTeleportPosition = glm::vec3(0.0f);
    ctx.pendingTeleportSentMs = 0;
    ctx.awaitingTeleportAck = false;
    ctx.awaitingExplodeDeath = false;
    ctx.teleportResync = false;
    ctx.transformEpoch = 0;

    // Clear pending requests and events
    ctx.shotEvents.clear();
    ctx.disagreementEvents.clear();
    ctx.processedDisagreementIds.clear();
    ctx.processedPelletBlastSerials.clear();
    ctx.lastReceivedShotSerial.clear();
    ctx.fireRejections.clear();
    ctx.processedRefundSerials.clear();
    ctx.pendingFireRequests.clear();
    ctx.pendingKnockback = glm::vec3(0.0f);
    ctx.pendingKnockbackSource.clear();
    ctx.incomingChatMessages.clear();
    ctx.outgoingQueue.clear();

    // Reset reconnect timers
    ctx.reconnectAttempts = 0;
    ctx.reconnectBackoffMs = 1000;
    ctx.lastReconnectAttemptMs = 0;

    // Preserve monotonically increasing serials (NEVER reset):
    //   nextLocalProjectileFireSerial
    //   nextLocalShotSerial
    //   nextLocalMeleeAttackSerial
    //   nextLocalDashSerial, nextLocalGroundJumpSerial, etc.
    //   nextLocalRespawnSerial

    // Close transport if open
    if (ctx.transport)
    {
        ctx.transport->close();
        ctx.transport.reset();
    }
    if (ctx.sock != INVALID_SOCKET)
    {
        closesocket(ctx.sock);
        ctx.sock = INVALID_SOCKET;
    }

    // Log state after teardown
    printf("[NET TEARDOWN] complete policy=%s\n", disconnectPolicyName(policy));
}

void beginConnectionAttempt(MultiplayerContext& ctx, const std::string& roomCode,
    const std::string& address, uint16_t port)
{
    // Tear down any previous session first
    teardownPreviousSession(ctx, DisconnectPolicy::NewConnection);

    // Increment attempt ID (never reset, permanently monotonic)
    ++ctx.connectionAttemptId;

    // Install new connection parameters
    ctx.roomCode = roomCode;
    ctx.serverAddress = address;
    ctx.serverPort = port;
    ctx.currentRoomCode = roomCode;

    // Enter initial connecting state
    ctx.connectionState = ConnectionState::Connecting;
    ctx.active = true;
    ctx.connectStartMs = nowMs();
    ctx.connectionStatus = "Connecting...";

    printf("[NET CONNECT ATTEMPT] attemptId=%u room=%s addr=%s:%u\n",
           ctx.connectionAttemptId, roomCode.c_str(), address.c_str(), port);
}

bool mpInit(MultiplayerContext& ctx, const std::string& address, const std::string& playerName)
{
    ctx.serverAddress = address;

    if (!netStartup())
    {
        printf("[NET CONNECT] FATAL: WSAStartup failed\n");
        return false;
    }

    ctx.sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (ctx.sock == INVALID_SOCKET)
    {
        printf("[NET CONNECT] FATAL: socket() failed error=%d\n", WSAGetLastError());
        netShutdown();
        return false;
    }
    setNonBlocking(ctx.sock);
    printf("[NET SOCKET] created sock=%d\n", (int)ctx.sock);

    {
        sockaddr_in localBind{};
        localBind.sin_family = AF_INET;
        localBind.sin_addr.s_addr = htonl(INADDR_ANY);
        localBind.sin_port = htons(0);
        if (bind(ctx.sock, (sockaddr*)&localBind, sizeof(localBind)) == SOCKET_ERROR)
            printf("[NET CONNECT] WARNING: bind() port=0 failed error=%d (non-fatal)\n", WSAGetLastError());
        else
        {
            sockaddr_in actual{};
            int actualLen = sizeof(actual);
            if (getsockname(ctx.sock, (sockaddr*)&actual, &actualLen) == 0)
                printf("[NET SOCKET] bound local endpoint=%s\n", addressToString(actual).c_str());
        }
    }

    if (!parseAddress(address, ctx.serverAddr))
    {
        printf("[NET CONNECT] FATAL: invalid address: %s\n", address.c_str());
        closesocket(ctx.sock);
        netShutdown();
        return false;
    }

    ctx.transport = std::make_unique<UdpTransport>(ctx.sock, ctx.serverAddr);
    ctx.active = true;
    ctx.localPlayerId = 0;
    ctx.tick = 0;
    ctx.lastHelloMs = 0;
    ctx.connectionState = ConnectionState::Connecting;
    ctx.lastSnapshotReceivedMs = 0;
    ctx.connectStartMs = nowMs();
    ctx.packetsSent = 0;
    ctx.packetsReceived = 0;
    ctx.snapshotsReceived = 0;
    ctx.snapshotsMissed = 0;
    ctx.remotePlayers.clear();
    ctx.remoteNpcs.clear();
    ctx.remotePlayerInterpolation.clear();
    ctx.remoteNpcInterpolation.clear();
    ctx.networkProjectiles.clear();
    ctx.playerRegistry.clear();
    ctx.approvedLocalName.clear();
    ctx.hasLocalServerPosition = false;
    ctx.localPlayerReconciled = false;
    ctx.lastLocalCorrectionLogMs = 0;
    ctx.pendingTeleportPosition = glm::vec3(0.0f);
    ctx.pendingTeleportSentMs = 0;
    ctx.awaitingTeleportAck = false;
    ctx.awaitingExplodeDeath = false;
    ctx.localServerVelocity = glm::vec3(0.0f);
    ctx.localServerYaw = 0.0f;
    ctx.localServerOnGround = false;
    ctx.localServerHealth = 100;
    ctx.connected = false;
    ctx.connectFailed = false;
    ctx.connectionStatus = "Connecting...";
    ctx.outgoingQueue.clear();
    ctx.shotEvents.clear();
    ctx.lastReceivedShotSerial.clear();
    ctx.nextLocalShotSerial = 1;
    ctx.nextLocalProjectileFireSerial = 1;
    ctx.nextLocalMeleeAttackSerial = 1;
    ctx.lastPingSentMs = 0;
    ctx.localPingMs = 0;
    ctx.lastHeardServerMs = 0;
    ctx.lastDisconnectLogMs = 0;
    ctx.lastFakeLagLogMs = 0;

    printf("[NET CONNECT] connecting to %s as \"%s\"\n", address.c_str(), playerName.c_str());
    return true;
}

void mpShutdown(MultiplayerContext& ctx)
{
    if (!ctx.active)
        return;

    printf("[NET DISCONNECT] initiating shutdown for playerId=%u\n", ctx.localPlayerId);

    // Send goodbye
    if (ctx.localPlayerId)
    {
        DisconnectPacket bye{};
        bye.header.type = PACKET_DISCONNECT;
        bye.header.tick = ctx.tick;
        bye.header.playerId = ctx.localPlayerId;
        if (ctx.transport)
            ctx.transport->send(&bye, sizeof(bye));
        else if (ctx.sock != INVALID_SOCKET)
            sendto(ctx.sock, (const char*)&bye, sizeof(bye), 0,
                   (sockaddr*)&ctx.serverAddr, sizeof(ctx.serverAddr));
    }

    teardownPreviousSession(ctx, DisconnectPolicy::Leave);
    netShutdown();
    printf("[NET DISCONNECT] shutdown complete\n");
}

void mpRequestNpcSpawn(MultiplayerContext& ctx, const glm::vec3& position, float difficulty)
{
    if (!ctx.active || !ctx.localPlayerId)
        return;

    SpawnNpcRequestPacket request{};
    request.header.type = PACKET_SPAWN_NPC_REQUEST;
    request.header.tick = ctx.tick;
    request.header.playerId = ctx.localPlayerId;
    request.px = position.x;
    request.py = position.y;
    request.pz = position.z;
    request.difficulty = difficulty;
    mpSendPacket(ctx, &request, sizeof(request));
}

void mpRequestTeleport(MultiplayerContext& ctx, const glm::vec3& position)
{
    if (!ctx.active || !ctx.localPlayerId)
        return;

    TeleportRequestPacket request{};
    request.header.type = PACKET_TELEPORT_REQUEST;
    request.header.tick = ctx.tick;
    request.header.playerId = ctx.localPlayerId;
    request.px = position.x;
    request.py = position.y;
    request.pz = position.z;
    ctx.pendingTeleportPosition = position;
    ctx.pendingTeleportSentMs = nowMs();
    ctx.awaitingTeleportAck = true;
    mpSendPacket(ctx, &request, sizeof(request));
}

void mpRequestExplode(MultiplayerContext& ctx)
{
    if (!ctx.active || !ctx.localPlayerId)
        return;

    ExplodeRequestPacket request{};
    request.header.type = PACKET_EXPLODE_REQUEST;
    request.header.tick = ctx.tick;
    request.header.playerId = ctx.localPlayerId;
    ctx.awaitingExplodeDeath = true;
    mpSendPacket(ctx, &request, sizeof(request));
}

void mpSendServerCommand(MultiplayerContext& ctx, const std::string& command)
{
    if (!ctx.active || !ctx.localPlayerId)
        return;

    ServerCommandPacket packet{};
    packet.header.type = PACKET_SERVER_COMMAND;
    packet.header.tick = ctx.tick;
    packet.header.playerId = ctx.localPlayerId;
    std::memset(packet.commandText, 0, sizeof(packet.commandText));
    std::strncpy(packet.commandText, command.c_str(), sizeof(packet.commandText) - 1);
    mpSendPacket(ctx, &packet, sizeof(packet));
    printf("[NET SERVER COMMAND SEND] cmd=\"%s\"\n", command.c_str());
}

// ── Migration: connection state name ──────────────────────────────────

const char* connectionStateName(ConnectionState state)
{
    switch (state)
    {
        case ConnectionState::Disconnected:     return "Disconnected";
        case ConnectionState::ResolvingCode:    return "ResolvingCode";
        case ConnectionState::RequestingJoin:   return "RequestingJoin";
        case ConnectionState::WaitJoinAccept:   return "WaitJoinAccept";
        case ConnectionState::NatNegotiating:   return "NatNegotiating";
        case ConnectionState::Connecting:       return "Connecting";
        case ConnectionState::Connected:        return "Connected";
        case ConnectionState::Reconnecting:     return "Reconnecting";
        case ConnectionState::DisconnectPending: return "DisconnectPending";
    }
    return "Unknown";
}

// ── Migration: connect with join token ────────────────────────────────

bool mpConnectWithToken(MultiplayerContext& ctx, const std::string& address,
    uint16_t port, const std::string& joinToken, const std::string& playerName)
{
    // beginConnectionAttempt must be called first. This function does NOT
    // tear down the previous session — that ownership belongs to beginConnectionAttempt.
    if (!netStartup())
    {
        printf("[NET CONNECT] FATAL: WSAStartup failed\n");
        return false;
    }

    ctx.sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (ctx.sock == INVALID_SOCKET)
    {
        printf("[NET CONNECT] FATAL: socket() failed error=%d\n", WSAGetLastError());
        netShutdown();
        return false;
    }
    setNonBlocking(ctx.sock);
    printf("[NET SOCKET] created sock=%d\n", (int)ctx.sock);

    {
        sockaddr_in localBind{};
        localBind.sin_family = AF_INET;
        localBind.sin_addr.s_addr = htonl(INADDR_ANY);
        localBind.sin_port = htons(0);
        if (bind(ctx.sock, (sockaddr*)&localBind, sizeof(localBind)) == SOCKET_ERROR)
            printf("[NET CONNECT] WARNING: bind() port=0 failed error=%d (non-fatal)\n", WSAGetLastError());
        else
        {
            sockaddr_in actual{};
            int actualLen = sizeof(actual);
            if (getsockname(ctx.sock, (sockaddr*)&actual, &actualLen) == 0)
                printf("[NET SOCKET] bound local endpoint=%s\n", addressToString(actual).c_str());
        }
    }

    ctx.serverAddr = {};
    ctx.serverAddr.sin_family = AF_INET;
    ctx.serverAddr.sin_port = htons(port);
    if (inet_pton(AF_INET, address.c_str(), &ctx.serverAddr.sin_addr) != 1)
    {
        printf("[NET CONNECT] FATAL: invalid address: %s\n", address.c_str());
        closesocket(ctx.sock);
        netShutdown();
        return false;
    }

    ctx.transport = std::make_unique<UdpTransport>(ctx.sock, ctx.serverAddr);
    ctx.active = true;
    ctx.localPlayerId = 0;
    ctx.tick = 0;
    ctx.lastHelloMs = 0;
    ctx.lastSnapshotReceivedMs = 0;
    ctx.connectStartMs = nowMs();
    ctx.packetsSent = 0;
    ctx.packetsReceived = 0;
    ctx.snapshotsReceived = 0;
    ctx.snapshotsMissed = 0;
    ctx.remotePlayers.clear();
    ctx.remoteNpcs.clear();
    ctx.remotePlayerInterpolation.clear();
    ctx.remoteNpcInterpolation.clear();
    ctx.networkProjectiles.clear();
    ctx.playerRegistry.clear();
    ctx.approvedLocalName.clear();
    ctx.hasLocalServerPosition = false;
    ctx.localPlayerReconciled = false;
    ctx.connectionState = ConnectionState::Connecting;
    ctx.joinToken = joinToken;
    ctx.serverAddress = address + ":" + std::to_string(port);
    ctx.connected = false;
    ctx.connectFailed = false;
    ctx.connectionStatus = "Connecting...";
    ctx.outgoingQueue.clear();
    ctx.shotEvents.clear();
    ctx.lastReceivedShotSerial.clear();
    ctx.nextLocalShotSerial = 1;
    ctx.nextLocalProjectileFireSerial = 1;
    ctx.nextLocalMeleeAttackSerial = 1;
    ctx.lastPingSentMs = 0;
    ctx.localPingMs = 0;
    ctx.lastHeardServerMs = 0;
    ctx.lastDisconnectLogMs = 0;
    ctx.disagreementEvents.clear();
    ctx.processedDisagreementIds.clear();
    ctx.serverPort = port;

    printf("[NET CONNECT] connecting to %s:%u with join token as \"%s\"\n",
           address.c_str(), port, playerName.c_str());
    return true;
}

// ── ICE client connection ──────────────────────────────────────────────
// Connects to an ICE-enabled server via the coordinator.
// Creates an IceAgent, exchanges SDP through coordinator, establishes
// an encrypted (or direct) P2P path, and wraps it in IceTransport.
// On return, ctx.transport is set to the ICE transport.
// The caller must still send Hello/JoinRequest through the normal flow.

static bool waitForIceAgentState(IceAgent& agent, IceAgentState target,
                                   int timeoutMs, std::string* earlyRecv = nullptr)
{
    int waited = 0;
    while (waited < timeoutMs)
    {
        agent.tick();
        std::vector<IceEvent> evs;
        agent.pollEvents(evs);
        for (auto& ev : evs)
        {
            if (ev.type == IceEventType::Recv && earlyRecv)
                earlyRecv->assign(ev.data.data(), ev.data.size());
        }

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

bool mpIceConnect(MultiplayerContext& ctx, const std::string& roomCode,
                  const std::string& playerName)
{
    if (ctx.active)
        mpShutdown(ctx);

    printf("[ICE CONNECT] starting ICE connection to room=%s as \"%s\"\n",
           roomCode.c_str(), playerName.c_str());

    if (!netStartup())
    {
        printf("[ICE CONNECT] FATAL: WSAStartup failed\n");
        return false;
    }

    // Load ICE config (STUN + optional TURN from VPS)
    IceConfiguration iceConfig = loadIceConfig();
    if (iceConfig.turn.password.empty())
    {
        printf("[ICE CONNECT] WARNING: no TURN password in ice-dev.json; "
               "direct connections may fail behind symmetric NAT\n");
    }

    // Create ICE agent on heap (IceAgent is not movable)
    auto agentPtr = std::make_unique<IceAgent>();
    if (!agentPtr->initialize(iceConfig))
    {
        printf("[ICE CONNECT] FATAL: agent initialization failed\n");
        netShutdown();
        return false;
    }
    ctx.connectionState = ConnectionState::NatNegotiating;
    ctx.connectionStatus = "ICE: gathering candidates...";

    if (!agentPtr->gatherCandidates())
    {
        printf("[ICE CONNECT] FATAL: candidate gathering failed\n");
        netShutdown();
        return false;
    }
    if (!waitForIceAgentState(*agentPtr, IceAgentState::GatheringComplete, 15000))
    {
        printf("[ICE CONNECT] FATAL: gather timeout (15s)\n");
        netShutdown();
        return false;
    }
    printf("[ICE CONNECT] candidates gathered\n");

    // Exchange SDP via coordinator
    std::string sessionId = "client_" + std::to_string(GetCurrentProcessId())
        + "_" + std::to_string(nowMs());
    ctx.connectionStatus = "ICE: contacting coordinator...";

    auto joinResult = coordinatorIceJoin(roomCode, sessionId, agentPtr->localSdp());
    if (!joinResult.ok || joinResult.hostIceDescription.empty())
    {
        printf("[ICE CONNECT] FATAL: coordinator ICE join failed\n");
        netShutdown();
        return false;
    }
    printf("[ICE CONNECT] received host SDP (%zu bytes)\n",
           joinResult.hostIceDescription.size());

    // Set remote description (host's SDP)
    if (!agentPtr->setRemoteDescription(joinResult.hostIceDescription))
    {
        printf("[ICE CONNECT] FATAL: setRemoteDescription failed\n");
        netShutdown();
        return false;
    }
    ctx.connectionStatus = "ICE: connecting...";

    // Wait for ICE connection
    std::string earlyRecv;
    if (!waitForIceAgentState(*agentPtr, IceAgentState::Connected, 30000, &earlyRecv))
    {
        printf("[ICE CONNECT] FATAL: connection timeout (30s)\n");
        netShutdown();
        return false;
    }
    printf("[ICE CONNECT] ICE connection established\n");
    agentPtr->logSelectedPath();

    // Create ICE transport, reset multiplayer context state
    ctx.transport = std::make_unique<IceTransport>(std::move(agentPtr));
    ctx.useIce = true;
    ctx.active = true;
    ctx.localPlayerId = 0;
    ctx.tick = 0;
    ctx.lastHelloMs = 0;
    ctx.lastSnapshotReceivedMs = 0;
    ctx.connectStartMs = nowMs();
    ctx.packetsSent = 0;
    ctx.packetsReceived = 0;
    ctx.snapshotsReceived = 0;
    ctx.snapshotsMissed = 0;
    ctx.remotePlayers.clear();
    ctx.remoteNpcs.clear();
    ctx.remotePlayerInterpolation.clear();
    ctx.remoteNpcInterpolation.clear();
    ctx.networkProjectiles.clear();
    ctx.playerRegistry.clear();
    ctx.approvedLocalName.clear();
    ctx.hasLocalServerPosition = false;
    ctx.localPlayerReconciled = false;
    ctx.connectionState = ConnectionState::Connecting;
    ctx.joinToken = joinResult.joinToken;
    ctx.serverAddress = "ice:" + roomCode;
    ctx.connected = false;
    ctx.connectFailed = false;
    ctx.connectionStatus = "Connected via ICE";
    ctx.outgoingQueue.clear();
    ctx.shotEvents.clear();
    ctx.lastReceivedShotSerial.clear();
    ctx.nextLocalShotSerial = 1;
    ctx.nextLocalProjectileFireSerial = 1;
    ctx.nextLocalMeleeAttackSerial = 1;
    ctx.lastPingSentMs = 0;
    ctx.localPingMs = 0;
    ctx.lastHeardServerMs = 0;
    ctx.lastDisconnectLogMs = 0;
    ctx.disagreementEvents.clear();
    ctx.processedDisagreementIds.clear();
    ctx.currentRoomCode = roomCode;

    printf("[ICE CONNECT] connected via ICE to room=%s\n", roomCode.c_str());
    return true;
}

// ── Migration: disagreement processing ────────────────────────────────

void mpProcessDisagreementPacket(MultiplayerContext& ctx, const DisagreementPacket* packet)
{
    // Validate reason is in range
    if (packet->reason > DISAGREEMENT_SELF_TARGET)
    {
        printf("[NET DISAGREEMENT RECV] reason=%u out of range — ignoring\n",
               (unsigned)packet->reason);
        return;
    }

    // Validate position and correction are finite
    if (!std::isfinite(packet->posX) || !std::isfinite(packet->posY) || !std::isfinite(packet->posZ) ||
        !std::isfinite(packet->correctionX) || !std::isfinite(packet->correctionY) || !std::isfinite(packet->correctionZ))
    {
        printf("[NET DISAGREEMENT RECV] non-finite position/correction — ignoring\n");
        return;
    }

    // Deduplicate: skip already-processed event IDs
    if (packet->eventId != 0)
    {
        if (ctx.processedDisagreementIds.count(packet->eventId))
        {
            printf("[NET DISAGREEMENT RECV] eventId=%u duplicate=1 skipped=1\n", packet->eventId);
            return;
        }
        ctx.processedDisagreementIds.insert(packet->eventId);

        // Bounded dedup set: remove old entries if it grows too large
        if (ctx.processedDisagreementIds.size() > 128)
            ctx.processedDisagreementIds.clear();
    }

    bool duplicate = ctx.processedDisagreementIds.count(packet->eventId) > 0;

    DisagreementEvent event;
    event.timeMs = nowMs();
    event.reason = (DisagreementReason)packet->reason;
    event.eventId = packet->eventId;
    event.relatedSerial = packet->relatedSerial;
    event.sourcePlayerId = packet->sourcePlayerId;
    event.targetPlayerId = packet->targetPlayerId;
    event.position = {packet->posX, packet->posY, packet->posZ};
    event.correction = {packet->correctionX, packet->correctionY, packet->correctionZ};
    // Safely copy description with bounded length (packet may not be null-terminated)
    {
        char buf[sizeof(packet->description) + 1];
        std::memcpy(buf, packet->description, sizeof(packet->description));
        buf[sizeof(packet->description)] = '\0';
        event.description = buf;
    }
    ctx.disagreementEvents.push_back(event);

    printf("[NET DISAGREEMENT RECV] eventId=%u shotSerial=%u shooter=%u target=%u "
           "reason=%u pos=(%.2f,%.2f,%.2f) correction=(%.2f,%.2f,%.2f) desc=\"%s\" "
           "duplicate=%d tick=%u\n",
           packet->eventId, packet->relatedSerial, packet->sourcePlayerId, packet->targetPlayerId,
           (unsigned)packet->reason,
           packet->posX, packet->posY, packet->posZ,
           packet->correctionX, packet->correctionY, packet->correctionZ,
           event.description.c_str(),
           (int)duplicate, packet->header.tick);
}

// ── Migration: reconnect ──────────────────────────────────────────────

void mpStartReconnect(MultiplayerContext& ctx)
{
    if (!ctx.active || ctx.reconnectToken.empty())
    {
        printf("[NET RECONNECT] cannot reconnect: no reconnect token\n");
        return;
    }

    ctx.connectionState = ConnectionState::Reconnecting;
    ctx.reconnectAttempts = 0;
    ctx.reconnectBackoffMs = 1000;
    ctx.lastReconnectAttemptMs = nowMs();
    printf("[NET RECONNECT] starting reconnect for player=%u with token=%s\n",
           ctx.localPlayerId, ctx.reconnectToken.c_str());
}

void mpTickReconnect(MultiplayerContext& ctx)
{
    if (ctx.connectionState != ConnectionState::Reconnecting)
        return;
    if (ctx.reconnectAttempts >= 10)
    {
        printf("[NET RECONNECT] max attempts reached, giving up\n");
        ctx.connectionState = ConnectionState::Disconnected;
        ctx.connectionStatus = "Reconnect failed";
        return;
    }

    const uint64_t now = nowMs();
    if (now - ctx.lastReconnectAttemptMs < ctx.reconnectBackoffMs)
        return;

    ctx.lastReconnectAttemptMs = now;
    ++ctx.reconnectAttempts;

    ReconnectRequestPacket packet{};
    packet.header.type = PACKET_RECONNECT_REQUEST;
    packet.header.tick = ctx.tick;
    packet.header.playerId = ctx.localPlayerId;
    std::memset(packet.reconnectToken, 0, sizeof(packet.reconnectToken));
    std::strncpy(packet.reconnectToken, ctx.reconnectToken.c_str(), sizeof(packet.reconnectToken) - 1);
    sendto(ctx.sock, (const char*)&packet, sizeof(packet), 0,
           (sockaddr*)&ctx.serverAddr, sizeof(ctx.serverAddr));
    ++ctx.packetsSent;

    printf("[NET RECONNECT] attempt=%d/10 backoff=%llums\n",
           ctx.reconnectAttempts, (unsigned long long)ctx.reconnectBackoffMs);

    ctx.reconnectBackoffMs = std::min<uint64_t>(ctx.reconnectBackoffMs * 2, 15000);
}

} // namespace MimitaNet
