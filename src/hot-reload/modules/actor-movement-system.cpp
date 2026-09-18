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
#include "hot-reload/hot-movement-preset-log.h"
#include "hot-reload/hot-package.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {

// Server-side simulation toggle. While the input-receive policy adopts the
// client's validated movement (spec phase 1), the actor system must not fight
// it. Set true to return to server-authoritative simulation live.
constexpr bool kSimulateServerActors = false;
// Server NPC ownership. When true the hot system exposes actor.move.npc, which
// the cold NPC kernel calls inline after the AI writes intent, so NPC movement
// and routing are hot-reloadable. Edit live.
constexpr bool kSimulateServerNpcs = true;

using PhysicsMoveFn = void (MIMITA_GAME_CALL *)(void*, MovementStateV1*, float,
                                                std::uint32_t);

// Reads the per-actor preset from the generic ActorProfileState component
// (movementPresetHash = gameHash(preset name)). Absent/unknown -> active preset.
MimitaHotMovement::MovementPresetId actorMovementPreset(GameplayContextV1* ctx,
                                                        std::uint64_t entity,
                                                        bool* outFromProfile = nullptr)
{
    if (outFromProfile)
        *outFromProfile = false;
    if (!ctx->dynamicReadComponent)
        return MimitaHotMovement::kActiveMovementPreset;
    struct ActorProfileStateV1 {
        std::uint64_t movementPresetHash;
        std::uint64_t behaviorProfileHash;
        std::uint32_t flags;
        std::uint32_t reserved;
    };
    ActorProfileStateV1 profile{};
    if (!ctx->dynamicReadComponent(ctx->host, entity, gameHash("ActorProfileState"),
                                   &profile, sizeof(profile)) ||
        profile.movementPresetHash == 0u)
        return MimitaHotMovement::kActiveMovementPreset;
    if (outFromProfile)
        *outFromProfile = true;
    return MimitaHotMovement::movementPresetIdFromHash(profile.movementPresetHash);
}

