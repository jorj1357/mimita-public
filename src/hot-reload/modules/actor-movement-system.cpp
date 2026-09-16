// 09 16 2026
/* purpose
* movement.actors: the hot actor-movement system. Iterates authoritative actors
* (server human players), reads the generic Transform/Velocity/MovementIntent/
* RuntimeState/Body components, runs the shared Source movement policy, resolves
* collision through the kernel physics.move primitive (headless world), and
* writes the result back with a per-tick stamp so the cold server yields.
* Human and NPC actors share this code; only the intent source differs.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-actor-movement.h"
#include "hot-reload/hot-movement-policy.h"
#include "hot-reload/hot-package.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {

// Server-side simulation toggle. While the input-receive policy adopts the
// client's validated movement (spec phase 1), the actor system must not fight
// it. Set true to return to server-authoritative simulation live.
constexpr bool kSimulateServerActors = false;

using PhysicsMoveFn = void (MIMITA_GAME_CALL *)(void*, MovementStateV1*, float,
                                                std::uint32_t);

void simulateOneActor(GameplayContextV1* ctx, std::uint64_t e, float dt,
                      std::uint32_t tick)
{
    GameControlSourceComponentV1 cs{};
    const bool hasControl =
        ctx->readComponent(ctx->host, e, GAME_COMPONENT_CONTROL_SOURCE, &cs,
                           sizeof(cs));
    // Only authoritative remote-network actors (server human players). The local
    // human is owned by movement.main; NPCs stay on the cold shared-policy path
    // until their yield is wired. An absent component is treated as a server
    // player (cold does not always stamp control source).
    if (hasControl && cs.source != GAME_CONTROL_REMOTE_NETWORK)
        return;

    GameMovementIntentComponentV1 mi{};
    if (!ctx->readComponent(ctx->host, e, GAME_COMPONENT_MOVEMENT_INTENT, &mi,
                            sizeof(mi)))
        return;

    GameTransformComponentV1 tf{};
    if (!ctx->readComponent(ctx->host, e, GAME_COMPONENT_TRANSFORM, &tf,
                            sizeof(tf)))
        return;

    GameVelocityComponentV1 vl{};
    ctx->readComponent(ctx->host, e, GAME_COMPONENT_VELOCITY, &vl, sizeof(vl));

    GameMovementRuntimeStateComponentV1 rs{};
    if (!ctx->readComponent(ctx->host, e,
                            GAME_COMPONENT_MOVEMENT_RUNTIME_STATE, &rs,
                            sizeof(rs)) ||
        rs.version != MOVEMENT_RUNTIME_STATE_VERSION)
    {
        rs = GameMovementRuntimeStateComponentV1{};
        rs.version = MOVEMENT_RUNTIME_STATE_VERSION;
        rs.airJumpsLeft = 1;
        rs.jumpAirJumpArmed = 1u;
        rs.dashAvailable = 1u;
        rs.downDashAvailable = 1u;
    }

    GameBodyComponentV1 body{};
    if (!ctx->readComponent(ctx->host, e, GAME_COMPONENT_BODY, &body,
                            sizeof(body)))
    {
        body.radius = 0.4f;
        body.height = 1.8f;
        body.sizeScale = 1.0f;
    }

    const MimitaHotMovement::MovementMode& m =
        MimitaHotMovement::defaultActorMovementMode();

    float vx = vl.linear[0];
    float vy = vl.linear[1];
    float vz = vl.linear[2];

    const float wishX = mi.moveX;
    const float wishY = mi.moveY;
    const float wishLen = std::sqrt(wishX * wishX + wishY * wishY);
    const bool hasWish = mi.pressed && wishLen > 1e-4f;
    const float wdx = hasWish ? wishX / wishLen : 0.0f;
    const float wdy = hasWish ? wishY / wishLen : 0.0f;

    rs.dashCooldownSeconds = std::max(0.0f, rs.dashCooldownSeconds - dt);
    const bool dashEdge = mi.dash && !rs.dashHeldPreviously;
    const bool downDashEdge = mi.downDash && !rs.downDashHeldPreviously;
    const bool jumpEdge = mi.jump && !rs.jumpHeldPreviously;
    const float speed = m.walkSpeed;

    if (rs.grounded)
    {
        GameGroundMoveV1 g{};
        g.velocity[0] = vx;
        g.velocity[1] = vy;
        g.wishDir[0] = wdx;
        g.wishDir[1] = wdy;
        g.wishSpeed = speed;
        g.groundAcceleration = m.groundAccel;
        g.frictionAmount = m.groundFriction;
        g.stopspeed = 0.0f;
        g.dt = dt;
        g.hasInput = hasWish ? 1u : 0u;
        float out[2] = {vx, vy};
        MimitaHotMovement::groundMove(g, out);
        vx = out[0];
        vy = out[1];
    }
    else if (hasWish)
    {
        GameAirAccelerateV1 a{};
        a.velocity[0] = vx;
        a.velocity[1] = vy;
        a.wishDir[0] = wdx;
        a.wishDir[1] = wdy;
        a.wishSpeed = speed;
        a.wishspd = speed;
        a.maxSpeed = speed;
        a.airAcceleration = m.airAccel;
        a.surfaceFriction = 1.0f;
        a.airSpeedGainMultiplier = 1.0f;
        a.dt = dt;
        a.currentSpeed = vx * wdx + vy * wdy;
        a.blendedAddSpeed = a.wishspd - a.currentSpeed;
        float out[2] = {vx, vy};
        MimitaHotMovement::airAccelerate(a, out);
        vx = out[0];
        vy = out[1];
    }

    {
        const float yawRad = tf.yaw * MimitaHotMovement::kRadPerDegree;
        GameDashPolicyV1 dp{};
        dp.velocity[0] = vx;
        dp.velocity[1] = vy;
        dp.velocity[2] = vz;
        dp.moveAxes[0] = hasWish ? wdx : 0.0f;
        dp.moveAxes[1] = hasWish ? wdy : 0.0f;
        dp.cameraForward[0] = std::cos(yawRad);
        dp.cameraForward[1] = std::sin(yawRad);
        dp.groundDashImpulse = m.dashImpulse;
        dp.airDashImpulse = m.dashImpulse;
        dp.downDashVerticalSpeed = m.downDashSpeed;
        dp.dashPressed =
            (dashEdge && rs.dashAvailable && rs.dashCooldownSeconds <= 0.0f) ? 1u : 0u;
        dp.downDashPressed = (downDashEdge && rs.downDashAvailable) ? 1u : 0u;
        dp.grounded = rs.grounded ? 1u : 0u;
        dp.dashAvailable = rs.dashAvailable ? 1u : 0u;
        dp.downDashAvailable = rs.downDashAvailable ? 1u : 0u;
        dp.dashEnabled = 1u;
        dp.downDashEnabled = 1u;
        MimitaHotMovement::dashPolicy(dp);
        vx = dp.outVelocity[0];
        vy = dp.outVelocity[1];
        vz = dp.outVelocity[2];
        rs.dashAvailable = dp.outDashAvailable;
        rs.downDashAvailable = dp.outDownDashAvailable;
        if (dp.outDidDash)
            rs.dashCooldownSeconds = 0.0f;
    }

    {
        GameJumpPolicyV1 jp{};
        jp.velocityZ = vz;
        jp.jumpSpeed = m.jumpSpeed;
        jp.dt = dt;
        jp.coyoteSeconds = 0.0f;
        jp.jumpBufferSeconds = 0.0f;
        jp.grounded = rs.grounded ? 1u : 0u;
        jp.jumpPressed = jumpEdge ? 1u : 0u;
        jp.jumpHeld = mi.jump ? 1u : 0u;
        jp.jumpHeldPreviously = rs.jumpHeldPreviously ? 1u : 0u;
        jp.autoBhopEnabled = 1u;
        jp.maximumAirJumps = 1u;
        jp.airJumpsLeft = rs.airJumpsLeft;
        jp.airJumpArmed = rs.jumpAirJumpArmed;
        MimitaHotMovement::jumpPolicy(jp);
        vz = jp.outVelocityZ;
        rs.grounded = jp.outGrounded;
        rs.airJumpsLeft = jp.airJumpsLeft;
        rs.jumpAirJumpArmed = jp.airJumpArmed;
    }

    {
        GameGravityV1 gv{};
        gv.velocityZ = vz;
        gv.gravityZ = -m.gravity;
        gv.maximumFallSpeed = m.maxFallSpeed;
        gv.dt = dt;
        float oz = vz;
        MimitaHotMovement::gravity(gv, oz);
        vz = oz;
    }
    if (vz < -m.maxFallSpeed)
        vz = -m.maxFallSpeed;

    MovementStateV1 st{};
    st.position[0] = tf.position[0];
    st.position[1] = tf.position[1];
    st.position[2] = tf.position[2];
    st.velocity[0] = vx;
    st.velocity[1] = vy;
    st.velocity[2] = vz;
    st.yaw = tf.yaw;
    st.radius = body.radius > 0.0f ? body.radius : 0.4f;
    st.halfHeight = body.height > 0.0f ? body.height * 0.5f : 0.9f;
    st.sizeScale = body.sizeScale;
    // Gravity is owned above; tell the capsule solver not to add its own.
    st.gravityScale = -1.0f;
    st.grounded = rs.grounded;

    if (ctx->resolveCapability)
    {
        auto move = reinterpret_cast<PhysicsMoveFn>(
            ctx->resolveCapability(ctx->host, GAME_CAP_PHYSICS_MOVE));
        if (move)
            move(ctx->host, &st, dt, GAME_PHYSICS_MOVE_HEADLESS);
    }
    else
    {
        st.position[0] += st.velocity[0] * dt;
        st.position[1] += st.velocity[1] * dt;
        st.position[2] += st.velocity[2] * dt;
    }

    rs.grounded = st.grounded;
    if (rs.grounded)
    {
        rs.airJumpsLeft = 1;
        rs.jumpAirJumpArmed = 1u;
        rs.dashAvailable = 1u;
        rs.downDashAvailable = 1u;
    }
    rs.jumpHeldPreviously = mi.jump ? 1u : 0u;
    rs.dashHeldPreviously = mi.dash ? 1u : 0u;
    rs.downDashHeldPreviously = mi.downDash ? 1u : 0u;
    rs.freezePreviously = mi.freeze ? 1u : 0u;

    GameTransformComponentV1 otf = tf;
    otf.position[0] = st.position[0];
    otf.position[1] = st.position[1];
    otf.position[2] = st.position[2];
    ctx->writeComponent(ctx->host, e, GAME_COMPONENT_TRANSFORM, &otf, sizeof(otf));

    GameVelocityComponentV1 ovl = vl;
    ovl.linear[0] = st.velocity[0];
    ovl.linear[1] = st.velocity[1];
    ovl.linear[2] = st.velocity[2];
    ctx->writeComponent(ctx->host, e, GAME_COMPONENT_VELOCITY, &ovl, sizeof(ovl));

    rs.reserved[GAME_MOVEMENT_STAMP_TICK] = tick;
    rs.reserved[GAME_MOVEMENT_STAMP_GENERATION] = (std::uint32_t)ctx->generation;
    rs.reserved[GAME_MOVEMENT_STAMP_FLAGS] = GAME_MOVEMENT_STAMP_FLAG_ACTIVE;
    ctx->writeComponent(ctx->host, e, GAME_COMPONENT_MOVEMENT_RUNTIME_STATE,
                        &rs, sizeof(rs));
}

void MIMITA_GAME_CALL actorMovementTick(void* host, std::uint64_t tick, float dt)
{
    if (!kSimulateServerActors)
        return;
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || ctx->structSize < sizeof(GameplayContextV1))
        return;
    if (!ctx->readComponent || !ctx->writeComponent || !ctx->findEntities)
        return;
    if (dt <= 0.0f)
        return;

    std::uint64_t entities[128];
    const std::uint32_t count = ctx->findEntities(
        ctx->host, 0, GAME_COMPONENT_MOVEMENT_INTENT, entities, 128);

    // Never simulate the local human here: movement.main owns it.
    std::uint64_t localEntity = 0;
    if (ctx->permanentStorage &&
        ctx->permanentStorageSize >= sizeof(GameSharedStateV1))
    {
        auto* shared = reinterpret_cast<GameSharedStateV1*>(ctx->permanentStorage);
        if (shared->magic == GAME_SHARED_MAGIC)
            localEntity = shared->localPlayerEntity;
    }

    for (std::uint32_t i = 0; i < count; ++i)
        if (entities[i] != localEntity)
            simulateOneActor(ctx, entities[i], dt, (std::uint32_t)tick);
}

} // namespace

const MimitaHotPackage::SystemRegistrar s_actorMovementRegistration{
    {gameHash("movement.actors"), GAME_DOMAIN_GAMEPLAY, 50, 0,
     actorMovementTick, "movement.actors"}};

#endif
