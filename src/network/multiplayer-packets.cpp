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
#include "network/community-match-client.h"
#include "network/packets.h"
#include "network/connection-health.h"
#include "network/simulation-constants.h"
#include "network/udp-transport.h"
#include "network/ice-transport.h"
#include "network/ice/ice-agent.h"
#include "network/ice/ice-config.h"
#include "network/coordinator-client.h"
#include "network/badconn/badconn.h"
#include "config/networking-config.h"
#include "analytics/analytics-manager.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "ragdoll/ragdoll-mode.h"
#include "physics/runtime-objects.h"
#include "notifications/notifications.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>

namespace MimitaNet {

uint32_t mpFireRenderTick(const MultiplayerContext& ctx, uint32_t fallbackNewestTick)
{
    const auto& interpCfg = NetworkingConfig::instance().data().remotePlayers;
    if (!interpCfg.directRender && interpCfg.enabled)
    {
        // Remote bodies render at `estimatedServerNow - renderDelay`, where
        // renderDelay is the per-entity adaptive buffer depth that grows under
        // jitter/loss (up to adaptive_snapshot_buffer.maximum_delay_ms). The
        // rewind tick the server validates against must be the exact tick the
        // shooter was looking at, so subtract the deepest currently-rendered
        // entity's adaptive delay (not just the base interpolation delay).
        // Without this the server rewinds too shallow under jitter and the
        // predicted hit lands on a newer pose than the shooter actually saw.
        if (ctx.interpolationClockStarted && ctx.interpolationRenderTick > 0.0)
        {
            double renderDelay = NetworkingConfig::instance()
                .effectiveRemoteInterpolationDelaySeconds();
            for (const auto& kv : ctx.remotePlayerInterpolation)
                renderDelay = std::max(renderDelay, kv.second.adaptiveDelaySeconds);
            for (const auto& kv : ctx.remoteNpcInterpolation)
                renderDelay = std::max(renderDelay, kv.second.adaptiveDelaySeconds);

            const double delayTicks =
                renderDelay * (double)GAMEPLAY_SIMULATION_HZ;
            const double viewTick = ctx.interpolationRenderTick - delayTicks;
            if (viewTick > 1.0)
                return (uint32_t)std::floor(viewTick);
        }
        const uint32_t newest = ctx.latestServerTick != 0
            ? ctx.latestServerTick
            : ctx.latestLocalSnapshotTick;
        if (newest != 0)
            return newest;
    }
    return fallbackNewestTick;
}

// Fire tick for a SPECIFIC claimed target: uses that entity's own adaptive
// render delay so the server rewinds to exactly the pose the shooter rendered
// for that target (not the deepest-rendered entity across the whole scene).
uint32_t mpFireRenderTickForTarget(const MultiplayerContext& ctx, uint32_t targetId,
                                   uint32_t fallbackNewestTick)
{
    if (targetId != 0 && ctx.interpolationClockStarted &&
        ctx.interpolationRenderTick > 0.0)
    {
        double delay = -1.0;
        {
            auto it = ctx.remotePlayerInterpolation.find(targetId);
            if (it != ctx.remotePlayerInterpolation.end())
                delay = it->second.adaptiveDelaySeconds;
        }
        if (delay < 0.0)
        {
            auto it = ctx.remoteNpcInterpolation.find(targetId);
            if (it != ctx.remoteNpcInterpolation.end())
                delay = it->second.adaptiveDelaySeconds;
        }
        if (delay > 0.0)
        {
            const double delayTicks = delay * (double)GAMEPLAY_SIMULATION_HZ;
            const double viewTick = ctx.interpolationRenderTick - delayTicks;
            if (viewTick > 1.0)
                return (uint32_t)std::floor(viewTick);
        }
    }
    return mpFireRenderTick(ctx, fallbackNewestTick);
}

bool mpSendPacket(MultiplayerContext& ctx, const void* data, int bytes)
{
    if (!ctx.active || !data || bytes <= 0)
        return false;
    if (!ctx.transport && ctx.sock == INVALID_SOCKET)
        return false;

    const auto packetType = ((const PacketHeader*)data)->type;
    const bool godballClaim = packetType == PACKET_GODBALL_HIT_CLAIM;
    if (godballClaim)
    {
        Debug::warn(Debug::Category::Weapons,
            "[GODBALL_DBG] CLAIM_SEND_BEGIN type=%u bytes=%d transport=%d socketValid=%d",
            (unsigned)packetType, bytes, (int)(ctx.transport != nullptr),
            (int)(ctx.sock != INVALID_SOCKET));
    }

    // Per-client badconn simulator may delay, reorder, or drop this packet.
    if (badconn::processOutgoing(data, (size_t)bytes))
    {
        if (godballClaim)
        {
            Debug::warn(Debug::Category::Weapons,
                "[GODBALL_DBG] CLAIM_SEND_RESULT type=%u sent=0 reason=badconn-drop",
                (unsigned)packetType);
        }
        return false;
    }

    ctx.lastPacketSentMs = nowMs();

    // Use ICE transport if available
    if (ctx.transport)
    {
        const bool sent = ctx.transport->send(data, (size_t)bytes);
        if (sent)
            ++ctx.packetsSent;
        if (godballClaim)
        {
            Debug::warn(Debug::Category::Weapons,
                "[GODBALL_DBG] CLAIM_SEND_RESULT type=%u sent=%d packetsSent=%u",
                (unsigned)packetType, (int)sent, ctx.packetsSent);
        }
        return sent;
    }

    int sentBytes = sendto(ctx.sock, (const char*)data, bytes, 0,
                           (sockaddr*)&ctx.serverAddr, sizeof(ctx.serverAddr));
    if (sentBytes == SOCKET_ERROR)
        printf("[NET TX ERROR] sendto failed error=%d\n", WSAGetLastError());
    else
        printf("[NET TX] type=%d bytes=%d\n", ((PacketHeader*)data)->type, bytes);
    const bool sent = sentBytes == bytes;
    if (sent)
        ++ctx.packetsSent;
    if (godballClaim)
    {
        Debug::warn(Debug::Category::Weapons,
            "[GODBALL_DBG] CLAIM_SEND_RESULT type=%u sent=%d sentBytes=%d packetsSent=%u",
            (unsigned)packetType, (int)sent, sentBytes, ctx.packetsSent);
    }
    return sent;
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

static void logReconnectResourceProbe(const MultiplayerContext& ctx,
                                      const char* phase,
                                      const char* policyName = "not-applicable")
{
    uint64_t incomingAvatarDeclaredBytes = 0;
    uint64_t incomingAvatarChunkBytes = 0;
    for (const auto& item : ctx.incomingAvatarAssets) {
        incomingAvatarDeclaredBytes += item.second.totalBytes;
        for (const auto& chunk : item.second.chunks)
            incomingAvatarChunkBytes += chunk.size();
    }

    uint64_t avatarManifestBytes = 0;
    for (const auto& item : ctx.remoteAvatarManifests) {
        avatarManifestBytes += item.second.avatarJson.size();
        for (const auto& chunk : item.second.manifestChunks)
            avatarManifestBytes += chunk.size();
    }

    uint64_t avatarUploadBytes = 0;
    for (const auto& chunk : ctx.avatarNetworkUploadQueue)
        avatarUploadBytes += chunk.size();

    uint64_t snapshotChunkCount = 0;
    for (const auto& item : ctx.snapshotChunkBuffers)
        snapshotChunkCount += item.second.chunks.size();

    StructuredLogger::instance().writeEvent(
        StructuredCategory::Network, StructuredLevel::Important,
        "network.reconnect.resource-probe",
        "reconnect:" + std::to_string(ctx.localPlayerId),
        phase, ctx.tick,
        {{"phase", phase},
         {"disconnect_policy", policyName},
         {"connection_state", connectionStateName(ctx.connectionState)},
         {"reconnect_attempts", ctx.reconnectAttempts},
         {"remote_players", ctx.remotePlayers.size()},
         {"remote_npcs", ctx.remoteNpcs.size()},
         {"remote_player_interpolation", ctx.remotePlayerInterpolation.size()},
         {"remote_npc_interpolation", ctx.remoteNpcInterpolation.size()},
         {"player_registry", ctx.playerRegistry.size()},
         {"remote_avatar_manifests", ctx.remoteAvatarManifests.size()},
         {"remote_avatar_manifest_bytes", avatarManifestBytes},
         {"incoming_avatar_assets", ctx.incomingAvatarAssets.size()},
         {"incoming_avatar_declared_bytes", incomingAvatarDeclaredBytes},
         {"incoming_avatar_chunk_bytes", incomingAvatarChunkBytes},
         {"avatar_upload_queue_chunks", ctx.avatarNetworkUploadQueue.size()},
         {"avatar_upload_queue_bytes", avatarUploadBytes},
         {"snapshot_chunk_buffers", ctx.snapshotChunkBuffers.size()},
         {"snapshot_chunks", snapshotChunkCount},
         {"snapshot_chunk_payload_bytes",
             snapshotChunkCount * sizeof(SnapshotChunkPacket)},
         {"processed_reliable_events", ctx.processedReliableEventIds.size()},
         {"pending_fire_requests", ctx.pendingFireRequests.size()},
         {"pending_attack_requests", ctx.pendingAttackRequests.size()},
         {"pending_hit_claims", ctx.pendingHitClaims.size()},
         {"pending_reload_requests", ctx.pendingReloadRequests.size()}},
        __FILE__, __LINE__, __func__);
}

void teardownPreviousSession(MultiplayerContext& ctx, DisconnectPolicy policy)
{
    logReconnectResourceProbe(ctx, "teardown-before", disconnectPolicyName(policy));
    // Log the teardown
    printf("[NET TEARDOWN] policy=%s reason=%s\n",
           disconnectPolicyName(policy),
           ctx.connectionStatus.c_str());

    // CommunityMatchClient is a process-wide singleton, so clearing the
    // MultiplayerContext alone would leave an old server countdown cached.
    CommunityMatchClient::instance().reset();

    // Policy: keep the reconnect token for recoverable failures (Timeout and
    // ConnectionFailure) so the client keeps reconnecting to the SAME session
    // for the full grace window instead of giving up instantly. Only clear it
    // when the session is intentionally left/rejected/kicked/replaced.
    if (policy == DisconnectPolicy::Leave ||
        policy == DisconnectPolicy::NewConnection ||
        policy == DisconnectPolicy::Rejected ||
        policy == DisconnectPolicy::AuthFailure ||
        policy == DisconnectPolicy::ServerStopped)
        ctx.reconnectToken.clear();

    // Clear join token, room code, session identity
    ctx.joinToken.clear();
    ctx.vipJoinTicket.clear();
    ctx.vipJoinTicketRequested = false;
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
    mpResetAvatarNetwork(ctx);
    ctx.remotePlayerInterpolation.clear();
    ctx.remoteNpcInterpolation.clear();
    ctx.remoteRagdoll.clear();
    RagdollModeSystem::instance().clearAllReplicatedBodies();
    RagdollModeSystem::instance().clearCorpses();
    ctx.interpolationRenderTick = 0.0;
    ctx.interpolationClockStarted = false;
    ctx.interpolationClockLastUpdateMs = 0;
    ctx.interpolationFrameNumber = 0;
    ctx.interpolationReanchorCount = 0;
    ctx.lastInterpolationClockStepMs = 0.0;
    ctx.lastInterpolationReanchorMagnitudeMs = 0.0;
    ctx.lastInterpolationReanchorReason.clear();
    ctx.lastClockAnchorServerTick = 0;
    ctx.networkProjectiles.clear();
    ctx.projectileTerminals.clear();
    ctx.reliableEventSessionId = 0;
    ctx.processedReliableEventIds.clear();
    ctx.processedReliableEventOrder.clear();
    ctx.playerRegistry.clear();
    ctx.pendingVipStyles.clear();
    mpResetAvatarNetwork(ctx);
    ctx.predictedProjectileIds.clear();
    ctx.predictedExplosions.clear();
    ctx.predictedSelfKnockbacks.clear();
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
    ctx.latestAppliedMembershipTick = 0;
    ctx.gameplayActive = false;
    ctx.pendingTeleportPosition = glm::vec3(0.0f);
    ctx.pendingTeleportSentMs = 0;
    ctx.awaitingTeleportAck = false;
    ctx.proceduralTeleportStartEpoch = 0;
    ctx.proceduralTeleportSentMs = 0;
    ctx.proceduralTeleportPending = false;
    ctx.awaitingExplodeDeath = false;
    ctx.explodeRequestLastSendMs = 0;
    ctx.teleportResync = false;
    ctx.transformEpoch = 0;

    // Clear pending requests and events
    ctx.shotEvents.clear();
    ctx.pendingShotEvents.clear();
    ctx.pendingPelletBlastEvents.clear();
    ctx.disagreementEvents.clear();
    ctx.processedDisagreementIds.clear();
    ctx.processedDisagreementOrder.clear();
    ctx.hasPendingServerPositionCorrection = false;
    ctx.reliableEventSessionId = 0;
    ctx.processedReliableEventIds.clear();
    ctx.processedReliableEventOrder.clear();
    ctx.processedPelletBlastSerials.clear();
    ctx.lastReceivedShotSerial.clear();
    ctx.fireRejections.clear();
    ctx.processedRefundSerials.clear();
    ctx.pendingFireRequests.clear();
    ctx.pendingAttackRequests.clear();
    ctx.pendingHitClaims.clear();
    ctx.recentInputCommands.clear();
    ctx.pendingReloadRequests.clear();
    ctx.pendingKnockbacks.clear();
    ctx.incomingChatMessages.clear();
    ctx.processedChatMessageIds.clear();
    ctx.pendingChatRequests.clear();
    badconn::noteConnectionTeardown();

    // Reset reconnect timers
    ctx.reconnectAttempts = 0;
    ctx.reconnectBackoffMs =
        (uint64_t)NetworkingConfig::instance().data()
            .retries.reconnectInitialBackoffMs;
    ctx.lastReconnectAttemptMs = 0;
    ctx.reconnectGraceDeadlineMs = 0;
    ctx.disconnectStartedMs = 0;
    ctx.lastPacketSentMs = 0;
    ctx.remoteConnectionStates.clear();
    ctx.lastNotifiedConnectionState = ConnectionState::Disconnected;

    // Preserve monotonically increasing serials (NEVER reset):
    //   nextLocalProjectileFireSerial
    //   nextLocalShotSerial
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
    logReconnectResourceProbe(ctx, "teardown-after", disconnectPolicyName(policy));
}

bool mpInit(MultiplayerContext& ctx, const std::string& address, const std::string& playerName)
{
    if (ctx.active || ctx.transport || ctx.sock != INVALID_SOCKET)
    {
        printf("[NET CONNECT] replacing stale session before direct join address=%s\n",
               address.c_str());
        mpIceConnectCancel();
        const bool ownedUdpSocket = ctx.sock != INVALID_SOCKET;
        teardownPreviousSession(ctx, DisconnectPolicy::NewConnection);
        if (ownedUdpSocket)
            netShutdown();
    }

    // A new connection may target a server whose match IDs restarted at a
    // lower value. Reset cached community state before packet ordering starts.
    CommunityMatchClient::instance().reset();
    ctx.serverAddress = address;
    ++ctx.connectionAttemptId;
    ctx.joinRequestSends = 0;

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
    ctx.lastSnapshotDiagnosticsMs = 0;
    ctx.lastSnapshotDiagnosticArrivalMs = 0;
    ctx.serverIdentityMismatch = false;
    ctx.connectStartMs = nowMs();
    ctx.packetsSent = 0;
    ctx.packetsReceived = 0;
    ctx.snapshotsReceived = 0;
    ctx.snapshotsMissed = 0;
    ctx.remotePlayers.clear();
    ctx.remoteNpcs.clear();
    ctx.remotePlayerInterpolation.clear();
    ctx.remoteNpcInterpolation.clear();
    ctx.remoteRagdoll.clear();
    RagdollModeSystem::instance().clearAllReplicatedBodies();
    ctx.interpolationRenderTick = 0.0;
    ctx.interpolationClockStarted = false;
    ctx.interpolationClockLastUpdateMs = 0;
    ctx.interpolationFrameNumber = 0;
    ctx.interpolationReanchorCount = 0;
    ctx.lastInterpolationClockStepMs = 0.0;
    ctx.lastInterpolationReanchorMagnitudeMs = 0.0;
    ctx.lastInterpolationReanchorReason.clear();
    ctx.lastClockAnchorServerTick = 0;
    ctx.networkProjectiles.clear();
    ctx.projectileTerminals.clear();
    ctx.reliableEventSessionId = 0;
    ctx.playerRegistry.clear();
    ctx.pendingVipStyles.clear();
    ctx.approvedLocalName.clear();
    ctx.hasLocalServerPosition = false;
    ctx.localPlayerReconciled = false;
    ctx.lastLocalCorrectionLogMs = 0;
    ctx.pendingTeleportPosition = glm::vec3(0.0f);
    ctx.pendingTeleportSentMs = 0;
    ctx.awaitingTeleportAck = false;
    ctx.proceduralTeleportStartEpoch = 0;
    ctx.proceduralTeleportSentMs = 0;
    ctx.proceduralTeleportPending = false;
    ctx.awaitingExplodeDeath = false;
    ctx.localServerVelocity = glm::vec3(0.0f);
    ctx.localServerYaw = 0.0f;
    ctx.localServerOnGround = false;
    ctx.localServerHealth = 100;
    ctx.connected = false;
    ctx.connectFailed = false;
    ctx.connectionStatus = "Connecting...";
    ctx.shotEvents.clear();
    ctx.pendingShotEvents.clear();
    ctx.pendingPelletBlastEvents.clear();
    ctx.pendingHitClaims.clear();
    ctx.lastReceivedShotSerial.clear();
    ctx.nextLocalShotSerial = 1;
    ctx.nextLocalProjectileFireSerial = 1;
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
    // Cancel any in-flight async ICE connect first so the worker thread is
    // finished before this context is torn down (no concurrent ctx access).
    mpIceConnectCancel();

    if (!ctx.active)
    {
        CommunityMatchClient::instance().reset();
        return;
    }

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
    clearRuntimeObjects("network-shutdown");
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
    ctx.explodeRequestStartSpawnGeneration = ctx.lastKnownSpawnGeneration;
    ctx.explodeRequestLastSendMs = 0;
    mpSendPacket(ctx, &request, sizeof(request));
    ctx.explodeRequestLastSendMs = nowMs();
}

// ── Generic AttackRequest with pending tracking and retry ──────────────
uint32_t mpSendAttackRequest(MultiplayerContext& ctx,
    uint16_t weaponDefNetworkId,
    int16_t equippedSlot,
    const glm::vec3& aimOrigin,
    const glm::vec3& aimDirection,
    const glm::vec3& predictedMuzzle,
    uint8_t attackVariant,
    uint32_t claimedTargetId,
    const glm::vec3& claimedHit,
    uint8_t claimedBodyPart,
    uint16_t movesetRevision,
    uint32_t movesetHash)
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
    req.clientSimulationTick = claimedTargetId != 0
        ? mpFireRenderTickForTarget(ctx, claimedTargetId, ctx.latestLocalSnapshotTick)
        : mpFireRenderTick(ctx, ctx.latestLocalSnapshotTick);
    // Diagnostic: confirm the fire tick stays in the server's tick domain (close
    // to latestServerTick, not seconds behind). Throttled to once per second.
    Debug::logThrottled(Debug::Category::Networking, "fire-tick-health", 1.0,
        "[FIRE TICK] playerId=%u renderClock=%.1f latestServerTick=%u "
        "latestLocalSnapshot=%u fireTick=%u claimed=%u\n",
        ctx.localPlayerId, ctx.interpolationRenderTick,
        ctx.latestServerTick, ctx.latestLocalSnapshotTick,
        req.clientSimulationTick, claimedTargetId);
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
    req.claimedTargetId = claimedTargetId;
    req.claimedHitX = claimedHit.x;
    req.claimedHitY = claimedHit.y;
    req.claimedHitZ = claimedHit.z;
    req.claimedBodyPart = claimedBodyPart;
    // Melee move-set revision for client/server alignment, carried in the
    // two unused reserved bytes so the packet size does not change.
    req.reservedAttack[0] = (uint8_t)(movesetRevision & 0xff);
    req.reservedAttack[1] = (uint8_t)((movesetRevision >> 8) & 0xff);
    req.movesetHash = movesetHash;

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
    pending.movesetRevision = movesetRevision;
    pending.movesetHash = movesetHash;
    pending.claimedTargetId = claimedTargetId;
    pending.claimedHit = claimedHit;
    pending.claimedBodyPart = claimedBodyPart;
    pending.firstSentMs = nowMs();
    pending.lastSentMs = nowMs();
    pending.attempts = 1;
    ctx.pendingAttackRequests[requestId] = pending;

    // Record the client's local hit claim so a server disagreement (rejected
    // attack, different target, or a miss) can spawn a "HIT REJECTED" effect.
    if (claimedTargetId != 0)
    {
        ++ctx.predictedHits;
        MultiplayerContext::PendingHitClaim claim;
        claim.requestId = requestId;
        claim.claimedTargetId = claimedTargetId;
        claim.claimedHit = claimedHit;
        claim.sentMs = nowMs();
        ctx.pendingHitClaims[requestId] = claim;
    }

    Debug::log(Debug::Category::Weapons, "[ATTACK REQUEST SEND] playerId=%u requestId=%u weaponDefNetId=%u spawnGen=%u pending=%zu\n",
               ctx.localPlayerId, requestId, weaponDefNetworkId, req.spawnGeneration,
               ctx.pendingAttackRequests.size());
    return requestId;
}

void mpSendServerCommand(MultiplayerContext& ctx, const std::string& command)
{
    if (!ctx.active || !ctx.localPlayerId)
        return;

    if (command == "procedural_world_teleport_highest")
    {
        // The server chooses the destination.  Mark the handoff before the
        // command leaves the client so the next movement packet cannot carry
        // the old predicted room position while the command is in flight.
        ctx.proceduralTeleportStartEpoch = ctx.transformEpoch;
        ctx.proceduralTeleportSentMs = nowMs();
        ctx.proceduralTeleportPending = true;
        ctx.teleportResync = true;
        printf("[CLIENT PROCEDURAL TELEPORT BEGIN] playerId=%u startEpoch=%u\n",
               ctx.localPlayerId, (unsigned)ctx.proceduralTeleportStartEpoch);
    }

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
        case ConnectionState::WeakConnection:   return "WeakConnection";
        case ConnectionState::ReconnectFailed:  return "ReconnectFailed";
        case ConnectionState::HostClosed:       return "HostClosed";
        case ConnectionState::Kicked:           return "Kicked";
        case ConnectionState::ServerCrashed:    return "ServerCrashed";
    }
    return "Unknown";
}

