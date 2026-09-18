// 09 17 2026
/* purpose
* Fixed-tick (postmovement.60) exact-pose physical animation runtime.
*
* For each animated actor it:
*   1. follows the hot state machine's selected action (AnimationState.v2) and
*      resolves an imported Blender clip (BlenderPhysical + Exact) when one
*      exists, else hands control back to the procedural pose generator;
*   2. samples the authored pose for exactly this simulation tick;
*   3. sweeps the active body parts + weapon marker through the world through the
*      shared `collision.main` package (slide/bounce, static world never pushed);
*   4. pushes dynamic actors and applies the rejected/redirected reaction to the
*      animating actor through `physics.impulse`;
*   5. writes the resolved pose + additive reactions to a kernel-owned dynamic
*      component (survives hot reload) and emits generic animation facts.
*
* Animation owns pose/sweep/contact/reaction. It never applies damage, ammo, or
* cooldowns: the tool owner consumes `animation.weapon-contact` through its own
* authority path. No cold ABI field/enum, no per-query heap allocation.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-action.h"
#include "hot-reload/hot-animation.h"
#include "hot-reload/hot-animation-blender.h"
#include "hot-reload/hot-animation-clips.h"
#include "hot-reload/hot-animation-physical.h"
#include "hot-reload/hot-package.h"
#include "hot-reload/packages/collision/collision-abi.h"

#include <cmath>
#include <cstdint>
#include <cstdio>

namespace {

using namespace HotPhys;

using SkeletonApplyFn = void (MIMITA_GAME_CALL *)(void*,
                                                   const GameSkeletonPoseV1*);
using CollisionSolveFn = HotCollisionPackage::GameCollisionSolveFn;
using ImpulseFn = GamePhysicsImpulseFn;
using EmitFn = void (MIMITA_GAME_CALL *)(GameplayContextV1*, const GameEventV1*);

constexpr float kTickDt = 1.0f / 60.0f;
constexpr float kDeg2Rad = 3.14159265358979323846f / 180.0f;
constexpr std::uint32_t kMaxActors = 256;
constexpr std::uint32_t kMaxTargets = 64;

// Global default mode: BlenderPhysical. A missing imported clip falls back to
// the procedural generator, so behavior is unchanged until a clip exists.
bool g_blenderPhysicalDefault = true;

// Approximate rest-pose part offsets (MiMITA Z-up, X lateral, metres). The
// marker system refines these per clip; exact skeleton metadata is a later step.
struct RestOffset {
    float x, y, z;
    float radius;
};
const RestOffset kRest[HotAnim::PartCount] = {
    {0.0f, 0.0f, 1.10f, 0.30f},   // torso
    {0.0f, 0.0f, 1.62f, 0.18f},   // head
    {0.34f, 0.0f, 1.25f, 0.14f},  // leftArm
    {-0.34f, 0.0f, 1.25f, 0.14f}, // rightArm
    {0.15f, 0.0f, 0.55f, 0.16f},  // leftLeg
    {-0.15f, 0.0f, 0.55f, 0.16f}, // rightLeg
};

float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

void rotateEulerXYZ(const float rotDeg[3], const float v[3], float out[3])
{
    const float rx = rotDeg[0] * kDeg2Rad;
    const float ry = rotDeg[1] * kDeg2Rad;
    const float rz = rotDeg[2] * kDeg2Rad;
    const float cx = std::cos(rx), sx = std::sin(rx);
    const float cy = std::cos(ry), sy = std::sin(ry);
    const float cz = std::cos(rz), sz = std::sin(rz);
    // Rz * Ry * Rx
    const float m00 = cz * cy;
    const float m01 = cz * sy * sx - sz * cx;
    const float m02 = cz * sy * cx + sz * sx;
    const float m10 = sz * cy;
    const float m11 = sz * sy * sx + cz * cx;
    const float m12 = sz * sy * cx - cz * sx;
    const float m20 = -sy;
    const float m21 = cy * sx;
    const float m22 = cy * cx;
    out[0] = m00 * v[0] + m01 * v[1] + m02 * v[2];
    out[1] = m10 * v[0] + m11 * v[1] + m12 * v[2];
    out[2] = m20 * v[0] + m21 * v[1] + m22 * v[2];
}

void yawRotate(float yawDeg, const float v[3], float out[3])
{
    const float a = yawDeg * kDeg2Rad;
    const float c = std::cos(a), s = std::sin(a);
    out[0] = c * v[0] - s * v[1];
    out[1] = s * v[0] + c * v[1];
    out[2] = v[2];
}

// World position of a part's probe: actor origin + yaw(rest + poseTrans) +
// yaw(euler(partRot, markerOffset)).
void partWorldPosition(const float actorPos[3], float yawDeg, std::uint32_t part,
                       const ActorPoseV1& pose, const float markerOffset[3],
                       float out[3])
{
    float base[3] = {kRest[part].x, kRest[part].y, kRest[part].z};
    for (int k = 0; k < 3; ++k)
        base[k] += pose.trans[part][k];
    float rotatedBase[3];
    yawRotate(yawDeg, base, rotatedBase);
    float local[3];
    rotateEulerXYZ(pose.rot[part], markerOffset, local);
    float rotatedLocal[3];
    yawRotate(yawDeg, local, rotatedLocal);
    for (int k = 0; k < 3; ++k)
        out[k] = actorPos[k] + rotatedBase[k] + rotatedLocal[k];
}

const HotAnim::BlenderMarker* findWeaponMarker(const HotAnim::BlenderClip* clip)
{
    if (!clip || !clip->markers)
        return nullptr;
    for (std::uint32_t i = 0; i < clip->markerCount; ++i)
        if ((clip->markers[i].flags & HotAnim::BLENDER_MARKER_WEAPON) != 0)
            return &clip->markers[i];
    return nullptr;
}

bool partEnabled(const PhysicalAnimationRecipeV1& r, std::uint32_t part)
{
    switch (part) {
        case HotAnim::PartHead: return r.collideHead != 0;
        case HotAnim::PartTorso: return r.collideTorso != 0;
        case HotAnim::PartLeftArm:
        case HotAnim::PartRightArm: return r.collideArms != 0;
        case HotAnim::PartLeftLeg:
        case HotAnim::PartRightLeg: return r.collideLegs != 0;
        default: return false;
    }
}

void copyPose(const HotAnim::Pose& src, ActorPoseV1& dst)
{
    dst.clear();
    dst.mask = src.mask;
    for (std::uint32_t i = 0; i < HotAnim::PartCount; ++i)
        for (int k = 0; k < 3; ++k) {
            dst.trans[i][k] = src.part[i].trans[k];
            dst.rot[i][k] = src.part[i].rot[k];
        }
}

void emitAnimationEvent(GameplayContextV1* ctx, std::uint64_t typeId,
                        std::uint64_t actor, std::uint64_t other,
                        const PhysicalContactResultV1& contact, float strength,
                        std::uint64_t tick)
{
    if (!ctx->emitEvent)
        return;
    struct EventPayloadV1 {
        std::uint64_t actorEntity;
        std::uint64_t otherEntity;
        std::uint32_t bodyPart;
        std::uint32_t response;
        float strength;
        PhysicalContactResultV1 contact;
    };
    EventPayloadV1 payload{};
    payload.actorEntity = actor;
    payload.otherEntity = other;
    payload.bodyPart = contact.bodyPart;
    payload.response = static_cast<std::uint32_t>(
        contact.dynamicActor ? PhysicalContactResponse::PushDynamicActor
                             : PhysicalContactResponse::RedirectToAnimatingActor);
    payload.strength = strength;
    payload.contact = contact;

    GameEventV1 event{};
    event.typeId = typeId;
    event.schemaHash = ANIM_EVENT_SCHEMA;
    event.payloadVersion = 1;
    event.payloadSize = sizeof(payload);
    event.sourceEntity = actor;
    event.targetEntity = other;
    event.tick = tick;
    event.payload = &payload;
    reinterpret_cast<EmitFn>(ctx->emitEvent)(ctx, &event);
}

// Sweep one probe through the shared collision package. Static world is never
// pushed; the caller turns the response into an actor reaction.
bool probeWorld(GameplayContextV1* ctx, CollisionSolveFn solve,
                const float worldPos[3], const float velocity[3], float radius,
                std::uint32_t part, std::uint64_t tick,
                PhysicalContactResultV1& out)
{
    if (!solve)
        return false;
    HotCollisionPackage::CollisionSolveV1 q{};
    q.entityId = 0;  // scratch identity: never perturb a real actor's collision memory
    q.tick = tick;
    q.dt = kTickDt;
    q.yaw = 0.0f;
    q.sizeScale = 1.0f;
    q.mask = HotCollisionPackage::COLLISION_MASK_WORLD;
    q.flags = 0;
    q.colliderCount = 1;
    HotCollisionPackage::CollisionColliderV1& c = q.colliders[0];
    c.partId = part;
    c.shape = HotCollisionPackage::COLLISION_SHAPE_SPHERE;
    c.policyId = (part == 7u) ? HotCollisionPackage::COLLISION_POLICY_WEAPON
                              : HotCollisionPackage::COLLISION_POLICY_BODY;
    c.flags = 0;
    c.position[0] = worldPos[0];
    c.position[1] = worldPos[1];
    c.position[2] = worldPos[2];
    c.radius = radius;
    c.halfHeight = 0.0f;
    c.extents[0] = c.extents[1] = c.extents[2] = 0.0f;
    c.velocity[0] = velocity[0];
    c.velocity[1] = velocity[1];
    c.velocity[2] = velocity[2];
    for (int i = 0; i < 3; ++i) {
        q.position[i] = worldPos[i];
        q.velocity[i] = velocity[i];
    }
    solve(ctx, &q);
    if (q.handled == 0)
        return false;

    const bool hit = q.worldContact != 0 || q.contactCount > 0;
    if (!hit)
        return false;

    out.hit = 1;
    out.staticWorld = 1;
    out.dynamicActor = 0;
    out.bodyPart = (part == 7u) ? 0xFFFFFFFFu : part;
    out.otherEntity = 0;
    if (q.contactCount > 0) {
        const HotCollisionPackage::CollisionContactV1& contact = q.contacts[0];
        for (int i = 0; i < 3; ++i) {
            out.contactPoint[i] = contact.point[i];
            out.normal[i] = contact.normal[i];
        }
    } else {
        // Fall back to the reversed incoming velocity as the separation normal.
        const float len = std::sqrt(velocity[0] * velocity[0] +
                                    velocity[1] * velocity[1] +
                                    velocity[2] * velocity[2]);
        const float inv = len > 1e-5f ? 1.0f / len : 0.0f;
        for (int i = 0; i < 3; ++i)
            out.normal[i] = -velocity[i] * inv;
        for (int i = 0; i < 3; ++i)
            out.contactPoint[i] = worldPos[i];
    }
    for (int i = 0; i < 3; ++i) {
        out.slideVelocity[i] = q.outVelocity[i];
        out.bounceVelocity[i] = q.outVelocity[i];
    }
    return true;
}

// Push nearby dynamic actors and apply the reaction to the animating actor.
void resolveDynamicActors(GameplayContextV1* ctx, ImpulseFn impulse,
                          std::uint64_t actor, const float partWorld[3],
                          const float partVel[3], float radius,
                          const PhysicalAnimationRecipeV1& recipe,
                          std::uint64_t tick,
                          PhysicalContactResultV1& out)
{
    if (!recipe.affectDynamicActors || !impulse || !ctx->findEntities ||
        !ctx->readComponent)
        return;
    std::uint64_t actors[kMaxTargets];
    const std::uint32_t count =
        ctx->findEntities(ctx->host, 0, GAME_COMPONENT_HEALTH, actors, kMaxTargets);
    const float speed = std::sqrt(partVel[0] * partVel[0] +
                                  partVel[1] * partVel[1] +
                                  partVel[2] * partVel[2]);
    if (speed < 0.5f)
        return;
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint64_t other = actors[i];
        if (other == 0 || other == actor)
            continue;
        GameTransformComponentV1 tf{};
        if (!ctx->readComponent(ctx->host, other, GAME_COMPONENT_TRANSFORM, &tf,
                                sizeof(tf)))
            continue;
        const float dx = tf.position[0] - partWorld[0];
        const float dy = tf.position[1] - partWorld[1];
        const float dz = tf.position[2] - partWorld[2];
        const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (dist > radius + 0.6f)
            continue;
        const float inv = dist > 1e-5f ? 1.0f / dist : 0.0f;
        const float magnitude = clampf(speed * recipe.velocityToForce, 0.0f,
                                       recipe.maximumImpulse);
        GamePhysicsImpulseV1 push{};
        push.entity = other;
        push.linear[0] = dx * inv * magnitude;
        push.linear[1] = dy * inv * magnitude;
        push.linear[2] = dz * inv * magnitude;
        impulse(ctx->host, &push);

        out.hit = 1;
        out.dynamicActor = 1;
        out.staticWorld = 0;
        out.otherEntity = other;
        out.bodyPart = 0xFFFFFFFFu;
        for (int k = 0; k < 3; ++k) {
            out.contactPoint[k] = partWorld[k];
            out.normal[k] = -dx * inv;
            out.targetImpulse[k] = push.linear[k];
            out.animatingActorImpulse[k] = -push.linear[k] * 0.5f;
        }
        return;  // one push per weapon marker per tick
    }
}

void updateReactions(const PoseReactionRecipeV1& recipe,
                     const GameVelocityComponentV1& vel,
                     const float actorImpulse[3], PoseReactionStateV1& state)
{
    // Sway follows lateral motion; impact is a decaying offset. Additive only.
    const float damp = recipe.damping > 0.0f ? recipe.damping : 12.0f;
    const float decay = clampf(1.0f - damp * kTickDt, 0.0f, 1.0f);
    state.swayVelocity[0] += (vel.linear[0] * 0.05f - state.swayOffset[0]) *
                             clampf(recipe.swayStrength, 0.0f, 4.0f);
    state.swayVelocity[1] += (vel.linear[1] * 0.05f - state.swayOffset[1]) *
                             clampf(recipe.swayStrength, 0.0f, 4.0f);
    state.swayVelocity[2] += (vel.linear[2] * 0.05f - state.swayOffset[2]) *
                             clampf(recipe.swayStrength, 0.0f, 4.0f);
    for (int k = 0; k < 3; ++k) {
        state.swayOffset[k] += state.swayVelocity[k] * kTickDt;
        state.swayVelocity[k] *= decay;
        state.impactOffset[k] = state.impactOffset[k] * decay;
    }
    if (recipe.allowImpactReaction) {
        for (int k = 0; k < 3; ++k)
            state.impactOffset[k] += actorImpulse[k] * 0.002f;
    }
    if (!recipe.allowSway) {
        for (int k = 0; k < 3; ++k) {
            state.swayOffset[k] = 0.0f;
            state.swayVelocity[k] = 0.0f;
        }
    }
}

void resolvePhysicalPose(GameplayContextV1* ctx, SkeletonApplyFn /*apply*/,
                         std::uint64_t entity, std::uint64_t tick,
                         const HotAnim::BlenderClip* clip,
                         const ActorPoseV1& previousResolved,
                         PhysicalAnimationStateV1& state)
{
    const PhysicalAnimationRecipeV1 recipe =
        physicalRecipeForAction(HOT_ACTION_SLASH);
    const PoseReactionRecipeV1 reactionRecipe = defaultReactionRecipe();

    state.resolved = state.target;
    state.flags &= ~PHYS_FLAG_BLOCKED;

    GameTransformComponentV1 tf{};
    if (!ctx->readComponent ||
        !ctx->readComponent(ctx->host, entity, GAME_COMPONENT_TRANSFORM, &tf,
                            sizeof(tf)))
        return;
    const float* actorPos = tf.position;
    const float yaw = tf.yaw;

    CollisionSolveFn solve = reinterpret_cast<CollisionSolveFn>(
        ctx->resolveCapability ? ctx->resolveCapability(
                                     ctx->host, HotCollisionPackage::GAME_CAP_COLLISION)
                               : nullptr);
    ImpulseFn impulse = reinterpret_cast<ImpulseFn>(
        ctx->resolveCapability
            ? ctx->resolveCapability(ctx->host, GAME_CAP_PHYSICS_IMPULSE)
            : nullptr);

    float actorImpulse[3] = {0.0f, 0.0f, 0.0f};
    const HotAnim::BlenderMarker* weapon = findWeaponMarker(clip);

    if (recipe.enabled) {
        for (std::uint32_t part = 0; part < HotAnim::PartCount; ++part) {
            if ((state.target.mask & (1u << part)) == 0)
                continue;
            if (!partEnabled(recipe, part))
                continue;
            const float zero[3] = {0.0f, 0.0f, 0.0f};
            // The weapon marker is a separate probe; body parts keep their own
            // radius so the two never double-count the same contact.
            const float previousRadius = kRest[part].radius;
            float prevWorld[3];
            float curWorld[3];
            partWorldPosition(actorPos, yaw, part, previousResolved, zero,
                              prevWorld);
            partWorldPosition(actorPos, yaw, part, state.target, zero, curWorld);
            float velocity[3];
            for (int k = 0; k < 3; ++k)
                velocity[k] = (curWorld[k] - prevWorld[k]) / kTickDt;
            PhysicalContactResultV1 contact{};
            if (recipe.reactToStaticWorld &&
                probeWorld(ctx, solve, curWorld, velocity, previousRadius, part,
                           tick, contact)) {
                state.flags |= PHYS_FLAG_BLOCKED;
                const float magnitude = clampf(
                    std::sqrt(velocity[0] * velocity[0] +
                              velocity[1] * velocity[1] +
                              velocity[2] * velocity[2]) *
                        recipe.velocityToForce,
                    0.0f, recipe.maximumImpulse);
                for (int k = 0; k < 3; ++k)
                    contact.animatingActorImpulse[k] =
                        contact.normal[k] * magnitude;
                for (int k = 0; k < 3; ++k)
                    actorImpulse[k] += contact.animatingActorImpulse[k];
                emitAnimationEvent(ctx, ANIM_EVENT_STATIC_REACTION, entity, 0,
                                   contact, magnitude, tick);
            }
        }

        // Weapon marker: a dedicated contact probe that carries weapon force.
        const int weaponPart = weapon ? HotAnim::partIndexFromHash(weapon->part) : -1;
        if (recipe.collideWeapon && weapon && weaponPart >= 0 &&
            (state.target.mask & (1u << (std::uint32_t)weaponPart)) != 0u) {
            {
                float prevWorld[3];
                float curWorld[3];
                partWorldPosition(actorPos, yaw, (std::uint32_t)weaponPart,
                                  previousResolved, weapon->localOffset, prevWorld);
                partWorldPosition(actorPos, yaw, (std::uint32_t)weaponPart,
                                  state.target, weapon->localOffset, curWorld);
                float velocity[3];
                for (int k = 0; k < 3; ++k)
                    velocity[k] = (curWorld[k] - prevWorld[k]) / kTickDt;
                PhysicalContactResultV1 contact{};
                if (recipe.reactToStaticWorld &&
                    probeWorld(ctx, solve, curWorld, velocity, weapon->radius,
                               7u, tick, contact)) {
                    state.flags |= PHYS_FLAG_BLOCKED;
                    const float magnitude = clampf(
                        std::sqrt(velocity[0] * velocity[0] +
                                  velocity[1] * velocity[1] +
                                  velocity[2] * velocity[2]) *
                            recipe.velocityToForce,
                        0.0f, recipe.maximumImpulse);
                    for (int k = 0; k < 3; ++k)
                        contact.animatingActorImpulse[k] =
                            contact.normal[k] * magnitude;
                    for (int k = 0; k < 3; ++k)
                        actorImpulse[k] += contact.animatingActorImpulse[k];
                    emitAnimationEvent(ctx, ANIM_EVENT_STATIC_REACTION, entity, 0,
                                       contact, magnitude, tick);
                    emitAnimationEvent(ctx, ANIM_EVENT_WEAPON_CONTACT, entity, 0,
                                       contact, magnitude, tick);
                }
                PhysicalContactResultV1 dynamicContact{};
                resolveDynamicActors(ctx, impulse, entity, curWorld, velocity,
                                     weapon->radius, recipe, tick, dynamicContact);
                if (dynamicContact.hit) {
                    for (int k = 0; k < 3; ++k)
                        actorImpulse[k] += dynamicContact.animatingActorImpulse[k];
                    emitAnimationEvent(ctx, ANIM_EVENT_ACTOR_PUSH, entity,
                                       dynamicContact.otherEntity, dynamicContact,
                                       recipe.velocityToForce, tick);
                    emitAnimationEvent(ctx, ANIM_EVENT_WEAPON_CONTACT, entity,
                                       dynamicContact.otherEntity, dynamicContact,
                                       recipe.velocityToForce, tick);
                }
            }
        }
    }

    // Static-world reaction: the animating actor receives the rejected push.
    // Scaled down because body-part sweep velocity is authored motion, not a
    // gameplay impulse; the recipe's maximumImpulse still caps the tool case.
    constexpr float kActorReactionScale = 0.25f;
    float impulseMagnitude = std::sqrt(actorImpulse[0] * actorImpulse[0] +
                                       actorImpulse[1] * actorImpulse[1] +
                                       actorImpulse[2] * actorImpulse[2]);
    if (impulseMagnitude > 1e-4f && impulse && recipe.reactToStaticWorld) {
        GamePhysicsImpulseV1 reaction{};
        reaction.entity = entity;
        const float capped = clampf(impulseMagnitude, 0.0f, recipe.maximumImpulse);
        const float scale = capped / impulseMagnitude * kActorReactionScale;
        for (int k = 0; k < 3; ++k)
            reaction.linear[k] = actorImpulse[k] * scale;
        impulse(ctx->host, &reaction);
    }

    GameVelocityComponentV1 vel{};
    if (ctx->readComponent)
        ctx->readComponent(ctx->host, entity, GAME_COMPONENT_VELOCITY, &vel,
                           sizeof(vel));
    updateReactions(reactionRecipe, vel, actorImpulse, state.reactions);

    // Additive reaction layer: never replaces the authored exact target.
    if (reactionRecipe.allowSway || reactionRecipe.allowImpactReaction) {
        for (int k = 0; k < 3; ++k)
            state.resolved.trans[HotAnim::PartTorso][k] +=
                state.reactions.swayOffset[k] + state.reactions.impactOffset[k];
        for (int k = 0; k < 3; ++k)
            state.resolved.trans[HotAnim::PartHead][k] +=
                state.reactions.impactOffset[k];
    }
}

