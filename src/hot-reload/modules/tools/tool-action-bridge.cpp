// 09 17 2026
/* purpose
* The animation boundary in one place. Tool behaviors emit generic
* ToolActionEventV1 facts; this hot subscriber converts them into the existing
* HotActorActionState so the hot animation state machine keeps working. No
* behavior ever calls animation, and no kernel field or slot is added.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-action.h"
#include "hot-reload/hot-package.h"
#include "hot-reload/hot-tool-action.h"

namespace {

void MIMITA_GAME_CALL onToolAction(void* host, const GameEventV1* event)
{
    auto* ctx = static_cast<GameplayContextV1*>(host);
    auto* action = event ? static_cast<ToolActionEventV1*>(event->payload)
                         : nullptr;
    if (!ctx || !action || !ctx->dynamicReadComponent ||
        !ctx->dynamicWriteComponent || action->actorEntity == 0)
        return;

    HotActorActionStateV1 state{};
    const bool had = ctx->dynamicReadComponent(
        ctx->host, action->actorEntity, HOT_ACTOR_ACTION_COMPONENT, &state,
        sizeof(state));
    if (!had || state.version != HOT_ACTION_STATE_VERSION)
        state = HotActorActionStateV1{};
    state.version = HOT_ACTION_STATE_VERSION;
    state.byteSize = sizeof(HotActorActionStateV1);

    // Common identity.
    if (action->toolId != 0)
        state.weaponKey = action->toolId;
    if (action->actionSequence != 0)
        state.sourceEventSeq = action->actionSequence;

    switch (action->action) {
        case TOOL_ACTION_FIRED:
        case TOOL_ACTION_HIT:
        case TOOL_ACTION_PRIMARY_ACCEPTED:
            state.flags |= HOT_ACTION_FLAG_SHOOTING;
            state.shootEffectTimer = 0.12f;
            if (action->amount > 0)
                state.ammo = static_cast<std::uint32_t>(action->amount);
            break;
        case TOOL_ACTION_DRY_FIRE:
            state.flags |= HOT_ACTION_FLAG_SHOOTING;
            state.shootEffectTimer = 0.06f;
            break;
        case TOOL_ACTION_RELOAD_STARTED:
            state.flags |= HOT_ACTION_FLAG_RELOADING;
            state.isReloading = 1;
            state.reloadTimer = action->strength100 / 100.0f;
            break;
        case TOOL_ACTION_RELOAD_FINISHED:
        case TOOL_ACTION_RELOAD_CANCELLED:
            state.flags &= ~HOT_ACTION_FLAG_RELOADING;
            state.isReloading = 0;
            state.reloadTimer = 0.0f;
            break;
        case TOOL_ACTION_LUNGE:
        case TOOL_ACTION_MELEE_CONTACT:
        case TOOL_ACTION_SECONDARY_STARTED:
            state.flags |= HOT_ACTION_FLAG_MELEE;
            state.meleeAction = 1;
            break;
        case TOOL_ACTION_EQUIPPED:
            state.flags |= HOT_ACTION_FLAG_EQUIPPING;
            state.equipTimer = action->strength100 / 100.0f;
            break;
        case TOOL_ACTION_UNEQUIPPED:
            state.flags &= ~HOT_ACTION_FLAG_EQUIPPING;
            state.equipTimer = 0.0f;
            break;
        default:
            break;
    }

    ctx->dynamicWriteComponent(ctx->host, action->actorEntity,
                               HOT_ACTOR_ACTION_COMPONENT, &state,
                               sizeof(state));
}

} // namespace

const MimitaHotPackage::EventRegistrar s_toolActionBridge{
    {gameHash("tool.action"), gameHash("tool.action.v1"), 0, onToolAction,
     "tools.tool-action-bridge"}};

#endif // MIMITA_GAME_DLL
