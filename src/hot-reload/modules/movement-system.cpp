// 09 14 2026
/* purpose
* movement.main: the single local-player movement step, owned as a hot generic
* runtime system. It self-registers through HotPackageBuilder, so this file can
* be added, edited, renamed, split, deleted, or replaced live without a new EXE
* slot. It reads the local player's components, computes a movement step, asks
* the kernel for a capsule-vs-world solve (physics.moveCapsule), and publishes
* the result through the movement-override capability (kernel applies it and
* skips the built-in step).
* It is the ONLY movement path: the kernel's built-in step no longer runs.
* Movement modes live in the C++ table below and switch all tuning at once with
* the live `movementmode` command (config JSON is reference data, not the owner).
* Does NOT own entity storage, collision, rendering, or authority.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-movement-policy.h"
#include "hot-reload/hot-package.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

// ── Movement modes: one entry changes the whole tuning set at once ─────────
// Values are ported from config/movement/movement-source.json ("source", the
// default) and config/movement/movement-default.json ("default"). C++ is the
// source of truth; the JSON files are reference data only.
struct MovementMode {
    const char* name;
    float walkSpeed;       // horizontal target speed
    float groundAccel;     // how fast ground velocity reaches target
    float airAccel;        // air control strength
    float groundFriction;  // applied when grounded and no wish input
    float gravity;         // downward acceleration magnitude
    float jumpSpeed;       // upward velocity on jump
    float maxFallSpeed;    // terminal downward speed
    float dashImpulse;     // additive horizontal dash boost
    float dashCooldown;    // seconds before the next dash
    float downDashSpeed;   // vertical velocity assigned by down-dash (negative)
    float freeFlySpeed;    // creation/free-fly speed
};

constexpr MovementMode kModes[] = {
    // source: fast Source/GoldSrc preset (active default)
    {"source",  20.0f, 20.0f, 12.0f, 3.25f, 40.0f, 15.1f, 175.0f, 20.0f, 0.5f, -50.0f, 12.0f},
    // default: instant-control MiMITA preset
    {"default", 20.0f, 55.0f, 22.0f, 1.00f, 58.0f, 18.0f, 400.0f, 100.0f, 0.5f, -100.0f, 12.0f},
};
constexpr int kModeCount = (int)(sizeof(kModes) / sizeof(kModes[0]));
constexpr int kDefaultModeIndex = 0; // source

int gModeIndex = kDefaultModeIndex;

const MovementMode& tune()
{
    if (gModeIndex < 0 || gModeIndex >= kModeCount)
        gModeIndex = kDefaultModeIndex;
    return kModes[gModeIndex];
}

// Captured each tick so terminal commands (host == nullptr) can reach shared
// state, matching the editor module's command pattern.
GameSharedStateV1* gShared = nullptr;

GameSharedStateV1* sharedState(GameplayContextV1* ctx)
{
    if (!ctx || !ctx->permanentStorage ||
        ctx->permanentStorageSize < sizeof(GameSharedStateV1))
        return nullptr;
    GameSharedStateV1* shared =
        reinterpret_cast<GameSharedStateV1*>(ctx->permanentStorage);
    return shared->magic == GAME_SHARED_MAGIC ? shared : nullptr;
}

using PhysicsMoveFn = void (MIMITA_GAME_CALL *)(void*, MovementStateV1*, float, std::uint32_t);
using EffectSpawnFn = void (MIMITA_GAME_CALL *)(void*, const GameEffectSpawnV1*);

// Collision is a generic kernel primitive resolved by id. Falls back to the
// older capsule-only solve when the primitive is unavailable.
void resolveCollisions(GameplayContextV1* ctx, MovementStateV1* st, float dt)
{
    if (ctx->resolveCapability) {
        auto move = reinterpret_cast<PhysicsMoveFn>(
            ctx->resolveCapability(ctx->host, GAME_CAP_PHYSICS_MOVE));
        if (move) {
            move(ctx->host, st, dt, GAME_PHYSICS_MOVE_FULL_PIPELINE);
            return;
        }
    }
    if (ctx->moveCapsule)
        ctx->moveCapsule(ctx->host, st, dt);
}

// Effects are one generic spawn descriptor resolved by id; movement just emits
// named kinds and the kernel maps them to pooled emitters.
void spawnEffect(GameplayContextV1* ctx, std::uint64_t kind, const float pos[3],
                 const float dir[3], float scale, float lifetime)
{
    if (!ctx->resolveCapability)
        return;
    auto fn = reinterpret_cast<EffectSpawnFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_EFFECT_SPAWN));
    if (!fn)
        return;
    GameEffectSpawnV1 d{};
    d.kind = kind;
    d.scale = scale > 0.0f ? scale : 1.0f;
    d.lifetime = lifetime;
    d.color[0] = 1.0f; d.color[1] = 1.0f; d.color[2] = 1.0f; d.color[3] = 1.0f;
    for (int i = 0; i < 3; ++i) {
        d.position[i] = pos[i];
        d.direction[i] = dir ? dir[i] : 0.0f;
    }
    fn(ctx->host, &d);
}

void MIMITA_GAME_CALL movementMainTick(void* host, std::uint64_t /*tick*/, float dt)
{
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || ctx->structSize < sizeof(GameplayContextV1))
        return;
    if (!ctx->readComponent || !ctx->writeComponent || !ctx->moveCapsule ||
        !ctx->requestMovementOverride)
        return;

    GameSharedStateV1* shared = sharedState(ctx);
    if (!shared || shared->localPlayerEntity == 0)
        return;
    gShared = shared;
    const bool createMode = (shared->modeFlags & GAME_MODE_FLAG_CREATION) != 0;

    const std::uint64_t e = shared->localPlayerEntity;

    GameTransformComponentV1 tf{};
    GameVelocityComponentV1 vl{};
    GameMovementIntentComponentV1 mi{};
    GameMovementRuntimeStateComponentV1 rs{};
    GameBodyComponentV1 body{};
    if (!ctx->readComponent(ctx->host, e, GAME_COMPONENT_TRANSFORM, &tf, sizeof(tf)))
        return;
    ctx->readComponent(ctx->host, e, GAME_COMPONENT_VELOCITY, &vl, sizeof(vl));
    ctx->readComponent(ctx->host, e, GAME_COMPONENT_MOVEMENT_INTENT, &mi, sizeof(mi));
    if (!ctx->readComponent(ctx->host, e, GAME_COMPONENT_MOVEMENT_RUNTIME_STATE,
                            &rs, sizeof(rs)) ||
        rs.version != MOVEMENT_RUNTIME_STATE_VERSION) {
        rs = GameMovementRuntimeStateComponentV1{};
        rs.version = MOVEMENT_RUNTIME_STATE_VERSION;
        rs.airJumpsLeft = 1;
        rs.jumpAirJumpArmed = 1;
        rs.dashAvailable = 1;
        rs.downDashAvailable = 1;
    }
    if (!ctx->readComponent(ctx->host, e, GAME_COMPONENT_BODY, &body, sizeof(body))) {
        body.radius = 0.4f;
        body.height = 1.8f;
        body.sizeScale = 1.0f;
    }

    const float yaw = tf.yaw;

    MovementStateV1 st{};
    st.position[0] = tf.position[0];
    st.position[1] = tf.position[1];
    st.position[2] = tf.position[2];
    st.yaw = yaw;
    st.radius = body.radius > 0.0f ? body.radius : 0.4f;
    st.halfHeight = body.height > 0.0f ? body.height * 0.5f : 0.9f;
    st.sizeScale = body.sizeScale;

    const MovementMode& m = tune();

    if (createMode) {
        // Free-fly / noclip: camera-relative movement, no gravity/collision.
        GameAimIntentComponentV1 aim{};
        ctx->readComponent(ctx->host, e, GAME_COMPONENT_AIM_INTENT, &aim, sizeof(aim));
        float f[3] = {aim.direction[0], aim.direction[1], aim.direction[2]};
        float fl = std::sqrt(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
        if (fl < 1e-4f) { f[0] = 0.0f; f[1] = 1.0f; f[2] = 0.0f; fl = 1.0f; }
        f[0] /= fl; f[1] /= fl; f[2] /= fl;
        float r[3] = {f[1], -f[0], 0.0f};
        float rl = std::sqrt(r[0] * r[0] + r[1] * r[1]);
        if (rl < 1e-4f) { r[0] = 1.0f; r[1] = 0.0f; }
        else { r[0] /= rl; r[1] /= rl; }
        const float vertical = (mi.jump ? 1.0f : 0.0f) - (mi.freeze ? 1.0f : 0.0f);
        const float speed = m.freeFlySpeed;
        st.velocity[0] = (r[0] * mi.moveX + f[0] * mi.moveY) * speed;
        st.velocity[1] = (r[1] * mi.moveX + f[1] * mi.moveY) * speed;
        st.velocity[2] = vertical * speed;
        st.position[0] += st.velocity[0] * dt;
        st.position[1] += st.velocity[1] * dt;
        st.position[2] += st.velocity[2] * dt;
        st.grounded = 0;
    } else {
        float vx = vl.linear[0];
        float vy = vl.linear[1];
        float vz = vl.linear[2];

        // movement intent is already camera-relative WORLD XY (input-poll.cpp),
        // so it must not be rotated by yaw again.
        const float wishX = mi.moveX;
        const float wishY = mi.moveY;
        const float wishLen = std::sqrt(wishX * wishX + wishY * wishY);
        const bool hasWish = mi.pressed && wishLen > 1e-4f;
        const float wishDirX = hasWish ? wishX / wishLen : 0.0f;
        const float wishDirY = hasWish ? wishY / wishLen : 0.0f;

        rs.dashCooldownSeconds = std::max(0.0f, rs.dashCooldownSeconds - dt);
        const bool freezeNow = mi.freeze != 0;
        const bool freezeEdge = freezeNow && !rs.freezePreviously;
        const bool dashEdge = mi.dash != 0 && !rs.dashHeldPreviously;
        const bool downDashEdge = mi.downDash != 0 && !rs.downDashHeldPreviously;
        const bool jumpEdge = mi.jump != 0 && !rs.jumpHeldPreviously;

        bool didDash = false;
        bool didDownDash = false;
        float dashDirX = 0.0f;
        float dashDirY = 0.0f;

        // FREEZE: route through the ONE shared hot freeze policy.
        GameFreezePolicyV1 fp{};
        fp.velocity[0] = vx;
        fp.velocity[1] = vy;
        fp.velocity[2] = vz;
        fp.dt = dt;
        fp.durationSeconds = 0.0f;
        fp.freezePressed = freezeEdge ? 1u : 0u;
        fp.freezeHeld = freezeNow ? 1u : 0u;
        fp.freezeHeldPreviously = rs.freezePreviously ? 1u : 0u;
        fp.freezeEnabled = 1u;
        fp.freezeActive = freezeNow ? 1u : 0u;
        fp.freezeAvailable = 1u;
        fp.freezeTimerSeconds = 0.0f;
        MimitaHotMovement::freezePolicy(fp);
        if (fp.outFreezeActive != 0u) {
            // Frozen: velocity suppressed by the shared policy.
            vx = fp.outVelocity[0];
            vy = fp.outVelocity[1];
            vz = fp.outVelocity[2];
        } else {
            // SPEED: one shared hot speed policy derives the effective max speed
            // (size-scale aware), the same implementation the server uses.
            GameSpeedPolicyV1 sp{};
            sp.baseMaxSpeed = m.walkSpeed;
            sp.baseFallbackSpeed = m.walkSpeed;
            sp.sizeScale = body.sizeScale;
            sp.sizeExponent = 0.0f;
            sp.speedLimit = 0.0f;
            sp.airMaxWishspeed = 0.0f;
            sp.rawWishSpeed = m.walkSpeed;
            float optMax = m.walkSpeed;
            float optWish = m.walkSpeed;
            MimitaHotMovement::speedPolicy(sp, optMax, optWish);
            const float speed = optMax;
            if (rs.grounded) {
                // GROUND: route through the ONE shared hot ground-move policy
                // (friction + acceleration; the same implementation as server).
                GameGroundMoveV1 g{};
                g.velocity[0] = vx;
                g.velocity[1] = vy;
                g.wishDir[0] = wishDirX;
                g.wishDir[1] = wishDirY;
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
            } else if (hasWish) {
                // AIR: route through the ONE shared hot air-acceleration policy
                // (the same implementation the server authority uses).
                GameAirAccelerateV1 air{};
                air.velocity[0] = vx;
                air.velocity[1] = vy;
                air.wishDir[0] = wishDirX;
                air.wishDir[1] = wishDirY;
                air.wishSpeed = speed;
                air.wishspd = speed;
                air.maxSpeed = speed;
                air.airAcceleration = m.airAccel;
                air.surfaceFriction = 1.0f;
                air.airSpeedGainMultiplier = 1.0f;
                air.dt = dt;
                air.currentSpeed = vx * wishDirX + vy * wishDirY;
                air.blendedAddSpeed = air.wishspd - air.currentSpeed;
                float out[2] = {vx, vy};
                MimitaHotMovement::airAccelerate(air, out);
                vx = out[0];
                vy = out[1];
            }

            // Dash: additive horizontal impulse, ground or air, edge-triggered.
            // Falls back to camera-forward when no WASD is held.
            // DASH / DOWN-DASH: route through the ONE shared hot dash policy.
            {
                const float inVx = vx;
                const float inVy = vy;
                const float yawRad = yaw * 0.01745329252f;
                GameDashPolicyV1 dp{};
                dp.velocity[0] = vx;
                dp.velocity[1] = vy;
                dp.velocity[2] = vz;
                dp.moveAxes[0] = hasWish ? wishDirX : 0.0f;
                dp.moveAxes[1] = hasWish ? wishDirY : 0.0f;
                dp.cameraForward[0] = std::cos(yawRad);
                dp.cameraForward[1] = std::sin(yawRad);
                dp.groundDashImpulse = m.dashImpulse;
                dp.airDashImpulse = m.dashImpulse;
                dp.downDashVerticalSpeed = m.downDashSpeed;
                dp.dashPressed = (dashEdge && rs.dashAvailable &&
                                  rs.dashCooldownSeconds <= 0.0f) ? 1u : 0u;
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
                if (dp.outDidDash) {
                    rs.dashAvailable = dp.outDashAvailable;
                    rs.dashCooldownSeconds = m.dashCooldown;
                    didDash = true;
                    const float ddx = dp.outVelocity[0] - inVx;
                    const float ddy = dp.outVelocity[1] - inVy;
                    const float dl = std::sqrt(ddx * ddx + ddy * ddy);
                    dashDirX = dl > 1e-4f ? ddx / dl : 0.0f;
                    dashDirY = dl > 1e-4f ? ddy / dl : 0.0f;
                }
                if (dp.outDidDownDash) {
                    rs.downDashAvailable = dp.outDownDashAvailable;
                    didDownDash = true;
                }
            }

            // JUMP: route through the ONE shared hot jump policy (the same
            // implementation the server uses): eligibility, air jumps, impulse.
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
            jp.jumpIntentTimerSeconds = 0.0f;
            jp.coyoteTimerSeconds = 0.0f;
            jp.airJumpsLeft = static_cast<std::int32_t>(rs.airJumpsLeft);
            jp.airJumpArmed = rs.jumpAirJumpArmed ? 1u : 0u;
            jp.airJumpLocked = 0u;
            MimitaHotMovement::jumpPolicy(jp);
            vz = jp.outVelocityZ;
            rs.grounded = jp.outGrounded;
            rs.airJumpsLeft = static_cast<std::uint32_t>(jp.airJumpsLeft);
            rs.jumpAirJumpArmed = jp.airJumpArmed;

            if (vz < -m.maxFallSpeed)
                vz = -m.maxFallSpeed;
        }

        // GRAVITY: route through the ONE shared hot gravity policy (the same
        // implementation the server uses). physics.move no longer applies it.
        if (!freezeNow) {
            GameGravityV1 gv{};
            gv.velocityZ = vz;
            gv.gravityZ = -m.gravity;
            gv.maximumFallSpeed = m.maxFallSpeed;
            gv.dt = dt;
            float outZ = vz;
            MimitaHotMovement::gravity(gv, outZ);
            vz = outZ;
        }

        st.velocity[0] = vx;
        st.velocity[1] = vy;
        st.velocity[2] = vz;
        st.gravityScale = 0.0f;
        st.grounded = rs.grounded;
        resolveCollisions(ctx, &st, dt);
        rs.grounded = st.grounded;
        if (rs.grounded) {
            rs.airJumpsLeft = 1;
            rs.jumpAirJumpArmed = 1;
            rs.dashAvailable = 1;
            rs.downDashAvailable = 1;
        }

        if (didDash) {
            const float dir[3] = {dashDirX, dashDirY, 0.0f};
            spawnEffect(ctx, gameHash("effect.dash"), st.position, dir, st.sizeScale, 0.0f);
        }
        if (didDownDash)
            spawnEffect(ctx, gameHash("effect.downDash"), st.position, nullptr, st.sizeScale, 0.0f);
        if (freezeEdge)
            spawnEffect(ctx, gameHash("effect.freeze"), st.position, nullptr, st.sizeScale, 0.0f);
        else if (freezeNow)
            spawnEffect(ctx, gameHash("effect.freezeTrail"), st.position, nullptr, st.sizeScale, 0.0f);
    }

    rs.jumpHeldPreviously = mi.jump;
    rs.dashHeldPreviously = mi.dash;
    rs.downDashHeldPreviously = mi.downDash;
    rs.freezePreviously = mi.freeze;
    ctx->writeComponent(ctx->host, e, GAME_COMPONENT_MOVEMENT_RUNTIME_STATE,
                        &rs, sizeof(rs));

    // Publish: kernel applies this transform and skips the built-in step.
    ctx->requestMovementOverride(ctx->host, 1u, st.position, st.velocity, yaw);

    // Keep components coherent for observers and other hot systems.
    GameTransformComponentV1 outTf = tf;
    outTf.position[0] = st.position[0];
    outTf.position[1] = st.position[1];
    outTf.position[2] = st.position[2];
    ctx->writeComponent(ctx->host, e, GAME_COMPONENT_TRANSFORM, &outTf, sizeof(outTf));
    GameVelocityComponentV1 outVl = vl;
    outVl.linear[0] = st.velocity[0];
    outVl.linear[1] = st.velocity[1];
    outVl.linear[2] = st.velocity[2];
    ctx->writeComponent(ctx->host, e, GAME_COMPONENT_VELOCITY, &outVl, sizeof(outVl));
}

