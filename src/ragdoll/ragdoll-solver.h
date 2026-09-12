// 09 12 2026
/* purpose
* Component-driven ragdoll solve core. One fixed substep integrates, solves
* joints/rotation limits, resolves grabs, collides against the world and self,
* and settles limbs. Owners run this from the ragdoll.solver domain at the
* editable solver_hz; the RagdollBody passed in is the per-substep workspace
* whose dynamic state is loaded from and stored back into the limb components.
* Does NOT own input, networking, rendering, or the domain clock.
*/
#pragma once

#include "ragdoll/ragdoll-components.h"
#include "ragdoll/ragdoll-mode.h"

struct World;
struct RagdollModeConfigData;

namespace Ragdoll {
namespace Solver {

// One physics-only substep. No input, no motors; safe for alive ragdolls and
// corpses alike.
void solveSubstep(RagdollBody& body, const World& world, float dt,
                  const RagdollModeConfigData& cfg, const SolveParams& params);

// Individual passes, exposed so tests and tools can exercise them.
void solveJoints(RagdollBody& body, const RagdollModeConfigData& cfg,
                 int iterations, bool positionPass, const SolveParams& params);
void solveRotationLimits(RagdollBody& body, float betaOverride,
                         const RagdollModeConfigData& cfg);
void solveGrabs(RagdollBody& body, const RagdollModeConfigData& cfg, int iterations);
void selfCollision(RagdollBody& body, const RagdollModeConfigData& cfg);

} // namespace Solver
} // namespace Ragdoll