// ── Bounded connection/session diagnostics ────────────────────────────
// These helpers emit one structured record per transition. They never poll or
// run per frame; callers must only call them when a stage actually changes.

static std::string runtimeBuildId()
{
    const char* value = std::getenv("MIMITA_BUILD_ID");
    return value && *value ? std::string(value) : std::string();
}

static void writeConnectStage(const std::string& roomCode,
                              const std::string& transport,
                              const std::string& serverAddress,
                              uint32_t attempt, uint32_t maxAttempts,
                              const char* stage, const char* result,
                              uint64_t elapsedMs, uint64_t timeoutMs,
                              const std::string& errorCode,
                              const std::string& reason)
{
    const bool failed = std::strcmp(result, "failed") == 0 ||
                        std::strcmp(result, "timeout") == 0 ||
                        std::strcmp(result, "rejected") == 0 ||
                        std::strcmp(result, "cancelled") == 0;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Network,
        failed ? StructuredLevel::Errors : StructuredLevel::Important,
        "network.connect.stage", roomCode,
        reason.empty() ? (failed ? "connection stage failed" : "connection stage")
                       : reason,
        0,
        nlohmann::json{
            {"attempt", attempt}, {"max_attempts", maxAttempts},
            {"stage", stage}, {"result", result},
            {"elapsed_ms", elapsedMs}, {"timeout_ms", timeoutMs},
            {"room_code", roomCode}, {"transport", transport},
            {"server_address", serverAddress},
            {"build_id", runtimeBuildId()},
            {"error_code", errorCode},
            {"error_status", failed ? result : ""}},
        __FILE__, __LINE__, __func__);
}

