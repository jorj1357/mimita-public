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
* Presets live in the hot C++ registry (hot-movement-presets.h): source,
* default, heavy, retrograd_fast, counterstrike. The global active preset is a
* C++ constant; per-actor presets come from the generic ActorProfileState
* component. Config JSON is reference/comparison data, never the owner.
* Does NOT own entity storage, collision, rendering, or authority.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-movement-fired.h"
#include "hot-reload/packages/collision/collision-log.h"
#include "hot-reload/hot-movement-policy.h"
#include "hot-reload/hot-movement-preset-log.h"
#include "hot-reload/hot-movement-presets.h"
#include "hot-reload/hot-package.h"
#include "hot-reload/packages/collision/collision-abi.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace {

// movement.tuning: cold callers (server, NPC, prediction setup, validation)
// request a preset's tuning by id. The hot handler is the single authority and
// fills the full preset; JSON is never consulted.
void MIMITA_GAME_CALL onMovementTuning(void* host, const GameEventV1* event)
{
    auto* t = event ? static_cast<GameMovementTuningV1*>(event->payload) : nullptr;
    if (!t)
        return;
    const auto id = t->presetId < MimitaHotMovement::kMovementPresetCount
                        ? static_cast<MimitaHotMovement::MovementPresetId>(t->presetId)
                        : MimitaHotMovement::kActiveMovementPreset;
    const MimitaHotMovement::MovementPreset& preset =
        MimitaHotMovement::getMovementPreset(id);
    *t = preset.tuning;
    t->presetId = static_cast<std::uint32_t>(id);
    t->handled = 1u;
    t->reserved = 0u;

    static MimitaHotMovement::MovementPresetTuningLogState sTuningLog;
    const std::uint64_t tick = event->tick;
    MimitaHotMovement::movementPresetLogTuning(host, sTuningLog, id, tick, tick);

    static const char* lastLoggedMode = nullptr;
    if (lastLoggedMode != preset.name) {
        lastLoggedMode = preset.name;
        std::printf("[MOVEMENT TUNING] source=cpp preset=%s authority=shared-hot-movement\n",
                    preset.name);
    }
}

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

// The player collider list: the movement/smoothing capsule plus the head,
// torso, arms, and legs resolved from the skeleton. The caller supplies the
// generic shape description; the collision package owns the solve.
void buildPlayerCollision(
    GameplayContextV1* ctx, MovementStateV1* st, float dt, std::uint64_t entity,
    std::uint64_t tick, HotCollisionPackage::CollisionSolveV1& q)
{
    using namespace HotCollisionPackage;
    q = CollisionSolveV1{};
    q.entityId = entity;
    q.tick = tick;
    q.dt = dt;
    q.yaw = st->yaw;
    q.sizeScale = st->sizeScale > 0.0f ? st->sizeScale : 1.0f;
    q.mask = COLLISION_MASK_WORLD;
    q.flags = COLLISION_SOLVE_SPAWN_IMPACTS;
    // Identity/timing for live records. The local human is a player; the frame
    // and client tick come from the kernel context when available.
    q.actorKind = HotCollisionPackage::COLLISION_LOG_ACTOR_PLAYER;
    q.frame = ctx->tick;
    q.clientTick = ctx->tick;
    q.serverTick = 0;
    for (int i = 0; i < 3; ++i) {
        q.position[i] = st->position[i];
        q.velocity[i] = st->velocity[i];
    }

    CollisionColliderV1& capsule = q.colliders[q.colliderCount++];
    capsule.partId = COLLISION_PART_CAPSULE;
    capsule.shape = COLLISION_SHAPE_CAPSULE;
    capsule.policyId = COLLISION_POLICY_CAPSULE;
    capsule.radius = st->radius;
    capsule.halfHeight = st->halfHeight;
    for (int i = 0; i < 3; ++i)
        capsule.position[i] = st->position[i];

    if (!ctx->resolveCapability ||
        q.colliderCount >= COLLISION_MAX_COLLIDERS)
        return;
    auto rawFn = reinterpret_cast<GameSocketRawFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_SOCKET_RAW));
    if (!rawFn)
        return;

    static const struct { std::uint32_t part; const char* name; float radius; }
        kParts[] = {
            {COLLISION_PART_HEAD, "head", 0.30f},
            {COLLISION_PART_TORSO, "torso", 0.42f},
            {COLLISION_PART_LEFT_ARM, "leftArm", 0.17f},
            {COLLISION_PART_RIGHT_ARM, "rightArm", 0.17f},
            {COLLISION_PART_LEFT_LEG, "leftLeg", 0.20f},
            {COLLISION_PART_RIGHT_LEG, "rightLeg", 0.20f},
        };
    const float s = q.sizeScale;
    glm::mat4 root = glm::translate(
        glm::mat4(1.0f),
        glm::vec3(st->position[0], st->position[1], st->position[2]));
    root *= glm::rotate(glm::mat4(1.0f), glm::radians(st->yaw),
                        glm::vec3(0.0f, 0.0f, 1.0f));
    for (const auto& p : kParts) {
        if (q.colliderCount >= COLLISION_MAX_COLLIDERS)
            break;
        GameSocketRawV1 r{};
        r.entity = entity;
        r.socket = gameHash(p.name);
        if (!rawFn(ctx->host, &r) || !r.valid)
            continue;
        const glm::vec3 local(r.position[0] * s, r.position[1] * s,
                              r.position[2] * s);
        const glm::vec3 world = glm::vec3(root * glm::vec4(local, 1.0f));
        CollisionColliderV1& c = q.colliders[q.colliderCount++];
        c.partId = p.part;
        c.shape = COLLISION_SHAPE_SPHERE;
        c.policyId = COLLISION_POLICY_BODY;
        c.radius = p.radius * s;
        c.position[0] = world.x;
        c.position[1] = world.y;
        c.position[2] = world.z;
    }
}

