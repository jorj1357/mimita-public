#include "network/multiplayer-context.h"
#include "network/net_common.h"
#include "avatar/avatar.h"
#include "config/player-settings.h"
#include "combat/weapon-registry.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"
#include "audio/audio.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <glm/glm.hpp>

namespace MimitaNet {
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
    return transform;
}

} // anonymous namespace

void pushInterpolationTarget(
    EntityInterpolationState& interpolation,
    const SnapshotEntity& entity,
    uint32_t serverTick)
{
    SnapshotTransform next = transformFromEntity(entity);
    next.serverTick = serverTick;
    if (interpolation.hasTarget) {
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
}

static void resetPresentationAfterRespawn(Player& player, const SnapshotTransform& target)
{
    player.proceduralFrozen = false;
    player.dead = false;
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

    printf("[NET PRESENTATION RESPAWN] entityId=%u epoch=%u pos=(%.2f,%.2f,%.2f)\n",
           target.networkEntityId, (unsigned)target.transformEpoch,
           target.position.x, target.position.y, target.position.z);
}

void updateRenderedReplica(
    Player& player,
    EntityInterpolationState& interpolation,
    float dt)
{
    if (!interpolation.hasTarget)
        return;

    constexpr double INTERPOLATION_DELAY_MS = 50.0;
    float t = 1.0f;
    if (interpolation.hasPrevious) {
        const double span = double(interpolation.target.receivedMs - interpolation.previous.receivedMs);
        if (span > 1.0) {
            const double renderTime = double(nowMs()) - INTERPOLATION_DELAY_MS;
            t = std::clamp(
                float((renderTime - double(interpolation.previous.receivedMs)) / span),
                0.0f, 1.0f);
        }
    }

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
                t = 1.0f;
                // Run presentation reset once
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

    // Position uses interpolation for smooth movement.
    if (respawned) {
        // After respawn, snap directly to target position
        player.pos = interpolation.target.position;
    } else {
        player.pos = interpolation.previous.position +
            (interpolation.target.position - interpolation.previous.position) * t;
    }
    player.vel = interpolation.target.velocity;
    player.currentHp = interpolation.target.health;
    player.dead = interpolation.target.health <= 0;

    // Body-facing yaw and aim use the NEWEST target snapshot directly.
    {
        const float targetYaw = interpolation.target.yaw;
        player.yaw = targetYaw;
        const glm::vec3 newestAim = interpolation.target.aimDirection;
        if (std::isfinite(newestAim.x) && std::isfinite(newestAim.y) && std::isfinite(newestAim.z) &&
            glm::dot(newestAim, newestAim) > 0.000001f)
        {
            player.aimDirection = glm::normalize(newestAim);
            player.hasAimData = true;
        }
        else
        {
            player.hasAimData = false;
        }
    }

    player.ground.onGround = interpolation.target.onGround;
    player.equippedSlot = interpolation.target.equippedSlot;
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

    player.networkShootEffectTimer =
        std::max(0.0f, player.networkShootEffectTimer - dt);
    player.networkWeaponState = interpolation.target.weaponState;
    if (player.networkShootEffectTimer > 0.0f)
        player.networkWeaponState |= 1u;
    player.sizeScale = interpolation.target.sizeScale;
    player.username = interpolation.displayName;
    player.networkStateFlags = interpolation.target.stateFlags;
    player.ground.onGround = interpolation.target.onGround;

    // ── Freeze state ─────────────────────────────────────────────────
    // Set freezeActive for the freeze pose state machine in
    // updateProceduralAnimation. Do NOT set proceduralFrozen — that
    // field causes updateProceduralAnimation to return early and skip
    // the freeze pose code entirely. proceduralFrozen is reserved for
    // pause/replay/cinematic use.
    player.freeze.freezeActive =
        (interpolation.target.stateFlags & NET_STATE_FREEZING) != 0;

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
        (interpolation.target.stateFlags & NET_STATE_WALKING) != 0;
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

void mpUpdateRemoteEntities(MultiplayerContext& ctx, float dt)
{
    for (auto& kv : ctx.remotePlayers)
    {
        auto interpolation = ctx.remotePlayerInterpolation.find(kv.first);
        if (interpolation != ctx.remotePlayerInterpolation.end())
            updateRenderedReplica(kv.second, interpolation->second, dt);
    }
    for (auto& kv : ctx.remoteNpcs)
    {
        auto interpolation = ctx.remoteNpcInterpolation.find(kv.first);
        if (interpolation != ctx.remoteNpcInterpolation.end())
            updateRenderedReplica(kv.second, interpolation->second, dt);
    }
}

} // namespace MimitaNet

