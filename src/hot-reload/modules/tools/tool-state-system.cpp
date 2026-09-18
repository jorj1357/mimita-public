// 09 17 2026
/* purpose
* One hot fixed-tick system that owns ToolInstanceState advancement: cooldowns
* count down, reloads complete through the single canonical transition, and a
* ReloadFinished action fact is emitted. State lives on the tool entity, so two
* actors with the same definition tick independently. No cold edit to add a tool.
* Does NOT own damage, firing, or animation.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"
#include "hot-reload/hot-tool-action.h"
#include "hot-reload/hot-tool-state.h"
#include "hot-reload/hot-tool-visual.h"

namespace {

// Register the per-instance state schema so the dynamic store tracks it like any
// other generic component (runtime-only; never replicated).
const MimitaHotPackage::SchemaRegistrar s_toolStateSchema{
    {HOT_TOOL_STATE_COMPONENT, gameHash("ToolInstanceState.v1"),
     sizeof(ToolInstanceStateV1), 8, GAME_COPY_RUNTIME_ONLY, GAME_NET_NONE,
     "ToolInstanceState", 1, 0}};

void MIMITA_GAME_CALL toolStateTick(void* host, std::uint64_t tick, float dt)
{
    auto* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || !ctx->dynamicEnumerateComponent || !ctx->dynamicReadComponent ||
        !ctx->dynamicWriteComponent)
        return;

    constexpr std::uint32_t kMax = 128;
    std::uint64_t ids[kMax];
    const std::uint32_t count = ctx->dynamicEnumerateComponent(
        ctx->host, HOT_TOOL_STATE_COMPONENT, ids, kMax);
    for (std::uint32_t i = 0; i < count; ++i) {
        if (ids[i] == 0)
            continue;
        ToolInstanceStateV1 s{};
        if (!toolStateRead(ctx, ids[i], &s))
            continue;
        if (s.cooldownRemaining <= 0.0f && !s.isReloading)
            continue;

        const ToolDefinitionV1* def = findToolDefinition(s.definitionId);
        const std::int32_t magazine = def ? def->magazineSize : 0;
        const std::uint32_t completed = toolStateAdvance(ctx, s, dt);
        if (completed && magazine > 0) {
            toolStateCompleteReload(ctx, s, magazine);
            ToolActionEventV1 ev{};
            ev.actorEntity = s.actorEntity;
            ev.toolEntity = s.toolEntity;
            ev.toolId = s.definitionId;
            ev.action = TOOL_ACTION_RELOAD_FINISHED;
            ev.simulationTick = tick;
            ev.amount = s.currentAmmo;
            emitToolAction(ctx, ev);
        }
    }
}

} // namespace

// Runs in the gameplay fixed tick; priority after movement/other gameplay.
const MimitaHotPackage::SystemRegistrar s_toolStateSystem{
    {gameHash("tools.tool-state"), GAME_DOMAIN_GAMEPLAY, 20, 0, toolStateTick,
     "tools.tool-state"}};

#endif // MIMITA_GAME_DLL
