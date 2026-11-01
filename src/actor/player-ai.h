// 2026-11-01
/* purpose
* Drives the existing local player actor from a bounded AI input source.
* Produces the same InputFrame consumed by human input, so movement, collision,
* weapons, and server validation remain unchanged.
* Does NOT create a second actor, mutate server-owned state, or bypass rules.
*/
#pragma once

struct Player;
struct Camera;
struct World;
struct InputFrame;

namespace PlayerAiController {

bool active();
void setActive(bool enabled);

// Builds one input frame from replicated hostiles and the current player pose.
// Safe to call when inactive (returns an empty frame).
InputFrame buildFrame(const Player& player, const Camera& camera,
                      const World& world);

} // namespace PlayerAiController
