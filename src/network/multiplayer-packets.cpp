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
#include "entities/death-ghost.h"
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

void teardownPreviousSession(MultiplayerContext& ctx, DisconnectPolicy policy)
{
    // Log the teardown
    printf("[NET TEARDOWN] policy=%s reason=%s\n",
           disconnectPolicyName(policy),
           ctx.connectionStatus.c_str());

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
    ctx.remotePlayerInterpolation.clear();
    ctx.remoteNpcInterpolation.clear();
    DeathGhostSystem::instance().clear();
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
    // Cancel any in-flight async ICE connect first so the worker thread is
    // finished before this context is torn down (no concurrent ctx access).
    mpIceConnectCancel();

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
    uint8_t claimedBodyPart)
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

void mpRecordOpHitscanShot(MultiplayerContext& ctx,
                           const WeaponDefinition& definition,
                           uint16_t weaponDefNetworkId,
                           int16_t equippedSlot,
                           uint32_t targetId,
                           int damage,
                           uint8_t bodyPart,
                           uint32_t tick,
                           uint32_t spawnGeneration)
{
    if (!ctx.active || !ctx.localPlayerId ||
        definition.networkMode != WeaponNetworkMode::ClientBatchedHitscan)
        return;

    auto& batch = ctx.pendingOpHitscanBatch;
    if (!batch.active || batch.weaponDefNetworkId != weaponDefNetworkId ||
        batch.equippedSlot != equippedSlot) {
        batch.startTick = tick;
        batch.endTick = tick;
        batch.shotsFired = 0;
        batch.hitCount = 0;
        batch.weaponDefNetworkId = weaponDefNetworkId;
        batch.equippedSlot = equippedSlot;
        batch.active = true;
    }
    batch.endTick = tick;
    batch.shotsFired++;
    if (targetId != 0 && batch.hitCount < MAX_OP_HIT_ENTRIES) {
        OpHitscanHitEntry& hit = batch.hits[batch.hitCount++];
        hit.targetId = targetId;
        hit.damage = (int16_t)std::clamp(damage, -32768, 32767);
        hit.bodyPart = bodyPart;
        hit.relativeTick = (uint8_t)std::min<uint32_t>(tick - batch.startTick, 255);
    }

    const uint64_t now = nowMs();
    if (now - batch.lastSendMs < 100 && batch.hitCount < MAX_OP_HIT_ENTRIES)
        return;
    mpFlushOpHitscanBatch(ctx, spawnGeneration);
}

