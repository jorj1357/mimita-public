// 09 21 2026
/* purpose
* Implements the single compatibility seam over the legacy movement
* orchestrator. See movement-compat-adapter.h.
*/
#include "physics/movement/movement-compat-adapter.h"

#include "physics/physics-mini.h"

namespace MovementCompat {

void stepActor(Player& player, const World& world, const InputState& input,
               float dt, int subSteps, const MovementConfig* overrideConfig)
{
    physicsMainUpdate(player, world, input, dt, subSteps, overrideConfig);
}

} // namespace MovementCompat
