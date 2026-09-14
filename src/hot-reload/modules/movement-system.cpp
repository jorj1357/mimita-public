// 09 14 2026
/* purpose
* movement.main: the built-in movement step reimplemented as a hot generic
* runtime system. It self-registers through HotPackageBuilder, so this file can
* be added, edited, renamed, split, deleted, or replaced live without a new EXE
* slot. It reads the local player's components, computes a movement step, asks
* the kernel for a capsule-vs-world solve (physics.moveCapsule), and publishes
* the result through the movement-override capability (kernel applies it and
* skips the built-in step).
* Opt-in via `hotmovement 1` until parity with the built-in step is proven.
* Does NOT own entity storage, collision, rendering, or authority.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace {

// Hot-editable movement tuning (edit + save to change movement live).
struct MovementTuning {
    float walkSpeed = 7.0f;
    float groundAccel = 60.0f;
    float airAccel = 12.0f;
    float gravity = 22.0f;
    float jumpSpeed = 9.0f;
    float groundFriction = 10.0f;
    float maxFallSpeed = 40.0f;
    float freeFlySpeed = 12.0f;
};
const MovementTuning kTune{};

// Grounded has no component yet; carry it per local entity across ticks.
std::uint64_t gGroundedEntity = 0;
bool gGrounded = false;

GameSharedStateV1* sharedState(GameplayContextV1* ctx)
{
    if (!ctx || !ctx->permanentStorage ||
        ctx->permanentStorageSize < sizeof(GameSharedStateV1))
        return nullptr;
    GameSharedStateV1* shared =
        reinterpret_cast<GameSharedStateV1*>(ctx->permanentStorage);
    return shared->magic == GAME_SHARED_MAGIC ? shared : nullptr;
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
    // Create mode = free-fly (always). Otherwise the hot step is opt-in until
    // it reaches parity with the built-in step.
    const bool createMode = (shared->modeFlags & GAME_MODE_FLAG_CREATION) != 0;
    const bool hotMove = (shared->modeFlags & GAME_MODE_FLAG_HOT_MOVEMENT) != 0;
    if (!createMode && !hotMove)
        return;

    const std::uint64_t e = shared->localPlayerEntity;

    GameTransformComponentV1 tf{};
    GameVelocityComponentV1 vl{};
    GameMovementIntentComponentV1 mi{};
    GameBodyComponentV1 body{};
    if (!ctx->readComponent(ctx->host, e, GAME_COMPONENT_TRANSFORM, &tf, sizeof(tf)))
        return;
    ctx->readComponent(ctx->host, e, GAME_COMPONENT_VELOCITY, &vl, sizeof(vl));
    ctx->readComponent(ctx->host, e, GAME_COMPONENT_MOVEMENT_INTENT, &mi, sizeof(mi));
    if (!ctx->readComponent(ctx->host, e, GAME_COMPONENT_BODY, &body, sizeof(body))) {
        body.radius = 0.4f;
        body.height = 1.8f;
        body.sizeScale = 1.0f;
    }

    if (gGroundedEntity != e) {
        gGrounded = false;
        gGroundedEntity = e;
    }

    const float yaw = tf.yaw;
    const float cy = std::cos(yaw);
    const float sy = std::sin(yaw);
    // Engine is Z-up; forward/right on the XY plane from yaw.
    const float fx = sy, fy = cy;
    const float rx = cy, ry = -sy;

    MovementStateV1 st{};
    st.position[0] = tf.position[0];
    st.position[1] = tf.position[1];
    st.position[2] = tf.position[2];
    st.yaw = yaw;
    st.radius = body.radius > 0.0f ? body.radius : 0.4f;
    st.halfHeight = body.height > 0.0f ? body.height * 0.5f : 0.9f;
    st.sizeScale = body.sizeScale;

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
        const float speed = kTune.freeFlySpeed;
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
        const float wishX = rx * mi.moveX + fx * mi.moveY;
        const float wishY = ry * mi.moveX + fy * mi.moveY;
        const float wishLen = std::sqrt(wishX * wishX + wishY * wishY);
        const float speed = kTune.walkSpeed;
        float desiredVx = 0.0f;
        float desiredVy = 0.0f;
        if (mi.pressed && wishLen > 1e-4f) {
            desiredVx = wishX / wishLen * speed;
            desiredVy = wishY / wishLen * speed;
        }
        const float accel = gGrounded ? kTune.groundAccel : kTune.airAccel;
        const float blend = std::min(1.0f, accel * dt / std::max(speed, 1e-3f));
        vx += (desiredVx - vx) * blend;
        vy += (desiredVy - vy) * blend;
        if (gGrounded && !mi.pressed) {
            const float friction = std::max(0.0f, 1.0f - kTune.groundFriction * dt);
            vx *= friction;
            vy *= friction;
        }
        if (gGrounded && mi.jump) {
            vz = kTune.jumpSpeed;
            gGrounded = false;
        } else {
            vz -= kTune.gravity * dt;
            if (vz < -kTune.maxFallSpeed)
                vz = -kTune.maxFallSpeed;
        }
        st.velocity[0] = vx;
        st.velocity[1] = vy;
        st.velocity[2] = vz;
        st.grounded = gGrounded ? 1u : 0u;
        ctx->moveCapsule(ctx->host, &st, dt);
        gGrounded = st.grounded != 0;
    }

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

const MimitaHotPackage::SystemRegistrar s_movementMain{
    {gameHash("movement.main"), GAME_DOMAIN_GAMEPLAY, 0, 0,
     movementMainTick, "movement.main"}};

} // namespace

#endif
