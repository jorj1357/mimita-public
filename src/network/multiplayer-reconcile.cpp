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
#include "combat/weapon-runtime.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"
#include "config/networking-config.h"
#include "debug/structured-log.h"
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

        mpApplyAuthoritativeTransform(ctx, player);
        ctx.lastAppliedEpoch = ctx.localServerEpoch;
        ctx.localPlayerReconciled = true;

        if (ctx.proceduralTeleportPending &&
            ctx.localServerEpoch != ctx.proceduralTeleportStartEpoch)
        {
            printf("[CLIENT PROCEDURAL TELEPORT COMPLETE] playerId=%u "
                   "epoch=%u position=(%.2f,%.2f,%.2f)\n",
                   ctx.localPlayerId,
                   (unsigned)ctx.localServerEpoch,
                   ctx.localServerPosition.x,
                   ctx.localServerPosition.y,
                   ctx.localServerPosition.z);
            ctx.proceduralTeleportPending = false;
            ctx.teleportResync = true;
        }
    }

    // Client-authoritative roll avatars: the local prediction owns the
    // transform, so skip drift correction (the epoch hard-snap above still
    // applies for spawn/respawn/teleport).
    if (player.rig.movement.mode == "roll")
        return;

    const glm::vec3 clientPosition = player.pos;
    const glm::vec3 correction = ctx.localServerPosition - player.pos;
    const float error = glm::length(correction);
    // Wire the tunable hard-snap distance from config so
    // config/networkingconfig.json local_player_reconciliation.hard_snap_distance
    // actually controls when the client hard-snaps to the server position.
    MovementValidationConfig correctionConfig;
    correctionConfig.majorCorrectionDistance =
        NetworkingConfig::instance().data().localReconciliation.hardSnapDistance;
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
    const uint32_t previousLocalSpawnGeneration = player.spawnGeneration;
    const bool spawnGenerationAdvanced = ctx.lastKnownSpawnGeneration != 0 &&
        ctx.lastKnownSpawnGeneration > previousLocalSpawnGeneration;
    const uint32_t prevEpoch = (uint32_t)ctx.lastAppliedEpoch;
    const uint32_t newEpoch = (uint32_t)ctx.localServerEpoch;
    const bool epochAdvanced = newEpoch != 0 && newEpoch > prevEpoch;
    const bool authoritativeNewLife = ctx.localServerHealth > 0 &&
        (spawnGenerationAdvanced ||
         (ctx.pendingRespawnSerial != 0 && epochAdvanced &&
          (ctx.pendingRespawnStartEpoch == 0 ||
           newEpoch > (uint32_t)ctx.pendingRespawnStartEpoch) &&
          ctx.lastAppliedEpoch == ctx.localServerEpoch));

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
    const bool serverActorCollision =
        authoritativeEpochReady &&
        ctx.localPlayerReconciled &&
        !ctx.awaitingTeleportAck &&
        !player.dead &&
        (ctx.localServerStateFlags & NET_STATE_ACTOR_COLLISION) != 0 &&
        error > 0.02f;
    const bool serverPositionCorrection =
        ((ctx.hasPendingServerPositionCorrection && ctx.localPlayerReconciled) ||
         (authoritativeEpochReady && ctx.localPlayerReconciled &&
          (ctx.localServerStateFlags & NET_STATE_POSITION_CORRECTED) != 0)) &&
        !ctx.awaitingTeleportAck &&
        !player.dead &&
        (ctx.hasPendingServerPositionCorrection || error > 0.001f);
    const bool applyPosition =
        initialSpawn || serverRespawnedPlayer || catastrophicDivergence ||
        postGapResyncActive || teleportCompletionResync || epochChanged ||
        serverActorCollision || serverPositionCorrection;

    if (applyPosition)
    {
        ctx.teleportResync = false;
        ctx.postGapResync = false;
    }

    if (applyPosition)
    {
        const glm::vec3 predictedPosition = player.pos;
        const bool applyEventCorrection =
            serverPositionCorrection && ctx.hasPendingServerPositionCorrection;
        if (applyEventCorrection)
        {
            player.pos = ctx.pendingServerPositionCorrection;
            player.lastValidWorldCollisionPosition = player.pos;
            player.hasLastValidWorldCollisionPosition = true;
            player.updateModelWorldTransforms();
            ctx.hasPendingServerPositionCorrection = false;
        }
        else
            mpApplyAuthoritativeTransform(ctx, player);
        if (authoritativeEpochReady)
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
        if (serverActorCollision)
        {
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Collision, StructuredLevel::Important,
                "actor.collision.reconcile", "local_player",
                "applied_authoritative_actor_collision",
                ctx.latestLocalSnapshotTick,
                nlohmann::json{
                    {"source", "client"},
                    {"error_m", error},
                    {"predicted", {predictedPosition.x, predictedPosition.y,
                                   predictedPosition.z}},
                    {"authoritative", {player.pos.x, player.pos.y,
                                       player.pos.z}},
                    {"server_tick", ctx.latestLocalSnapshotTick},
                    {"transform_epoch", (uint32_t)ctx.localServerEpoch}},
                __FILE__, __LINE__, __FUNCTION__);
        }
        if (serverPositionCorrection)
        {
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Network, StructuredLevel::Important,
                "movement.position-correction.applied", "local_player",
                "applied server collision or movement correction independent of drift threshold",
                ctx.latestLocalSnapshotTick,
                nlohmann::json{
                    {"error_m", error},
                    {"predicted", {predictedPosition.x, predictedPosition.y,
                                   predictedPosition.z}},
                    {"authoritative", {ctx.localServerPosition.x,
                                       ctx.localServerPosition.y,
                                       ctx.localServerPosition.z}},
                    {"server_tick", ctx.latestLocalSnapshotTick},
                    {"transform_epoch", (uint32_t)ctx.localServerEpoch}},
                __FILE__, __LINE__, __FUNCTION__);
        }
    }

    // ── Lifecycle-aware health reconciliation ─────────────────────────
    if (authoritativeNewLife || serverRespawnedPlayer)
    {
        const bool wasDead = player.dead;
        ctx.pendingVictimHealth.clear();
        player.currentHp = ctx.localServerHealth;
        player.maxHp = ctx.localServerHealth;
        player.dead = false;
        player.netPredictedDead = false;
        player.networkDeathPresented = false;
        player.proceduralFrozen = false;
        player.deathAnim = Player::DeathAnimState{};
        player.respawnTimer = 0.0f;
        player.killedBy.clear();
        player.spawnGeneration = ctx.lastKnownSpawnGeneration;
        resetAllWeaponRuntimesForSpawn(player, "multiplayer-reconcile spawn");
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Network, StructuredLevel::Important,
            "client.local.spawn_reconciled", std::to_string(ctx.localPlayerId),
            spawnGenerationAdvanced
                ? "authoritative spawn generation cleared the local dead render gate"
                : "authoritative health transition cleared the local dead render gate",
            ctx.latestLocalSnapshotTick,
            nlohmann::json{{"actor_id", ctx.localPlayerId},
                           {"respawn_detection", spawnGenerationAdvanced
                                ? "spawn_generation" : "health_transition_or_request"},
                           {"spawn_generation_before", previousLocalSpawnGeneration},
                           {"spawn_generation_after", player.spawnGeneration},
                           {"pending_respawn_serial", ctx.pendingRespawnSerial},
                           {"transform_epoch", (uint32_t)ctx.localServerEpoch},
                           {"health", player.currentHp},
                           {"dead_before", wasDead},
                           {"dead_after", player.dead},
                           {"snapshot_tick", ctx.latestLocalSnapshotTick},
                           {"position", {player.pos.x, player.pos.y, player.pos.z}}},
            __FILE__, __LINE__, __FUNCTION__);
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

    // Melee meters are absolute values from the local actor's snapshot. They
    // never accumulate here, so a repeated snapshot cannot add percent twice.
    player.percent = ctx.localServerMeleePercent;
    player.meleeShieldFraction = std::clamp(ctx.localServerMeleeShield, 0.0f, 1.0f);
    player.meleeShieldActive =
        (ctx.localServerStateFlags & NET_STATE_SHIELDING) != 0;

    // Actor-pair prediction and push live in the shared fixed-tick solver
    // (src/physics/movement/actor-pair-collision.cpp). This reconciler only
    // applies the authoritative result selected by the server collision flag.

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
                serverPositionCorrection ? "server-position-correction" :
                serverActorCollision ? "actor-collision" :
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
        StructuredLogger::instance().writeEvent(
            StructuredCategory::NpcCombat, StructuredLevel::Important,
            "npc.damage", "local_player", "victim_hp_apply",
            0u,
            nlohmann::json{
                {"victim", "local_player"},
                {"victim_hp", player.currentHp},
                {"health_after", it->healthAfter},
                {"killed", it->killed},
                {"knockback", {it->knockback.x, it->knockback.y, it->knockback.z}}},
            __FILE__, __LINE__, __FUNCTION__);
        Debug::log(Debug::Category::Networking,
            "[NET VICTIM HEALTH APPLY] hp=%d killed=%d timelineReady=%d\n",
            player.currentHp, (int)it->killed, (int)timelineReady);
        it = ctx.pendingVictimHealth.erase(it);
    }
}

} // namespace MimitaNet
