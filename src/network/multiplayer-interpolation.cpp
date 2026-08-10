// 07 21 2026, 17 10
/* purpose
* Owns client-side remote entity interpolation and remote movement presentation triggers.
* Filters remote snapshots by server tick, spawn generation, and transform epoch.
* Applies accepted snapshot targets to rendered remote player replicas.
* Does NOT parse sockets, validate client movement reports, or own packet schemas.
* Does NOT simulate authoritative server movement or local prediction.
* Does NOT interpolate across respawn, teleport, or stale lifecycle boundaries.
*/

#include "network/multiplayer-context.h"
#include "network/net_common.h"
#include "network/simulation-constants.h"
#include "network/remote-entity-lifecycle.h"
#include "network/disagreement-visuals.h"
#include "config/networking-config.h"
#include "config/ragdoll-death-config.h"
#include "avatar/avatar.h"
#include "config/player-settings.h"
#include "entities/death-ghost.h"
#include "combat/death-system.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-runtime.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"
#include "audio/audio.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "terminal/terminal-state.h"
#include "network/movement-validation.h"
#include "world/world.h"
#include "physics/movement/physics-collision.h"
#include "physics/movement/physics-collision-shared.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <glm/glm.hpp>

// Shared online-mode context pointer (defined in main.cpp), used by the
// predicted kill-heal rollback inside clearPredictedDeath so the HP restore and
// the disagreement effect land on the exact same tick.
extern MimitaNet::MultiplayerContext* gpMpContext;

// Client render world (defined in main.cpp, set at map load). Used by the
// remote-body geometry safety clamp in updateRenderedReplica.
extern World* gpWorld;

