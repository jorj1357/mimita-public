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
#include "config/networking-config.h"
#include "avatar/avatar.h"
#include "config/player-settings.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-runtime.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"
#include "audio/audio.h"
#include "debug/debug-log.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <glm/glm.hpp>

namespace MimitaNet {

bool gNetInterpDebug = false;

namespace {

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
                            SnapshotTransform& out)
{
    const uint32_t oldestTick = interpolation.buffer.front().serverTick;
    const uint32_t newestTick = interpolation.buffer.back().serverTick;

    const double delayTicks = delaySeconds * (double)GAMEPLAY_SIMULATION_HZ;
    const double renderTick = globalRenderTick - delayTicks;
    interpolation.renderTick = renderTick;

    // Buffer ran dry: renderTick is newer than everything buffered. Extrapolate
    // along the newest velocity. When extrapolation_keep_moving is set, keep
    // gliding past the time cap with exponentially decaying velocity instead of
    // freezing the body at a fixed spot (a hard hold read as "jitter in one
    // spot, then skip"). Otherwise stop at the cap.
    if (renderTick > (double)newestTick)
    {
        const SnapshotTransform& newest = interpolation.buffer.back();
        out = newest;
        interpolation.extrapolating = true;
        if (allowExtrapolation)
        {
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
            out.position = newest.position +
                newest.velocity * (float)(moveMs / 1000.0);
            if (gNetInterpDebug)
                printf("[NETINTERP EXTRAP] id=%u over=%.1fms\n",
                       interpolation.networkEntityId, moveMs);
        }
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
        out = interpolation.buffer.front();
        out.serverTick = oldestTick;
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

    // Time-gap protection: a hole wider than teleportGapTicks (e.g. a long
    // blackout) is a discontinuity, not a velocity bridge. With teleport_gap_snap
    // snap to the newest snapshot rather than sliding a stale straight line
    // across seconds of missing motion; otherwise fall through so the motion
    // filter converges the body smoothly. Short loss gaps interpolate linearly.
    if ((uint32_t)(newer->serverTick - older->serverTick) >
            interpCfg.teleportGapTicks &&
        interpCfg.teleportGapSnap)
    {
        out = *newer;
        out.serverTick = newer->serverTick;
        return;
    }

    // Corruption protection: an absurd per-segment distance without a
    // lifecycle change is treated as a teleport. Not a speed limit.
    const float gapDist = glm::length(newer->position - older->position);
    if (gapDist > interpCfg.teleportDistance)
    {
        out = *newer;
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

} // anonymous namespace

bool pushInterpolationTarget(
    EntityInterpolationState& interpolation,
    const SnapshotEntity& entity,
    uint32_t serverTick)
{
    if (interpolation.hasTarget &&
        !movementSnapshotIsFresh(serverTick,
                                 entity.spawnGeneration,
                                 entity.transformEpoch,
                                 interpolation.lastServerTick,
                                 interpolation.lastSpawnGeneration,
                                 interpolation.lastSnapshotTransformEpoch))
    {
        ++interpolation.staleSnapshotCount;
        return false;
    }

    // allow_out_of_order_insertion=false: a reordered older snapshot must not
    // be inserted mid-buffer (it can shift the bracketing pair and make a
    // smooth-connection body appear to slide backward to a spot it just left).
    if (interpolation.hasTarget &&
        serverTick < interpolation.lastServerTick &&
        !NetworkingConfig::instance().data().snapshotBuffer.allowOutOfOrderInsertion)
    {
        ++interpolation.outOfOrderSnapshotCount;
        return false;
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
    interpolation.lastServerTick = serverTick;
    interpolation.lastSpawnGeneration = entity.spawnGeneration;
    if (entity.transformEpoch != 0)
        interpolation.lastSnapshotTransformEpoch = entity.transformEpoch;

    // ── Tick-ordered snapshot buffer (time-based interpolation) ─────
    const auto& interpCfg = NetworkingConfig::instance().data().remotePlayers;

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

    while (interpolation.buffer.size() > interpCfg.maximumBufferedSnapshots)
        interpolation.buffer.pop_front();

    return true;
}

static void resetPresentationAfterRespawn(Player& player, const SnapshotTransform& target)
{
    player.proceduralFrozen = false;
    player.dead = false;
    player.deathAnim = Player::DeathAnimState{};
    player.currentHp = target.health;

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

    // ── Detect respawn via epoch change ──────────────────────────────
    // Hard-snap interpolation position and run presentation reset.
    // Do NOT replay existing event serials.
    bool respawned = false;
    if (targetEpoch != 0 && targetEpoch != interpolation.lastTransformEpoch)
    {
        if (interpolation.lastTransformEpoch != 0)
        {
            // Epoch changed while entity was alive → respawn
            const bool wasDead = interpolation.previous.health <= 0;
            const bool nowAlive = interpolation.target.health > 0;
            if (wasDead || nowAlive || interpolation.lastTransformEpoch != 0)
            {
                // Hard-snap position: no lerp from corpse to spawn
                resetPresentationAfterRespawn(player, interpolation.target);
                respawned = true;
                printf("[NET EPOCH RESPAWN] entityId=%u oldEpoch=%u newEpoch=%u "
                       "wasDead=%d nowAlive=%d pos=(%.2f,%.2f,%.2f)\n",
                       entityId, (unsigned)interpolation.lastTransformEpoch,
                       (unsigned)targetEpoch, (int)wasDead, (int)nowAlive,
                       interpolation.target.position.x,
                       interpolation.target.position.y,
                       interpolation.target.position.z);
            }
        }
        interpolation.lastTransformEpoch = targetEpoch;
    }

    // ── Render state at one consistent view time ─────────────────────
    // One rule: position, velocity, yaw, aim, animation state, and weapon all
    // come from the SAME render-time snapshot, so the body can never desync
    // from its own animation or shots. The render snapshot is produced at
    // `estimatedServerNow - delay` on a continuous wall-clock clock, so the
    // interpolation alpha is always fractional (smooth at any frame rate; no
    // freeze/jump from a clamped catch-up clock).
    const NetworkingConfigData& netCfg = NetworkingConfig::instance().data();
    const bool linearMode = (motion.renderFilter == "linear");
    // linear mode uses a fixed whole-tick delay (the loss buffer); other modes
    // use the adaptive per-entity delay that grows under jitter/loss.
    const double delaySeconds = linearMode
        ? (double)motion.linearDelayTicks / (double)GAMEPLAY_SIMULATION_HZ
        : adaptiveDelaySeconds(interpolation, netCfg, dt);

    SnapshotTransform render = interpolation.target;

    if (!interpCfg.directRender && interpCfg.enabled)
    {
        if (interpolation.buffer.size() >= 2)
        {
            // `renderTick` is the estimated current server tick (ticks). The
            // render time is that minus the delay; the interpolation alpha is
            // always fractional, so the body moves smoothly at any frame rate.
            // linear mode never extrapolates when holding on dry; other modes
            // honor interpCfg.allowExtrapolation.
            const bool allowExtrap = linearMode
                ? !motion.linearHoldOnDry
                : interpCfg.allowExtrapolation;
            buildReceiveTimeRender(
                interpolation, interpCfg, delaySeconds, renderTick,
                allowExtrap, render);
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

    // ── Post-interpolation motion filter ─────────────────────────────
    // "direct":  set the position directly (no smoothing).
    // "bounded": move toward the interpolated target at a capped speed so a
    //            discontinuity (loss hole, extrapolation resume, blackout)
    //            converges over a few frames instead of snapping. Normal smooth
    //            motion and dashes pass through (target tracks the body).
    // "spring":  always-on critically-damped spring — literally cannot snap;
    //            adds a few ms of follow-lag on fast turns.
    if (interpolation.hasRendered && !respawned && !interpCfg.directRender)
    {
        const float safeDt = std::min(dt, 0.05f);
        if (motion.renderFilter == "spring")
        {
            // Implicit-velocity critically-damped spring: unconditionally
            // stable, so it never rings at high frequency (a semi-implicit
            // Euler spring under-damps in discrete time and wobbles ±1).
            const float k = (float)motion.springStiffness;
            const float c = (float)motion.springDamping;
            const float denom = 1.0f + c * safeDt;
            glm::vec3 vel = interpolation.renderSpring.velocity;
            const glm::vec3 accel =
                (renderPos - interpolation.renderSpring.value) * k;
            vel = (vel + accel * safeDt) / denom;
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
    }
    else
        player.pos = renderPos;
    player.vel = render.velocity;
    player.currentHp = render.health;
    player.dead = render.health <= 0;

    // ── Death effect (snapshot-driven, loss-proof) ───────────────────
    // Detect the >0 → <=0 health transition in the render stream and spawn the
    // death ellipsoid so every client (attacker, victim, observers) sees it,
    // even when the reliable damage event is dropped under packet loss.
    if (spawnDeathEffects && interpolation.hasRendered &&
        NetworkingConfig::instance().data().deathEffects.remotePlayerDeathEffect)
    {
        const bool wasAlive = interpolation.lastRender.health > 0;
        const bool nowDead = render.health <= 0;
        if (wasAlive && nowDead)
        {
            // Elongate toward the pre-death planar velocity (the death frame
            // itself carries zero velocity because dead players stop).
            const glm::vec3& deathVel = interpolation.lastRender.velocity;
            const glm::vec3 deathDir =
                glm::length(deathVel) > 0.001f
                    ? glm::normalize(glm::vec3(deathVel.x, deathVel.y, 0.0f))
                    : glm::vec3(0.0f, 0.0f, 1.0f);
            const auto& deCfg = HitEffects::config().deathEllipsoid;
            if (deCfg.enabled)
            {
                EffectPartSystem::instance().spawnDeathEllipsoid(
                    player.pos, deathDir,
                    deCfg.length, deCfg.radius, deCfg.lifetime,
                    player.sizeScale);
            }
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

void mpUpdateRemoteEntities(MultiplayerContext& ctx, float dt)
{
    // Continuous wall-clock-anchored render clock in server-tick units:
    // estimatedServerNow = newest received server tick + elapsed real time
    // since that snapshot was received. It advances smoothly between snapshot
    // arrivals and is re-anchored on every snapshot, so interpolation runs on
    // a smooth time base (no packet-arrival-driven accumulator). This is what
    // makes the interpolation alpha fractional at any frame rate and lets
    // reordered/lost-snapshot bursts pass through without an artificial
    // catch-up fast-forward.
    if (ctx.latestServerTick != 0 && ctx.lastSnapshotReceivedMs != 0)
    {
        const double tickMs = 1000.0 / (double)GAMEPLAY_SIMULATION_HZ;
        const uint64_t now = nowMs();
        const double ageTicks = now >= ctx.lastSnapshotReceivedMs
            ? (double)(now - ctx.lastSnapshotReceivedMs) / tickMs
            : 0.0;
        ctx.interpolationRenderTick = (double)ctx.latestServerTick + ageTicks;
        ctx.interpolationClockStarted = true;
    }

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
}

} // namespace MimitaNet
