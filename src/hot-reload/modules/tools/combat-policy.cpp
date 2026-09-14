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

namespace {

void MIMITA_GAME_CALL onToolUse(void* host, const GameEventV1* event)
{
    auto* context = static_cast<GameplayContextV1*>(host);
    auto* use = event ? static_cast<ToolUsePolicyV1*>(event->payload) : nullptr;
    if (!context || !use)
        return;
    const std::uint64_t key = use->toolId != 0 ? use->toolId : use->toolNetworkId;
    HotToolUseFn behavior = HotPackageBuilder::instance().findToolBehavior(key);
    if (!behavior)
        return;  // no hot behavior: cold fire path owns this use
    use->handled = 1;
    use->outFire = use->baseFire;
    behavior(use, context);
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

#endif