void MIMITA_GAME_CALL physicalAnimationTick(void* host, std::uint64_t tick,
                                            float /*dt*/)
{
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || !ctx->dynamicEnumerateComponent || !ctx->dynamicReadComponent ||
        !ctx->dynamicWriteComponent || !ctx->resolveCapability)
        return;

    std::uint64_t entities[kMaxActors];
    const std::uint32_t count = ctx->dynamicEnumerateComponent(
        ctx->host, HOT_ANIMATION_STATE_COMPONENT, entities, kMaxActors);
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint64_t entity = entities[i];
        HotAnimationStateV2 anim{};
        if (!ctx->dynamicReadComponent(ctx->host, entity,
                                       HOT_ANIMATION_STATE_COMPONENT, &anim,
                                       sizeof(anim)))
            continue;
        const HotAnim::BlenderClip* clip =
            g_blenderPhysicalDefault
                ? HotAnim::findBlenderClipForAction(anim.actionId, anim.weaponKey)
                : nullptr;

        PhysicalAnimationStateV1 state{};
        const bool hasState = ctx->dynamicReadComponent(
            ctx->host, entity, HOT_PHYSICAL_ANIMATION_COMPONENT, &state,
            sizeof(state));

        if (!clip) {
            // No imported clip: hand the actor back to the procedural generator.
            if (hasState && ctx->dynamicRemoveComponent)
                ctx->dynamicRemoveComponent(ctx->host, entity,
                                           HOT_PHYSICAL_ANIMATION_COMPONENT);
            continue;
        }

        const bool restart =
            !hasState || state.animationId != clip->animationId ||
            state.actionSequence != anim.sourceEventSeq ||
            state.durationTicks != clip->durationTicks;
        ActorPoseV1 previousResolved = state.resolved;
        if (restart) {
            state = PhysicalAnimationStateV1{};
            state.version = PHYSICAL_ANIMATION_STATE_VERSION;
            state.byteSize = sizeof(PhysicalAnimationStateV1);
            state.animationId = clip->animationId;
            state.actionSequence = anim.sourceEventSeq;
            state.durationTicks = clip->durationTicks;
            state.mode = static_cast<std::uint32_t>(AnimationMode::BlenderPhysical);
            state.authority = static_cast<std::uint32_t>(PoseAuthority::Exact);
            state.flags = PHYS_FLAG_ACTIVE;
            state.currentTick = 0;
        } else if ((state.flags & PHYS_FLAG_COMPLETED) == 0) {
            const std::uint32_t lastTick =
                clip->durationTicks > 0 ? clip->durationTicks - 1 : 0;
            if (state.currentTick + 1 > lastTick) {
                state.currentTick = lastTick;
                state.flags |= PHYS_FLAG_COMPLETED;
                PhysicalContactResultV1 done{};
                emitAnimationEvent(ctx, ANIM_EVENT_POSE_COMPLETED, entity, 0, done,
                                   1.0f, tick);
            } else {
                ++state.currentTick;
            }
        }

        HotAnim::Pose target{};
        HotAnim::sampleBlenderClipAtTick(*clip, state.currentTick, target);
        copyPose(target, state.target);

        resolvePhysicalPose(ctx, nullptr, entity, tick, clip, previousResolved,
                            state);
        ctx->dynamicWriteComponent(ctx->host, entity,
                                   HOT_PHYSICAL_ANIMATION_COMPONENT, &state,
                                   sizeof(state));
    }
}

