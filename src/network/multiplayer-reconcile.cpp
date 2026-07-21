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
    }

    const glm::vec3 clientPosition = player.pos;
    const glm::vec3 correction = ctx.localServerPosition - player.pos;
    const float error = glm::length(correction);
    const MovementValidationConfig correctionConfig;
    const MovementCorrectionClass correctionClass =
        classifyMovementCorrection(error, correctionConfig);
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

    // Catastrophic divergence: only apply after the current authoritative
    // epoch has been applied locally.  This prevents a stale server position
    // from snapping the player while initial spawn/respawn/teleport
    // acknowledgement is still in flight.
    const bool authoritativeEpochReady =
        ctx.localServerEpoch != 0 &&
        ctx.transformEpoch == ctx.localServerEpoch &&
        ctx.lastAppliedEpoch == ctx.localServerEpoch;
    const bool catastrophicDivergence =
        authoritativeEpochReady &&
        ctx.localPlayerReconciled &&
        correctionClass == MovementCorrectionClass::Major &&
        !ctx.awaitingTeleportAck &&
        !player.dead;

    const bool teleportCompletionResync =
        ctx.teleportResync && !ctx.awaitingTeleportAck;
    const bool epochChanged =
        ctx.localPlayerReconciled &&
        ctx.localServerEpoch != ctx.lastAppliedEpoch &&
        ctx.localServerEpoch != 0;
    const bool applyPosition =
        initialSpawn || serverRespawnedPlayer || catastrophicDivergence ||
        teleportCompletionResync || epochChanged;

    if (applyPosition)
        ctx.teleportResync = false;

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

    if (!player.dead)
    {
        const Capsule localCapsule = player.getCapsule();
        for (const auto& entry : ctx.remotePlayers)
        {
            const Player& remote = entry.second;
            if (remote.dead)
                continue;

            const Capsule remoteCapsule = remote.getCapsule();
            const float localBottom = localCapsule.a.z - localCapsule.r;
            const float localTop = localCapsule.b.z + localCapsule.r;
            const float remoteBottom = remoteCapsule.a.z - remoteCapsule.r;
            const float remoteTop = remoteCapsule.b.z + remoteCapsule.r;
            if (localTop <= remoteBottom || remoteTop <= localBottom)
                continue;

            glm::vec2 delta(player.pos.x - remote.pos.x, player.pos.y - remote.pos.y);
            float distance = glm::length(delta);
            const float minimumDistance = localCapsule.r + remoteCapsule.r;
            if (distance >= minimumDistance)
                continue;

            glm::vec2 normal(1.0f, 0.0f);
            if (distance > 0.0001f)
                normal = delta / distance;
            const float penetration = minimumDistance - distance;
            player.pos += glm::vec3(normal * penetration, 0.0f);

            glm::vec2 planarVelocity(player.vel.x, player.vel.y);
            const float intoRemote = glm::dot(planarVelocity, normal);
            if (intoRemote < 0.0f)
            {
                planarVelocity -= normal * intoRemote;
                player.vel.x = planarVelocity.x;
                player.vel.y = planarVelocity.y;
            }

            static uint64_t lastCollisionLogMs = 0;
            const uint64_t collisionNowMs = nowMs();
            if (collisionNowMs - lastCollisionLogMs >= 250)
            {
                printf("[CLIENT PLAYER COLLISION] localId=%u remoteId=%u penetration=%.3f "
                       "localPos=(%.2f,%.2f,%.2f) remotePos=(%.2f,%.2f,%.2f)\n",
                       ctx.localPlayerId, entry.first, penetration,
                       player.pos.x, player.pos.y, player.pos.z,
                       remote.pos.x, remote.pos.y, remote.pos.z);
                lastCollisionLogMs = collisionNowMs;
            }
        }
    }

    const bool logCorrection =
        initialSpawn || applyPosition || error >= CORRECTION_LOG_DISTANCE;
    if (logCorrection &&
        (applyPosition || currentMs - ctx.lastLocalCorrectionLogMs >= 500))
    {
        printf("[LOCAL CORRECTION] distance=%.3f class=%s "
               "serverPos=(%.2f,%.2f,%.2f) clientPos=(%.2f,%.2f,%.2f) "
               "applied=%d reason=%s localEpoch=%u serverEpoch=%u lastAppliedEpoch=%u\n",
               error,
               movementCorrectionClassName(correctionClass),
               ctx.localServerPosition.x,
               ctx.localServerPosition.y,
               ctx.localServerPosition.z,
               clientPosition.x,
               clientPosition.y,
               clientPosition.z,
               (int)applyPosition,
                initialSpawn ? "initial-spawn" :
                serverRespawnedPlayer ? "server-respawn" :
                catastrophicDivergence ? "catastrophic-divergence" :
                epochChanged ? "epoch-changed" :
                serverKilledPlayer ? "server-death" : "within-tolerance",
               ctx.transformEpoch,
               (uint32_t)ctx.localServerEpoch,
               (uint32_t)ctx.lastAppliedEpoch);
        ctx.lastLocalCorrectionLogMs = currentMs;
    }
    ctx.localPlayerReconciled = true;
}

} // namespace MimitaNet
