// 09 12 2026
/* purpose
* EXE-side bridge to the hot actor module.
* Wraps ActorStateV1/ActorCommandV1 in GameEnvelopes and calls the active
* module's chooseActorCommand / updateActorEmotion / chooseActorRole.
* Does NOT own the actor data or apply decisions to gameplay.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

namespace LiveActor {

bool available();
bool chooseCommand(const ActorStateV1& state, ActorCommandV1& out);
void updateEmotion(ActorStateV1& state, const ActorEventV1* event, float dt);
std::uint32_t chooseRole(const ActorStateV1& state);

} // namespace LiveActor
