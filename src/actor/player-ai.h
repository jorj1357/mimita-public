// 2026-11-01
/* purpose
* Drives the existing local player actor from a bounded AI input source.
* Produces the same InputFrame consumed by human input, so movement, collision,
* weapons, and server validation remain unchanged.
* Does NOT create a second actor, mutate server-owned state, or bypass rules.
*/
#pragma once

#include <string>

struct Player;
struct Camera;
struct World;
struct InputFrame;

namespace PlayerAiController {

bool active();
void setActive(bool enabled);

// Force one melee weapon for an agent-driven Mode 1 test. Empty id restores
// normal range-based weapon choice. The weapon is equipped through the normal
// slot/equip owner; it is not written into authoritative state here.
void setMeleeWeapon(const std::string& weaponId);
const std::string& meleeWeapon();

// Builds one input frame from replicated hostiles and the current player pose.
// Safe to call when inactive (returns an empty frame).
InputFrame buildFrame(const Player& player, Camera& camera,
                      const World& world);

} // namespace PlayerAiController
