// 07 21 2026, 17 10
/* purpose
* Owns client local-player reconciliation against authoritative server snapshots.
* Applies lifecycle snaps for spawn, respawn, teleport, reconnect, death, and catastrophic divergence.
* Reports correction severity using the same movement validation thresholds as the server.
* Does NOT parse network packets, send input packets, or own server validation policy.
* Does NOT replace local physics prediction or remote entity interpolation.
* Does NOT smooth over authoritative lifecycle changes.
*/

#include "network/multiplayer-context.h"
#include "network/server.h"
#include "combat/weapon-runtime.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <glm/glm.hpp>

namespace MimitaNet {

void mpReconcileLocalPlayer(MultiplayerContext& ctx, Player& player, float dt)
{
    (void)dt;
    if (!ctx.connected || !ctx.hasLocalServerPosition)
        return;

    // ── Epoch changed: apply authoritative transform immediately ──────
    // When the server increments its epoch, the client must hard-snap
    // to the server position before any local movement or input is built.
    // This handles spawn, respawn, teleport, and reconnect.
    if (ctx.localServerEpoch != 0 &&
        ctx.lastAppliedEpoch != ctx.localServerEpoch)
    {
        printf("[CLIENT AUTHORITATIVE SNAPSHOT] playerId=%u "
               "snapshotEpoch=%u previousServerEpoch=%u lastAppliedEpoch=%u "
               "position=(%.2f,%.2f,%.2f) needsApply=1\n",
               ctx.localPlayerId,
               (uint32_t)ctx.localServerEpoch,
               (uint32_t)ctx.localServerEpoch,
               (uint32_t)ctx.lastAppliedEpoch,
               ctx.localServerPosition.x,
               ctx.localServerPosition.y,
               ctx.localServerPosition.z);

        player.pos = ctx.localServerPosition;
        player.vel = ctx.localServerVelocity;
        player.yaw = ctx.localServerYaw;
        player.ground.onGround = ctx.localServerOnGround;
        player.externalImpulse = glm::vec3(0.0f);
        player.syncLegacyStateToLayers();
        player.updateModelWorldTransforms();
        ctx.lastAppliedEpoch = ctx.localServerEpoch;
        ctx.localPlayerReconciled = true;

        // Sync outgoing epoch so the client-transform gate uses the new epoch
        if ((uint32_t)ctx.localServerEpoch > ctx.transformEpoch)
            ctx.transformEpoch = ctx.localServerEpoch;

        // Clear movement history for new lifecycle
        ctx.localServerAcknowledgedMovementSequence = 0;
        ctx.sentMovementHistory.clear();
    }

    constexpr float CORRECTION_LOG_DISTANCE = 0.5f;
    constexpr uint64_t TELEPORT_ACK_TIMEOUT_MS = 1500;
    const uint64_t currentMs = nowMs();
    if (ctx.awaitingTeleportAck &&
        currentMs - ctx.pendingTeleportSentMs > TELEPORT_ACK_TIMEOUT_MS)
    {
        ctx.awaitingTeleportAck = false;
        ctx.teleportResync = true;
    }
    const bool initialSpawn = !ctx.localPlayerReconciled;

    // ── Authoritative lifecycle detection ─────────────────────────────
    // A new life is detected when the server epoch advances past our
    // pending respawn start epoch AND the server reports health > 0.
    const uint32_t prevEpoch = (uint32_t)ctx.lastAppliedEpoch;
    const uint32_t newEpoch = (uint32_t)ctx.localServerEpoch;
    const bool epochAdvanced = newEpoch != 0 && newEpoch > prevEpoch;
    const bool authoritativeNewLife =
        ctx.pendingRespawnSerial != 0 &&
        epochAdvanced &&
        ctx.localServerHealth > 0 &&
        (ctx.pendingRespawnStartEpoch == 0 || newEpoch > (uint32_t)ctx.pendingRespawnStartEpoch) &&
        ctx.lastAppliedEpoch == ctx.localServerEpoch;

    const bool serverKilledPlayer = ctx.localServerHealth <= 0 && !player.dead;
    // Legacy health-transition based detection (still useful for non-respawn deaths)
    const bool serverRespawnedPlayer =
        !authoritativeNewLife &&
        ctx.localServerHealth > 0 &&
        ctx.lastSeenServerHealth <= 0 &&
        ctx.localPlayerReconciled;

    // ── Acknowledged-input reconciliation ──────────────────────────────
    // Instead of comparing current prediction against raw server position
    // (which creates a rubberband loop), find the sent movement report
    // that the server acknowledges and compute the error from that point.
    const MovementValidationConfig correctionConfig;
    const bool authoritativeEpochReady =
        ctx.localServerEpoch != 0 &&
        ctx.transformEpoch == ctx.localServerEpoch &&
        ctx.lastAppliedEpoch == ctx.localServerEpoch;
    const bool sameEpoch =
        ctx.lastAppliedEpoch != 0 &&
        ctx.lastAppliedEpoch == ctx.localServerEpoch;

    // Find matching history entry for acknowledged sequence
    glm::vec3 acknowledgedError{0.0f};
    bool hasAcknowledgedHistory = false;
    glm::vec3 sentPositionAtAck{0.0f};
    uint32_t acknowledgedSeq = 0;

    if (sameEpoch && ctx.localServerAcknowledgedMovementSequence != 0)
    {
        acknowledgedSeq = ctx.localServerAcknowledgedMovementSequence;
        for (const auto& entry : ctx.sentMovementHistory)
        {
            if (entry.sequence == acknowledgedSeq &&
                entry.spawnGeneration == ctx.lastKnownSpawnGeneration &&
                entry.transformEpoch == ctx.lastAppliedEpoch)
            {
                sentPositionAtAck = entry.position;
                acknowledgedError = ctx.localServerPosition - entry.position;
                hasAcknowledgedHistory = true;
                break;
            }
        }

        // Prune acknowledged entries from history
        while (!ctx.sentMovementHistory.empty() &&
               ctx.sentMovementHistory.front().sequence < acknowledgedSeq)
            ctx.sentMovementHistory.pop_front();
    }

    const float rawError = glm::length(ctx.localServerPosition - player.pos);
    const float acknowledgedErrorLen = hasAcknowledgedHistory
        ? glm::length(acknowledgedError) : rawError;

    const MovementCorrectionClass correctionClass =
        classifyMovementCorrection(acknowledgedErrorLen, correctionConfig);

    const bool teleportCompletionResync =
        ctx.teleportResync && !ctx.awaitingTeleportAck;
    const bool epochChanged =
        ctx.localPlayerReconciled &&
        ctx.localServerEpoch != ctx.lastAppliedEpoch &&
        ctx.localServerEpoch != 0;
    const bool catastrophicDivergence =
        authoritativeEpochReady &&
        ctx.localPlayerReconciled &&
        acknowledgedErrorLen >= correctionConfig.majorCorrectionDistance &&
        !ctx.awaitingTeleportAck &&
        !player.dead;

    const bool applyPosition =
        initialSpawn || serverRespawnedPlayer || catastrophicDivergence ||
        teleportCompletionResync || epochChanged;

    if (applyPosition)
        ctx.teleportResync = false;

    // ── Apply correction ──────────────────────────────────────────────
    // Only hard-snap for authoritative lifecycle changes (spawn, respawn,
    // teleport, epoch change) or true catastrophic divergence (>=100m).
    // Ordinary prediction errors are trusted — the client's predicted
    // position is used as-is. The server validates and accepts input;
    // small disagreements converge naturally during normal gameplay.
    if (applyPosition)
    {
        player.pos = ctx.localServerPosition;
        player.vel = ctx.localServerVelocity;
        player.yaw = ctx.localServerYaw;
        player.ground.onGround = ctx.localServerOnGround;
        player.externalImpulse = glm::vec3(0.0f);
        player.syncLegacyStateToLayers();
        player.updateModelWorldTransforms();
        ctx.lastAppliedEpoch = ctx.localServerEpoch;
    }

    // ── Lifecycle-aware health reconciliation ─────────────────────────
    if (authoritativeNewLife || serverRespawnedPlayer)
    {
        player.currentHp = ctx.localServerHealth;
        player.dead = false;
        player.proceduralFrozen = false;
        player.respawnTimer = 0.0f;
        player.killedBy.clear();
        resetAllWeaponRuntimesForSpawn(player, "multiplayer-reconcile spawn");
        printf("[CLIENT RESPAWN CONFIRMED] playerId=%u requestSerial=%u "
               "epoch=%u snapshotTick=%u position=(%.2f,%.2f,%.2f) "
               "health=%d pendingMs=%llu\n",
               ctx.localPlayerId, ctx.pendingRespawnSerial,
               newEpoch, ctx.latestLocalSnapshotTick,
               ctx.localServerPosition.x, ctx.localServerPosition.y,
               ctx.localServerPosition.z,
               ctx.localServerHealth,
               (unsigned long long)(nowMs() - ctx.pendingRespawnStartedMs));
        ctx.pendingRespawnSerial = 0;
        ctx.pendingRespawnStartEpoch = 0;
    }
    else if (serverKilledPlayer)
    {
        player.currentHp = 0;
    }
    else if (ctx.localPlayerReconciled)
    {
        // Within same life: apply health min (monotonic health across server updates)
        // but ONLY if we already share the same epoch (prevent cross-life contamination)
        if (ctx.lastAppliedEpoch == ctx.localServerEpoch)
            player.currentHp = std::min(player.currentHp, ctx.localServerHealth);
    }
    else
    {
        // First reconciliation of this epoch — apply server health directly
        player.currentHp = ctx.localServerHealth;
    }
    ctx.lastSeenServerHealth = ctx.localServerHealth;

    const glm::vec3 clientPosition = player.pos;
    const bool logCorrection =
        initialSpawn || applyPosition || rawError >= CORRECTION_LOG_DISTANCE;
    if (logCorrection &&
        (applyPosition || currentMs - ctx.lastLocalCorrectionLogMs >= 500))
    {
        printf("[LOCAL CORRECTION] serverTick=%u ackSeq=%u historyFound=%d "
               "sentAtAck=(%.2f,%.2f,%.2f) serverPos=(%.2f,%.2f,%.2f) "
               "currentPredicted=(%.2f,%.2f,%.2f) ackErr=%.3f rawErr=%.3f "
               "class=%s action=%s epoch=%u spawnGen=%u\n",
               ctx.latestLocalSnapshotTick,
               acknowledgedSeq,
               (int)hasAcknowledgedHistory,
               sentPositionAtAck.x, sentPositionAtAck.y, sentPositionAtAck.z,
               ctx.localServerPosition.x,
               ctx.localServerPosition.y,
               ctx.localServerPosition.z,
               clientPosition.x,
               clientPosition.y,
               clientPosition.z,
               acknowledgedErrorLen,
               rawError,
               movementCorrectionClassName(correctionClass),
                applyPosition ? "hard-snap" :
                "trust-client",
                ctx.transformEpoch,
                ctx.lastKnownSpawnGeneration);
        ctx.lastLocalCorrectionLogMs = currentMs;
    }
    ctx.localPlayerReconciled = true;
}

} // namespace MimitaNet
