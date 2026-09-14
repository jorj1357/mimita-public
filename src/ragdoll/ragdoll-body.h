// 09 12 2026
/* purpose
* Shared ragdoll body template construction and skeleton write-back. One owner
* so the local alive ragdoll, corpses, and remote presentation all build and
* apply the same body the same way.
* Does NOT own the solver, input, networking, or the domain clock.
*/
#pragma once

#include <glm/glm.hpp>

#include "ragdoll/ragdoll-mode.h"

struct Player;
struct RagdollModeConfigData;

namespace Ragdoll {

glm::mat4 rigidWorld(const RigidBody& body);

// Build the physics template (limbs, joints, mesh mapping) from a player's
// model and the ragdoll config. Overwrites `out`.
void buildBody(const Player& player, const RagdollModeConfigData& cfg, RagdollBody& out);

// Write the authoritative root and skeleton transforms for a body into a player.
// Uses `part.body` transforms (smoothed by body_smoothing) and the body's static
// mesh mapping. Presentation-only callers may set `part.body` to interpolated
// values first.
void applyBodyToPlayer(Player& player, RagdollBody& body, const RagdollModeConfigData& cfg);

} // namespace Ragdoll