// ── command: movementmode [name|index] ──────────────────────────────
void MIMITA_GAME_CALL movementModeCommand(void* /*host*/, const char* args)
{
    if (args && args[0] != '\0') {
        bool matched = false;
        for (int i = 0; i < kModeCount; ++i) {
            if (std::strncmp(args, kModes[i].name, std::strlen(kModes[i].name)) == 0) {
                gModeIndex = i;
                matched = true;
                break;
            }
        }
        if (!matched && args[0] >= '0' && args[0] <= '9') {
            const int requested = std::atoi(args);
            if (requested >= 0 && requested < kModeCount) {
                gModeIndex = requested;
                matched = true;
            }
        }
        if (!matched) {
            std::printf("[MOVEMENT MODE] unknown '%s'\n", args);
            return;
        }
    }
    std::printf("[MOVEMENT MODE] %s (index %d)\n", kModes[gModeIndex].name, gModeIndex);
}

const MimitaHotPackage::SystemRegistrar s_movementMain{
    {gameHash("movement.main"), GAME_DOMAIN_GAMEPLAY, 0, 0,
     movementMainTick, "movement.main"}};

const MimitaHotPackage::CommandRegistrar s_movementModeCmd{
    {"movementmode", "movementmode [source|default|0..N] - switch movement preset", 0,
     movementModeCommand}};

} // namespace

#endif
