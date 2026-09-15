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
#include "network/disagreement-visuals.h"
#include "live-code/live-behavior.h"
#include "hot-reload/hot-reconciliation.h"
#include "hot-reload/hot-reload-system.h"
#include "combat/weapon-runtime.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"
#include "config/networking-config.h"
#include "npc/npc-combat-log.h"
#include "debug/debug-log.h"

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
    MovementCorrectionClass correctionClass =
        classifyMovementCorrection(error, correctionConfig);

    // Hot reconciliation policy: the hot handler owns the error metric,
    // thresholds, and snap/smooth/hard-reset decision. Cold code executes it.
    {
        GameReconcileV1 rq{};
        rq.predictedPosition[0] = player.pos.x;
        rq.predictedPosition[1] = player.pos.y;
        rq.predictedPosition[2] = player.pos.z;
        rq.predictedVelocity[0] = player.vel.x;
        rq.predictedVelocity[1] = player.vel.y;
        rq.predictedVelocity[2] = player.vel.z;
        rq.authoritativePosition[0] = ctx.localServerPosition.x;
        rq.authoritativePosition[1] = ctx.localServerPosition.y;
        rq.authoritativePosition[2] = ctx.localServerPosition.z;
        rq.authoritativeVelocity[0] = ctx.localServerVelocity.x;
        rq.authoritativeVelocity[1] = ctx.localServerVelocity.y;
        rq.authoritativeVelocity[2] = ctx.localServerVelocity.z;
        rq.positionError = error;
        rq.velocityError =
            glm::length(ctx.localServerVelocity - player.vel);
        rq.smallDistance = correctionConfig.smallCorrectionDistance;
        rq.mediumDistance = correctionConfig.mediumCorrectionDistance;
        rq.majorDistance = correctionConfig.majorCorrectionDistance;
        rq.predictedTick = ctx.latestLocalSnapshotTick;
        // Real logical generation identity: predicted state is produced by the
        // locally active generation; authoritative state by the server's.
        rq.predictedGeneration =
            (std::uint64_t)HotReloadSystem::instance().status().activeGeneration;
        rq.authoritativeGeneration = (std::uint64_t)ctx.serverCodeGeneration;
        rq.handled = 0;
        if (LiveBehavior::dispatchGameplayEvent64(GAME_EVENT_RECONCILE, &rq,
                                                  sizeof(rq), 0, 0, 0) &&
            rq.handled) {
            switch (rq.correctionMode) {
            case 1u: correctionClass = MovementCorrectionClass::Small; break;
            case 2u: correctionClass = MovementCorrectionClass::Medium; break;
            case 3u:
            case 4u: correctionClass = MovementCorrectionClass::Major; break;
            default: correctionClass = MovementCorrectionClass::None; break;
            }
        }
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

    // Post-blackout resync: after a snapshot gap (blackout/reconnect) the local
    // prediction may have drifted Medium-far from the server's authoritative
    // position. Snap it back so it self-corrects instead of sticking until the
    // next death. Only fires when the divergence is real (> 1.5 units) and the
    // authoritative epoch has been applied.
    constexpr float POST_GAP_MIN_ERROR = 1.5f;
    const bool postGapResyncActive =
        ctx.postGapResync &&
        currentMs < ctx.postGapResyncDeadlineMs &&
        ctx.localPlayerReconciled &&
        authoritativeEpochReady &&
        !ctx.awaitingTeleportAck &&
        !player.dead &&
        error > POST_GAP_MIN_ERROR;

    const bool teleportCompletionResync =
        ctx.teleportResync && !ctx.awaitingTeleportAck;
    const bool epochChanged =
        ctx.localPlayerReconciled &&
        ctx.localServerEpoch != ctx.lastAppliedEpoch &&
        ctx.localServerEpoch != 0;
    bool applyPosition =
        initialSpawn || serverRespawnedPlayer || catastrophicDivergence ||
        postGapResyncActive || teleportCompletionResync || epochChanged;

    if (applyPosition)
    {
        ctx.teleportResync = false;
        ctx.postGapResync = false;
    }

    // Hot reconcile policy seam: while ragdoll physics owns the local root, the
    // authoritative snap must not fight it. Otherwise the client snaps its
    // reported position back to the stale server position every frame and the
    // server (which takes validated client position) can never advance. The
    // behavior owns the decision; the kernel enforces the ragdoll default even
    // when no behavior is loaded.
    if (applyPosition)
    {
        MovementReconcileV1 policy{};
        policy.ownerActor = ctx.localPlayerId;
        policy.clientPosition[0] = clientPosition.x;
        policy.clientPosition[1] = clientPosition.y;
        policy.clientPosition[2] = clientPosition.z;
        policy.serverPosition[0] = ctx.localServerPosition.x;
        policy.serverPosition[1] = ctx.localServerPosition.y;
        policy.serverPosition[2] = ctx.localServerPosition.z;
        policy.error = error;
        policy.majorThreshold = correctionConfig.majorCorrectionDistance;
        policy.epochReady = authoritativeEpochReady ? 1u : 0u;
        policy.ragdollActive = player.ragdollModeActive ? 1u : 0u;
        policy.teleportPending = ctx.awaitingTeleportAck ? 1u : 0u;
        policy.dead = player.dead ? 1u : 0u;
        policy.applyPosition = 1u;
        LiveBehavior::dispatchPayload(GAME_EVENT_MOVEMENT_RECONCILE, &policy,
                                      sizeof(policy), 0, 0, 0, 0);
        if (policy.handled && policy.applyPosition == 0)
            applyPosition = false;
        else if (!policy.handled && policy.ragdollActive)
            applyPosition = false;
    }

    if (applyPosition)
    {
        const glm::vec3 predictedPosition = player.pos;
        player.pos = ctx.localServerPosition;
        player.vel = ctx.localServerVelocity;
        player.yaw = ctx.localServerYaw;
        player.ground.onGround = ctx.localServerOnGround;
        player.externalImpulse = glm::vec3(0.0f);
        player.syncLegacyStateToLayers();
        player.updateModelWorldTransforms();
        ctx.lastAppliedEpoch = ctx.localServerEpoch;

        // Local-only correction indicator for the corrected player.
        // Catastrophic divergence is the one applyPosition case that is a real
        // client/server disagreement (not spawn, respawn, or a requested teleport).
        if (catastrophicDivergence)
        {
            DisagreementEvent event;
            event.timeMs = currentMs;
            event.reason = DISAGREEMENT_POSITION_CORRECTION;
            event.position = predictedPosition;
            event.correction = ctx.localServerPosition - predictedPosition;
            event.description = "catastrophic divergence";
            spawnLocalDisagreementIndicator(event);
        }
    }

    // ── Lifecycle-aware health reconciliation ─────────────────────────
    if (authoritativeNewLife || serverRespawnedPlayer)
    {
        ctx.pendingVictimHealth.clear();
        player.currentHp = ctx.localServerHealth;
        player.maxHp = ctx.localServerHealth;
        player.dead = false;
        player.proceduralFrozen = false;
        player.deathAnim = Player::DeathAnimState{};
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
    else if (serverKilledPlayer && ctx.pendingVictimHealth.empty())
    {
        // Death timed to the shot visual: when a server-confirmed damage event
        // for this life is still held (pendingVictimHealth), the death applies
        // when the pending is drained — not the frame the snapshot shows 0.
        player.currentHp = 0;

        // Server-confirmed local death: spawn the death ellipsoid so the victim
        // sees it even though the death was not locally predicted.
        if (NetworkingConfig::instance().data().deathEffects.localPlayerDeathEffect)
        {
            const auto& deCfg = HitEffects::config().deathEllipsoid;
            if (deCfg.enabled)
            {
                EffectPartSystem::instance().spawnDeathEllipsoid(
                    player.pos, glm::vec3(0.0f, 0.0f, 1.0f),
                    deCfg.length, deCfg.radius, deCfg.lifetime,
                    player.sizeScale);
            }
            Debug::log(Debug::Category::Networking,
                "[NET LOCAL DEATH FX] playerId=%u pos=(%.1f,%.1f,%.1f)",
                ctx.localPlayerId, player.pos.x, player.pos.y, player.pos.z);
        }
    }
    else if (ctx.localPlayerReconciled)
    {
        // Within same life the local player's HP is event-driven and timed to
        // the shot visual (see pendingVictimHealth / mpDrainPendingVictimHealth).
        // Snapshots only feed server health RISES (kill-heal flows up so the
        // 100% heal-back always sticks); they never drop HP here.
        if (ctx.lastAppliedEpoch == ctx.localServerEpoch &&
            ctx.localServerHealth > player.currentHp)
            player.currentHp = ctx.localServerHealth;
        // The server's max HP (healthall override) can exceed the local max.
        // Track the highest server health seen this life so the bar shows the
        // real denominator (999/999, not 999/100).
        if (ctx.localServerHealth > player.maxHp)
            player.maxHp = ctx.localServerHealth;
    }
    else
    {
        // First reconciliation of this epoch — apply server health directly
        player.currentHp = ctx.localServerHealth;
        if (ctx.localServerHealth > player.maxHp)
            player.maxHp = ctx.localServerHealth;
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

// Apply server-confirmed victim health changes in step with the shot visual.
// DamageConfirmedEvent packets queue a pending change with a short render-delay
// hold; once the hold elapses the HP drops exactly when the corresponding shot
// visual plays (both are delayed by the same render delay).
void mpDrainPendingVictimHealth(MultiplayerContext& ctx, Player& player)
{
    if (ctx.pendingVictimHealth.empty())
        return;
    const uint64_t now = nowMs();
    for (auto it = ctx.pendingVictimHealth.begin();
         it != ctx.pendingVictimHealth.end(); )
    {
        // Apply in the same frame the shot visual plays: the attacker's body
        // must have rendered past the shot's server tick (or the max-hold
        // wall-clock fallback must have elapsed) so the HP drop is never out
        // of order relative to the bullet/attacker. NPC damage (shooterId==0)
        // has no visual-timeline gate, so it applies immediately.
        const bool npcDamage = (it->shooterId == 0);
        const bool timelineReady =
            npcDamage ||
            (it->shooterId != 0 &&
             mpVisualTimelineReady(ctx, it->shooterId, it->eventServerTick,
                                   it->receivedMs));
        const bool maxHoldElapsed = now >= it->applyAtMs;
        if (!timelineReady && !maxHoldElapsed)
        {
            ++it;
            continue;
        }
        if (it->killed)
            player.currentHp = 0;
        else
            player.currentHp = std::max(0, it->healthAfter);
        // Knockback applies in the exact same frame as the HP drop (both are
        // tied to the bullet visual via the same timeline gate).
        if (glm::length(it->knockback) > 0.001f)
            player.externalImpulse += it->knockback;
        npcLog("npc-victim-hp-apply hp=%d healthAfter=%d killed=%d apply=1 knockback=(%.2f %.2f %.2f)",
               player.currentHp, it->healthAfter, (int)it->killed,
               it->knockback.x, it->knockback.y, it->knockback.z);
        Debug::log(Debug::Category::Networking,
            "[NET VICTIM HEALTH APPLY] hp=%d killed=%d timelineReady=%d\n",
            player.currentHp, (int)it->killed, (int)timelineReady);
        it = ctx.pendingVictimHealth.erase(it);
    }
}

} // namespace MimitaNet
