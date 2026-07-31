// 07 21 2026, 17 10
/* purpose
* Owns multiplayer packet send helpers, connection teardown, and connection attempt setup.
* Keeps session lifecycle state reset before new UDP or ICE connections.
* Provides request helpers for gameplay packets outside the main receive loop.
* Does NOT parse server snapshots, validate movement reports, or render network entities.
* Does NOT own authoritative server gameplay state or packet binary schemas.
* Does NOT carry movement lifecycle ids across unrelated sessions.
*/

#include "network/multiplayer-context.h"
#include "network/packets.h"
#include "network/udp-transport.h"
#include "network/ice-transport.h"
#include "network/ice/ice-agent.h"
#include "network/ice/ice-config.h"
#include "network/coordinator-client.h"
#include "network/badconn/badconn.h"
#include "analytics/analytics-manager.h"
#include "debug/debug-log.h"

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

    // Per-client badconn simulator may delay, reorder, or drop this packet.
    if (badconn::processOutgoing(data, (size_t)bytes))
        return;

    // Use ICE transport if available
    if (ctx.transport)
    {
        ctx.transport->send(data, (size_t)bytes);
        ++ctx.packetsSent;
        return;
    }

    int sentBytes = sendto(ctx.sock, (const char*)data, bytes, 0,
                           (sockaddr*)&ctx.serverAddr, sizeof(ctx.serverAddr));
    if (sentBytes == SOCKET_ERROR)
        printf("[NET TX ERROR] sendto failed error=%d\n", WSAGetLastError());
    else
        printf("[NET TX] type=%d bytes=%d\n", ((PacketHeader*)data)->type, bytes);
    ++ctx.packetsSent;
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
    ctx.serverAddress.clear();
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
    ctx.projectileTerminals.clear();
    ctx.reliableEventSessionId = 0;
    ctx.processedReliableEventIds.clear();
    ctx.processedReliableEventOrder.clear();
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
    ctx.lastKnownSpawnGeneration = 0;
    ctx.nextMovementSequence = 1;
    ctx.latestLocalSnapshotTick = 0;
    ctx.latestAliveSnapshotTick = 0;
    ctx.gameplayActive = false;
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
    ctx.reliableEventSessionId = 0;
    ctx.processedReliableEventIds.clear();
    ctx.processedReliableEventOrder.clear();
    ctx.processedPelletBlastSerials.clear();
    ctx.lastReceivedShotSerial.clear();
    ctx.fireRejections.clear();
    ctx.processedRefundSerials.clear();
    ctx.pendingFireRequests.clear();
    ctx.pendingAttackRequests.clear();
    ctx.pendingReloadRequests.clear();
    ctx.pendingKnockback = glm::vec3(0.0f);
    ctx.pendingKnockbackSource.clear();
    ctx.incomingChatMessages.clear();
    badconn::noteConnectionTeardown();

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
    ctx.projectileTerminals.clear();
    ctx.reliableEventSessionId = 0;
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
    ctx.shotEvents.clear();
    ctx.lastReceivedShotSerial.clear();
    ctx.nextLocalShotSerial = 1;
    ctx.nextLocalProjectileFireSerial = 1;
    ctx.nextLocalMeleeAttackSerial = 1;
    ctx.lastPingSentMs = 0;
    ctx.localPingMs = 0;
    ctx.lastHeardServerMs = 0;
    ctx.lastDisconnectLogMs = 0;
    badconn::noteConnectionEstablished();

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