// ── Live collision diagnostics (events.jsonl) ───────────────────────────────
// One throttled record per second for the branch that did NOT solve, so a
// missing capability or a declined solve is visible while the game runs.
struct MovementBranchLog {
    float sinceLogSeconds = 0.0f;
};
MovementBranchLog& movementBranchLog()
{
    static MovementBranchLog b;
    return b;
}

void logMovementBranch(GameplayContextV1* ctx, const char* why,
                       const MovementStateV1* st, std::uint64_t entity,
                       std::uint64_t tick)
{
    MovementBranchLog& b = movementBranchLog();
    b.sinceLogSeconds += 1.0f / 60.0f;
    if (b.sinceLogSeconds < 1.0f)
        return;
    char msg[224];
    std::snprintf(msg, sizeof(msg),
                  "branch=%s entity=%llu pos=(%.2f %.2f %.2f) vz=%.2f "
                  "hasCapability=%d",
                  why, (unsigned long long)entity, st->position[0],
                  st->position[1], st->position[2], st->velocity[2],
                  (int)(ctx && ctx->resolveCapability != nullptr));
    HotCollisionPackage::collisionLogFull(
        ctx, 3u, "COLLISION", "movement.collision", msg, why, entity, entity,
        HotCollisionPackage::COLLISION_LOG_ACTOR_PLAYER, tick, 0, tick, tick);
    b.sinceLogSeconds = 0.0f;
}

