#include "network/multiplayer-context.h"
#include "network/net_common.h"
#include "avatar/avatar.h"
#include "config/player-settings.h"
#include "combat/weapon-registry.h"
#include "effects/effect-part.h"

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

    {
        static uint64_t lastLookDecodeLogMs = 0;
        uint64_t nowLookDecode = nowMs();
        if (nowLookDecode - lastLookDecodeLogMs >= 1000)
        {
            printf("[LOOK SNAPSHOT DECODE] entityId=%u epoch=%u yaw=%.2f aim=(%.2f,%.2f,%.2f) "
                   "pos=(%.2f,%.2f,%.2f)\n",
                   entity.networkEntityId, (unsigned)entity.transformEpoch,
                   entity.yaw, entity.aimX, entity.aimY, entity.aimZ,
                   entity.px, entity.py, entity.pz);
            lastLookDecodeLogMs = nowLookDecode;
        }
    }

    {
        static uint64_t lastWalkDecodeLogMs = 0;
        uint64_t nowWalkDecode = nowMs();
        if (nowWalkDecode - lastWalkDecodeLogMs >= 1000)
        {
            glm::vec2 planar(entity.vx, entity.vy);
            printf("[WALK CLIENT DECODE] entityId=%u vel=(%.2f,%.2f,%.2f) "
                   "planarSpeed=%.2f onGround=%d stateFlags=0x%04x walkingFlag=%d\n",
                   entity.networkEntityId, entity.vx, entity.vy, entity.vz,
                   glm::length(planar), (int)entity.onGround,
                   (unsigned)entity.stateFlags,
                   (int)((entity.stateFlags & NET_STATE_WALKING) != 0));
            lastWalkDecodeLogMs = nowWalkDecode;
        }
    }
    transform.lastDashSerial = entity.lastDashSerial;
    transform.sizeScale = entity.sizeScale;
    transform.dashSerial = entity.dashSerial;
    transform.jumpSerial = entity.jumpSerial;
    transform.downDashSerial = entity.downDashSerial;
    transform.equipSerial = entity.equipSerial;
    return transform;
}

