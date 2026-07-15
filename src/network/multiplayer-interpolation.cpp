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

    player.pos = interpolation.previous.position +
        (interpolation.target.position - interpolation.previous.position) * t;
    player.vel = interpolation.target.velocity;
    player.yaw = angleLerpDegrees(interpolation.previous.yaw, interpolation.target.yaw, t);
    player.currentHp = interpolation.target.health;
    player.dead = interpolation.target.health <= 0;
    player.ground.onGround = interpolation.target.onGround;
    player.equippedSlot = interpolation.target.equippedSlot;
    {
        // Look up weapon ID from slot for animation system
        player.equippedWeaponId.clear();
        if (player.equippedSlot >= 1) {
            auto reg = WeaponRegistry::instance().all();
            for (const auto& w : reg) {
                if (w.second.slot == player.equippedSlot) {
                    player.equippedWeaponId = w.first;
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
    player.aimDirection = interpolation.hasPrevious
        ? glm::normalize(glm::mix(interpolation.previous.aimDirection, interpolation.target.aimDirection, t))
        : interpolation.target.aimDirection;
    player.hasAimData = glm::length(player.aimDirection) > 0.001f;
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
    player.updateProceduralAnimation(dt, player.aimDirection, player.pos);
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

