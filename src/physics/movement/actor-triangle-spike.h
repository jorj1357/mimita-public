// 2026-09-27
/* purpose
* Phase 0 feasibility probe for the actor-triangle collision migration.
* Proves, without touching the active collision path, that body-part triangles
* and weapon render-mesh triangles can be extracted CPU-only (no GL context)
* and transformed into world space like the solver will need.
* Does NOT collide, correct position, or change gameplay.
* Does NOT replace the shared loader; Phase 2 will fold this probe into it.
* Does NOT load GPU resources.
*/
#pragma once

#include <string>

// Runs the probe and writes a human-readable report into `outSummary`.
// Returns true when every spike check passes.
bool actorTriangleSpike(std::string* outSummary = nullptr);
