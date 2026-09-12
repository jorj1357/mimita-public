// 09 12 2026
/* purpose
* Implements the EXE-side bridge to the hot actor module.
* Owns envelope construction and ABI-safe module invocation only.
*/
#include "live-code/live-actor.h"

#include "hot-reload/hot-reload-system.h"
#include "live-code/live-modules.h"

namespace {

const GameActorModuleV1* actorModule()
{
    return static_cast<const GameActorModuleV1*>(
        LiveModules::findFunctions("actor", sizeof(GameActorModuleV1)));
}

GameEnvelope makeEnvelope(std::uint32_t stateType, std::uint32_t stateVersion,
                          void* data, std::uint32_t byteSize, std::uint64_t tick)
{
    GameEnvelope envelope{};
    envelope.stateType = stateType;
    envelope.stateVersion = stateVersion;
    envelope.byteSize = byteSize;
    envelope.data = data;
    envelope.tick = tick;
    envelope.codeGeneration = HotReloadSystem::instance().gameMemory().reloadCount;
    return envelope;
}

} // namespace

namespace LiveActor {

bool available()
{
    const GameActorModuleV1* module = actorModule();
    return module && module->chooseActorCommand &&
           module->updateActorEmotion && module->chooseActorRole;
}

bool chooseCommand(const ActorStateV1& state, ActorCommandV1& out)
{
    const GameActorModuleV1* module = actorModule();
    if (!module || !module->chooseActorCommand)
        return false;

    out = ActorCommandV1{};
    GameEnvelope stateEnvelope = makeEnvelope(
        GAME_STATE_ACTOR, ACTOR_STATE_VERSION,
        const_cast<ActorStateV1*>(&state), sizeof(ActorStateV1), state.tick);
    GameEnvelope commandEnvelope = makeEnvelope(
        GAME_STATE_ACTOR, ACTOR_COMMAND_VERSION, &out, sizeof(ActorCommandV1), state.tick);
    return module->chooseActorCommand(
        &stateEnvelope, &commandEnvelope, &HotReloadSystem::instance().gameMemory());
}

void updateEmotion(ActorStateV1& state, const ActorEventV1* event, float dt)
{
    const GameActorModuleV1* module = actorModule();
    if (!module || !module->updateActorEmotion)
        return;

    GameEnvelope stateEnvelope = makeEnvelope(
        GAME_STATE_ACTOR, ACTOR_STATE_VERSION, &state, sizeof(ActorStateV1), state.tick);
    GameEnvelope eventEnvelope{};
    GameEnvelope* eventPtr = nullptr;
    if (event) {
        eventEnvelope = makeEnvelope(
            GAME_STATE_ACTOR, ACTOR_EVENT_VERSION,
            const_cast<ActorEventV1*>(event), sizeof(ActorEventV1), event->tick);
        eventPtr = &eventEnvelope;
    }
    module->updateActorEmotion(
        &stateEnvelope, eventPtr, dt, &HotReloadSystem::instance().gameMemory());
}

std::uint32_t chooseRole(const ActorStateV1& state)
{
    const GameActorModuleV1* module = actorModule();
    if (!module || !module->chooseActorRole)
        return 0;
    GameEnvelope stateEnvelope = makeEnvelope(
        GAME_STATE_ACTOR, ACTOR_STATE_VERSION,
        const_cast<ActorStateV1*>(&state), sizeof(ActorStateV1), state.tick);
    return module->chooseActorRole(
        &stateEnvelope, &HotReloadSystem::instance().gameMemory());
}

} // namespace LiveActor
