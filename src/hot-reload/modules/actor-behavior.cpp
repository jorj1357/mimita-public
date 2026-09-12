// 09 12 2026
/* purpose
* Hot replaceable actor decision module for the shared Actor model.
* Implements chooseActorCommand, updateActorEmotion, and chooseActorRole as
* deterministic plain-data functions over ActorStateV1/ActorCommandV1.
* Does NOT own world, identity, physics, combat, or rendering.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/game-modules.h"

#include <algorithm>
#include <cmath>

namespace {

bool MIMITA_GAME_CALL chooseActorCommand(
    const GameEnvelope* stateEnvelope, GameEnvelope* outEnvelope, GameMemory*)
{
    if (!stateEnvelope || !stateEnvelope->data ||
        !outEnvelope || !outEnvelope->data)
        return false;
    if (stateEnvelope->stateVersion != ACTOR_STATE_VERSION ||
        outEnvelope->stateVersion != ACTOR_COMMAND_VERSION)
        return false;

    const ActorStateV1* state = static_cast<const ActorStateV1*>(stateEnvelope->data);
    ActorCommandV1* command = static_cast<ActorCommandV1*>(outEnvelope->data);
    *command = ActorCommandV1{};

    // Fear slows the advance; confidence above baseline advances harder.
    const float fear = std::clamp(state->emotionFear, 0.0f, 1.0f);
    const float confidence = std::clamp(state->emotionConfidence, 0.0f, 1.0f);
    command->speedScale = std::clamp(
        1.0f - 0.30f * fear + 0.30f * (confidence - 0.5f), 0.2f, 1.6f);
    command->role = state->role;
    command->emotionPanic = state->emotionPanic;
    command->emotionFear = state->emotionFear;
    command->emotionConfidence = state->emotionConfidence;
    command->emotionStress = state->emotionStress;
    command->tick = state->tick;
    return true;
}

void MIMITA_GAME_CALL updateActorEmotion(
    GameEnvelope* stateEnvelope, const GameEnvelope* eventEnvelope,
    float dt, GameMemory*)
{
    if (!stateEnvelope || !stateEnvelope->data || dt <= 0.0f)
        return;
    if (stateEnvelope->stateVersion != ACTOR_STATE_VERSION)
        return;

    ActorStateV1* state = static_cast<ActorStateV1*>(stateEnvelope->data);

    if (eventEnvelope && eventEnvelope->data &&
        eventEnvelope->stateVersion == ACTOR_EVENT_VERSION) {
        const ActorEventV1* event =
            static_cast<const ActorEventV1*>(eventEnvelope->data);
        if (event->type == ACTOR_EVENT_DAMAGED) {
            state->emotionFear = std::min(1.0f, state->emotionFear + 0.20f);
            state->emotionPanic = std::min(1.0f, state->emotionPanic + 0.15f);
            state->emotionStress = std::min(1.0f, state->emotionStress + 0.20f);
            state->emotionConfidence = std::max(0.0f, state->emotionConfidence - 0.10f);
        } else if (event->type == ACTOR_EVENT_KILLED) {
            state->emotionConfidence = std::min(1.0f, state->emotionConfidence + 0.15f);
            state->emotionFear = std::max(0.0f, state->emotionFear - 0.20f);
            state->emotionStress = std::max(0.0f, state->emotionStress - 0.15f);
        }
    }

    const float decay = std::clamp(dt * 0.25f, 0.0f, 1.0f);
    state->emotionFear += (0.0f - state->emotionFear) * decay;
    state->emotionPanic += (0.0f - state->emotionPanic) * decay;
    state->emotionStress += (0.0f - state->emotionStress) * decay;
    state->emotionConfidence += (0.5f - state->emotionConfidence) * decay;
}

std::uint32_t MIMITA_GAME_CALL chooseActorRole(
    const GameEnvelope* stateEnvelope, GameMemory*)
{
    if (!stateEnvelope || !stateEnvelope->data ||
        stateEnvelope->stateVersion != ACTOR_STATE_VERSION)
        return 0;
    const ActorStateV1* state = static_cast<const ActorStateV1*>(stateEnvelope->data);
    if (state->distanceToTarget >= 0.0f && state->distanceToTarget < 6.0f)
        return 1; // aggressive / close
    return 2;     // ranged
}

const GameActorModuleV1 gActorModuleV1 = {
    1u,
    sizeof(GameActorModuleV1),
    chooseActorCommand,
    updateActorEmotion,
    chooseActorRole,
};

} // namespace

const GameModuleDescriptor* MimitaGetActorModule()
{
    static const GameModuleDescriptor descriptor = {
        "actor", 1u, sizeof(GameActorModuleV1), &gActorModuleV1};
    return &descriptor;
}

#endif