// Collision is owned by exactly one system: the collision package
// (`collision.main`). There is no second in-DLL solver; if the package is not
// available the actor keeps its plain-integrated velocity for one tick rather
// than being mutated by a competing owner.
void resolveCollisions(GameplayContextV1* ctx, MovementStateV1* st, float dt,
                       std::uint64_t entity, std::uint64_t tick)
{
    using namespace HotCollisionPackage;
    GameCollisionSolveFn fn = nullptr;
    if (ctx->resolveCapability)
        fn = reinterpret_cast<GameCollisionSolveFn>(
            ctx->resolveCapability(ctx->host, GAME_CAP_COLLISION));
    if (!fn) {
        for (int i = 0; i < 3; ++i)
            st->position[i] += st->velocity[i] * dt;
        st->grounded = 0;
        st->collided = 0;
        // The package capability is missing: no collision owner at all.
        logMovementBranch(ctx, "no_capability", st, entity, tick);
        return;
    }
    CollisionSolveV1 q;
    buildPlayerCollision(ctx, st, dt, entity, tick, q);
    // The collision package resolves capabilities from the gameplay context, so
    // it receives `ctx` (the context), not `ctx->host` (the opaque kernel host).
    fn(ctx, &q);
    if (!q.handled) {
        // Package could not solve (for example the world is not bound yet):
        // integrate plainly and state the result explicitly, so grounded is
        // never left undefined and no second owner invents a result.
        for (int i = 0; i < 3; ++i)
            st->position[i] += st->velocity[i] * dt;
        st->grounded = 0;
        st->collided = 0;
        logMovementBranch(ctx, "declined", st, entity, tick);
        return;
    }
    for (int i = 0; i < 3; ++i) {
        st->position[i] = q.outPosition[i];
        st->velocity[i] = q.outVelocity[i];
    }
    st->grounded = q.grounded;
    st->collided = (q.worldContact || q.bodyContact) ? 1u : 0u;

    // Throttled branch record: proves which owner ran and the collider count
    // that was actually sent (capsule + resolved body parts), plus the actor
    // identity and the frame/client/server tick.
    MovementBranchLog& b = movementBranchLog();
    b.sinceLogSeconds += dt;
    if (b.sinceLogSeconds >= 1.0f) {
        char msg[256];
        std::snprintf(msg, sizeof(msg),
                      "branch=solved colliders=%u parts=%u grounded=%u worldContact=%u "
                      "contacts=%u pos=(%.2f %.2f %.2f) vz=%.2f",
                      q.colliderCount, q.colliderCount > 0 ? q.colliderCount - 1 : 0,
                      q.grounded, q.worldContact, q.contactCount, q.outPosition[0],
                      q.outPosition[1], q.outPosition[2], q.outVelocity[2]);
        HotCollisionPackage::collisionLogFull(
            ctx, 2u, "COLLISION", "movement.collision", msg,
            q.grounded ? "grounded" : ((q.worldContact || q.bodyContact)
                                           ? "contact"
                                           : "no_contact"),
            entity, entity,
            HotCollisionPackage::COLLISION_LOG_ACTOR_PLAYER, ctx->tick, 0,
            ctx->tick, tick);
        b.sinceLogSeconds = 0.0f;
    }
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

void MIMITA_GAME_CALL movementMainTick(void* host, std::uint64_t tick, float dt)
{
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || ctx->structSize < sizeof(GameplayContextV1))
        return;
    if (!ctx->readComponent || !ctx->writeComponent ||
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

    bool presetFromProfile = false;
    const MimitaHotMovement::MovementPresetId presetId =
        actorMovementPreset(ctx, e, &presetFromProfile);
    const MimitaHotMovement::MovementPreset& preset =
        MimitaHotMovement::getMovementPreset(presetId);
    const GameMovementTuningV1& m = preset.tuning;
    MimitaHotMovement::movementPresetLogActor(
        ctx, e, MimitaHotMovement::MOVEMENT_LOG_ACTOR_PLAYER, presetId,
        presetFromProfile ? "actor-profile" : "active", tick, tick);

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
            sp.speedLimit = m.speedLimitEnabled ? m.speedLimit : 0.0f;
            sp.speedLimitFixed = (m.speedLimitMode == 1u) ? 1u : 0u;
            sp.airMaxWishspeed = m.airMaxWishspeed;
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
                g.groundAcceleration = m.groundAcceleration;
                g.frictionAmount = (m.walkMode == MimitaHotMovement::kWalkModeSource)
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
                // AIR: route through the ONE shared hot air-acceleration policy
                // (the same implementation the server authority uses).
                GameAirAccelerateV1 air{};
                air.velocity[0] = vx;
                air.velocity[1] = vy;
                air.wishDir[0] = wishDirX;
                air.wishDir[1] = wishDirY;
                air.wishSpeed = m.sourceAirAccelerateBugCompatible ? speed : optWish;
                air.wishspd = optWish;
                air.maxSpeed = speed;
                air.airAcceleration = m.airAcceleration;
                air.surfaceFriction = m.surfaceFriction;
                air.airSpeedGainMultiplier = m.airSpeedGainMultiplier;
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
                dp.groundDashImpulse = m.groundDashImpulse;
                dp.airDashImpulse = m.airDashImpulse;
                dp.downDashVerticalSpeed = m.downDashSpeed;
                dp.dashPressed = (dashEdge && rs.dashAvailable &&
                                  rs.dashCooldownSeconds <= 0.0f) ? 1u : 0u;
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
                if (dp.outDidDash) {
                    rs.dashAvailable = dp.outDashAvailable;
                    // No time-based cooldown: dash is restored only by touching
                    // the world (universal contact reset), per the movement spec.
                    rs.dashCooldownSeconds = 0.0f;
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
            jp.coyoteSeconds = m.coyoteSeconds;
            jp.jumpBufferSeconds = m.jumpBufferSeconds;
            // Touch anything (ground/wall/ceiling/prop) and the jump is eligible.
            const bool contactLastTick =
                (rs.reserved[GAME_MOVEMENT_STAMP_FLAGS] & 2u) != 0u;
            jp.grounded = (rs.grounded || contactLastTick) ? 1u : 0u;
            jp.jumpPressed = jumpEdge ? 1u : 0u;
            jp.jumpHeld = mi.jump ? 1u : 0u;
            jp.jumpHeldPreviously = rs.jumpHeldPreviously ? 1u : 0u;
            jp.autoBhopEnabled = m.autoBhopEnabled;
            jp.maximumAirJumps = m.maximumAirJumps;
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
        // A grounded actor rests on the ground: applying gravity every tick
        // would feed the collision kernel a downward speed each tick and make
        // it micro-bounce forever. Airborne actors get full gravity.
        if (!freezeNow && !rs.grounded) {
            GameGravityV1 gv{};
            gv.velocityZ = vz;
            gv.gravityZ = -static_cast<float>(m.gravityMagnitude);
            gv.maximumFallSpeed = m.maxFallSpeed;
            gv.dt = dt;
            float outZ = vz;
            MimitaHotMovement::gravity(gv, outZ);
            vz = outZ;
        }

        st.velocity[0] = vx;
        st.velocity[1] = vy;
        st.velocity[2] = vz;
        // Gravity is owned by the hot gravity policy above; tell the generic
        // capsule solver not to add its own (negative = already integrated).
        st.gravityScale = -1.0f;
        st.grounded = rs.grounded;
        resolveCollisions(ctx, &st, dt, e, tick);
        rs.grounded = st.grounded;
        // Universal contact reset (movement spec): touching anything restores
        // every touch-reset ability. No time-based ability cooldowns.
        const bool contactNow = (st.grounded != 0) || (st.collided != 0);
        if (contactNow) {
            rs.airJumpsLeft = 1;
            rs.jumpAirJumpArmed = 1;
            rs.dashAvailable = 1;
            rs.downDashAvailable = 1;
        }
        if (contactNow)
            rs.reserved[GAME_MOVEMENT_STAMP_FLAGS] |= 2u;
        else
            rs.reserved[GAME_MOVEMENT_STAMP_FLAGS] &= ~2u;

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

        // Publish generic "ability actually fired" facts for the animation system.
        // OR-accumulated until the hot animation policy reads and clears them.
        const std::uint32_t fired =
            (didDash ? HOT_FIRED_DASH : 0u) |
            (didDownDash ? HOT_FIRED_DOWN_DASH : 0u) |
            (freezeEdge ? HOT_FIRED_FREEZE : 0u) |
            (jumpEdge && rs.grounded ? HOT_FIRED_GROUND_JUMP : 0u) |
            (jumpEdge && !rs.grounded ? HOT_FIRED_AIR_JUMP : 0u);
        if (fired != 0u && ctx->dynamicReadComponent &&
            ctx->dynamicWriteComponent) {
            HotMovementFiredV1 acc{};
            ctx->dynamicReadComponent(ctx->host, e, HOT_MOVEMENT_FIRED_COMPONENT,
                                      &acc, sizeof(acc));
            acc.version = HOT_MOVEMENT_FIRED_VERSION;
            acc.flags |= fired;
            ctx->dynamicWriteComponent(ctx->host, e, HOT_MOVEMENT_FIRED_COMPONENT,
                                       &acc, sizeof(acc));
        }
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

// ── command: movementmode [name] ────────────────────────────────────
// Lists/reports the hot C++ presets. The global active preset is a C++ constant
// (kActiveMovementPreset); per-actor presets come from ActorProfileState.
void MIMITA_GAME_CALL movementModeCommand(void* /*host*/, const char* args)
{
    if (args && args[0] != '\0') {
        if (!MimitaHotMovement::movementPresetNameExists(args)) {
            std::printf("[MOVEMENT MODE] unknown '%s'\n", args);
            return;
        }
        std::printf("[MOVEMENT MODE] '%s' is valid; global active is '%s' "
                    "(edit kActiveMovementPreset + rebuild DLL to change)\n",
                    args, MimitaHotMovement::getActiveMovementPreset().name);
        return;
    }
    std::printf("[MOVEMENT MODE] active='%s' presets:",
                MimitaHotMovement::getActiveMovementPreset().name);
    for (std::uint32_t i = 0; i < MimitaHotMovement::kMovementPresetCount; ++i)
        std::printf(" %s", MimitaHotMovement::kMovementPresets[i].name);
    std::printf("\n");
}

const MimitaHotPackage::SystemRegistrar s_movementMain{
    {gameHash("movement.main"), GAME_DOMAIN_GAMEPLAY, 0, 0,
     movementMainTick, "movement.main"}};

const MimitaHotPackage::CommandRegistrar s_movementModeCmd{
    {"movementmode", "movementmode [name] - list/report hot C++ movement presets", 0,
     movementModeCommand}};

const MimitaHotPackage::EventRegistrar s_movementTuningRegistration{
    {GAME_EVENT_MOVEMENT_TUNING, 0, 0, onMovementTuning, "movement.tuning"}};

} // namespace

#endif