void mpLogConnectStage(const MultiplayerContext& ctx,
                       const char* stage, const char* result,
                       uint64_t elapsedMs, uint64_t timeoutMs,
                       const std::string& errorCode,
                       const std::string& reason)
{
    writeConnectStage(ctx.roomCode.empty() ? ctx.currentRoomCode : ctx.roomCode,
                      ctx.transport ? "ice" : "udp", ctx.serverAddress,
                      ctx.connectionAttemptId, 0, stage, result,
                      elapsedMs, timeoutMs, errorCode, reason);
}

void mpLogConnectStageForRoom(const std::string& roomCode,
                              const std::string& transport,
                              const std::string& serverAddress,
                              uint32_t attempt, uint32_t maxAttempts,
                              const char* stage, const char* result,
                              uint64_t elapsedMs, uint64_t timeoutMs,
                              const std::string& errorCode,
                              const std::string& reason)
{
    writeConnectStage(roomCode, transport, serverAddress, attempt, maxAttempts,
                      stage, result, elapsedMs, timeoutMs, errorCode, reason);
}

// ── ICE client connection ──────────────────────────────────────────────
// Connects to an ICE-enabled server via the coordinator.
// Creates an IceAgent, exchanges SDP through coordinator, establishes
// an encrypted (or direct) P2P path, and wraps it in IceTransport.
// On return, ctx.transport is set to the ICE transport.
// The caller must still send Hello/JoinRequest through the normal flow.

