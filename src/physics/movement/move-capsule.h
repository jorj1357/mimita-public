// 09 14 2026
/* purpose
* Kernel capsule-vs-world movement primitive exposed to hot movement systems
* through the physics.moveCapsule capability. One fixed solve: integrate the
* supplied MovementStateV1, resolve world collision, write the result back.
* Does NOT own player state, input, or gameplay policy.
*/
#pragma once

#include "hot-reload/game-api.h"

struct World;

namespace Physics {

void moveCapsuleStep(MovementStateV1& state, const World* world, float dt);

} // namespace Physics
