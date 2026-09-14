// 09 14 2026
/* purpose
* A tool + projectile behavior unknown when mimita.exe started. Registers a
* tool-use behavior (banana.launcher) and a projectile-impact behavior
* (banana.projectile) through the generic combat router. The tool owns its use,
* creates a tool entity, records package dynamic state, links ownership with a
* generic relationship, and emits a generic fact. No kernel enum/switch/slot.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"
#include "hot-reload/hot-projectile.h"

#include <cmath>
#include <cstdint>
#include <cstdio>

namespace {

const std::uint64_t kBananaTool = gameHash("banana.launcher");
const std::uint64_t kBananaProjectile = gameHash("banana.projectile");
const std::uint64_t kBananaState = gameHash("BananaLauncherState");
const std::uint64_t kOwnsToolRel = gameHash("relationship.owns-tool");
const std::uint64_t kFiredEvent = gameHash("banana.launcher.fired");

struct BananaLauncherState {
    std::int32_t shotsFired;
    float power;
    std::uint64_t toolEntity;
};

void MIMITA_GAME_CALL bananaUse(const ToolUsePolicyV1* use, GameplayContextV1* ctx)
{
    if (!use || !ctx)
        return;
    // The tool owns its use: suppress the built-in fire path entirely.
    auto* mutableUse = const_cast<ToolUsePolicyV1*>(use);
    mutableUse->outFire = 0;
    mutableUse->ammoCost = 0;
    mutableUse->handled = 1;

    if (!ctx->matchCurrent || !ctx->dynamicReadComponent || !ctx->dynamicWriteComponent)
        return;
    std::uint64_t match = 0;
    ctx->matchCurrent(ctx->host, &match);
    if (match == 0)
        return;

    BananaLauncherState state{};
    if (!ctx->dynamicReadComponent(ctx->host, match, kBananaState, &state, sizeof(state)))
        state = BananaLauncherState{0, 2.0f, 0};

    if (state.toolEntity == 0 && ctx->entityCreate) {
        std::uint64_t toolEntity = 0;
        if (ctx->entityCreate(ctx->host, 0u /* server */, &toolEntity) && toolEntity != 0) {
            state.toolEntity = toolEntity;
            if (ctx->relationshipAdd && use->userEntity != 0)
                ctx->relationshipAdd(ctx->host, kOwnsToolRel, use->userEntity,
                                     toolEntity, kBananaTool);
        }
    }

    state.shotsFired += 1;
    ctx->dynamicWriteComponent(ctx->host, match, kBananaState, &state, sizeof(state));

    // Composition-driven authoritative spawn: the projectile is an entity with
    // package dynamic state; the hot `projectiles.60` system owns its lifecycle.
    if (ctx->entityCreate && ctx->dynamicWriteComponent) {
        float dx = use->direction[0], dy = use->direction[1], dz = use->direction[2];
        const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (len < 0.001f) { dx = 1.0f; dy = 0.0f; dz = 0.0f; }
        else { dx /= len; dy /= len; dz /= len; }
        std::uint64_t projectileEntity = 0;
        if (ctx->entityCreate(ctx->host, 0u /* server */, &projectileEntity) &&
            projectileEntity != 0) {
            HotProjectileStateV1 proj{};
            proj.position[0] = use->origin[0];
            proj.position[1] = use->origin[1];
            proj.position[2] = use->origin[2];
            proj.velocity[0] = dx * 18.0f;
            proj.velocity[1] = dy * 18.0f;
            proj.velocity[2] = dz * 18.0f;
            proj.gravity = 18.0f;
            proj.lifetime = 4.0f;
            proj.radius = 0.2f;
            proj.impactDamage = 25.0f;
            proj.ownerEntity = use->userEntity;
            proj.toolEntity = state.toolEntity;
            proj.typeId = kBananaProjectile;
            proj.flags = HOT_PROJECTILE_EXPLODE_ON_ACTOR |
                         HOT_PROJECTILE_EXPLODE_ON_WORLD |
                         HOT_PROJECTILE_EXPLODE_ON_LIFETIME;
            ctx->dynamicWriteComponent(ctx->host, projectileEntity,
                                       HOT_PROJECTILE_COMPONENT, &proj, sizeof(proj));
            if (ctx->relationshipAdd && state.toolEntity != 0)
                ctx->relationshipAdd(ctx->host, gameHash("relationship.fired-projectile"),
                                     state.toolEntity, projectileEntity, kBananaProjectile);
            std::printf("[BANANA.LAUNCHER] spawned hot projectile entity=%llu\n",
                        (unsigned long long)projectileEntity);
        }
    }

    if (ctx->emitEvent) {
        GameEventV1 event{};
        event.typeId = kFiredEvent;
        event.schemaHash = gameHash("banana.launcher.fired.v1");
        event.payloadVersion = 1;
        event.sourceEntity = state.toolEntity;
        event.tick = use->tick;
        using EmitFn = void (MIMITA_GAME_CALL *)(GameplayContextV1*, const GameEventV1*);
        reinterpret_cast<EmitFn>(ctx->emitEvent)(ctx, &event);
    }
    std::printf("[BANANA.LAUNCHER] use #%d power=%.1f tool=%llu\n",
                state.shotsFired, state.power,
                (unsigned long long)state.toolEntity);
}