// ── Generic AttackRequest with pending tracking and retry ──────────────
uint32_t mpSendAttackRequest(MultiplayerContext& ctx,
    uint16_t weaponDefNetworkId,
    int16_t equippedSlot,
    const glm::vec3& aimOrigin,
    const glm::vec3& aimDirection,
    const glm::vec3& predictedMuzzle,
    uint8_t attackVariant)
{
    if (!ctx.active || !ctx.localPlayerId)
        return 0;

    uint32_t requestId = ctx.nextActionRequestId++;
    if (ctx.nextActionRequestId == 0)
        ctx.nextActionRequestId = 1;

    AttackRequestPacket req{};
    req.header.type = PACKET_ATTACK_REQUEST;
    req.header.tick = ctx.tick;
    req.header.playerId = ctx.localPlayerId;
    req.requestId = requestId;
    req.spawnGeneration = ctx.lastKnownSpawnGeneration;
    req.clientSimulationTick = ctx.latestLocalSnapshotTick;
    req.basedOnInputSequence = (uint16_t)std::min<uint32_t>(ctx.nextMovementSequence, 0xffffu);
    req.equippedSlot = equippedSlot;
    req.weaponDefNetworkId = weaponDefNetworkId;
    req.aimOriginX = aimOrigin.x;
    req.aimOriginY = aimOrigin.y;
    req.aimOriginZ = aimOrigin.z;
    glm::vec3 dir = glm::length(aimDirection) > 0.001f ? glm::normalize(aimDirection) : glm::vec3(1.0f, 0.0f, 0.0f);
    req.aimDirX = dir.x;
    req.aimDirY = dir.y;
    req.aimDirZ = dir.z;
    req.muzzlePosX = predictedMuzzle.x;
    req.muzzlePosY = predictedMuzzle.y;
    req.muzzlePosZ = predictedMuzzle.z;
    req.deterministicSeed = (uint32_t)(requestId * 73856093);
    req.attackVariant = attackVariant;

    mpSendPacket(ctx, &req, sizeof(req));

    // Track pending attack for retransmission
    MultiplayerContext::PendingAttackRequest pending;
    pending.requestId = requestId;
    pending.spawnGeneration = req.spawnGeneration;
    pending.weaponDefNetworkId = weaponDefNetworkId;
    pending.equippedSlot = equippedSlot;
    pending.clientSimulationTick = req.clientSimulationTick;
    pending.basedOnInputSequence = req.basedOnInputSequence;
    pending.aimOrigin = aimOrigin;
    pending.aimDirection = dir;
    pending.predictedMuzzle = predictedMuzzle;
    pending.deterministicSeed = req.deterministicSeed;
    pending.attackVariant = req.attackVariant;
    pending.firstSentMs = nowMs();
    pending.lastSentMs = nowMs();
    pending.attempts = 1;
    ctx.pendingAttackRequests[requestId] = pending;

    Debug::log(Debug::Category::Weapons, "[ATTACK REQUEST SEND] playerId=%u requestId=%u weaponDefNetId=%u spawnGen=%u pending=%zu\n",
               ctx.localPlayerId, requestId, weaponDefNetworkId, req.spawnGeneration,
               ctx.pendingAttackRequests.size());
    return requestId;
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

    IceConfiguration iceConfig = loadIceConfig();

    // Fetch TURN credentials from coordinator (overrides local ice-dev.json)
    {
        TurnCredentials turnCreds = coordinatorRequestTurnCredentials();
        if (turnCreds.ok && !turnCreds.credential.empty())
        {
            iceConfig.turn.host = turnCreds.host;
            iceConfig.turn.port = turnCreds.port;
            iceConfig.turn.username = turnCreds.username;
            iceConfig.turn.password = turnCreds.credential;
            printf("[ICE CONNECT] TURN credentials fetched from coordinator: %s:%u\n",
                   turnCreds.host.c_str(), turnCreds.port);
        }
        else
        {
            printf("[ICE CONNECT] WARNING: no TURN credentials from coordinator; "
                   "direct connections may fail behind symmetric NAT\n");
        }
    }

    // Retry ICE connection with backoff on transient coordinator/server failures
    constexpr int kMaxRetries = 5;
    constexpr uint64_t kInitialBackoffMs = 2000;
    uint64_t backoffMs = kInitialBackoffMs;

    for (int attempt = 1; attempt <= kMaxRetries; ++attempt)
    {
        printf("[ICE CONNECT] attempt %d/%d room=%s\n", attempt, kMaxRetries, roomCode.c_str());

        auto agentPtr = std::make_unique<IceAgent>();
        if (!agentPtr->initialize(iceConfig))
        {
            printf("[ICE CONNECT] agent initialization failed (attempt %d)\n", attempt);
            if (attempt < kMaxRetries) { Sleep(backoffMs); backoffMs *= 2; }
            continue;
        }
        ctx.connectionState = ConnectionState::NatNegotiating;
        ctx.connectionStatus = "ICE: gathering candidates...";

        if (!agentPtr->gatherCandidates())
        {
            printf("[ICE CONNECT] candidate gathering failed (attempt %d)\n", attempt);
            if (attempt < kMaxRetries) { Sleep(backoffMs); backoffMs *= 2; }
            continue;
        }
        if (!waitForIceAgentState(*agentPtr, IceAgentState::GatheringComplete, 15000))
        {
            printf("[ICE CONNECT] gather timeout (15s) (attempt %d)\n", attempt);
            if (attempt < kMaxRetries) { Sleep(backoffMs); backoffMs *= 2; }
            continue;
        }
        printf("[ICE CONNECT] candidates gathered (attempt %d)\n", attempt);

        // Exchange SDP via coordinator (two-phase offer/answer)
        std::string sessionId = "client_" + std::to_string(GetCurrentProcessId())
            + "_" + std::to_string(nowMs()) + "_" + std::to_string(attempt);
        ctx.connectionStatus = "ICE: contacting coordinator...";

        auto beginJoin = coordinatorIceBeginJoin(roomCode, sessionId, agentPtr->localSdp());
        if (!beginJoin.ok || beginJoin.requestId.empty())
        {
            printf("[ICE CONNECT] coordinator ICE begin-join failed "
                   "room=%s error=%s (attempt %d)\n",
                   roomCode.c_str(),
                   beginJoin.errorCode.c_str(), attempt);
            if (attempt < kMaxRetries) { Sleep(backoffMs); backoffMs *= 2; }
            continue;
        }

        printf("[ICE CONNECT] begin-join accepted room=%s request=%s (attempt %d)\n",
               roomCode.c_str(),
               beginJoin.requestId.substr(0, std::min<size_t>(12, beginJoin.requestId.size())).c_str(),
               attempt);

        ctx.connectionStatus = "ICE: waiting for server answer...";

        std::string hostIceDescription = beginJoin.hostIceDescription;
        const uint64_t answerWaitStartedMs = nowMs();
        constexpr uint64_t kHostAnswerTimeoutMs = 15000;
        bool retriedBeginJoin = false;

        while (hostIceDescription.empty() &&
               nowMs() - answerWaitStartedMs < kHostAnswerTimeoutMs)
        {
            auto pollResult =
                coordinatorIceClientPoll(roomCode, beginJoin.requestId);

            if (pollResult.ok && !pollResult.hostIceDescription.empty())
            {
                hostIceDescription = pollResult.hostIceDescription;
                break;
            }

            if (pollResult.status == "failed" ||
                pollResult.status == "expired")
            {
                printf("[ICE CONNECT] server answer failed "
                       "room=%s request=%s status=%s error=%s (attempt %d)\n",
                       roomCode.c_str(),
                       beginJoin.requestId.substr(0, std::min<size_t>(12, beginJoin.requestId.size())).c_str(),
                       pollResult.status.c_str(),
                       pollResult.errorCode.c_str(), attempt);
                break;
            }

            // If pending for >5s, cancel the stale request and create a fresh one.
            // This handles coordinator-side request loss or race conditions.
            if (!retriedBeginJoin && nowMs() - answerWaitStartedMs > 5000)
            {
                printf("[ICE CONNECT] request stale, retrying begin-join room=%s (attempt %d)\n",
                       roomCode.c_str(), attempt);
                coordinatorIceRequestComplete(roomCode, beginJoin.requestId);
                std::string retrySessionId = sessionId + "_retry";
                auto retryJoin = coordinatorIceBeginJoin(roomCode, retrySessionId, agentPtr->localSdp());
                if (retryJoin.ok && !retryJoin.requestId.empty())
                {
                    beginJoin.requestId = retryJoin.requestId;
                    beginJoin.joinToken = retryJoin.joinToken;
                    hostIceDescription = retryJoin.hostIceDescription;
                    retriedBeginJoin = true;
                    printf("[ICE CONNECT] retry begin-join accepted room=%s request=%s\n",
                           roomCode.c_str(),
                           retryJoin.requestId.substr(0, std::min<size_t>(12, retryJoin.requestId.size())).c_str());
                    continue;
                }
                else
                {
                    printf("[ICE CONNECT] retry begin-join also failed\n");
                }
            }

            Sleep(100);
        }

        if (hostIceDescription.empty())
        {
            printf("[ICE CONNECT] server answer timeout "
                   "room=%s request=%s (attempt %d)\n",
                   roomCode.c_str(),
                   beginJoin.requestId.substr(0, std::min<size_t>(12, beginJoin.requestId.size())).c_str(),
                   attempt);
            if (attempt < kMaxRetries) { Sleep(backoffMs); backoffMs *= 2; }
            continue;
        }

        printf("[ICE CONNECT] received host SDP request=%s bytes=%zu (attempt %d)\n",
               beginJoin.requestId.substr(0, std::min<size_t>(12, beginJoin.requestId.size())).c_str(),
               hostIceDescription.size(), attempt);

        if (!agentPtr->setRemoteDescription(hostIceDescription))
        {
            printf("[ICE CONNECT] setRemoteDescription failed (attempt %d)\n", attempt);
            if (attempt < kMaxRetries) { Sleep(backoffMs); backoffMs *= 2; }
            continue;
        }
        printf("[ICE CONNECT] remote description applied (attempt %d)\n", attempt);
        ctx.connectionStatus = "ICE: connecting...";

        std::string earlyRecv;
        if (!waitForIceAgentState(*agentPtr, IceAgentState::Connected, 30000, &earlyRecv))
        {
            printf("[ICE CONNECT] connection timeout (30s) (attempt %d)\n", attempt);
            if (attempt < kMaxRetries) { Sleep(backoffMs); backoffMs *= 2; }
            continue;
        }
        printf("[ICE CONNECT] ICE connection established (attempt %d)\n", attempt);
        agentPtr->logSelectedPath();

        // Success — create transport and set up context
        ctx.transport = std::make_unique<IceTransport>(std::move(agentPtr));
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
        ctx.joinToken = beginJoin.joinToken;
        ctx.serverAddress = "ice:" + roomCode;
        ctx.connected = false;
        ctx.connectFailed = false;
        ctx.connectionStatus = "Connected via ICE";
        badconn::noteConnectionEstablished();
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

    printf("[ICE CONNECT] FATAL: all %d attempts failed room=%s\n", kMaxRetries, roomCode.c_str());

    // Check if room still exists — helps user understand the failure
    CoordinatorLookupResult lookup = coordinatorIceLookup(roomCode);
    if (!lookup.exists)
    {
        printf("[ICE CONNECT] room %s no longer exists on coordinator\n", roomCode.c_str());
    }
    else
    {
        printf("[ICE CONNECT] room %s still exists (%d/%d players) — "
               "connection failed. Check NAT/firewall or set MIMITA_TURN_PASSWORD env var.\n",
               roomCode.c_str(), lookup.players, lookup.maxPlayers);
    }

    netShutdown();
    return false;
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
