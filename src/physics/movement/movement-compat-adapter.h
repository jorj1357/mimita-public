// 09 21 2026
/* purpose
* The single compatibility entry point for the legacy built-in movement
* orchestrator (`physicsMainUpdate`).
*
* The hot movement path (`movement.main` + the shared `movement-step` kernel)
* owns gameplay movement. During migration the legacy orchestrator remains a
* fallback when no hot override/capability is present. This adapter is the ONE
* allowed caller of `physicsMainUpdate`; gameplay call sites route through it so
* there is no direct gameplay mutation of movement through the legacy owner.
*
* Remove this adapter (and the fallback) after live acceptance of the hot path.
* Does NOT own movement policy, collision, rendering, networking, or authority.
*/
#pragma once

struct Player;
struct World;
struct InputState;
struct MovementConfig;

namespace MovementCompat {

// Compatibility fallback step for one actor. Identical to the legacy
// `physicsMainUpdate` contract; exists only to keep the legacy owner behind a
// named seam.
void stepActor(Player& player, const World& world, const InputState& input,
               float dt, int subSteps = 6,
               const MovementConfig* overrideConfig = nullptr);

} // namespace MovementCompat