void MIMITA_GAME_CALL physicalAnimationPoseTick(void* host, std::uint64_t /*tick*/,
                                                float /*dt*/)
{
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || !ctx->dynamicEnumerateComponent || !ctx->dynamicReadComponent ||
        !ctx->resolveCapability)
        return;
    SkeletonApplyFn apply = reinterpret_cast<SkeletonApplyFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_SKELETON_APPLY));
    if (!apply)
        return;

    std::uint64_t entities[kMaxActors];
    const std::uint32_t count = ctx->dynamicEnumerateComponent(
        ctx->host, HOT_PHYSICAL_ANIMATION_COMPONENT, entities, kMaxActors);
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint64_t entity = entities[i];
        PhysicalAnimationStateV1 state{};
        if (!ctx->dynamicReadComponent(ctx->host, entity,
                                       HOT_PHYSICAL_ANIMATION_COMPONENT, &state,
                                       sizeof(state)))
            continue;
        if ((state.flags & PHYS_FLAG_ACTIVE) == 0)
            continue;
        GameSkeletonPoseV1 pose{};
        pose.entity = entity;
        pose.flags = 2;  // pose contract version 2 (radians)
        for (std::uint32_t p = 0; p < HotAnim::PartCount; ++p) {
            if ((state.resolved.mask & (1u << p)) == 0)
                continue;
            if (pose.count >= GAME_MAX_POSE_PARTS)
                break;
            GamePosePartV1& out = pose.parts[pose.count++];
            out.part = HotAnim::partHash(p);
            for (int k = 0; k < 3; ++k) {
                out.translation[k] = state.resolved.trans[p][k];
                // Library unit is degrees; the hot boundary unit is radians.
                out.rotationEuler[k] = state.resolved.rot[p][k] * kDeg2Rad;
            }
        }
        apply(ctx->host, &pose);
    }
}

