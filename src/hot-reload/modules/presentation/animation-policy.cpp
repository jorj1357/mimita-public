// 09 16 2026
/* purpose
* Hot animation state machine. A render.frame system selects exactly one
* animation action per generic actor from generic actor state (Velocity, Health,
* MovementIntent/RuntimeState, and the generic ActorActionState facts) and
* advances AnimationState.v2. Documented precedence (highest first):
*   dead > freeze > dash/down-dash > shooting/slash/lunge > reload > equip >
*   jump/fall/land > walk > idle.
* One-shot actions persist to completion unless a strictly higher-precedence
* action interrupts; action changes restart playback and open a blend window.
* The cold renderer still owns skeleton/skinning/draw; this file owns the
* "which animation / how fast / how long" decision. No Player/Npc/Monster type.
* Owns the AnimationState.v2 schema, AnimationMemory, and the v1 -> v2
* migration. Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-action.h"
#include "hot-reload/hot-animation-clips.h"
#include "hot-reload/hot-animation.h"
#include "hot-reload/hot-movement-fired.h"
#include "hot-reload/hot-package.h"

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace {

// Deterministic v1 -> v2 migration. Missing v2 fields receive safe defaults so
// old stored/replicated state keeps working after an animation-code switch.
bool migrateAnimationV1ToV2(const void* oldState, std::size_t oldSize,
                            void* newState, std::size_t newSize)
{
    if (!oldState || !newState || oldSize < sizeof(HotAnimationStateV1) ||
        newSize < sizeof(HotAnimationStateV2))
        return false;
    const auto* v1 = static_cast<const HotAnimationStateV1*>(oldState);
    auto* v2 = static_cast<HotAnimationStateV2*>(newState);
    *v2 = HotAnimationStateV2{};
    v2->version = HOT_ANIMATION_STATE_VERSION;
    v2->byteSize = static_cast<std::uint32_t>(sizeof(HotAnimationStateV2));
    v2->actionId = v1->clipId;  // v1 clip ids map 1:1 onto v2 action ids
    v2->loop = v1->loop;
    v2->playbackTime = v1->playbackTime;
    v2->playbackRate = v1->playbackRate;
    v2->blendWeight = 1.0f;
    return true;
}

bool migrateMemoryV1ToV2(const void* oldState, std::size_t oldSize,
                         void* newState, std::size_t newSize)
{
    if (!oldState || !newState || oldSize < sizeof(HotAnimationMemoryV1) ||
        newSize < sizeof(HotAnimationMemoryV2))
        return false;
    const auto* v1 = static_cast<const HotAnimationMemoryV1*>(oldState);
    auto* v2 = static_cast<HotAnimationMemoryV2*>(newState);
    *v2 = HotAnimationMemoryV2{};
    v2->version = HOT_ANIMATION_MEMORY_VERSION;
    v2->prevFlags = v1->prevFlags;
    v2->prevGrounded = v1->prevGrounded;
    v2->prevDead = v1->prevDead;
    v2->prevLifecycleGeneration = v1->prevLifecycleGeneration;
    v2->prevMeleeAction = v1->prevMeleeAction;
    v2->prevActionId = v1->prevActionId;
    v2->sourceEventSeq = v1->sourceEventSeq;
    v2->prevHealth = v1->prevHealth;
    v2->timeSinceGrounded = v1->timeSinceGrounded;
    v2->locomotionTime = v1->locomotionTime;
    v2->prevWeaponKey = 0;
    v2->departingWeaponKey = 0;
    return true;
}

struct ActionFacts {
    bool grounded = false;
    bool dead = false;
    bool jumping = false;
    bool dashing = false;
    bool downDashing = false;
    bool freezing = false;
    bool shooting = false;
    bool justShot = false;
    bool reloading = false;
    bool equipping = false;
    bool unequipping = false;
    bool melee = false;
    bool hurt = false;
    bool moving = false;       // intent to move (not velocity)
    bool dashFired = false;    // ability actually activated this frame
    bool downDashFired = false;
    bool jumpFired = false;
    std::uint32_t meleeAction = 0;
    float speed01 = 0.0f;
    float vy = 0.0f;
    std::uint64_t weaponKey = 0;
    std::uint32_t lifecycleGeneration = 0;
};

// Higher rank wins. Matches the documented precedence order.
int actionRank(std::uint64_t a)
{
    if (a == HOT_ACTION_DEATH) return 100;
    if (a == HOT_ACTION_FREEZE) return 90;
    if (a == HOT_ACTION_DASH || a == HOT_ACTION_DOWN_DASH) return 80;
    if (a == HOT_ACTION_SHOOT || a == HOT_ACTION_SLASH ||
        a == HOT_ACTION_LUNGE) return 70;
    if (a == HOT_ACTION_JUST_SHOT) return 68;
    if (a == HOT_ACTION_RELOAD) return 60;
    if (a == HOT_ACTION_EQUIP) return 50;
    if (a == HOT_ACTION_UNEQUIP) return 48;
    if (a == HOT_ACTION_HURT) return 45;
    if (a == HOT_ACTION_JUMP || a == HOT_ACTION_FALL) return 30;
    if (a == HOT_ACTION_LAND) return 28;
    if (a == HOT_ACTION_RESPAWN) return 25;
    if (a == HOT_ACTION_WALK) return 20;
    return 10;  // idle / equipped-idle
}

bool actionIsOneShot(std::uint64_t a)
{
    switch (a) {
        case HOT_ACTION_DEATH:
        case HOT_ACTION_DASH:
        case HOT_ACTION_DOWN_DASH:
        case HOT_ACTION_SHOOT:
        case HOT_ACTION_JUST_SHOT:
        case HOT_ACTION_SLASH:
        case HOT_ACTION_LUNGE:
        case HOT_ACTION_EQUIP:
        case HOT_ACTION_UNEQUIP:
        case HOT_ACTION_LAND:
        case HOT_ACTION_HURT:
        case HOT_ACTION_RESPAWN:
            return true;
        default:
            return false;
    }
}

float blendSecondsFor(std::uint64_t a)
{
    switch (a) {
        case HOT_ACTION_DEATH:
        case HOT_ACTION_HURT:
            return 0.10f;
        case HOT_ACTION_DASH:
        case HOT_ACTION_DOWN_DASH:
        case HOT_ACTION_LAND:
            return 0.06f;
        default:
            return 0.12f;
    }
}

std::uint64_t selectAction(const ActionFacts& f, bool justLanded)
{
    (void)justLanded;
    if (f.dead) return HOT_ACTION_DEATH;
    if (f.freezing) return HOT_ACTION_FREEZE;
    if (f.dashFired) return HOT_ACTION_DASH;
    if (f.downDashFired) return HOT_ACTION_DOWN_DASH;
    // Melee actions take priority over generic shooting so a weapon that also
    // sets a shoot-effect timer (quick-hit/sword) still plays slash/lunge.
    if (f.melee)
        return f.meleeAction == 2u ? HOT_ACTION_LUNGE : HOT_ACTION_SLASH;
    if (f.shooting) return HOT_ACTION_SHOOT;
    if (f.justShot) return HOT_ACTION_JUST_SHOT;
    if (f.reloading) return HOT_ACTION_RELOAD;
    if (f.equipping) return HOT_ACTION_EQUIP;
    if (f.unequipping) return HOT_ACTION_UNEQUIP;
    if (f.hurt) return HOT_ACTION_HURT;
    if (f.jumpFired) return HOT_ACTION_JUMP;
    // Locomotion is intent-driven: walk whenever the actor intends to move,
    // otherwise idle (including while airborne). No velocity/FALL gating.
    if (f.moving) return HOT_ACTION_WALK;
    return f.weaponKey != 0 ? HOT_ACTION_EQUIPPED_IDLE : HOT_ACTION_IDLE;
}

void MIMITA_GAME_CALL animationPolicyTick(void* host, std::uint64_t /*tick*/,
                                          float dt)
{
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || !ctx->dynamicEnumerateComponent || !ctx->dynamicReadComponent ||
        !ctx->dynamicWriteComponent)
        return;

    std::uint64_t entities[256];
    const std::uint32_t count = ctx->dynamicEnumerateComponent(
        ctx->host, HOT_ANIMATION_STATE_COMPONENT, entities, 256);
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint64_t entity = entities[i];
        HotAnimationStateV2 anim{};
        if (!ctx->dynamicReadComponent(ctx->host, entity,
                                       HOT_ANIMATION_STATE_COMPONENT, &anim,
                                       sizeof(anim)))
            continue;

        ActionFacts f{};
        HotActorActionStateV1 actionState{};
        if (ctx->dynamicReadComponent(ctx->host, entity,
                                      HOT_ACTOR_ACTION_COMPONENT, &actionState,
                                      sizeof(actionState))) {
            const std::uint64_t fl = actionState.flags;
            f.grounded = (fl & HOT_ACTION_FLAG_GROUNDED) != 0;
            f.dead = (fl & HOT_ACTION_FLAG_DEAD) != 0;
            f.jumping = (fl & HOT_ACTION_FLAG_JUMPING) != 0;
            f.dashing = (fl & HOT_ACTION_FLAG_DASHING) != 0;
            f.downDashing = (fl & HOT_ACTION_FLAG_DOWN_DASH) != 0;
            f.freezing = (fl & HOT_ACTION_FLAG_FREEZING) != 0;
            f.shooting = (fl & HOT_ACTION_FLAG_SHOOTING) != 0 ||
                         actionState.shootEffectTimer > 0.0f;
            f.reloading = (fl & HOT_ACTION_FLAG_RELOADING) != 0 ||
                          actionState.isReloading != 0 ||
                          actionState.reloadTimer > 0.0f;
            f.equipping = (fl & HOT_ACTION_FLAG_EQUIPPING) != 0 ||
                          actionState.equipTimer > 0.0f;
            f.justShot = actionState.fireCooldown > 0.0f &&
                         actionState.shootEffectTimer <= 0.0f;
            f.melee = (fl & HOT_ACTION_FLAG_MELEE) != 0;
            f.meleeAction = actionState.meleeAction;
            f.weaponKey = actionState.weaponKey;
            f.lifecycleGeneration = actionState.lifecycleGeneration;
        }

        GameVelocityComponentV1 vel{};
        float speed = actionState.speed;
        if (ctx->readComponent(ctx->host, entity, GAME_COMPONENT_VELOCITY, &vel,
                               sizeof(vel))) {
            const float planar =
                std::sqrt(vel.linear[0] * vel.linear[0] +
                          vel.linear[1] * vel.linear[1]);
            if (planar > speed)
                speed = planar;
            f.vy = vel.linear[2];
        }
        GameMovementRuntimeStateComponentV1 runtime{};
        if (ctx->readComponent(ctx->host, entity,
                               GAME_COMPONENT_MOVEMENT_RUNTIME_STATE, &runtime,
                               sizeof(runtime)))
            f.grounded = runtime.grounded != 0;

        // Locomotion is intent-driven: walk whenever the actor intends to move.
        // Remote actors without a replicated intent fall back to velocity.
        bool hasIntent = false;
        GameMovementIntentComponentV1 intent{};
        if (ctx->readComponent(ctx->host, entity, GAME_COMPONENT_MOVEMENT_INTENT,
                               &intent, sizeof(intent))) {
            hasIntent = true;
            const float wish =
                std::fabs(intent.moveX) + std::fabs(intent.moveY);
            f.moving = intent.pressed != 0 && wish > 1e-3f;
        }

        // Ability animations play only on an actual activation published by hot
        // movement.main; consume the pulse after reading it.
        if (ctx->dynamicReadComponent && ctx->dynamicWriteComponent) {
            HotMovementFiredV1 fired{};
            if (ctx->dynamicReadComponent(ctx->host, entity,
                                          HOT_MOVEMENT_FIRED_COMPONENT, &fired,
                                          sizeof(fired)) &&
                fired.flags != 0u) {
                f.dashFired = (fired.flags & HOT_FIRED_DASH) != 0;
                f.downDashFired = (fired.flags & HOT_FIRED_DOWN_DASH) != 0;
                f.jumpFired = (fired.flags & (HOT_FIRED_GROUND_JUMP |
                                              HOT_FIRED_AIR_JUMP)) != 0;
                fired.flags = 0;
                ctx->dynamicWriteComponent(ctx->host, entity,
                                           HOT_MOVEMENT_FIRED_COMPONENT, &fired,
                                           sizeof(fired));
            }
        }

        GameHealthComponentV1 hp{};
        int healthCurrent = 0;
        bool hasHealth = false;
        if (ctx->readComponent(ctx->host, entity, GAME_COMPONENT_HEALTH, &hp,
                               sizeof(hp))) {
            f.dead = f.dead || hp.dead != 0;
            healthCurrent = hp.current;
            hasHealth = true;
        }

        constexpr float kWalkSpeedRef = 6.0f;
        f.speed01 = speed / kWalkSpeedRef;
        if (f.speed01 < 0.0f) f.speed01 = 0.0f;
        if (f.speed01 > 1.5f) f.speed01 = 1.5f;
        if (!hasIntent)
            f.moving = f.speed01 > 0.08f;  // remote fallback: velocity

        HotAnimationMemoryV2 mem{};
        mem.version = HOT_ANIMATION_MEMORY_VERSION;
        if (!ctx->dynamicReadComponent(ctx->host, entity,
                                       HOT_ANIMATION_MEMORY_COMPONENT, &mem,
                                       sizeof(mem))) {
            mem = HotAnimationMemoryV2{};
            mem.version = HOT_ANIMATION_MEMORY_VERSION;
            mem.prevGrounded = f.grounded ? 1u : 0u;
            mem.prevLifecycleGeneration = f.lifecycleGeneration;
            mem.prevHealth = healthCurrent;
            mem.prevWeaponKey = f.weaponKey;
        }
        if (hasHealth && healthCurrent < static_cast<int>(mem.prevHealth))
            f.hurt = true;
        // Tool removal edge: remember the departing tool so its unequip phase
        // keeps resolving for the whole one-shot, not just this tick.
        if (f.weaponKey == 0 && mem.prevWeaponKey != 0) {
            f.unequipping = true;
            mem.departingWeaponKey = mem.prevWeaponKey;
        }

        const bool justLanded = f.grounded && mem.prevGrounded == 0;
        const bool wasDead = mem.prevDead != 0 || anim.actionId == HOT_ACTION_DEATH;
        const bool genChanged =
            mem.prevLifecycleGeneration != f.lifecycleGeneration;

        std::uint64_t chosen = selectAction(f, justLanded);
        const bool forceRespawn = genChanged && wasDead && !f.dead;
        if (forceRespawn)
            chosen = HOT_ACTION_RESPAWN;

        // One-shot persistence: a one-shot action keeps playing unless a
        // strictly higher-precedence action wants the body. A forced respawn
        // bypasses it so the death hold cannot block the respawn clip.
        bool changed = (chosen != anim.actionId);
        if (!forceRespawn && changed && actionIsOneShot(anim.actionId)) {
            const float dur = HotAnim::actionClip(anim.actionId).duration;
            const bool stillPlaying = anim.playbackTime < dur;
            if (stillPlaying && actionRank(chosen) <= actionRank(anim.actionId)) {
                chosen = anim.actionId;
                changed = false;
            }
        }

        if (changed) {
            anim.actionId = chosen;
            anim.playbackTime = 0.0f;
            anim.actionPhase = 0;
            anim.blendWeight = 0.0f;
            anim.blendDuration = blendSecondsFor(chosen);
            anim.flags |= HOT_ANIM_FLAG_BLENDING;
            ++mem.sourceEventSeq;
        } else {
            const float rate = anim.playbackRate > 0.0f ? anim.playbackRate : 1.0f;
            anim.playbackTime += dt * rate;
            if (anim.blendDuration > 1e-4f && anim.blendWeight < 1.0f)
                anim.blendWeight += dt / anim.blendDuration;
            if (anim.blendWeight >= 1.0f) {
                anim.blendWeight = 1.0f;
                anim.flags &= ~HOT_ANIM_FLAG_BLENDING;
                anim.blendDuration = 0.0f;
            }
        }

        const bool oneShot = actionIsOneShot(anim.actionId);
        const float dur = HotAnim::actionClip(anim.actionId).duration;
        anim.loop = oneShot ? 0u : 1u;
        anim.actionPhase = (oneShot && anim.playbackTime >= dur) ? 1u : 0u;
        anim.weaponKey = f.weaponKey;
        anim.lifecycleGeneration = f.lifecycleGeneration;
        anim.sourceEventSeq = mem.sourceEventSeq;
        anim.version = HOT_ANIMATION_STATE_VERSION;
        anim.byteSize = static_cast<std::uint32_t>(sizeof(HotAnimationStateV2));

        ctx->dynamicWriteComponent(ctx->host, entity,
                                   HOT_ANIMATION_STATE_COMPONENT, &anim,
                                   sizeof(anim));

        mem.prevFlags = actionState.flags;
        mem.prevGrounded = f.grounded ? 1u : 0u;
        mem.prevDead = f.dead ? 1u : 0u;
        mem.prevLifecycleGeneration = f.lifecycleGeneration;
        mem.prevMeleeAction = f.meleeAction;
        mem.prevActionId = anim.actionId;
        mem.prevWeaponKey = f.weaponKey;
        if (hasHealth)
            mem.prevHealth = healthCurrent;
        mem.locomotionTime += dt;
        if (mem.locomotionTime > 1000.0f)
            mem.locomotionTime -= 1000.0f;
        mem.version = HOT_ANIMATION_MEMORY_VERSION;
        ctx->dynamicWriteComponent(ctx->host, entity,
                                   HOT_ANIMATION_MEMORY_COMPONENT, &mem,
                                   sizeof(mem));
    }
}

