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
    constexpr float CATASTROPHIC_DIVERGENCE = 100.0f;
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
    const bool serverKilledPlayer = ctx.localServerHealth <= 0 && !player.dead;
    const bool serverRespawnedPlayer =
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
        error > CATASTROPHIC_DIVERGENCE &&
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

    if (serverRespawnedPlayer)
        player.currentHp = ctx.localServerHealth;
    else if (serverKilledPlayer)
        player.currentHp = 0;
    else
        player.currentHp = std::min(player.currentHp, ctx.localServerHealth);
    ctx.lastSeenServerHealth = ctx.localServerHealth;
    if (serverRespawnedPlayer)
    {
        resetAllWeaponRuntimesForSpawn(player, "multiplayer-reconcile serverRespawn");
        player.dead = false;
        player.proceduralFrozen = false;
        player.respawnTimer = 0.0f;
        player.killedBy.clear();
        ctx.nextLocalRespawnSerial = 0;
        printf("%s [CLIENT RESPAWN CONFIRMED] player=%u respawnSerial=0 (server acknowledged)\n",
               serverTimestamp(), ctx.localPlayerId);
    }

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
        printf("[LOCAL CORRECTION] distance=%.3f "
               "serverPos=(%.2f,%.2f,%.2f) clientPos=(%.2f,%.2f,%.2f) "
               "applied=%d reason=%s localEpoch=%u serverEpoch=%u lastAppliedEpoch=%u\n",
               error,
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