bool simulateOneActor(GameplayContextV1* ctx, std::uint64_t e, float dt,
                      std::uint32_t tick, bool npcOnly)
{
    GameControlSourceComponentV1 cs{};
    const bool hasControl =
        ctx->readComponent(ctx->host, e, GAME_COMPONENT_CONTROL_SOURCE, &cs,
                           sizeof(cs));
    // Two disjoint ownership sets so players and NPCs never double-simulate:
    // remote-network human players (gameplay domain) and server NPCs (called
    // inline by the cold NPC kernel). An absent control component is treated as
    // a server player because cold does not always stamp it.
    if (npcOnly)
    {
        if (!hasControl || cs.source != GAME_CONTROL_SERVER_NPC)
            return false;
    }
    else
    {
        if (hasControl && cs.source != GAME_CONTROL_REMOTE_NETWORK)
            return false;
    }

    GameMovementIntentComponentV1 mi{};
    if (!ctx->readComponent(ctx->host, e, GAME_COMPONENT_MOVEMENT_INTENT, &mi,
                            sizeof(mi)))
        return false;

    GameTransformComponentV1 tf{};
    if (!ctx->readComponent(ctx->host, e, GAME_COMPONENT_TRANSFORM, &tf,
                            sizeof(tf)))
        return false;

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

    bool presetFromProfile = false;
    const MimitaHotMovement::MovementPresetId presetId =
        actorMovementPreset(ctx, e, &presetFromProfile);
    const GameMovementTuningV1& m =
        MimitaHotMovement::getMovementPreset(presetId).tuning;
    MimitaHotMovement::movementPresetLogActor(
        ctx, e,
        npcOnly ? MimitaHotMovement::MOVEMENT_LOG_ACTOR_NPC
                : MimitaHotMovement::MOVEMENT_LOG_ACTOR_REMOTE,
        presetId, presetFromProfile ? "actor-profile" : "active", tick, tick);

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
    const bool freezeNow = mi.freeze != 0;
    const bool freezeEdge = freezeNow && !rs.freezePreviously;

    // FREEZE: the ONE shared hot freeze policy (same as local prediction).
    GameFreezePolicyV1 fp{};
    fp.velocity[0] = vx;
    fp.velocity[1] = vy;
    fp.velocity[2] = vz;
    fp.dt = dt;
    fp.durationSeconds = m.freezeDurationSeconds;
    fp.freezePressed = freezeEdge ? 1u : 0u;
    fp.freezeHeld = freezeNow ? 1u : 0u;
    fp.freezeHeldPreviously = rs.freezePreviously ? 1u : 0u;
    fp.freezeEnabled = m.freezeEnabled;
    fp.freezeActive = freezeNow ? 1u : 0u;
    fp.freezeAvailable = 1u;
    fp.freezeTimerSeconds = rs.freezeTimerSeconds;
    MimitaHotMovement::freezePolicy(fp);
    rs.freezeTimerSeconds = fp.outFreezeTimerSeconds;

    if (fp.outFreezeActive != 0u) {
        vx = fp.outVelocity[0];
        vy = fp.outVelocity[1];
        vz = fp.outVelocity[2];
    } else {
        GameSpeedPolicyV1 sp{};
        sp.baseMaxSpeed = m.walkSpeed;
        sp.baseFallbackSpeed = m.walkSpeed;
        sp.sizeScale = body.sizeScale;
        sp.sizeExponent = 0.0f;
        sp.speedLimit = m.speedLimitEnabled ? m.speedLimit : 0.0f;
        sp.speedLimitFixed = (m.speedLimitMode == 1u) ? 1u : 0u;
        sp.airMaxWishspeed = m.airMaxWishspeed;
        sp.rawWishSpeed = m.walkSpeed;
        float optMax = m.walkSpeed;
        float optWish = m.walkSpeed;
        MimitaHotMovement::speedPolicy(sp, optMax, optWish);
        const float speed = optMax;

        if (rs.grounded) {
            GameGroundMoveV1 g{};
            g.velocity[0] = vx;
            g.velocity[1] = vy;
            g.wishDir[0] = wdx;
            g.wishDir[1] = wdy;
            g.wishSpeed = speed;
            g.groundAcceleration = m.groundAcceleration;
            g.frictionAmount =
                (m.walkMode == MimitaHotMovement::kWalkModeSource)
                    ? m.groundFriction
                    : m.groundFrictionAmount;
            g.stopspeed = m.stopspeed;
            g.dt = dt;
            g.hasInput = hasWish ? 1u : 0u;
            float out[2] = {vx, vy};
            MimitaHotMovement::groundMove(g, out);
            vx = out[0];
            vy = out[1];
        } else if (hasWish) {
            GameAirAccelerateV1 a{};
            a.velocity[0] = vx;
            a.velocity[1] = vy;
            a.wishDir[0] = wdx;
            a.wishDir[1] = wdy;
            a.wishSpeed = m.sourceAirAccelerateBugCompatible ? speed : optWish;
            a.wishspd = optWish;
            a.maxSpeed = speed;
            a.airAcceleration = m.airAcceleration;
            a.surfaceFriction = m.surfaceFriction;
            a.airSpeedGainMultiplier = m.airSpeedGainMultiplier;
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
            dp.groundDashImpulse = m.groundDashImpulse;
            dp.airDashImpulse = m.airDashImpulse;
            dp.downDashVerticalSpeed = m.downDashSpeed;
            dp.dashPressed =
                (dashEdge && rs.dashAvailable && rs.dashCooldownSeconds <= 0.0f)
                    ? 1u : 0u;
            dp.downDashPressed = (downDashEdge && rs.downDashAvailable) ? 1u : 0u;
            dp.grounded = rs.grounded ? 1u : 0u;
            dp.dashAvailable = rs.dashAvailable ? 1u : 0u;
            dp.downDashAvailable = rs.downDashAvailable ? 1u : 0u;
            dp.dashEnabled = m.dashEnabled;
            dp.downDashEnabled = m.downDashEnabled;
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
            jp.coyoteSeconds = m.coyoteSeconds;
            jp.jumpBufferSeconds = m.jumpBufferSeconds;
            jp.grounded = rs.grounded ? 1u : 0u;
            jp.jumpPressed = jumpEdge ? 1u : 0u;
            jp.jumpHeld = mi.jump ? 1u : 0u;
            jp.jumpHeldPreviously = rs.jumpHeldPreviously ? 1u : 0u;
            jp.autoBhopEnabled = m.autoBhopEnabled;
            jp.maximumAirJumps = m.maximumAirJumps;
            jp.airJumpsLeft = rs.airJumpsLeft;
            jp.airJumpArmed = rs.jumpAirJumpArmed;
            MimitaHotMovement::jumpPolicy(jp);
            vz = jp.outVelocityZ;
            rs.grounded = jp.outGrounded;
            rs.airJumpsLeft = jp.airJumpsLeft;
            rs.jumpAirJumpArmed = jp.airJumpArmed;
        }

        if (vz < -m.maxFallSpeed)
            vz = -m.maxFallSpeed;
    }

    if (!freezeNow && !rs.grounded)
    {
        GameGravityV1 gv{};
        gv.velocityZ = vz;
        gv.gravityZ = -static_cast<float>(m.gravityMagnitude);
        gv.maximumFallSpeed = m.maxFallSpeed;
        gv.dt = dt;
        float oz = vz;
        MimitaHotMovement::gravity(gv, oz);
        vz = oz;
    }

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
    return true;
}

void actorMovementTickImpl(void* host, std::uint64_t tick, float dt, bool npcOnly)
{
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
            simulateOneActor(ctx, entities[i], dt, (std::uint32_t)tick, npcOnly);
}

void MIMITA_GAME_CALL actorMovementTick(void* host, std::uint64_t tick, float dt)
{
    if (!kSimulateServerActors)
        return;
    actorMovementTickImpl(host, tick, dt, false);
}

// actor.move.npc: called inline by the cold NPC kernel right after the AI has
// written the movement intent, so integration happens in place and the moved
// result is visible to the same tick's post steps. Returns 1 when this module
// owns the actor; 0 lets the cold kernel integrate it instead.
std::uint32_t MIMITA_GAME_CALL npcMove(void* host, std::uint64_t entity,
                                       std::uint32_t tick, float dt)
{
    if (!kSimulateServerNpcs)
        return 0u;
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || ctx->structSize < sizeof(GameplayContextV1))
        return 0u;
    if (!ctx->readComponent || !ctx->writeComponent || dt <= 0.0f)
        return 0u;
    return simulateOneActor(ctx, entity, dt, tick, true) ? 1u : 0u;
}

} // namespace

const MimitaHotPackage::SystemRegistrar s_actorMovementRegistration{
    {gameHash("movement.actors"), GAME_DOMAIN_GAMEPLAY, 50, 0,
     actorMovementTick, "movement.actors"}};

const MimitaHotPackage::CapabilityRegistrar s_npcMoveProvider{
    {GAME_CAP_NPC_MOVE, gameHash("sig.actor.move.npc.v1"), 0,
     reinterpret_cast<void*>(&npcMove), "actor.move.npc"}};

#endif