const MimitaHotPackage::SchemaRegistrar s_animationStateSchema{
    {HOT_ANIMATION_STATE_COMPONENT, gameHash("AnimationState.v2"),
     sizeof(HotAnimationStateV2), 8, GAME_COPY_RUNTIME_ONLY, GAME_NET_ALL,
     "AnimationState", 2, 0}};
// Generic action facts, replicated so every client can reconstruct the same
// animation. The cold bridge registers the identical schema; this keeps the
// server (no render projection) able to replicate it.
const MimitaHotPackage::SchemaRegistrar s_actionStateSchema{
    {HOT_ACTOR_ACTION_COMPONENT, gameHash("ActorActionState.v1"),
     sizeof(HotActorActionStateV1), 8, GAME_COPY_RUNTIME_ONLY, GAME_NET_ALL,
     "ActorActionState", HOT_ACTION_STATE_VERSION, 0}};
// Local-only state-machine memory; deterministic and reconstructed per client.
const MimitaHotPackage::SchemaRegistrar s_animationMemorySchema{
    {HOT_ANIMATION_MEMORY_COMPONENT, gameHash("AnimationMemory.v2"),
     sizeof(HotAnimationMemoryV2), 8, GAME_COPY_RUNTIME_ONLY, GAME_NET_NONE,
     "AnimationMemory", HOT_ANIMATION_MEMORY_VERSION, 0}};
// Local-only ability-fired pulse, written by hot movement.main and consumed by
// this system. Never replicated.
const MimitaHotPackage::SchemaRegistrar s_movementFiredSchema{
    {HOT_MOVEMENT_FIRED_COMPONENT, gameHash("MovementFired.v1"),
     sizeof(HotMovementFiredV1), 8, GAME_COPY_RUNTIME_ONLY, GAME_NET_NONE,
     "MovementFired", HOT_MOVEMENT_FIRED_VERSION, 0}};
const MimitaHotPackage::MigrationRegistrar s_animationStateMigration{
    HOT_ANIMATION_STATE_COMPONENT, 1, 2,
    reinterpret_cast<void*>(&migrateAnimationV1ToV2)};
const MimitaHotPackage::MigrationRegistrar s_animationMemoryMigration{
    HOT_ANIMATION_MEMORY_COMPONENT, 1, 2,
    reinterpret_cast<void*>(&migrateMemoryV1ToV2)};
const MimitaHotPackage::SystemRegistrar s_animationPolicySystem{
    {gameHash("hot.animation-policy"), GAME_DOMAIN_RENDER, 1, 0,
     animationPolicyTick, "hot.animation-policy"}};

} // namespace

#endif
