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

// Builds the render-time snapshot for a remote actor using tick-based
// interpolation with a clamped per-entity render clock. The clock advances at
// most dt*60 per frame and is capped at `newestBufferedTick - delayTicks`, so
// bursts of reordered snapshots (badconn) catch up smoothly over a few frames
// instead of flashing the body forward. Interpolating by server tick (uniform
// 60Hz) rather than arrival time keeps motion uniform under reordering.
void buildReceiveTimeRender(EntityInterpolationState& interpolation,
                            const RemotePlayerInterpolationConfig& interpCfg,
                            const double delaySeconds,
                            float dt,
                            SnapshotTransform& out)
{
    const uint32_t oldestTick = interpolation.buffer.front().serverTick;
    const uint32_t newestTick = interpolation.buffer.back().serverTick;

    const double delayTicks = delaySeconds * (double)GAMEPLAY_SIMULATION_HZ;
    const double targetTick = (double)newestTick - delayTicks;

    // Advance the clamped render clock. Never exceed the target, so a burst of
    // new snapshots cannot make it jump; it catches up smoothly instead.
    if (interpolation.renderTick <= 0.0)
        interpolation.renderTick = targetTick;
    else
        interpolation.renderTick += (double)dt * (double)GAMEPLAY_SIMULATION_HZ;
    interpolation.renderTick = std::min(interpolation.renderTick, targetTick);
    const double renderTick = std::max(interpolation.renderTick, (double)oldestTick);
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

    // Buffer ran dry: renderTick is newer than everything buffered. Extrapolate
    // along the newest velocity (time-limited) if allowed, else hold newest.
    if (renderTick > (double)newestTick)
    {
        const SnapshotTransform& newest = interpolation.buffer.back();
        out = newest;
        if (interpCfg.allowExtrapolation)
        {
            const double extraTicks = renderTick - (double)newestTick;
            const double extraMs =
                extraTicks / (double)GAMEPLAY_SIMULATION_HZ * 1000.0;
            const double extrapMs = std::min(
                extraMs, interpCfg.maximumExtrapolationSeconds * 1000.0);
            if (extrapMs > 0.0)
            {
                out.position = newest.position +
                    newest.velocity * (float)(extrapMs / 1000.0);
                if (gNetInterpDebug)
                    printf("[NETINTERP EXTRAP] id=%u over=%.1fms\n",
                           interpolation.networkEntityId, extrapMs);
            }
        }
        return;
    }

    if (renderTick < (double)oldestTick)
    {
        // renderTick older than everything buffered (fresh spawn / very thin
        // buffer): hold the oldest snapshot. No lerp yet.
        out = *older;
        return;
    }

    double alpha = 1.0;
    if (newer->serverTick > older->serverTick)
    {
        alpha = (renderTick - (double)older->serverTick) /
                (double)(newer->serverTick - older->serverTick);
        alpha = std::clamp(alpha, 0.0, 1.0);
    }

    // Corruption protection: an absurd per-segment gap without a lifecycle
    // change is treated as a teleport. Not a speed limit.
    const float gapDist = glm::length(newer->position - older->position);
    if (gapDist > interpCfg.teleportDistance)
    {
        out = *newer;
        return;
    }

    out = *newer;
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
        return false;
    }

    SnapshotTransform next = transformFromEntity(entity);
    next.serverTick = serverTick;
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
    float dt)
{
    if (!interpolation.hasTarget)
        return;

    (void)renderTick;  // receive-time interpolation uses nowMs() internally
    player.dash.didDash = false;

    const auto& interpCfg = NetworkingConfig::instance().data().remotePlayers;

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
    // `newestBufferedTick - interpolationDelay` via tick-based interpolation
    // with a clamped per-entity clock (or is the newest snapshot in
    // direct-render mode, which is internally consistent).
    const double delaySeconds =
        NetworkingConfig::instance().effectiveRemoteInterpolationDelaySeconds();

    SnapshotTransform render = interpolation.target;

    if (!interpCfg.directRender && interpCfg.enabled)
    {
        if (interpolation.buffer.size() >= 2)
        {
            buildReceiveTimeRender(
                interpolation, interpCfg, delaySeconds, dt, render);
        }
        else if (interpolation.hasRendered)
        {
            // Buffer too thin to interpolate: HOLD the last rendered snapshot.
            // Never snap the body to the newest packet just because it arrived.
            render = interpolation.lastRender;
        }
        else
        {
            // First render (fresh spawn / very thin buffer): seed from the
            // newest snapshot; subsequent frames will hold/interpolate.
            render = interpolation.target;
        }
    }

    const glm::vec3 renderPos = render.position;
    const float renderYaw = render.yaw;
    const glm::vec3 renderAim = render.aimDirection;

    if (respawned)
    {
        player.pos = interpolation.target.position;
        // Hard-snap the remembered render state too, so a thin buffer on the
        // next frame holds the respawn position rather than the old corpse.
        interpolation.lastRender = interpolation.target;
    }
    else
        player.pos = renderPos;
    player.vel = render.velocity;
    player.currentHp = render.health;
    player.dead = render.health <= 0;

    // Remember what was actually rendered so a thin buffer holds it exactly.
    if (!respawned)
        interpolation.lastRender = render;
    interpolation.hasRendered = true;

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
    player.spawnGeneration = interpolation.target.spawnGeneration;
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
    if (interpolation.target.dashSerial != 0 &&
        interpolation.target.dashSerial != player.networkLastDashSerial)
    {
        player.networkLastDashSerial = interpolation.target.dashSerial;
        player.dash.didDash = true;
        dashTriggered = true;
        glm::vec3 dashDir = glm::length(player.vel) > 0.001f
            ? glm::normalize(player.vel)
            : glm::vec3(0.0f, 1.0f, 0.0f);
        HitEffects::spawnMovementDashBurst(player.pos, dashDir, glm::length(player.vel));
        playWorldSound("entity/player/dash", player.pos, 1.0f, 1.0f, 36.0f);
        printf("[NET PRESENTATION RX] entityId=%u event=dash serial=%u triggered=1\n",
               entityId, (unsigned)interpolation.target.dashSerial);
    }

    // Ground jump
    if (interpolation.target.groundJumpSerial != 0 &&
        interpolation.target.groundJumpSerial != player.networkLastGroundJumpSerial)
    {
        player.networkLastGroundJumpSerial = interpolation.target.groundJumpSerial;
        glm::vec3 jumpDir = glm::vec3(0.0f);
        glm::vec3 jumpPos = player.pos;
        jumpPos.z -= 0.5f;
        HitEffects::spawnGroundJumpBurst(jumpPos, jumpDir);
        playWorldSound("entity/player/jump", player.pos, 1.0f, 1.0f, 28.0f);
        printf("[NET PRESENTATION RX] entityId=%u event=groundJump serial=%u triggered=1\n",
               entityId, (unsigned)interpolation.target.groundJumpSerial);
    }

    // Air jump
    if (interpolation.target.airJumpSerial != 0 &&
        interpolation.target.airJumpSerial != player.networkLastAirJumpSerial)
    {
        player.networkLastAirJumpSerial = interpolation.target.airJumpSerial;
        glm::vec3 jumpDir = glm::vec3(0.0f);
        glm::vec3 jumpPos = player.pos;
        jumpPos.z -= 1.0f;
        HitEffects::spawnAirJumpBurst(jumpPos, jumpDir);
        playWorldSound("entity/player/jump", player.pos, 1.0f, 1.0f, 28.0f);
        printf("[NET PRESENTATION RX] entityId=%u event=airJump serial=%u triggered=1\n",
               entityId, (unsigned)interpolation.target.airJumpSerial);
    }

    // Down dash
    if (interpolation.target.downDashSerial != 0 &&
        interpolation.target.downDashSerial != player.networkLastDownDashSerial)
    {
        player.networkLastDownDashSerial = interpolation.target.downDashSerial;
        glm::vec3 downDashPos = player.pos;
        downDashPos.z -= 0.3f;
        EffectPartSystem::instance().spawnDownDash(downDashPos);
        printf("[NET PRESENTATION RX] entityId=%u event=downDash serial=%u triggered=1\n",
               entityId, (unsigned)interpolation.target.downDashSerial);
    }

    // Freeze one-shot (didFreeze)
    if (interpolation.target.freezeSerial != 0 &&
        interpolation.target.freezeSerial != player.networkLastFreezeSerial)
    {
        player.networkLastFreezeSerial = interpolation.target.freezeSerial;
        glm::vec3 freezePos = player.pos;
        freezePos.z -= 0.3f;
        EffectPartSystem::instance().spawnFreeze(freezePos, 2.0f);
        playWorldSound("entity/player/freezebegin", player.pos, 1.0f, 1.0f, 30.0f);
        printf("[NET PRESENTATION RX] entityId=%u event=freeze serial=%u triggered=1\n",
               entityId, (unsigned)interpolation.target.freezeSerial);
    }

    // Freeze trail (sustained while freezeActive)
    if (player.freeze.freezeActive)
    {
        EffectPartSystem::instance().spawnFreezeTrail(player.pos);
    }

    // Direction change one-shot directional walk burst
    if (interpolation.target.directionChangeSerial != 0 &&
        interpolation.target.directionChangeSerial != player.networkLastDirectionChangeSerial)
    {
        player.networkLastDirectionChangeSerial = interpolation.target.directionChangeSerial;
        glm::vec3 dirWalkVel = glm::length(player.vel) > 0.001f
            ? glm::normalize(glm::vec3(player.vel.x, player.vel.y, 0.0f))
            : glm::vec3(0.0f, 0.0f, 0.0f);
        float speed = glm::length(glm::vec2(player.vel.x, player.vel.y));
        HitEffects::spawnWalkBurst(player.pos, -dirWalkVel, speed);
        printf("[NET PRESENTATION RX] entityId=%u event=directionChange serial=%u triggered=1\n",
               entityId, (unsigned)interpolation.target.directionChangeSerial);
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
    // Advance the interpolation render clock at the fixed 60 tick/s rate so
    // rendering is time-based rather than tied to packet arrival.
    if (!ctx.interpolationClockStarted)
    {
        uint32_t newest = 0;
        for (const auto& kv : ctx.remotePlayerInterpolation)
            if (kv.second.hasTarget && kv.second.target.serverTick > newest)
                newest = kv.second.target.serverTick;
        for (const auto& kv : ctx.remoteNpcInterpolation)
            if (kv.second.hasTarget && kv.second.target.serverTick > newest)
                newest = kv.second.target.serverTick;
        if (newest != 0)
        {
            ctx.interpolationRenderTick = (double)newest;
            ctx.interpolationClockStarted = true;
        }
    }
    else
    {
        ctx.interpolationRenderTick += (double)dt * (double)GAMEPLAY_SIMULATION_HZ;
    }

    for (auto& kv : ctx.remotePlayers)
    {
        auto interpolation = ctx.remotePlayerInterpolation.find(kv.first);
        if (interpolation != ctx.remotePlayerInterpolation.end())
            updateRenderedReplica(kv.second, interpolation->second, ctx.interpolationRenderTick, dt);
    }
    for (auto& kv : ctx.remoteNpcs)
    {
        auto interpolation = ctx.remoteNpcInterpolation.find(kv.first);
        if (interpolation != ctx.remoteNpcInterpolation.end())
            updateRenderedReplica(kv.second, interpolation->second, ctx.interpolationRenderTick, dt);
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