static bool waitForIceAgentState(IceAgent& agent, IceAgentState target,
                                   int timeoutMs, std::string* earlyRecv = nullptr,
                                   const std::atomic<bool>* cancel = nullptr)
{
    int waited = 0;
    while (waited < timeoutMs)
    {
        if (cancel && cancel->load(std::memory_order_relaxed))
            return false;
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

// ── Async ICE connect (background thread) ─────────────────────────────
// The ICE connect handshake (gather, SDP exchange, ICE negotiation) runs on
// a worker thread so the game never blocks or freezes. The worker does NOT
// touch the MultiplayerContext; on success it leaves a finished transport in
// the job for the main thread to install via mpInstallIceConnectSuccess.

namespace {

struct IceConnectJob
{
    std::mutex mutex;
    IceConnectStatus status;
    std::atomic<bool> cancel{false};
    std::thread thread;
};

IceConnectJob* gIceConnectJob = nullptr;

// Interruptible sleep: returns false early when cancel is requested.
bool sleepInterruptible(uint64_t ms, const std::atomic<bool>& cancel)
{
    uint64_t waited = 0;
    while (waited < ms)
    {
        if (cancel.load(std::memory_order_relaxed))
            return false;
        Sleep(25);
        waited += 25;
    }
    return !cancel.load(std::memory_order_relaxed);
}

void iceConnectSetMessage(IceConnectJob* job, const std::string& msg)
{
    std::lock_guard<std::mutex> lock(job->mutex);
    job->status.message = msg;
}

void iceConnectFinish(IceConnectJob* job, bool success, const char* msg)
{
    std::lock_guard<std::mutex> lock(job->mutex);
    job->status.message = msg;
    job->status.done = true;
    job->status.success = success;
}

void iceConnectWorker(std::string roomCode, std::string playerName)
{
    IceConnectJob* job = gIceConnectJob;
    if (!job)
        return;

    auto cancelled = [&]() { return job->cancel.load(std::memory_order_relaxed); };

    // A newly registered room can take a moment to appear on every coordinator
    // read path. Treat this lookup as diagnostic; begin-join retries below are
    // authoritative, so a transient propagation delay must not fail the join.
    iceConnectSetMessage(job, "Checking room...");
    {
        const uint64_t lookupStartMs = nowMs();
        CoordinatorLookupResult lookup = coordinatorIceLookup(roomCode);
        mpLogConnectStageForRoom(roomCode, "ice", "ice:" + roomCode, 1, 1,
            "room_lookup",
            lookup.reachable ? (lookup.exists ? "ok" : "not_visible") : "unreachable",
            nowMs() - lookupStartMs, 5000, lookup.status,
            lookup.reachable ? "" : "coordinator lookup failed");
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Network, StructuredLevel::Important,
            "network.ice-room-lookup", roomCode,
            lookup.exists
                ? "client confirmed room before ICE join"
                : "room is not visible yet; ICE join will retry registration propagation",
            0,
            {{"room_code", roomCode}, {"reachable", lookup.reachable},
             {"exists", lookup.exists}, {"status", lookup.status},
             {"join_will_retry", lookup.reachable && !lookup.exists}},
            __FILE__, __LINE__, __FUNCTION__);
    }
    if (cancelled()) { iceConnectFinish(job, false, "Cancelled"); return; }

    printf("[ICE CONNECT] starting ICE connection to room=%s as \"%s\"\n",
           roomCode.c_str(), playerName.c_str());

    if (!netStartup())
    {
        printf("[ICE CONNECT] FATAL: WSAStartup failed\n");
        iceConnectFinish(job, false, "Network init failed");
        return;
    }

    IceConfiguration iceConfig = loadIceConfig();

    // Fetch TURN credentials from coordinator (overrides local ice-dev.json)
    {
        const uint64_t turnStartMs = nowMs();
        TurnCredentials turnCreds = coordinatorRequestTurnCredentials();
        mpLogConnectStageForRoom(roomCode, "ice", "ice:" + roomCode, 1, 1,
            "turn_credentials",
            turnCreds.ok && !turnCreds.credential.empty() ? "ok" : "unavailable",
            nowMs() - turnStartMs, 5000,
            turnCreds.ok ? "" : "turn-credentials-unavailable",
            turnCreds.ok && !turnCreds.credential.empty()
                ? ""
                : "direct connections may fail behind symmetric NAT");
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
    if (cancelled()) { netShutdown(); iceConnectFinish(job, false, "Cancelled"); return; }

    // Retry ICE connection with backoff on transient coordinator/server failures
    constexpr int kMaxRetries = 5;
    constexpr uint64_t kInitialBackoffMs = 2000;
    uint64_t backoffMs = kInitialBackoffMs;

    for (int attempt = 1; attempt <= kMaxRetries; ++attempt)
    {
        if (cancelled()) break;

        printf("[ICE CONNECT] attempt %d/%d room=%s\n", attempt, kMaxRetries, roomCode.c_str());
        const uint64_t attemptStartMs = nowMs();

        auto agentPtr = std::make_unique<IceAgent>();
        if (!agentPtr->initialize(iceConfig))
        {
            mpLogConnectStageForRoom(roomCode, "ice", "ice:" + roomCode,
                attempt, kMaxRetries, "agent_init", "failed",
                nowMs() - attemptStartMs, 0, "agent-init-failed", "ICE agent init failed");
            printf("[ICE CONNECT] agent initialization failed (attempt %d)\n", attempt);
            if (attempt < kMaxRetries && !sleepInterruptible(backoffMs, job->cancel)) break;
            backoffMs = std::min<uint64_t>(backoffMs * 2, 16000);
            continue;
        }

        iceConnectSetMessage(job, "ICE: gathering candidates...");
        mpLogConnectStageForRoom(roomCode, "ice", "ice:" + roomCode,
            attempt, kMaxRetries, "candidate_gather", "started",
            nowMs() - attemptStartMs, 15000, "", "");
        const uint64_t gatherStartMs = nowMs();
        if (!agentPtr->gatherCandidates())
        {
            mpLogConnectStageForRoom(roomCode, "ice", "ice:" + roomCode,
                attempt, kMaxRetries, "candidate_gather", "failed",
                nowMs() - gatherStartMs, 15000, "gather-start-failed",
                "juice_gather_candidates failed");
            printf("[ICE CONNECT] candidate gathering failed (attempt %d)\n", attempt);
            if (attempt < kMaxRetries && !sleepInterruptible(backoffMs, job->cancel)) break;
            backoffMs = std::min<uint64_t>(backoffMs * 2, 16000);
            continue;
        }
        if (!waitForIceAgentState(*agentPtr, IceAgentState::GatheringComplete, 15000,
                                  nullptr, &job->cancel))
        {
            if (cancelled()) break;
            mpLogConnectStageForRoom(roomCode, "ice", "ice:" + roomCode,
                attempt, kMaxRetries, "candidate_gather", "timeout",
                nowMs() - gatherStartMs, 15000, "gather-timeout",
                "candidate gathering did not complete");
            printf("[ICE CONNECT] gather timeout (15s) (attempt %d)\n", attempt);
            if (attempt < kMaxRetries && !sleepInterruptible(backoffMs, job->cancel)) break;
            backoffMs = std::min<uint64_t>(backoffMs * 2, 16000);
            continue;
        }
        mpLogConnectStageForRoom(roomCode, "ice", "ice:" + roomCode,
            attempt, kMaxRetries, "candidate_gather", "ok",
            nowMs() - gatherStartMs, 15000, "", "");
        printf("[ICE CONNECT] candidates gathered (attempt %d)\n", attempt);

        // Exchange SDP via coordinator (two-phase offer/answer)
        std::string sessionId = "client_" + std::to_string(GetCurrentProcessId())
            + "_" + std::to_string(nowMs()) + "_" + std::to_string(attempt);
        iceConnectSetMessage(job, "ICE: contacting coordinator...");

        const uint64_t beginJoinStartMs = nowMs();
        auto beginJoin = coordinatorIceBeginJoin(roomCode, sessionId, agentPtr->localSdp());
        if (!beginJoin.ok || beginJoin.requestId.empty())
        {
            mpLogConnectStageForRoom(roomCode, "ice", "ice:" + roomCode,
                attempt, kMaxRetries, "coordinator_begin_join", "failed",
                nowMs() - beginJoinStartMs, 5000, beginJoin.errorCode,
                "coordinator rejected or lost begin-join");
            printf("[ICE CONNECT] coordinator ICE begin-join failed "
                   "room=%s error=%s (attempt %d)\n",
                   roomCode.c_str(),
                   beginJoin.errorCode.c_str(), attempt);
            if (attempt < kMaxRetries && !sleepInterruptible(backoffMs, job->cancel)) break;
            backoffMs = std::min<uint64_t>(backoffMs * 2, 16000);
            continue;
        }

        mpLogConnectStageForRoom(roomCode, "ice", "ice:" + roomCode,
            attempt, kMaxRetries, "coordinator_begin_join", "ok",
            nowMs() - beginJoinStartMs, 5000, "",
            beginJoin.requestId.substr(0, 12));
        printf("[ICE CONNECT] begin-join accepted room=%s request=%s (attempt %d)\n",
               roomCode.c_str(),
               beginJoin.requestId.substr(0, std::min<size_t>(12, beginJoin.requestId.size())).c_str(),
               attempt);

        iceConnectSetMessage(job, "ICE: waiting for server answer...");

        std::string hostIceDescription = beginJoin.hostIceDescription;
        const uint64_t answerWaitStartedMs = nowMs();
        constexpr uint64_t kHostAnswerTimeoutMs = 15000;
        bool retriedBeginJoin = false;
        bool hostAnswerTerminalLogged = !hostIceDescription.empty();
        if (!hostAnswerTerminalLogged)
        {
            mpLogConnectStageForRoom(roomCode, "ice", "ice:" + roomCode,
                attempt, kMaxRetries, "host_answer", "waiting",
                0, kHostAnswerTimeoutMs, "", "waiting for server SDP");
        }
        else
        {
            mpLogConnectStageForRoom(roomCode, "ice", "ice:" + roomCode,
                attempt, kMaxRetries, "host_answer", "ok",
                nowMs() - answerWaitStartedMs, kHostAnswerTimeoutMs, "",
                "server SDP supplied by begin-join");
        }

        while (hostIceDescription.empty() &&
               nowMs() - answerWaitStartedMs < kHostAnswerTimeoutMs)
        {
            if (cancelled()) break;

            auto pollResult =
                coordinatorIceClientPoll(roomCode, beginJoin.requestId);

            if (pollResult.ok && !pollResult.hostIceDescription.empty())
            {
                hostIceDescription = pollResult.hostIceDescription;
                mpLogConnectStageForRoom(roomCode, "ice", "ice:" + roomCode,
                    attempt, kMaxRetries, "host_answer", "ok",
                    nowMs() - answerWaitStartedMs, kHostAnswerTimeoutMs, "",
                    "server SDP received");
                hostAnswerTerminalLogged = true;
                break;
            }

            if (pollResult.status == "failed" ||
                pollResult.status == "expired")
            {
                hostAnswerTerminalLogged = true;
                mpLogConnectStageForRoom(roomCode, "ice", "ice:" + roomCode,
                    attempt, kMaxRetries, "host_answer", "failed",
                    nowMs() - answerWaitStartedMs, kHostAnswerTimeoutMs,
                    pollResult.errorCode, pollResult.status);
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

            if (!sleepInterruptible(100, job->cancel)) break;
        }

        if (cancelled()) break;

        if (hostIceDescription.empty())
        {
            if (!hostAnswerTerminalLogged)
            {
                mpLogConnectStageForRoom(roomCode, "ice", "ice:" + roomCode,
                    attempt, kMaxRetries, "host_answer", "timeout",
                    nowMs() - answerWaitStartedMs, kHostAnswerTimeoutMs,
                    "host-answer-timeout", "server never answered begin-join");
            }
            printf("[ICE CONNECT] server answer timeout "
                   "room=%s request=%s (attempt %d)\n",
                   roomCode.c_str(),
                   beginJoin.requestId.substr(0, std::min<size_t>(12, beginJoin.requestId.size())).c_str(),
                   attempt);
            if (attempt < kMaxRetries && !sleepInterruptible(backoffMs, job->cancel)) break;
            backoffMs = std::min<uint64_t>(backoffMs * 2, 16000);
            continue;
        }

        printf("[ICE CONNECT] received host SDP request=%s bytes=%zu (attempt %d)\n",
               beginJoin.requestId.substr(0, std::min<size_t>(12, beginJoin.requestId.size())).c_str(),
               hostIceDescription.size(), attempt);

        if (!agentPtr->setRemoteDescription(hostIceDescription))
        {
            mpLogConnectStageForRoom(roomCode, "ice", "ice:" + roomCode,
                attempt, kMaxRetries, "ice_remote_description", "failed",
                nowMs() - answerWaitStartedMs, 0, "set-remote-description-failed",
                "libjuice rejected the server SDP");
            printf("[ICE CONNECT] setRemoteDescription failed (attempt %d)\n", attempt);
            if (attempt < kMaxRetries && !sleepInterruptible(backoffMs, job->cancel)) break;
            backoffMs = std::min<uint64_t>(backoffMs * 2, 16000);
            continue;
        }
        printf("[ICE CONNECT] remote description applied (attempt %d)\n", attempt);
        iceConnectSetMessage(job, "ICE: connecting...");

        std::string earlyRecv;
        const uint64_t iceConnectStartMs = nowMs();
        mpLogConnectStageForRoom(roomCode, "ice", "ice:" + roomCode,
            attempt, kMaxRetries, "ice_connect", "started",
            0, 30000, "", "");
        if (!waitForIceAgentState(*agentPtr, IceAgentState::Connected, 30000,
                                  &earlyRecv, &job->cancel))
        {
            if (cancelled()) break;
            mpLogConnectStageForRoom(roomCode, "ice", "ice:" + roomCode,
                attempt, kMaxRetries, "ice_connect", "timeout",
                nowMs() - iceConnectStartMs, 30000, "ice-connect-timeout",
                "ICE agent did not reach Connected");
            printf("[ICE CONNECT] connection timeout (30s) (attempt %d)\n", attempt);
            if (attempt < kMaxRetries && !sleepInterruptible(backoffMs, job->cancel)) break;
            backoffMs = std::min<uint64_t>(backoffMs * 2, 16000);
            continue;
        }
        mpLogConnectStageForRoom(roomCode, "ice", "ice:" + roomCode,
            attempt, kMaxRetries, "ice_connect", "ok",
            nowMs() - iceConnectStartMs, 30000, "", "");
        printf("[ICE CONNECT] ICE connection established (attempt %d)\n", attempt);
        agentPtr->logSelectedPath();

        // Success — hand the finished transport to the main thread via the job.
        {
            std::lock_guard<std::mutex> lock(job->mutex);
            job->status.transport = std::make_unique<IceTransport>(std::move(agentPtr));
            job->status.joinToken = beginJoin.joinToken;
            job->status.serverAddress = "ice:" + roomCode;
            job->status.roomCode = roomCode;
            job->status.state = ConnectionState::Connecting;
            job->status.message = "Connected via ICE";
            job->status.done = true;
            job->status.success = true;
        }
        printf("[ICE CONNECT] connected via ICE to room=%s\n", roomCode.c_str());
        return;
    }

    netShutdown();
    if (cancelled())
    {
        iceConnectFinish(job, false, "Cancelled");
        return;
    }

    mpLogConnectStageForRoom(roomCode, "ice", "ice:" + roomCode,
        kMaxRetries, kMaxRetries, "ice_connect", "failed",
        0, 0, "all-attempts-failed",
        "all ICE join attempts failed");
    printf("[ICE CONNECT] FATAL: all %d attempts failed room=%s\n", kMaxRetries, roomCode.c_str());

    // Check if room still exists — helps user understand the failure
    CoordinatorLookupResult lookup = coordinatorIceLookup(roomCode);
    if (lookup.reachable && !lookup.exists)
    {
        iceConnectFinish(job, false, "Room not found");
    }
    else if (!lookup.reachable)
    {
        iceConnectFinish(job, false, "Coordinator unreachable");
    }
    else
    {
        printf("[ICE CONNECT] room %s still exists (%d/%d players) — "
               "connection failed. Check NAT/firewall.\n",
               roomCode.c_str(), lookup.players, lookup.maxPlayers);
        iceConnectFinish(job, false, "Connection failed (NAT/firewall?)");
    }
}

} // anonymous namespace

bool mpIceConnectStart(MultiplayerContext& ctx, const std::string& roomCode,
                       const std::string& playerName)
{
    // Never allow two concurrent connect jobs — cancel and wait for any old one.
    if (gIceConnectJob)
        mpIceConnectCancel();

    if (ctx.active)
        mpShutdown(ctx);

    ctx.currentRoomCode = roomCode;
    ++ctx.connectionAttemptId;
    ctx.joinRequestSends = 0;
    ctx.connectFailed = false;
    ctx.connectionStatus = "Connecting...";

    // Mark the context active immediately so mpTick runs during the async
    // ICE connect and can consume the finished transport. Without this,
    // mpTick is gated behind ctx.active and the completed job is never
    // installed (mpInstallIceConnectSuccess), leaving the client stuck
    // pre-join on the fallback map.
    ctx.active = true;
    ctx.connectionState = ConnectionState::NatNegotiating;
    ctx.connectStartMs = nowMs();
    ctx.sock = INVALID_SOCKET;

    gIceConnectJob = new IceConnectJob();
    gIceConnectJob->status.roomCode = roomCode;
    gIceConnectJob->status.state = ConnectionState::NatNegotiating;
    gIceConnectJob->status.message = "Starting connection...";
    gIceConnectJob->thread = std::thread(iceConnectWorker, roomCode, playerName);
    return true;
}

IceConnectStatus mpIceConnectPoll()
{
    IceConnectJob* job = gIceConnectJob;
    if (!job)
        return IceConnectStatus{};

    IceConnectStatus st;
    {
        std::lock_guard<std::mutex> lock(job->mutex);
        if (!job->status.done)
        {
            st.active = true;
            st.message = job->status.message;
            st.state = job->status.state;
            st.roomCode = job->status.roomCode;
            return st;
        }
        st = std::move(job->status);
    }

    if (job->thread.joinable())
        job->thread.join();
    delete job;
    gIceConnectJob = nullptr;
    return st;
}

bool mpIceConnectActive()
{
    IceConnectJob* job = gIceConnectJob;
    if (!job)
        return false;
    std::lock_guard<std::mutex> lock(job->mutex);
    return !job->status.done;
}

void mpIceConnectCancel()
{
    IceConnectJob* job = gIceConnectJob;
    if (!job)
        return;
    job->cancel.store(true, std::memory_order_relaxed);
    if (job->thread.joinable())
        job->thread.join();
    delete job;
    gIceConnectJob = nullptr;
}

void mpInstallIceConnectSuccess(MultiplayerContext& ctx, IceConnectStatus& status)
{
    ctx.transport = std::move(status.transport);
    ctx.active = true;
    ctx.localPlayerId = 0;
    ctx.tick = 0;
    ctx.lastHelloMs = 0;
    ctx.lastSnapshotReceivedMs = 0;
    ctx.lastSnapshotDiagnosticsMs = 0;
    ctx.lastSnapshotDiagnosticArrivalMs = 0;
    ctx.serverIdentityMismatch = false;
    ctx.connectStartMs = nowMs();
    ctx.packetsSent = 0;
    ctx.packetsReceived = 0;
    ctx.snapshotsReceived = 0;
    ctx.snapshotsMissed = 0;
    ctx.remotePlayers.clear();
    ctx.remoteNpcs.clear();
    ctx.remotePlayerInterpolation.clear();
    ctx.remoteNpcInterpolation.clear();
    ctx.remoteRagdoll.clear();
    RagdollModeSystem::instance().clearAllReplicatedBodies();
    ctx.interpolationRenderTick = 0.0;
    ctx.interpolationClockStarted = false;
    ctx.interpolationClockLastUpdateMs = 0;
    ctx.interpolationFrameNumber = 0;
    ctx.interpolationReanchorCount = 0;
    ctx.lastInterpolationClockStepMs = 0.0;
    ctx.lastInterpolationReanchorMagnitudeMs = 0.0;
    ctx.lastInterpolationReanchorReason.clear();
    ctx.lastClockAnchorServerTick = 0;
    ctx.networkProjectiles.clear();
    ctx.playerRegistry.clear();
    ctx.pendingVipStyles.clear();
    ctx.approvedLocalName.clear();
    ctx.hasLocalServerPosition = false;
    ctx.localPlayerReconciled = false;
    ctx.connectionState = ConnectionState::Connecting;
    ctx.joinRequestSends = 0;
    ctx.vipJoinTicket.clear();
    ctx.vipJoinTicketRequested = false;
    ctx.joinToken = status.joinToken;
    ctx.serverAddress = status.serverAddress;
    ctx.connected = false;
    ctx.connectFailed = false;
    ctx.connectionStatus = "Connected via ICE";
    badconn::noteConnectionEstablished();
    ctx.shotEvents.clear();
    ctx.pendingShotEvents.clear();
    ctx.pendingPelletBlastEvents.clear();
    ctx.pendingHitClaims.clear();
    ctx.lastReceivedShotSerial.clear();
    ctx.nextLocalShotSerial = 1;
    ctx.nextLocalProjectileFireSerial = 1;
    ctx.lastPingSentMs = 0;
    ctx.localPingMs = 0;
    ctx.lastHeardServerMs = 0;
    ctx.lastDisconnectLogMs = 0;
    ctx.disagreementEvents.clear();
    ctx.processedDisagreementIds.clear();
    ctx.processedDisagreementOrder.clear();
    ctx.hasPendingServerPositionCorrection = false;
    if (!status.roomCode.empty())
        ctx.currentRoomCode = status.roomCode;
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

    // Deduplicate: skip already-processed event IDs while retaining a bounded
    // sliding window. Clearing the whole set would let retransmissions of
    // recent corrections spawn duplicate effects every time the window filled.
    bool duplicate = false;
    if (packet->eventId != 0)
    {
        if (ctx.processedDisagreementIds.count(packet->eventId))
        {
            printf("[NET DISAGREEMENT RECV] eventId=%u duplicate=1 skipped=1\n", packet->eventId);
            return;
        }
        ctx.processedDisagreementIds.insert(packet->eventId);
        ctx.processedDisagreementOrder.push_back(packet->eventId);

        constexpr size_t kDisagreementDedupWindow = 128;
        while (ctx.processedDisagreementOrder.size() > kDisagreementDedupWindow)
        {
            ctx.processedDisagreementIds.erase(ctx.processedDisagreementOrder.front());
            ctx.processedDisagreementOrder.pop_front();
        }
    }

    DisagreementEvent event;
    event.timeMs = nowMs();
    event.reason = (DisagreementReason)packet->reason;
    event.eventId = packet->eventId;
    event.relatedSerial = packet->relatedSerial;
    event.sourcePlayerId = packet->sourcePlayerId;
    event.targetPlayerId = packet->targetPlayerId;
    event.position = {packet->posX, packet->posY, packet->posZ};
    event.correction = {packet->correctionX, packet->correctionY, packet->correctionZ};
    if (event.reason == DISAGREEMENT_POSITION_CORRECTION &&
        event.sourcePlayerId == ctx.localPlayerId)
    {
        ctx.pendingServerPositionCorrection = event.position + event.correction;
        ctx.hasPendingServerPositionCorrection = true;
    }
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

// ── Migration: reconnect + honest connection health ───────────────────

static std::string healthServerLabel(const MultiplayerContext& ctx)
{
    if (!ctx.roomCode.empty()) return ctx.roomCode;
    if (!ctx.currentRoomCode.empty()) return ctx.currentRoomCode;
    if (!ctx.serverAddress.empty()) return ctx.serverAddress;
    return "server";
}

static void pushConnectionNotification(const MultiplayerContext& ctx, const char* title,
                                       const char* message)
{
    (void)ctx;
    NotificationSystem::instance().pushCritical(title, message, 0);
}

static bool sendReconnectRequest(MultiplayerContext& ctx)
{
    if (ctx.localPlayerId == 0 || ctx.reconnectToken.empty())
        return false;

    ReconnectRequestPacket packet{};
    packet.header.type = PACKET_RECONNECT_REQUEST;
    packet.header.tick = ctx.tick;
    packet.header.playerId = ctx.localPlayerId;
    std::memset(packet.reconnectToken, 0, sizeof(packet.reconnectToken));
    std::strncpy(packet.reconnectToken, ctx.reconnectToken.c_str(),
                 sizeof(packet.reconnectToken) - 1);

    // Send through the active transport when available (fixes silent ICE loss
    // where ctx.sock is INVALID_SOCKET); fall back to the raw UDP socket.
    if (ctx.transport)
    {
        ctx.transport->send(&packet, sizeof(packet));
    }
    else if (ctx.sock != INVALID_SOCKET)
    {
        sendto(ctx.sock, (const char*)&packet, sizeof(packet), 0,
               (sockaddr*)&ctx.serverAddr, sizeof(ctx.serverAddr));
    }
    else
    {
        return false;
    }
    ++ctx.packetsSent;
    ctx.lastPacketSentMs = nowMs();
    return true;
}

static void mpNotifyReconnectAttempt(const MultiplayerContext& ctx)
{
    char buf[256];
    const double elapsed = ctx.disconnectStartedMs
        ? (double)(nowMs() - ctx.disconnectStartedMs) / 1000.0 : 0.0;
    snprintf(buf, sizeof(buf), "Attempting reconnect #%d... %.2fs",
             ctx.reconnectAttempts, elapsed);
    pushConnectionNotification(ctx, "Reconnecting", buf);
}

static void writeSessionHealthEvent(const MultiplayerContext& ctx,
                                    ConnectionState before, ConnectionState next,
                                    const char* trigger,
                                    const std::string& title,
                                    const std::string& message)
{
    const auto& cfg = NetworkingConfig::instance().data();
    const uint64_t now = nowMs();
    const uint64_t lastHeardAge = ctx.lastHeardServerMs > 0
        ? now - ctx.lastHeardServerMs : 0;
    const uint64_t graceRemaining = ctx.reconnectGraceDeadlineMs > now
        ? ctx.reconnectGraceDeadlineMs - now : 0;
    const bool heardSinceDisconnect =
        ctx.disconnectStartedMs > 0 && ctx.lastHeardServerMs > ctx.disconnectStartedMs;
    const bool falsePositive =
        next == ConnectionState::Reconnecting &&
        lastHeardAge > 0 &&
        lastHeardAge <= (uint64_t)cfg.timeouts.stalePacketThresholdMs;
    const bool terminal = next == ConnectionState::ReconnectFailed ||
                          next == ConnectionState::HostClosed ||
                          next == ConnectionState::Kicked ||
                          next == ConnectionState::ServerCrashed;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Network,
        terminal ? StructuredLevel::Errors : StructuredLevel::Important,
        "network.session.health",
        "CONNECTION_" + std::to_string(ctx.localPlayerId),
        "client connection health transition", ctx.tick,
        nlohmann::json{
            {"from", connectionStateName(before)},
            {"to", connectionStateName(next)},
            {"trigger", trigger},
            {"last_heard_age_ms", lastHeardAge},
            {"stale_threshold_ms", (uint64_t)cfg.timeouts.stalePacketThresholdMs},
            {"hard_timeout_ms", (uint64_t)cfg.timeouts.clientTimeoutMs},
            {"grace_deadline_ms", ctx.reconnectGraceDeadlineMs},
            {"grace_remaining_ms", graceRemaining},
            {"heard_since_disconnect", heardSinceDisconnect},
            {"packet_received_after_disconnect", heardSinceDisconnect},
            {"reconnect_attempt", ctx.reconnectAttempts},
            {"reconnect_max_attempts", cfg.retries.reconnectMaxAttempts},
            {"reconnect_backoff_ms", ctx.reconnectBackoffMs},
            {"last_heard_server_ms", ctx.lastHeardServerMs},
            {"disconnect_started_ms", ctx.disconnectStartedMs},
            {"false_positive_suspected", falsePositive},
            {"notification_title", title},
            {"notification_message", message},
            {"active", ctx.active},
            {"connected", ctx.connected},
            {"room_code", ctx.roomCode.empty() ? ctx.currentRoomCode : ctx.roomCode},
            {"server_address", ctx.serverAddress},
            {"local_player_id", ctx.localPlayerId},
            {"build_id", runtimeBuildId()}},
        __FILE__, __LINE__, __func__);
}

void mpNotifyConnectionStateChange(MultiplayerContext& ctx,
                                   ConnectionState before, ConnectionState next)
{
    if (before == next || ctx.lastNotifiedConnectionState == next)
        return;
    ctx.lastNotifiedConnectionState = next;

    std::string title;
    std::string message;
    char buf[256];
    switch (next)
    {
    case ConnectionState::WeakConnection:
    {
        const double age = ctx.lastHeardServerMs
            ? (double)(nowMs() - ctx.lastHeardServerMs) / 1000.0 : 0.0;
        snprintf(buf, sizeof(buf),
                 "Server status: Weak connection — last packet %.2fs ago", age);
        title = "Weak connection";
        message = buf;
        break;
    }
    case ConnectionState::Connected:
    {
        const bool recovered = before == ConnectionState::Reconnecting;
        title = recovered ? "Reconnected" : "Connected";
        message = recovered ? "Server status: Reconnected"
                            : (ctx.serverName.empty()
                                ? "Server status: Connected"
                                : "Connected to " + ctx.serverName + "!!!!");
        break;
    }
    case ConnectionState::Reconnecting:
        snprintf(buf, sizeof(buf),
                 "Reconnecting to %s — attempt #%d (%.1fs elapsed)",
                 healthServerLabel(ctx).c_str(),
                 std::max(1, ctx.reconnectAttempts),
                 ctx.disconnectStartedMs
                     ? (double)(nowMs() - ctx.disconnectStartedMs) / 1000.0
                     : 0.0);
        title = "Reconnecting";
        message = buf;
        break;
    case ConnectionState::ReconnectFailed:
        title = "Disconnected";
        message = "Server status: Disconnected — reconnect failed";
        break;
    case ConnectionState::HostClosed:
        title = "Host closed";
        message = "Server status: Host closed the connection";
        break;
    case ConnectionState::Kicked:
        title = "Kicked";
        message = "Server status: Kicked from server";
        break;
    case ConnectionState::ServerCrashed:
        title = "Server unreachable";
        message = "Server status: Server unreachable — connection lost";
        break;
    default:
        break;
    }

    if (!title.empty())
        pushConnectionNotification(ctx, title.c_str(), message.c_str());

    // Keep the legacy connectionStatus string honest for existing HUD paths.
    ctx.connectionStatus = mpConnectionHealthText(ctx);

    StructuredLogger::instance().writeEvent(
        StructuredCategory::Network,
        StructuredLevel::Important,
        "network.connection-state.changed",
        "CONNECTION_" + std::to_string(ctx.localPlayerId),
        "client connection state transition",
        ctx.tick,
        {{"from", connectionStateName(before)},
         {"to", connectionStateName(next)},
         {"active", ctx.active},
         {"connected", ctx.connected},
         {"room_code", ctx.roomCode},
         {"hud_room_code", ctx.currentRoomCode},
         {"server_name", ctx.serverName},
         {"server_address", ctx.serverAddress},
         {"local_player_id", ctx.localPlayerId},
         {"required_map_id", ctx.requiredMapId}},
        __FILE__, __LINE__, __func__);

    writeSessionHealthEvent(ctx, before, next, "notification", title, message);
}

void mpStartReconnect(MultiplayerContext& ctx)
{
    if (!ctx.active || ctx.reconnectToken.empty())
    {
        Debug::warn(Debug::Category::Networking,
                    "[NET RECONNECT] cannot reconnect: no reconnect token "
                    "(state=%s)\n", connectionStateName(ctx.connectionState));
        return;
    }

    const uint64_t now = nowMs();
    ctx.connectionState = ConnectionState::Reconnecting;
    ctx.connected = false;
    ctx.reconnectAttempts = 0;
    if (ctx.reconnectGraceDeadlineMs == 0)
    {
        ctx.reconnectGraceDeadlineMs = now + (uint64_t)
            NetworkingConfig::instance().data().retries.reconnectGraceMs;
    }
    if (ctx.disconnectStartedMs == 0)
        ctx.disconnectStartedMs = now;

    // Attempt #1 immediately; later attempts run on reconnect_interval_ms.
    ctx.reconnectBackoffMs = (uint64_t)
        NetworkingConfig::instance().data().retries.reconnectIntervalMs;
    ctx.lastReconnectAttemptMs = 0;
    if (sendReconnectRequest(ctx))
    {
        ++ctx.reconnectAttempts;
        ctx.lastReconnectAttemptMs = now;
    }
    logReconnectResourceProbe(ctx, "attempt-start");

    writeSessionHealthEvent(ctx, ConnectionState::Reconnecting,
                            ConnectionState::Reconnecting,
                            "reconnect_attempt", "Reconnecting",
                            "attempt #" + std::to_string(ctx.reconnectAttempts));

    Debug::warn(Debug::Category::Networking,
                "[NET RECONNECT] starting player=%u graceDeadlineMs=%llu\n",
                ctx.localPlayerId,
                (unsigned long long)ctx.reconnectGraceDeadlineMs);
}

void mpTickReconnect(MultiplayerContext& ctx)
{
    if (ctx.connectionState != ConnectionState::Reconnecting)
        return;

    const auto& retryCfg = NetworkingConfig::instance().data().retries;
    const uint64_t now = nowMs();

    // The true give-up is owned by the grace deadline in
    // mpUpdateConnectionHealth so the client always waits the full window;
    // the attempt cap is only a secondary limit on how often we send.
    if (ctx.reconnectAttempts >= (int)retryCfg.reconnectMaxAttempts)
        return;

    const uint64_t intervalMs = (uint64_t)retryCfg.reconnectIntervalMs;
    if (ctx.reconnectBackoffMs == 0)
        ctx.reconnectBackoffMs = intervalMs;
    if (now - ctx.lastReconnectAttemptMs < ctx.reconnectBackoffMs)
        return;

    ctx.lastReconnectAttemptMs = now;
    ++ctx.reconnectAttempts;

    if (!sendReconnectRequest(ctx))
    {
        logReconnectResourceProbe(ctx, "attempt-send-failed");
        Debug::warn(Debug::Category::Networking,
                    "[NET RECONNECT] attempt=%d/%u failed to send (no transport)\n",
                    ctx.reconnectAttempts, (unsigned)retryCfg.reconnectMaxAttempts);
        return;
    }

    logReconnectResourceProbe(ctx, "attempt-sent");

    mpNotifyReconnectAttempt(ctx);
    writeSessionHealthEvent(ctx, ConnectionState::Reconnecting,
                            ConnectionState::Reconnecting,
                            "reconnect_attempt", "Reconnecting",
                            "attempt #" + std::to_string(ctx.reconnectAttempts));

    Debug::warn(Debug::Category::Networking,
                "[NET RECONNECT] attempt=%d/%u interval=%llums elapsed=%llums\n",
                ctx.reconnectAttempts, (unsigned)retryCfg.reconnectMaxAttempts,
                (unsigned long long)intervalMs,
                (unsigned long long)(now - ctx.disconnectStartedMs));
}

std::string mpConnectionHealthText(const MultiplayerContext& ctx)
{
    if (!ctx.active)
        return "Disconnected";

    const uint64_t now = nowMs();
    char buf[192];
    switch (ctx.connectionState)
    {
    case ConnectionState::WeakConnection:
    {
        const double age = ctx.lastHeardServerMs
            ? (double)(now - ctx.lastHeardServerMs) / 1000.0 : 0.0;
        snprintf(buf, sizeof(buf), "Weak connection — last packet %.2fs ago", age);
        return buf;
    }
    case ConnectionState::Reconnecting:
    {
        const double elapsed = ctx.disconnectStartedMs
            ? (double)(now - ctx.disconnectStartedMs) / 1000.0 : 0.0;
        snprintf(buf, sizeof(buf), "Reconnecting — attempt #%d ... %.2fs",
                 ctx.reconnectAttempts, elapsed);
        return buf;
    }
    case ConnectionState::ReconnectFailed:
        return "Disconnected — reconnect failed";
    case ConnectionState::HostClosed:
        return "Host closed the connection";
    case ConnectionState::Kicked:
        return "Kicked from server";
    case ConnectionState::ServerCrashed:
        return "Server unreachable — connection lost";
    case ConnectionState::Connected:
        return "Connected";
    default:
        return ctx.connectionStatus.empty()
            ? connectionStateName(ctx.connectionState)
            : ctx.connectionStatus;
    }
}

void mpUpdateConnectionHealth(MultiplayerContext& ctx)
{
    if (!ctx.active)
        return;

    const uint64_t now = nowMs();
    const auto& cfg = NetworkingConfig::instance().data();
    const uint64_t staleThreshold = (uint64_t)cfg.timeouts.stalePacketThresholdMs;
    const uint64_t hardTimeout = (uint64_t)cfg.timeouts.clientTimeoutMs;
    const uint64_t graceMs = (uint64_t)cfg.retries.reconnectGraceMs;
    const uint64_t lastHeardAge = ctx.lastHeardServerMs > 0
        ? now - ctx.lastHeardServerMs : 0;
    const bool heardSinceDisconnect =
        ctx.disconnectStartedMs > 0 && ctx.lastHeardServerMs > ctx.disconnectStartedMs;

    const ConnectionState before = ctx.connectionState;
    ConnectionState next = mpNextConnectionHealth(
        before, now, lastHeardAge, heardSinceDisconnect,
        staleThreshold, hardTimeout, ctx.reconnectGraceDeadlineMs, graceMs);

    if (next == before)
        return;

    ctx.connectionState = next;
    switch (next)
    {
    case ConnectionState::Reconnecting:
        ctx.connected = false;
        ctx.disconnectStartedMs = now;
        ctx.reconnectGraceDeadlineMs = now + graceMs;
        if (ctx.reconnectToken.empty())
        {
            // Nothing to reconnect with: give up immediately and honestly.
            ctx.connectionState = ConnectionState::ReconnectFailed;
            teardownPreviousSession(ctx, DisconnectPolicy::ConnectionFailure);
            mpNotifyConnectionStateChange(ctx, before,
                                          ConnectionState::ReconnectFailed);
            return;
        }
        mpStartReconnect(ctx);
        mpNotifyConnectionStateChange(ctx, before, ConnectionState::Reconnecting);
        break;
    case ConnectionState::Connected:
        ctx.connected = true;
        ctx.reconnectGraceDeadlineMs = 0;
        ctx.disconnectStartedMs = 0;
        ctx.reconnectAttempts = 0;
        ctx.reconnectBackoffMs = 0;
        mpNotifyConnectionStateChange(ctx, before, ConnectionState::Connected);
        break;
    case ConnectionState::ReconnectFailed:
        teardownPreviousSession(ctx, DisconnectPolicy::ConnectionFailure);
        mpNotifyConnectionStateChange(ctx, before, ConnectionState::ReconnectFailed);
        break;
    default:
        break;
    }
}

} // namespace MimitaNet