void MIMITA_GAME_CALL bananaImpact(const ProjectileImpactPolicyV1* impact,
                                   GameplayContextV1* ctx)
{
    if (!impact || !ctx)
        return;
    auto* mutableImpact = const_cast<ProjectileImpactPolicyV1*>(impact);
    mutableImpact->outExplode = 1;

    // Authoritative damage through the generic server-context primitive.
    if (impact->victimEntity != 0 && ctx->resolveCapability) {
        auto apply = reinterpret_cast<GameDamageApplyFn>(
            ctx->resolveCapability(ctx->host, GAME_CAP_DAMAGE_APPLY));
        if (apply) {
            GameDamageApplyV1 request{};
            request.victimEntity = impact->victimEntity;
            request.sourceEntity = impact->ownerEntity;
            request.amount = 40;
            request.sourceKind = GAME_DAMAGE_SOURCE_EXPLOSION;
            apply(ctx->host, &request);
            std::printf("[BANANA.LAUNCHER] applied damage victim=%llu applied=%u killed=%u\n",
                        (unsigned long long)impact->victimEntity,
                        (unsigned)request.applied, (unsigned)request.killed);
        }
    }
    std::printf("[BANANA.LAUNCHER] projectile impact hitKind=%u victim=%u\n",
                (unsigned)impact->hitKind, (unsigned)impact->victimId);
}

// Binding target: an equipped tool entity can declare
// `on.primary-use -> banana.launcher.use`, making this the per-entity owner of
// the use instead of a global tool-hash router.
void MIMITA_GAME_CALL bananaBindingUse(void* host, const GameEventV1* event)
{
    auto* use = event ? static_cast<ToolUsePolicyV1*>(event->payload) : nullptr;
    bananaUse(use, static_cast<GameplayContextV1*>(host));
}

} // namespace

const MimitaHotPackage::ToolBehaviorRegistrar s_bananaTool{kBananaTool, bananaUse};
const MimitaHotPackage::ProjectileBehaviorRegistrar s_bananaProjectile{
    kBananaProjectile, bananaImpact};
const MimitaHotPackage::SchemaRegistrar s_bananaState{
    {kBananaState, gameHash("BananaLauncherState.v1"), sizeof(BananaLauncherState), 8,
     GAME_COPY_AUTHORING, 0, "BananaLauncherState", 1, 0}};
const MimitaHotPackage::EventRegistrar s_bananaBindingEvent{
    {gameHash("banana.launcher.use"), 0, 0, bananaBindingUse,
     "banana.launcher.use"}};

#endif