namespace MimitaNet {

bool gNetInterpDebug = false;

namespace {

uint64_t gInterpolationDebugFrame = 0;
double gInterpolationClockStepMs = 0.0;
uint32_t gInterpolationReanchorCount = 0;
double gInterpolationReanchorMagnitudeMs = 0.0;
std::string gInterpolationReanchorReason;

SnapshotTransform transformFromEntity(const SnapshotEntity& entity)
{
    SnapshotTransform transform;
    transform.position = {entity.px, entity.py, entity.pz};
    transform.velocity = {entity.vx, entity.vy, entity.vz};
    transform.yaw = entity.yaw;
    transform.health = entity.health;
    transform.onGround = entity.onGround != 0;
    transform.equippedSlot = entity.equippedSlot;
    transform.weaponState = entity.weaponState;
    transform.aimDirection = {entity.aimX, entity.aimY, entity.aimZ};
    transform.pingMs = entity.pingMs;
    transform.vipAppearance = MimitaVip::appearanceFromBytes(
        entity.vipTier, entity.vipStyleKind, entity.vipColorR,
        entity.vipColorG, entity.vipColorB, entity.vipFlags);
    transform.receivedMs = nowMs();
    transform.stateFlags = entity.stateFlags;
    transform.sizeScale = entity.sizeScale;
    transform.networkEntityId = entity.networkEntityId;
    transform.transformEpoch = entity.transformEpoch;
    transform.dashSerial = entity.dashSerial;
    transform.groundJumpSerial = entity.groundJumpSerial;
    transform.airJumpSerial = entity.airJumpSerial;
    transform.downDashSerial = entity.downDashSerial;
    transform.directionChangeSerial = entity.directionChangeSerial;
    transform.equipSerial = entity.equipSerial;
    transform.freezeSerial = entity.freezeSerial;
    transform.spawnGeneration = entity.spawnGeneration;
    return transform;
}

// Shortest-path angular interpolation (yaw is in radians).
float lerpAngle(float a, float b, float t)
{
    float delta = std::fmod(b - a, 6.28318530718f);
    if (delta > 3.14159265359f) delta -= 6.28318530718f;
    if (delta < -3.14159265359f) delta += 6.28318530718f;
    return a + delta * t;
}

// Builds the render-time snapshot for a remote actor using a continuous
// wall-clock-anchored render clock: `globalRenderTick` is the estimated
// current server time (in 60Hz tick units) and the body is rendered at
// `globalRenderTick - delayTicks`. Because the render time is derived from a
// smooth clock instead of a per-entity accumulator clamped to a target, the
// interpolation alpha is ALWAYS fractional between the two buffered snapshots
// that bracket it — at any frame rate. Reordered bursts and lost-snapshot
// holes pass through the buffer as real time advances; there is no artificial
// "catch-up fast-forward" that made remote bodies freeze-then-jump.
void buildReceiveTimeRender(EntityInterpolationState& interpolation,
                            const RemotePlayerInterpolationConfig& interpCfg,
                            const double delaySeconds,
                            const double globalRenderTick,
                            bool allowExtrapolation,
                            uint32_t linearSnapGapTicks,
                            SnapshotTransform& out)
{
    const uint32_t oldestTick = interpolation.buffer.front().serverTick;
    const uint32_t newestTick = interpolation.buffer.back().serverTick;
    const double previousAlpha = interpolation.sampleAlpha;

    // `linearSnapGapTicks` is 0 for every non-linear mode and nonzero for
    // linear mode (the caller passes motion.linearSnapAfterGapTicks there).
    // linear_never_skip: linear must never render the newest directly — both
    // emergency snaps are bypassed and wide tick gaps freeze instead of bridge.
    const auto& motionCfg = NetworkingConfig::instance()
        .data().remoteMotionSmoothing;
    const bool linearMode = linearSnapGapTicks > 0;
    // ease is a linear-family mode but never hole-freezes (gaps bridge smoothly).
    const bool easeMode = linearMode && motionCfg.renderFilter == "ease";
    const bool linearNeverSkip = linearMode && !easeMode && motionCfg.linearNeverSkip;

    const double delayTicks = delaySeconds * (double)GAMEPLAY_SIMULATION_HZ;
    const double desiredRenderTick = globalRenderTick - delayTicks;
    // Monotonic render time: never render earlier than the previous frame,
    // even when the (adaptive) delay deepens. Prevents any backward slide.
    const double prevRenderTick = interpolation.renderTick;
    const double renderTick = (prevRenderTick > desiredRenderTick)
        ? prevRenderTick : desiredRenderTick;
    interpolation.previousRenderTick = prevRenderTick;
    interpolation.renderTickDelta =
        prevRenderTick > 0.0 ? renderTick - prevRenderTick : 0.0;
    interpolation.holding = false;
    interpolation.snappedOrCorrected = false;
    interpolation.renderSampleJumped = false;
    interpolation.sampleOlderTick = oldestTick;
    interpolation.sampleNewerTick = newestTick;
    interpolation.previousSampleAlpha = previousAlpha;
    interpolation.sampleAlpha = 0.0;
    interpolation.sampleAlphaDelta = 0.0;
    interpolation.renderTick = renderTick;
    const double renderStepMs = interpolation.renderTickDelta /
        (double)GAMEPLAY_SIMULATION_HZ * 1000.0;
    const double maxJumpMs = motionCfg.maxRenderTimeJumpSeconds * 1000.0;
    if (prevRenderTick > 0.0 && maxJumpMs > 0.0 &&
        renderStepMs > gInterpolationClockStepMs + maxJumpMs)
    {
        interpolation.renderSampleJumped = true;
        ++interpolation.renderJumpCount;
    }

    // Buffer ran dry: renderTick is newer than everything buffered. Extrapolate
    // along the newest velocity. When extrapolation_keep_moving is set, keep
    // gliding past the time cap with exponentially decaying velocity instead of
    // freezing the body at a fixed spot (a hard hold read as "jitter in one
    // spot, then skip"). Otherwise stop at the cap.
    if (renderTick > (double)newestTick)
    {
        ++interpolation.bufferUnderrunCount;
        const SnapshotTransform& newest = interpolation.buffer.back();
        interpolation.sampleOlderTick = newestTick;
        interpolation.sampleNewerTick = newestTick;
        interpolation.sampleAlpha = 1.0;
        interpolation.sampleAlphaDelta = interpolation.sampleAlpha - previousAlpha;
        if (!allowExtrapolation)
        {
            interpolation.extrapolating = false;
            interpolation.holding = true;
            ++interpolation.holdCount;
            out = interpolation.hasRendered ? interpolation.lastRender : newest;
            return;
        }
        out = newest;
        interpolation.extrapolating = true;
        const double extraTicks = renderTick - (double)newestTick;
        const double extraMs =
            extraTicks / (double)GAMEPLAY_SIMULATION_HZ * 1000.0;
        const double capMs =
            interpCfg.maximumExtrapolationSeconds * 1000.0;
        double moveMs = extraMs;
        if (extraMs > capMs && interpCfg.extrapolationKeepMoving)
        {
            const double beyond = extraMs - capMs;
            const double decayMs = std::max(1.0, capMs * 4.0);
            moveMs = capMs + beyond * std::exp(-beyond / decayMs);
        }
        else if (extraMs > capMs)
        {
            moveMs = capMs;
        }
        glm::vec3 extrapPos = newest.position +
            newest.velocity * (float)(moveMs / 1000.0);
        // A grounded body must never be extrapolated below its last known floor
        // height (a resting player's broadcast vz is ~0 now, but this is the
        // hard guarantee against sinking into the ground during dry spells).
        if (newest.onGround)
            extrapPos.z = std::max(extrapPos.z, newest.position.z);
        out.position = extrapPos;
        if (gNetInterpDebug)
            Debug::log(Debug::Category::Networking,
                "[NETINTERP EXTRAP] id=%u over=%.1fms",
                interpolation.networkEntityId, moveMs);
        out.serverTick = (uint32_t)std::floor(renderTick);
        return;
    }
    interpolation.extrapolating = false;

    // Fresh spawn / very thin buffer: render time older than everything
    // buffered. Hold the oldest snapshot until enough history accumulates;
    // the clock is wall-clock anchored, so it recovers naturally as snapshots
    // arrive (no manual catch-up needed).
    if (renderTick < (double)oldestTick)
    {
        interpolation.holding = true;
        ++interpolation.holdCount;
        out = interpolation.buffer.front();
        out.serverTick = oldestTick;
        interpolation.sampleOlderTick = oldestTick;
        interpolation.sampleNewerTick = oldestTick;
        interpolation.sampleAlpha = 0.0;
        interpolation.sampleAlphaDelta = -previousAlpha;
        return;
    }

    const uint32_t renderFloor = (uint32_t)std::floor(renderTick);
    const SnapshotTransform* older = &interpolation.buffer.front();
    const SnapshotTransform* newer = &interpolation.buffer.back();
    for (size_t i = 0; i + 1 < interpolation.buffer.size(); ++i)
    {
        const SnapshotTransform& a = interpolation.buffer[i];
        const SnapshotTransform& b = interpolation.buffer[i + 1];
        if (a.serverTick <= renderFloor)
        {
            older = &a;
            if (b.serverTick > renderFloor)
            {
                newer = &b;
                break;
            }
        }
        else
        {
            older = &a;
            newer = &b;
            break;
        }
    }

    double alpha = 1.0;
    if (newer->serverTick > older->serverTick)
    {
        alpha = (renderTick - (double)older->serverTick) /
                (double)(newer->serverTick - older->serverTick);
        alpha = std::clamp(alpha, 0.0, 1.0);
    }
    interpolation.sampleOlderTick = older->serverTick;
    interpolation.sampleNewerTick = newer->serverTick;
    interpolation.sampleAlpha = alpha;
    interpolation.sampleAlphaDelta = alpha - previousAlpha;

    // Time-gap protection: a hole wider than the gap threshold (e.g. a long
    // blackout) is a discontinuity, not a velocity bridge. With teleport_gap_snap
    // snap to the newest snapshot rather than sliding a stale straight line
    // across seconds of missing motion; otherwise fall through so the motion
    // filter converges the body smoothly. Short loss gaps interpolate linearly.
    // linear mode may override the threshold via linear_snap_after_gap_ticks.
    // linear_never_skip disables this emergency snap entirely.
    const uint32_t gapTicks = linearSnapGapTicks > 0
        ? linearSnapGapTicks : interpCfg.teleportGapTicks;
    if (!linearNeverSkip &&
        (uint32_t)(newer->serverTick - older->serverTick) > gapTicks &&
        interpCfg.teleportGapSnap)
    {
        interpolation.snappedOrCorrected = true;
        ++interpolation.hardSnapCount;
        out = *newer;
        out.serverTick = newer->serverTick;
        return;
    }

    // Corruption protection: an absurd per-segment distance without a
    // lifecycle change is treated as a teleport. Not a speed limit.
    // linear_never_skip disables this emergency snap entirely.
    const float gapDist = glm::length(newer->position - older->position);
    if (!linearNeverSkip && gapDist > interpCfg.teleportDistance)
    {
        interpolation.snappedOrCorrected = true;
        ++interpolation.hardSnapCount;
        out = *newer;
        return;
    }

    // linear_never_skip hole freeze: a tick gap wider than
    // linear_hold_gap_ticks is a loss hole / blackout, not a velocity bridge.
    // Bridging it would lerp across the hole with a near-1 alpha in a single
    // frame (the "snap" linear was famous for). Freeze at the last rendered
    // position until the render clock reaches continuous data; the final glide
    // gate then handles the resume smoothly.
    if (linearNeverSkip && interpolation.hasRendered &&
        (uint32_t)(newer->serverTick - older->serverTick) >
            motionCfg.linearHoldGapTicks)
    {
        interpolation.holding = true;
        ++interpolation.holdCount;
        out = interpolation.lastRender;
        out.serverTick = renderFloor;
        return;
    }

    out = (alpha >= 1.0) ? *newer : *older;
    // position_mode: "linear" (default) lerps a straight line between samples.
    if (interpCfg.positionMode == "linear")
        out.position = older->position +
            (newer->position - older->position) * (float)alpha;
    else
        out.position = glm::mix(older->position, newer->position, (float)alpha);
    out.velocity = older->velocity +
        (newer->velocity - older->velocity) * (float)alpha;
    // rotation_mode: "slerp" (default) takes the shortest angular path.
    if (interpCfg.rotationMode == "slerp")
        out.yaw = lerpAngle(older->yaw, newer->yaw, (float)alpha);
    else
        out.yaw = older->yaw + (newer->yaw - older->yaw) * (float)alpha;
    {
        glm::vec3 aim = older->aimDirection * (1.0f - (float)alpha) +
                        newer->aimDirection * (float)alpha;
        if (glm::dot(aim, aim) > 0.000001f)
            out.aimDirection = glm::normalize(aim);
    }
    out.serverTick = renderFloor;
}

double adaptiveDelaySeconds(EntityInterpolationState& interpolation,
                            const NetworkingConfigData& cfg,
                            float dt)
{
    double desired = NetworkingConfig::instance()
        .effectiveRemoteInterpolationDelaySeconds();

    const AdaptiveSnapshotBufferConfig& adaptive =
        cfg.adaptiveSnapshotBuffer;
    if (adaptive.enabled)
    {
        const double minSnapshotDelay =
            cfg.remotePlayers.minimumSnapshotsBeforeRendering > 1
                ? ((double)cfg.remotePlayers.minimumSnapshotsBeforeRendering - 1.0) /
                    (double)GAMEPLAY_SIMULATION_HZ
                : 0.0;
        desired = std::max(desired, adaptive.minimumDelaySeconds);
        desired = std::max(desired, minSnapshotDelay);
        desired = std::max(
            desired,
            (interpolation.estimatedArrivalJitterMs *
             adaptive.jitterMultiplier) / 1000.0);
        // Loss-driven growth: when snapshots arrive gappy, deepen the buffer so
        // the render stays inside it and never falls into extrapolation.
        desired = std::max(
            desired,
            adaptive.minimumDelaySeconds +
            interpolation.recentLossFraction * adaptive.lossDelayBudgetSeconds);
        desired = std::min(desired, adaptive.maximumDelaySeconds);
    }

    if (interpolation.adaptiveDelaySeconds <= 0.0)
    {
        interpolation.adaptiveDelaySeconds = desired;
        return desired;
    }

    const double rateMs = desired > interpolation.adaptiveDelaySeconds
        ? adaptive.increaseRateMsPerSecond
        : adaptive.decreaseRateMsPerSecond;
    const double maxStep = (rateMs / 1000.0) * (double)dt;
    const double delta = desired - interpolation.adaptiveDelaySeconds;
    if (std::abs(delta) <= maxStep)
        interpolation.adaptiveDelaySeconds = desired;
    else
        interpolation.adaptiveDelaySeconds += delta > 0.0 ? maxStep : -maxStep;
    return interpolation.adaptiveDelaySeconds;
}

void clearPredictedDeath(Player& player, const glm::vec3& at,
                         uint32_t entityId, const char* reason,
                         bool showDisagreement)
{
    if (!player.netPredictedDead)
        return;

    // A predicted kill-heal must roll back in the exact same tick the server
    // disagreement effect appears, so the HP restore is never a silent desync.
    // Force the disagreement visual on whenever a pending predicted kill-heal
    // for this entity is being rolled back.
    if (gpMpContext && gpMpContext->active &&
        gpMpContext->predictedKillHealPending &&
        gpMpContext->predictedKillHealTargetEntityId == entityId)
    {
        showDisagreement = true;
        mpRollbackPredictedKillHeal(*gpMpContext, entityId);
    }

    player.dead = false;
    player.netPredictedDead = false;
    player.proceduralFrozen = false;
    player.respawnTimer = 0.0f;
    player.deathAnim = Player::DeathAnimState{};
    // This life never died: a predicted-death ghost must be removed and the
    // next real death must be allowed to present again.
    player.networkDeathPresented = false;
    DeathGhostSystem::instance().removeForOwner(entityId);
    Debug::warn(Debug::Category::Networking,
        "[NET PREDICTED DEATH ROLLBACK] entityId=%u reason=%s pos=(%.2f,%.2f,%.2f)\n",
        entityId, reason ? reason : "unknown", at.x, at.y, at.z);

    if (showDisagreement &&
        NetworkingConfig::instance().data().disagreement.enabled)
    {
        DisagreementEvent ev;
        ev.timeMs = nowMs();
        ev.reason = DISAGREEMENT_INVALID_STATE;
        ev.position = at;
        ev.correction = glm::vec3(0.0f);
        ev.targetPlayerId = entityId;
        ev.description = reason ? reason : "PREDICTED DEATH ROLLBACK";
        spawnDisagreementEffect(ev);
        logDisagreement(ev);
    }
}

int applyPredictedHealthOverlay(Player& player,
                                EntityInterpolationState& interpolation,
                                int authoritativeHealth)
{
    const uint64_t now = nowMs();
    const double timeoutMs = NetworkingConfig::instance()
        .data().retries.attackRequestTimeoutMs;

    if (interpolation.predictedHealthCap >= 0 &&
        authoritativeHealth <= interpolation.predictedHealthCap)
    {
        interpolation.pendingPredictedDamage = 0;
        interpolation.predictedHealthCap = -1;
    }
    else if (interpolation.predictedHealthCap >= 0 &&
             interpolation.predictedHealthUpdatedMs != 0 &&
             now >= interpolation.predictedHealthUpdatedMs &&
             (double)(now - interpolation.predictedHealthUpdatedMs) > timeoutMs)
    {
        interpolation.pendingPredictedDamage = 0;
        interpolation.predictedHealthCap = -1;
        ++interpolation.predictedHealthRollbackCount;
        if (authoritativeHealth > 0)
            clearPredictedDeath(player, player.pos, interpolation.networkEntityId,
                                "PREDICTED HIT TIMEOUT", false);
    }

    if (interpolation.predictedHealthCap >= 0)
        return std::min(authoritativeHealth, interpolation.predictedHealthCap);
    return authoritativeHealth;
}

void logInterpolationState(EntityInterpolationState& interpolation,
                           uint32_t entityId,
                           const glm::vec3& rawPos,
                           const glm::vec3& finalPos)
{
    const NetworkingConfigData& cfg = NetworkingConfig::instance().data();
    if (!gNetInterpDebug && !cfg.debug.logInterpolationState)
        return;

    const uint64_t now = nowMs();
    const double rateHz = std::max(0.1, cfg.debug.interpolationLogRateHz);
    const uint64_t intervalMs = (uint64_t)std::max(1.0, 1000.0 / rateHz);
    const bool alphaJump =
        cfg.debug.detectInterpolationJitter &&
        std::abs(interpolation.sampleAlphaDelta) >
            cfg.debug.maxAllowedAlphaJump;
    const bool forced = interpolation.renderSampleJumped || alphaJump ||
        interpolation.snappedOrCorrected;
    if (!forced && interpolation.lastInterpolationDebugLogMs != 0 &&
        now - interpolation.lastInterpolationDebugLogMs < intervalMs)
        return;

    interpolation.lastInterpolationDebugLogMs = now;

    double spanMs = 0.0;
    if (interpolation.buffer.size() >= 2)
    {
        spanMs = (double)(interpolation.buffer.back().serverTick -
                          interpolation.buffer.front().serverTick) *
                 (1000.0 / (double)GAMEPLAY_SIMULATION_HZ);
    }
    const double renderDtMs = interpolation.renderTickDelta /
        (double)GAMEPLAY_SIMULATION_HZ * 1000.0;
    const double delayTicks = interpolation.adaptiveDelaySeconds *
        (double)GAMEPLAY_SIMULATION_HZ;

    Debug::warn(Debug::Category::Networking,
        "[NETINTERP STATE] frame=%llu nowMs=%llu id=%u latest=%u older=%u newer=%u "
        "buf=%zu spanMs=%.1f delayMs=%.1f delayTicks=%.2f renderTick=%.3f "
        "renderDtMs=%.2f jumped=%d alpha=%.3f alphaDt=%.3f extrap=%d hold=%d "
        "snap=%d reanchors=%u reanchorReason=%s reanchorMs=%.1f raw=(%.2f,%.2f,%.2f) "
        "final=(%.2f,%.2f,%.2f) dFinal=%.3f dz=%.3f vz=%.2f hardSnaps=%u "
        "underruns=%u predDmg=%d hpCap=%d\n",
        (unsigned long long)gInterpolationDebugFrame,
        (unsigned long long)now,
        entityId,
        interpolation.target.serverTick,
        interpolation.sampleOlderTick,
        interpolation.sampleNewerTick,
        interpolation.buffer.size(),
        spanMs,
        interpolation.adaptiveDelaySeconds * 1000.0,
        delayTicks,
        interpolation.renderTick,
        renderDtMs,
        (int)interpolation.renderSampleJumped,
        interpolation.sampleAlpha,
        interpolation.sampleAlphaDelta,
        (int)interpolation.extrapolating,
        (int)interpolation.holding,
        (int)interpolation.snappedOrCorrected,
        (unsigned)gInterpolationReanchorCount,
        gInterpolationReanchorReason.empty()
            ? "none" : gInterpolationReanchorReason.c_str(),
        gInterpolationReanchorMagnitudeMs,
        rawPos.x, rawPos.y, rawPos.z,
        finalPos.x, finalPos.y, finalPos.z,
        interpolation.lastFinalRenderDelta,
        interpolation.lastFinalRenderVerticalDelta,
        interpolation.lastFinalRenderVerticalVelocity,
        (unsigned)interpolation.hardSnapCount,
        (unsigned)interpolation.bufferUnderrunCount,
        interpolation.pendingPredictedDamage,
        interpolation.predictedHealthCap);
}

} // anonymous namespace

bool pushInterpolationTarget(
    EntityInterpolationState& interpolation,
    const SnapshotEntity& entity,
    uint32_t serverTick)
{
    if (interpolation.hasTarget)
    {
        // Lifecycle freshness always enforced: a sample from an older spawn
        // generation or transform epoch is a previous life and must never feed
        // this life's interpolation history.
        if (!movementSnapshotLifecycleFresh(
                entity.spawnGeneration, entity.transformEpoch,
                interpolation.lastSpawnGeneration,
                interpolation.lastSnapshotTransformEpoch))
        {
            ++interpolation.staleSnapshotCount;
            return false;
        }
        // Tick ordering: a sample older than the newest seen is normally
        // rejected. With allow_out_of_order_insertion it is still accepted if
        // it lands inside the buffered tick window (it fills a hole left by a
        // reordered/dropped packet) — safe because the render clock is
        // monotonic and the buffer stays ascending by serverTick.
        const bool allowOutOfOrder = NetworkingConfig::instance()
            .data().snapshotBuffer.allowOutOfOrderInsertion;
        const bool bufferHasFront = !interpolation.buffer.empty();
        const bool fillsHole = allowOutOfOrder && bufferHasFront &&
            serverTick >= interpolation.buffer.front().serverTick;
        if (serverTick < interpolation.lastServerTick && !fillsHole)
        {
            ++interpolation.outOfOrderSnapshotCount;
            return false;
        }
    }

    SnapshotTransform next = transformFromEntity(entity);
    next.serverTick = serverTick;
    if (interpolation.hasTarget)
    {
        if (serverTick < interpolation.lastServerTick)
            ++interpolation.outOfOrderSnapshotCount;
        else if (serverTick > interpolation.lastServerTick &&
                 interpolation.lastSnapshotArrivalMs != 0)
        {
            const double expectedMs =
                (double)(serverTick - interpolation.lastServerTick) *
                (1000.0 / (double)GAMEPLAY_SIMULATION_HZ);
            const double actualMs =
                next.receivedMs >= interpolation.lastSnapshotArrivalMs
                    ? (double)(next.receivedMs - interpolation.lastSnapshotArrivalMs)
                    : 0.0;
            const double sample = std::abs(actualMs - expectedMs);
            const double smoothing = NetworkingConfig::instance()
                .data().adaptiveSnapshotBuffer.arrivalJitterSmoothing;
            interpolation.estimatedArrivalJitterMs +=
                (sample - interpolation.estimatedArrivalJitterMs) * smoothing;

            // Loss-fraction EMA: gap > 1 tick (a dropped snapshot) raises it,
            // a normal 1-tick gap decays it. Drives loss-based buffer growth.
            const auto& lossCfg = NetworkingConfig::instance()
                .data().adaptiveSnapshotBuffer;
            const double tickGap =
                (double)(serverTick - interpolation.lastServerTick);
            const double lossSample = lossCfg.lossGapTicks > 0.0
                ? std::min(1.0, (tickGap - 1.0) / (double)lossCfg.lossGapTicks)
                : 1.0;
            interpolation.recentLossFraction +=
                (lossSample - interpolation.recentLossFraction) *
                lossCfg.lossSmoothing;
        }
    }
    interpolation.lastSnapshotArrivalMs = next.receivedMs;
    const bool newLifecycle = interpolation.hasTarget &&
        ((entity.spawnGeneration != 0 &&
          entity.spawnGeneration != interpolation.lastSpawnGeneration) ||
         (entity.transformEpoch != 0 &&
          entity.transformEpoch != interpolation.lastTransformEpoch));

    // `interpolation.target` is the newest authoritative sample (used for
    // respawn snaps, thin-buffer seeding, and shooter re-base). An out-of-order
    // older sample may fill the interpolation buffer below, but it must never
    // regress the target. `previous` stays the target from one tick ago.
    const bool advancesTarget =
        !interpolation.hasTarget ||
        serverTick >= interpolation.target.serverTick;
    if (advancesTarget)
    {
        if (interpolation.hasTarget && !newLifecycle) {
            interpolation.previous = interpolation.target;
            interpolation.hasPrevious = true;
        } else {
            interpolation.previous = next;
            interpolation.hasPrevious = true;
        }
        interpolation.target = next;
        interpolation.hasTarget = true;
        interpolation.networkEntityId = entity.networkEntityId;
        interpolation.displayName = entity.displayName;
        if (serverTick > interpolation.lastServerTick)
            interpolation.lastServerTick = serverTick;
        interpolation.lastSpawnGeneration = entity.spawnGeneration;
        if (entity.transformEpoch != 0)
            interpolation.lastSnapshotTransformEpoch = entity.transformEpoch;
    }

    // ── Tick-ordered snapshot buffer (time-based interpolation) ─────
    const auto& interpCfg = NetworkingConfig::instance().data().remotePlayers;
    const auto& motionCfg =
        NetworkingConfig::instance().data().remoteMotionSmoothing;

    // Auto-size the buffer so it always spans the configured render delay.
    // linear_max_delay_ticks can far exceed maximum_buffered_snapshots (the
    // old fixed 64), which made the render time fall below the buffer's oldest
    // sample and step instead of glide at high delays. The buffer now holds at
    // least max(maximum_buffered_snapshots, linear_max_delay_ticks + margin).
    const std::size_t bufferCap = std::max<std::size_t>(
        interpCfg.maximumBufferedSnapshots,
        (std::size_t)motionCfg.linearMaxDelayTicks + 16);

    // Lifecycle change (respawn/teleport) resets the buffer so the render
    // snaps to the new life instead of smearing across it.
    if (newLifecycle && interpCfg.snapOnRespawn)
        interpolation.buffer.clear();

    // Same-tick duplicate: replace in place (keep latest state).
    for (auto it = interpolation.buffer.begin(); it != interpolation.buffer.end(); ++it)
    {
        if (it->serverTick == serverTick)
        {
            *it = next;
            ++interpolation.duplicateSnapshotCount;
            return true;
        }
    }

    // Out-of-order insertion keeps the buffer ascending by serverTick.
    auto insertIt = interpolation.buffer.begin();
    while (insertIt != interpolation.buffer.end() && insertIt->serverTick < serverTick)
        ++insertIt;
    interpolation.buffer.insert(insertIt, next);

    while (interpolation.buffer.size() > bufferCap)
        interpolation.buffer.pop_front();

    return true;
}

void mpApplyPredictedDamage(MultiplayerContext& ctx, uint32_t entityId,
                            int damage, bool npc)
{
    if (entityId == 0 || damage <= 0)
        return;
    auto& replicas = npc ? ctx.remoteNpcs : ctx.remotePlayers;
    auto& states = npc ? ctx.remoteNpcInterpolation : ctx.remotePlayerInterpolation;
    auto playerIt = replicas.find(entityId);
    auto stateIt = states.find(entityId);
    if (playerIt == replicas.end() || stateIt == states.end())
        return;

    EntityInterpolationState& interpolation = stateIt->second;
    Player& replica = playerIt->second;
    const int baseHealth = std::max(0, replica.currentHp);
    const int predictedHealth = std::max(0, baseHealth - damage);
    if (interpolation.predictedHealthCap < 0)
        interpolation.predictedHealthCap = predictedHealth;
    else
        interpolation.predictedHealthCap =
            std::min(interpolation.predictedHealthCap, predictedHealth);
    interpolation.pendingPredictedDamage =
        std::max(0, interpolation.pendingPredictedDamage + damage);
    interpolation.predictedHealthUpdatedMs = nowMs();
    replica.currentHp = interpolation.predictedHealthCap;

    Debug::log(Debug::Category::Networking,
        "[NET PREDICTED DAMAGE] entityId=%u npc=%d damage=%d displayHp=%d pending=%d",
        entityId, (int)npc, damage, replica.currentHp,
        interpolation.pendingPredictedDamage);
}

void mpConfirmPredictedDamage(MultiplayerContext& ctx, uint32_t entityId,
                              int healthAfter, bool killed, bool npc)
{
    if (entityId == 0)
        return;
    auto& replicas = npc ? ctx.remoteNpcs : ctx.remotePlayers;
    auto& states = npc ? ctx.remoteNpcInterpolation : ctx.remotePlayerInterpolation;
    auto playerIt = replicas.find(entityId);
    auto stateIt = states.find(entityId);
    if (playerIt == replicas.end() || stateIt == states.end())
        return;

    EntityInterpolationState& interpolation = stateIt->second;
    Player& replica = playerIt->second;
    healthAfter = std::max(0, healthAfter);

    const bool predictedDeathRollback = !killed && replica.netPredictedDead;
    if (predictedDeathRollback)
    {
        clearPredictedDeath(replica, replica.pos, entityId,
                            "PREDICTED KILL SERVER SURVIVED", true);
    }

    interpolation.pendingPredictedDamage = 0;
    interpolation.predictedHealthCap = healthAfter;

    interpolation.predictedHealthUpdatedMs = nowMs();
    ++interpolation.predictedHealthConfirmCount;
    replica.currentHp = killed ? 0 : healthAfter;

    Debug::log(Debug::Category::Networking,
        "[NET PREDICTED DAMAGE CONFIRM] entityId=%u npc=%d healthAfter=%d killed=%d cap=%d pending=%d",
        entityId, (int)npc, healthAfter, (int)killed,
        interpolation.predictedHealthCap,
        interpolation.pendingPredictedDamage);
}

void mpRollbackPredictedDamage(MultiplayerContext& ctx, uint32_t entityId,
                               bool npc, const char* reason)
{
    if (entityId == 0)
        return;
    auto& replicas = npc ? ctx.remoteNpcs : ctx.remotePlayers;
    auto& states = npc ? ctx.remoteNpcInterpolation : ctx.remotePlayerInterpolation;
    auto playerIt = replicas.find(entityId);
    auto stateIt = states.find(entityId);
    if (playerIt == replicas.end() || stateIt == states.end())
        return;

    EntityInterpolationState& interpolation = stateIt->second;
    Player& replica = playerIt->second;
    interpolation.pendingPredictedDamage = 0;
    interpolation.predictedHealthCap = -1;
    interpolation.predictedHealthUpdatedMs = 0;
    ++interpolation.predictedHealthRollbackCount;
    if (interpolation.hasTarget)
        replica.currentHp = interpolation.target.health;
    clearPredictedDeath(replica, replica.pos, entityId,
                        reason ? reason : "PREDICTED HIT ROLLBACK", false);
}

void mpApplyPredictedKillHeal(MultiplayerContext& ctx, uint32_t entityId, bool npc)
{
    // Only one pending predicted heal at a time; the first unresolved kill owns
    // it until it is confirmed or rolled back.
    if (!gpPlayer || entityId == 0 || ctx.predictedKillHealPending)
        return;
    ctx.predictedKillHealPending = true;
    ctx.predictedKillHealTargetEntityId = entityId;
    ctx.predictedKillHealTargetIsNpc = npc;
    ctx.predictedKillHealBeforeHp = gpPlayer->currentHp;
    DeathSystem::instance().healKillerToFull(*gpPlayer, gpPlayer->username);
    Debug::log(Debug::Category::Networking,
        "[NET PREDICTED KILL HEAL] target=%u npc=%d before=%d hp=%d\n",
        entityId, (int)npc, ctx.predictedKillHealBeforeHp, gpPlayer->currentHp);
}

void mpConfirmPredictedKillHeal(MultiplayerContext& ctx, uint32_t entityId)
{
    if (!ctx.predictedKillHealPending)
        return;
    if (entityId != ctx.predictedKillHealTargetEntityId)
        return;
    ctx.predictedKillHealPending = false;
    Debug::log(Debug::Category::Networking,
        "[NET PREDICTED KILL HEAL CONFIRMED] target=%u hp=%d\n",
        entityId, gpPlayer ? gpPlayer->currentHp : -1);
}

void mpRollbackPredictedKillHeal(MultiplayerContext& ctx, uint32_t entityId)
{
    if (!ctx.predictedKillHealPending)
        return;
    if (entityId != ctx.predictedKillHealTargetEntityId)
        return;
    ctx.predictedKillHealPending = false;
    if (gpPlayer)
    {
        gpPlayer->currentHp = ctx.predictedKillHealBeforeHp;
        Debug::log(Debug::Category::Networking,
            "[NET PREDICTED KILL HEAL ROLLBACK] target=%u hp=%d\n",
            entityId, gpPlayer->currentHp);
    }
}

static void resetPresentationAfterRespawn(Player& player, const SnapshotTransform& target)
{
    player.proceduralFrozen = false;
    player.dead = false;
    player.netPredictedDead = false;
    player.networkDeathPresented = false;
    player.deathAnim = Player::DeathAnimState{};
    player.currentHp = target.health;
    player.maxHp = target.health;

    // Walking/idle animation state
    player.footstepTimer = 0.0f;
    player.animStateTime = 0.0f;
    player.currentAnimName = "idle";
    player.proceduralTime = 0.0f;
    player.previousProceduralVelocity = glm::vec3(0.0f);
    player.previousMove01 = 0.0f;

    // Dash pose state
    player.dashPoseTimer = -1.0f;
    player.forceDashPose = false;
    player.dash.didDash = false;
    player.dash.didDownDash = false;

    // Freeze pose state
    player.freezePoseTimer = -1.0f;
    player.freezePoseActive = false;
    player.freeze.freezeActive = false;
    player.freeze.didFreeze = false;

    // Body-part and physical-body poses
    for (PhysicalBodyPart& part : player.physicalBody.parts) {
        part.pose = ProceduralPose{};
        part.perfectPose = ProceduralPose{};
        part.translationSpring = SpringState{};
        part.rotationSpring = SpringState{};
    }

    // Legacy layers and model world transforms
    player.syncLegacyStateToLayers();
    player.updateModelWorldTransforms();

    // Re-resolve equipped weapon from the replicated slot
    player.equippedWeaponId.clear();
    player.hasValidWeapon = false;
    if (target.equippedSlot >= 1) {
        auto reg = WeaponRegistry::instance().all();
        for (const auto& w : reg) {
            if (w.second.slot == target.equippedSlot) {
                player.equippedWeaponId = w.first;
                player.hasValidWeapon = true;
                break;
            }
        }
    }

    // Re-seed presentation serial baselines so the new life never replays
    // events that already occurred in the previous life.
    baselinePresentationSerials(player, target);

    printf("[NET PRESENTATION RESPAWN] entityId=%u epoch=%u pos=(%.2f,%.2f,%.2f)\n",
           target.networkEntityId, (unsigned)target.transformEpoch,
           target.position.x, target.position.y, target.position.z);
}

// Post-filter geometry safety clamp for rendered remote bodies. After all
// interpolation + motion filters have produced the final render position, push
// the body's capsule out of any world triangle it penetrates. Reuses the same
// capsule-vs-triangle solver the local player's collision uses, so local and
// remote bodies follow the same "never inside geometry" rule. Runs on the
// client only; the server already simulates NPCs against its own collision
// world, so authoritative targets are valid — this corrects the render side.
void resolveRemoteBodyAgainstGeometry(
    Player& player, EntityInterpolationState& interpolation, const World& world)
{
    if (world.collisionMesh.empty())
        return;
    const auto& motion = NetworkingConfig::instance().data().remoteMotionSmoothing;
    if (!motion.geometrySafeEnabled)
        return;

    const Capsule cap = player.getCapsule();
    std::vector<int> candidates;
    appendChunkTrianglesForAABB(world,
        makeSweptCapsuleAABB(cap, glm::vec3(0.0f)),
        0.25f, candidates, "remote-geometry-safety");
    if (candidates.empty())
        return;

    std::vector<RecoveryContact> contacts =
        collectCapsuleRecoveryContacts(world, cap, candidates, "remote-geometry-safety");
    if (contacts.empty())
        return;

    glm::vec3 correction = solveBatchedCorrection(
        contacts, (float)motion.geometrySafeSlopUnits, nullptr, nullptr,
        glm::vec3(0.0f), player.pos);
    if (!std::isfinite(correction.x) || !std::isfinite(correction.y) ||
        !std::isfinite(correction.z) ||
        glm::dot(correction, correction) < 1e-10f)
        return;

    player.pos += correction;
    // Keep filter state coherent so the spring/ease/linear state cannot re-pull
    // the body into geometry on the next frame.
    interpolation.renderSpring.value += correction;
    interpolation.lastRender.position += correction;

    Debug::logThrottled(Debug::Category::Networking,
        "remote-geometry-safety", 0.25f,
        "[REMOTE GEOMETRY SAFETY] id=%u corr=%.3f pos=(%.2f,%.2f,%.2f)\n",
        interpolation.networkEntityId, glm::length(correction),
        player.pos.x, player.pos.y, player.pos.z);
}

void updateRenderedReplica(
    Player& player,
    EntityInterpolationState& interpolation,
    double renderTick,
    float dt,
    bool spawnDeathEffects)
{
    if (!interpolation.hasTarget)
        return;

    player.dash.didDash = false;

    const auto& interpCfg = NetworkingConfig::instance().data().remotePlayers;
    const auto& motion = NetworkingConfig::instance().data().remoteMotionSmoothing;

    const uint32_t entityId = interpolation.networkEntityId;
    const uint16_t targetEpoch = interpolation.target.transformEpoch;
    const uint32_t targetSpawnGen = interpolation.target.spawnGeneration;

    // ── Detect respawn via lifecycle change ──────────────────────────
    // A new life is signalled by EITHER a transformEpoch change OR a
    // spawnGeneration change (the server bumps both on respawn). Detect on
    // both so a missed epoch (e.g. a dropped snapshot) still snaps the body to
    // the new spawn instead of leaving it frozen/invisible. Hard-snap
    // interpolation position and run the presentation reset. The very first
    // render of a brand-new entity (both baselines still 0) is not a respawn.
    bool respawned = false;
    const bool epochChanged =
        targetEpoch != 0 && targetEpoch != interpolation.lastTransformEpoch;
    const bool spawnGenChanged =
        targetSpawnGen != 0 && interpolation.lastSpawnGeneration != 0 &&
        targetSpawnGen != interpolation.lastSpawnGeneration;
    if (epochChanged || spawnGenChanged)
    {
        if (interpolation.lastTransformEpoch != 0 ||
            interpolation.lastSpawnGeneration != 0)
        {
            // Lifecycle changed while entity was alive → respawn
            const bool wasDead = interpolation.previous.health <= 0;
            const bool nowAlive = interpolation.target.health > 0;
            if (wasDead || nowAlive ||
                interpolation.lastTransformEpoch != 0 ||
                interpolation.lastSpawnGeneration != 0)
            {
                // Hard-snap position: no lerp from corpse to spawn
                resetPresentationAfterRespawn(player, interpolation.target);
                respawned = true;
                printf("[NET EPOCH RESPAWN] entityId=%u oldEpoch=%u newEpoch=%u "
                       "oldGen=%u newGen=%u wasDead=%d nowAlive=%d pos=(%.2f,%.2f,%.2f)\n",
                       entityId, (unsigned)interpolation.lastTransformEpoch,
                       (unsigned)targetEpoch,
                       (unsigned)interpolation.lastSpawnGeneration,
                       (unsigned)targetSpawnGen, (int)wasDead, (int)nowAlive,
                       interpolation.target.position.x,
                       interpolation.target.position.y,
                       interpolation.target.position.z);
            }
        }
        interpolation.lastTransformEpoch = targetEpoch;
        interpolation.lastSpawnGeneration = targetSpawnGen;
    }

    // ── Render state at one consistent view time ─────────────────────
    // One rule: position, velocity, yaw, aim, animation state, and weapon all
    // come from the SAME render-time snapshot, so the body can never desync
    // from its own animation or shots. The render snapshot is produced at
    // `estimatedServerNow - delay` on a continuous wall-clock clock, so the
    // interpolation alpha is always fractional (smooth at any frame rate; no
    // freeze/jump from a clamped catch-up clock).
    const NetworkingConfigData& netCfg = NetworkingConfig::instance().data();
    // "ease" is a linear-family mode (same lerp, delay, deadzone, glide gates)
    // but always extrapolates and never hole-freezes; it also runs a persistent
    // tight-inertia filter (see the ease branch below) instead of raw output.
    const bool isRawLinear = (motion.renderFilter == "linear");
    const bool isEase = (motion.renderFilter == "ease");
    const bool linearMode = isRawLinear || isEase;
    // linear mode uses an adaptive loss buffer whose depth grows smoothly with
    // jitter and loss (driven by the arrival-jitter / loss-fraction EMA from
    // pushInterpolationTarget), clamped to [linear_min_delay_ticks,
    // linear_max_delay_ticks] in ticks. A good connection stays at the base
    // linear_delay_ticks; a lossy/jittery one deepens so the render never runs
    // out of packets (no extrapolation/hold pops). Because the render time is
    // monotonic (see buildReceiveTimeRender), delay changes never slide the
    // body backward. linear_max_delay_ticks == 0 → fixed at linear_delay_ticks.
    const double delaySeconds = [&]() -> double {
        if (!linearMode)
            return adaptiveDelaySeconds(interpolation, netCfg, dt);
        double delayTicks = (double)motion.linearDelayTicks;
        if (motion.linearMaxDelayTicks > 0)
        {
            const double minT = (double)(motion.linearMinDelayTicks > 0
                ? motion.linearMinDelayTicks : motion.linearDelayTicks);
            const double maxT = (double)motion.linearMaxDelayTicks;
            const double jitterTicks =
                interpolation.estimatedArrivalJitterMs *
                (double)motion.linearDelayJitterMultiplier *
                netCfg.adaptiveSnapshotBuffer.jitterMultiplier / 1000.0 *
                (double)GAMEPLAY_SIMULATION_HZ;
            const double lossFrac = interpolation.recentLossFraction *
                (double)motion.linearDelayLossWeight;
            const double lossTicks = lossFrac * (maxT - minT);
            const double desiredTicks = (double)motion.linearDelayTicks +
                jitterTicks + lossTicks;
            delayTicks = std::clamp(desiredTicks, minT, maxT);
        }
        const double delay = delayTicks / (double)GAMEPLAY_SIMULATION_HZ;
        // Feed the effective delay back so the shot-rewind tick
        // (mpFireRenderTick) validates against the exact pose linear rendered.
        interpolation.adaptiveDelaySeconds = delay;
        return delay;
    }();

    SnapshotTransform render = interpolation.target;

    if (!interpCfg.directRender && interpCfg.enabled)
    {
        if (interpolation.buffer.size() >= 2)
        {
            // `renderTick` is the estimated current server tick (ticks). The
            // render time is that minus the delay; the interpolation alpha is
            // always fractional, so the body moves smoothly at any frame rate.
            // linear mode never extrapolates when holding on dry; other modes
            // honor interpCfg.allowExtrapolation. ease ALWAYS extrapolates so it
            // never freezes on a dry buffer.
            const bool allowExtrap = linearMode
                ? (isEase || (motion.linearAllowExtrapolation && !motion.linearHoldOnDry))
                : interpCfg.allowExtrapolation;
            buildReceiveTimeRender(
                interpolation, interpCfg, delaySeconds, renderTick,
                allowExtrap,
                linearMode ? motion.linearSnapAfterGapTicks : 0u,
                render);
        }
        else if (linearMode && interpolation.hasRendered)
        {
            // linear mode: too few snapshots for a segment — HOLD the last
            // rendered position (never snap to the newest packet directly).
            render = interpolation.lastRender;
        }
        else
        {
            // First render / fresh spawn (or a non-linear mode's thin buffer):
            // seed from the newest authoritative state. No freeze-hold of a
            // stale render: the wall-clock render clock recovers on its own as
            // snapshots accumulate.
            render = interpolation.target;
        }
    }

    glm::vec3 renderPos = render.position;
    const float renderYaw = render.yaw;
    const glm::vec3 renderAim = render.aimDirection;
    interpolation.lastRawInterpolatedPosition = render.position;

    // ── Post-interpolation motion filter ─────────────────────────────
    // "direct":  set the position directly (no smoothing).
    // "bounded": move toward the interpolated target at a capped speed so a
    //            discontinuity (loss hole, extrapolation resume, blackout)
    //            converges over a few frames instead of snapping. Normal smooth
    //            motion and dashes pass through (target tracks the body).
    // "spring":  always-on critically-damped spring — literally cannot snap;
    //            adds a few ms of follow-lag on fast turns.
    // "ease":    persistent tight-inertia integrator (see the ease branch):
    //            direct-feeling velocity-following with a gentle capped pull to
    //            the exact target, so it never freezes and never snaps.
    if (interpolation.hasRendered && !respawned && !interpCfg.directRender &&
        !isRawLinear)
    {
        const float safeDt = std::min(dt, 0.05f);

        // Seed the persistent filter state on the first filtered frame (fresh
        // entity or after a respawn reset) so spring/hybrid/ease start from the
        // first real rendered position instead of the world origin.
        if (!interpolation.renderFilterSeeded)
        {
            interpolation.renderSpring.value = renderPos;
            interpolation.renderSpring.velocity = render.velocity;
            interpolation.renderSpringTargetVel = render.velocity;
            interpolation.renderFilterSeeded = true;
        }

        if (motion.renderFilter == "spring")
        {
            // Spring mode is fully tunable:
            //  - spring_frequency_hz > 0  -> k=(2πf)², c=2ω·damping_ratio
            //    (spring_frequency_hz == 0 -> legacy spring_stiffness/damping)
            //  - spring_feed_forward      -> velocity matches the interpolated
            //    velocity (1 = crisp linear-look, 0 = classic laggy spring)
            //  - spring_linear_deadzone   -> render exactly at the linear target
            //    once the spring has converged, so it LOOKS like linear mode
            //    while real discontinuities are still glided (no snap).
            // The velocity update is implicit (unconditionally stable, no ring).
            const float zMult =
                std::max(0.5f, (float)motion.springFrequencyZMultiplier);
            float k, c;
            if (motion.springFrequencyHz > 0.0f)
            {
                const float omega = 2.0f * 3.14159265f * (float)motion.springFrequencyHz;
                k = omega * omega;
                c = 2.0f * omega * (float)motion.springDampingRatio;
            }
            else
            {
                k = (float)motion.springStiffness;
                c = (float)motion.springDamping;
            }
            const float kZ = k * zMult * zMult;
            const float cZ = c * zMult;
            const float denom = 1.0f + c * safeDt;
            const float denomZ = 1.0f + cZ * safeDt;

            const glm::vec3 targetPos = renderPos;

            // Low-pass the feed-forward velocity (anti-jitter).
            const float ffSmooth = std::clamp(
                (float)motion.springFeedForwardSmoothing, 0.0f, 1.0f);
            const glm::vec3 rawTargetVel = render.velocity;
            if (ffSmooth > 0.0f)
                interpolation.renderSpringTargetVel +=
                    (rawTargetVel - interpolation.renderSpringTargetVel) * ffSmooth;
            else
                interpolation.renderSpringTargetVel = rawTargetVel;
            const glm::vec3 targetVel =
                interpolation.renderSpringTargetVel * (float)motion.springFeedForward;

            glm::vec3 vel = interpolation.renderSpring.velocity;
            const glm::vec3 err = targetPos - interpolation.renderSpring.value;
            vel.x = (vel.x + (err.x * k + targetVel.x * c) * safeDt) / denom;
            vel.y = (vel.y + (err.y * k + targetVel.y * c) * safeDt) / denom;
            vel.z = (vel.z + (err.z * kZ + targetVel.z * cZ) * safeDt) / denomZ;
            if (motion.springMaxSpeedUnitsPerSecond > 0.0f)
            {
                const float maxSpd = (float)motion.springMaxSpeedUnitsPerSecond;
                const float velLen = glm::length(vel);
                if (velLen > maxSpd)
                    vel = vel * (maxSpd / velLen);
            }
            interpolation.renderSpring.velocity = vel;
            interpolation.renderSpring.value += vel * safeDt;

            // Deadzone: once the spring has converged to within the deadzone of
            // the linear target, render exactly the linear target (pixel-identical
            // to linear mode). Past the deadzone, render the spring value so a real
            // discontinuity is glided instead of snapped.
            const float springErr = glm::length(
                targetPos - interpolation.renderSpring.value);
            if (motion.springLinearDeadzoneUnits > 0.0f)
            {
                const float deadzone = (float)motion.springLinearDeadzoneUnits;
                const float blend = std::clamp(
                    1.0f - springErr / deadzone, 0.0f, 1.0f);
                renderPos = glm::mix(
                    interpolation.renderSpring.value, targetPos, blend);
            }
            else
            {
                renderPos = interpolation.renderSpring.value;
            }
        }
        else if (motion.renderFilter == "hybrid")
        {
            // Velocity-feed-forward spring with an implicit integrator. Feeding
            // the interpolated velocity forward cancels the spring's steady-state
            // follow-lag (lag = V·damping/stiffness), so fast / up-down motion
            // tracks crisply while discontinuities converge with zero snap. The
            // implicit velocity update removes discrete-time ringing.
            const float omega = 2.0f * 3.14159265f * (float)motion.hybridFrequencyHz;
            const float k = omega * omega;
            const float c = 2.0f * omega * (float)motion.hybridDampingRatio;
            const float zMult = std::max(0.5f, (float)motion.hybridFrequencyZMultiplier);
            const float omegaZ = omega * zMult;
            const float kZ = omegaZ * omegaZ;
            const float cZ = 2.0f * omegaZ * (float)motion.hybridDampingRatio;
            const float denom = 1.0f + c * safeDt;
            const float denomZ = 1.0f + cZ * safeDt;

            // Low-pass the feed-forward velocity so snapshot-boundary slope
            // changes are not amplified into jitter by the stiff spring.
            const float ffSmooth = std::clamp(
                (float)motion.hybridFeedForwardSmoothing, 0.0f, 1.0f);
            const glm::vec3 rawTargetVel = render.velocity;
            if (ffSmooth > 0.0f)
                interpolation.renderSpringTargetVel +=
                    (rawTargetVel - interpolation.renderSpringTargetVel) * ffSmooth;
            else
                interpolation.renderSpringTargetVel = rawTargetVel;
            const glm::vec3 targetVel =
                interpolation.renderSpringTargetVel *
                (float)motion.hybridFeedForward;

            glm::vec3 vel = interpolation.renderSpring.velocity;
            const glm::vec3 err = renderPos - interpolation.renderSpring.value;
            vel.x = (vel.x + (err.x * k + targetVel.x * c) * safeDt) / denom;
            vel.y = (vel.y + (err.y * k + targetVel.y * c) * safeDt) / denom;
            vel.z = (vel.z + (err.z * kZ + targetVel.z * cZ) * safeDt) / denomZ;
            if (motion.hybridMaxSpeedUnitsPerSecond > 0.0f)
            {
                const float maxSpd = (float)motion.hybridMaxSpeedUnitsPerSecond;
                const float velLen = glm::length(vel);
                if (velLen > maxSpd)
                    vel = vel * (maxSpd / velLen);
            }
            interpolation.renderSpring.velocity = vel;
            interpolation.renderSpring.value += vel * safeDt;
            renderPos = interpolation.renderSpring.value;
        }
        else if (motion.renderFilter == "ease")
        {
            // Tight-inertia integrator: a persistent, low-passed velocity state
            // (inertia) drives position, and a gentle capped pull pins it to the
            // exact interpolated target. Because the velocity state persists and
            // the pull is capped, the body can never stop abruptly or snap, while
            // the velocity feed-forward keeps it reading direct (not floaty).
            const glm::vec3 targetPos = renderPos;
            const glm::vec3 targetVel = render.velocity;
            const float smooth = std::clamp(
                (float)motion.easeVelocitySmoothing, 0.0f, 1.0f);
            interpolation.renderSpringTargetVel +=
                (targetVel - interpolation.renderSpringTargetVel) * smooth;
            interpolation.renderSpring.value +=
                interpolation.renderSpringTargetVel * safeDt;
            const glm::vec3 err = targetPos - interpolation.renderSpring.value;
            const float rate = std::min(1.0f, (float)motion.easeCorrectionRate * safeDt);
            interpolation.renderSpring.value += err * rate;
            renderPos = interpolation.renderSpring.value;
        }
        else if (motion.renderFilter == "bounded")
        {
            const float maxStep = (float)(
                motion.correctionMaxStepUnitsPerSecond * (double)dt);
            const glm::vec3 delta = renderPos - player.pos;
            const float len = glm::length(delta);
            if (len > (float)motion.correctionMinDeltaUnits && len > maxStep)
                renderPos = player.pos + delta * (maxStep / len);
        }

        // Universal Z floor guard: never render below the interpolated
        // authoritative body, so filter overshoot after a fast landing cannot
        // push the body through the floor. Does not block legitimate falls.
        if (motion.filterClampZBelowTarget)
            renderPos.z = std::max(renderPos.z, render.position.z);

        // Universal final hard cap: the absolute "never teleport" guarantee.
        if (motion.filterMaxStepUnitsPerSecond > 0.0f)
        {
            const float maxStep = (float)(
                motion.filterMaxStepUnitsPerSecond * (double)dt);
            const glm::vec3 delta = renderPos - player.pos;
            const float len = glm::length(delta);
            if (len > maxStep)
                renderPos = player.pos + delta * (maxStep / len);
        }
    }
    interpolation.wasExtrapolating = interpolation.extrapolating;

    if (respawned)
    {
        player.pos = interpolation.target.position;
        // Hard-snap the remembered render state too, so a thin buffer on the
        // next frame holds the respawn position rather than the old corpse.
        interpolation.lastRender = interpolation.target;
        // The new life must not inherit the old corpse's spring velocity.
        interpolation.renderSpring = SpringState{};
        interpolation.renderSpringTargetVel = glm::vec3(0.0f);
        interpolation.renderFilterSeeded = false;
        interpolation.pendingPredictedDamage = 0;
        interpolation.predictedHealthCap = -1;
        interpolation.predictedHealthUpdatedMs = 0;
    }
    else
    {
        // linear mode deadzone: sub-threshold rendered deltas snap to the exact
        // previous rendered position so standing-still bodies never micro-jitter
        // from tiny broadcast noise. Real movement above the threshold passes.
        if ((motion.renderFilter == "linear" || isEase) &&
            motion.linearDeadzoneUnits > 0.0)
        {
            const float delta = glm::length(renderPos - player.pos);
            if (delta < (float)motion.linearDeadzoneUnits)
                renderPos = player.pos;
        }
        // linear/ease anti-snap glide gate: the rendered body may only move
        // toward the interpolated target at linear_glide_max_units_per_second
        // (units = meters). This is the single write point for every non-respawn
        // remote position, so NO code path (hole bridge, blackout resume, first
        // seed, unforeseen future path) can ever teleport the body — it always
        // glides from the last known position to the newest confirmed position.
        // Normal motion (walk ~20, dash ~100, terminal fall ~400 u/s) is below
        // the cap and passes untouched. Skipped on the first render (hasRendered
        // false) so a new entity seeds directly at its authoritative position.
        if ((motion.renderFilter == "linear" || isEase) &&
            interpolation.hasRendered &&
            motion.linearGlideMaxUnitsPerSecond > 0.0f)
        {
            const float maxStep = (float)(
                motion.linearGlideMaxUnitsPerSecond * (double)dt);
            const glm::vec3 delta = renderPos - player.pos;
            const float len = glm::length(delta);
            if (len > maxStep && len > 0.0f)
            {
                renderPos = player.pos + delta * (maxStep / len);
                ++interpolation.glideSnapCount;
            }
        }
        player.pos = renderPos;
    }

    // Final render-safety clamp: the rendered remote body may never intersect
    // world geometry, regardless of filter mode, loss pattern, or extrapolation
    // drift. Runs after every path that writes player.pos above.
    if (gpWorld)
        resolveRemoteBodyAgainstGeometry(player, interpolation, *gpWorld);

    player.vel = render.velocity;
    const int displayHealth =
        applyPredictedHealthOverlay(player, interpolation, render.health);
    player.currentHp = displayHealth;
    // Server max HP (healthall override) isn't transmitted; derive it from the
    // highest server health seen so nameplates/NPC bars show 999/999 not 999/100.
    if (displayHealth > player.maxHp)
        player.maxHp = displayHealth;
    player.dead = displayHealth <= 0 || player.netPredictedDead;

    // ── Remote death lifecycle (players + NPCs) ───────────────────────
    // Respawn (dead → alive): fully recover the body into its new life.
    // The epoch-change snap above is the primary respawn path; this is the
    // belt-and-suspenders fallback for when the render health transitions
    // 0 → positive (the epoch snap was missed, or a delayed kill confirmation
    // for the previous life landed after the respawn). Without clearing EVERY
    // piece of death state here, a respawned body stays `player.dead == true`
    // — invisible but still hittable at its spawn point — until the
    // predicted-health timeout clears. No deathAnim requirement: the body may
    // have already finished its fall-over before the respawn rendered.
    if (interpolation.hasRendered &&
        interpolation.lastRender.health <= 0 && render.health > 0)
    {
        player.deathAnim = Player::DeathAnimState{};
        player.netPredictedDead = false;
        player.dead = false;
        player.currentHp = render.health;
        interpolation.pendingPredictedDamage = 0;
        interpolation.predictedHealthCap = -1;
        Debug::warn(Debug::Category::Networking,
            "[NET REMOTE RESPAWN RECOVER] entityId=%u lastHp=%d renderHp=%d "
            "netPredictedDead=%d — full death state cleared\n",
            entityId, interpolation.lastRender.health, render.health,
            (int)player.netPredictedDead);
    }

    // ── Death effect (snapshot-driven, loss-proof) ───────────────────
    // Detect the >0 → <=0 health transition in the render stream and spawn a
    // SEPARATE fall-over ghost so the remote body itself is never pinned or
    // frozen. If a reliable kill event already presented this death
    // (networkDeathPresented), skip so there is never a second death.
    // `!respawned` guards the respawn-frame stale-render mismatch (the buffer
    // was just cleared, so render still shows the old corpse while lastRender
    // already holds the new life) — that mismatch would otherwise fire a false
    // death at the new life's position.
    if (spawnDeathEffects && interpolation.hasRendered && !respawned &&
        NetworkingConfig::instance().data().deathEffects.remotePlayerDeathEffect &&
        !player.networkDeathPresented)
    {
        const bool wasAlive = interpolation.lastRender.health > 0;
        const bool nowDead = render.health <= 0;
        if (wasAlive && nowDead)
        {
            // This snapshot transition presented the death — a later reliable
            // kill event must not play a second one.
            player.networkDeathPresented = true;
            // Elongate toward the pre-death planar velocity (the death frame
            // itself carries zero velocity because dead players stop).
            const glm::vec3& deathVel = interpolation.lastRender.velocity;
            const glm::vec3 deathDir =
                glm::length(deathVel) > 0.001f
                    ? glm::normalize(glm::vec3(deathVel.x, deathVel.y, 0.0f))
                    : glm::vec3(0.0f, 0.0f, 1.0f);
            DeathGhostSystem::instance().spawnFromPlayer(
                player, deathDir,
                "net_" + std::to_string(entityId), entityId);
            Debug::log(Debug::Category::Networking,
                "[NET REMOTE DEATH FX] entityId=%u pos=(%.1f,%.1f,%.1f)",
                entityId, player.pos.x, player.pos.y, player.pos.z);
        }
    }

    // Remember what was actually rendered so a thin buffer holds it exactly.
    if (!respawned)
        interpolation.lastRender = render;
    interpolation.hasRendered = true;
    interpolation.lastRenderedServerTick = render.serverTick;
    if (interpolation.hasFinalRenderPosition)
    {
        const glm::vec3 delta = player.pos - interpolation.lastFinalRenderPosition;
        interpolation.lastFinalRenderDelta = glm::length(delta);
        interpolation.lastFinalRenderVerticalDelta = delta.z;
        interpolation.lastFinalRenderVerticalVelocity =
            dt > 0.0f ? delta.z / dt : 0.0f;
    }
    else
    {
        interpolation.lastFinalRenderDelta = 0.0f;
        interpolation.lastFinalRenderVerticalDelta = 0.0f;
        interpolation.lastFinalRenderVerticalVelocity = 0.0f;
        interpolation.hasFinalRenderPosition = true;
    }
    interpolation.lastFinalRenderPosition = player.pos;
    logInterpolationState(interpolation, entityId,
                          interpolation.lastRawInterpolatedPosition,
                          player.pos);

    // Body-facing yaw and aim use the interpolated render values.
    {
        player.yaw = renderYaw;
        if (std::isfinite(renderAim.x) && std::isfinite(renderAim.y) && std::isfinite(renderAim.z) &&
            glm::dot(renderAim, renderAim) > 0.000001f)
        {
            player.aimDirection = glm::normalize(renderAim);
            player.hasAimData = true;
        }
        else
        {
            player.hasAimData = false;
        }
    }

    player.ground.onGround = render.onGround;
    player.equippedSlot = render.equippedSlot;
    {
        player.equippedWeaponId.clear();
        player.hasValidWeapon = false;
        if (player.equippedSlot >= 1) {
            auto reg = WeaponRegistry::instance().all();
            for (const auto& w : reg) {
                if (w.second.slot == player.equippedSlot) {
                    player.equippedWeaponId = w.first;
                    player.hasValidWeapon = true;
                    break;
                }
            }
        }
    }

    // ── Ensure remote player has a WeaponRuntime for animation ──────
    if (player.hasValidWeapon && !player.equippedWeaponId.empty()) {
        auto rtIt = player.weaponRuntimes.find(player.equippedWeaponId);
        if (rtIt == player.weaponRuntimes.end()) {
            const WeaponDefinition* wdef = WeaponRegistry::instance().get(player.equippedWeaponId);
            if (wdef) {
                WeaponRuntime& rt = player.weaponRuntimes[player.equippedWeaponId];
                rt = WeaponRuntime{};
                WeaponRuntimeHelper::initRuntime(rt, *wdef);
            }
        }
    }

    player.networkShootEffectTimer =
        std::max(0.0f, player.networkShootEffectTimer - dt);
    player.networkWeaponState = render.weaponState;
    if (player.networkShootEffectTimer > 0.0f)
        player.networkWeaponState |= 1u;
    player.sizeScale = render.sizeScale;
    player.spawnGeneration = render.spawnGeneration;
    player.username = interpolation.displayName;
    player.vipAppearance = render.vipAppearance;
    player.networkStateFlags = render.stateFlags;
    player.ground.onGround = render.onGround;

    // ── Freeze state ─────────────────────────────────────────────────
    // Set freezeActive for the freeze pose state machine in
    // updateProceduralAnimation. Do NOT set proceduralFrozen — that
    // field causes updateProceduralAnimation to return early and skip
    // the freeze pose code entirely. proceduralFrozen is reserved for
    // pause/replay/cinematic use.
    player.freeze.freezeActive =
        (render.stateFlags & NET_STATE_FREEZING) != 0;

    // ── Event serial changes → one-shot VFX ─────────────────────────
    // entityId is already defined above from interpolation.networkEntityId

    // Dash
    bool dashTriggered = false;
    if (render.dashSerial != 0 &&
        render.dashSerial != player.networkLastDashSerial)
    {
        player.networkLastDashSerial = render.dashSerial;
        player.dash.didDash = true;
        dashTriggered = true;
        glm::vec3 dashDir = glm::length(player.vel) > 0.001f
            ? glm::normalize(player.vel)
            : glm::vec3(0.0f, 1.0f, 0.0f);
        HitEffects::spawnMovementDashBurst(player.pos, dashDir, glm::length(player.vel));
        playWorldSound("entity/player/dash", player.pos, 1.0f, 1.0f, 36.0f);
        printf("[NET PRESENTATION RX] entityId=%u event=dash serial=%u triggered=1\n",
               entityId, (unsigned)render.dashSerial);
    }

    // Ground jump
    if (render.groundJumpSerial != 0 &&
        render.groundJumpSerial != player.networkLastGroundJumpSerial)
    {
        player.networkLastGroundJumpSerial = render.groundJumpSerial;
        glm::vec3 jumpDir = glm::vec3(0.0f);
        glm::vec3 jumpPos = player.pos;
        jumpPos.z -= 0.5f;
        HitEffects::spawnGroundJumpBurst(jumpPos, jumpDir);
        playWorldSound("entity/player/jump", player.pos, 1.0f, 1.0f, 28.0f);
        printf("[NET PRESENTATION RX] entityId=%u event=groundJump serial=%u triggered=1\n",
               entityId, (unsigned)render.groundJumpSerial);
    }

    // Air jump
    if (render.airJumpSerial != 0 &&
        render.airJumpSerial != player.networkLastAirJumpSerial)
    {
        player.networkLastAirJumpSerial = render.airJumpSerial;
        glm::vec3 jumpDir = glm::vec3(0.0f);
        glm::vec3 jumpPos = player.pos;
        jumpPos.z -= 1.0f;
        HitEffects::spawnAirJumpBurst(jumpPos, jumpDir);
        playWorldSound("entity/player/jump", player.pos, 1.0f, 1.0f, 28.0f);
        printf("[NET PRESENTATION RX] entityId=%u event=airJump serial=%u triggered=1\n",
               entityId, (unsigned)render.airJumpSerial);
    }

    // Down dash
    if (render.downDashSerial != 0 &&
        render.downDashSerial != player.networkLastDownDashSerial)
    {
        player.networkLastDownDashSerial = render.downDashSerial;
        glm::vec3 downDashPos = player.pos;
        downDashPos.z -= 0.3f;
        EffectPartSystem::instance().spawnDownDash(downDashPos);
        printf("[NET PRESENTATION RX] entityId=%u event=downDash serial=%u triggered=1\n",
               entityId, (unsigned)render.downDashSerial);
    }

    // Freeze one-shot (didFreeze)
    if (render.freezeSerial != 0 &&
        render.freezeSerial != player.networkLastFreezeSerial)
    {
        player.networkLastFreezeSerial = render.freezeSerial;
        glm::vec3 freezePos = player.pos;
        freezePos.z -= 0.3f;
        EffectPartSystem::instance().spawnFreeze(freezePos, 2.0f);
        playWorldSound("entity/player/freezebegin", player.pos, 1.0f, 1.0f, 30.0f);
        printf("[NET PRESENTATION RX] entityId=%u event=freeze serial=%u triggered=1\n",
               entityId, (unsigned)render.freezeSerial);
    }

    // Freeze trail (sustained while freezeActive)
    if (player.freeze.freezeActive)
    {
        EffectPartSystem::instance().spawnFreezeTrail(player.pos);
    }

    // Direction change one-shot directional walk burst
    if (render.directionChangeSerial != 0 &&
        render.directionChangeSerial != player.networkLastDirectionChangeSerial)
    {
        player.networkLastDirectionChangeSerial = render.directionChangeSerial;
        glm::vec3 dirWalkVel = glm::length(player.vel) > 0.001f
            ? glm::normalize(glm::vec3(player.vel.x, player.vel.y, 0.0f))
            : glm::vec3(0.0f, 0.0f, 0.0f);
        float speed = glm::length(glm::vec2(player.vel.x, player.vel.y));
        HitEffects::spawnWalkBurst(player.pos, -dirWalkVel, speed);
        printf("[NET PRESENTATION RX] entityId=%u event=directionChange serial=%u triggered=1\n",
               entityId, (unsigned)render.directionChangeSerial);
    }

    // ── Walking VFX (sustained) ─────────────────────────────────────
    // Walking animation driven by client stateFlags, not server grounded state,
    // so it works immediately after respawn and during brief airtime.
    const bool remoteWalking =
        (render.stateFlags & NET_STATE_WALKING) != 0;
    if (remoteWalking)
    {
        glm::vec2 planarVel(player.vel.x, player.vel.y);
        float speed = glm::length(planarVel);
        if (speed > 0.5f)
        {
            player.footstepTimer -= dt;
            if (player.footstepTimer <= 0.0f)
            {
                glm::vec3 walkDir = glm::length(player.vel) > 0.001f
                    ? glm::normalize(glm::vec3(player.vel.x, player.vel.y, 0.0f))
                    : glm::vec3(0.0f, 0.0f, 0.0f);
                // Reconstruct same footstep VFX as local Player::updateAudio()
                Capsule cap = player.getCapsule();
                glm::vec3 footPos = cap.a;
                footPos.z -= cap.r;
                EffectPartSystem::instance().spawnFootstep(footPos, player.sizeScale);
                HitEffects::spawnWalkBurst(player.pos, -walkDir, speed);
                player.footstepTimer = 0.35f;
            }
        }
        else
        {
            player.footstepTimer = 0.0f;
        }
    }
    else
    {
        player.footstepTimer = 0.0f;
    }

    // ── Procedural animation ───────────────────────────────────────
    player.updateProceduralAnimation(
        dt,
        player.aimDirection,
        player.pos,
        remoteWalking
    );
}

glm::vec3 mpRemoteShooterRenderDelta(const MultiplayerContext& ctx, uint32_t shooterId)
{
    if (shooterId == 0 || shooterId == ctx.localPlayerId)
        return glm::vec3(0.0f);

    // Rendered replica body vs. newest authoritative snapshot position.
    // Re-basing muzzle/tracer visuals by this delta puts the shot exactly on
    // the body the client sees instead of the lagged/ghost authoritative spot.
    auto findDelta = [](const Player& replica,
                        const EntityInterpolationState* interp) -> glm::vec3 {
        if (!interp || !interp->hasTarget)
            return glm::vec3(0.0f);
        return replica.pos - interp->target.position;
    };

    auto playerIt = ctx.remotePlayers.find(shooterId);
    if (playerIt != ctx.remotePlayers.end())
    {
        auto interpIt = ctx.remotePlayerInterpolation.find(shooterId);
        return findDelta(playerIt->second,
                         interpIt != ctx.remotePlayerInterpolation.end()
                             ? &interpIt->second : nullptr);
    }

    auto npcIt = ctx.remoteNpcs.find(shooterId);
    if (npcIt != ctx.remoteNpcs.end())
    {
        auto interpIt = ctx.remoteNpcInterpolation.find(shooterId);
        return findDelta(npcIt->second,
                         interpIt != ctx.remoteNpcInterpolation.end()
                             ? &interpIt->second : nullptr);
    }

    return glm::vec3(0.0f);
}

glm::vec3 mpRemoteShooterMuzzle(const MultiplayerContext& ctx, uint32_t shooterId,
                                const glm::vec3& fallbackMuzzle)
{
    if (shooterId == 0 || shooterId == ctx.localPlayerId)
        return fallbackMuzzle;
    const Player* replica = nullptr;
    {
        auto it = ctx.remotePlayers.find(shooterId);
        if (it != ctx.remotePlayers.end())
            replica = &it->second;
    }
    if (!replica)
    {
        auto it = ctx.remoteNpcs.find(shooterId);
        if (it != ctx.remoteNpcs.end())
            replica = &it->second;
    }
    if (!replica)
        return fallbackMuzzle;
    // Weapon muzzle from the rendered body: forward along the rendered aim
    // (fallback to yaw), slightly up — the gun on the body the viewer sees.
    glm::vec3 fwd = glm::length(replica->aimDirection) > 0.001f
        ? glm::normalize(replica->aimDirection)
        : glm::vec3(std::cos(replica->yaw), std::sin(replica->yaw), 0.0f);
    return replica->pos + fwd * 0.9f + glm::vec3(0.0f, 0.0f, 1.15f);
}

void mpUpdateRemoteEntities(MultiplayerContext& ctx, float dt)
{
    // Free-running render clock in server-tick units, advanced by real frame
    // time. Interpolation runs on a smooth monotonic time base so the alpha is
    // always fractional at any frame rate (no packet-arrival accumulator, no
    // freeze/jump, no backward slides).
    // MONOTONIC + FREE-RUNNING: the clock only ever moves forward, and it
    // advances every frame regardless of when snapshots arrive. A late arrival
    // cannot freeze it (a frozen clock made the render time hold-then-lurch,
    // which showed up as vertical jitter on every filter) and cannot step it
    // backward (the original bug that made alpha oscillate = "double set").
    // It is re-anchored upward only if the newest data ever overtakes it.
    const auto& motionClock =
        NetworkingConfig::instance().data().remoteMotionSmoothing;
    const bool linearClock = (motionClock.renderFilter == "linear");
    const double linearCatchupTps =
        motionClock.linearCatchupRateTicksPerSecond;
    ++ctx.interpolationFrameNumber;

    // If the server regressed its tick domain (map change / server restart),
    // the old clock is invalid — reset it so monotonicity can't pin it high.
    if (ctx.lastClockAnchorServerTick != 0 &&
        ctx.latestServerTick < ctx.lastClockAnchorServerTick)
    {
        ctx.interpolationRenderTick = 0.0;
        ctx.interpolationClockStarted = false;
        ctx.interpolationClockLastUpdateMs = 0;
        ctx.lastInterpolationReanchorReason = "server-tick-regressed";
    }
    ctx.lastClockAnchorServerTick = ctx.latestServerTick;

    if (ctx.latestServerTick != 0 && ctx.lastSnapshotReceivedMs != 0)
    {
        const uint64_t nowClock = nowMs();
        if (!ctx.interpolationClockStarted)
        {
            ctx.interpolationRenderTick = (double)ctx.latestServerTick;
            ctx.interpolationClockStarted = true;
            ctx.interpolationClockLastUpdateMs = nowClock;
            ctx.lastInterpolationClockStepMs = 0.0;
        }
        else
        {
            // Advance the render clock by REAL wall-clock elapsed time for EVERY
            // render filter (not just linear). The render clock is the client's
            // estimate of the current server tick — it MUST track the server's
            // real 60 Hz tick, independent of frame rate / frame-dt semantics.
            // Advancing it by the frame `dt` let it run slower than the server
            // at low FPS (2 clients on one machine), so the fire tick drifted
            // hundreds of ticks into the past and every hit rewind missed.
            double elapsedMs = 0.0;
            elapsedMs = ctx.interpolationClockLastUpdateMs != 0 &&
                        nowClock >= ctx.interpolationClockLastUpdateMs
                ? (double)(nowClock - ctx.interpolationClockLastUpdateMs)
                : 0.0;
            ctx.interpolationClockLastUpdateMs = nowClock;
            const double dtTicks =
                elapsedMs / 1000.0 * (double)GAMEPLAY_SIMULATION_HZ;
            double clockStepTicks = dtTicks;
            if (linearClock && linearCatchupTps > 0.0)
                clockStepTicks += linearCatchupTps * (elapsedMs / 1000.0);
            ctx.interpolationRenderTick += clockStepTicks;
            ctx.lastInterpolationClockStepMs =
                clockStepTicks / (double)GAMEPLAY_SIMULATION_HZ * 1000.0;

            // The render clock can NEVER be behind the newest received server
            // tick (the client has that data). If it fell behind (drift), pull
            // it up so the fire tick stays in the server's tick domain and the
            // rewind lands on the pose the shooter actually saw.
            if (ctx.latestServerTick != 0 &&
                ctx.interpolationRenderTick < (double)ctx.latestServerTick)
            {
                const double before = ctx.interpolationRenderTick;
                ctx.interpolationRenderTick = (double)ctx.latestServerTick;
                ++ctx.interpolationReanchorCount;
                ctx.lastInterpolationReanchorMagnitudeMs =
                    (ctx.interpolationRenderTick - before) /
                    (double)GAMEPLAY_SIMULATION_HZ * 1000.0;
                ctx.lastInterpolationReanchorReason = "clock-fell-behind-newest";
                Debug::warn(Debug::Category::Networking,
                    "[NETINTERP CLOCK LIFT] latest=%u before=%.2f after=%.2f "
                    "magnitudeMs=%.1f\n",
                    ctx.latestServerTick, before, ctx.interpolationRenderTick,
                    ctx.lastInterpolationReanchorMagnitudeMs);
            }

            // Rare safety re-anchor only. Snapshot bursts must fill the buffer;
            // they must not drag the render clock forward every time the newest
            // received server tick jumps.
            const double errorTicks =
                (double)ctx.latestServerTick - ctx.interpolationRenderTick;
            const double reanchorTicks =
                motionClock.linearReanchorOnlyIfErrorSeconds *
                (double)GAMEPLAY_SIMULATION_HZ;
            if (motionClock.linearReanchorEnabled &&
                errorTicks > reanchorTicks)
            {
                const double before = ctx.interpolationRenderTick;
                ctx.interpolationRenderTick = (double)ctx.latestServerTick;
                ++ctx.interpolationReanchorCount;
                ctx.lastInterpolationReanchorMagnitudeMs =
                    (ctx.interpolationRenderTick - before) /
                    (double)GAMEPLAY_SIMULATION_HZ * 1000.0;
                ctx.lastInterpolationReanchorReason = "clock-lag-exceeded";
                Debug::warn(Debug::Category::Networking,
                    "[NETINTERP REANCHOR] reason=%s latest=%u before=%.2f after=%.2f "
                    "magnitudeMs=%.1f thresholdMs=%.1f\n",
                    ctx.lastInterpolationReanchorReason.c_str(),
                    ctx.latestServerTick, before, ctx.interpolationRenderTick,
                    ctx.lastInterpolationReanchorMagnitudeMs,
                    motionClock.linearReanchorOnlyIfErrorSeconds * 1000.0);
            }
        }
    }

    gInterpolationDebugFrame = ctx.interpolationFrameNumber;
    gInterpolationClockStepMs = ctx.lastInterpolationClockStepMs;
    gInterpolationReanchorCount = ctx.interpolationReanchorCount;
    gInterpolationReanchorMagnitudeMs =
        ctx.lastInterpolationReanchorMagnitudeMs;
    gInterpolationReanchorReason = ctx.lastInterpolationReanchorReason;

    for (auto& kv : ctx.remotePlayers)
    {
        auto interpolation = ctx.remotePlayerInterpolation.find(kv.first);
        if (interpolation != ctx.remotePlayerInterpolation.end())
            updateRenderedReplica(kv.second, interpolation->second,
                                  ctx.interpolationRenderTick, dt, true);
    }
    for (auto& kv : ctx.remoteNpcs)
    {
        auto interpolation = ctx.remoteNpcInterpolation.find(kv.first);
        if (interpolation != ctx.remoteNpcInterpolation.end())
            updateRenderedReplica(kv.second, interpolation->second,
                                  ctx.interpolationRenderTick, dt, true);
    }

    // ── Remote-NPC position alignment diagnostic (once per second, aggregate) ─
    // Proves body / hitbox / muzzle alignment after the direct-render fix:
    // authoritative pos, rendered body pos, hitbox, muzzle, distances, velocity,
    // snapshot age, and interpolation mode.
    {
        static uint64_t lastNpcAlignLog = 0;
        const uint64_t nowDiag = nowMs();
        if (nowDiag - lastNpcAlignLog >= 1000 && !ctx.remoteNpcs.empty())
        {
            lastNpcAlignLog = nowDiag;
            std::string summary;
            for (const auto& kv : ctx.remoteNpcs)
            {
                const auto it = ctx.remoteNpcInterpolation.find(kv.first);
                const SnapshotTransform* target =
                    (it != ctx.remoteNpcInterpolation.end() && it->second.hasTarget)
                        ? &it->second.target : nullptr;
                const glm::vec3 auth = target ? target->position : kv.second.pos;
                const glm::vec3 render = kv.second.pos;
                const Capsule cap = kv.second.getCapsule();
                const glm::vec3 hitbox = (cap.a + cap.b) * 0.5f;
                const glm::vec3 muzzle = render + glm::vec3(0.0f, 0.0f, 0.8f);
                const uint64_t ageMs = target
                    ? (nowDiag > target->receivedMs ? nowDiag - target->receivedMs : 0)
                    : 0;
                char buf[256];
                snprintf(buf, sizeof(buf),
                    "npc=%u auth=(%.1f,%.1f,%.1f) render=(%.1f,%.1f,%.1f) "
                    "hitbox=(%.1f,%.1f,%.1f) muzzle=(%.1f,%.1f,%.1f) "
                    "dAuthRender=%.2f dRenderMuzzle=%.2f vel=(%.1f,%.1f,%.1f) "
                    "ageMs=%llu interp=%s; ",
                    kv.first,
                    auth.x, auth.y, auth.z,
                    render.x, render.y, render.z,
                    hitbox.x, hitbox.y, hitbox.z,
                    muzzle.x, muzzle.y, muzzle.z,
                    glm::length(auth - render), glm::length(render - muzzle),
                    kv.second.vel.x, kv.second.vel.y, kv.second.vel.z,
                    (unsigned long long)ageMs,
                    NetworkingConfig::instance().data().remotePlayers.directRender
                        ? "direct" : "interp");
                summary += buf;
            }
            Debug::warn(Debug::Category::NpcCombat,
                "[CLIENT NPC ALIGN] %s\n", summary.c_str());
        }
    }

    // ── Per-second client/server divergence report ───────────────────
    // Always-on (configurable via debuglogger.json "network" level, default
    // verbose). Writes the exact client-side numbers / true-false states into
    // logs/<date>/Network_log_<runid>.txt so a test can compare what THIS
    // client sees (local predicted pos/hp, rendered remote positions, shot
    // verdicts) against what the server thinks.
    {
        static uint64_t lastNetDivergenceLog = 0;
        const uint64_t nowDiv = nowMs();
        if (nowDiv - lastNetDivergenceLog >= 1000 &&
            ::StructuredLogger::instance().shouldLog(
                ::StructuredCategory::Network, ::StructuredLevel::Verbose))
        {
            lastNetDivergenceLog = nowDiv;
            std::string msg;

            char buf[512];
            snprintf(buf, sizeof(buf),
                "clock tick=%.2f stepMs=%.1f reanchor=%u reason=%s mode=%s",
                ctx.interpolationRenderTick, ctx.lastInterpolationClockStepMs,
                (unsigned)ctx.interpolationReanchorCount,
                ctx.lastInterpolationReanchorReason.empty()
                    ? "none" : ctx.lastInterpolationReanchorReason.c_str(),
                NetworkingConfig::instance().data().remoteMotionSmoothing.renderFilter.c_str());
            msg += buf;

            if (gpPlayer && ctx.hasLocalServerPosition)
            {
                const glm::vec3 corr = ctx.localServerPosition - gpPlayer->pos;
                const float err = glm::length(corr);
                const MovementValidationConfig mvCfg;
                snprintf(buf, sizeof(buf),
                    " | local me=(%.2f,%.2f,%.2f) srv=(%.2f,%.2f,%.2f) "
                    "d=%.3fm class=%s hpMe=%d hpSrv=%d hpDelta=%d "
                    "hasSrv=%d reconciled=%d teleportAck=%d resync=%d",
                    gpPlayer->pos.x, gpPlayer->pos.y, gpPlayer->pos.z,
                    ctx.localServerPosition.x, ctx.localServerPosition.y,
                    ctx.localServerPosition.z, err,
                    movementCorrectionClassName(
                        classifyMovementCorrection(err, mvCfg)),
                    gpPlayer->currentHp, ctx.localServerHealth,
                    gpPlayer->currentHp - ctx.localServerHealth,
                    (int)ctx.hasLocalServerPosition,
                    (int)ctx.localPlayerReconciled,
                    (int)ctx.awaitingTeleportAck,
                    (int)ctx.teleportResync);
                msg += buf;
            }

            uint64_t totHold = 0, totUnderrun = 0, totGlide = 0, totJump = 0,
                     totHardSnap = 0;
            auto accumulate = [&](const auto& map)
            {
                for (const auto& kv : map)
                {
                    totHold += kv.second.holdCount;
                    totUnderrun += kv.second.bufferUnderrunCount;
                    totGlide += kv.second.glideSnapCount;
                    totJump += kv.second.renderJumpCount;
                    totHardSnap += kv.second.hardSnapCount;
                }
            };
            accumulate(ctx.remotePlayerInterpolation);
            accumulate(ctx.remoteNpcInterpolation);

            snprintf(buf, sizeof(buf),
                " | remotes players=%zu npcs=%zu "
                "holds=%llu underruns=%llu glides=%llu jumps=%llu snaps=%llu",
                ctx.remotePlayers.size(), ctx.remoteNpcs.size(),
                (unsigned long long)totHold,
                (unsigned long long)totUnderrun,
                (unsigned long long)totGlide,
                (unsigned long long)totJump,
                (unsigned long long)totHardSnap);
            msg += buf;

            snprintf(buf, sizeof(buf),
                " | hits pred=%llu conf=%llu rej=%llu",
                (unsigned long long)ctx.predictedHits,
                (unsigned long long)ctx.confirmedHits,
                (unsigned long long)ctx.rejectedHits);
            msg += buf;

            // Per-remote-entity one-liner: server target vs client rendered.
            char entBuf[256];
            for (const auto& kv : ctx.remoteNpcInterpolation)
            {
                const auto& s = kv.second;
                if (!s.hasTarget) continue;
                auto repIt = ctx.remoteNpcs.find(kv.first);
                const glm::vec3 render =
                    repIt != ctx.remoteNpcs.end() ? repIt->second.pos : s.target.position;
                snprintf(entBuf, sizeof(entBuf),
                    " | npc%u srv=(%.1f,%.1f,%.1f) ren=(%.1f,%.1f,%.1f) "
                    "d=%.2f hold=%u glide=%u under=%u jump=%u",
                    kv.first, s.target.position.x, s.target.position.y,
                    s.target.position.z, render.x, render.y, render.z,
                    glm::length(s.target.position - render),
                    (unsigned)s.holdCount, (unsigned)s.glideSnapCount,
                    (unsigned)s.bufferUnderrunCount, (unsigned)s.renderJumpCount);
                msg += entBuf;
            }
            for (const auto& kv : ctx.remotePlayerInterpolation)
            {
                const auto& s = kv.second;
                if (!s.hasTarget) continue;
                auto repIt = ctx.remotePlayers.find(kv.first);
                const glm::vec3 render =
                    repIt != ctx.remotePlayers.end() ? repIt->second.pos : s.target.position;
                snprintf(entBuf, sizeof(entBuf),
                    " | p%u srv=(%.1f,%.1f,%.1f) ren=(%.1f,%.1f,%.1f) "
                    "d=%.2f hold=%u glide=%u under=%u jump=%u",
                    kv.first, s.target.position.x, s.target.position.y,
                    s.target.position.z, render.x, render.y, render.z,
                    glm::length(s.target.position - render),
                    (unsigned)s.holdCount, (unsigned)s.glideSnapCount,
                    (unsigned)s.bufferUnderrunCount, (unsigned)s.renderJumpCount);
                msg += entBuf;
            }

            ::StructuredLogger::Entry e;
            e.category = ::StructuredCategory::Network;
            e.level = ::StructuredLevel::Verbose;
            e.eventId = "client-divergence";
            e.reason = "client vs server state summary";
            e.sourceFile = __FILE__;
            e.sourceLine = __LINE__;
            e.functionName = __FUNCTION__;
            if (msg.size() > 3000) msg.resize(3000);
            e.message = msg;
            ::StructuredLogger::instance().write(e);
        }
    }
}

} // namespace MimitaNet

// build timing touch 2026-08-08T20:21:46.5042606-04:00