void mpFlushOpHitscanBatch(MultiplayerContext& ctx, uint32_t spawnGeneration)
{
    auto& batch = ctx.pendingOpHitscanBatch;
    if (!ctx.active || !ctx.localPlayerId || !batch.active ||
        (batch.shotsFired == 0 && batch.hitCount == 0))
        return;
    OpHitscanBatchPacket packet{};
    packet.header.type = PACKET_OP_HIT_BATCH;
    packet.header.tick = ctx.tick;
    packet.header.playerId = ctx.localPlayerId;
    packet.spawnGeneration = spawnGeneration;
    packet.batchSequence = batch.batchSequence++;
    packet.startTick = batch.startTick;
    packet.endTick = batch.endTick;
    packet.shotsFired = batch.shotsFired;
    packet.weaponDefNetworkId = batch.weaponDefNetworkId;
    packet.equippedSlot = batch.equippedSlot;
    packet.hitCount = batch.hitCount;
    std::memcpy(packet.hits, batch.hits,
                sizeof(OpHitscanHitEntry) * packet.hitCount);
    mpSendPacket(ctx, &packet, sizeof(packet));
    Debug::logThrottled(Debug::Category::Weapons, "op-hit-batch-send", 1.0,
        "[OP HIT BATCH] player=%u weapon=%u shots=%u hits=%u span=%u..%u\n",
        ctx.localPlayerId, packet.weaponDefNetworkId, packet.shotsFired,
        packet.hitCount, packet.startTick, packet.endTick);
    batch.lastSendMs = nowMs();
    batch.startTick = ctx.tick;
    batch.endTick = ctx.tick;
    batch.shotsFired = 0;
    batch.hitCount = 0;
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
        case ConnectionState::WeakConnection:   return "WeakConnection";
        case ConnectionState::ReconnectFailed:  return "ReconnectFailed";
        case ConnectionState::HostClosed:       return "HostClosed";
        case ConnectionState::Kicked:           return "Kicked";
        case ConnectionState::ServerCrashed:    return "ServerCrashed";
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

    // Fast-fail: if the room no longer exists on the coordinator, report it
    // immediately instead of burning through the retry attempts.
    iceConnectSetMessage(job, "Checking room...");
    {
        CoordinatorLookupResult lookup = coordinatorIceLookup(roomCode);
        if (!lookup.exists)
        {
            printf("[ICE CONNECT] room=%s not found on coordinator\n", roomCode.c_str());
            iceConnectFinish(job, false, "Room not found");
            return;
        }
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
    if (cancelled()) { netShutdown(); iceConnectFinish(job, false, "Cancelled"); return; }

    // Retry ICE connection with backoff on transient coordinator/server failures
    constexpr int kMaxRetries = 5;
    constexpr uint64_t kInitialBackoffMs = 2000;
    uint64_t backoffMs = kInitialBackoffMs;

    for (int attempt = 1; attempt <= kMaxRetries; ++attempt)
    {
        if (cancelled()) break;

        printf("[ICE CONNECT] attempt %d/%d room=%s\n", attempt, kMaxRetries, roomCode.c_str());

        auto agentPtr = std::make_unique<IceAgent>();
        if (!agentPtr->initialize(iceConfig))
        {
            printf("[ICE CONNECT] agent initialization failed (attempt %d)\n", attempt);
            if (attempt < kMaxRetries && !sleepInterruptible(backoffMs, job->cancel)) break;
            backoffMs = std::min<uint64_t>(backoffMs * 2, 16000);
            continue;
        }

        iceConnectSetMessage(job, "ICE: gathering candidates...");
        if (!agentPtr->gatherCandidates())
        {
            printf("[ICE CONNECT] candidate gathering failed (attempt %d)\n", attempt);
            if (attempt < kMaxRetries && !sleepInterruptible(backoffMs, job->cancel)) break;
            backoffMs = std::min<uint64_t>(backoffMs * 2, 16000);
            continue;
        }
        if (!waitForIceAgentState(*agentPtr, IceAgentState::GatheringComplete, 15000,
                                  nullptr, &job->cancel))
        {
            if (cancelled()) break;
            printf("[ICE CONNECT] gather timeout (15s) (attempt %d)\n", attempt);
            if (attempt < kMaxRetries && !sleepInterruptible(backoffMs, job->cancel)) break;
            backoffMs = std::min<uint64_t>(backoffMs * 2, 16000);
            continue;
        }
        printf("[ICE CONNECT] candidates gathered (attempt %d)\n", attempt);

        // Exchange SDP via coordinator (two-phase offer/answer)
        std::string sessionId = "client_" + std::to_string(GetCurrentProcessId())
            + "_" + std::to_string(nowMs()) + "_" + std::to_string(attempt);
        iceConnectSetMessage(job, "ICE: contacting coordinator...");

        auto beginJoin = coordinatorIceBeginJoin(roomCode, sessionId, agentPtr->localSdp());
        if (!beginJoin.ok || beginJoin.requestId.empty())
        {
            printf("[ICE CONNECT] coordinator ICE begin-join failed "
                   "room=%s error=%s (attempt %d)\n",
                   roomCode.c_str(),
                   beginJoin.errorCode.c_str(), attempt);
            if (attempt < kMaxRetries && !sleepInterruptible(backoffMs, job->cancel)) break;
            backoffMs = std::min<uint64_t>(backoffMs * 2, 16000);
            continue;
        }

        printf("[ICE CONNECT] begin-join accepted room=%s request=%s (attempt %d)\n",
               roomCode.c_str(),
               beginJoin.requestId.substr(0, std::min<size_t>(12, beginJoin.requestId.size())).c_str(),
               attempt);

        iceConnectSetMessage(job, "ICE: waiting for server answer...");

        std::string hostIceDescription = beginJoin.hostIceDescription;
        const uint64_t answerWaitStartedMs = nowMs();
        constexpr uint64_t kHostAnswerTimeoutMs = 15000;
        bool retriedBeginJoin = false;

        while (hostIceDescription.empty() &&
               nowMs() - answerWaitStartedMs < kHostAnswerTimeoutMs)
        {
            if (cancelled()) break;

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

            if (!sleepInterruptible(100, job->cancel)) break;
        }

        if (cancelled()) break;

        if (hostIceDescription.empty())
        {
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
            printf("[ICE CONNECT] setRemoteDescription failed (attempt %d)\n", attempt);
            if (attempt < kMaxRetries && !sleepInterruptible(backoffMs, job->cancel)) break;
            backoffMs = std::min<uint64_t>(backoffMs * 2, 16000);
            continue;
        }
        printf("[ICE CONNECT] remote description applied (attempt %d)\n", attempt);
        iceConnectSetMessage(job, "ICE: connecting...");

        std::string earlyRecv;
        if (!waitForIceAgentState(*agentPtr, IceAgentState::Connected, 30000,
                                  &earlyRecv, &job->cancel))
        {
            if (cancelled()) break;
            printf("[ICE CONNECT] connection timeout (30s) (attempt %d)\n", attempt);
            if (attempt < kMaxRetries && !sleepInterruptible(backoffMs, job->cancel)) break;
            backoffMs = std::min<uint64_t>(backoffMs * 2, 16000);
            continue;
        }
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

    printf("[ICE CONNECT] FATAL: all %d attempts failed room=%s\n", kMaxRetries, roomCode.c_str());

    // Check if room still exists — helps user understand the failure
    CoordinatorLookupResult lookup = coordinatorIceLookup(roomCode);
    if (!lookup.exists)
    {
        iceConnectFinish(job, false, "Room not found");
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
    ctx.connectStartMs = nowMs();
    ctx.packetsSent = 0;
    ctx.packetsReceived = 0;
    ctx.snapshotsReceived = 0;
    ctx.snapshotsMissed = 0;
    ctx.remotePlayers.clear();
    ctx.remoteNpcs.clear();
    ctx.remotePlayerInterpolation.clear();
    ctx.remoteNpcInterpolation.clear();
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
    ctx.nextLocalMeleeAttackSerial = 1;
    ctx.lastPingSentMs = 0;
    ctx.localPingMs = 0;
    ctx.lastHeardServerMs = 0;
    ctx.lastDisconnectLogMs = 0;
    ctx.disagreementEvents.clear();
    ctx.processedDisagreementIds.clear();
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

void mpNotifyConnectionStateChange(MultiplayerContext& ctx,
                                   ConnectionState before, ConnectionState next)
{
    if (before == next || ctx.lastNotifiedConnectionState == next)
        return;
    ctx.lastNotifiedConnectionState = next;

    char buf[256];
    switch (next)
    {
    case ConnectionState::WeakConnection:
    {
        const double age = ctx.lastHeardServerMs
            ? (double)(nowMs() - ctx.lastHeardServerMs) / 1000.0 : 0.0;
        snprintf(buf, sizeof(buf),
                 "Server status: Weak connection — last packet %.2fs ago", age);
        pushConnectionNotification(ctx, "Weak connection", buf);
        break;
    }
    case ConnectionState::Connected:
    {
        const bool recovered = before == ConnectionState::Reconnecting;
        pushConnectionNotification(ctx,
            recovered ? "Reconnected" : "Connected",
            recovered ? "Server status: Reconnected"
                      : "Server status: Connected");
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
        pushConnectionNotification(ctx, "Reconnecting", buf);
        break;    case ConnectionState::ReconnectFailed:
        pushConnectionNotification(ctx, "Disconnected",
            "Server status: Disconnected — reconnect failed");
        break;
    case ConnectionState::HostClosed:
        pushConnectionNotification(ctx, "Host closed",
            "Server status: Host closed the connection");
        break;
    case ConnectionState::Kicked:
        pushConnectionNotification(ctx, "Kicked",
            "Server status: Kicked from server");
        break;
    case ConnectionState::ServerCrashed:
        pushConnectionNotification(ctx, "Server unreachable",
            "Server status: Server unreachable — connection lost");
        break;
    default:
        break;
    }

    // Keep the legacy connectionStatus string honest for existing HUD paths.
    ctx.connectionStatus = mpConnectionHealthText(ctx);

    {
        char msg[128];
        std::snprintf(msg, sizeof(msg), "from=%d to=%d player=%u",
                      (int)before, (int)next, ctx.localPlayerId);
        ::logStructured(::StructuredCategory::Network, ::StructuredLevel::Important,
                        "CONNECTION_STATE",
                        "CONNECTION_" + std::to_string(ctx.localPlayerId),
                        "connection state transition", msg);
    }
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
        Debug::warn(Debug::Category::Networking,
                    "[NET RECONNECT] attempt=%d/%u failed to send (no transport)\n",
                    ctx.reconnectAttempts, (unsigned)retryCfg.reconnectMaxAttempts);
        return;
    }

    mpNotifyReconnectAttempt(ctx);

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