void MIMITA_GAME_CALL physicalAnimCommand(void* /*host*/, const char* args)
{
    g_blenderPhysicalDefault = !(args && args[0] == '0');
    std::printf("[PHYSANIM] blender physical default = %d\n",
                (int)g_blenderPhysicalDefault);
}

} // namespace

const MimitaHotPackage::SchemaRegistrar s_physicalAnimationSchema{
    {HOT_PHYSICAL_ANIMATION_COMPONENT, gameHash("PhysicalAnimationState.v1"),
     sizeof(PhysicalAnimationStateV1), 8, GAME_COPY_RUNTIME_ONLY, GAME_NET_NONE,
     "PhysicalAnimationState", 1, 0}};
const MimitaHotPackage::SchemaRegistrar s_animationModeSchema{
    {HOT_ANIMATION_MODE_COMPONENT, gameHash("AnimationModeState.v1"),
     sizeof(AnimationModeStateV1), 4, GAME_COPY_RUNTIME_ONLY, GAME_NET_NONE,
     "AnimationModeState", 1, 0}};
const MimitaHotPackage::SystemRegistrar s_physicalAnimationSystem{
    {gameHash("hot.animation-physical"), GAME_DOMAIN_POST_MOVEMENT, 5, 0,
     physicalAnimationTick, "hot.animation-physical"}};
const MimitaHotPackage::SystemRegistrar s_physicalAnimationPoseSystem{
    {gameHash("hot.animation-physical-pose"), GAME_DOMAIN_RENDER, 1, 0,
     physicalAnimationPoseTick, "hot.animation-physical-pose"}};
const MimitaHotPackage::CommandRegistrar s_physicalAnimCommand{
    {"physanim", "physanim 1|0 - exact Blender physical pose default", 0,
     physicalAnimCommand}};
const MimitaHotPackage::CapabilityRequirementRegistrar
    s_physicalAnimationCollisionRequirement{
        HotCollisionPackage::GAME_CAP_COLLISION,
        HotCollisionPackage::GAME_SIG_COLLISION, 0};
const MimitaHotPackage::CapabilityRequirementRegistrar
    s_physicalAnimationImpulseRequirement{GAME_CAP_PHYSICS_IMPULSE,
                                          gameHash("sig.physics.impulse.v1"), 0};

#endif // MIMITA_GAME_DLL
