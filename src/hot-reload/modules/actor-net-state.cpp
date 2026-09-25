// 09 24 2026
/* purpose
* Hot server-side actor replication projection. Reads the typed Transform /
* Velocity components plus the actor control source for every generic actor
* entity and writes one versioned `ActorNetState` dynamic component. Generic
* dynamic replication then carries position/velocity/aim/yaw to clients because
* the actor possesses the component, not because packet code has an NPC branch.
* This is the server half of Phase 4 (generic actor replication); the compact
* ENTITY_NPC snapshot stays as a compatibility transport until parity.
* Does NOT own the loop, transport, spawn/destroy, damage, or the renderer.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"

#include <cstdint>

namespace {

const std::uint64_t kHealthState = gameHash("ActorHealthState");
const std::uint64_t kNetState = gameHash("ActorNetState");
const std::uint64_t kWeaponState = gameHash("ActorWeaponState");

// Wire-facing projection of authoritative Transform/Velocity. Layout must match
// MimitaNet::ActorNetStateV1 in src/network/actor-state.h.
struct ActorNetStateV1 {
    float position[3];
    float velocity[3];
    float aim[3];
    float yaw;
    std::uint32_t onGround;
    std::int16_t equippedSlot;
    std::uint8_t weaponState;
    std::uint8_t reserved8;
    std::uint32_t reserved;
};

// Generic weapon-presentation input (hot input only). Layout must match
// MimitaNet::ActorWeaponStateV1.
struct ActorWeaponStateV1 {
    std::int16_t equippedSlot;
    std::uint8_t weaponState;
    std::uint8_t reserved8;
    std::uint32_t reserved;
};

void MIMITA_GAME_CALL actorNetStateTick(void* host, std::uint64_t /*tick*/,
                                        float /*dt*/)
{
    auto* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || !ctx->readComponent || !ctx->dynamicEnumerateComponent ||
        !ctx->dynamicReadComponent || !ctx->dynamicWriteComponent)
        return;

    std::uint64_t entities[256] = {0};
    const std::uint32_t count =
        ctx->dynamicEnumerateComponent(ctx->host, kHealthState, entities, 256);
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint64_t e = entities[i];
        // Server realm only (bits 40..43). A listen-server process also holds
        // client-replicated entities; never project those.
        if (((e >> 40) & 0x0fu) != 0u)
            continue;

        GameControlSourceComponentV1 cs{};
        if (ctx->readComponent(ctx->host, e, GAME_COMPONENT_CONTROL_SOURCE, &cs,
                               sizeof(cs))) {
            if (cs.source != GAME_CONTROL_SERVER_NPC)
                continue;
        } else {
            continue;
        }

        GameTransformComponentV1 tf{};
        if (!ctx->readComponent(ctx->host, e, GAME_COMPONENT_TRANSFORM, &tf,
                                sizeof(tf)))
            continue;
        GameVelocityComponentV1 vl{};
        ctx->readComponent(ctx->host, e, GAME_COMPONENT_VELOCITY, &vl,
                           sizeof(vl));
        GameMovementRuntimeStateComponentV1 rs{};
        const bool haveRuntime = ctx->readComponent(
            ctx->host, e, GAME_COMPONENT_MOVEMENT_RUNTIME_STATE, &rs, sizeof(rs));

        ActorWeaponStateV1 ws{};
        const bool haveWeapon = ctx->dynamicReadComponent(
            ctx->host, e, kWeaponState, &ws, sizeof(ws));

        ActorNetStateV1 ns{};
        ns.position[0] = tf.position[0];
        ns.position[1] = tf.position[1];
        ns.position[2] = tf.position[2];
        ns.velocity[0] = vl.linear[0];
        ns.velocity[1] = vl.linear[1];
        ns.velocity[2] = vl.linear[2];
        ns.aim[0] = tf.look[0];
        ns.aim[1] = tf.look[1];
        ns.aim[2] = tf.look[2];
        ns.yaw = tf.yaw;
        ns.onGround = (haveRuntime && rs.grounded) ? 1u : 0u;
        ns.equippedSlot = haveWeapon ? ws.equippedSlot : (std::int16_t)0;
        ns.weaponState = haveWeapon ? ws.weaponState : (std::uint8_t)0;
        ctx->dynamicWriteComponent(ctx->host, e, kNetState, &ns, sizeof(ns));
    }
}

} // namespace

const MimitaHotPackage::SystemRegistrar s_actorNetState{
    {gameHash("actor.net-state"), GAME_DOMAIN_GAMEPLAY, 690, 0,
     actorNetStateTick, "actor.net-state"}};

#endif // MIMITA_GAME_DLL