float angleLerpDegrees(float from, float to, float t)
{
    float delta = std::fmod(to - from + 540.0f, 360.0f) - 180.0f;
    return from + delta * t;
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
    interpolation.displayName = entity.displayName;
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

    // Position uses interpolation for smooth movement.
    player.pos = interpolation.previous.position +
        (interpolation.target.position - interpolation.previous.position) * t;
    player.vel = interpolation.target.velocity;
    player.currentHp = interpolation.target.health;
    player.dead = interpolation.target.health <= 0;

    // Body-facing yaw and aim use the NEWEST target snapshot directly.
    // Do not interpolate look through the delayed position timeline.
    {
        const float previousYaw = interpolation.previous.yaw;
        const float targetYaw = interpolation.target.yaw;
        player.yaw = targetYaw;
        const glm::vec3 newestAim = interpolation.target.aimDirection;
        const glm::vec3 prevAim = interpolation.previous.aimDirection;
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

        // Rate-limited logging for look debug
        static uint64_t lastLookLogMs = 0;
        uint64_t nowLook = nowMs();
        if (nowLook - lastLookLogMs >= 1000)
        {
            printf("[LOOK REMOTE APPLY] entityId=%u serverTick=%u "
                   "previousYaw=%.2f targetYaw=%.2f appliedYaw=%.2f "
                   "previousAim=(%.2f,%.2f,%.2f) targetAim=(%.2f,%.2f,%.2f) "
                   "appliedAim=(%.2f,%.2f,%.2f) hasAim=%d interpolationT=%.3f\n",
                   interpolation.target.serverTick,
                   interpolation.target.serverTick,
                   previousYaw, targetYaw, player.yaw,
                   prevAim.x, prevAim.y, prevAim.z,
                   newestAim.x, newestAim.y, newestAim.z,
                   player.aimDirection.x, player.aimDirection.y, player.aimDirection.z,
                   (int)player.hasAimData, t);
            lastLookLogMs = nowLook;
        }
    }

    player.ground.onGround = interpolation.target.onGround;
    player.equippedSlot = interpolation.target.equippedSlot;
    {
        // Look up weapon ID from slot for animation system
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

        // Log remote weapon pose input
        {
            static uint64_t lastWeaponPoseLogMs = 0;
            uint64_t nowWp = nowMs();
            if (nowWp - lastWeaponPoseLogMs >= 2000)
            {
                const bool hasRuntime =
                    !player.equippedWeaponId.empty() &&
                    player.weaponRuntimes.find(player.equippedWeaponId) !=
                        player.weaponRuntimes.end();
                printf("[REMOTE WEAPON POSE INPUT] entityId=%u "
                       "equippedSlot=%d equippedWeaponId=%s "
                       "hasValidWeapon=%d hasRuntime=%d runtimeCount=%zu "
                       "networkWeaponState=%u equipSerial=%u\n",
                       interpolation.target.serverTick,
                       (int)player.equippedSlot,
                       player.equippedWeaponId.c_str(),
                       (int)player.hasValidWeapon,
                       (int)hasRuntime,
                       player.weaponRuntimes.size(),
                       (unsigned)interpolation.target.weaponState,
                       (unsigned)interpolation.target.equipSerial);
                lastWeaponPoseLogMs = nowWp;
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

    // Apply replicated state flags directly (override per-player)
    player.networkStateFlags = interpolation.target.stateFlags;
    player.ground.onGround = interpolation.target.onGround;
    player.freeze.freezeActive =
        (interpolation.target.stateFlags & NET_STATE_FREEZING) != 0;
    player.proceduralFrozen = player.freeze.freezeActive;

    // Dash serial change → trigger dash effect once
    if (interpolation.target.dashSerial != 0 &&
        interpolation.target.dashSerial != player.networkLastDashSerial)
    {
        player.dash.didDash = true;
        player.networkLastDashSerial = interpolation.target.dashSerial;
        EffectPartSystem::instance().spawnDash(player.pos, player.sizeScale);
        printf("[NET DASH] remote dash serial=%u\n",
               (unsigned)interpolation.target.dashSerial);
    }

    // Jump serial change → trigger jump effect once
    if (interpolation.target.jumpSerial != 0 &&
        interpolation.target.jumpSerial != player.networkLastJumpSerial)
    {
        player.networkLastJumpSerial = interpolation.target.jumpSerial;
        // Jump effect handled by updating procedural animation below
    }

    // Down-dash serial change → trigger down-dash effect once
    if (interpolation.target.downDashSerial != 0 &&
        interpolation.target.downDashSerial != player.networkLastDownDashSerial)
    {
        player.networkLastDownDashSerial = interpolation.target.downDashSerial;
        // Down-dash effect handled by procedural animation
    }

    // Pass reconstructed aim direction so local animation system
    // produces matching limb positions for the remote player.
    const bool remoteWalking =
        (interpolation.target.stateFlags &
        NET_STATE_WALKING) != 0;

    {
        static uint64_t lastAnimInputLogMs = 0;
        uint64_t nowAnimInput = nowMs();
        if (nowAnimInput - lastAnimInputLogMs >= 1000)
        {
            glm::vec2 planar(interpolation.target.velocity.x, interpolation.target.velocity.y);
            printf("[REMOTE ANIM INPUT] entityId=%u remoteWalking=%d stateFlags=0x%04x "
                   "vel=(%.2f,%.2f,%.2f) planarSpeed=%.2f onGround=%d "
                   "proceduralFrozen=%d modelLoaded=%d "
                   "currentAnim=%s animStateTime=%.3f\n",
                   interpolation.target.serverTick, (int)remoteWalking,
                   (unsigned)interpolation.target.stateFlags,
                   interpolation.target.velocity.x, interpolation.target.velocity.y,
                   interpolation.target.velocity.z, glm::length(planar),
                   (int)interpolation.target.onGround,
                   (int)player.proceduralFrozen, (int)player.modelLoaded,
                   player.currentAnimName.c_str(), player.animStateTime);
            lastAnimInputLogMs = nowAnimInput;
        }
    }

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

