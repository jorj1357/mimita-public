// 09 14 2026
/* purpose
* Generic combat policy routers. The kernel emits one generic fact per tool use
* and per projectile impact; these handlers look up the runtime-registered
* behavior for the fact's key and dispatch to it. A tool/projectile with no
* registered behavior leaves `handled = 0`, so the cold kernel path owns it.
* Adding a tool or projectile behavior = adding a file; no kernel enum, switch,
* or ABI change.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"
#include "hot-reload/hot-tool-action.h"
#include "hot-reload/hot-tool-visual.h"

#include <cstdio>

namespace {

// Resolve the shared behavior for a tool use. Priority:
//   1. a behavior registered directly under the runtime key (behaviorId == key)
//   2. the tool definition's named behaviorId (many tools share one behavior)
//   3. the legacy per-tool behavior table
// Returns null when no hot behavior owns the use, so the cold path runs.
HotToolUseFn resolveToolUseBehavior(std::uint64_t key, const char** outSource)
{
    if (outSource)
        *outSource = "none";
    if (key == 0)
        return nullptr;
    if (HotToolUseFn fn = HotPackageBuilder::instance().findBehavior(key)) {
        if (outSource)
            *outSource = "behavior-id";
        return fn;
    }
    // Resolve the recipe for hash keys, or the projectile recipe for numeric
    // network keys (5=rocket, 7=grenade) so definitions dispatch by behaviorId.
    const ToolVisualRecipeV1* recipe = findToolVisual(key);
    if (!recipe)
        recipe = findProjectileVisual(key);
    if (recipe && recipe->definition.behaviorId != 0) {
        if (HotToolUseFn fn = HotPackageBuilder::instance().findBehavior(
                recipe->definition.behaviorId)) {
            if (outSource)
                *outSource = "definition.behaviorId";
            return fn;
        }
    }
    if (HotToolUseFn fn = HotPackageBuilder::instance().findToolBehavior(key)) {
        if (outSource)
            *outSource = "per-tool";
        return fn;
    }
    return nullptr;
}

void MIMITA_GAME_CALL onToolUse(void* host, const GameEventV1* event)
{
    auto* context = static_cast<GameplayContextV1*>(host);
    auto* use = event ? static_cast<ToolUsePolicyV1*>(event->payload) : nullptr;
    if (!context || !use)
        return;
    const std::uint64_t key = use->toolId != 0 ? use->toolId : use->toolNetworkId;
    const char* source = "none";
    HotToolUseFn behavior = resolveToolUseBehavior(key, &source);
    if (!behavior) {
        // No hot owner: the cold fire path keeps ownership (handled stays 0).
        char msg[GAME_LOG_MESSAGE];
        std::snprintf(msg, sizeof(msg), "tool=%llu no hot behavior; cold owns",
                      (unsigned long long)key);
        toolLogEvent(context, 1, "tool.route", msg, "cold", use->toolEntity,
                     use->userEntity, 1, use->tick);
        return;
    }
    // Phase-0 execution opt-in. A definition that names a behavior is not yet
    // migrated until its flag says so; until then the cold attack path stays
    // authoritative. Definitions found by hash key or numeric family id.
    const ToolVisualRecipeV1* recipe = findToolVisual(key);
    if (!recipe)
        recipe = findToolVisualByNetworkId(key);
    if (recipe && recipe->definition.behaviorId != 0 &&
        (recipe->definition.toolFlags & TOOL_FLAG_OWNS_EXECUTION) == 0) {
        char msg[GAME_LOG_MESSAGE];
        std::snprintf(msg, sizeof(msg),
                      "tool=%llu definition has not opted into hot execution; "
                      "cold owns",
                      (unsigned long long)key);
        toolLogEvent(context, 1, "tool.route", msg, "cold-not-migrated",
                     use->toolEntity, use->userEntity, 1, use->tick);
        return;  // handled stays 0 so the cold path runs
    }

    use->handled = 1;
    use->outFire = use->baseFire;
    {
        char msg[GAME_LOG_MESSAGE];
        std::snprintf(msg, sizeof(msg),
                      "tool=%llu behavior resolved by %s (tick=%u)",
                      (unsigned long long)key, source, (unsigned)use->tick);
        toolLogEvent(context, 1, "tool.route", msg, "hot", use->toolEntity,
                     use->userEntity, 1, use->tick);
    }
    behavior(use, context);

    // Generic handling record: the actor's action was handled by hot code this
    // tick, so the cold legacy fallback can skip without knowing any weapon
    // category. No tool-specific kernel query exists.
    if (context->dynamicWriteComponent && use->userEntity != 0) {
        struct ActorActionStateV1 {
            std::uint64_t lastHandledTick;
            std::uint32_t handled;
            std::uint32_t reserved;
        };
        ActorActionStateV1 state{};
        state.lastHandledTick = use->tick;
        state.handled = 1;
        context->dynamicWriteComponent(context->host, use->userEntity,
                                       gameHash("ActorActionState"), &state,
                                       sizeof(state));
    }
}

void MIMITA_GAME_CALL onProjectileImpact(void* host, const GameEventV1* event)
{
    auto* context = static_cast<GameplayContextV1*>(host);
    auto* impact = event ? static_cast<ProjectileImpactPolicyV1*>(event->payload) : nullptr;
    if (!context || !impact)
        return;
    const std::uint64_t key = impact->projectileTypeId != 0
        ? impact->projectileTypeId : impact->weaponNetworkId;
    HotProjectileImpactFn behavior =
        HotPackageBuilder::instance().findProjectileBehavior(key);
    if (!behavior)
        return;  // no hot behavior: cold per-type flags own the impact
    impact->handled = 1;
    impact->outExplode = 1;
    behavior(impact, context);
}

} // namespace

const MimitaHotPackage::EventRegistrar s_combatToolPrimary{
    {gameHash("tool.primary-use"), gameHash("tool.use.v1"), 0, onToolUse,
     "combat.tool-primary-use"}};
const MimitaHotPackage::EventRegistrar s_combatToolAlt{
    {gameHash("tool.alt-use"), gameHash("tool.use.v1"), 0, onToolUse,
     "combat.tool-alt-use"}};
const MimitaHotPackage::EventRegistrar s_combatProjectileImpact{
    {gameHash("projectile.impact"), gameHash("projectile.impact.v1"), 0,
     onProjectileImpact, "combat.projectile-impact"}};
// Generic handling record schema (no weapon/component category).
const MimitaHotPackage::SchemaRegistrar s_actorActionStateSchema{
    {gameHash("ActorActionState"), gameHash("ActorActionState.v1"), 16, 8,
     GAME_COPY_RUNTIME_ONLY, GAME_NET_NONE, "ActorActionState", 1, 0}};

#endif
